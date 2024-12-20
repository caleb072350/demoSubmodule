#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>


#define SERVER_IP "127.0.0.1"
#define PORT 9898

int main() {
    int  sock_fd;
    struct sockaddr_in server_addr;

    sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0) {
        perror("socket");
        return -1;
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(SERVER_IP);
    server_addr.sin_port = htons(PORT);

    if (connect(sock_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("connect");
        close(sock_fd);
        return -1;
    }

    printf("connect to server %s:%d success!\n", SERVER_IP, PORT);

    const char *message = "Hello from client!\n";
    size_t len = strlen(message);

    size_t send = write(sock_fd, message, len);
    if (send < len) {
        perror("send");
    } else {
        printf("send success!\n");
    }

    while (1)
    {
        sleep(1);
    }
    return 0;
}