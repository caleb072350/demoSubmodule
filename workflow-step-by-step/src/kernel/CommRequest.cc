#include <errno.h>
#include "CommScheduler.h"
#include "CommRequest.h"

void CommRequest::handle(int state, int error)
{
	this->state = state;
	this->error = error;
	if (error != ETIMEDOUT)
		this->timeout_reason = TOR_NOT_TIMEOUT;
	else if (!this->get_message_out())
		this->timeout_reason = TOR_CONNECT_TIMEOUT;
	else
		this->timeout_reason = TOR_TRANSMIT_TIMEOUT;

	this->subtask_done();
}

void CommRequest::dispatch()
{
	if (this->scheduler->request(this, this->object, this->wait_timeout, &this->target) < 0)
	{
		this->state = CS_STATE_ERROR;
		this->error = errno;
		if (errno != ETIMEDOUT)
			this->timeout_reason = TOR_NOT_TIMEOUT;
		else
			this->timeout_reason = TOR_WAIT_TIMEOUT;

		this->subtask_done();
	}
}