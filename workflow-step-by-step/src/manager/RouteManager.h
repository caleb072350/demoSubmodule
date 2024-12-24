#ifndef _ROUTEMANAGER_H_
#define _ROUTEMANAGER_H_

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string>
#include <mutex>
#include "rbtree.h"
#include "WFConnection.h"
#include "EndpointParams.h"
#include "CommScheduler.h"

class RouteManager
{
public:
    class RouteResult
	{
	public:
		void *cookie;
		CommSchedObject *request_object;

	public:
	//	RouteResult(): cookie(NULL), request_object(NULL) { }
		void clear() { cookie = NULL; request_object = NULL; }
	};

    class RouteTarget : public CommSchedTarget
	{
	public:
		int state;

	private:
		virtual CommConnection *new_connection(int)
		{
			return new WFConnection;
		}

	public:
		RouteTarget() : state(0) { }
	};

public:
	int get(TransportType type,
			const struct addrinfo *addrinfo,
			const std::string& other_info,
			const struct EndpointParams *endpoint_params,
			RouteResult& result);

	RouteManager()
	{
		cache_.rb_node = NULL;
	}

	~RouteManager();

private:
	std::mutex mutex_;
	struct rb_root cache_;

public:
	static void notify_unavailable(void *cookie, CommTarget *target);
	static void notify_available(void *cookie, CommTarget *target);
};

#endif