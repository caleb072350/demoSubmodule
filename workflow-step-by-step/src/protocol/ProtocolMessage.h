#ifndef _PROTOCOLMESSAGE_H_
#define _PROTOCOLMESSAGE_H_

#include <errno.h>
#include <stddef.h>
#include "Communicator.h"

/**
 * @file   ProtocolMessage.h
 * @brief  General Protocol Interface
 */
namespace protocol
{
class ProtocolMessage : public CommMessageOut, public CommMessageIn
{
private:
    virtual int encode(struct iovec vectors[], int max)
    {
        errno = ENOSYS;
        return -1;
    }

    /* You have to implement one of the 'append' functions, and the first one
	 * with arguement 'size_t *size' is recommmended. */

	/* Argument 'size' indicates bytes to append, and returns bytes used. */
	virtual int append(const void *buf, size_t *size)
	{
		return this->append(buf, *size);
	}

	/* When implementing this one, all bytes are consumed. Cannot support
	 * streaming protocol. */
	virtual int append(const void *buf, size_t size)
	{
		errno = ENOSYS;
		return -1;
	}

public:
	void set_size_limit(size_t limit) { this->size_limit = limit; }
	size_t get_size_limit() const { return this->size_limit; }

protected:
	size_t size_limit;   
public:
	ProtocolMessage() { this->size_limit = (size_t)-1; }
};

}

#endif