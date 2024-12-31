#ifndef _WFTASKFACTORY_H_
#define _WFTASKFACTORY_H_

#include <functional>
#include "URIParser.h"
// #include "RedisMessage.h"
#include "HttpMessage.h"
// #include "MySQLMessage.h"
#include "DNSRoutine.h"
#include "WFTask.h"
#include "Workflow.h"
#include "EndpointParams.h"
// #include "WFAlgoTaskFactory.h"

// Network Client/Server tasks

using WFHttpTask = WFNetworkTask<protocol::HttpRequest,
								 protocol::HttpResponse>;
using http_callback_t = std::function<void (WFHttpTask *)>;

// DNS task. For internal usage only.
using WFDNSTask = WFThreadTask<DNSInput, DNSOutput>;
using dns_callback_t = std::function<void (WFDNSTask *)>;

class WFTaskFactory
{
public:
	static WFHttpTask *create_http_task(const std::string& url,
										int redirect_max,
										int retry_max,
										http_callback_t callback);

	static WFDNSTask *create_dns_task(const std::string& host,
									  unsigned short port,
									  dns_callback_t callback);										
};

template<class INPUT, class OUTPUT>
class WFThreadTaskFactory
{
private:
	using T = WFThreadTask<INPUT, OUTPUT>;
	using MT = WFMultiThreadTask<INPUT, OUTPUT>;

public:
	// static T *create_thread_task(const std::string& queue_name,
	// 							 std::function<void (INPUT *, OUTPUT *)> routine,
	// 							 std::function<void (T *)> callback);

	// static T *create_thread_task(const std::string& queue_name,
	// 							 INPUT input,
	// 							 std::function<void (INPUT *, OUTPUT *)> routine,
	// 							 std::function<void (T *)> callback);

	// static MT *create_multi_thread_task(const std::string& queue_name,
	// 									std::function<void (INPUT *, OUTPUT *)> routine,
	// 									size_t nthreads,
	// 									std::function<void (MT *)> callback);

public:
	static T *create_thread_task(ExecQueue *queue, Executor *executor,
								 std::function<void (INPUT *, OUTPUT *)> routine,
								 std::function<void (T *)> callback);

	// static T *create_thread_task(ExecQueue *queue, Executor *executor,
	// 							 INPUT input,
	// 							 std::function<void (INPUT *, OUTPUT *)> routine,
	// 							 std::function<void (T *)> callback);
};

#include "WFTaskFactory.inl"

#endif