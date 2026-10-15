#include "../common/oalIPC.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void)
{
    oalIPC_t *ipc = NULL;
    const oalIPC_config_t config = {
        .role = OALIPC_SERVER,
        .endpoint = "/tmp/lxc_demo.sock"
    };
    const char request[] = "COUCOU";
    const char expected[] = "POUET";
    char buffer[OALIPC_MAX_MESSAGE_SIZE];
    int result = EXIT_SUCCESS;

    /* Keep logs immediately visible, including when stdout is redirected. */
    setvbuf(stdout, NULL, _IOLBF, 0);

    puts("[Ext] Waiting for a client on /tmp/lxc_demo.sock");
    if (oalIPC_init(&ipc, &config) == -1) {
        perror("[Ext] oalIPC_init");
        return EXIT_FAILURE;
    }

    for (;;) {
        if (oalIPC_send(ipc, request, sizeof(request) - 1) == -1) {
            perror("[Ext] oalIPC_send");
            result = EXIT_FAILURE;
            break;
        }
        puts("[Ext] TX: COUCOU");

        ssize_t size = oalIPC_receive(ipc, buffer, sizeof(buffer));
        if (size == -1) {
            perror("[Ext] oalIPC_receive");
            result = EXIT_FAILURE;
            break;
        }
        if (size == 0) {
            puts("[Ext] Client disconnected");
            break;
        }
        printf("[Ext] RX: %.*s\n", (int)size, buffer);
        if ((size_t)size != sizeof(expected) - 1 ||
            memcmp(buffer, expected, sizeof(expected) - 1) != 0) {
            fprintf(stderr, "[Ext] Invalid response: expected POUET\n");
            result = EXIT_FAILURE;
            break;
        }
    }

    oalIPC_close(&ipc);
    return result;
}
