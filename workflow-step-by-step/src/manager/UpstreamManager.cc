#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include <string>
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <algorithm>
#include <functional>
#include <chrono>
#include <random>
#include "list.h"
#include "rbtree.h"
#include "Mutex.h"
#include "URIParser.h"
#include "StringUtil.h"
#include "EndpointParams.h"
#include "UpstreamManager.h"

#define GET_CURRENT_SECOND	std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now().time_since_epoch()).count()
#define MTTR_SECOND			30
#define VIRTUAL_GROUP_SIZE	16

class UpstreamAddress;
class UpstreamGroup;
class Upstream;

class UpstreamAddress
{
public:
	UpstreamGroup *group;
	AddressParams params;
	struct list_head list;
	int64_t broken_timeout;
	unsigned int consistent_hash[VIRTUAL_GROUP_SIZE];
	std::string address;
	std::string host;
	std::string port;
	std::atomic<unsigned int> fail_count;
	unsigned short port_value;

public:
	UpstreamAddress(const std::string& address,
					const struct AddressParams *address_params);
};

class UpstreamGroup
{
public:
	Upstream *upstream;
	struct rb_node rb;
	std::mutex mutex;
	std::vector<UpstreamAddress *> masters;
	std::vector<UpstreamAddress *> slaves;
	struct list_head breaker_list;
	std::atomic<int> nbreak;
	std::atomic<int> nalive;
	int weight;
	int group_id;

private:
	std::mt19937 g;

public:
	UpstreamGroup(int group_id, Upstream *us):
		upstream(us),
		nbreak(0),
		nalive(0),
		weight(0),
		group_id(group_id),
		g(std::random_device{}())
	{
		INIT_LIST_HEAD(&this->breaker_list);
	}

	const UpstreamAddress *get_one();
	const UpstreamAddress *get_one_slave();
};

class Upstream
{
public:
	Upstream();
	~Upstream();

	int add(UpstreamAddress *ua);
	int del(const std::string& address);
	void disable_server(const std::string& address);
	void enable_server(const std::string& address);
	const UpstreamAddress *get(const ParsedURI& uri);
	int set_select_callback(upstream_route_t&& select_callback);
	int set_consistent_mode(upstream_route_t&& consistent_callback);
	int set_attr(bool try_another, upstream_route_t rehash_callback);
	void check_one_breaker(UpstreamGroup *group, int64_t cur_time);
	void check_all_breaker();
	void get_all_master(std::vector<std::string>& addr_list);

	static void notify_unavailable(UpstreamAddress *ua);
	static void notify_available(UpstreamAddress *ua);

protected:
	pthread_rwlock_t rwlock_;
	int total_weight_;
	int available_weight_;
	std::vector<UpstreamAddress *> masters_;
	std::unordered_map<std::string, std::vector<UpstreamAddress *>> server_map_;
	struct rb_root group_map_;
	upstream_route_t select_callback_;
	upstream_route_t consistent_callback_;
	UpstreamGroup *default_group_;
	bool try_another_;
	bool is_consistent_;

private:
	void lose_one_server(UpstreamGroup *group, const UpstreamAddress *ua);
	void gain_one_server(UpstreamGroup *group, const UpstreamAddress *ua);
	const UpstreamAddress *weighted_random_try_another() const;
	const UpstreamAddress *consistent_hash_select(unsigned int hash) const;
};

const UpstreamAddress *UpstreamGroup::get_one()
{
	if (this->nalive == 0)
		return NULL;

	std::lock_guard<std::mutex> lock(this->mutex);

	std::shuffle(this->masters.begin(), this->masters.end(), g);
	for (const auto *master : this->masters)
	{
		if (master->fail_count < master->params.max_fails)
			return master;
	}

	std::shuffle(this->slaves.begin(), this->slaves.end(), g);
	for (const auto *slave : this->slaves)
	{
		if (slave->fail_count < slave->params.max_fails)
			return slave;
	}

	return NULL;
}

const UpstreamAddress *UpstreamGroup::get_one_slave()
{
	if (this->nalive == 0)
		return NULL;

	std::lock_guard<std::mutex> lock(this->mutex);

	// std::random_shuffle(this->slaves.begin(), this->slaves.end());
	std::shuffle(this->slaves.begin(), this->slaves.end(), g);
	for (const auto *slave : this->slaves)
	{
		if (slave->fail_count < slave->params.max_fails)
			return slave;
	}

	return NULL;
}

static unsigned int __default_consistent_hash(const char *path,
											  const char *query,
											  const char *fragment)
{
	static std::hash<std::string> std_hash;
	std::string str(path);

	str += query;
	str += fragment;
	return std_hash(str);
}

UpstreamAddress::UpstreamAddress(const std::string& address,
								 const struct AddressParams *address_params)
{
	static std::hash<std::string> std_hash;
	std::vector<std::string> arr = StringUtil::split(address, ':');

	this->list.next = NULL;
	this->fail_count = 0;
	this->params = *address_params;
	this->address = address;
	for (int i = 0; i < VIRTUAL_GROUP_SIZE; i++)
		this->consistent_hash[i] = std_hash(address + "|v" + std::to_string(i));

	if (this->params.weight == 0)
		this->params.weight = 1;

	if (this->params.max_fails == 0)
		this->params.max_fails = 1;

	if (this->params.group_id < 0)
		this->params.group_id = -1;

	if (arr.size() == 0)
		this->host = "";
	else
		this->host = arr[0];

	if (arr.size() <= 1)
	{
		this->port = "";
		this->port_value = 0;
	}
	else
	{
		this->port = arr[1];
		this->port_value = atoi(arr[1].c_str());
	}
}

Upstream::Upstream():
	rwlock_(PTHREAD_RWLOCK_INITIALIZER),
	total_weight_(0),
	available_weight_(0),
	select_callback_(nullptr),
	consistent_callback_(nullptr),
	try_another_(false),
	is_consistent_(false)
{
	group_map_.rb_node = NULL;
	default_group_ = new UpstreamGroup(-1, this);
	rb_link_node(&default_group_->rb, NULL, &group_map_.rb_node);
	rb_insert_color(&default_group_->rb, &group_map_);
}

Upstream::~Upstream()
{
	UpstreamGroup *group;

	while (group_map_.rb_node)
	{
		group = rb_entry(group_map_.rb_node, UpstreamGroup, rb);
		rb_erase(group_map_.rb_node, &group_map_);
		delete group;
	}
}

void Upstream::lose_one_server(UpstreamGroup *group, const UpstreamAddress *ua)
{
	if (--group->nalive == 0 && ua->params.group_id >= 0)
		available_weight_ -= group->weight;

	if (ua->params.group_id < 0 && ua->params.server_type == SERVER_TYPE_MASTER)
		available_weight_ -= ua->params.weight;
}

void Upstream::gain_one_server(UpstreamGroup *group, const UpstreamAddress *ua)
{
	if (group->nalive++ == 0 && ua->params.group_id >= 0)
		available_weight_ += group->weight;

	if (ua->params.group_id < 0 && ua->params.server_type == SERVER_TYPE_MASTER)
		available_weight_ += ua->params.weight;
}

int Upstream::add(UpstreamAddress *ua)
{
	int group_id = ua->params.group_id;
	rb_node **p = &group_map_.rb_node;
	rb_node *parent = NULL;
	UpstreamGroup *group;
	WriteLock lock(rwlock_);

	server_map_[ua->address].push_back(ua);
	while (*p)
	{
		parent = *p;
		group = rb_entry(*p, UpstreamGroup, rb);

		if (group_id < group->group_id)
			p = &(*p)->rb_left;
		else if (group_id > group->group_id)
			p = &(*p)->rb_right;
		else
			break;
	}

	if (*p == NULL)
	{
		group = new UpstreamGroup(group_id, this);
		rb_link_node(&group->rb, parent, p);
		rb_insert_color(&group->rb, &group_map_);
	}

	if (ua->params.server_type == SERVER_TYPE_MASTER)
	{
		total_weight_ += ua->params.weight;
		masters_.push_back(ua);
	}

	group->mutex.lock();
	gain_one_server(group, ua);
	ua->group = group;
	if (ua->params.server_type == SERVER_TYPE_MASTER)
	{
		group->weight += ua->params.weight;
		group->masters.push_back(ua);
	}
	else
		group->slaves.push_back(ua);

	group->mutex.unlock();

	return 0;
}

int Upstream::del(const std::string& address)
{
	WriteLock lock(rwlock_);
	const auto map_it = server_map_.find(address);
	if (map_it != server_map_.end())
	{
		for (auto ua : map_it->second)
		{
			auto *group = ua->group;
			std::vector<UpstreamAddress*> *vec;

			if (ua->params.server_type == SERVER_TYPE_MASTER)
			{
				total_weight_ -= ua->params.weight;
				vec = &group->masters;
			} else 
				vec = &group->slaves;
			
			std::lock_guard<std::mutex> lock(group->mutex);

			ua->group = NULL;
			if (ua->fail_count < ua->params.max_fails)
				lose_one_server(group, ua);
			
			if (ua->params.server_type == SERVER_TYPE_MASTER)
				group->weight -= ua->params.weight;
			
			for (auto it = vec->begin(); it != vec->end(); it++)
			{
				if (*it == ua)
				{
					vec->erase(it);
					break;
				}
			}
		}

		server_map_.erase(map_it);
	}

	int n = (int)masters_.size();
	int new_n = 0;

	for (int i = 0; i < n; i++)
	{
		if (masters_[i]->address != address)
		{
			if (new_n != i)
				masters_[new_n++] = masters_[i];
			else 
				new_n++;
		}
	}

	if (new_n < n)
	{
		masters_.resize(new_n);
		return n - new_n;
	}

	return 0;
}

void Upstream::disable_server(const std::string& address)
{
	ReadLock lock(rwlock_);
	const auto map_it = server_map_.find(address);
	
	if (map_it != server_map_.cend())
	{
		for (auto ua : map_it->second)
		{
			auto *group = ua->group;

			if (group)
			{
				std::lock_guard<std::mutex> lock(group->mutex);

				ua->fail_count = ua->params.max_fails;
				if (ua->group == group && !ua->list.next)
				{
					ua->broken_timeout = GET_CURRENT_SECOND + MTTR_SECOND;
					list_add_tail(&ua->list, &group->breaker_list);
					group->nbreak++;
					group->upstream->lose_one_server(group, ua);
				}
			} else 
				ua->fail_count = ua->params.max_fails;
		}
	}
}

void Upstream::enable_server(const std::string& address)
{
	ReadLock lock(rwlock_);
	const auto map_it = server_map_.find(address);

	if (map_it != server_map_.cend())
	{
		for (auto ua : map_it->second)
			UpstreamManager::notify_available(ua);
	}
}

void Upstream::get_all_master(std::vector<std::string>& addr_list)
{
	ReadLock lock(rwlock_);
	for (const auto *master : masters_)
		addr_list.push_back(master->address);
}

static inline const UpstreamAddress *__check_get_strong(const UpstreamAddress *ua)
{
	if (ua->fail_count >= ua->params.max_fails)
	{
		if (ua->params.group_id < 0)
			ua = NULL;
		else 
			ua = ua->group->get_one();
	}
	return ua;
}

static inline const UpstreamAddress *__check_get_weak(const UpstreamAddress *ua)
{
	if (ua && ua->fail_count >= ua->params.max_fails && ua->params.group_id >= 0)
	{
		const auto *ret = ua->group->get_one();

		if (ret)
			ua = ret;
	}
	return ua;
}

static inline bool __is_alive_or_group_alive(const UpstreamAddress *ua)
{
	return (ua->params.group_id >= 0 && ua->group->nalive > 0)
		|| (ua->params.group_id < 0 && ua->fail_count < ua->params.max_fails);
}

const UpstreamAddress *Upstream::weighted_random_try_another() const 
{
	if (available_weight_ == 0) 
		return NULL;
	const UpstreamAddress *ua = NULL;
	int x = rand() % available_weight_;
	int s = 0;

	for (const auto *master : masters_)
	{
		if (__is_alive_or_group_alive(master))
		{
			ua = master;
			s += master->params.weight;
			if (s > x)
				break;
		}
	}
	return __check_get_weak(ua);
}

const UpstreamAddress *Upstream::consistent_hash_select(unsigned int hash) const
{
	const UpstreamAddress *ua = NULL;
	unsigned int min_dis = (unsigned int)-1;

	for (const auto *master : masters_)
	{
		if (__is_alive_or_group_alive(master))
		{
			for (int i = 0; i < VIRTUAL_GROUP_SIZE; i++)
			{
				unsigned int dis = std::min<unsigned int>(hash - master->consistent_hash[i], master->consistent_hash[i] - hash);
				if (dis < min_dis)
				{
					min_dis = dis;
					ua = master;
				}
			}
		}
	}
	return __check_get_weak(ua);
}

void Upstream::notify_available(UpstreamAddress *ua)
{
	auto *group = ua->group;

	if (group)
	{
		std::lock_guard<std::mutex> lock(group->mutex);

		if (ua->list.next) // in the list
		{
			if (ua->group == group && ua->fail_count >= ua->params.max_fails)
				group->upstream->gain_one_server(group, ua);
			list_del(&ua->list);
			ua->list.next = NULL;
		}
		ua->fail_count = 0;
	} else 
		ua->fail_count = 0;
}

void Upstream::notify_unavailable(UpstreamAddress *ua) {
	auto *group = ua->group;

	if (group)
	{
		std::lock_guard<std::mutex> lock(group->mutex);

		if (++ua->fail_count == ua->params.max_fails && ua->group == group && !ua->list.next)
		{
			ua->broken_timeout = GET_CURRENT_SECOND + MTTR_SECOND;
			list_add_tail(&ua->list, &group->breaker_list);
			group->nbreak++;
			group->upstream->lose_one_server(group, ua);
		}
	}
	else
		++ua->fail_count;
}

void Upstream::check_one_breaker(UpstreamGroup *group, int64_t cur_time)
{
	struct list_head *pos, *tmp;
	UpstreamAddress *ua;

	if (group->nbreak == 0)
		return;
	
	std::lock_guard<std::mutex> lock(group->mutex);

	list_for_each_safe(pos, tmp, &group->breaker_list)
	{
		ua = list_entry(pos, UpstreamAddress, list);
		if (cur_time >= ua->broken_timeout)
		{
			if (ua->fail_count >= ua->params.max_fails)
			{
				ua->fail_count = ua->params.max_fails - 1;
				if (ua->group == group)
					gain_one_server(group, ua);
			}

			list_del(pos);
			ua->list.next = NULL;
			group->nbreak--;
		}
	}
}

void Upstream::check_all_breaker()
{
	if (!group_map_.rb_node)
		return;
	struct rb_node *cur = rb_first(&group_map_);
	UpstreamGroup *group;
	int64_t cur_time = GET_CURRENT_SECOND;

	while (cur)
	{
		group = rb_entry(cur, UpstreamGroup, rb);
		check_one_breaker(group, cur_time);
		cur = rb_next(cur);
	}
}

const UpstreamAddress *Upstream::get(const ParsedURI& uri)
{
	unsigned int idx;
	unsigned int hash_value;
	const UpstreamAddress *ua;

	if (is_consistent_) // consistent mode
	{
		if (consistent_callback_)
		{
			hash_value = consistent_callback_(uri.path ? uri.path : "",
											  uri.query ? uri.query : "",
											  uri.fragment ? uri.fragment : "");
		} else 
		{
			hash_value = __default_consistent_hash(uri.path ? uri.path : "",
												   uri.query ? uri.query : "",
												   uri.fragment ? uri.fragment : "");
		}
		ReadLock lock(rwlock_);
		check_all_breaker();
		ua = consistent_hash_select(hash_value);
	} else 
	{
		ReadLock lock(rwlock_);
		unsigned int n = (unsigned int)masters_.size();

		if (n == 0)
			return NULL;
		else if (n == 1)
			idx = 0;
		else if (select_callback_)
		{
			idx = select_callback_(uri.path ? uri.path : "",
								   uri.query ? uri.query : "",
								   uri.fragment ? uri.fragment : "");
			if (idx >= n)
				idx %= n;
		} else 
		{
			int x = 0;
			int s = 0;
			if (total_weight_ > 0)
				x = rand() % total_weight_;
			for (idx = 0; idx < n; idx++)
			{
				s += masters_[idx]->params.weight;
				if (s > x)
					break;
			}
			if (idx == n)
				idx = n-1;
		}

		ua = masters_[idx];
		if (ua->fail_count >= ua->params.max_fails)
		{
			check_all_breaker();
			ua = __check_get_strong(ua);
			if (!ua && try_another_)
			{
				if (!select_callback_) // weighted random mode
					ua = weighted_random_try_another();
				else // manual mode
				{
					if (consistent_callback_)
						hash_value = consistent_callback_(uri.path ? uri.path : "",
														  uri.query ? uri.query : "",
														  uri.fragment ? uri.fragment : "");
					else
						hash_value = __default_consistent_hash(uri.path ? uri.path : "",
															   uri.query ? uri.query : "",
															   uri.fragment ? uri.fragment : "");
					ua = consistent_hash_select(hash_value);
				}
			}
		}
	}

	if (!ua)
		ua = default_group_->get_one_slave(); // get one slave from group[-1]
	return ua;
}

int Upstream::set_select_callback(upstream_route_t&& select_callback)
{
	WriteLock lock(rwlock_);
	select_callback_ = std::move(select_callback);
	return 0;
}

int Upstream::set_consistent_mode(upstream_route_t&& consistent_callback)
{
	WriteLock lock(rwlock_);
	is_consistent_ = true;
	consistent_callback_ = std::move(consistent_callback);
	return 0;
}
int Upstream::set_attr(bool try_another, upstream_route_t rehash_callback)
{
	WriteLock lock(rwlock_);
	try_another_ = try_another;
	consistent_callback_ = std::move(rehash_callback);
	return 0;
}

class __UpstreamManager
{
public:
	static __UpstreamManager *get_instance()
	{
		static __UpstreamManager kInstance;
		return &kInstance;
	}

	int upstream_create(const std::string& name,
						upstream_route_t&& consistent_hash)
	{
		Upstream *upstream = NULL;
		{
			WriteLock lock(rwlock_);

			if (upstream_map_.find(name) == upstream_map_.end())
				upstream = &upstream_map_[name];
		}

		if (upstream)
			return upstream->set_consistent_mode(std::move(consistent_hash));

		return -1;
	}

	int upstream_create(const std::string& name, bool try_another)
	{
		Upstream *upstream = NULL;
		{
			WriteLock lock(rwlock_);
			if (upstream_map_.find(name) == upstream_map_.end())
				upstream = &upstream_map_[name];
		}
		if (upstream)
			return upstream->set_attr(try_another, nullptr);
		return -1;
	}

	int upstream_create(const std::string& name, upstream_route_t&& select,
						bool try_another, upstream_route_t&& consistent_hash)
	{
		Upstream *upstream = NULL;
		{
			WriteLock lock(rwlock_);
			if (upstream_map_.find(name) == upstream_map_.end())
				upstream = &upstream_map_[name];
		}
		if (upstream)
		{
			upstream->set_select_callback(std::move(select));
			upstream->set_attr(try_another, std::move(consistent_hash));
			return 0;
		}
		return 0;
	}

	int upstream_add_server(const std::string& name,
							const std::string& address,
							const AddressParams *address_params)
	{
		auto *ua = new UpstreamAddress(address, address_params);
		{
			WriteLock lock(rwlock_);
			addresses_.push_back(ua);
		}

		Upstream *upstream = NULL;
		{
			ReadLock lock(rwlock_);
			auto it = upstream_map_.find(name);

			if (it != upstream_map_.end())
				upstream = &it->second;
		}

		if (upstream)
			return upstream->add(ua);
		return -1;
	}

	int upstream_remove_server(const std::string& name, const std::string& address)
	{
		Upstream *upstream = NULL;
		{
			ReadLock lock(rwlock_);
			auto it = upstream_map_.find(name);
			if (it != upstream_map_.end())
				upstream = &it->second;
		}
		if (upstream)
			return upstream->del(address);
		return -1;
	}

	int upstream_replace_server(const std::string& name,
								const std::string& address,
								const AddressParams *address_params)
	{
		Upstream *upstream = NULL;
		auto *ua = new UpstreamAddress(address, address_params);
		WriteLock lock(rwlock_);

		addresses_.push_back(ua);
		auto it = upstream_map_.find(name);

		if (it != upstream_map_.end())
			upstream = &it->second;
		if (upstream)
		{
			upstream->del(address);
			return upstream->add(ua);
		}
		return -1;
	}	

	int upstream_choose(ParsedURI& uri, UpstreamManager::UpstreamResult& result)
	{
		result.cookie = NULL;
		result.address_params = NULL;
		result.state = UPSTREAM_NOTFOUND;

		if (uri.state != URI_STATE_SUCCESS || !uri.host)
			return 0; // UPSTREAM_NOTFOUND
		
		Upstream *upstream;
		{
			ReadLock lock(rwlock_);
			auto it = upstream_map_.find(uri.host);
			if (it == upstream_map_.end())
				return 0; // UPSTREAM_NOTFOUND
			upstream = &it->second;
		}

		const auto *ua = upstream->get(uri);
		if (!ua)
		{
			result.state = UPSTREAM_ALL_DOWN;
			return 0;
		}

		char *host = NULL;
		char *port = NULL;

		if (!ua->host.empty())
		{
			host = strdup(ua->host.c_str());
			if (!host)
				return -1;
		}

		if (ua->port_value > 0)
		{
			port = strdup(ua->port.c_str());
			if (!port)
			{
				free(host);
				return -1;
			}
			free(uri.port);
			uri.port = port;
		}

		free(uri.host);
		uri.host = host;

		result.state = UPSTREAM_SUCCESS;
		result.address_params = &ua->params;
		result.cookie = const_cast<UpstreamAddress *>(ua);
		return 0;
	}	
							
private:
	__UpstreamManager():
		rwlock_(PTHREAD_RWLOCK_INITIALIZER)
	{}

	~__UpstreamManager()
	{
		for (auto *ua : addresses_)
			delete ua;
	}

private:
	pthread_rwlock_t rwlock_;
	std::unordered_map<std::string, Upstream> upstream_map_;
	std::vector<UpstreamAddress *> addresses_;
};

void UpstreamManager::notify_available(void *cookie)
{
	if (cookie)
		Upstream::notify_available((UpstreamAddress *)cookie);
}

void UpstreamManager::notify_unavailable(void *cookie)
{
	if (cookie)
		Upstream::notify_unavailable((UpstreamAddress *)cookie);
}

int UpstreamManager::choose(ParsedURI& uri, UpstreamResult& result)
{
	auto *manager = __UpstreamManager::get_instance();
	return manager->upstream_choose(uri, result);
}