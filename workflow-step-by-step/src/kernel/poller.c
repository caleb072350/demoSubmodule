#include <sys/epoll.h>
#include <sys/timerfd.h>
#include <time.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/uio.h>
#include <errno.h>
#include "list.h"
#include "rbtree.h"
#include "poller.h"

#define POLLER_NODES_MAX		65536
#define POLLER_EVENTS_MAX		256
#define POLLER_NODE_ERROR		((struct __poller_node *)-1)

typedef struct __poller poller_t;

struct __poller_node
{
	int state;
	int error;
	struct poller_data data;
	union
	{
		struct list_head list;
		struct rb_node rb;
	};
	char in_rbtree;
	char removed;
	int event;
	struct timespec timeout;
	struct __poller_node *res;
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

static inline int __poller_del_fd(int fd, int event, poller_t *poller)
{
	return epoll_ctl(poller->pfd, EPOLL_CTL_DEL, fd, NULL);
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

static inline int __poller_create_timerfd() {
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

struct poller_result *poller_queue_get(poller_queue_t *queue)
{
	struct __poller_node *node;

	pthread_mutex_lock(&queue->get_mutex);
	if (!list_empty(queue->get_list) || __poller_queue_swap(queue) > 0)
	{
		node = list_entry(queue->get_list->next, struct __poller_node, list);
		list_del(&node->list);
	}
	else
	{
		node = NULL;
		errno = ENOENT;
	}

	pthread_mutex_unlock(&queue->get_mutex);
	return (struct poller_result *)node;
}

poller_queue_t *poller_queue_create(size_t maxlen)
{
	poller_queue_t *queue = (poller_queue_t *)malloc(sizeof (poller_queue_t));
	int ret;

	if (!queue)
		return NULL;

	ret = pthread_mutex_init(&queue->get_mutex, NULL);
	if (ret == 0)
	{
		ret = pthread_mutex_init(&queue->put_mutex, NULL);
		if (ret == 0)
		{
			ret = pthread_cond_init(&queue->get_cond, NULL);
			if (ret == 0)
			{
				ret = pthread_cond_init(&queue->put_cond, NULL);
				if (ret == 0)
				{
					queue->res_max = maxlen;
					INIT_LIST_HEAD(&queue->res_list1);
					INIT_LIST_HEAD(&queue->res_list2);
					queue->get_list = &queue->res_list1;
					queue->put_list = &queue->res_list2;
					queue->res_cnt = 0;
					queue->nonblock = 0;
					return queue;
				}

				pthread_cond_destroy(&queue->get_cond);
			}

			pthread_mutex_destroy(&queue->put_mutex);
		}

		pthread_mutex_destroy(&queue->get_mutex);
	}

	errno = ret;
	free(queue);
	return NULL;
}

void poller_queue_destroy(poller_queue_t *queue)
{
	pthread_cond_destroy(&queue->put_cond);
	pthread_cond_destroy(&queue->get_cond);
	pthread_mutex_destroy(&queue->put_mutex);
	pthread_mutex_destroy(&queue->get_mutex);
	free(queue);
}

static void __poller_add_result(struct __poller_node *res,
								poller_t *poller)
{
	poller_queue_t *queue = poller->params.result_queue;

	if (res->res)
		free(res->res);

	pthread_mutex_lock(&queue->put_mutex);
	while (queue->res_cnt > queue->res_max - 1 && !poller->stopping)
		pthread_cond_wait(&queue->put_cond, &queue->put_mutex);

	list_add_tail(&res->list, queue->put_list);
	queue->res_cnt++;
	pthread_mutex_unlock(&queue->put_mutex);
	pthread_cond_signal(&queue->get_cond);
}

static inline long __timeout_cmp(const struct __poller_node *node1,
								 const struct __poller_node *node2)
{
	long ret = node1->timeout.tv_sec - node2->timeout.tv_sec;

	if (ret == 0)
		ret = node1->timeout.tv_nsec - node2->timeout.tv_nsec;

	return ret;
}

static void __poller_tree_insert(struct __poller_node *node, poller_t *poller)
{
	struct rb_node **p = &poller->timeo_tree.rb_node;
	struct rb_node *parent = NULL;
	struct __poller_node *entry;
	int first = 1;

	while (*p)
	{
		parent = *p;
		entry = rb_entry(*p, struct __poller_node, rb);
		if (__timeout_cmp(node, entry) < 0)
			p = &(*p)->rb_left;
		else
		{
			p = &(*p)->rb_right;
			first = 0;
		}
	}

	if (first)
		poller->tree_first = &node->rb;

	node->in_rbtree = 1;
	rb_link_node(&node->rb, parent, p);
	rb_insert_color(&node->rb, &poller->timeo_tree);
}

static inline void __poller_tree_erase(struct __poller_node *node,
									   poller_t *poller)
{
	if (&node->rb == poller->tree_first)
		poller->tree_first = rb_next(&node->rb);

	rb_erase(&node->rb, &poller->timeo_tree);
	node->in_rbtree = 0;
}

static int __poller_remove_node(struct __poller_node *node, poller_t *poller)
{
	int removed;

	pthread_mutex_lock(&poller->mutex);
	removed = node->removed;
	if (!removed)
	{
		poller->nodes[node->data.fd] = NULL;

		if (node->in_rbtree)
			__poller_tree_erase(node, poller);
		else
			list_del(&node->list);

		__poller_del_fd(node->data.fd, node->event, poller);
	}

	pthread_mutex_unlock(&poller->mutex);
	return removed;
}

static int __poller_append_message(const void *buf, size_t *n,
								   struct __poller_node *node,
								   poller_t *poller)
{
	poller_message_t *msg = node->data.message;
	struct __poller_node *res;
	int ret;

	if (!msg)
	{
		res = (struct __poller_node *)malloc(sizeof (struct __poller_node));
		if (!res)
			return -1;

		msg = poller->params.create_message(node->data.context);
		if (!msg)
		{
			free(res);
			return -1;
		}

		node->data.message = msg;
		node->res = res;
	}
	else
		res = node->res;

	ret = msg->append(buf, n, msg);
	if (ret > 0)
	{
		res->data = node->data;
		res->error = 0;
		res->state = PR_ST_SUCCESS;
		res->res = NULL;
		__poller_add_result(res, poller);

		node->data.message = NULL;
		node->res = NULL;
	}

	return ret;
}

int __poller_data_get_event(int *event, const struct poller_data *data)
{
	switch (data->operation)
	{
	case PD_OP_READ:
		*event = EPOLLIN | EPOLLET;
		return !!data->message; // 双感叹号作用是将任意非零值归一化为布尔意义上的1，并显示表示这是一个布尔结果
    case PD_OP_LISTEN:
        *event = EPOLLIN | EPOLLET;
        return 1;
	}
}

static void __poller_node_set_timeout(int timeout, struct __poller_node *node)
{
	clock_gettime(CLOCK_MONOTONIC, &node->timeout);
	node->timeout.tv_sec += timeout / 1000;
	node->timeout.tv_nsec += (timeout % 1000) * 1000000;
	if (node->timeout.tv_nsec >= 1000000000)
	{
		node->timeout.tv_sec++;
		node->timeout.tv_nsec -= 1000000000;
	}
}

static void __poller_insert_node(struct __poller_node *node, poller_t *poller)
{
	struct __poller_node *end;
	end = list_entry(poller->timeo_list.prev, struct __poller_node, list);
	if (list_empty(&poller->timeo_list) || __timeout_cmp(node, end) >= 0)
		list_add_tail(&node->list, &poller->timeo_list);
	else
		__poller_tree_insert(node, poller);
	
	if (&node->list == poller->timeo_list.next) 
	{
		if (poller->tree_first)
			end = rb_entry(poller->tree_first, struct __poller_node, rb);
		else
			end = NULL;
	} else if (&node->rb == poller->tree_first)
		end = list_entry(poller->timeo_list.next, struct __poller_node, list);
	else
		return;
	if (!end || __timeout_cmp(node, end) < 0)
		__poller_set_timerfd(poller->timerfd, &node->timeout, poller);
}

int poller_add(const struct poller_data *data, int timeout, poller_t *poller)
{
	struct __poller_node *res = NULL;
	struct __poller_node *node;
	int need_res;
	int event;

	if ((size_t)data->fd >= poller->params.max_open_files)
	{
		errno = data->fd < 0 ? EBADF : EMFILE;
		return -1;
	}

	need_res = __poller_data_get_event(&event, data);
	if (need_res < 0)
		return -1;

	if (need_res)
	{
		res = (struct __poller_node *)malloc(sizeof (struct __poller_node));
		if (!res)
			return -1;
	}

	node = (struct __poller_node *)malloc(sizeof (struct __poller_node));
	if (node)
	{
		node->data = *data;
		node->event = event;
		node->in_rbtree = 0;
		node->removed = 0;
		node->res = res;
		if (timeout >= 0)
			__poller_node_set_timeout(timeout, node);

		pthread_mutex_lock(&poller->mutex);
		if (!poller->nodes[data->fd])
		{
			if (__poller_add_fd(data->fd, event, node, poller) >= 0)
			{
				if (timeout >= 0)
					__poller_insert_node(node, poller);
				else
					list_add_tail(&node->list, &poller->no_timeo_list);

				poller->nodes[data->fd] = node;
				node = NULL;
			}
		}
		else if (poller->nodes[data->fd] == POLLER_NODE_ERROR)
			errno = EINVAL;
		else
			errno = EEXIST;

		pthread_mutex_unlock(&poller->mutex);
		if (node == NULL)
			return 0;

		free(node);
	}

	free(res);
	return -1;
}

int poller_del(int fd, poller_t *poller)
{
	struct __poller_node *node;

	if ((size_t)fd >= poller->params.max_open_files)
	{
		errno = fd < 0 ? EBADF : EMFILE;
		return -1;
	}

	pthread_mutex_lock(&poller->mutex);
	node = poller->nodes[fd];
	if (node)
	{
		poller->nodes[fd] = NULL;

		if (node->in_rbtree)
			__poller_tree_erase(node, poller);
		else
			list_del(&node->list);

		__poller_del_fd(fd, node->event, poller);

		node->error = 0;
		node->state = PR_ST_DELETED;
		if (poller->stopped)
			__poller_add_result(node, poller);
		else
		{
			node->removed = 1;
			write(poller->pipe_wr, &node, sizeof (void *));
		}
	}
	else
		errno = ENOENT;

	pthread_mutex_unlock(&poller->mutex);
	return -!node;
}

static int __poller_create_timer(poller_t *poller)
{
	poller->timerfd = __poller_create_timerfd();
	if (poller->timerfd < 0)
		return -1;

	if (__poller_add_timerfd(poller->timerfd, poller) < 0)
	{
		close(poller->timerfd);
		return -1;
	}

	return 0;
}

poller_t *poller_create(const struct poller_params *params)
{
	poller_t *poller = (poller_t *)malloc(sizeof(poller_t));
	size_t n;
	int ret;

	if (!poller) {
		return NULL;
	}

	n = params->max_open_files;
	if (n == 0) 
		n = POLLER_NODES_MAX;
	
	poller->nodes = (struct __poller_node **)calloc(n, sizeof (struct __poller_node *));
	if (!poller->nodes)
	{
		free(poller);
		return NULL;
	}
	
	poller->pfd = __poller_create_pfd();
	if (poller->pfd < 0)
	{
		free(poller->nodes);
		free(poller);
		return NULL;
	}

	if (__poller_create_timer(poller) < 0)
	{
		close(poller->pfd);
		free(poller->nodes);
		free(poller);
		return NULL;
	}

	ret = pthread_mutex_init(&poller->mutex, NULL);
	if (ret == 0) 
	{
		poller->params = *params;
		poller->params.max_open_files = n;
		poller->timeo_tree.rb_node = NULL;
		poller->tree_first = NULL;
		INIT_LIST_HEAD(&poller->timeo_list);
		INIT_LIST_HEAD(&poller->no_timeo_list);
		poller->nodes[poller->timerfd] = POLLER_NODE_ERROR;
		poller->nodes[poller->pfd] = POLLER_NODE_ERROR;
		poller->stopped = 1;
		poller->stopping = 1;
		return poller;
	}

	errno = ret;
	close(poller->timerfd);
	close(poller->pfd);
	free(poller->nodes);
	free(poller);
	return NULL;
}

void poller_destroy(poller_t *poller)
{
	pthread_mutex_destroy(&poller->mutex);
	close(poller->timerfd);
	close(poller->pfd);
	free(poller->nodes);
	free(poller);
}

static int __poller_open_pipe(poller_t *poller)
{
	int pipefd[2];
	if (pipe(pipefd) < 0)
		return -1;
	if (__poller_add_fd(pipefd[0], EPOLLIN, (void*)1, poller) >= 0)
	{
		poller->pipe_rd = pipefd[0];
		poller->pipe_wr = pipefd[1];
		return 0;
	}
	close(pipefd[0]);
	close(pipefd[1]);
	return -1;
}

static void __poller_set_timer(poller_t *poller)
{
	struct __poller_node *node = NULL;
	struct __poller_node *first;
	struct timespec abstime;

	pthread_mutex_lock(&poller->mutex);
	if (!list_empty(&poller->timeo_list))
	 	node = list_entry(poller->timeo_list.next, struct __poller_node, list);
	
	if (poller->tree_first)
	{
		first = rb_entry(poller->tree_first, struct __poller_node, rb);
		if (!node || __timeout_cmp(first, node) < 0)
			node = first;
	}

	if (node)
	 	abstime = node->timeout;
	else
	{
		abstime.tv_sec = 0;
		abstime.tv_nsec = 0;
	}
	__poller_set_timerfd(poller->timerfd, &abstime, poller);
	pthread_mutex_unlock(&poller->mutex);
}

static void __poller_handle_listen(struct __poller_node *node,
                                   poller_t *poller)
{
    struct __poller_node *res = node->res;
    struct sockaddr_storage ss;
    socklen_t len;
    int sockfd;
    void *p;

    while (1) 
    {
        len = sizeof (struct sockaddr_storage);
        sockfd = accept(node->data.fd, (struct sockaddr *)&ss, &len);
        if (sockfd < 0) 
        {
            if (errno == EAGAIN)
                return;
            else
                break;
        }

        p = node->data.accept((const struct sockaddr *)&ss, len, sockfd, node->data.context);
        if (!p)
            break;
        
        res->data = node->data;
        res->data.result = p;
        res->error = 0;
        res->state = PR_ST_SUCCESS;
        res->res = NULL;
        __poller_add_result(res, poller);

        res = (struct __poller_node *)malloc(sizeof (struct __poller_node));
        node->res = res;
        if (!res)
            break;
    }

    if (__poller_remove_node(node, poller))
        return;
    
    node->error = errno;
    node->state = PR_ST_ERROR;
    __poller_add_result(node, poller);
}

static void __poller_handle_connect(struct __poller_node *node,
                                    poller_t *poller)
{
    socklen_t len = sizeof (int);
    int error;

    if (getsockopt(node->data.fd, SOL_SOCKET, SO_ERROR, &error, &len) < 0)
        error = errno;
    
    if (__poller_remove_node(node, poller))
        return;
    
    if (error == 0)
    {
        node->error = 0;
        node->state = PR_ST_FINISHED;
    } else 
    {
        node->error = error;
        node->state = PR_ST_ERROR;
    }

    __poller_add_result(node, poller);
}

static void __poller_handle_read(struct __poller_node *node,
                                 poller_t *poller)
{
    ssize_t nleft;
    size_t n;
    char *p;

    while (1) 
    {
        p = poller->buf;
        if (node->data.ssl)
        {
            nleft = SSL_read(node->data.ssl, p, POLLER_BUFSIZE);
            if (nleft < 0)
            {
                return;
            }
        } else 
        {
            nleft = read(node->data.fd, p, POLLER_BUFSIZE);
            if (nleft < 0)
            {
                if (errno == EAGAIN)
                    return;
            }
        }
        if (nleft <= 0)
            break;
        
        do
        {
            n = nleft;
            if (__poller_append_message(p, &n, node, poller) >= 0)
            {
                nleft -= n;
                p += n;
            } else 
                nleft = -1;
        } while (nleft > 0);
        
        if (nleft < 0)
            break;
    }

    if (__poller_remove_node(node, poller))
        return;
    
    if (nleft == 0)
    {
        node->error = 0;
        node->state = PR_ST_FINISHED;
    } else 
    {
        node->error = errno;
        node->state = PR_ST_ERROR;
    }

    __poller_add_result(node, poller);
}   

#  define IOV_MAX	1024

static void __poller_handle_write(struct __poller_node *node,
                                  poller_t *poller)
{
    struct iovec *iov = node->data.write_iov;
    size_t count = 0;
    ssize_t nleft;
    int iovcnt;
    int ret;

    while (node->data.iovcnt > 0 && iov->iov_len == 0)
    {
        iov++;
        node->data.iovcnt--;
    }

    while (node->data.iovcnt > 0)
    {
        if (node->data.ssl)
        {
            nleft = SSL_write(node->data.ssl, iov->iov_base, iov->iov_len);
            if (nleft <= 0) 
            {
                break;
            }
        } else 
        {
            iovcnt = node->data.iovcnt;
            if (iovcnt > IOV_MAX)
                iovcnt = IOV_MAX;
            
            nleft = writev(node->data.fd, iov, iovcnt);
            if (nleft < 0)
            {
                ret = errno == EAGAIN ? 0 : -1;
                break;
            }
        }

        count += nleft;

    }
}         

static int __poller_handle_pipe(poller_t *poller)
{
    struct __poller_node **node = (struct __poller_node **)poller->buf;
    int stop = 0;
    int n;
    int i;

    n = read(poller->pipe_rd, node, POLLER_BUFSIZE) / sizeof(void *);
    for (i = 0; i < n; i++)
    {
        if (node[i])
            __poller_add_result(node[i], poller);
        else
            stop = 1;
    }
    return stop;
}

static void __poller_handle_timeout(const struct __poller_node *time_node,
                                    poller_t *poller)
{

}

static void *__poller_thread_routine(void *arg)
{
	poller_t *poller = (poller_t *)arg;
	__poller_event_t events[POLLER_EVENTS_MAX];
	struct __poller_node time_node;
	struct __poller_node *node;
	int has_pipe_event;
	int nevents;
	int i;

	while (1)
	{
		__poller_set_timer(poller);
		nevents = __poller_wait(events, POLLER_EVENTS_MAX, poller);
		clock_gettime(CLOCK_MONOTONIC, &time_node.timeout);
		has_pipe_event = 0;
		for (i = 0; i < nevents; i++)
		{
			node = (struct __poller_node *)__poller_event_data(&events[i]);
			if (node > (struct __poller_node *)1)
			{
				switch (node->data.operation)
				{
				case PD_OP_READ:
					__poller_handle_read(node, poller);
					break;
				case PD_OP_WRITE:
					__poller_handle_write(node, poller);
					break;
				case PD_OP_LISTEN:
					__poller_handle_listen(node, poller);
					break;
                case PD_OP_CONNECT:
                    __poller_handle_connect(node, poller);
				default:
					break;
				}
			} else if (node == (struct __poller_node *)1)
			{
				has_pipe_event = 1;
			}

			if (has_pipe_event)
			{
				if (__poller_handle_pipe(poller))
					break;
			}

			__poller_handle_timeout(&time_node, poller);
		}
	}
	return NULL;
}

int poller_start(poller_t *poller)
{
	pthread_t tid;
	int ret;

	pthread_mutex_lock(&poller->mutex);
	if (__poller_open_pipe(poller) >= 0) 
	{
		poller->stopping = 0;
		ret = pthread_create(&tid, NULL, __poller_thread_routine, poller);
		if (ret == 0)
		{
			poller->tid = tid;
			poller->nodes[poller->pipe_rd] = POLLER_NODE_ERROR;
			poller->nodes[poller->pipe_wr] = POLLER_NODE_ERROR;
			poller->stopped = 0;
		} else 
		{
			errno = ret;
			poller->stopping = 1;
			close(poller->pipe_rd);
			close(poller->pipe_wr);
		}
	}

	pthread_mutex_unlock(&poller->mutex);
	return -poller->stopped;
}