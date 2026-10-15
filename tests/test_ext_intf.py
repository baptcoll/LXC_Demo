#!/usr/bin/env python3
"""Tester trois échanges Ext -> socket -> Intf_Ext -> queues POSIX -> Python."""
from pathlib import Path
import ctypes
import errno
import json
import os
import subprocess
import time
import uuid

REQUEST = b"/lxc_demo_request"
RESPONSE = b"/lxc_demo_response"
MAX_SIZE = 4096


class QueuePeer:
    """Accès aux queues Linux sans dépendance Python externe."""
    def __init__(self):
        self.lib = ctypes.CDLL("librt.so.1", use_errno=True)
        self.lib.mq_open.argtypes = [ctypes.c_char_p, ctypes.c_int]
        self.lib.mq_open.restype = ctypes.c_int
        self.lib.mq_receive.argtypes = [ctypes.c_int, ctypes.c_void_p, ctypes.c_size_t, ctypes.c_void_p]
        self.lib.mq_receive.restype = ctypes.c_ssize_t
        self.lib.mq_send.argtypes = [ctypes.c_int, ctypes.c_char_p, ctypes.c_size_t, ctypes.c_uint]
        self.lib.mq_send.restype = ctypes.c_int
        self.lib.mq_close.argtypes = [ctypes.c_int]
        self.lib.mq_close.restype = ctypes.c_int
        self.lib.mq_unlink.argtypes = [ctypes.c_char_p]
        self.lib.mq_unlink.restype = ctypes.c_int
        self.request = -1
        self.response = -1

    @staticmethod
    def error():
        code = ctypes.get_errno()
        return OSError(code, os.strerror(code))

    def open(self, check_alive):
        deadline = time.monotonic() + 3
        while True:
            check_alive()
            self.request = self.lib.mq_open(REQUEST, os.O_RDONLY | os.O_NONBLOCK)
            if self.request != -1:
                break
            if ctypes.get_errno() != errno.ENOENT:
                raise self.error()
            if time.monotonic() >= deadline:
                raise TimeoutError("Queue de requêtes indisponible")
            time.sleep(0.01)
        while True:
            check_alive()
            self.response = self.lib.mq_open(RESPONSE, os.O_WRONLY | os.O_NONBLOCK)
            if self.response != -1:
                break
            if ctypes.get_errno() != errno.ENOENT:
                raise self.error()
            if time.monotonic() >= deadline:
                raise TimeoutError("Queue de réponses indisponible")
            time.sleep(0.01)

    def receive(self, check_alive):
        buffer = ctypes.create_string_buffer(MAX_SIZE)
        deadline = time.monotonic() + 3
        while True:
            check_alive()
            size = self.lib.mq_receive(self.request, buffer, MAX_SIZE, None)
            if size >= 0:
                return buffer.raw[:size]
            if ctypes.get_errno() not in (errno.EAGAIN, errno.EINTR):
                raise self.error()
            if time.monotonic() >= deadline:
                raise TimeoutError("Requête non reçue sur la queue")
            time.sleep(0.01)

    def reply(self):
        if self.lib.mq_send(self.response, b"POUET", 5, 0) == -1:
            raise self.error()

    def close(self):
        for fd in (self.request, self.response):
            if fd != -1:
                self.lib.mq_close(fd)

    def unlink_owned_names(self):
        for name in (REQUEST, RESPONSE):
            if self.lib.mq_unlink(name) == -1 and ctypes.get_errno() != errno.ENOENT:
                raise self.error()


def main():
    root = Path(__file__).resolve().parents[1]
    endpoint = Path("/tmp/lxc_demo.sock")
    if endpoint.exists() or endpoint.is_socket():
        raise RuntimeError("Socket déjà présent : aucun processus lancé")
    for name in (REQUEST, RESPONSE):
        if (Path("/dev/mqueue") / name.decode().lstrip("/")).exists():
            raise RuntimeError("Queue déjà présente : aucun processus lancé")

    peer = QueuePeer()
    log_dir = root / "logs" / ("test-relay-" + uuid.uuid4().hex[:12])
    log_dir.mkdir(parents=True)
    processes = {}
    ready = False
    error = None
    completed = 0
    started = time.monotonic()

    def check_alive():
        if any(process.poll() is not None for process in processes.values()):
            raise RuntimeError("Un programme s'est arrêté prématurément")

    with (log_dir / "ext.log").open("w") as ext_log, (log_dir / "intf_ext.log").open("w") as intf_log, (log_dir / "queue_peer.log").open("w", buffering=1) as peer_log:
        try:
            processes["ext"] = subprocess.Popen([str(root / "ext")], cwd=root, stdout=ext_log, stderr=subprocess.STDOUT)
            deadline = time.monotonic() + 3
            while not endpoint.is_socket():
                check_alive()
                if time.monotonic() >= deadline:
                    raise TimeoutError("Socket Ext indisponible")
                time.sleep(0.01)
            ready = True
            processes["intf_ext"] = subprocess.Popen([str(root / "intf_ext")], cwd=root, stdout=intf_log, stderr=subprocess.STDOUT)
            peer.open(check_alive)
            for index in range(4):
                request = peer.receive(check_alive)
                print(f"[Test] Queue RX: {request!r}", file=peer_log)
                if request != b"COUCOU":
                    raise ValueError(f"Requête modifiée par le relais : {request!r}")
                if index < 3:
                    peer.reply()
                    print("[Test] Queue TX: POUET", file=peer_log)
            # La quatrième requête prouve qu'Ext a accepté la troisième réponse.
            deadline = time.monotonic() + 3
            while True:
                check_alive()
                ext_text = (log_dir / "ext.log").read_text()
                intf_text = (log_dir / "intf_ext.log").read_text()
                expected = ("Socket RX: COUCOU", "Queue TX: COUCOU", "Queue RX: POUET", "Socket TX: POUET")
                if ext_text.count("[Ext] RX: POUET\n") == 3 and all(intf_text.count("[Intf_Ext] " + item + "\n") >= 3 for item in expected):
                    break
                if time.monotonic() >= deadline:
                    raise TimeoutError("Logs du relais incomplets")
                time.sleep(0.01)
            messages = [line for line in intf_text.splitlines() if any(line.startswith("[Intf_Ext] " + label) for label in ("Socket RX:", "Queue TX:", "Queue RX:", "Socket TX:"))]
            for index, line in enumerate(messages):
                if line != "[Intf_Ext] " + expected[index % 4]:
                    raise ValueError("Ordre du relais incorrect : " + line)
            completed = 3
        except Exception as exc:
            error = str(exc)
        finally:
            for process in processes.values():
                if process.poll() is None:
                    process.terminate()
            for process in processes.values():
                try:
                    process.wait(timeout=2)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait(timeout=2)
            peer.close()
            # O_EXCL et le log de succès établissent que nos processus ont
            # créé ces queues ; ne jamais supprimer celles d'un autre test.
            if "[Intf_Ext] Message queues ready:" in (log_dir / "intf_ext.log").read_text():
                peer.unlink_owned_names()
            if ready and endpoint.exists():
                endpoint.unlink()

    report = {
        "success": error is None, "error": error,
        "elapsed_seconds": round(time.monotonic() - started, 3),
        "completed_exchanges": completed, "log_dir": str(log_dir),
        "queues_removed": all(not (Path("/dev/mqueue") / name.decode().lstrip("/")).exists() for name in (REQUEST, RESPONSE)),
        "processes": {name: {"pid": process.pid, "exit_status": process.returncode, "stopped": process.poll() is not None} for name, process in processes.items()},
    }
    if not report["queues_removed"]:
        report["success"] = False
        report["error"] = error or "Queues non nettoyées"
    print(json.dumps(report, indent=2, ensure_ascii=False))
    return 0 if report["success"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
