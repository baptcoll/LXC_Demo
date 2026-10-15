# Intf_Ext

Exchanges and errors are appended to `intf_ext.log` in the working directory.
Lines are flushed immediately. Existing logs are preserved across launches.

Unix socket client using the same oalIPC implementation as Ext, compiled from
`../Ext/oalIPC.c` and `../Ext/socket_posix.c` without duplicating the abstraction.

```sh
make
make test
./intf_ext /tmp/lxc_demo_ext.sock
```

Start Ext first, then Intf_Ext in another terminal. The socket path is optional
and defaults to `/tmp/lxc_demo_ext.sock` in both programs.

Intf_Ext waits for exactly `COUCOU` (6 bytes), validates it, sends `SALUT`
(5 bytes), then waits for the next request. Neither message includes a NUL
terminator or newline. Fragmented reads are accumulated before replying.
Incorrect or truncated requests cause a failure exit. A disconnect between
requests exits normally. There is no receive timeout or automatic reconnect.

Local builds target the host OS; deployment to the Pi requires Linux ARM64
executables. Only executables are to be uploaded to `/home/pi/Test_LXC`.
