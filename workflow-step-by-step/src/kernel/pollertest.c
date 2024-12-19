#include "poller.h"
#include <stdio.h>

int main() {
    struct poller_params params = {
        .max_open_files = 65536,
        .result_queue = NULL,
        .create_message = NULL,
        .partial_written = NULL,
    };

    poller_t *poller = poller_create(&params);
    if (!poller) {
        fprintf(stderr, "Failed to create poller\n");
        return 1;
    } else {
        printf("Poller created");
    }

    poller_destroy(poller);
    return 0;
}