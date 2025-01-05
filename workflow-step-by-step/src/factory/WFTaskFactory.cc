#include <ctype.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <string>
#include <mutex>
#include "list.h"
#include "rbtree.h"
#include "DNSRoutine.h"
#include "WFGlobal.h"
#include "WFTaskError.h"
#include "WFTaskFactory.h"

WFDNSTask *WFTaskFactory::create_dns_task(const std::string& host,
										  unsigned short port,
										  dns_callback_t callback)
{
	auto *task = WFThreadTaskFactory<DNSInput, DNSOutput>::
						create_thread_task(WFGlobal::get_dns_queue(),
										   WFGlobal::get_dns_executor(),
										   DNSRoutine::run,
										   std::move(callback));

	task->get_input()->reset(host, port);
	return task;
}

/********** RouterTask **********/
void WFRouterTask::dispatch()
{
    insert_dns_ = true;
    if (dns_cache_level_ != DNS_CACHE_LEVEL_0)
    {
        auto *dns_cache = WFGlobal::get_dns_cache();
        const DNSHandle *addr_handle = NULL;

        switch (dns_cache_level_)
        {
        case DNS_CACHE_LEVEL_1:
            addr_handle = dns_cache->get_confident(host_, port_);
            break;
        case DNS_CACHE_LEVEL_2:
            addr_handle = dns_cache->get_ttl(host_, port_);
            break;
        case DNS_CACHE_LEVEL_3:
            addr_handle = dns_cache->get(host_, port_);
            break;
        default:
            break;
        }

        if (addr_handle)
        {
            if (addr_handle->value.addrinfo)
            {
                auto *route_manager = WFGlobal::get_route_manager();
                struct addrinfo *addrinfo = addr_handle->value.addrinfo;
                struct addrinfo first;

                if (first_addr_only_ && addrinfo->ai_next)
                {
                    first = *addrinfo;
                    first.ai_next = NULL;
                    addrinfo = &first;
                }

                if (route_manager->get(type_, addrinfo, info_, &endpoint_params_, route_result_) < 0)
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
                    this->state = WFT_STATE_SUCCESS;
                insert_dns_ = false;
            }

            dns_cache->release(addr_handle);
        }
    }

    if (insert_dns_ && !host_.empty())
    {
        char front = host_.front();
        char back = host_.back();
        struct in6_addr addr;
        int ret;

        if (host_.find(':') != std::string::npos)
            ret = inet_pton(AF_INET6, host_.c_str(), &addr);
        else if (isdigit(back) && isdigit(front))
            ret = inet_pton(AF_INET, host_.c_str(), &addr);

        else if (front == '/')
            ret = 1;
        else
            ret = 0;
        
        if (ret == 1)
        {
            DNSInput dns_in;
            DNSOutput dns_out;

            dns_in.reset(host_, port_);
            DNSRoutine::run(&dns_in, &dns_out);
            dns_callback_internal(&dns_out, (unsigned int)-1, (unsigned int)-1);
            insert_dns_ = false;
        }
    }

    if (insert_dns_)
	{
		auto&& cb = std::bind(&WFRouterTask::dns_callback, this, std::placeholders::_1);
		WFDNSTask *dns_task = WFTaskFactory::create_dns_task(host_, port_, std::move(cb));

		series_of(this)->push_front(dns_task);
	}

	this->subtask_done();
}

SubTask* WFRouterTask::done()
{
	SeriesWork *series = series_of(this);

	if (!insert_dns_)
	{
		if (callback_)
			callback_(this);

		delete this;
	}

	return series->pop();
}

void WFRouterTask::dns_callback_internal(DNSOutput *dns_out,
										 unsigned int ttl_default,
										 unsigned int ttl_min)
{
	int dns_error = dns_out->get_error();
	if (dns_error)
	{
#ifdef EAI_SYSTEM
		if (dns_error == EAI_SYSTEM)
		{
			this->state = WFT_STATE_SYS_ERROR;
			this->error = errno;
		}
		else
#endif
		{
			this->state = WFT_STATE_DNS_ERROR;
			this->error = dns_error;
		}
	}
	else
	{
		struct addrinfo *addrinfo = dns_out->move_addrinfo();

		if (addrinfo)
		{
			auto *route_manager = WFGlobal::get_route_manager();
			auto *dns_cache = WFGlobal::get_dns_cache();
			const DNSHandle *addr_handle = dns_cache->put(host_, port_,
														  addrinfo,
														  (unsigned int)ttl_default,
														  (unsigned int)ttl_min);
			LOG_INFO("put {} {} addrinfo to dns_cache, ttl_default: {}, ttl_min: {}", host_, port_, ttl_default, ttl_min);
			if (route_manager->get(type_, addrinfo, info_, &endpoint_params_, route_result_) < 0)
			{
				this->state = WFT_STATE_SYS_ERROR;
				this->error = errno;
			}
			else if (!route_result_.request_object)
			{
				//should not happen
				this->state = WFT_STATE_SYS_ERROR;
				this->error = EAGAIN;
			}
			else
			{
				this->state = WFT_STATE_SUCCESS;
				LOG_INFO("get {} {} dns info success!", host_, port_);
			}

			dns_cache->release(addr_handle);
		}
		else
		{
			//system promise addrinfo not null, here should not happen
			this->state = WFT_STATE_SYS_ERROR;
			this->error = EINVAL;
		}
	}
}

void WFRouterTask::dns_callback(WFDNSTask *dns_task)
{
	if (dns_task->get_state() == WFT_STATE_SUCCESS)
		dns_callback_internal(dns_task->get_output(), dns_ttl_default_, dns_ttl_min_);
	else
	{
		this->state = dns_task->get_state();
		this->error = dns_task->get_error();
	}

	if (callback_)
		callback_(this);

	delete this;
}