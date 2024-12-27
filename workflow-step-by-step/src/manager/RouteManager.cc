#include <openssl/ssl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <chrono>
#include <mutex>
#include <vector>
#include <string>
#include <algorithm>
#include "list.h"
#include "rbtree.h"
#include "WFGlobal.h"
#include "WFConnection.h"
#include "MD5Util.h"
#include "CommScheduler.h"
#include "EndpointParams.h"
#include "RouteManager.h"
#include "StringUtil.h"

#define GET_CURRENT_SECOND	std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now().time_since_epoch()).count()
#define MTTR_SECOND			30

using RouteTargetTCP = RouteManager::RouteTarget;

class RouteTargetUDP : public RouteManager::RouteTarget
{
private:
	virtual int create_connect_fd()
	{
		const struct sockaddr *addr;
		socklen_t addrlen;

		this->get_addr(&addr, &addrlen);
		return socket(addr->sa_family, SOCK_DGRAM, 0);
	}
};

class RouteTargetSCTP : public RouteManager::RouteTarget
{
private:
	virtual int create_connect_fd()
	{
		const struct sockaddr *addr;
		socklen_t addrlen;

		this->get_addr(&addr, &addrlen);
		return socket(addr->sa_family, SOCK_STREAM, IPPROTO_SCTP);
	}
};

//  protocol_name\n user\n pass\n dbname\n ai_addr ai_addrlen \n....
//

struct RouterParams
{
	TransportType transport_type;
	const struct addrinfo *addrinfo;
	uint64_t md5_16;
	SSL_CTX *ssl_ctx;
	int connect_timeout;
	int ssl_connect_timeout;
	int response_timeout;
	size_t max_connections;
};

RouteManager::~RouteManager()
{
	
}