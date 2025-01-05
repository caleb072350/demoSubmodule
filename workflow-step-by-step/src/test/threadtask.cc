#include <iostream>
#include <unistd.h>
#include <thread>
#include <vector>
#include <mutex>
#include "WFTask.h"
#include "Executor.h"
#include "Workflow.h"

using namespace std;

template<class INPUT, class OUTPUT>
class MyThreadTask : public WFThreadTask<INPUT, OUTPUT>
{
    void execute()
    {   
        cout << "hello,world" << "!" << endl;
        cout << "execute" << endl;
        sleep(2);
    }

public:
    void callback(WFThreadTask<string, string> * task)
    {
        cout << "req: " << task->get_input()->c_str() << endl;
        cout << "resp: " << task->get_output()->c_str() << endl;
    }

public:
    MyThreadTask(ExecQueue* q, Executor *exec): WFThreadTask<INPUT, OUTPUT>(q, exec, nullptr) {}
    virtual ~MyThreadTask(){}
};

int main()
{
    const size_t num_threads = 4;
    const size_t num_task = 10;

    // Initialize the executor
    Executor executor;
    if (executor.init(num_threads) < 0)
    {
        cout << "Failed to initialize executor" << endl;
        return -1;
    }

    // Initialize the execQueue
    ExecQueue queue;
    if (queue.init() != 0)
    {
        cout << "Failed to initialize execQueue" << endl;
        executor.deinit();
        return -1;
    }

    MyThreadTask<string, string> *threadTask = new MyThreadTask<string, string>(&queue, &executor);
    std::function<void (WFThreadTask<string, string> *)>&& cb = std::bind(&MyThreadTask<string, string>::callback, threadTask, std::placeholders::_1);
    threadTask->set_callback(cb);
    threadTask->get_input()->assign("input");
    threadTask->get_output()->assign("output");

    threadTask->start();
    sleep(4);
}