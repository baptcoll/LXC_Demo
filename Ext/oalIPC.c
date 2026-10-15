#define _POSIX_C_SOURCE 200809L

#include "oalIPC.h"
#include "socket_posix.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#ifdef __linux__
#include <fcntl.h>
#include <mqueue.h>
#endif

/* Transport details stay private; the application only sees oalIPC. */
struct oalIPC {
    oalIPC_transport transport;
    int write_closed;
    int read_closed;
    int fd;
    socket_posix_server server;
#ifdef __linux__
    mqd_t rx;
    mqd_t tx;
    char c2s[256];
    char s2c[256];
    int owns_c2s;
    int owns_s2c;
#endif
};

static int socket_init(oalIPC *ipc, const oalIPC_config *config)
{
    struct sockaddr_un address = {0};
    if (config->role == OAL_IPC_SERVER) {
        if (socket_posix_init(&ipc->server, config->endpoint) == -1)
            return -1;
        do {
            ipc->fd = socket_posix_accept(&ipc->server);
        } while (ipc->fd == -1 && errno == EINTR);
        if (ipc->fd == -1)
            return -1;
    } else {
        if (strlen(config->endpoint) >= sizeof(address.sun_path)) {
            errno = ENAMETOOLONG;
            return -1;
        }
        address.sun_family = AF_UNIX;
        strcpy(address.sun_path, config->endpoint);
        ipc->fd = socket(AF_UNIX, SOCK_STREAM, 0);
        if (ipc->fd == -1)
            return -1;
        if (connect(ipc->fd, (struct sockaddr *)&address, sizeof(address)) == -1)
            return -1;
    }
#ifdef SO_NOSIGPIPE
    {
        int enabled = 1;
        if (setsockopt(ipc->fd, SOL_SOCKET, SO_NOSIGPIPE,
                       &enabled, sizeof(enabled)) == -1)
            return -1;
    }
#endif
    return 0;
}

#ifdef __linux__
static int queue_init(oalIPC *ipc, const oalIPC_config *config)
{
    struct mq_attr attributes = {0};
    struct mq_attr actual;
    mqd_t c2s, s2c;
    int server = config->role == OAL_IPC_SERVER;
    int flags = O_RDWR | (server ? O_CREAT | O_EXCL : 0);
    size_t length = strlen(config->endpoint);

    if (config->endpoint[0] != '/' || length < 2 ||
        strchr(config->endpoint + 1, '/') != NULL) {
        errno = EINVAL;
        return -1;
    }
    if (length + sizeof("_c2s") > sizeof(ipc->c2s)) {
        errno = ENAMETOOLONG;
        return -1;
    }
    snprintf(ipc->c2s, sizeof(ipc->c2s), "%s_c2s", config->endpoint);
    snprintf(ipc->s2c, sizeof(ipc->s2c), "%s_s2c", config->endpoint);
    attributes.mq_maxmsg = 10;
    attributes.mq_msgsize = OAL_IPC_MAX_SIZE;

    c2s = mq_open(ipc->c2s, flags, 0600, &attributes);
    if (c2s == (mqd_t)-1)
        return -1;
    ipc->owns_c2s = server;
    if (server)
        ipc->rx = c2s;
    else
        ipc->tx = c2s;

    s2c = mq_open(ipc->s2c, flags, 0600, &attributes);
    if (s2c == (mqd_t)-1)
        return -1;
    ipc->owns_s2c = server;
    if (server)
        ipc->tx = s2c;
    else
        ipc->rx = s2c;

    if (mq_getattr(c2s, &actual) == -1)
        return -1;
    if (actual.mq_msgsize != OAL_IPC_MAX_SIZE) {
        errno = EMSGSIZE;
        return -1;
    }
    if (mq_getattr(s2c, &actual) == -1)
        return -1;
    if (actual.mq_msgsize != OAL_IPC_MAX_SIZE) {
        errno = EMSGSIZE;
        return -1;
    }
    return 0;
}
#endif

int oalIPC_init(oalIPC **ipc, const oalIPC_config *config)
{
    oalIPC *instance;
    int result;
    int saved_errno;
    if (ipc == NULL || *ipc != NULL || config == NULL ||
        config->endpoint == NULL || config->endpoint[0] == '\0' ||
        (config->role != OAL_IPC_SERVER && config->role != OAL_IPC_CLIENT) ||
        (config->transport != OAL_IPC_SOCKET &&
         config->transport != OAL_IPC_MESSAGE_QUEUE)) {
        errno = EINVAL;
        return -1;
    }
    instance = calloc(1, sizeof(*instance));
    if (instance == NULL)
        return -1;
    instance->transport = config->transport;
    instance->fd = -1;
    instance->server.fd = -1;
#ifdef __linux__
    instance->rx = instance->tx = (mqd_t)-1;
#endif
    if (config->transport == OAL_IPC_SOCKET) {
        result = socket_init(instance, config);
    } else {
#ifdef __linux__
        result = queue_init(instance, config);
#else
        errno = ENOTSUP;
        result = -1;
#endif
    }
    if (result == -1) {
        saved_errno = errno;
        oalIPC_close(&instance);
        errno = saved_errno;
        return -1;
    }
    *ipc = instance;
    return 0;
}

ssize_t oalIPC_receive(oalIPC *ipc, void *buffer, size_t capacity)
{
    ssize_t count;
    if (ipc == NULL || buffer == NULL || capacity < OAL_IPC_MAX_SIZE) {
        errno = EINVAL;
        return -1;
    }
    if (ipc->read_closed)
        return 0;
    do {
        if (ipc->transport == OAL_IPC_SOCKET) {
            count = socket_posix_receive(ipc->fd, buffer, OAL_IPC_MAX_SIZE);
        } else {
#ifdef __linux__
            count = mq_receive(ipc->rx, buffer, OAL_IPC_MAX_SIZE, NULL);
#else
            errno = ENOTSUP;
            return -1;
#endif
        }
    } while (count == -1 && errno == EINTR);
    if (count == 0)
        ipc->read_closed = 1;
    return count;
}

static int send_bytes(oalIPC *ipc, const void *buffer, size_t length)
{
    if (ipc->transport == OAL_IPC_SOCKET)
        return socket_posix_send(ipc->fd, buffer, length);
#ifdef __linux__
    int result;
    do {
        result = mq_send(ipc->tx, buffer, length, 0);
    } while (result == -1 && errno == EINTR);
    return result;
#else
    errno = ENOTSUP;
    return -1;
#endif
}

int oalIPC_send(oalIPC *ipc, const void *buffer, size_t length)
{
    if (ipc == NULL || buffer == NULL || length == 0) {
        errno = EINVAL;
        return -1;
    }
    if (length > OAL_IPC_MAX_SIZE) {
        errno = EMSGSIZE;
        return -1;
    }
    if (ipc->write_closed) {
        errno = EPIPE;
        return -1;
    }
    return send_bytes(ipc, buffer, length);
}

int oalIPC_shutdown(oalIPC *ipc)
{
    int result;
    if (ipc == NULL) {
        errno = EINVAL;
        return -1;
    }
    if (ipc->write_closed)
        return 0;
    result = ipc->transport == OAL_IPC_SOCKET
        ? shutdown(ipc->fd, SHUT_WR) : send_bytes(ipc, "", 0);
    if (result == 0)
        ipc->write_closed = 1;
    return result;
}

void oalIPC_close(oalIPC **ipc)
{
    oalIPC *instance;
    if (ipc == NULL || *ipc == NULL)
        return;
    instance = *ipc;
    if (instance->fd != -1)
        close(instance->fd);
    socket_posix_close(&instance->server);
#ifdef __linux__
    if (instance->rx != (mqd_t)-1)
        mq_close(instance->rx);
    if (instance->tx != (mqd_t)-1)
        mq_close(instance->tx);
    if (instance->owns_c2s)
        mq_unlink(instance->c2s);
    if (instance->owns_s2c)
        mq_unlink(instance->s2c);
#endif
    free(instance);
    *ipc = NULL;
}
