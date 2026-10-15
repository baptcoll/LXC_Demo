#include "socket_posix.h"

#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

int socket_posix_init(socket_posix_server *server, const char *path)
{
    struct sockaddr_un address = {0};
    int saved_errno;

    if (server == NULL || path == NULL || path[0] == '\0') {
        errno = EINVAL;
        return -1;
    }
    if (server->fd != -1) {
        errno = EALREADY;
        return -1;
    }
    if (strlen(path) >= sizeof(address.sun_path)) {
        errno = ENAMETOOLONG;
        return -1;
    }

    address.sun_family = AF_UNIX;
    strcpy(address.sun_path, path);
    server->fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (server->fd == -1)
        return -1;

    if (bind(server->fd, (struct sockaddr *)&address, sizeof(address)) == -1) {
        saved_errno = errno;
        close(server->fd);
        server->fd = -1;
        errno = saved_errno;
        return -1;
    }
    strcpy(server->path, path);
    if (listen(server->fd, 4) == -1) {
        saved_errno = errno;
        socket_posix_close(server);
        errno = saved_errno;
        return -1;
    }
    return 0;
}

int socket_posix_accept(const socket_posix_server *server)
{
    return accept(server->fd, NULL, NULL);
}

ssize_t socket_posix_receive(int client_fd, void *buffer, size_t capacity)
{
    return recv(client_fd, buffer, capacity, 0);
}

int socket_posix_send(int client_fd, const void *buffer, size_t length)
{
    const unsigned char *bytes = buffer;
    size_t sent = 0;

    while (sent < length) {
        ssize_t count = send(client_fd, bytes + sent, length - sent, 0);
        if (count < 0) {
            if (errno == EINTR)
                continue;
            return -1;
        }
        if (count == 0) {
            errno = EPIPE;
            return -1;
        }
        sent += (size_t)count;
    }
    return 0;
}

void socket_posix_close(socket_posix_server *server)
{
    if (server->fd != -1) {
        close(server->fd);
        server->fd = -1;
    }
    if (server->path[0] != '\0') {
        unlink(server->path);
        server->path[0] = '\0';
    }
}
