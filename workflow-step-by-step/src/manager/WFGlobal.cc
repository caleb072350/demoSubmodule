#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/engine.h>
#include <openssl/conf.h>
#include <openssl/crypto.h>
#include <assert.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <string>
#include <unordered_map>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include "WFGlobal.h"
#include "EndpointParams.h"
#include "CommScheduler.h"
#include "DNSCache.h"
#include "RouteManager.h"
#include "Executor.h"
#include "Mutex.h"
#include "WFTask.h"
#include "WFTaskError.h"

const char *library_version = "Workflow Library Version: 1.12.6";

class __WFGlobal
{
public:
	static __WFGlobal *get_instance()
	{
		static __WFGlobal kInstance;
		return &kInstance;
	}

    const WFGlobalSettings *get_global_settings() const
	{
		return &settings_;
	}

	void set_global_settings(const WFGlobalSettings *settings)
	{
		settings_ = *settings;
	}

    const char *get_default_port(const std::string& scheme) const
	{
		const auto it = static_scheme_port_.find(scheme);

		if (it != static_scheme_port_.end())
			return it->second;

		const auto it2 = user_scheme_port_.find(scheme);

		if (it2 != user_scheme_port_.end())
			return it2->second.c_str();

		return NULL;
	}

    void register_scheme_port(const std::string& scheme, unsigned short port)
	{
		user_scheme_port_mutex_.lock();
		user_scheme_port_[scheme] = std::to_string(port);
		user_scheme_port_mutex_.unlock();
	}

private:
	__WFGlobal():
		settings_(GLOBAL_SETTINGS_DEFAULT)
	{
		static_scheme_port_["http"] = "80";
		static_scheme_port_["https"] = "443";
		static_scheme_port_["redis"] = "6379";
		static_scheme_port_["rediss"] = "6379";
		static_scheme_port_["mysql"] = "3306";
		static_scheme_port_["kafka"] = "9092";
		// sync_count_ = 0;
		// sync_max_ = 0;
	}

private:
	struct WFGlobalSettings settings_;
	std::unordered_map<std::string, const char *> static_scheme_port_;
	std::unordered_map<std::string, std::string> user_scheme_port_;
	std::mutex user_scheme_port_mutex_;
	// std::mutex sync_mutex_;
	// int sync_count_;
	// int sync_max_;
};

class __DNSManager
{
public:
	ExecQueue *get_dns_queue() { return &dns_queue_; }
	Executor *get_dns_executor() { return &dns_executor_; }

	__DNSManager()
	{
		int ret;

		ret = dns_queue_.init();
		if (ret < 0)
			abort();

		ret = dns_executor_.init(__WFGlobal::get_instance()->
											 get_global_settings()->
											 dns_threads);
		if (ret < 0)
			abort();
	}

	~__DNSManager()
	{
		dns_executor_.deinit();
		dns_queue_.deinit();
	}

private:
	ExecQueue dns_queue_;
	Executor dns_executor_;
};

class __CommManager
{
public:
	static __CommManager *get_instance()
	{
		static __CommManager kInstance;
		return &kInstance;
	}

	CommScheduler *get_scheduler() { return &scheduler_; }
	RouteManager *get_route_manager() { return &route_manager_; }
	
	ExecQueue *get_dns_queue()
	{
		return get_dns_manager_safe()->get_dns_queue();
	}

	Executor *get_dns_executor()
	{
		return get_dns_manager_safe()->get_dns_executor();
	}

private:
	__CommManager():
		dns_manager_(NULL),
		dns_flag_(false)
	{
#ifdef SIGPIPE
		signal(SIGPIPE, SIG_IGN);
#endif
		const auto *settings = __WFGlobal::get_instance()->get_global_settings();
		int ret = scheduler_.init(settings->poller_threads, settings->handler_threads);

		if (ret < 0)
			abort();
	}

	~__CommManager()
	{
		if (dns_manager_)
			delete dns_manager_;

		scheduler_.deinit();
	}

	__DNSManager *get_dns_manager_safe()
	{
		if (!dns_flag_)
		{
			dns_mutex_.lock();
			if (!dns_flag_)
			{
				dns_manager_ = new __DNSManager();
				dns_flag_ = true;
			}

			dns_mutex_.unlock();
		}

		return dns_manager_;
	}

private:
	CommScheduler scheduler_;
	RouteManager route_manager_;
	__DNSManager *dns_manager_;
	volatile bool dns_flag_;
	std::mutex dns_mutex_;
};

class __DNSCache
{
public:
	static __DNSCache *get_instance()
	{
		static __DNSCache kInstance;
		return &kInstance;
	}

	DNSCache *get_dns_cache() { return &dns_cache_; }

private:
	__DNSCache() { }

	~__DNSCache() { }

private:
	DNSCache dns_cache_;
};

class __ExecManager
{
protected:
	using ExecQueueMap = std::unordered_map<std::string, ExecQueue *>;

public:
	static __ExecManager *get_instance()
	{
		static __ExecManager kInstance;
		return &kInstance;
	}

	ExecQueue *get_exec_queue(const std::string& queue_name)
	{
		ExecQueue *queue;
		ExecQueueMap::iterator iter;

		{
			ReadLock lock(mutex_);

			iter = queue_map_.find(queue_name);
			if (iter != queue_map_.end())
				return iter->second;
		}

		queue = new ExecQueue();
		if (queue->init() >= 0)
		{
			WriteLock lock(mutex_);
			auto ret = queue_map_.emplace(queue_name, queue);

			if (!ret.second)
			{
				queue->deinit();
				delete queue;
				queue = ret.first->second;
			}

			return queue;
		}

		delete queue;
		return NULL;
	}

	Executor *get_compute_executor() { return &compute_executor_; }

private:
	__ExecManager():
		mutex_(PTHREAD_RWLOCK_INITIALIZER)
	{
		int compute_threads = __WFGlobal::get_instance()->get_global_settings()->compute_threads;

		if (compute_threads <= 0)
			compute_threads = sysconf(_SC_NPROCESSORS_ONLN);

		if (compute_executor_.init(compute_threads) < 0)
			abort();
	}

	~__ExecManager()
	{
		compute_executor_.deinit();

		for (auto& kv : queue_map_)
		{
			kv.second->deinit();
			delete kv.second;
		}
	}

private:
	pthread_rwlock_t mutex_;
	ExecQueueMap queue_map_;
	Executor compute_executor_;
};

CommScheduler *WFGlobal::get_scheduler()
{
	return __CommManager::get_instance()->get_scheduler();
}

DNSCache *WFGlobal::get_dns_cache()
{
	return __DNSCache::get_instance()->get_dns_cache();
}

RouteManager *WFGlobal::get_route_manager()
{
	return __CommManager::get_instance()->get_route_manager();
}

const char *WFGlobal::get_default_port(const std::string& scheme)
{
	return __WFGlobal::get_instance()->get_default_port(scheme);
}

ExecQueue *WFGlobal::get_dns_queue()
{
	return __CommManager::get_instance()->get_dns_queue();
}

Executor *WFGlobal::get_dns_executor()
{
	return __CommManager::get_instance()->get_dns_executor();
}

const WFGlobalSettings *WFGlobal::get_global_settings()
{
	return __WFGlobal::get_instance()->get_global_settings();
}
