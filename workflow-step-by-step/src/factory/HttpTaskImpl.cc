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