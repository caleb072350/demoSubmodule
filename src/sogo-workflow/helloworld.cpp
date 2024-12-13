#include <stdio.h>
#include "workflow/WFHttpServer.h"

int main() {
    WFHttpServer server([](WFHttpTask *task) {
        task->get_resp()->append_output_body("<html>Hello World! This is sogo workflow!</html>");
    });

    if (server.start(80) == 0) {  // start server on port 80
        getchar(); // press "Enter" to end.
        server.stop();
    } else {
        printf("start server failed!\n");
    }

    return 0;
}