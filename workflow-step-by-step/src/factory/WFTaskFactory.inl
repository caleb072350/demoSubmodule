#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <time.h>
#include <netdb.h>
#include <stdio.h>
#include <new>
#include <string>
#include <functional>
#include <utility>
#include "WFGlobal.h"
#include "Workflow.h"
#include "WFTask.h"
#include "UpstreamManager.h"
#include "RouteManager.h"
#include "URIParser.h"
#include "WFTaskError.h"
#include "EndpointParams.h"

#define DNS_CACHE_LEVEL_0		0
#define DNS_CACHE_LEVEL_1		1
#define DNS_CACHE_LEVEL_2		2
#define DNS_CACHE_LEVEL_3		3

class WFRouterTask : public WFGenericTask
{
private:
	using router_callback_t = std::function<void (WFRouterTask *)>;
	using WFDNSTask = WFThreadTask<DNSInput, DNSOutput>;

public:
	RouteManager::RouteResult route_result_;

	WFRouterTask(TransportType type,
				 const std::string& host,
				 unsigned short port,
				 const std::string& info,
				 int dns_cache_level,
				 unsigned int dns_ttl_default,
				 unsigned int dns_ttl_min,
				 const struct EndpointParams *endpoint_params,
				 bool first_addr_only,
				 router_callback_t&& callback) :
		type_(type),
		host_(host),
		port_(port),
		info_(info),
		dns_cache_level_(dns_cache_level),
		dns_ttl_default_(dns_ttl_default),
		dns_ttl_min_(dns_ttl_min),
		endpoint_params_(*endpoint_params),
		first_addr_only_(first_addr_only),
		callback_(std::move(callback))
		{}

private:
	virtual void dispatch();
	virtual SubTask *done();
	void  dns_callback(WFDNSTask *dns_task);
	void dns_callback_internal(DNSOutput *dns_task, unsigned int ttl_default, unsigned int ttl_min);
	
private:
	TransportType type_;
	std::string host_;
	unsigned short port_;
	std::string info_;
	int dns_cache_level_;
	unsigned int dns_ttl_default_;
	unsigned int dns_ttl_min_;
	struct EndpointParams endpoint_params_;
	bool first_addr_only_;
	bool insert_dns_;
	router_callback_t callback_;
};


template<class REQ, class RESP, typename CTX = bool>
class WFComplexClientTask : public WFClientTask<REQ, RESP>
{
protected:
	using task_callback_t = std::function<void (WFNetworkTask<REQ, RESP> *)>;

public:
	WFComplexClientTask(int retry_max, task_callback_t&& callback):
		WFClientTask<REQ, RESP>(NULL, WFGlobal::get_scheduler(),
								std::move(callback)),
		retry_max_(retry_max),
		first_addr_only_(false),
		router_task_(NULL),
		type_(TT_TCP),
		retry_times_(0),
		is_retry_(false),
		has_original_uri_(true),
		redirect_(false)
	{}

protected:
	// new api for children
	virtual bool init_success() { return true; }
	virtual void init_failed() { }
	virtual bool check_request() { return true; }
	virtual SubTask *route();
	virtual bool finish_once() { return true; }

public:
	void set_info(const std::string& info)
	{
		info_.assign(info);
	}

	void set_info(const char *info)
	{
		info_.assign(info);
	}

	void set_type(TransportType type)
	{
		type_ = type;
	}

	void init(const ParsedURI& uri)
	{
		is_sockaddr_ = false;
		init_state_ = 0;
		uri_ = uri;
		init_with_uri();
	}

	void init(ParsedURI&& uri)
	{
		is_sockaddr_ = false;
		init_state_ = 0;
		uri_ = std::move(uri);
		init_with_uri();
	}

	void init(TransportType type,
			  const struct sockaddr *addr,
			  socklen_t addrlen,
			  const std::string& info);

	const ParsedURI *get_original_uri() const { return &original_uri_; }
	const ParsedURI *get_current_uri() const { return &uri_; }

	void set_redirect(const ParsedURI& uri)
	{
		redirect_ = true;
		init(uri);
		retry_times_ = 0;
	}

	void set_redirect(TransportType type, const struct sockaddr *addr,
					  socklen_t addrlen, const std::string& info)
	{
		redirect_ = true;
		init(type, addr, addrlen, info);
		retry_times_ = 0;
	}

protected:
	void set_redirect()
	{
		redirect_ = true;
		retry_times_ = 0;
	}

	void set_retry(const ParsedURI& uri)
	{
		redirect_ = true;
		init(uri);
		retry_times_++;
	}

	void set_retry()
	{
		redirect_ = true;
		retry_times_++;
	}

	virtual void dispatch();
	virtual SubTask *done();

	void clear_resp()
	{
		size_t size = this->resp.get_size_limit();

		this->resp.~RESP();
		new(&this->resp) RESP();
		this->resp.set_size_limit(size);
	}

	bool is_user_request() const
	{
		return this->get_message_out() == &this->req;
	}

	void disable_retry()
	{
		retry_times_ = retry_max_;
	}

	TransportType get_transport_type() const { return type_; }

	ParsedURI uri_;
	ParsedURI original_uri_;

	int retry_max_;
	bool is_sockaddr_;
	bool first_addr_only_;
	CTX ctx_;
	SubTask *router_task_;

public:
	CTX *get_mutable_ctx() { return &ctx_; }

private:
	void init_with_uri();
	bool set_port();
	void router_callback(SubTask *task); // default: DNS
	void switch_callback(WFTimerTask *task);

	RouteManager::RouteResult route_result_;
	UpstreamManager::UpstreamResult upstream_result_;

	TransportType type_;
	std::string info_;

	int retry_times_;

	/* state 0: uninited or failed; 1: inited but not checked; 2: checked. */
	char init_state_;
	bool is_retry_;
	bool has_original_uri_;
	bool redirect_;
};

template<class REQ, class RESP, typename CTX>
void WFComplexClientTask<REQ, RESP, CTX>::init(TransportType type,
											   const struct sockaddr *addr,
											   socklen_t addrlen,
											   const std::string& info)
{
	is_sockaddr_ = true;
	init_state_ = 0;
	type_ = type;
	info_.assign(info);
	struct addrinfo addrinfo;
	const auto *params = &WFGlobal::get_global_settings()->endpoint_params;

	addrinfo.ai_addrlen = addrlen;
	addrinfo.ai_addr = (struct sockaddr *)addr;
	addrinfo.ai_canonname = NULL;
	addrinfo.ai_next = NULL;
	addrinfo.ai_flags = 0;
	addrinfo.ai_family = addr->sa_family;
	addrinfo.ai_socktype = SOCK_STREAM;
	addrinfo.ai_protocol = 0;

	if (WFGlobal::get_route_manager()->get(type, &addrinfo, info_, params,
									route_result_) < 0)
	{
		this->state = WFT_STATE_SYS_ERROR;
		this->error = errno;
	}	
	else if (!route_result_.request_object)
	{
		// should not happen
		this->state = WFT_STATE_SYS_ERROR;
		this->error = EAGAIN;
	}	
	else
	{
		init_state_ = this->init_success() ? 1 : 0;
		return;
	}		
	this->init_failed();
	return;					
}		

template<class REQ, class RESP, typename CTX>
bool WFComplexClientTask<REQ, RESP, CTX>::set_port()
{
	int port = 0;
	if (uri_.port && uri_.port[0])
	{
		port = atoi(uri_.port);
		if (port < 0 || port > 65535)
		{
			this->state = WFT_STATE_TASK_ERROR;
			this->error = WFT_ERR_URI_PORT_INVALID;
			return false;
		}
	}

	if (port == 0 && uri_.scheme)
	{
		const char *port_str = WFGlobal::get_default_port(uri_.scheme);
		if (port_str)
		{
			size_t port_len = strlen(port_str);
			if (uri_.port)
			{
				free(uri_.port);
			}
			uri_.port = (char *)malloc(port_len + 1);
			if (!uri_.port)
			{
				uri_.state = URI_STATE_ERROR;
				uri_.error = errno;
				this->state = WFT_STATE_SYS_ERROR;
				this->error = errno;
				return false;
			}
			memcpy(uri_.port, port_str, port_len + 1);
		}
	}
	return true;
}

template<class REQ, class RESP, typename CTX>
void WFComplexClientTask<REQ, RESP, CTX>::init_with_uri()
{
	if (has_original_uri_)
	{
		original_uri_ = uri_;
		has_original_uri_ = true;
	}

	route_result_.clear();
	if (uri_.state == URI_STATE_SUCCESS && this->set_port())
	{
		int ret = UpstreamManager::choose(uri_, upstream_result_);
		if (ret < 0)
		{
			this->state = WFT_STATE_SYS_ERROR;
			this->error = errno;
		}
		else if (upstream_result_.state == UPSTREAM_ALL_DOWN)
		{
			this->state = WFT_STATE_TASK_ERROR;
			this->error = WFT_ERR_UPSTREAM_UNAVAILABLE;
		}
		else 
		{
			init_state_ = this->init_success() ? 1 : 0;
			return;
		}
	}
	else 
	{
		if (uri_.state == URI_STATE_ERROR)
		{
			this->state = WFT_STATE_SYS_ERROR;
			this->error = uri_.error;
		}
		else 
		{
			this->state = WFT_STATE_TASK_ERROR;
			this->error = WFT_ERR_URI_PARSE_FAILED;
		}
	}
	this->init_failed();
	return;
}

template<class REQ, class RESP, typename CTX>
SubTask *WFComplexClientTask<REQ, RESP, CTX>::route()
{
	unsigned int dns_ttl_default;
	unsigned int dns_ttl_min;
	const struct EndpointParams *endpoint_params;
	int dns_cache_level = (is_retry_ ? DNS_CACHE_LEVEL_1 : DNS_CACHE_LEVEL_2);
	auto&& cb = std::bind(&WFComplexClientTask::router_callback, this, std::placeholders::_1);
	is_retry_ = false; // route means refresh DNS cache level
	if (upstream_result_.state == UPSTREAM_SUCCESS)
	{
		const auto *params = upstream_result_.address_params;
		dns_ttl_default = params->dns_ttl_default;
		dns_ttl_min = params->dns_ttl_min;
		endpoint_params = &params->endpoint_params;
	}
	else 
	{
		const auto *params = WFGlobal::get_global_settings();
		dns_ttl_default = params->dns_ttl_default;
		dns_ttl_min = params->dns_ttl_min;
		endpoint_params = &params->endpoint_params;
	}
	return new WFRouterTask(type_, uri_.host ? uri_.host : "",
							uri_.port ? atoi(uri_.port) : 0, info_,
							dns_cache_level, dns_ttl_default, dns_ttl_min,
							endpoint_params, first_addr_only_, std::move(cb));
}

/*
 * router callback`s obligation:
 * if success:
 * 				1. set route_result_ or call this->init()
 * 				2. series->push_front(ORIGIN_TASK)
 * if failed:
 *				1. this->finish_once() is optional;
 *				2. this->callback() is necessary;
*/
template<class REQ, class RESP, typename CTX>
void WFComplexClientTask<REQ, RESP, CTX>::router_callback(SubTask *task)
{
	WFRouterTask *router_task = static_cast<WFRouterTask *>(task);
	int state = router_task->get_state();
	if (state == WFT_STATE_SUCCESS)
		route_result_ = router_task->route_result_;
	else
	{
		this->state = state;
		this->error = router_task->get_error();
	}

	if (route_result_.request_object)
		series_of(this)->push_front(this);
	else
	{
		UpstreamManager::notify_unavailable(upstream_result_.cookie);
		if (this->callback)
			this->callback(this);
		if (redirect_)
		{
			init_state_ = this->init_success() ? 1 : 0;
			redirect_ = false;
			this->state = WFT_STATE_UNDEFINED;
			this->error = 0;
			this->timeout_reason = TOR_NOT_TIMEOUT;
			series_of(this)->push_front(this);
		}
		else
			delete this;
	}
}

template<class REQ, class RESP, typename CTX>
void WFComplexClientTask<REQ, RESP, CTX>::dispatch()
{
	// 1. children check_request()
	if (init_state_ == 1)
	{
		init_state_ = this->check_request() ? 2 : 0;
	}
	if (init_state_)
	{
		if (route_result_.request_object)
		{
			// 2. origin task dispatch()
			this->set_request_object(route_result_.request_object);
			this->WFClientTask<REQ, RESP>::dispatch();
			return;
		}

		if (is_sockaddr_ || uri_.state == URI_STATE_SUCCESS)
		{
			// 3. DNS route() or children route()
			router_task_ = this->route();
			if (router_task_)
			{
				series_of(this)->push_front(router_task_);
			}
			else 
			{
				this->state = WFT_STATE_TASK_ERROR;
				this->error = WFT_ERR_ROUTE_FAILED;
			}
		}
		else
		{
			if (uri_.state == URI_STATE_ERROR)
			{
				this->state = WFT_STATE_SYS_ERROR;
				this->error = uri_.error;
			}
			else
			{
				this->state = WFT_STATE_TASK_ERROR;
				this->error = WFT_ERR_URI_PARSE_FAILED;
			}
		}
	}
	this->subtask_done();
}

template<class REQ, class RESP, typename CTX>
SubTask *WFComplexClientTask<REQ, RESP, CTX>::done()
{
	SeriesWork *series = series_of(this);

	// 1. routing
	if (router_task_)
	{
		router_task_ = NULL;
		return series->pop();
	}

	if (init_state_)
	{
		// 2. children can set_redirect() here
		bool is_user_request = this->finish_once();
		// 3. complex task success
		if (this->state == WFT_STATE_SUCCESS)
		{
			RouteManager::notify_available(route_result_.cookie, this->target);
			UpstreamManager::notify_available(upstream_result_.cookie);
			upstream_result_.clear();
			// 4. children message out sth. else
			if (!is_user_request)
				return this;
		}
		else if (this->state == WFT_STATE_SYS_ERROR)
		{
			RouteManager::notify_unavailable(route_result_.cookie, this->target);
			UpstreamManager::notify_unavailable(upstream_result_.cookie);
			// 5. complex task failed: retry
			if (retry_times_ < retry_max_)
			{
				if (is_sockaddr_)
					set_retry();
				else 
					set_retry(original_uri_);
				is_retry_ = true; // will influence next round dns cache time
			}
		}
	}

	/*
	 * When target is NULL, it's very likely that we are still in the
	 * 'dispatch' thread. Running a timer will switch callback function
	 * to a handler thread, and this can prevent stack overflow.
	*/	
	if (!this->target)
	{
		// auto&& cb = std::bind(&WFComplexClientTask::switch_callback,
		// 					  this,
		// 					  std::placeholders::_1);
		// WFTimerTask *timer = WFTaskFactory::create_timer_task(0, std::move(cb));

		// series->push_front(timer);
	}
	else 
		this->switch_callback(NULL);

	return series->pop();
}

template<class REQ, class RESP, typename CTX>
void  WFComplexClientTask<REQ, RESP, CTX>::switch_callback(WFTimerTask *)
{
	if (!redirect_)
	{
		if (this->state == WFT_STATE_SYS_ERROR && this->error < 0)
		{
			this->state = WFT_STATE_SSL_ERROR;
			this->error = -this->error;
		}

		// 4. children finish before user callback
		if (this->callback)
			this->callback(this);
	}

	if (redirect_)
	{
		init_state_ = this->init_success() ? 1 : 0;
		redirect_ = false;
		clear_resp();

		this->target = NULL;
		this->timeout_reason = TOR_NOT_TIMEOUT;
		this->state = WFT_STATE_UNDEFINED;
		this->error = 0;
		series_of(this)->push_front(this);
	}
	else
		delete this;
}

/////////////////////////////////////////////

template<class INPUT, class OUTPUT>
class __WFThreadTask : public WFThreadTask<INPUT, OUTPUT>
{
protected:
	virtual void execute()
	{
		this->routine(&this->input, &this->output);
	}

protected:
	std::function<void (INPUT *, OUTPUT *)> routine;

public:
	__WFThreadTask(ExecQueue *queue, Executor *executor,
				   std::function<void (INPUT *, OUTPUT *)>&& rt,
				   std::function<void (WFThreadTask<INPUT, OUTPUT> *)>&& cb) :
		WFThreadTask<INPUT, OUTPUT>(queue, executor, std::move(cb)),
		routine(std::move(rt))
	{
	}
};

template<class INPUT, class OUTPUT>
WFThreadTask<INPUT, OUTPUT> *
WFThreadTaskFactory<INPUT, OUTPUT>::create_thread_task(ExecQueue *queue, Executor *executor,
						std::function<void (INPUT *, OUTPUT *)> routine,
						std::function<void (WFThreadTask<INPUT, OUTPUT> *)> callback)
{
	return new __WFThreadTask<INPUT, OUTPUT>(queue, executor,
											 std::move(routine),
											 std::move(callback));
}

