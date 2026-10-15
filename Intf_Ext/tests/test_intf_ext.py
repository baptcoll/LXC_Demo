"""Validate Intf_Ext against a controlled Unix socket peer."""
import contextlib
from pathlib import Path
import socket
import subprocess
import tempfile
import unittest

BINARY = Path(__file__).resolve().parents[1] / "intf_ext"


@contextlib.contextmanager
def client_process():
    with tempfile.TemporaryDirectory(prefix="intf-") as directory:
        endpoint = str(Path(directory) / "s")
        log_path = Path(directory) / "intf_ext.log"
        log_path.write_text("previous run\n")
        with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as listener:
            listener.settimeout(3)
            listener.bind(endpoint)
            listener.listen(1)
            process = subprocess.Popen(
                [str(BINARY), endpoint], stdout=subprocess.DEVNULL,
                stderr=subprocess.PIPE, text=True, cwd=directory,
            )
            try:
                peer, _ = listener.accept()
                with peer:
                    peer.settimeout(3)
                    yield process, peer, log_path
            finally:
                if process.poll() is None:
                    process.kill()
                process.communicate(timeout=3)


def receive_exact(peer, size):
    result = bytearray()
    while len(result) < size:
        chunk = peer.recv(size - len(result))
        if not chunk:
            raise AssertionError("Unexpected EOF")
        result.extend(chunk)
    return bytes(result)


class ProtocolTests(unittest.TestCase):
    def test_waits_then_replies_and_repeats(self):
        with client_process() as (process, peer, log_path):
            for _ in range(3):
                peer.settimeout(0.05)
                with self.assertRaises(socket.timeout):
                    peer.recv(1)
                peer.settimeout(3)
                peer.sendall(b"COU")
                peer.settimeout(0.05)
                with self.assertRaises(socket.timeout):
                    peer.recv(1)
                peer.settimeout(3)
                peer.sendall(b"COU")
                self.assertEqual(receive_exact(peer, 5), b"SALUT")
            peer.shutdown(socket.SHUT_WR)
            self.assertEqual(peer.recv(1), b"")
            self.assertEqual(process.wait(timeout=3), 0)

            log = log_path.read_text()
            self.assertTrue(log.startswith("previous run\n"))
            self.assertEqual(log.count("[Intf_Ext] RX: COUCOU"), 3)
            self.assertEqual(log.count("[Intf_Ext] TX: SALUT"), 3)

    def test_rejects_invalid_or_oversized_request(self):
        for request in (b"BONJOU", b"COUCOUextra"):
            with self.subTest(request=request), client_process() as (process, peer, log_path):
                peer.sendall(request)
                self.assertNotEqual(process.wait(timeout=3), 0)
                self.assertIn("Invalid request", log_path.read_text())
                self.assertEqual(peer.recv(1), b"")

    def test_rejects_truncated_request(self):
        with client_process() as (process, peer, log_path):
            peer.sendall(b"COU")
            peer.shutdown(socket.SHUT_WR)
            self.assertNotEqual(process.wait(timeout=3), 0)
            self.assertIn("Incomplete request", log_path.read_text())
            self.assertEqual(peer.recv(1), b"")

    def test_disconnect_without_request(self):
        with client_process() as (process, peer, log_path):
            peer.shutdown(socket.SHUT_WR)
            self.assertEqual(peer.recv(1), b"")
            self.assertEqual(process.wait(timeout=3), 0)


if __name__ == "__main__":
    unittest.main()
