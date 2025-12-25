#!/usr/bin/env pytest

import queue
import socket
import ssl
import struct
import subprocess
import threading
import time
from contextlib import closing
from pathlib import Path
import shutil
import tempfile

from ornithology import action, standup, Condor

FORWARD_ID = "http_api_forward"
SOCKET_DIR_NAME = "shared_port_sockets"


class DomainSocketHTTPServer:
    def __init__(self, socket_path: Path, ssl_context: ssl.SSLContext):
        self.socket_path = Path(socket_path)
        self.ssl_context = ssl_context
        self._listener = None
        self._thread = None
        self._stop = threading.Event()
        self._ready = threading.Event()
        self.seen_requests = queue.Queue()

    def _log(self, msg: str):
        print(f"[domain-socket-server] {msg}", flush=True)

    def start(self):
        if self.socket_path.exists():
            self.socket_path.unlink()

        self.socket_path.parent.mkdir(parents=True, exist_ok=True)

        self._listener = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        self._listener.bind(str(self.socket_path))
        self._listener.listen(5)
        self._listener.settimeout(0.5)

        self._thread = threading.Thread(target=self._serve, daemon=True)
        self._thread.start()
        self._ready.wait(timeout=5)

    def stop(self):
        self._stop.set()
        if self._listener:
            try:
                self._listener.shutdown(socket.SHUT_RDWR)
            except OSError:
                pass
            self._listener.close()
        if self._thread:
            self._thread.join(timeout=5)
        if self.socket_path.exists():
            try:
                self.socket_path.unlink()
            except OSError:
                pass

    def _serve(self):
        self._ready.set()
        while not self._stop.is_set():
            try:
                conn, _ = self._listener.accept()
            except socket.timeout:
                continue
            except OSError:
                break

            try:
                self._handle_domain_connection(conn)
            except Exception:
                self._log("exception while handling domain connection")
                conn.close()
                continue

    def _handle_domain_connection(self, conn: socket.socket):
        deadline = time.monotonic() + 15

        while True:
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                self._log("timeout waiting for fd arrival")
                conn.close()
                return

            conn.settimeout(remaining)
            try:
                msg, ancdata, _flags, _addr = conn.recvmsg(1024, socket.CMSG_SPACE(struct.calcsize("i")))
            except (socket.timeout, BlockingIOError, OSError):
                self._log("timeout or recvmsg error before fd arrival")
                conn.close()
                return

            if msg == b"":
                self._log("domain socket closed before fd arrival")
                conn.close()
                return

            if not ancdata:
                continue
            break

        fd = struct.unpack("i", ancdata[0][2])[0]
        client_sock = socket.socket(fileno=fd)
        client_sock.settimeout(5)

        try:
            first = client_sock.recv(1, socket.MSG_PEEK)
        except (socket.timeout, OSError):
            self._log("peek timeout/err on forwarded socket")
            client_sock.close()
            conn.close()
            return

        if first.startswith(b"\x16"):
            handler = self._handle_tls_client
            self._log("handling TLS client")
        else:
            handler = self._handle_http_client
            self._log("handling HTTP client")

        handler(client_sock)
        conn.close()

    def _http_response(self, body: bytes) -> bytes:
        return b"HTTP/1.1 200 OK\r\nContent-Length: %d\r\nConnection: close\r\n\r\n%s" % (
            len(body),
            body,
        )

    def _handle_http_client(self, client_sock: socket.socket):
        try:
            data = client_sock.recv(4096)
            self.seen_requests.put(("http", data))
            client_sock.sendall(self._http_response(b"http-ok"))
        finally:
            client_sock.close()

    def _handle_tls_client(self, client_sock: socket.socket):
        try:
            tls_sock = self.ssl_context.wrap_socket(client_sock, server_side=True)
            data = tls_sock.recv(4096)
            self.seen_requests.put(("https", data))
            tls_sock.sendall(self._http_response(b"https-ok"))
            tls_sock.close()
        finally:
            client_sock.close()


def _pick_ephemeral_port() -> int:
    with closing(socket.socket(socket.AF_INET, socket.SOCK_STREAM)) as s:
        s.bind(("127.0.0.1", 0))
        return s.getsockname()[1]


def _create_self_signed_cert(target_dir: Path):
    key_path = target_dir / "server.key"
    cert_path = target_dir / "server.crt"

    subprocess.run(
        [
            "openssl",
            "req",
            "-new",
            "-newkey",
            "rsa:2048",
            "-nodes",
            "-x509",
            "-days",
            "1",
            "-subj",
            "/CN=localhost",
            "-keyout",
            key_path,
            "-out",
            cert_path,
        ],
        check=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )

    return cert_path, key_path


@action
def shared_port_socket_dir():
    path = Path(tempfile.mkdtemp(prefix="htcondor-shared-port-", dir="/tmp"))
    yield path
    shutil.rmtree(path, ignore_errors=True)


@standup
def condor(test_dir, shared_port_socket_dir):
    shared_port_socket_dir.mkdir(parents=True, exist_ok=True)

    shared_port_port = _pick_ephemeral_port()

    config = {
        "DAEMON_LIST": "MASTER SHARED_PORT",
        "SHARED_PORT_PORT": str(shared_port_port),
        "USE_SHARED_PORT": True,
        "DAEMON_SOCKET_DIR": shared_port_socket_dir.as_posix(),
        "COLLECTOR_USES_SHARED_PORT": False,
        "SHARED_PORT_HTTP_FORWARDING_ID": FORWARD_ID,
        "SHARED_PORT_LOG": "$(LOG)/SharedPortLog",
    }

    with Condor(local_dir=test_dir / "condor", config=config) as c:
        yield c


@action
def ssl_context(test_dir):
    cert_path, key_path = _create_self_signed_cert(test_dir)
    context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    context.load_cert_chain(certfile=cert_path, keyfile=key_path)
    return context


@action
def domain_socket_server(shared_port_socket_dir, ssl_context):
    socket_path = shared_port_socket_dir / FORWARD_ID
    server = DomainSocketHTTPServer(socket_path, ssl_context)
    server.start()
    yield server
    server.stop()


@action
def shared_port_port(condor):
    return int(condor.run_command(["condor_config_val", "SHARED_PORT_PORT"], echo=False, suppress=True).stdout)


class TestSharedPortHTTPForwarding:
    def _read_response(self, conn: socket.socket) -> bytes:
        chunks = []
        while True:
            chunk = conn.recv(4096)
            if not chunk:
                break
            chunks.append(chunk)
        return b"".join(chunks)

    def test_http_is_forwarded(self, shared_port_port, domain_socket_server):
        with socket.create_connection(("127.0.0.1", shared_port_port), timeout=5) as conn:
            conn.sendall(b"GET / HTTP/1.1\r\nHost: localhost\r\n\r\n")
            response = self._read_response(conn)

        assert b"HTTP/1.1 200 OK" in response
        assert b"http-ok" in response

    def test_https_is_forwarded(self, shared_port_port, domain_socket_server, ssl_context):
        client_ctx = ssl.create_default_context()
        client_ctx.check_hostname = False
        client_ctx.verify_mode = ssl.CERT_NONE

        with client_ctx.wrap_socket(socket.create_connection(("127.0.0.1", shared_port_port), timeout=5), server_hostname="localhost") as conn:
            conn.sendall(b"GET /secure HTTP/1.1\r\nHost: localhost\r\n\r\n")
            response = self._read_response(conn)

        assert b"HTTP/1.1 200 OK" in response
        assert b"https-ok" in response
