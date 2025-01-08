#include <iostream>
#include <unistd.h>
#include <thread>
#include <vector>
#include <mutex>
#include <condition_variable>
#include "WFTask.h"
#include "Executor.h"
#include "Workflow.h"
#include "WFGlobal.h"
#include "WFTaskFactory.h"

using namespace std;

int main()
{
    mutex mtx;
    condition_variable cond;
    bool done = false;
    struct timespec value = {
        .tv_sec = 2,
        .tv_nsec = 0
    };

    auto *task = WFTaskFactory::create_timer_task(1000000, [&mtx, &cond, &done](WFTimerTask *task) {
        mtx.lock();
        done = true;
        mtx.unlock();
        cond.notify_one();
        printf("success!\n");
    });
    task->start();

    unique_lock<std::mutex> lock(mtx);
    while (!done)
        cond.wait(lock);
    lock.unlock();
    return 0;
}