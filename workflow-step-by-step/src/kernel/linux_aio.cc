#include <libaio.h>
#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <errno.h>

#define BUFFER_SIZE 1024
#define MAX_EVENTS  10
#define EPOLL_TIMEOUT 1000 // 超时时间，单位：毫秒

int main() {
    // 打开文件
    int fd = open("example.txt", O_RDONLY | O_DIRECT);
    if (fd == -1) {
        std::cerr << "Failed to open file: " << strerror(errno) << std::endl;
        return 1;
    }

    // 初始化 AIO 上下文
    io_context_t ctx;
    memset(&ctx, 0, sizeof(ctx)); // 这行代码必须得有
    if (io_setup(10, &ctx) < 0) {
        std::cerr << "Failed to initialize AIO context : " << strerror(errno) << std::endl;
        close(fd);
        return 1;
    }

    // 创建eventfd
    int efd = eventfd(0, EFD_NONBLOCK);
    if (efd == -1) {
        perror("Failed to create eventfd!");
        io_destroy(ctx);
        close(fd);
        return 1;
    }

    // 创建epoll实例
    int epfd = epoll_create1(0);
    if (epfd == -1) {
        perror("Failed to create epoll instance");
        close(efd);
        io_destroy(ctx);
        close(fd);
        return 1;
    }

    // 将 eventfd 添加到 epoll
    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = efd;
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, efd, &ev) == -1) {
        perror("Failed to add eventfd to epoll");
        close(epfd);
        close(efd);
        io_destroy(ctx);
        close(fd);
        return 1;
    }

    // 分配缓冲区
    void *buffer = aligned_alloc(512, BUFFER_SIZE);
    if (!buffer) {
        std::cerr << "Failed to allocate buffer" << std::endl;
        io_destroy(ctx);
        close(fd);
        return 1;
    }

    // 初始化 iocb 结构体
    struct iocb cb;
    io_prep_pread(&cb, fd, buffer, BUFFER_SIZE, 0);
    io_set_eventfd(&cb, efd);

    // 提交异步读请求
    struct iocb *cbs[1] = { &cb };
    if (io_submit(ctx, 1, cbs) < 0) {
        perror("Failed to submit AIO request");
        free(buffer);
        close(epfd);
        close(efd);
        io_destroy(ctx);
        close(fd);
        return 1;
    }

    // 事件循环
    while (true) {
        struct epoll_event events[MAX_EVENTS];
        int nfds = epoll_wait(epfd, events, MAX_EVENTS, EPOLL_TIMEOUT);
        if (nfds == -1) {
            perror("epoll_wait failed");
            break;
        } else if (nfds == 0) {
            // 超时，执行其他任务
            std::cout << "Timeout occurred, performing other tasks..." << std::endl;
            continue;
        }

        // 处理事件
        for (int i = 0; i < nfds; ++i) {
            if (events[i].data.fd == efd) {
                uint64_t result;
                if (read(efd, &result, sizeof(result)) != sizeof(result)) {
                    perror("Failed to read from eventfd");
                    break;
                }

                // 获取 AIO 事件
                struct io_event aio_events[1];
                int num_events = io_getevents(ctx, 1, 1, aio_events, NULL);
                if (num_events > 0) {
                    // 处理 AIO 完成事件
                    std::cout << "Read " << aio_events[0].res << " bytes: "
                              << std::string(static_cast<char*>(buffer), aio_events[0].res) << std::endl;
                } else {
                    perror("Failed to get AIO events");
                }
            }
        }
    }

    free(buffer);
    close(epfd);
    close(efd);
    io_destroy(ctx);
    close(fd);

    return 0;
}
