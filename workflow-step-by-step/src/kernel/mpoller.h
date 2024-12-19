#ifndef _MPOLLER_H_
#define _MPOLLER_H_

#include <sys/types.h>
#include <sys/socket.h>
#include "poller.h"

typedef struct __mpoller mpoller_t;

#ifdef __cplusplus
extern "C"
{
#endif

mpoller_t *mpoller_create(const struct poller_params *params, size_t nthreads);
int mpoller_start(mpoller_t *mpoller);
void mpoller_stop(mpoller_t *mpoller);
void mpoller_destroy(mpoller_t *mpoller);

#ifdef __cplusplus
}
#endif

struct __mpoller
{
	unsigned int nthreads;
	poller_t *poller[1];
};

static inline int mpoller_add(const struct poller_data *data, int timeout,
							  mpoller_t *mpoller)
{
	unsigned int index = (unsigned int)data->fd % mpoller->nthreads;
	return poller_add(data, timeout, mpoller->poller[index]);
}

static inline int mpoller_del(int fd, mpoller_t *mpoller)
{
	unsigned int index = (unsigned int)fd % mpoller->nthreads;
	return poller_del(fd, mpoller->poller[index]);
}

#endif