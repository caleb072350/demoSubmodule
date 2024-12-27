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
	{ }

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
	// void switch_callback(WFTimerTask *task);

	RouteManager::RouteResult route_result_;
	// UpstreamManager::UpstreamResult upstream_result_;

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
		int ret = UpStreamManager::choose(uri_, upstream_result_);
	}
}

template<class REQ, class RESP, typename CTX>
SubTask *WFComplexClientTask<REQ, RESP, CTX>::route()
{
	return NULL;
}

template<class REQ, class RESP, typename CTX>
void WFComplexClientTask<REQ, RESP, CTX>::router_callback(SubTask *task)
{

}

template<class REQ, class RESP, typename CTX>
void WFComplexClientTask<REQ, RESP, CTX>::dispatch()
{

}

template<class REQ, class RESP, typename CTX>
SubTask *WFComplexClientTask<REQ, RESP, CTX>::done()
{
	return NULL;
}