#include "../common/oalIPC.h"
#include "../common/message_queue.h"

#include <stdio.h>
#include <stdlib.h>

_Static_assert(MESSAGE_QUEUE_MAX_SIZE == OALIPC_MAX_MESSAGE_SIZE,
               "Socket and queue message sizes must match");

int main(void)
{
    oalIPC_t *ipc = NULL;
    message_queue_t *queue = NULL;
    const oalIPC_config_t config = {
        .role = OALIPC_CLIENT,
        .endpoint = "/tmp/lxc_demo.sock"
    };
    char buffer[OALIPC_MAX_MESSAGE_SIZE];
    int result = EXIT_SUCCESS;

    /* Keep logs immediately visible, including when stdout is redirected. */
    setvbuf(stdout, NULL, _IOLBF, 0);

    if (message_queue_init(&queue) == -1) {
        perror("[Intf_Ext] message_queue_init");
        return EXIT_FAILURE;
    }
    puts("[Intf_Ext] Message queues ready: " MESSAGE_QUEUE_REQUEST_NAME
         " / " MESSAGE_QUEUE_RESPONSE_NAME);

    puts("[Intf_Ext] Connecting to Ext on /tmp/lxc_demo.sock");
    if (oalIPC_init(&ipc, &config) == -1) {
        perror("[Intf_Ext] oalIPC_init");
        message_queue_close(&queue);
        return EXIT_FAILURE;
    }

    for (;;) {
        ssize_t size = oalIPC_receive(ipc, buffer, sizeof(buffer));
        if (size == -1) {
            perror("[Intf_Ext] oalIPC_receive");
            result = EXIT_FAILURE;
            break;
        }
        if (size == 0) {
            puts("[Intf_Ext] Ext disconnected");
            break;
        }
        printf("[Intf_Ext] Socket RX: %.*s\n", (int)size, buffer);
        if (message_queue_send(queue, buffer, (size_t)size) == -1) {
            perror("[Intf_Ext] message_queue_send");
            result = EXIT_FAILURE;
            break;
        }
        printf("[Intf_Ext] Queue TX: %.*s\n", (int)size, buffer);

        size = message_queue_receive(queue, buffer, sizeof(buffer));
        if (size == -1) {
            perror("[Intf_Ext] message_queue_receive");
            result = EXIT_FAILURE;
            break;
        }
        printf("[Intf_Ext] Queue RX: %.*s\n", (int)size, buffer);
        if (oalIPC_send(ipc, buffer, (size_t)size) == -1) {
            perror("[Intf_Ext] oalIPC_send");
            result = EXIT_FAILURE;
            break;
        }
        printf("[Intf_Ext] Socket TX: %.*s\n", (int)size, buffer);
    }

    oalIPC_close(&ipc);
    message_queue_close(&queue);
    return result;
}
