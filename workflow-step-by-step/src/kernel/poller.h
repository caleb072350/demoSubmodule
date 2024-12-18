#ifndef _POLLER_H
#define _POLLER_H

#include <sys/types.h>
#include <sys/socket.h>
#include <openssl/ssl.h>

typedef struct __poller poller_t;
typedef struct __poller_queue poller_queue_t;
typedef struct __poller_message poller_message_t;

struct __poller_message
{
    int (*append)(const void*, size_t *, poller_message_t *);
    char data[0];
};



struct poller_data
{
    short operation;
    unsigned short iovcnt;
    int fd;
    union
    {
        SSL *ssl;
        void *(*accept)(const struct sockaddr*, socklen_t, int, void *);
        void *(*event)(void *);
        void *(*notify)(void *, void *);
    };
    void *context;
    union {
        poller_message_t *message;
        struct iovec *write_iov;
        void *result;
    };
};

struct poller_params
{
    size_t max_open_files;
    poller_queue_t *result_queue;
    poller_message_t *(*create_message)(void *);
    int (*partial_written_(size_t, void *));
};

#ifdef __cplusplus
extern "C"
{
#endif

poller_t *poller_create(const struct poller_params *params);

#ifdef __cplusplus
}
#endif

#endif