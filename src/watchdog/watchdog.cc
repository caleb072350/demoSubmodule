#include <iostream>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>
#include <cstdlib>
#include <cstring>
#include <cassert>
#include <vector>

struct ProcessInfo
{
    const char* name;
    const char* path;
    pid_t pid;
};

volatile sig_atomic_t shutdownSignal = 0;

void sigHandler(int signal)
{
    if (signal == SIGTERM || signal == SIGINT || signal == SIGKILL) {
        shutdownSignal = 1;
    }
}

void startTargetProcess(ProcessInfo& process) {
    process.pid = fork();
    if (process.pid < 0) {
        std::cerr << "Fork failed for " << process.name << ": " << strerror(errno) << std::endl;
        std::exit(EXIT_FAILURE);
    } else if (process.pid == 0) {
        // 子进程：执行目标进程的可执行文件
        execl(process.path, process.name, nullptr);
        // 如果 execl 返回，说明执行失败
        std::cerr << "Failed to start " << process.name << ": " << strerror(errno) << std::endl;
        std::exit(EXIT_FAILURE);
    }
}

void monitorProcesses(std::vector<ProcessInfo>& processes) {
    while (true) {
        if (shutdownSignal) {
            std::cout << "Shutdown signal received. Terminating target processes..." << std::endl;
            for (auto& process : processes) {
                if (process.pid > 0) {
                    kill(process.pid, SIGTERM); // 通知子进程退出
                    int status;
                    waitpid(process.pid, &status, 0); // 等待子进程退出
                    std::cout << process.name << " exited." << std::endl;
                }
            }
            std::cout << "Target process exited. Watchdog exiting." << std::endl;
            std::exit(EXIT_SUCCESS);
        }

        for (auto& process : processes) {
            int status;
            pid_t result = waitpid(process.pid, &status, WNOHANG);
            if (result == 0) {
                // 子进程在正常运行
                continue;
            }
            if (result < 0) {
                std::cerr << "Waitpid failed for " << process.name << ": " << strerror(errno) << std::endl;
                std::exit(EXIT_FAILURE);
            }
            if (WIFEXITED(status)) {
                std::cout << process.name << " exited with status " << WEXITSTATUS(status) << std::endl;
            } else if (WIFSIGNALED(status)) {
                std::cout << process.name << " killed by signal " << WTERMSIG(status) << std::endl;
            }
            // 等待一段时间后重新启动子进程
            sleep(3);
            std::cout << "Restarting " << process.name << "..." << std::endl;
            startTargetProcess(process);
        }
        sleep(5); // 每隔5秒检查一次子进程状态
    }
}

int main() {

    // 注册信号处理函数
    struct sigaction sa;
    sa.sa_handler = sigHandler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGTERM, &sa, nullptr);
    sigaction(SIGINT, &sa, nullptr);

    std::cout << "Watchdog process started, PID: " << getpid() << std::endl;
    // 初始化要监控的进程信息
    std::vector<ProcessInfo> processes = {
        {"target_process", "/usr/local/target_process", -1},
        {"target_process2", "/usr/local/target_process2", -1}
    };

    // 启动所有目标进程
    for (auto& process : processes) {
        startTargetProcess(process);
    }

    // 监控所有目标进程
    monitorProcesses(processes);
    return 0;
}
