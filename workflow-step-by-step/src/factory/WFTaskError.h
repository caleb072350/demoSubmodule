#ifndef _WFTASKERROR_H_
#define _WFTASKERROR_H_

/**
 * @file    WFTaskError.h
 * @brief   Workflow Task Error Code List
 */

/**
 * @brief   Defination of task state code
 * @note    Only for WFNetworkTask and only when get_state()==WFT_STATE_TASK_ERROR
 */
enum
{
	//COMMON
	WFT_ERR_URI_PARSE_FAILED = 1001,            ///< URI, parse failed
	WFT_ERR_URI_SCHEME_INVALID = 1002,          ///< URI, invalid scheme
	WFT_ERR_URI_PORT_INVALID = 1003,            ///< URI, invalid port
	WFT_ERR_UPSTREAM_UNAVAILABLE = 1004,        ///< Upstream, all target server down
	WFT_ERR_ROUTE_FAILED = 1005,                ///< Route, route task is NULL

	//HTTP
	WFT_ERR_HTTP_BAD_REDIRECT_HEADER = 2001,    ///< Http, 301/302/303/307/308 Location header value is NULL

	//REDIS
	WFT_ERR_REDIS_ACCESS_DENIED = 3001,         ///< Redis, invalid password
	WFT_ERR_REDIS_COMMAND_DISALLOWED = 3002,    ///< Redis, command disabled, cannot be "AUTH"/"SELECT"

	//MYSQL
	WFT_ERR_MYSQL_HOST_NOT_ALLOWED = 4001,      ///< MySQL
	WFT_ERR_MYSQL_ACCESS_DENIED = 4002,         ///< MySQL, authentication failed
	WFT_ERR_MYSQL_INVALID_CHARACTER_SET = 4003, ///< MySQL, invalid charset, not found in MySQL-Documentation
	WFT_ERR_MYSQL_COMMAND_DISALLOWED = 4004,    ///< MySQL, sql command disabled, cannot be "USE"/"SET NAMES"/"SET CHARSET"/"SET CHARACTER SET"
	WFT_ERR_MYSQL_QUERY_NOT_SET = 4005          ///< MySQL, query not set sql, maybe forget please check
};

#endif
