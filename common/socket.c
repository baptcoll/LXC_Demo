#define _POSIX_C_SOURCE 200809L

#include "socket.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

struct unix_socket {
    int connection;
    int listener;
    int owns_endpoint;
    char endpoint[sizeof(((struct sockaddr_un *)0)->sun_path)];
};

/* Return 1 for a complete read, 0 for EOF before any bytes, or -1 on error.
 * EOF inside the requested byte range is treated as a truncated transfer. */
int unix_socket_receive(unix_socket_t *connection, void *buffer, size_t size)
{
    unsigned char *bytes = buffer;
    size_t offset = 0;

    if (connection == NULL || buffer == NULL || size == 0) {
        errno = EINVAL;
        return -1;
    }
    while (offset < size) {
        ssize_t count = recv(connection->connection, bytes + offset, size - offset, 0);
        if (count == -1) {
            if (errno == EINTR)
                continue;
            return -1;
        }
        if (count == 0) {
            if (offset == 0)
                return 0;
            errno = EPROTO;
            return -1;
        }
        offset += (size_t)count;
    }
    return 1;
}

int unix_socket_send(unix_socket_t *connection, const void *buffer, size_t size)
{
    const unsigned char *bytes = buffer;
    size_t offset = 0;

    if (connection == NULL || buffer == NULL || size == 0) {
        errno = EINVAL;
        return -1;
    }
    while (offset < size) {
        ssize_t count = send(connection->connection, bytes + offset, size - offset, MSG_NOSIGNAL);
        if (count == -1) {
            if (errno == EINTR)
                continue;
            return -1;
        }
        if (count == 0) {
            errno = EPIPE;
            return -1;
        }
        offset += (size_t)count;
    }
    return 0;
}

int unix_socket_open(unix_socket_t **connection, unix_socket_role_t role,
                     const char *path)
{
    struct sockaddr_un address = {0};
    unix_socket_t *instance;
    int saved_errno;

    if (connection == NULL || *connection != NULL ||
        path == NULL || path[0] == '\0' ||
        (role != UNIX_SOCKET_SERVER && role != UNIX_SOCKET_CLIENT)) {
        errno = EINVAL;
        return -1;
    }
    if (strlen(path) >= sizeof(address.sun_path)) {
        errno = ENAMETOOLONG;
        return -1;
    }
    instance = calloc(1, sizeof(*instance));
    if (instance == NULL)
        return -1;
    instance->connection = -1;
    instance->listener = -1;
    strcpy(instance->endpoint, path);
    address.sun_family = AF_UNIX;
    strcpy(address.sun_path, path);

    if (role == UNIX_SOCKET_SERVER) {
        instance->listener = socket(AF_UNIX, SOCK_STREAM, 0);
        if (instance->listener == -1)
            goto fail;
        /* Do not unlink an existing endpoint: it may belong to another server. */
        if (bind(instance->listener, (struct sockaddr *)&address, sizeof(address)) == -1)
            goto fail;
        instance->owns_endpoint = 1;
        if (listen(instance->listener, 1) == -1)
            goto fail;
        do {
            instance->connection = accept(instance->listener, NULL, NULL);
        } while (instance->connection == -1 && errno == EINTR);
        if (instance->connection == -1)
            goto fail;
        close(instance->listener);
        instance->listener = -1;
    } else {
        instance->connection = socket(AF_UNIX, SOCK_STREAM, 0);
        if (instance->connection == -1)
            goto fail;
        if (connect(instance->connection, (struct sockaddr *)&address, sizeof(address)) == -1)
            goto fail;
    }

    *connection = instance;
    return 0;

fail:
    saved_errno = errno;
    unix_socket_close(&instance);
    errno = saved_errno;
    return -1;
}

void unix_socket_close(unix_socket_t **connection)
{
    unix_socket_t *instance;

    if (connection == NULL || *connection == NULL)
        return;
    instance = *connection;
    if (instance->connection != -1)
        close(instance->connection);
    if (instance->listener != -1)
        close(instance->listener);
    if (instance->owns_endpoint)
        unlink(instance->endpoint);
    free(instance);
    *connection = NULL;
}
