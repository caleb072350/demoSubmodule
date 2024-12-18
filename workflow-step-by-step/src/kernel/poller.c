#include "poller.h"
#include <sys/epoll.h>
#include <sys/timerfd.h>
#include <time.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/time.h>
#include "list.h"
#include "rbtree.h"
#include <linux/time.h>

#define POLLER_BUFSIZE			(256 * 1024)

struct __poller_queue
{
    size_t res_max;
    size_t res_cnt;
    int nonblock;
    struct list_head res_list1;
    struct list_head res_list2;
    struct list_head *get_list;
    struct list_head *put_list;
    pthread_mutex_t  get_mutex;
    pthread_mutex_t  put_mutex;
    pthread_cond_t   put_cond;
    pthread_cond_t   get_cond;
};

struct __poller
{
    struct poller_params params;
    pthread_t tid;
    int pfd;
    int timerfd;
    int pipe_rd;
    int pipe_wr;
    int stopping;
    int stopped;
    struct rb_root timeo_tree;
    struct rb_node *tree_first;
    struct list_head timeo_list;
    struct __poller_node **nodes;
    pthread_mutex_t mutex;
    char buf[POLLER_BUFSIZE];
};

static inline int __poller_create_pfd()
{
    return epoll_create(1);
}

static inline int __poller_add_fd(int fd, int event, void *data, poller_t *poller)
{
    struct epoll_event ev = {
        .events = event,
        .data = {
            .ptr = data
        }
    };
    return epoll_ctl(poller->pfd, EPOLL_CTL_ADD, fd, &ev);
}

static inline int __poller_mod_fd(int fd, int old_event,
                                  int new_event, void *data, 
                                  poller_t *poller)
{
    struct epoll_event ev = {
        .events = new_event,
        .data = {
            .ptr = data
        }
    };
    return epoll_ctl(poller->pfd, EPOLL_CTL_MOD, fd, &ev);
}

static inline __poller_create_timerfd() {
    return timerfd_create(CLOCK_MONOTONIC, 0);
}

static inline int __poller_add_timerfd(int fd, poller_t *poller) {
    struct epoll_event ev = {
        .events = EPOLLIN,
        .data = {
            .ptr = NULL
        }
    };
    return epoll_ctl(poller->pfd, EPOLL_CTL_ADD, fd, &ev);
}

static inline int __poller_set_timerfd(int fd, const struct timespec *abstime,
                                       poller_t *poller)
{
    struct itimerspec timer = {
        .it_interval = {},
        .it_value = *abstime
    };
    return timerfd_settime(fd, TFD_TIMER_ABSTIME, &timer, NULL);
}

typedef struct epoll_event __poller_event_t;

static inline int __poller_wait(__poller_event_t *events, int maxevents,
                                poller_t *poller)
{
    return epoll_wait(poller->pfd, events, maxevents, -1);
}

static inline void *__poller_event_data(const __poller_event_t *event) {
    return event->data.ptr;
}

void poller_queue_set_nonblock(poller_queue_t *queue)
{
    queue->nonblock = 1;
    pthread_mutex_lock(&queue->put_mutex);
    pthread_cond_signal(&queue->get_cond);
    pthread_mutex_unlock(&queue->put_mutex);
}

void poller_queue_set_block(poller_queue_t *queue) {
    queue->nonblock = 0;
}

static size_t __poller_queue_swap(poller_queue_t *queue)
{
    struct list_head *get_list = queue->get_list;
    size_t cnt;

    queue->get_list = queue->put_list;
    pthread_mutex_lock(&queue->put_mutex);
    while (queue->res_cnt == 0 && !queue->nonblock)
        pthread_cond_wait(&queue->get_cond, &queue->put_mutex);
    cnt = queue->res_cnt;
    if (cnt > queue->res_max - 1)
        pthread_cond_broadcast(&queue->put_cond);
    queue->put_list = get_list;
    queue->res_cnt = 0;
    pthread_mutex_unlock(&queue->put_mutex);
    return cnt;
}