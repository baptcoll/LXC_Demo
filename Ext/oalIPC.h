#ifndef OAL_IPC_H
#define OAL_IPC_H

#include <stddef.h>
#include <sys/types.h>

#define OAL_IPC_MAX_SIZE 4096

typedef struct oalIPC oalIPC;

typedef enum {
    OAL_IPC_SOCKET,
    OAL_IPC_MESSAGE_QUEUE
} oalIPC_transport;

typedef enum {
    OAL_IPC_SERVER,
    OAL_IPC_CLIENT
} oalIPC_role;

typedef struct {
    oalIPC_transport transport;
    oalIPC_role role;
    /* Socket path, or POSIX queue base name (e.g. /lxc_demo_ext).
     * Queue names are derived as <endpoint>_c2s and <endpoint>_s2c. */
    const char *endpoint;
} oalIPC_config;

/* Blocking, one server and one client. Start the server first.
 * Socket server init waits for a client; queue server init creates queues.
 * Existing endpoints are never overwritten. *ipc must initially be NULL.
 * Functions return -1 on failure with errno set; init/send return 0 on success.
 * POSIX message queues require Linux; unsupported hosts return ENOTSUP. */
int oalIPC_init(oalIPC **ipc, const oalIPC_config *config);

/* Receive a chunk of bytes, with no portable message-boundary guarantee.
 * capacity must be >= OAL_IPC_MAX_SIZE. Returns byte count, 0 for peer EOF,
 * or -1 on error. Send accepts 1..OAL_IPC_MAX_SIZE bytes. */
ssize_t oalIPC_receive(oalIPC *ipc, void *buffer, size_t capacity);
int oalIPC_send(oalIPC *ipc, const void *buffer, size_t length);

/* Signal outgoing EOF after all sends, while still allowing receives.
 * For queues this is an empty message; abrupt peer termination has no EOF
 * notification. Queue operations block when empty/full. */
int oalIPC_shutdown(oalIPC *ipc);

/* Close resources; server also removes endpoints it created. Idempotent.
 * close does not send EOF: use shutdown when the peer must be notified. */
void oalIPC_close(oalIPC **ipc);

#endif
