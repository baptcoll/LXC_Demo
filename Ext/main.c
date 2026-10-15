#include "oalIPC.h"
#include "log_output.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv)
{
    oalIPC *ipc = NULL;
    oalIPC_config config = {
        OAL_IPC_SOCKET, OAL_IPC_SERVER, "/tmp/lxc_demo_ext.sock"
    };
    static const char request[] = "COUCOU";
    static const char expected[] = "SALUT";
    char buffer[OAL_IPC_MAX_SIZE];
    int result = EXIT_SUCCESS;

    if (log_output_init("ext.log") == -1) {
        perror("ext.log");
        return EXIT_FAILURE;
    }

    /* ********************************************************************
     * INPUT
     * *******************************************************************/
    if (argc > 2) {
        fprintf(stderr, "Usage: %s [socket_path]\n", argv[0]);
        return EXIT_FAILURE;
    }
    if (argc == 2)
        config.endpoint = argv[1];

    printf("Ext starting on %s\n", config.endpoint);
    fflush(stdout);

    /* ********************************************************************
     * INIT
     * *******************************************************************/
    if (oalIPC_init(&ipc, &config) == -1) {
        perror("oalIPC_init");
        return EXIT_FAILURE;
    }


    /* ********************************************************************
     * ECHANGE
     * *******************************************************************/
    /* Fixed-size wire protocol: 6 request bytes and 5 response bytes,
     * without a terminating NUL or newline. Socket reads may be partial. */
    for (;;) {
        size_t received = 0;
        int disconnected = 0;

        if (oalIPC_send(ipc, request, sizeof(request) - 1) == -1) {
            perror("oalIPC_send");
            result = EXIT_FAILURE;
            break;
        }
        puts("[Ext] TX: COUCOU");
        fflush(stdout);
        while (received < sizeof(expected) - 1) {
            ssize_t count = oalIPC_receive(ipc, buffer, sizeof(buffer));
            if (count == -1) {
                perror("oalIPC_receive");
                result = EXIT_FAILURE;
                break;
            }
            if (count == 0) {
                disconnected = 1;
                if (received != 0) {
                    fprintf(stderr, "Incomplete response: expected SALUT\n");
                    result = EXIT_FAILURE;
                }
                break;
            }
            if ((size_t)count > sizeof(expected) - 1 - received ||
                memcmp(buffer, expected + received, (size_t)count) != 0) {
                fprintf(stderr, "Invalid response: expected SALUT\n");
                result = EXIT_FAILURE;
                break;
            }
            received += (size_t)count;
        }
        if (result != EXIT_SUCCESS || disconnected)
            break;
        puts("[Ext] RX: SALUT");
        fflush(stdout);
    }
    if (result == EXIT_SUCCESS && oalIPC_shutdown(ipc) == -1) {
        perror("oalIPC_shutdown");
        result = EXIT_FAILURE;
    }
    oalIPC_close(&ipc);
    return result;
}
