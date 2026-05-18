#!/usr/bin/env python3
# Shared rig for black-box tests of the multiplayer wire protocol in
# src/simutrans/network/network_cmd*.cc.
#
# Three concerns live here so the per-command modules don't repeat them:
#
#   - Server: context manager that spawns a headless simutrans server on
#     a chosen port and tears it down deterministically.
#   - recv_exact / read_packets: stream-parsing helpers that match the
#     fixed 6-byte header (`size, version, id`).  Tests assert on parsed
#     packets, not raw bytes, so a benign field reorder still surfaces
#     as the right diagnostic.
#   - run_tests: the tiny CLI runner the per-command modules share.
#
# Per-command wire layouts (build / parse / TESTS) live in their own
# test_<command>.py — see test_auth_player.py for the canonical shape.

import os
import shutil
import signal
import socket
import struct
import subprocess
import sys
import tempfile
import time
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
PAK_DIR = ROOT / "simutrans" / "pak"
BASE_DIR = ROOT / "simutrans"
EMPTY_SAVE = ROOT / "tests" / "empty-16x16.sve"

NETWORK_VERSION = 1
HEADER_SIZE = 6

# nwc_* command IDs, mirroring the network_command_id enum in
# src/simutrans/network/network_cmd.h.  The table doubles as a
# name lookup for diagnostic output (see pkt_name).
NWC_GAMEINFO     = 1
NWC_JOIN         = 4
NWC_SYNC         = 5
NWC_GAME         = 6
NWC_READY        = 7
NWC_TOOL         = 8
NWC_CHECK        = 9
NWC_SERVICE      = 11
NWC_AUTH_PLAYER  = 12
NWC_CHG_PLAYER   = 13
NWC_STEP         = 16

_PACKET_NAMES = {
    NWC_GAMEINFO: "NWC_GAMEINFO",
    NWC_JOIN: "NWC_JOIN",
    NWC_SYNC: "NWC_SYNC",
    NWC_GAME: "NWC_GAME",
    NWC_READY: "NWC_READY",
    NWC_TOOL: "NWC_TOOL",
    NWC_CHECK: "NWC_CHECK",
    NWC_SERVICE: "NWC_SERVICE",
    NWC_AUTH_PLAYER: "NWC_AUTH_PLAYER",
    NWC_CHG_PLAYER: "NWC_CHG_PLAYER",
    NWC_STEP: "NWC_STEP",
}


def pkt_name(pkt_id: int) -> str:
    """`NWC_FOO(8)` for known ids, `?(99)` for unknown.  Used in
    failure messages so a developer doesn't have to cross-reference
    network_cmd.h to read the diagnostic."""
    return f"{_PACKET_NAMES.get(pkt_id, '?')}({pkt_id})"


def find_simutrans() -> Path:
    for candidate in [
        ROOT / "build-headless" / "simutrans" / "simutrans",
        ROOT / "build" / "simutrans" / "simutrans",
        ROOT / "sim",
    ]:
        if candidate.is_file() and os.access(candidate, os.X_OK):
            return candidate
    sys.exit("no simutrans binary found; build with cmake or autoconf first")


def require_pakset():
    if not PAK_DIR.is_dir() or not any(PAK_DIR.iterdir()):
        sys.exit(f"no pakset at {PAK_DIR}; run "
                 "`cd simutrans && ../tools/get_pak.sh pak64` once")
    if not EMPTY_SAVE.is_file():
        sys.exit(f"missing starter savegame {EMPTY_SAVE}")


def _free_port() -> int:
    """Pick a free TCP port from the OS.  There's a brief TOCTOU
    window between close() and the simutrans process binding, but in
    practice the kernel doesn't recycle that fast and a collision
    just surfaces as the same "did not start listening" error as a
    binary that genuinely failed to launch."""
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.bind(("127.0.0.1", 0))
        return s.getsockname()[1]


class Server:
    """Context manager that spawns a headless server on a free port.

    Uses a per-test temp userdir so concurrent runs don't collide with
    each other or with a developer's real ~/simutrans/save tree.  The
    starter savegame is staged in that userdir's save/ subdir under the
    name `empty.sve`.  Port defaults to an ephemeral one picked by the
    OS; pass an explicit port only when reproducing a specific failure.
    """

    def __init__(self, port: int | None = None):
        self.port = port if port is not None else _free_port()
        self.proc: subprocess.Popen | None = None
        self.userdir: Path | None = None

    def __enter__(self) -> "Server":
        self.userdir = Path(tempfile.mkdtemp(prefix="nwc-e2e-"))
        save_dir = self.userdir / "save"
        save_dir.mkdir(parents=True)
        shutil.copyfile(EMPTY_SAVE, save_dir / "empty.sve")

        sim = find_simutrans()
        require_pakset()
        env = os.environ.copy()
        env.setdefault("SDL_VIDEODRIVER", "dummy")
        args = [
            str(sim),
            "-set_basedir", str(BASE_DIR),
            "-set_userdir", str(self.userdir),
            "-objects", "pak",
            "-nomidi", "-nosound", "-mute",
            "-server", str(self.port),
            "-load", "empty",
            "-debug", "2",
        ]
        self.proc = subprocess.Popen(
            args, env=env,
            stdout=subprocess.DEVNULL, stderr=subprocess.PIPE,
        )
        if not self._wait_listening(timeout=20.0):
            stderr = b""
            if self.proc.stderr:
                try:
                    stderr = self.proc.stderr.read(8192) or b""
                except Exception:
                    pass
            self.__exit__(None, None, None)
            raise RuntimeError(
                f"server did not start listening on {self.port} within "
                f"20s; stderr tail:\n"
                f"{stderr.decode(errors='replace')[-2000:]}"
            )
        return self

    def __exit__(self, *exc):
        if self.proc is not None:
            if self.proc.poll() is None:
                self.proc.send_signal(signal.SIGTERM)
                try:
                    self.proc.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    self.proc.kill()
                    self.proc.wait()
        if self.userdir and self.userdir.exists():
            shutil.rmtree(self.userdir, ignore_errors=True)

    def _wait_listening(self, timeout: float) -> bool:
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            if self.proc and self.proc.poll() is not None:
                return False
            try:
                with socket.create_connection(("127.0.0.1", self.port),
                                              timeout=0.5):
                    return True
            except OSError:
                time.sleep(0.2)
        return False

    def alive(self) -> bool:
        """Process running AND port accepting new connections."""
        if self.proc is None or self.proc.poll() is not None:
            return False
        try:
            with socket.create_connection(("127.0.0.1", self.port),
                                          timeout=1.0):
                return True
        except OSError:
            return False

    def read_stderr_tail(self, limit: int = 4096) -> str:
        if not self.proc or not self.proc.stderr:
            return ""
        try:
            return (self.proc.stderr.read(limit) or b"").decode(
                errors="replace")[-2000:]
        except Exception:
            return ""


def pack_header(packet_id: int, body: bytes) -> bytes:
    """Prepend the standard 6-byte (size, version, id) header.

    `size` is the total packet length including header — the same field
    network_command_t::rdwr writes on the wire."""
    size = HEADER_SIZE + len(body)
    return struct.pack("<HHH", size, NETWORK_VERSION, packet_id) + body


def unpack_header(data: bytes) -> tuple[int, int, int]:
    """Decode the 6-byte header.  Caller checks the values."""
    if len(data) < HEADER_SIZE:
        raise AssertionError(
            f"need {HEADER_SIZE}B for header, got {len(data)}: {data.hex()}"
        )
    return struct.unpack("<HHH", data[:HEADER_SIZE])


def recv_exact(sock: socket.socket, n: int, timeout: float) -> bytes:
    """Read up to `n` bytes; return what arrived before close/timeout."""
    sock.settimeout(timeout)
    buf = b""
    while len(buf) < n:
        try:
            chunk = sock.recv(n - len(buf))
        except (socket.timeout, OSError):
            break
        if not chunk:
            break
        buf += chunk
    return buf


def read_packets(sock: socket.socket, timeout: float,
                 max_packets: int = 8) -> list[tuple[int, bytes]]:
    """Drain up to `max_packets` whole packets from `sock`.

    Returns a list of `(packet_id, body)` tuples, where `body` is the
    raw bytes after the 6-byte header.  Stops on socket close, timeout,
    or when `max_packets` packets have been parsed.  Used by the
    silent-pass tests to verify the server keeps emitting valid
    framing after a malicious input."""
    sock.settimeout(timeout)
    out: list[tuple[int, bytes]] = []
    buf = b""
    deadline = time.monotonic() + timeout
    while len(out) < max_packets and time.monotonic() < deadline:
        try:
            chunk = sock.recv(4096)
        except (socket.timeout, OSError):
            break
        if not chunk:
            break
        buf += chunk
        while True:
            if len(buf) < HEADER_SIZE:
                break
            size, version, pkt_id = struct.unpack("<HHH", buf[:HEADER_SIZE])
            if size < HEADER_SIZE:
                raise AssertionError(
                    f"impossible packet size={size}: {buf[:HEADER_SIZE].hex()}"
                )
            if len(buf) < size:
                break
            if version != NETWORK_VERSION:
                raise AssertionError(
                    f"version={version} (expected {NETWORK_VERSION}); "
                    f"raw: {buf[:size].hex()}"
                )
            out.append((pkt_id, buf[HEADER_SIZE:size]))
            buf = buf[size:]
            if len(out) >= max_packets:
                break
    return out


def send_and_recv_exact(srv: "Server", pkt: bytes, n: int,
                        recv_timeout: float = 3.0,
                        connect_timeout: float = 2.0) -> bytes:
    """Open a fresh connection to `srv`, send `pkt`, read up to `n`
    bytes back, close.  Returns whatever arrived before timeout or
    socket close — caller decides if a short read is a failure."""
    with socket.create_connection(("127.0.0.1", srv.port),
                                  timeout=connect_timeout) as s:
        s.sendall(pkt)
        return recv_exact(s, n, timeout=recv_timeout)


def expect_heartbeat(srv: "Server", pkt: bytes,
                     drain_seconds: float = 3.0,
                     min_steps: int = 1) -> str | None:
    """Send `pkt` on a fresh connection, drain the reply stream, and
    require at least `min_steps` valid NWC_STEP heartbeat packets to
    arrive.  Used by tests that pin "server silently absorbs the
    malicious packet and keeps ticking" — i.e. the D5 (NWC_SERVICE)
    and D8 (NWC_TOOL) regression class, where pre-fix the same packet
    crashed the server.

    Returns None on pass, or a failure description.  Also fails if any
    received packet has the wrong version (framing corruption) or if
    no new TCP connection succeeds after the drain (server died)."""
    try:
        with socket.create_connection(("127.0.0.1", srv.port),
                                      timeout=2.0) as s:
            s.sendall(pkt)
            try:
                packets = read_packets(s, timeout=drain_seconds,
                                       max_packets=16)
            except AssertionError as e:
                return f"framing corruption after send: {e}"
    except OSError as e:
        return f"send failed: {e}; stderr tail:\n{srv.read_stderr_tail()}"

    steps = [p for p in packets if p[0] == NWC_STEP]
    if len(steps) < min_steps:
        names = [pkt_name(pid) for pid, _ in packets]
        return (f"saw {len(steps)} NWC_STEP heartbeat(s), wanted "
                f">= {min_steps}; received = {names}; "
                f"stderr tail:\n{srv.read_stderr_tail()}")
    if not srv.alive():
        return ("heartbeat seen but server failed to accept a new "
                "connection after; stderr tail:\n" + srv.read_stderr_tail())
    return None


def run_tests(tests: dict, argv: list[str]) -> None:
    """Tiny test runner.  Filters by substring match on argv[1:],
    prints PASS/FAIL per case, exits non-zero on any failure."""
    patterns = argv[1:]
    selected = {n: f for n, f in tests.items()
                if not patterns or any(p in n for p in patterns)}
    if not selected:
        sys.exit(f"no tests match {patterns}; available: {list(tests)}")

    fails = []
    for name, fn in selected.items():
        print(f"=== {name} ", end="", flush=True)
        try:
            err = fn()
        except Exception as e:
            err = f"raised {type(e).__name__}: {e}"
        if err is None:
            print("PASS")
        else:
            print("FAIL")
            print(err)
            fails.append(name)

    if fails:
        print(f"\n{len(fails)} failed: {', '.join(fails)}")
        sys.exit(1)
    print(f"\nall {len(selected)} passed")
