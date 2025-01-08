#include <iostream>
#include <thread>
#include <chrono>
#include <csignal>
#include <cstdlib>

using namespace std;

// 信号处理
void signalHandler(int signum)
{
    cout << "receive signal: " << signum << ", exit!" << endl;
    exit(signum);
}

int main()
{
     // 注册信号处理函数
    std::signal(SIGINT, signalHandler);  // 捕获 Ctrl+C 信号
    std::signal(SIGTERM, signalHandler); // 捕获终止信号
    std::cout << "目标进程启动, PID: " << getpid() << std::endl;

    while (true)
    {
        std::cout << "目标进程正在运行..." << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(5)); // 每隔5秒输出一次
    }
    return 0;
}