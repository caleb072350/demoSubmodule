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

public:
	UpstreamGroup(int group_id, Upstream *us):
		upstream(us),
		nbreak(0),
		nalive(0),
		weight(0),
		group_id(group_id)
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

	std::random_shuffle(this->masters.begin(), this->masters.end());
	for (const auto *master : this->masters)
	{
		if (master->fail_count < master->params.max_fails)
			return master;
	}

	std::random_shuffle(this->slaves.begin(), this->slaves.end());
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

	std::random_shuffle(this->slaves.begin(), this->slaves.end());
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