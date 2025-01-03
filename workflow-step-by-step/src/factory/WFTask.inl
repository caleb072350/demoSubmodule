template<class REQ, class RESP>
int WFNetworkTask<REQ, RESP>::get_peer_addr(struct sockaddr *addr,
											socklen_t *addrlen) const
{
	const struct sockaddr *p;
	socklen_t len;

	if (this->target)
	{
		this->target->get_addr(&p, &len);
		if (*addrlen >= len)
		{
			memcpy(addr, p, len);
			*addrlen = len;
			return 0;
		}

		errno = ENOBUFS;
	}
	else
		errno = ENOTCONN;

	return -1;
};

template<class REQ, class RESP>
WFConnection *WFNetworkTask<REQ, RESP>::get_connection() const
{
	CommConnection *conn;

	if (this->target)
	{
		conn = this->CommSession::get_connection();
		if (conn)
			return static_cast<WFConnection *>(conn);
	}

	errno = ENOTCONN;
	return NULL;
}

template<class REQ, class RESP>
class WFClientTask : public WFNetworkTask<REQ, RESP>
{
protected:
	virtual CommMessageOut *message_out()
	{
		/* By using prepare function, users can modify request after
		 * the connection is established. */
		if (this->prepare)
			this->prepare(this);

		return &this->req;
	}

	virtual CommMessageIn *message_in() { return &this->resp; }

public:
	void set_prepare(std::function<void (WFNetworkTask<REQ, RESP> *)> prep)
	{
		this->prepare = std::move(prep);
	}

protected:
	std::function<void (WFNetworkTask<REQ, RESP> *)> prepare;

public:
	WFClientTask(CommSchedObject *object, CommScheduler *scheduler,
				 std::function<void (WFNetworkTask<REQ, RESP> *)>&& cb) :
		WFNetworkTask<REQ, RESP>(object, scheduler, std::move(cb))
	{
		LOG_INFO("WFClientTask(null, WFGlobal::get_scheduler, wget_callback)");
	}

protected:
	virtual ~WFClientTask() { }
};

template<class REQ, class RESP>
class WFServerTask : public WFNetworkTask<REQ, RESP>
{
protected:
	virtual CommMessageOut *message_out() { return &this->resp; }
	virtual CommMessageIn *message_in() { return &this->req; }
	virtual void handle(int state, int error);

protected:
	virtual void dispatch()
	{
		if (this->state == WFT_STATE_TOREPLY)
		{
			/* After reply success, get_connection() is enabled again. */
			this->processor.task = this;
			if (this->scheduler->reply(this) >= 0)
				return;

			this->state = WFT_STATE_SYS_ERROR;
			this->error = errno;
			this->processor.task = NULL;
		}

		this->subtask_done();
	}

	/* CommSession's get_connection() is supposed to be called only in its
	 * virtual functions. As a server task, call of this method after process()
	 * and before callback() is very dangerous and should be blocked. */
	virtual WFConnection *get_connection() const
	{
		if (this->processor.task)
			return this->WFNetworkTask<REQ, RESP>::get_connection();

		errno = EPERM;
		return NULL;
	}

protected:
	class Processor : public SubTask
	{
	public:
		Processor(WFServerTask<REQ, RESP> *task,
				 std::function<void (WFNetworkTask<REQ, RESP> *)>& proc) :
			process(proc)
		{
			this->task = task;
		}

		virtual void dispatch()
		{
			this->process(this->task);
			this->task = NULL;	/* As a flag. get_conneciton() disabled. */
			this->subtask_done();
		}

		virtual SubTask *done()
		{
			return series_of(this)->pop();
		}

		std::function<void (WFNetworkTask<REQ, RESP> *)>& process;
		WFServerTask<REQ, RESP> *task;
	} processor;

public:
	WFServerTask(CommScheduler *scheduler,
				 std::function<void (WFNetworkTask<REQ, RESP> *)>& proc) :
		WFNetworkTask<REQ, RESP>(NULL, scheduler, nullptr),
		processor(this, proc)
	{
	}

protected:
	virtual ~WFServerTask() { }
};

template<class REQ, class RESP>
void WFServerTask<REQ, RESP>::handle(int state, int error)
{
	if (state == WFT_STATE_TOREPLY)
	{
		this->state = WFT_STATE_TOREPLY;
		this->target = this->get_target();
		Workflow::start_series_work(&this->processor, this, nullptr);
	}
	else if (this->state == WFT_STATE_TOREPLY)
	{
		this->state = state;
		this->error = error;
		if (error == ETIMEDOUT)
			this->timeout_reason = TOR_TRANSMIT_TIMEOUT;

		this->subtask_done();
	}
	else
		delete this;
}