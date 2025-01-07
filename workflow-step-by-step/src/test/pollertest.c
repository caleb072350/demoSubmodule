#include "poller.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <string.h>

#define PORT 9898
#define BACKLOG 10

// 这里的accept 其实是传入的系统调用accept后返回的fd，也就是对端fd, 可以直接读写该fd, 因此把该fd添加到监听读事件列表中
void *myaccept(const struct sockaddr *addr, socklen_t addrlen, int sockfd, void *context)
{
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);

    poller_t *poller = (poller_t *)context;

    struct poller_data data;
    data.operation = PD_OP_READ;
    data.fd = sockfd;
    data.context = context;
    data.result = NULL;

    poller_add(&data, -1, poller);
}

int main() {
    struct poller_params params = {
        .max_open_files = 65536,
        .result_queue = NULL,
        .create_message = NULL,
        .partial_written = NULL,
    };

    poller_queue_t *queue = poller_queue_create(1024);

    params.result_queue = queue;

    poller_t *poller = poller_create(&params);
    if (!poller) {
        fprintf(stderr, "Failed to create poller\n");
        return 1;
    } else {
        printf("Poller created");
    }

    // 添加监听fd
    int listen_fd;
    struct sockaddr_in server_addr;

    listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        perror("socket error!");
        return -1;
    }

    int opt = 1;
    if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt");
        close(listen_fd);
        exit(EXIT_FAILURE);
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(PORT);

    if (bind(listen_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("bind");
        close(listen_fd);
        return -1;
    }

    if (listen(listen_fd, BACKLOG) < 0) {
        perror("listen");
        close(listen_fd);
        return -1;
    }

    struct poller_data data;
    data.operation = PD_OP_LISTEN;
    data.fd = listen_fd;
    data.accept = myaccept;
    data.context = poller;
    data.result = NULL;

    poller_add(&data, -1,  poller);

    poller_start(poller);

    pthread_join(poller->tid, NULL);

    poller_destroy(poller);
    return 0;
}