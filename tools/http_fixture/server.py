#!/usr/bin/env python3
# Canned HTTP fixture for the simutrans e2e suite.
#
# Serves the listserver-side routes that the in-house HTTP code in
# `src/simutrans/network/network_file_transfer.cc` hits when simutrans
# runs with `-listserver` / `-ip_query_host` pointed at us:
#
#   POST /announce       200, empty body; request body is logged
#                        so the driver can assert announce parameters
#   GET  /get_IP.php     200, body "127.0.0.1\n"
#   *                    404
#
# Usage:
#   ./server.py [--host 127.0.0.1] [--port 0]
#
# Prints `FIXTURE_LISTENING host port` on stdout once accept() is up,
# then runs until SIGINT/SIGTERM.  Request logs go to stderr with the
# `FIXTURE:` prefix; announce bodies are logged as `FIXTURE: ANNOUNCE
# body=<bytes>` so tests can assert on the payload.

import argparse
import socket
import sys
import threading


def log(msg: str) -> None:
    print("FIXTURE:", msg, file=sys.stderr, flush=True)


def read_request(conn: socket.socket) -> tuple[str, str, dict, bytes]:
    buf = b""
    while b"\r\n\r\n" not in buf:
        chunk = conn.recv(4096)
        if not chunk:
            return ("", "", {}, b"")
        buf += chunk
        if len(buf) > 1 << 20:
            raise ValueError("request too large")
    header_blob, _, rest = buf.partition(b"\r\n\r\n")
    lines = header_blob.split(b"\r\n")
    parts = lines[0].decode("latin-1").split(" ")
    if len(parts) < 3:
        return ("", "", {}, b"")
    method, path = parts[0], parts[1]
    headers: dict[str, str] = {}
    for line in lines[1:]:
        if b":" in line:
            k, _, v = line.decode("latin-1").partition(":")
            headers[k.strip().lower()] = v.strip()
    content_length = int(headers.get("content-length", "0"))
    body = rest
    while len(body) < content_length:
        chunk = conn.recv(min(4096, content_length - len(body)))
        if not chunk:
            break
        body += chunk
    return method, path, headers, body


def send(conn, status, headers, body=b""):
    payload = f"HTTP/1.1 {status}\r\n".encode()
    for k, v in headers.items():
        payload += f"{k}: {v}\r\n".encode()
    payload += b"\r\n" + body
    conn.sendall(payload)


def handle(conn: socket.socket, addr) -> None:
    try:
        method, path, headers, body = read_request(conn)
        if not method:
            return
        log(f"{addr} {method} {path} clen={len(body)}")

        if method == "POST" and path == "/announce":
            log(f"ANNOUNCE body={body!r}")
            send(conn, "200 OK", {"Content-Length": "0"})
        elif method == "GET" and path == "/get_IP.php":
            out = b"127.0.0.1\n"
            send(conn, "200 OK",
                 {"Content-Type": "text/plain",
                  "Content-Length": str(len(out))},
                 out)
        else:
            send(conn, "404 Not Found", {"Content-Length": "0"})
    except Exception as exc:
        log(f"{addr} handler error: {exc!r}")
    finally:
        try:
            conn.shutdown(socket.SHUT_RDWR)
        except OSError:
            pass
        conn.close()


def serve(host: str, port: int) -> None:
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.bind((host, port))
    sock.listen(16)
    actual_host, actual_port = sock.getsockname()
    print(f"FIXTURE_LISTENING {actual_host} {actual_port}", flush=True)
    log(f"listening on {actual_host}:{actual_port}")

    while True:
        try:
            conn, addr = sock.accept()
        except KeyboardInterrupt:
            return
        threading.Thread(target=handle, args=(conn, addr), daemon=True).start()


def main() -> None:
    ap = argparse.ArgumentParser(description="HTTP fixture for simutrans e2e suite")
    ap.add_argument("--host", default="127.0.0.1")
    ap.add_argument("--port", type=int, default=0, help="0 = pick random free port")
    args = ap.parse_args()
    serve(args.host, args.port)


if __name__ == "__main__":
    main()
