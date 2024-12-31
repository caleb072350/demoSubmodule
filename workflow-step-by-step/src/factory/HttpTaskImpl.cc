#include <assert.h>
#include <string>
#include "WFTaskError.h"
#include "WFTaskFactory.h"
#include "StringUtil.h"
#include "WFGlobal.h"
#include "HttpUtil.h"

using namespace protocol;

#define HTTP_KEEPALIVE_DEFAULT	(60 * 1000)
#define HTTP_KEEPALIVE_MAX		(300 * 1000)

class ComplexHttpTask : public WFComplexClientTask<HttpRequest, HttpResponse>
{
public:
	ComplexHttpTask(int redirect_max,
					int retry_max,
					http_callback_t&& callback):
		WFComplexClientTask(retry_max, std::move(callback)),
		redirect_max_(redirect_max),
		redirect_count_(0)
	{
		HttpRequest *client_req = this->get_req();

		client_req->set_method(HttpMethodGet);
		client_req->set_http_version("HTTP/1.1");
	}

protected:
	virtual CommMessageOut *message_out();
	virtual CommMessageIn *message_in();
	virtual int keep_alive_timeout();
	virtual bool init_success();
	virtual void init_failed();
	virtual bool finish_once();

private:
	bool need_redirect();
	bool redirect_url(HttpResponse *client_resp);
	void set_empty_request();

	int redirect_max_;
	int redirect_count_;
};

CommMessageOut *ComplexHttpTask::message_out()
{
	return NULL;
}

CommMessageIn *ComplexHttpTask::message_in()
{
	return NULL;
}

int ComplexHttpTask::keep_alive_timeout()
{
	return 1;
}

bool ComplexHttpTask::init_success()
{
	HttpRequest *client_req = this->get_req();
	std::string request_uri;
	std::string header_host;
	bool is_ssl;
	bool is_unix = false;

	if (uri_.scheme && strcasecmp(uri_.scheme, "http") == 0)
		is_ssl = false;
	else if (uri_.scheme && strcasecmp(uri_.scheme, "https") == 0)
		is_ssl = true;
	else
	{
		this->state = WFT_STATE_TASK_ERROR;
		this->error = WFT_ERR_URI_SCHEME_INVALID;
		this->set_empty_request();
		return false;
	}

	//todo http+unix
	//https://stackoverflow.com/questions/26964595/whats-the-correct-way-to-use-a-unix-domain-socket-in-requests-framework
	//https://stackoverflow.com/questions/27037990/connecting-to-postgres-via-database-url-and-unix-socket-in-rails

	if (uri_.path && uri_.path[0])
		request_uri = uri_.path;
	else
		request_uri = "/";
	
	if (uri_.query && uri_.query[0])
	{
		request_uri += "?";
		request_uri += uri_.query;
	}

	if (uri_.host && uri_.host[0])
	{
		header_host = uri_.host;
		if (uri_.host[0] == '/')
			is_unix = true;
	}

	if (!is_unix && uri_.port && uri_.port[0])
	{
		int port = atoi(uri_.port);

		if (is_ssl)
		{
			if (port != 443)
			{
				header_host += ":";
				header_host += uri_.port;
			}
		}
		else
		{
			if (port != 80)
			{
				header_host += ":";
				header_host += uri_.port;
			}
		}
	}

	this->WFComplexClientTask::set_type(is_ssl ? TT_TCP_SSL : TT_TCP);
	client_req->set_request_uri(request_uri.c_str());
	client_req->set_header_pair("Host", header_host.c_str());

	return true;
}

void ComplexHttpTask::init_failed()
{

}

bool ComplexHttpTask::finish_once()
{
	return true;
}

bool ComplexHttpTask::need_redirect()
{
	return false;
}

bool ComplexHttpTask::redirect_url(HttpResponse *client_resp)
{
	return false;
}

void ComplexHttpTask::set_empty_request()
{
	
}

/**********Client Factory**********/

WFHttpTask *WFTaskFactory::create_http_task(const std::string& url,
											int redirect_max,
											int retry_max,
											http_callback_t callback)
{
	auto *task = new ComplexHttpTask(redirect_max,
									 retry_max,
									 std::move(callback));
	ParsedURI uri;

	URIParser::parse(url, uri);
	task->init(std::move(uri));
	task->set_keep_alive(HTTP_KEEPALIVE_DEFAULT);
	return task;
}