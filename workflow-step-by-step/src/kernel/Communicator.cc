#include <sys/types.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <errno.h>
#include <limits.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "list.h"
#include "thrdpool.h"
#include "poller.h"
#include "mpoller.h"
#include "Communicator.h"
#include "logger.h"

struct CommConnEntry
{
    struct list_head list;
    CommConnection *conn;
    long long seq;
    int sockfd;
#define CONN_STATE_CONNECTING	0
#define CONN_STATE_CONNECTED	1
#define CONN_STATE_RECEIVING	2
#define CONN_STATE_SUCCESS		3
#define CONN_STATE_IDLE			4
#define CONN_STATE_KEEPALIVE	5
#define CONN_STATE_CLOSING		6
#define CONN_STATE_ERROR		7
    int state;
    int error;
    int ref;
    struct iovec *write_iov;
    CommSession *session;
    CommTarget *target;
    mpoller_t *mpoller;
    /* Connection entry's mutex is for client session only. */
    pthread_mutex_t mutex;
};

static inline int __set_fd_nonblock(int fd)
{
	int flags = fcntl(fd, F_GETFL);

	if (flags >= 0)
		flags = fcntl(fd, F_SETFL, flags | O_NONBLOCK);

	return flags;
}

int CommTarget::init(const struct sockaddr *addr, socklen_t addrlen, int connect_timeout, int response_timeout)
{
	int ret;

	this->addr = (struct sockaddr *)malloc(addrlen);
	if (this->addr)
	{
		ret = pthread_mutex_init(&this->mutex, NULL);
		if (ret == 0)
		{
			memcpy(this->addr, addr, addrlen);
			this->addrlen = addrlen;
			this->connect_timeout = connect_timeout;
			this->response_timeout = response_timeout;
			INIT_LIST_HEAD(&this->idle_list);
			return 0;
		}

		errno = ret;
		free(this->addr);
	}

	return -1;
}

void CommTarget::deinit()
{
	pthread_mutex_destroy(&this->mutex);
	free(this->addr);
}

int CommMessageIn::feedback(const char *buf, size_t size)
{
	struct CommConnEntry *entry = this->entry;
	return write(entry->sockfd, buf, size);
}

CommSession::~CommSession()
{
	struct CommConnEntry *entry;
	struct list_head *pos;
	CommTarget *target;
	int errno_bak;

	if (!this->passive)
		return;

	target = this->target;
	if (this->passive == 1)
	{
		pthread_mutex_lock(&target->mutex);
		if (!list_empty(&target->idle_list))
		{
			pos = target->idle_list.next;
			entry = list_entry(pos, struct CommConnEntry, list);
			errno_bak = errno;
			mpoller_del(entry->sockfd, entry->mpoller);
			errno = errno_bak;
		}

		pthread_mutex_unlock(&target->mutex);
	}

	// ((CommServiceTarget *)target)->decref();
}

inline int Communicator::first_timeout(CommSession *session)
{
	int timeout = session->target->response_timeout;

	if (timeout < 0 || (unsigned int)session->timeout <= (unsigned int)timeout)
	{
		timeout = session->timeout;
		session->timeout = 0;
		session->begin_time.tv_nsec = 0;
	}
	else
		clock_gettime(CLOCK_MONOTONIC, &session->begin_time);

	return timeout;
}

int Communicator::next_timeout(CommSession *session)
{
	int timeout = session->target->response_timeout;
	struct timespec cur_time;
	int time_used, time_left;

	if (session->timeout > 0)
	{
		clock_gettime(CLOCK_MONOTONIC, &cur_time);
		time_used = 1000 * (cur_time.tv_sec - session->begin_time.tv_sec) +
					(cur_time.tv_nsec - session->begin_time.tv_nsec) / 1000000;
		time_left = session->timeout - time_used;
		if (time_left <= timeout) /* here timeout >= 0 */
		{
			timeout = time_left < 0 ? 0 : time_left;
			session->timeout = 0;
		}
	}

	return timeout;
}

int Communicator::first_timeout_send(CommSession *session)
{
	session->timeout = session->send_timeout();
	return Communicator::first_timeout(session);
}

int Communicator::first_timeout_recv(CommSession *session)
{
	session->timeout = session->receive_timeout();
	return Communicator::first_timeout(session);
}

void Communicator::release_conn(struct CommConnEntry *entry)
{
	delete entry->conn;
	close(entry->sockfd);
	free(entry);
}

int Communicator::send_message_sync(struct iovec vectors[], int cnt, struct CommConnEntry *entry)
{
	CommSession *session = entry->session;
	int timeout;
	ssize_t n;
	int i;

	while (1)
	{
		n = writev(entry->sockfd, vectors, cnt <= IOV_MAX ? cnt : IOV_MAX);
		if (n < 0)
			return errno == EAGAIN ? cnt : -1;

		for (i = 0; i < cnt; i++)
		{
			if ((size_t)n >= vectors[i].iov_len)
				n -= vectors[i].iov_len;
			else
			{
				vectors[i].iov_base = (char *)vectors[i].iov_base + n;
				vectors[i].iov_len -= n;
				break;
			}
		}

		cnt -= i;
		if (cnt == 0)
			break;

		if (i < IOV_MAX)
			return cnt;

		vectors += i;
	}
	
	if (entry->state == CONN_STATE_IDLE)
	{
		timeout = session->first_timeout();
		if (timeout == 0)
			timeout = Communicator::first_timeout_recv(session);
		else
		{
			session->timeout = -1;
			session->begin_time.tv_nsec = -1;
		}

		mpoller_set_timeout(entry->sockfd, timeout, this->mpoller);
	}

	entry->state = CONN_STATE_RECEIVING;
	return 0;
}

#define ENCODE_IOV_MAX		8192

int Communicator::send_message(struct CommConnEntry *entry)
{
	struct iovec vectors[ENCODE_IOV_MAX];
	struct iovec *end;
	int cnt;

	cnt = entry->session->out->encode(vectors, ENCODE_IOV_MAX);
	if ((unsigned int)cnt > ENCODE_IOV_MAX)
	{
		if (cnt > ENCODE_IOV_MAX)
			errno = EOVERFLOW;
		return -1;
	}

	end = vectors + cnt;
	return this->send_message_sync(vectors, cnt, entry);	
}

void Communicator::handle_incoming_reply(struct poller_result *res)
{
	struct CommConnEntry *entry = (struct CommConnEntry *)res->data.context;
	CommTarget *target = entry->target;
	CommSession *session = NULL;
	pthread_mutex_t *mutex;
	int state;

	switch (res->state)
	{
	case PR_ST_SUCCESS:
		session = entry->session;
		state = CS_STATE_SUCCESS;
		pthread_mutex_lock(&target->mutex);
		if (entry->state == CONN_STATE_SUCCESS)
		{
			__sync_add_and_fetch(&entry->ref, 1);
			if (session->timeout != 0) /* This is keep-alive timeout. */
			{
				entry->state = CONN_STATE_IDLE;
				list_add(&entry->list, &target->idle_list);
			}
			else
				entry->state = CONN_STATE_CLOSING;
		}

		pthread_mutex_unlock(&target->mutex);
		break;

	case PR_ST_FINISHED:
		res->error = ECONNRESET;
		if (1)
	case PR_ST_ERROR:
			state = CS_STATE_ERROR;
		else
	case PR_ST_DELETED:
	case PR_ST_STOPPED:
		state = CS_STATE_STOPPED;

		mutex = &entry->mutex;
		pthread_mutex_lock(&target->mutex);
		pthread_mutex_lock(mutex);
		switch (entry->state)
		{
		case CONN_STATE_IDLE:
			list_del(&entry->list);
			break;

		case CONN_STATE_ERROR:
			res->error = entry->error;
			state = CS_STATE_ERROR;
		case CONN_STATE_RECEIVING:
			session = entry->session;
			break;

		case CONN_STATE_SUCCESS:
			/* This may happen only if handler_threads > 1. */
			entry->state = CONN_STATE_CLOSING;
			entry = NULL;
			break;
		}

		pthread_mutex_unlock(&target->mutex);
		pthread_mutex_unlock(mutex);
		break;
	}

	if (entry)
	{
		if (session)
		{
			target->release();
			session->handle(state, res->error);
		}

		if (__sync_sub_and_fetch(&entry->ref, 1) == 0)
			this->release_conn(entry);
	}
}

void Communicator::handle_read_result(struct poller_result *res)
{
	struct CommConnEntry *entry = (struct CommConnEntry *)res->data.context;

	if (res->state != PR_ST_MODIFIED)
	{
		this->handle_incoming_reply(res);
	}
}

void Communicator::handle_connect_result(struct poller_result *res)
{
	struct CommConnEntry *entry = (struct CommConnEntry *)res->data.context;
	CommSession *session = entry->session;
	CommTarget *target = entry->target;
	int timeout;
	int state;
	int ret;

	switch (res->state)
	{
	case PR_ST_FINISHED:
		if ((session->out = session->message_out()) != NULL)
		{
			ret = this->send_message(entry);
			if (ret == 0)
			{
				res->data.operation = PD_OP_READ;
				res->data.message = NULL;
				timeout = session->first_timeout();
				if (timeout == 0)
					timeout = Communicator::first_timeout_recv(session);
				else
				{
					session->timeout = -1;
					session->begin_time.tv_nsec = -1;
				}
			}
			else if (ret > 0)
				break;
		}
		else
			ret = -1;

		if (ret >= 0)
		{
			if (mpoller_add(&res->data, timeout, this->mpoller) >= 0)
			{
				if (this->stop_flag)
					mpoller_del(res->data.fd, this->mpoller);
				break;
			}
		}

		res->error = errno;
		if (1)
	case PR_ST_ERROR:
			state = CS_STATE_ERROR;
		else
	case PR_ST_DELETED:
	case PR_ST_STOPPED:
			state = CS_STATE_STOPPED;

		target->release();
		session->handle(state, res->error);
		this->release_conn(entry);
		break;
	}
}

void Communicator::handler_thread_routine(void *context)
{
	Communicator *comm = (Communicator *)context;
	struct poller_result *res;

	while ((res = poller_queue_get(comm->queue)) != NULL)
	{
		switch (res->data.operation)
		{
		case PD_OP_READ:
			comm->handle_read_result(res);
			break;
		case PD_OP_CONNECT:
			comm->handle_connect_result(res);
			break;
		case PD_OP_TIMER:
			comm->handle_sleep_result(res);
			break;
		}
		free(res);
	}
}

int Communicator::append(const void *buf, size_t *size, poller_message_t *msg)
{
	CommMessageIn *in = (CommMessageIn *)msg;
	struct CommConnEntry *entry = in->entry;
	CommSession *session = entry->session;
	int timeout;
	int ret;

	ret = in->append(buf, size);
	if (ret > 0)
	{
		entry->state = CONN_STATE_SUCCESS;
		timeout = session->keep_alive_timeout();
		session->timeout = timeout; /* Reuse session's timeout field. */
		if (timeout == 0)
		{
			mpoller_del(entry->sockfd, entry->mpoller);
			return ret;
		}
	}
	else if (ret == 0 && session->timeout != 0)
	{
		if (session->begin_time.tv_nsec == -1)
			timeout = Communicator::first_timeout_recv(session);
		else
			timeout = Communicator::next_timeout(session);
	}
	else
		return ret;

	/* This set_timeout() never fails, which is very important. */
	mpoller_set_timeout(entry->sockfd, timeout, entry->mpoller);
	return ret;
}

poller_message_t *Communicator::create_message(void *context)
{
	struct CommConnEntry *entry = (struct CommConnEntry *)context;
	CommSession *session;

	if (entry->state == CONN_STATE_IDLE)
	{
		pthread_mutex_t *mutex;
		mutex = &entry->mutex;

		pthread_mutex_lock(mutex);
		/* do nothing */
		pthread_mutex_unlock(mutex);
	}

	if (entry->state != CONN_STATE_RECEIVING)
	{
		errno = EBADMSG;
		return NULL;
	}

	session = entry->session;
	session->in = session->message_in();
	if (session->in)
	{
		session->in->poller_message_t::append = Communicator::append;
		session->in->entry = entry;
	}

	return session->in;
}

int Communicator::partial_written(size_t n, void *context)
{
	struct CommConnEntry *entry = (struct CommConnEntry *)context;
	CommSession *session = entry->session;
	int timeout;

	timeout = Communicator::next_timeout(session);
	mpoller_set_timeout(entry->sockfd, timeout, entry->mpoller);
	return 0;
}

int Communicator::create_handler_threads(size_t handler_threads)
{
	struct thrdpool_task task = {
		.routine	=	Communicator::handler_thread_routine,
		.context	=	this
	};
	size_t i;

	this->thrdpool = thrdpool_create(handler_threads, 0);
	if (this->thrdpool)
	{
		for (i = 0; i < handler_threads; i++)
		{
			if (thrdpool_schedule(&task, this->thrdpool) < 0)
				break;
		}

		if (i == handler_threads)
			return 0;

		poller_queue_set_nonblock(this->queue);
		thrdpool_destroy(NULL, this->thrdpool);
	}

	return -1;
}

int Communicator::create_poller(size_t poller_threads)
{
	struct poller_params params = {
		.max_open_files		=	65536,
		.result_queue		=	poller_queue_create(4096),
		.create_message		=	Communicator::create_message,
		.partial_written	=	Communicator::partial_written,
	};

	this->queue = params.result_queue;
	if (this->queue)
	{
		this->mpoller = mpoller_create(&params, poller_threads);
		if (this->mpoller)
		{
			if (mpoller_start(this->mpoller) >= 0)
				return 0;

			mpoller_destroy(this->mpoller);
		}

		poller_queue_destroy(this->queue);
	}

	return -1;
}

int Communicator::init(size_t poller_threads, size_t handler_threads)
{
	if (poller_threads == 0 || handler_threads == 0)
	{
		errno = EINVAL;
		return -1;
	}

	if (this->create_poller(poller_threads) >= 0)
	{
		if (this->create_handler_threads(handler_threads) >= 0)
		{
			this->stop_flag = 0;
			return 0;
		}

		mpoller_stop(this->mpoller);
		mpoller_destroy(this->mpoller);
		poller_queue_destroy(this->queue);
	}

	return -1;
}

void Communicator::deinit()
{
	this->stop_flag = 1;
	mpoller_stop(this->mpoller);
	poller_queue_set_nonblock(this->queue);
	thrdpool_destroy(NULL, this->thrdpool);
	mpoller_destroy(this->mpoller);
	poller_queue_destroy(this->queue);
}

int Communicator::nonblock_connect(CommTarget *target)
{
	int sockfd = target->create_connect_fd();

	if (sockfd >= 0)
	{
		if (__set_fd_nonblock(sockfd) >= 0)
		{
			if (connect(sockfd, target->addr, target->addrlen) >= 0 ||
				errno == EINPROGRESS)
			{
				return sockfd;
			}
		}

		close(sockfd);
	}

	return -1;
}

struct CommConnEntry *Communicator::launch_conn(CommSession *session,
												CommTarget *target)
{
	struct CommConnEntry *entry;
	int sockfd;
	int ret;

	sockfd = this->nonblock_connect(target);
	if (sockfd >= 0)
	{
		entry = (struct CommConnEntry *)malloc(sizeof (struct CommConnEntry));
		if (entry)
		{
			ret = pthread_mutex_init(&entry->mutex, NULL);
			if (ret == 0)
			{
				entry->conn = target->new_connection(sockfd);
				if (entry->conn)
				{
					entry->seq = 0;
					entry->mpoller = this->mpoller;
					// entry->service = NULL;
					entry->target = target;
					entry->session = session;
					entry->sockfd = sockfd;
					entry->state = CONN_STATE_CONNECTING;
					entry->ref = 1;
					return entry;
				}

				pthread_mutex_destroy(&entry->mutex);
			}
			else
				errno = ret;

			free(entry);
		}

		close(sockfd);
	}

	return NULL;
}

struct CommConnEntry *Communicator::get_idle_conn(CommTarget *target)
{
	struct CommConnEntry *entry;
	struct list_head *pos;

	list_for_each(pos, &target->idle_list)
	{
		entry = list_entry(pos, struct CommConnEntry, list);
		if (mpoller_set_timeout(entry->sockfd, -1, this->mpoller) >= 0)
		{
			list_del(pos);
			return entry;
		}
	}

	errno = ENOENT;
	return NULL;
}

int Communicator::request_idle_conn(CommSession *session, CommTarget *target)
{
	struct CommConnEntry *entry;
	int ret = -1;

	pthread_mutex_lock(&target->mutex);
	entry = this->get_idle_conn(target);
	if (entry)
		pthread_mutex_lock(&entry->mutex);
	pthread_mutex_unlock(&target->mutex);
	if (entry)
	{
		entry->session = session;
		session->conn = entry->conn;
		session->seq = entry->seq++;
		session->out = session->message_out();
		if (session->out)
			ret = this->send_message(entry);

		if (ret < 0)
		{
			entry->error = errno;
			mpoller_del(entry->sockfd, this->mpoller);
			entry->state = CONN_STATE_ERROR;
			ret = 1;
		}

		pthread_mutex_unlock(&entry->mutex);
	}

	return ret;
}

int Communicator::request(CommSession *session, CommTarget *target)
{
	struct CommConnEntry *entry;
	struct poller_data data;
	int errno_bak;
	int ret;

	if (session->passive)
	{
		errno = EINVAL;
		return -1;
	}

	errno_bak = errno;
	session->target = target;
	session->out = NULL;
	session->in = NULL;
	ret = this->request_idle_conn(session, target);
	while (ret < 0)
	{
		entry = this->launch_conn(session, target);
		if (entry)
		{
			session->conn = entry->conn;
			session->seq = entry->seq++;
			data.operation = PD_OP_CONNECT;
			data.fd = entry->sockfd;
			data.context = entry;
			if (mpoller_add(&data, target->connect_timeout, this->mpoller) >= 0)
				break;

			this->release_conn(entry);
		}

		session->conn = NULL;
		session->seq = 0;
		return -1;
	}

	errno = errno_bak;
	return 0;
}

int Communicator::sleep(SleepSession *session)
{
	struct timespec value;

	if (session->duration(&value) >= 0)
	{
		if (mpoller_add_timer(session, &value, this->mpoller) >= 0)
			return 0;
	}

	return -1;
}

extern "C" void __thrdpool_schedule(const struct thrdpool_task *, void *, thrdpool_t *);	

void Communicator::handle_sleep_result(struct poller_result *res)
{
	SleepSession *session = (SleepSession *)res->data.context;
	int state;

	if (res->state == PR_ST_STOPPED)
		state = SS_STATE_DISRUPTED;
	else
		state = SS_STATE_COMPLETE;
	session->handle(state, 0);
}