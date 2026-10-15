#ifndef SOCKET_POSIX_H
#define SOCKET_POSIX_H

#include <stddef.h>
#include <sys/types.h>
#include <sys/un.h>

typedef struct {
    int fd;
    char path[sizeof(((struct sockaddr_un *)0)->sun_path)];
} socket_posix_server;

/* Initialize before use. init never removes an existing socket path. */
#define SOCKET_POSIX_SERVER_INITIALIZER { -1, { 0 } }

int socket_posix_init(socket_posix_server *server, const char *path);
int socket_posix_accept(const socket_posix_server *server);
/* Returns bytes received, 0 on disconnect, or -1 with errno set. */
ssize_t socket_posix_receive(int client_fd, void *buffer, size_t capacity);
/* Internal helper: sends the entire buffer or returns -1 with errno set.
 * oalIPC configures SO_NOSIGPIPE on hosts without MSG_NOSIGNAL. */
int socket_posix_send(int client_fd, const void *buffer, size_t length);
void socket_posix_close(socket_posix_server *server);

#endif
