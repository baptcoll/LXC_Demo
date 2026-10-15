#ifndef OAL_IPC_H
#define OAL_IPC_H

#include <stddef.h>
#include <sys/types.h>

#define OALIPC_MAX_MESSAGE_SIZE 4096U

/* Opaque handle: the application never accesses transport descriptors. */
typedef struct oalIPC oalIPC_t;

typedef enum {
    OALIPC_SERVER,
    OALIPC_CLIENT
} oalIPC_role_t;

typedef struct {
    oalIPC_role_t role;
    /* Shared address of the two processes. For the Unix socket backend,
     * this is a filesystem path, e.g. /tmp/lxc_demo.sock. */
    const char *endpoint;
} oalIPC_config_t;

/* Initialize one bidirectional connection. Start the server first.
 * Server initialization blocks until its single client connects.
 * *ipc must initially be NULL. Returns 0 on success, -1 with errno on error.
 * The current backend uses a POSIX Unix stream socket; transport details
 * remain confined to socket.c, with message framing in oalIPC.c. */
int oalIPC_init(oalIPC_t **ipc, const oalIPC_config_t *config);

/* Send one complete message of 1..OALIPC_MAX_MESSAGE_SIZE bytes.
 * Returns 0 on success, -1 with errno on error. */
int oalIPC_send(oalIPC_t *ipc, const void *message, size_t size);

/* Receive one complete message, preserving send boundaries even when the
 * socket fragments data. Returns its size, 0 for a peer disconnect between
 * messages, or -1 with errno on error (including an insufficient capacity).
 * A buffer of OALIPC_MAX_MESSAGE_SIZE bytes can receive any valid message.
 * Data is binary: no NUL terminator is appended. */
ssize_t oalIPC_receive(oalIPC_t *ipc, void *message, size_t capacity);

/* All operations block. After a send/receive error, close the connection.
 * Both processes must use this API: the socket wire format is a 32-bit
 * unsigned length in network byte order followed by the message bytes.
 * Use one reader and one writer per handle; concurrent sends need external
 * synchronization. Closing signals EOF to the peer. */

/* Close the connection and free the handle. The server removes only the
 * socket path it created. Safe with NULL or an already closed handle. */
void oalIPC_close(oalIPC_t **ipc);

#endif
