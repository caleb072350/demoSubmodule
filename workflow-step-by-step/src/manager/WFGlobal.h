#ifndef _WFGLOBAL_H_
#define _WFGLOBAL_H_

#if __cplusplus < 201100
#error CPLUSPLUS VERSION required at least C++11. Please use "-std=c++11".
#include <C++11_REQUIRED>
#endif

#include <openssl/ssl.h>
#include <string>
#include "CommScheduler.h"
#include "DNSCache.h"
#include "RouteManager.h"
#include "Executor.h"
#include "EndpointParams.h"

/**
 * @file    WFGlobal.h
 * @brief   Workflow Global Settings & Workflow Global APIs
 */

/**
 * @brief   Workflow Library Global Setting
 * @details
 * If you want set different settings with default, please call WORKFLOW_library_init at the beginning of the process
*/

struct WFGlobalSettings
{
	struct EndpointParams endpoint_params;
	unsigned int dns_ttl_default;	///< in seconds, DNS TTL when network request success
	unsigned int dns_ttl_min;		///< in seconds, DNS TTL when network request fail
	int dns_threads;
	int poller_threads;
	int handler_threads;
	int compute_threads;			///< auto-set by system CPU number if value<=0
};

/**
 * @brief   Default Workflow Library Global Settings
 */
static constexpr struct WFGlobalSettings GLOBAL_SETTINGS_DEFAULT =
{
	.endpoint_params	=	ENDPOINT_PARAMS_DEFAULT,
	.dns_ttl_default	=	12 * 3600,
	.dns_ttl_min		=	180,
	.dns_threads		=	1,
	.poller_threads		=	1,
	.handler_threads	=	1,
	.compute_threads	=	-1,
};

/**
 * @brief      Reset Workflow Library Global Setting
 * @param[in]  settings          custom settings pointer
*/
extern void WORKFLOW_library_init(const struct WFGlobalSettings *settings);

/**
 * @brief   Workflow Global Management Class
 * @details Workflow Global APIs
 */
class WFGlobal
{
public:
    /**
	 * @brief      register default port for one scheme string
	 * @param[in]  scheme           scheme string
	 * @param[in]  port             default port value
	 * @warning    No effect when scheme is "http"/"https"/"redis"/"rediss"/"mysql"/"kafka"
	 */
	static void register_scheme_port(const std::string& scheme, unsigned short port);
    /**
	 * @brief      get default port string for one scheme string
	 * @param[in]  scheme           scheme string
	 * @return     port string const pointer
	 * @retval     NULL             fail, scheme not found
	 * @retval     not NULL         success
	 */
	static const char *get_default_port(const std::string& scheme);
    /**
	 * @brief      get current global settings
	 * @return     current global settings const pointer
	 * @note       returnval never NULL
	 */
	static const struct WFGlobalSettings *get_global_settings();

    static const char *get_error_string(int state, int error);

public:
	/// @brief Internal use only
	static CommScheduler *get_scheduler();
	/// @brief Internal use only
	static DNSCache *get_dns_cache();
	/// @brief Internal use only
	static RouteManager *get_route_manager();
	/// @brief Internal use only
	static ExecQueue *get_dns_queue();
	/// @brief Internal use only
	static Executor *get_dns_executor();
};
#endif