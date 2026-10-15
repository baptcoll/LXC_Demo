#define _POSIX_C_SOURCE 200809L
#include "oalIPC.h"

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static void run_client(oalIPC_transport transport, const char *endpoint)
{
    oalIPC *ipc = NULL;
    oalIPC_config config = { transport, OAL_IPC_CLIENT, endpoint };
    unsigned char payload[OAL_IPC_MAX_SIZE];
    unsigned char reply[OAL_IPC_MAX_SIZE];
    struct timespec delay = { 0, 10000000 };
    int attempt;
    for (attempt = 0; attempt < 500; ++attempt) {
        if (oalIPC_init(&ipc, &config) == 0)
            break;
        assert(errno == ENOENT || errno == ECONNREFUSED);
        nanosleep(&delay, NULL);
    }
    assert(ipc != NULL);
    assert(oalIPC_send(ipc, payload, 0) == -1 && errno == EINVAL);
    assert(oalIPC_send(ipc, payload, sizeof(payload) + 1) == -1 && errno == EMSGSIZE);
    assert(oalIPC_receive(ipc, reply, sizeof(reply) - 1) == -1 && errno == EINVAL);
    for (size_t turn = 0; turn < 64; ++turn) {
        size_t length = turn == 0 ? 1 : sizeof(payload);
        size_t received = 0;
        for (size_t index = 0; index < length; ++index)
            payload[index] = (unsigned char)(index + turn);
        assert(oalIPC_send(ipc, payload, length) == 0);
        while (received < length) {
            ssize_t count = oalIPC_receive(ipc, reply, sizeof(reply));
            assert(count > 0 && (size_t)count <= length - received);
            assert(memcmp(reply, payload + received, (size_t)count) == 0);
            received += (size_t)count;
        }
    }
    assert(oalIPC_shutdown(ipc) == 0);
    assert(oalIPC_shutdown(ipc) == 0);
    assert(oalIPC_send(ipc, payload, 1) == -1 && errno == EPIPE);
    assert(oalIPC_receive(ipc, reply, sizeof(reply)) == 0);
    assert(oalIPC_receive(ipc, reply, sizeof(reply)) == 0);
    oalIPC_close(&ipc);
    assert(ipc == NULL);
    oalIPC_close(&ipc);
}

static void test_transport(oalIPC_transport transport)
{
    char endpoint[96];
    oalIPC *ipc = NULL;
    oalIPC *duplicate = NULL;
    unsigned char buffer[OAL_IPC_MAX_SIZE];
    oalIPC_config config = { transport, OAL_IPC_SERVER, endpoint };
    int status;
    snprintf(endpoint, sizeof(endpoint), transport == OAL_IPC_SOCKET
             ? "/tmp/oalipc_%ld.sock" : "/oalipc_%ld", (long)getpid());
    pid_t child = fork();
    assert(child != -1);
    if (child == 0) {
        run_client(transport, endpoint);
        _exit(0);
    }
    assert(oalIPC_init(&ipc, &config) == 0);
    assert(oalIPC_init(&duplicate, &config) == -1);
    assert(duplicate == NULL);
    for (;;) {
        ssize_t count = oalIPC_receive(ipc, buffer, sizeof(buffer));
        assert(count >= 0);
        if (count == 0)
            break;
        assert(oalIPC_send(ipc, buffer, (size_t)count) == 0);
    }
    assert(oalIPC_shutdown(ipc) == 0);
    assert(waitpid(child, &status, 0) == child);
    assert(WIFEXITED(status) && WEXITSTATUS(status) == 0);
    oalIPC_close(&ipc);
    oalIPC_close(&ipc);
    if (transport == OAL_IPC_SOCKET)
        assert(access(endpoint, F_OK) == -1 && errno == ENOENT);
    else {
        /* Recreating the server confirms both owned queue names were removed. */
        assert(oalIPC_init(&ipc, &config) == 0);
        oalIPC_close(&ipc);
    }
}

int main(void)
{
    oalIPC *ipc = NULL;
    oalIPC_config config = { OAL_IPC_SOCKET, OAL_IPC_SERVER, "" };
    alarm(20);
    assert(oalIPC_init(&ipc, &config) == -1 && errno == EINVAL);
    test_transport(OAL_IPC_SOCKET);
    puts("PASS: socket, client/server, binary echo, validation, EOF, cleanup");
#ifdef __linux__
    test_transport(OAL_IPC_MESSAGE_QUEUE);
    puts("PASS: POSIX message queue, client/server, binary echo, validation, EOF, cleanup");
#else
    config.transport = OAL_IPC_MESSAGE_QUEUE;
    config.endpoint = "/oalipc_test";
    assert(oalIPC_init(&ipc, &config) == -1 && errno == ENOTSUP);
    puts("PASS: message queues return ENOTSUP on this host (Linux test still required)");
#endif
    return EXIT_SUCCESS;
}
