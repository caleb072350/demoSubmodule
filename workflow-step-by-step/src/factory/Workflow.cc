#include <assert.h>
#include <stddef.h>
#include <string.h>
#include <utility>
#include <functional>
#include <mutex>
#include <fmt/core.h>
#include "Workflow.h"

SeriesWork::SeriesWork(SubTask *first, series_callback_t&& cb) :
	queue(new SubTask *[4]),
	callback(std::move(cb))
{
	this->queue_size = 4;
	this->front = 0;
	this->back = 0;
	this->in_parallel = false;
	this->canceled = false;
	first->set_pointer(this);
	this->first = first;
	this->last = NULL;
	this->context = NULL;
}

void SeriesWork::dismiss_recursive()
{
	SubTask *task = first;

	this->in_parallel = false;
	this->callback = nullptr;
	do
	{
		delete task;
		task = this->pop_task();
	} while (task);
}

void SeriesWork::expand_queue()
{
	int size = 2 * this->queue_size;
	SubTask **queue = new SubTask *[size];
	int i, j;

	i = 0;
	j = this->front;
	do
	{
		queue[i++] = this->queue[j++];
		if (j == this->queue_size)
			j = 0;
	} while (j != this->back);

	delete []this->queue;
	this->queue = queue;
	this->queue_size = size;
	this->front = 0;
	this->back = i;
}

void SeriesWork::push_front(SubTask *task)
{
	this->mutex.lock();
	if (--this->front == -1)
		this->front = this->queue_size - 1;

	task->set_pointer(this);
	this->queue[this->front] = task;
	if (this->front == this->back)
		this->expand_queue();

	this->mutex.unlock();
}

void SeriesWork::push_back(SubTask *task)
{
	this->mutex.lock();
	task->set_pointer(this);
	this->queue[this->back] = task;
	if (++this->back == this->queue_size)
		this->back = 0;

	if (this->front == this->back)
		this->expand_queue();

	this->mutex.unlock();
}

SubTask *SeriesWork::pop()
{
	bool canceled = this->canceled;
	SubTask *task = this->pop_task();

	if (!canceled)
		return task;

	while (task)
	{
		delete task;
		task = this->pop_task();
	}

	return NULL;
}

SubTask *SeriesWork::pop_task()
{
	SubTask *task;

	this->mutex.lock();
	if (this->front != this->back)
	{
		task = this->queue[this->front];
		if (++this->front == this->queue_size)
			this->front = 0;
	}
	else
	{
		task = this->last;
		this->last = NULL;
	}

	this->mutex.unlock();
	if (!task)
	{
		if (this->callback)
			this->callback(this);

		if (!this->in_parallel)
			delete this;
	}

	return task;
}