#define _POSIX_C_SOURCE 200809L

#include "socket_posix.h"

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char **argv)
{
    socket_posix_server server = SOCKET_POSIX_SERVER_INITIALIZER;
    const char *path = argc > 1 ? argv[1] : "/tmp/lxc_demo_ext.sock";
    struct sigaction action = {0};
    char buffer[4096];
    int client_fd;
    int result = EXIT_SUCCESS;

    if (argc > 2) {
        fprintf(stderr, "Usage: %s [socket_path]\n", argv[0]);
        return EXIT_FAILURE;
    }
    action.sa_handler = SIG_IGN;
    sigemptyset(&action.sa_mask);
    if (sigaction(SIGPIPE, &action, NULL) == -1) {
        perror("sigaction");
        return EXIT_FAILURE;
    }
    if (socket_posix_init(&server, path) == -1) {
        perror("socket_posix_init");
        return EXIT_FAILURE;
    }
    printf("Ext listening on %s\n", path);
    fflush(stdout);

    do {
        client_fd = socket_posix_accept(&server);
    } while (client_fd == -1 && errno == EINTR);
    if (client_fd == -1) {
        perror("socket_posix_accept");
        socket_posix_close(&server);
        return EXIT_FAILURE;
    }

    /* Initial exchange: echo bytes until this client disconnects. A stream
     * does not preserve message boundaries; a protocol can be added later. */
    for (;;) {
        ssize_t count = socket_posix_receive(client_fd, buffer, sizeof(buffer));
        if (count == 0)
            break;
        if (count < 0) {
            if (errno == EINTR)
                continue;
            perror("socket_posix_receive");
            result = EXIT_FAILURE;
            break;
        }
        if (socket_posix_send(client_fd, buffer, (size_t)count) == -1) {
            perror("socket_posix_send");
            result = EXIT_FAILURE;
            break;
        }
    }
    close(client_fd);
    socket_posix_close(&server);
    return result;
}
