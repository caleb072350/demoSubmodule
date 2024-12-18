#ifndef _IOSERVICE_LINUX_H_
#define _IOSERVICE_LINUX_H_

#include <sys/uio.h>
#include <sys/eventfd.h>
#include <stddef.h>
#include "list.h"

#define IOS_STATE_SUCCESS	0
#define IOS_STATE_ERROR		1

class IOSession
{
private:
    virtual int prepare() = 0;
    virtual void handle(int state, int error) = 0;
protected:
    /* prepare() has to call one of the prep_ functions. */
    void prep_pread(int fd, void *buf, size_t count, long long offset);
    void prep_pwrite(int fd, void *buf, size_t count, long long offset);
    void prep_preadv(int fd, const struct iovec *iov, int iovcnt, long long offset);
    void prep_pwritev(int fd, const struct iovec *iov, int iovcnt, long long offset);
    void prep_fsync(int fd);
    void prep_fdsync(int fd);

protected:
    long get_res() { return this->res; }

private:
    char iocb_buf[64];
    long res;

private:
    struct list_head list;

public:
    virtual ~IOSession() {}
    friend class IOService;
};

class IOService
{
public:
    int init(int maxevents);
    void deinit();

    int request(IOSession *session);
private:
    virtual void handle_stop(int error) {}
    virtual void handle_unbound() = 0;

private:
    virtual int create_event_fd()
    {
        return eventfd(0, 0);
    }

private:
    struct io_context *io_ctx;

private:
	void incref();
	void decref();

private:
	int event_fd;
	int ref;

private:
	struct list_head session_list;
	pthread_mutex_t mutex;

private:
	static void *aio_finish(void *context);

public:
	virtual ~IOService() { }
	friend class Communicator;
};

#endif