#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "thrdpool.h"

// 定义任务执行函数
void task_function(void *arg) {
    int task_id = *(int*)arg;
    printf("Task %d is being executed.\n", task_id);
    sleep(1); // 模拟任务执行时间
}

int main() {
    size_t num_threads = 4; // 线程池中的线程数量
    thrdpool_t *pool = thrdpool_create(num_threads, 0); // 创建线程池

    if (!pool) {
        fprintf(stderr, "Failed to create thread pool!\n");
        return -1;
    }

    // 调度任务
    for (int i = 0; i < 10; i++) {
        struct thrdpool_task task;
        task.routine = task_function;
        // 为每个任务分配一个新的任务ID
        int *task_id = malloc(sizeof(int));
        if (task_id) {
            *task_id = i;
            task.context = task_id;
            if (thrdpool_schedule(&task, pool) != 0) {
                fprintf(stderr, "Failed to schedule task %d\n", i);
                free(task_id);
            }
        } else {
            fprintf(stderr, "Failed to allocate memory for task ID.\n");
        }
    }

    // 等待所有任务完成
    sleep(5);
    // 销毁线程池
    thrdpool_destroy(NULL, pool);
    return 0;
}