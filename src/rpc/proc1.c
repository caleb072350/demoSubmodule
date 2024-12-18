#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#define SHM_KEY 1234
#define SHM_SIZE sizeof(shard_data_t)
#define FIFO_PATH "/tmp/my_fifo"

typedef struct {
    int data;
    int flags; // 0:无数据， 1：有数据
} shard_data_t;

int main() {
    // 创建共享内存
    int shm_id = shmget(SHM_KEY, SHM_SIZE, IPC_CREAT|0600);
    if (shm_id == -1) {
        perror("shmget");
        exit(-1);
    }
    shard_data_t *shm_ptr = (shard_data_t*)shmat(shm_id, NULL, 0);
    if (shm_ptr == (void*)-1) {
        perror("shmat");
        exit(-1);
    }

    // 创建FIFO
    if (mkfifo(FIFO_PATH, 0600) == -1 && errno != EEXIST) {
        perror("mkfifo");
        exit(-1);
    }

    // 写入共享内存
    shm_ptr->data = 42;
    shm_ptr->flags = 1;

    // 通知进程2
    int fd = open(FIFO_PATH, O_WRONLY);
    if (fd == -1) {
        perror("open fifo");
        exit(-1);
    }

    char msg = '1';
    if (write(fd, &msg, sizeof(msg)) == -1) {
        perror("write to fifo failed!");
        exit(-1);
    }

    // 等待进程2读取
    while (shm_ptr->flags == 1) {
        sleep(1);
    }

    // 清理
    if (shmdt(shm_ptr) == -1) {
        perror("shmdt");
        exit(-1);
    }

    if (shmctl(shm_id, IPC_RMID, NULL) == -1) {
        perror("shmctl");
        exit(-1);
    }

    close(fd);
    return 0;
}
