#ifndef MESSAGE_QUEUE_H
#define MESSAGE_QUEUE_H

#include <stddef.h>
#include <sys/types.h>

#define MESSAGE_QUEUE_MAX_SIZE 4096U
#define MESSAGE_QUEUE_REQUEST_NAME "/lxc_demo_request"
#define MESSAGE_QUEUE_RESPONSE_NAME "/lxc_demo_response"

typedef struct message_queue message_queue_t;

/* Intf_Ext owns the two POSIX queues. Initialize *queue to NULL.
 * The other process reads REQUEST and writes RESPONSE, opening them after
 * this initialization. Existing queues are never overwritten or consumed.
 * Returns 0 on success or -1 with errno set on error. */
int message_queue_init(message_queue_t **queue);

/* Open the existing queues as the other process: receive REQUEST and send
 * RESPONSE. Start Intf_Ext first. This handle never unlinks queue names.
 * Initialize *queue to NULL. Returns 0 on success, -1 with errno on error. */
int message_queue_connect(message_queue_t **queue);

/* Send one nonempty message (up to MESSAGE_QUEUE_MAX_SIZE bytes): a request
 * for the creator handle, a response for the connected peer handle.
 * Returns 0 on success or -1 with errno set on error. */
int message_queue_send(message_queue_t *queue, const void *message, size_t size);

/* Receive a response (creator) or request (peer).
 * capacity must be >= MESSAGE_QUEUE_MAX_SIZE.
 * Returns its size or -1 with errno set on error. Operations block; a peer
 * exit is not reported as EOF by POSIX queues. */
ssize_t message_queue_receive(message_queue_t *queue, void *message, size_t capacity);

/* Close descriptors, unlink owned queues, free the handle and set it to NULL.
 * After abrupt termination, stale queue names must be cleaned up separately. */
void message_queue_close(message_queue_t **queue);

#endif
