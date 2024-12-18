#include <libaio.h>
#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
// #include <linux/aio_abi.h>

#define BUFFER_SIZE 1024

int main() {
    // 打开文件
    int fd = open("example.txt", O_RDONLY);
    if (fd == -1) {
        std::cerr << "Failed to open file: " << strerror(errno) << std::endl;
        return 1;
    }

    // 初始化 AIO 上下文
    io_context_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    // if (io_queue_init(10, &ctx) < 0) {
    //     std::cerr << "Failed to initialize AIO context" << std::endl;
    //     close(fd);
    //     return 1;
    // }
    if (io_setup(10, &ctx) < 0) {
        std::cerr << "Failed to initialize AIO context" << std::endl;
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

    struct iocb cb;
    // memset(&cb, 0, sizeof(cb));
    // cb.aio_fildes = fd;
    // cb.aio_lio_opcode = IO_CMD_PREAD;
    // cb.u.c.buf = (void *)malloc(BUFFER_SIZE);
    // cb.u.c.nbytes = BUFFER_SIZE;
    // cb.u.c.offset = 0;
    // 初始化 iocb 结构体
    io_prep_pread(&cb, fd, buffer, BUFFER_SIZE, 0);

    struct iocb *cbs[1] = { &cb };
    if (io_submit(ctx, 1, cbs) < 0) {
        perror("Failed to submit AIO request");
        free((void *)buffer);
        // io_queue_release(ctx);
        io_destroy(ctx);
        close(fd);
        return 1;
    }

    struct io_event events[1];
    if (io_getevents(ctx, 1, 1, events, NULL) < 0) {
        perror("Failed to get AIO events");
        free((void *)cb.u.c.buf);
        io_queue_release(ctx);
        close(fd);
        return 1;
    }

    printf("Read %ld bytes: %s\n", events[0].res, (char *)buffer);

    free(buffer);
    // io_queue_release(ctx);
    io_destroy(ctx);
    close(fd);

    return 0;
}
