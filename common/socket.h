#ifndef COMMON_UNIX_SOCKET_H
#define COMMON_UNIX_SOCKET_H

#include <stddef.h>

typedef struct unix_socket unix_socket_t;

typedef enum {
    UNIX_SOCKET_SERVER,
    UNIX_SOCKET_CLIENT
} unix_socket_role_t;

/* Open one blocking, bidirectional Unix stream connection.
 * *connection must initially be NULL. Start the server first: server open
 * creates the endpoint and waits for a single client to connect.
 * Returns 0 on success, -1 with errno set on error. */
int unix_socket_open(unix_socket_t **connection, unix_socket_role_t role,
                     const char *path);

/* Send all size bytes. Returns 0 on success, -1 with errno on error.
 * Suppresses SIGPIPE so a closed peer produces an error instead of a signal. */
int unix_socket_send(unix_socket_t *connection, const void *buffer, size_t size);

/* Read exactly size bytes. Returns 1 on success, 0 for EOF before any bytes,
 * or -1 on error. EOF after a partial read sets errno to EPROTO.
 * Both send and receive require a non-NULL buffer and a nonzero size.
 * The caller defines framing; no message header is added by this layer.
 * After a transfer error, close the connection rather than retrying it. */
int unix_socket_receive(unix_socket_t *connection, void *buffer, size_t size);

/* Close descriptors and remove the endpoint if created by this connection.
 * Frees the handle and sets it to NULL. Safe to call more than once. */
void unix_socket_close(unix_socket_t **connection);

#endif
