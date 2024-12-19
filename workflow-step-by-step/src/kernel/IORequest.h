#ifndef _IOREQUEST_H_
#define _IOREQUEST_H_

#include "SubTask.h"
#include "IOService_linux.h"
#include <errno.h>

class IORequest : public SubTask, public IOSession
{
public:
    IORequest(IOService *service)
    {
        this->service = service;
    }

    virtual void dispatch()
    {
        if (this->service->request(this) < 0) 
        {
            this->state = IOS_STATE_ERROR;
            this->error = errno;
            this->subtask_done();
        }
    }

protected:
    int state;
    int error;

protected:
    IOService *service;

protected:
    virtual void handle(int state, int error)
    {
        this->state = state;
        this->error = error;
        this->subtask_done();
    }
};

#endif