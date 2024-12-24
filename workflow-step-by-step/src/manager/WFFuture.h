#ifndef _WFFUTURE_H_
#define _WFFUTURE_H_

#include <future>
#include <chrono>
#include <utility>
#include "WFGlobal.h"

template<typename RES>
class WFFuture
{
public:
	WFFuture(std::future<RES>&& fr) :
		future(std::move(fr))
	{
	}

    WFFuture() = default;
	WFFuture(const WFFuture&) = delete;
	WFFuture(WFFuture&& move) = default;

    WFFuture& operator=(const WFFuture&) = delete;
	WFFuture& operator=(WFFuture&& move) = default;

    void wait() const;

    template<class REP, class PERIOD>
	std::future_status wait_for(const std::chrono::duration<REP, PERIOD>& time_duration) const;

	template<class CLOCK, class DURATION>
	std::future_status wait_until(const std::chrono::time_point<CLOCK, DURATION>& timeout_time) const;

	RES get()
	{
		this->wait();
		return this->future.get();
	}

	bool valid() const { return this->future.valid(); }

private:
	std::future<RES> future;
};

template<typename RES>
class WFPromise
{
public:
	WFPromise() = default;
	WFPromise(const WFPromise& promise) = delete;
	WFPromise(WFPromise&& move) = default;
	WFPromise& operator=(const WFPromise& promise) = delete;
	WFPromise& operator=(WFPromise&& move) = default;

	WFFuture<RES> get_future()
	{
		return WFFuture<RES>(std::move(this->promise.get_future()));
	}

	void set_value(const RES& value) { this->promise.set_value(value); }
	void set_value(RES&& value) { this->promise.set_value(std::move(value)); }

private:
	std::promise<RES> promise;
};



#endif