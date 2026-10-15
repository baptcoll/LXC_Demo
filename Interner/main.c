#include "../common/message_queue.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void)
{
    message_queue_t *queue = NULL;
    const char expected[] = "COUCOU";
    const char response[] = "POUET";
    char buffer[MESSAGE_QUEUE_MAX_SIZE];
    int result = EXIT_SUCCESS;

    setvbuf(stdout, NULL, _IOLBF, 0);

    /* Intf_Ext creates the queues; Interner opens them in the other direction. */
    if (message_queue_connect(&queue) == -1) {
        perror("[Interner] message_queue_connect");
        return EXIT_FAILURE;
    }
    puts("[Interner] Connected to Intf_Ext message queues");

    for (;;) {
        ssize_t size = message_queue_receive(queue, buffer, sizeof(buffer));
        if (size == -1) {
            perror("[Interner] message_queue_receive");
            result = EXIT_FAILURE;
            break;
        }
        printf("[Interner] Queue RX: %.*s\n", (int)size, buffer);
        if ((size_t)size != sizeof(expected) - 1 ||
            memcmp(buffer, expected, sizeof(expected) - 1) != 0) {
            fprintf(stderr, "[Interner] Invalid request: expected COUCOU\n");
            result = EXIT_FAILURE;
            break;
        }

        if (message_queue_send(queue, response, sizeof(response) - 1) == -1) {
            perror("[Interner] message_queue_send");
            result = EXIT_FAILURE;
            break;
        }
        puts("[Interner] Queue TX: POUET");
    }

    message_queue_close(&queue);
    return result;
}
