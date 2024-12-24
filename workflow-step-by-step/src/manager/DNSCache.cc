#include <stdint.h>
#include <chrono>
#include "DNSCache.h"

#define GET_CURRENT_SECOND	std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now().time_since_epoch()).count()

#define CONFIDENT_INC		10
#define	TTL_INC				10

const DNSHandle *DNSCache::get_inner(const HostPort& host_port, int type)
{
	const DNSHandle *handle = cache_pool_.get(host_port);

	if (handle)
	{
		int64_t cur_time = GET_CURRENT_SECOND;

		switch (type)
		{
		case GET_TYPE_TTL:
			if (cur_time > handle->value.expire_time)
			{
				std::lock_guard<std::mutex> lock(mutex_);

				if (cur_time > handle->value.expire_time)
				{
					const_cast<DNSHandle *>(handle)->value.expire_time += TTL_INC;
					cache_pool_.release(handle);
					return NULL;
				}
			}

			break;

		case GET_TYPE_CONFIDENT:
			if (cur_time > handle->value.confident_time)
			{
				std::lock_guard<std::mutex> lock(mutex_);

				if (cur_time > handle->value.confident_time)
				{
					const_cast<DNSHandle *>(handle)->value.confident_time += CONFIDENT_INC;
					cache_pool_.release(handle);
					return NULL;
				}
			}

			break;

		default:
			break;
		}
	}

	return handle;
}

const DNSHandle *DNSCache::put(const HostPort& host_port,
							   struct addrinfo *addrinfo,
							   unsigned int dns_ttl_default,
							   unsigned int dns_ttl_min)
{
	int64_t expire_time;
	int64_t confident_time;
	int64_t cur_time = GET_CURRENT_SECOND;

	if (dns_ttl_min > dns_ttl_default)
		dns_ttl_min = dns_ttl_default;

	if (dns_ttl_min == (unsigned int)-1)
		confident_time = INT64_MAX;
	else
		confident_time = cur_time + dns_ttl_min;

	if (dns_ttl_default == (unsigned int)-1)
		expire_time = INT64_MAX;
	else
		expire_time = cur_time + dns_ttl_default;

	std::lock_guard<std::mutex> lock(mutex_);

	return cache_pool_.put(host_port, {addrinfo, confident_time, expire_time});
}