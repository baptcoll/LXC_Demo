"""Integration tests for the COUCOU/SALUT socket protocol."""
import contextlib
from pathlib import Path
import socket
import subprocess
import tempfile
import time
import unittest

BINARY = Path(__file__).resolve().parents[1] / "ext"


@contextlib.contextmanager
def server():
    with tempfile.TemporaryDirectory(prefix="ext-") as directory:
        endpoint = str(Path(directory) / "s")
        (Path(directory) / "ext.log").write_text("previous run\n")
        process = subprocess.Popen(
            [str(BINARY), endpoint], stdout=subprocess.DEVNULL,
            stderr=subprocess.PIPE, text=True, cwd=directory,
        )
        client = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        client.settimeout(3)
        try:
            deadline = time.monotonic() + 3
            while True:
                try:
                    client.connect(endpoint)
                    break
                except (FileNotFoundError, ConnectionRefusedError):
                    if process.poll() is not None or time.monotonic() >= deadline:
                        raise AssertionError("Server failed to start")
                    time.sleep(0.01)
            yield process, client, endpoint
        finally:
            client.close()
            if process.poll() is None:
                process.kill()
            process.communicate(timeout=3)


def receive_exact(client, size):
    result = bytearray()
    while len(result) < size:
        data = client.recv(size - len(result))
        if not data:
            raise AssertionError("Unexpected EOF")
        result.extend(data)
    return bytes(result)


class ProtocolTests(unittest.TestCase):
    def test_repeats_only_after_complete_salut(self):
        with server() as (process, client, endpoint):
            for _ in range(3):
                self.assertEqual(receive_exact(client, 6), b"COUCOU")
                client.sendall(b"SA")
                client.settimeout(0.05)
                with self.assertRaises(socket.timeout):
                    client.recv(1)
                client.settimeout(3)
                client.sendall(b"LUT")
            self.assertEqual(receive_exact(client, 6), b"COUCOU")
            client.shutdown(socket.SHUT_WR)
            self.assertEqual(client.recv(1), b"")
            self.assertEqual(process.wait(timeout=3), 0)
            log = (Path(endpoint).parent / "ext.log").read_text()
            self.assertTrue(log.startswith("previous run\n"))
            self.assertEqual(log.count("[Ext] TX: COUCOU"), 4)
            self.assertEqual(log.count("[Ext] RX: SALUT"), 3)
            self.assertFalse(Path(endpoint).exists())

    def test_invalid_and_oversized_responses(self):
        for response in (b"HELLO", b"SALUTextra"):
            with self.subTest(response=response), server() as (process, client, endpoint):
                self.assertEqual(receive_exact(client, 6), b"COUCOU")
                client.sendall(response)
                self.assertNotEqual(process.wait(timeout=3), 0)
                self.assertIn("Invalid response", (Path(endpoint).parent / "ext.log").read_text())
                self.assertFalse(Path(endpoint).exists())

    def test_truncated_response(self):
        with server() as (process, client, endpoint):
            self.assertEqual(receive_exact(client, 6), b"COUCOU")
            client.sendall(b"SAL")
            client.shutdown(socket.SHUT_WR)
            self.assertNotEqual(process.wait(timeout=3), 0)
            self.assertIn("Incomplete response", (Path(endpoint).parent / "ext.log").read_text())
            self.assertFalse(Path(endpoint).exists())


if __name__ == "__main__":
    unittest.main()
