#ifndef _WORKFLOW_H_
#define _WORKFLOW_H_

#include <assert.h>
#include <stddef.h>
#include <utility>
#include <functional>
#include <mutex>
#include "SubTask.h"
#include "logger.h"

class SeriesWork;
class ParallelWork;

using series_callback_t = std::function<void (const SeriesWork *)>;
using parallel_callback_t = std::function<void (const ParallelWork *)>;

class Workflow
{
public:
    static SeriesWork *
    create_series_work(SubTask *first, series_callback_t callback);

    static void
    start_series_work(SubTask *first, series_callback_t callback);

    static SeriesWork *
    create_series_work(SubTask *first, SubTask *last,
                       series_callback_t callback);
    
    static void
    start_series_work(SubTask *first, SubTask *last,
                      series_callback_t callback);

    static ParallelWork *
    create_parallel_work(parallel_callback_t callback);

    static ParallelWork *
    create_parallel_work(SeriesWork *const all_series[], size_t n,
                         parallel_callback_t callback);
    
    static void 
    start_parallel_work(SeriesWork *const all_series[], size_t n,
                        parallel_callback_t callback);
};

class SeriesWork
{
public:
    void start()
    {
        assert(!this->in_parallel);
        this->first->dispatch();
    }

    /* Call dismiss() only when you don't want to start a created series. */
    /* This operation is recursive, so only call on the "root". */
    void dismiss()
    {
        assert(!this->in_parallel);
        this->dismiss_recursive();
    }

public:
    void push_back(SubTask *task);
    void push_front(SubTask *task);

public:
    void *get_context() const { return this->context; }
    void set_context(void *context) { this->context = context; }

public:
    /* Calcel a running series. Typically, called in the callback of a task
     * that belongs to the series. All subsequent tasks in the series will be
     * destroyed immediately and recursively (ParallelWork), without callback.
     * But the callback of this canceled series will still be called.
     */
    void cancel() { this->canceled = true; }

    /**
     * Parallel work's callback may check the cancelation state of each
     * sub-series, and cancel it's super-series recursively.
     */
    bool is_canceled() const { return this->canceled; }

public:
	void set_callback(series_callback_t callback)
	{
		this->callback = std::move(callback);
	}

public:
	SubTask *pop();

private:
	void set_last_task(SubTask *last)
	{
		last->set_pointer(this);
		this->last = last;
	}

private:
	SubTask *pop_task();
	void expand_queue();
	void dismiss_recursive();

private:
    SubTask *first;
    SubTask *last;
    SubTask **queue;
    int queue_size;
    int front;
    int back;
    bool in_parallel;
    bool canceled;
    void *context;
    std::mutex mutex;
    series_callback_t callback;

private:
	SeriesWork(SubTask *first, series_callback_t&& callback);
	~SeriesWork() { delete []this->queue; }
	friend class ParallelWork;
	friend class Workflow;
};

static inline SeriesWork *series_of(const SubTask *task)
{
	return (SeriesWork *)task->get_pointer();
}

static inline SeriesWork& operator *(const SubTask& task)
{
	return *series_of(&task);
}

static inline SeriesWork& operator << (SeriesWork& series, SubTask *task)
{
	series.push_back(task);
	return series;
}

inline SeriesWork *
Workflow::create_series_work(SubTask *first, series_callback_t callback)
{
	return new SeriesWork(first, std::move(callback));
}

inline void
Workflow::start_series_work(SubTask *first, series_callback_t callback)
{
	new SeriesWork(first, std::move(callback));
	first->dispatch();
}

inline SeriesWork *
Workflow::create_series_work(SubTask *first, SubTask *last,
							 series_callback_t callback)
{
	SeriesWork *series = new SeriesWork(first, std::move(callback));
	series->set_last_task(last);
	return series;
}

inline void
Workflow::start_series_work(SubTask *first, SubTask *last,
							series_callback_t callback)
{
	SeriesWork *series = new SeriesWork(first, std::move(callback));
	series->set_last_task(last);
	first->dispatch();
}

#endif