# Ext / oalIPC

Exchanges and errors are appended to `ext.log` in the working directory.
Lines are flushed immediately. Existing logs are preserved across launches.

`main.c` uses only `oalIPC.h` for communication. The opaque handle hides the
transport resources. Ext initializes a Unix socket server:

```sh
make
make test
./ext /tmp/lxc_demo_ext.sock
```

Without arguments, the socket path is `/tmp/lxc_demo_ext.sock`. Transport is
set to `OAL_IPC_SOCKET` in the initialization configuration in `main.c`.
The oalIPC abstraction still supports POSIX message queues on Linux (including
the Pi); macOS returns `ENOTSUP` for that transport. The Linux build links
with `-lrt`.

## API

- `oalIPC_init`: initialize a server or client with a transport and endpoint.
- `oalIPC_send`: send 1 to `OAL_IPC_MAX_SIZE` (4096) bytes, handling partial writes.
- `oalIPC_receive`: receive a chunk into a buffer of at least 4096 bytes;
  returns 0 on incoming EOF. Message boundaries are not part of the common API.
- `oalIPC_shutdown`: signal outgoing EOF while still allowing reception.
- `oalIPC_close`: release the handle and remove endpoints owned by the server.

All operations block. Failures return -1 with `errno` set. Start the server first;
socket server initialization waits for its single client. The client uses the
same endpoint and transport, with `OAL_IPC_CLIENT` as its role:

```c
oalIPC *ipc = NULL;
oalIPC_config config = {
    OAL_IPC_MESSAGE_QUEUE, OAL_IPC_CLIENT, "/lxc_demo_ext"
};
if (oalIPC_init(&ipc, &config) == -1) {
    /* Handle errno. */
}
```

After a client connects, Ext sends exactly `COUCOU` (6 bytes), waits for exactly
`SALUT` (5 bytes), validates the reply, then repeats. Neither string includes
a NUL terminator or newline on the wire. Fragmented socket reads are accumulated
before starting the next exchange. An incorrect, oversized, or truncated reply
ends the program with a failure. A disconnect before any reply bytes ends the
program normally. There is no response timeout.

Socket
clients signal EOF with `shutdown(fd, SHUT_WR)`; oalIPC clients use
`oalIPC_shutdown` with either transport. Queue transport uses two queues,
`<endpoint>_c2s` and `<endpoint>_s2c`, with 10 slots of 4096 bytes each. Empty
messages represent EOF, so application payloads must be nonempty. An abrupt
queue peer exit is not detected: use the EOF handshake for orderly termination.
Use only one client per endpoint. A queue client may retry initialization if
the server has not finished creating both queues.

Existing endpoints are never overwritten. Normal cleanup and handled I/O errors
remove server endpoints; after an abrupt server exit, remove stale endpoints
before restarting. Socket-specific functions in `socket_posix.*` are internal
transport helpers; `main.c` has no dependency on them.

The Makefile builds for the host. macOS binaries are for local tests; deployment
to `/home/pi/Test_LXC` requires a Linux ARM64 build. Only executables are to be
uploaded to the Pi.
