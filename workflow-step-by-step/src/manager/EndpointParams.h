#ifndef _CONNECTIONPARAMS_H_
#define _CONNECTIONPARAMS_H_

#include <stddef.h>

/**
 * @file   EndpointParams.h
 * @brief  Network config for client task
 */

enum TransportType
{
	TT_TCP,
	TT_UDP,
	TT_SCTP,
	TT_TCP_SSL,
	TT_SCTP_SSL,
};

struct EndpointParams
{
	size_t max_connections;
	int connect_timeout;
	int response_timeout;
	int ssl_connect_timeout;
};

static constexpr struct EndpointParams ENDPOINT_PARAMS_DEFAULT =
{
	.max_connections		= 200,
	.connect_timeout		= 10 * 1000,
	.response_timeout		= 10 * 1000,
	.ssl_connect_timeout	= 10 * 1000,
};

#endif