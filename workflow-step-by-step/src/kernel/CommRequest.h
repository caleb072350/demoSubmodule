#ifndef _COMMREQUEST_H_
#define _COMMREQUEST_H_

#include <stddef.h>
#include "SubTask.h"
#include "Communicator.h"
#include "CommScheduler.h"
#include "logger.h"

class CommRequest : public SubTask, public CommSession
{
public:
	CommRequest(CommSchedObject *object, CommScheduler *scheduler)
	{
		this->scheduler = scheduler;
		this->object = object;
		this->wait_timeout = 0;
	}

	CommSchedObject *get_request_object() const { return this->object; }
	void set_request_object(CommSchedObject *object) { this->object = object; }
	int get_wait_timeout() const { return this->wait_timeout; }
	void set_wait_timeout(int timeout) { this->wait_timeout = timeout; }

public:
	virtual void dispatch();

protected:
	int state;
	int error;

protected:
	CommTarget *target;
#define TOR_NOT_TIMEOUT			0
#define TOR_WAIT_TIMEOUT		1
#define TOR_CONNECT_TIMEOUT		2
#define TOR_TRANSMIT_TIMEOUT	3
	int timeout_reason;

protected:
	int wait_timeout;
	CommSchedObject *object;
	CommScheduler *scheduler;

protected:
	virtual void handle(int state, int error);
};

#endif