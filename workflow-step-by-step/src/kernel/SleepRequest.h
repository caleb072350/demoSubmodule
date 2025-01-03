#ifndef _SLEEPREQUEST_H_
#define _SLEEPREQUEST_H_

#include <errno.h>
#include "SubTask.h"
#include "CommScheduler.h"

class SleepRequest : public SubTask, public SleepSession
{
public:
	SleepRequest(CommScheduler *scheduler)
	{
		this->scheduler = scheduler;
	}

public:
	virtual void dispatch()
	{
		if (this->scheduler->sleep(this) < 0)
		{
			this->state = SS_STATE_ERROR;
			this->error = errno;
			this->subtask_done();
		}
	}
protected:
	int state;
	int error;

protected:
	CommScheduler *scheduler;

protected:
	virtual void handle(int state, int error)
	{
		this->state = state;
		this->error = error;
		this->subtask_done();
	}
};

#endif