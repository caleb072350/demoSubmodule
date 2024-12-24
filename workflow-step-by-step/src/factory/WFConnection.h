#ifndef _WFCONNECTION_H_
#define _WFCONNECTION_H_

#include <utility>
#include <atomic>
#include <functional>
#include "Communicator.h"

class WFConnection : public CommConnection
{
public:
	void *get_context() const
	{
		return this->context;
	}

    void set_context(void *context, std::function<void (void *)> deleter)
	{
		this->context = context;
		this->deleter = std::move(deleter);
	}

    void *test_set_context(void *test_context, void *new_context,
						   std::function<void (void *)> deleter)
	{
		if (this->context.compare_exchange_strong(test_context, new_context))
		{
			this->deleter = std::move(deleter);
			return new_context;
		}

		return test_context;
	}
private:
	std::atomic<void *> context;
	std::function<void (void *)> deleter;

public:
	WFConnection() : context(NULL) { }
protected:
	virtual ~WFConnection()
	{
		if (this->deleter)
			this->deleter(this->context);
	} 
};

#endif