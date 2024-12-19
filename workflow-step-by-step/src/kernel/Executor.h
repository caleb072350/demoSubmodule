#ifndef _EXECUTOR_H_
#define _EXECUTOR_H_

#include <stddef.h>
#include <pthread.h>
#include "list.h"
#include "thrdpool.h"

#define ES_STATE_FINISHED	0
#define ES_STATE_ERROR		1
#define ES_STATE_CANCELED	2

class ExecQueue
{
public:
	int init();
	void deinit();

private:
	struct list_head task_list;
	pthread_mutex_t mutex;

public:
	virtual ~ExecQueue() { }
	friend class Executor;
};

class ExecSession
{
private:
	virtual void execute() = 0;
	virtual void handle(int state, int error) = 0;

protected:
	ExecQueue *get_queue() { return this->queue; }

private:
	ExecQueue *queue;

public:
	virtual ~ExecSession() { }
	friend class Executor;
};

class Executor
{
public:
	int init(size_t nthreads);
	void deinit();

	int request(ExecSession *session, ExecQueue *queue);

private:
	thrdpool_t *thrdpool;

private:
	static void executor_thread_routine(void *context);
	static void executor_cancel_tasks(const struct thrdpool_task *task);

public:
	virtual ~Executor() { }
};

#endif