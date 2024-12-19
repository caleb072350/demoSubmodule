#ifndef _EXECREQUEST_H_
#define _EXECREQUEST_H_

#include "SubTask.h"
#include "Executor.h"
#include <errno.h>

class ExecRequest : public SubTask, public ExecSession
{
public:
	ExecRequest(ExecQueue *queue, Executor *executor)
	{
		this->executor = executor;
		this->queue = queue;
	}

	ExecQueue *get_request_queue() const { return this->queue; }
	void set_request_queue(ExecQueue *queue) { this->queue = queue; }

public:
	virtual void dispatch()
	{
		if (this->executor->request(this, this->queue) < 0)
		{
			this->state = ES_STATE_ERROR;
			this->error = errno;
			this->subtask_done();
		}
	}

protected:
	int state;
	int error;

protected:
	ExecQueue *queue;
	Executor *executor;

protected:
	virtual void handle(int state, int error)
	{
		this->state = state;
		this->error = error;
		this->subtask_done();
	}
};

#endif