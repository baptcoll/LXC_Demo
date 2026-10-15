#define _POSIX_C_SOURCE 200809L

#include "message_queue.h"

#include <errno.h>
#include <fcntl.h>
#include <mqueue.h>
#include <stdlib.h>

struct message_queue {
    mqd_t outgoing;
    mqd_t incoming;
    int owns_request;
    int owns_response;
};

int message_queue_init(message_queue_t **queue)
{
    struct mq_attr attributes = {0};
    message_queue_t *instance;
    int saved_errno;

    if (queue == NULL || *queue != NULL) {
        errno = EINVAL;
        return -1;
    }
    instance = calloc(1, sizeof(*instance));
    if (instance == NULL)
        return -1;
    instance->outgoing = (mqd_t)-1;
    instance->incoming = (mqd_t)-1;
    attributes.mq_maxmsg = 10;
    attributes.mq_msgsize = MESSAGE_QUEUE_MAX_SIZE;

    instance->outgoing = mq_open(MESSAGE_QUEUE_REQUEST_NAME,
                               O_CREAT | O_EXCL | O_WRONLY, 0600, &attributes);
    if (instance->outgoing == (mqd_t)-1)
        goto fail;
    instance->owns_request = 1;
    instance->incoming = mq_open(MESSAGE_QUEUE_RESPONSE_NAME,
                                O_CREAT | O_EXCL | O_RDONLY, 0600, &attributes);
    if (instance->incoming == (mqd_t)-1)
        goto fail;
    instance->owns_response = 1;

    *queue = instance;
    return 0;

fail:
    saved_errno = errno;
    message_queue_close(&instance);
    errno = saved_errno;
    return -1;
}

int message_queue_connect(message_queue_t **queue)
{
    message_queue_t *instance;
    int saved_errno;

    if (queue == NULL || *queue != NULL) {
        errno = EINVAL;
        return -1;
    }
    instance = calloc(1, sizeof(*instance));
    if (instance == NULL)
        return -1;
    instance->outgoing = (mqd_t)-1;
    instance->incoming = mq_open(MESSAGE_QUEUE_REQUEST_NAME, O_RDONLY);
    if (instance->incoming == (mqd_t)-1)
        goto fail;
    instance->outgoing = mq_open(MESSAGE_QUEUE_RESPONSE_NAME, O_WRONLY);
    if (instance->outgoing == (mqd_t)-1)
        goto fail;

    *queue = instance;
    return 0;

fail:
    saved_errno = errno;
    message_queue_close(&instance);
    errno = saved_errno;
    return -1;
}

int message_queue_send(message_queue_t *queue, const void *message, size_t size)
{
    int result;
    if (queue == NULL || message == NULL || size == 0) {
        errno = EINVAL;
        return -1;
    }
    if (size > MESSAGE_QUEUE_MAX_SIZE) {
        errno = EMSGSIZE;
        return -1;
    }
    do {
        result = mq_send(queue->outgoing, message, size, 0);
    } while (result == -1 && errno == EINTR);
    return result;
}

ssize_t message_queue_receive(message_queue_t *queue, void *message, size_t capacity)
{
    ssize_t size;
    if (queue == NULL || message == NULL || capacity < MESSAGE_QUEUE_MAX_SIZE) {
        errno = EINVAL;
        return -1;
    }
    do {
        size = mq_receive(queue->incoming, message, capacity, NULL);
    } while (size == -1 && errno == EINTR);
    if (size == 0) {
        errno = EPROTO;
        return -1;
    }
    return size;
}

void message_queue_close(message_queue_t **queue)
{
    message_queue_t *instance;
    if (queue == NULL || *queue == NULL)
        return;
    instance = *queue;
    if (instance->outgoing != (mqd_t)-1)
        mq_close(instance->outgoing);
    if (instance->incoming != (mqd_t)-1)
        mq_close(instance->incoming);
    if (instance->owns_request)
        mq_unlink(MESSAGE_QUEUE_REQUEST_NAME);
    if (instance->owns_response)
        mq_unlink(MESSAGE_QUEUE_RESPONSE_NAME);
    free(instance);
    *queue = NULL;
}
