#include "SubTask.h"

void SubTask::subtask_done()
{
    SubTask *cur = this;
    ParallelTask *parent;
    SubTask **entry;

    while (1) {
        parent = cur->parent;
        entry = cur->entry;
        cur = cur->done();
        if (cur) {
            cur->parent = parent;
            cur->entry = entry;
            if (parent)
                *entry = parent;
            cur->dispatch();
        } else if (parent)
        {
            if (__sync_sub_and_fetch(&parent->nleft, 1) == 0)
            {
                cur = parent;
                continue;
            }
        }
        break;
    }
}

void ParallelTask::dispatch()
{
    SubTask **end = this->subtasks + this->subtasks_nr;
    SubTask **p = this->subtasks;

    this->nleft = this->subtasks_nr;
    if (this->nleft != 0) 
    {
        do
        {
            (*p)->parent = this;
            (*p)->entry = p;
            (*p)->dispatch();
        } while (++p != end);
    } else {
        this->subtask_done();
    }
}