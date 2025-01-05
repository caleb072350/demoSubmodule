#include <iostream>
#include <unistd.h>
#include <thread>
#include <vector>
#include <mutex>
#include "Executor.h"

using namespace std;

pthread_mutex_t mtx;

class MyExecSession : public ExecSession
{
private:
    void execute() override
    {
        pthread_mutex_lock(&mtx);
        cout << "Executing task in thread " << this_thread::get_id() << endl;
        pthread_mutex_unlock(&mtx);
        sleep(1);
    }

    void handle(int state, int error) override
    {
        pthread_mutex_lock(&mtx);
        cout << "task complete with state: " << state << " and error: " << error << " in thread " << this_thread::get_id() << endl;
        pthread_mutex_unlock(&mtx);
    }
public:
    virtual ~MyExecSession() {}
};

int main()
{
    const size_t num_threads = 4;
    const size_t num_task = 10;

    pthread_mutex_init(&mtx, NULL);

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

    // Create and submit tasks
    vector<MyExecSession*> sessions;
    for(size_t i = 0; i < num_task; i++)
    {
        MyExecSession *session = new MyExecSession();
        sessions.push_back(session);
        if (executor.request(session, &queue) != 0)
        {
            cout << "Failed to submit task" << endl;
            for (auto s : sessions) delete s;
            queue.deinit();
            executor.deinit();
            return -1;
        }
    }

    // Allow some time for tasks to complete
    sleep(5);

    // clean up
    for (auto session : sessions)
        delete session;
    queue.deinit();
    executor.deinit();
    
    return 0;
}
