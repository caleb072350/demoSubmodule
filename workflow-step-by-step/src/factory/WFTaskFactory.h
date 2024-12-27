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

class WFTaskFactory
{
public:
	static WFHttpTask *create_http_task(const std::string& url,
										int redirect_max,
										int retry_max,
										http_callback_t callback);
};

#include "WFTaskFactory.inl"

#endif