#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <unistd.h>

#define SHM_KEY 1234
#define SHM_SIZE sizeof(shared_data_t)
#define FIFO_PATH "/tmp/my_fifo"

typedef struct {
    int data;
    int flag; // 0: 无数据，1: 有数据
} shared_data_t;

int main() {
    // 创建共享内存
    int shm_id = shmget(SHM_KEY, SHM_SIZE, 0666);
    if (shm_id == -1) {
        perror("shmget");
        exit(EXIT_FAILURE);
    }
    shared_data_t *shm_ptr = (shared_data_t *)shmat(shm_id, NULL, 0);
    if (shm_ptr == (void *)-1) {
        perror("shmat");
        exit(EXIT_FAILURE);
    }

    // 打开 FIFO
    int fd = open(FIFO_PATH, O_RDONLY | O_NONBLOCK);
    if (fd == -1) {
        perror("open fifo failed!");
        exit(EXIT_FAILURE);
    }

    // 创建 epoll 实例
    int epfd = epoll_create1(0);
    if (epfd == -1) {
        perror("epoll_create1");
        exit(EXIT_FAILURE);
    }

    // 添加 FIFO 到 epoll 实例
    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = fd;
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev) == -1) {
        perror("epoll_ctl");
        exit(EXIT_FAILURE);
    }

    // 等待事件
    struct epoll_event events[1];
    int nfds = epoll_wait(epfd, events, 1, -1);
    if (nfds == -1) {
        perror("epoll_wait");
        exit(EXIT_FAILURE);
    }

    if (events[0].data.fd == fd) {
        char msg;
        if (read(fd, &msg, sizeof(msg)) == -1) {
            perror("read from fifo");
            exit(EXIT_FAILURE);
        }
        if (shm_ptr->flag == 1) {
            printf("Received data: %d\n", shm_ptr->data);
            shm_ptr->flag = 0;
        }
    }

    // 清理
    close(fd);
    close(epfd);
    if (shmdt(shm_ptr) == -1) {
        perror("shmdt");
        exit(EXIT_FAILURE);
    }

    return 0;
}
