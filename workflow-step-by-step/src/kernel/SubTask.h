#ifndef _SUBTASK_H_
#define _SUBTASK_H_

#include <stddef.h>

class ParallelTask;

class SubTask
{
public:
    virtual void dispatch() = 0;

private:
    virtual SubTask *done() = 0;

protected:
    void subtask_done();

public:
    ParallelTask *get_parent_task() const { return this->parent; }
	void *get_pointer() const { return this->pointer; }
	void set_pointer(void *pointer) { this->pointer = pointer; }

private:
    ParallelTask *parent;
    SubTask **entry;
    void *pointer;

public:
	SubTask()
	{
		this->parent = NULL;
		this->entry = NULL;
		this->pointer = NULL;
	}

    virtual ~SubTask() { }
	friend class ParallelTask;
};

class ParallelTask : public SubTask
{
public:
	ParallelTask(SubTask **subtasks, size_t n)
	{
		this->subtasks = subtasks;
		this->subtasks_nr = n;
	}

    SubTask **get_subtasks(size_t *n)
	{
		*n = this->subtasks_nr;
		return this->subtasks;
	}

	void set_subtasks(SubTask **subtasks, size_t n)
	{
		this->subtasks = subtasks;
		this->subtasks_nr = n;
	}

public:
	virtual void dispatch();

protected:
	SubTask **subtasks;
	size_t subtasks_nr;

private:
	size_t nleft;
	friend class SubTask;
};

#endif