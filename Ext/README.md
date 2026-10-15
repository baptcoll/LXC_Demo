# Ext

POSIX Unix-domain stream socket server for communication between local processes.

```sh
make
./ext /tmp/lxc_demo_ext.sock
```

The socket path argument is optional and defaults to `/tmp/lxc_demo_ext.sock`.
The initial program accepts one client, echoes its bytes, and exits after the
client disconnects. It closes the descriptors and removes the socket path on
normal exit or a handled I/O error. An existing path is never overwritten.

`socket_posix.h` exposes initialization, accept, receive, full-buffer send, and
server cleanup. The caller closes accepted client descriptors and ignores
`SIGPIPE`, as shown in `main.c`. Reads may be partial; message framing is left
to the application protocol.

The Makefile uses the host compiler. A macOS build is for local validation;
deployment to the Raspberry Pi requires a Linux ARM64 build.
