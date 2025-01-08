#ifndef _COMMUNICATOR_H_
#define _COMMUNICATOR_H_

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <time.h>
#include <stddef.h>
#include <pthread.h>
#include <openssl/ssl.h>
#include "list.h"
#include "thrdpool.h"
#include "poller.h"
#include "mpoller.h"

class CommConnection
{
public:
    virtual ~CommConnection() {}
};

class CommTarget
{
public:
    int init(const struct sockaddr *addr, socklen_t addrlen, int connect_timeout, int response_timeout);
    void deinit();

public:
    void get_addr(const struct sockaddr **addr, socklen_t *addrlen) const 
    {
        *addr = this->addr;
        *addrlen = this->addrlen;
    }

private:
    virtual int create_connect_fd()
    {
        return socket(this->addr->sa_family, SOCK_STREAM, 0);
    }

    virtual CommConnection *new_connection(int connect_fd)
    {
        return new CommConnection;
    }

public:
    virtual void release() {}

private:
    struct sockaddr *addr;
    socklen_t addrlen;
    int connect_timeout;
    int response_timeout;

private:
    struct list_head idle_list;
    pthread_mutex_t mutex;

public:
    virtual ~CommTarget() {}
    friend class CommSession;
    friend class Communicator;
};

class CommMessageOut
{
private:
    virtual int encode(struct iovec vectors[], int max) = 0;
public:
    virtual ~CommMessageOut() {}
    friend class Communicator;
};

class CommMessageIn : private poller_message_t
{
private:
    virtual int append(const void *buf, size_t *size) = 0;
protected:
    /* Send small packet while receiving. Call only in append(). */
	int feedback(const char *buf, size_t size);

private:
    struct CommConnEntry *entry;
public:
	virtual ~CommMessageIn() { }
	friend class Communicator;
};

#define CS_STATE_SUCCESS	0
#define CS_STATE_ERROR		1
#define CS_STATE_STOPPED	2
#define CS_STATE_TOREPLY	3	/* for service session only. */

class CommSession
{
private:
    virtual CommMessageOut *message_out() = 0;
    virtual CommMessageIn *message_in() = 0;
    virtual int send_timeout() { return -1; }
    virtual int receive_timeout() { return -1; }
    virtual int keep_alive_timeout() { return 0; }
    virtual int first_timeout() { return 0; } /* for client session only. */
    virtual void handle(int state, int error) = 0;

protected:
	CommTarget *get_target() const { return this->target; }
	CommConnection *get_connection() const { return this->conn; }
	CommMessageOut *get_message_out() const { return this->out; }
	CommMessageIn *get_message_in() const { return this->in; }
	long long get_seq() const { return this->seq; }

private:
    CommTarget *target;
    CommConnection *conn;
    CommMessageIn *in;
    CommMessageOut *out;
    long long seq;

private:
    struct timespec begin_time;
    int timeout;
    int passive;

public:
    CommSession() { this->passive = 0; }
    virtual ~CommSession();
    friend class Communicator;
};

#define SS_STATE_COMPLETE	0
#define SS_STATE_ERROR		1
#define SS_STATE_DISRUPTED	2

class SleepSession
{
private:
	virtual int duration(struct timespec *value) = 0;
	virtual void handle(int state, int error) = 0;

public:
	virtual ~SleepSession() { }
	friend class Communicator;
};

# include "IOService_linux.h"

class Communicator
{
public:
	int init(size_t poller_threads, size_t handler_threads);
	void deinit();

	int request(CommSession *session, CommTarget *target);

	int sleep(SleepSession *session);

private:
	poller_queue_t *queue;
	mpoller_t *mpoller;
	thrdpool_t *thrdpool;
	int stop_flag;

private:
	int create_poller(size_t poller_threads);

	int create_handler_threads(size_t handler_threads);

	int nonblock_connect(CommTarget *target);

	struct CommConnEntry *launch_conn(CommSession *session, CommTarget *target);

	void release_conn(struct CommConnEntry *entry);

	int send_message_sync(struct iovec vectors[], int cnt, struct CommConnEntry *entry);

	int send_message(struct CommConnEntry *entry);

	struct CommConnEntry *get_idle_conn(CommTarget *target);

	int request_idle_conn(CommSession *session, CommTarget *target);

	void handle_incoming_reply(struct poller_result *res);

	void handle_read_result(struct poller_result *res);

	void handle_sleep_result(struct poller_result *res);

	void handle_connect_result(struct poller_result *res);

	static void handler_thread_routine(void *context);

	static int first_timeout(CommSession *session);
	static int next_timeout(CommSession *session);

	static int first_timeout_send(CommSession *session);
	static int first_timeout_recv(CommSession *session);

	static int append(const void *buf, size_t *size, poller_message_t *msg);

	static poller_message_t *create_message(void *context);

	static int partial_written(size_t n, void *context);

public:
	virtual ~Communicator() { }
};

#endif