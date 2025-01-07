#include <iostream>
#include <unistd.h>
#include <thread>
#include <vector>
#include <mutex>
#include "WFTask.h"
#include "Executor.h"
#include "Workflow.h"
#include "WFGlobal.h"

using namespace std;

class MyClass
{
private:
    int a;
    int b;
public:
    MyClass(int x, int y):a(x),b(y) {}
    int get_a() {return a;}
    int get_b() {return b;}
};

void timer_callback(WFTimerTask * timer) 
{
    MyClass* mc = (MyClass*)timer->get_pointer();
    cout << mc->get_a() << endl;
    cout << mc->get_b() << endl;
}

class MyTimerTask : public WFTimerTask
{
private:
    virtual int duration(struct timespec *value)
    {
        *value = this->value;
        cout << "45" << endl;
        if (value == NULL)
        {
            cout << "Invalid timespec pointer." << endl;
            return -1;
        }
        int result = nanosleep(value, NULL);
        return 0;
    }
public:
    MyTimerTask(const struct timespec *value, CommScheduler* scheduler, std::function<void (WFTimerTask *)> cb):
        WFTimerTask(scheduler, cb)
    {
        this->value = *value;
    }
    
    virtual ~MyTimerTask() {}

    void set_callback(std::function<void (WFTimerTask *)> cb)
    {
        this->callback = cb;
    }
protected:
    struct timespec value;
};

void _callback(WFTimerTask * arg)
{
    cout << "callback" << endl;
}

int main()
{
    struct timespec* ts = new struct timespec;
    ts->tv_sec = 1;
    ts->tv_nsec = 0;
    MyTimerTask* timerTask = new MyTimerTask(ts, WFGlobal::get_scheduler(), nullptr);
    std::function<void (WFTimerTask *)>&& cb = std::bind(_callback, timerTask);
    timerTask->set_callback(cb);
    timerTask->start();
}