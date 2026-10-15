#include "oalIPC.h"
#include "socket.h"

#include <arpa/inet.h>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>

struct oalIPC {
    unix_socket_t *socket;
};

int oalIPC_init(oalIPC_t **ipc, const oalIPC_config_t *config)
{
    oalIPC_t *instance;
    unix_socket_role_t role;

    if (ipc == NULL || *ipc != NULL || config == NULL ||
        (config->role != OALIPC_SERVER && config->role != OALIPC_CLIENT)) {
        errno = EINVAL;
        return -1;
    }
    instance = calloc(1, sizeof(*instance));
    if (instance == NULL)
        return -1;
    role = config->role == OALIPC_SERVER ? UNIX_SOCKET_SERVER : UNIX_SOCKET_CLIENT;
    if (unix_socket_open(&instance->socket, role, config->endpoint) == -1) {
        int saved_errno = errno;
        free(instance);
        errno = saved_errno;
        return -1;
    }
    *ipc = instance;
    return 0;
}

int oalIPC_send(oalIPC_t *ipc, const void *message, size_t size)
{
    uint32_t header;

    if (ipc == NULL || message == NULL || size == 0) {
        errno = EINVAL;
        return -1;
    }
    if (size > OALIPC_MAX_MESSAGE_SIZE) {
        errno = EMSGSIZE;
        return -1;
    }
    header = htonl((uint32_t)size);
    if (unix_socket_send(ipc->socket, &header, sizeof(header)) == -1)
        return -1;
    return unix_socket_send(ipc->socket, message, size);
}

ssize_t oalIPC_receive(oalIPC_t *ipc, void *message, size_t capacity)
{
    uint32_t header;
    size_t size;
    int result;

    if (ipc == NULL || message == NULL || capacity == 0) {
        errno = EINVAL;
        return -1;
    }
    result = unix_socket_receive(ipc->socket, &header, sizeof(header));
    if (result != 1)
        return result;
    size = ntohl(header);
    if (size == 0 || size > OALIPC_MAX_MESSAGE_SIZE || size > capacity) {
        errno = EMSGSIZE;
        return -1;
    }
    result = unix_socket_receive(ipc->socket, message, size);
    if (result == 0)
        errno = EPROTO;
    return result == 1 ? (ssize_t)size : -1;
}

void oalIPC_close(oalIPC_t **ipc)
{
    if (ipc == NULL || *ipc == NULL)
        return;
    unix_socket_close(&(*ipc)->socket);
    free(*ipc);
    *ipc = NULL;
}
