#!/usr/bin/env python3
# Shared rig for the black-box tests in this package.  Server context
# manager, 6-byte (size, version, id) framing helpers, and the
# assert_heartbeat assertion used by the silent-absorb regression
# tests.  Conn, join() and connect_and_sync() are for tests that need
# a client connection kept open (a real join, or a bare connection
# that has seen the server's checklist).  Per-command wire layouts
# live in test_<command>.py.

import fcntl
import os
import re
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

# Mirrors network_command_id in src/simutrans/network/network_cmd.h.
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
    # Brief TOCTOU between close and simutrans binding; a collision
    # surfaces as the same "did not start listening" error as a real
    # launch failure, which is acceptable.
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.bind(("127.0.0.1", 0))
        return s.getsockname()[1]


class Server:
    """Context manager spawning a headless server on a free port.
    Each instance gets its own temp userdir so concurrent runs don't
    collide with each other or with a developer's ~/simutrans tree."""

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
        # Non-blocking so read_stderr_tail won't wait for a full read
        # or process exit when called from an assertion message.
        fd = self.proc.stderr.fileno()
        fcntl.fcntl(fd, fcntl.F_SETFL,
                    fcntl.fcntl(fd, fcntl.F_GETFL) | os.O_NONBLOCK)
        if not self._wait_listening(timeout=20.0):
            stderr = self.read_stderr_tail(limit=8192)
            self.__exit__(None, None, None)
            raise RuntimeError(
                f"server did not start listening on {self.port} within "
                f"20s; stderr tail:\n{stderr}"
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
            if self.proc.stderr is not None:
                self.proc.stderr.close()
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
            chunk = os.read(self.proc.stderr.fileno(), limit)
        except BlockingIOError:
            return ""
        return chunk.decode(errors="replace")[-2000:]


def pack_header(packet_id: int, body: bytes) -> bytes:
    # `size` is the total packet length including header — matches
    # network_command_t::rdwr on the wire.
    size = HEADER_SIZE + len(body)
    return struct.pack("<HHH", size, NETWORK_VERSION, packet_id) + body


def unpack_header(data: bytes) -> tuple[int, int, int]:
    if len(data) < HEADER_SIZE:
        raise AssertionError(
            f"need {HEADER_SIZE}B for header, got {len(data)}: {data.hex()}"
        )
    return struct.unpack("<HHH", data[:HEADER_SIZE])


def recv_exact(sock: socket.socket, n: int, timeout: float) -> bytes:
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
    """Drain whole packets from `sock` as `(packet_id, body)` tuples.
    Stops on close, timeout, or after `max_packets`."""
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
    with socket.create_connection(("127.0.0.1", srv.port),
                                  timeout=connect_timeout) as s:
        s.sendall(pkt)
        return recv_exact(s, n, timeout=recv_timeout)


def assert_heartbeat(srv: "Server", pkt: bytes,
                     drain_seconds: float = 3.0,
                     min_steps: int = 1) -> None:
    """Send `pkt`, drain the reply, require ≥`min_steps` NWC_STEP
    heartbeats and a still-alive server.  The "silently absorbs a
    malicious packet and keeps ticking" assertion."""
    try:
        with socket.create_connection(("127.0.0.1", srv.port),
                                      timeout=2.0) as s:
            s.sendall(pkt)
            packets = read_packets(s, timeout=drain_seconds, max_packets=16)
    except OSError as e:
        raise AssertionError(
            f"send failed: {e}; stderr tail:\n{srv.read_stderr_tail()}"
        )

    steps = [p for p in packets if p[0] == NWC_STEP]
    if len(steps) < min_steps:
        names = [pkt_name(pid) for pid, _ in packets]
        raise AssertionError(
            f"saw {len(steps)} NWC_STEP heartbeat(s), wanted "
            f">= {min_steps}; received = {names}; "
            f"stderr tail:\n{srv.read_stderr_tail()}"
        )
    if not srv.alive():
        raise AssertionError(
            "heartbeat seen but server failed to accept a new "
            "connection after; stderr tail:\n" + srv.read_stderr_tail()
        )


_TOOL_FLAGS = ("GENERAL_TOOL", "SIMPLE_TOOL", "DIALOGE_TOOL")


def tool_id(name: str) -> int:
    """Wire tool_id for `name` (e.g. "TOOL_QUIT"), read from the enums
    in src/simutrans/tool/simmenu.h so a renumbering cannot leave a
    test aiming at a different tool than it names."""
    text = (ROOT / "src" / "simutrans" / "tool" / "simmenu.h").read_text()
    for block in re.finditer(r"enum\s*\{(.*?)\};", text, re.S):
        body = re.sub(r"//[^\n]*", "", block.group(1))
        n = 0
        ids: dict[str, int] = {}
        flag = None
        for item in (i.strip() for i in body.split(",")):
            if not item:
                continue
            key, _, val = item.partition("=")
            key = key.strip()
            if val.strip():
                n = int(val.strip().rstrip("uU"), 0)
            if key in _TOOL_FLAGS:
                flag = n
            else:
                ids[key] = n
                n += 1
        if flag is not None and name in ids:
            return ids[name] | flag
    raise KeyError(f"{name} not found in simmenu.h tool enums")


class Conn:
    """One client connection with incremental packet framing.  The
    NWC_GAME payload (raw savegame bytes after the packet) is consumed
    transparently."""

    def __init__(self, srv: "Server", timeout: float = 2.0):
        self.sock = socket.create_connection(("127.0.0.1", srv.port),
                                             timeout=timeout)
        self.buf = b""
        self.eof = False

    def __enter__(self) -> "Conn":
        return self

    def __exit__(self, *exc):
        self.sock.close()

    def send(self, pkt: bytes) -> None:
        self.sock.sendall(pkt)

    def _fill(self, need: int, deadline: float) -> bool:
        while len(self.buf) < need:
            left = deadline - time.monotonic()
            if left <= 0 or self.eof:
                return False
            self.sock.settimeout(left)
            try:
                chunk = self.sock.recv(65536)
            except (socket.timeout, OSError):
                return False
            if not chunk:
                self.eof = True
                return False
            self.buf += chunk
        return True

    def next_packet(self, timeout: float) -> tuple[int, bytes] | None:
        """Next whole `(packet_id, body)`, or None on timeout / close."""
        deadline = time.monotonic() + timeout
        if not self._fill(HEADER_SIZE, deadline):
            return None
        size, version, pkt_id = struct.unpack("<HHH", self.buf[:HEADER_SIZE])
        if size < HEADER_SIZE or version != NETWORK_VERSION:
            raise AssertionError(
                f"bad framing size={size} version={version}: "
                f"{self.buf[:HEADER_SIZE].hex()}"
            )
        if not self._fill(size, deadline):
            return None
        body = self.buf[HEADER_SIZE:size]
        self.buf = self.buf[size:]
        if pkt_id == NWC_GAME:
            # our_client_id u32 | len u32, then `len` raw savegame bytes.
            length = struct.unpack("<I", body[4:8])[0]
            if not self._fill(length, deadline + 10.0):
                return None
            self.buf = self.buf[length:]
        return pkt_id, body

    def wait_for(self, pkt_id: int, timeout: float) -> bytes | None:
        """Body of the first `pkt_id` packet, discarding others."""
        deadline = time.monotonic() + timeout
        while True:
            left = deadline - time.monotonic()
            if left <= 0:
                return None
            pkt = self.next_packet(left)
            if pkt is None:
                return None
            if pkt[0] == pkt_id:
                return pkt[1]

    def drain(self, seconds: float) -> list[tuple[int, bytes]]:
        """All packets arriving within `seconds`.  Stops early on close."""
        out: list[tuple[int, bytes]] = []
        deadline = time.monotonic() + seconds
        while time.monotonic() < deadline and not self.eof:
            pkt = self.next_packet(deadline - time.monotonic())
            if pkt is not None:
                out.append(pkt)
        return out


def _pack_str(s: str) -> bytes:
    raw = s.encode()
    return struct.pack("<H", len(raw)) + raw


class WorldPoint:
    """What a tool packet needs to look like it came from a client in
    sync with the server: a recent sync step, the server's checklist
    for it, and the current map counter."""

    def __init__(self, sync_step: int, map_counter: int,
                 checklist: tuple[int, int, int, int, int]):
        self.sync_step = sync_step
        self.map_counter = map_counter
        self.checklist = checklist


def _checklist(raw: bytes) -> tuple[int, int, int, int, int]:
    return struct.unpack("<IIHHH", raw[:14])


def connect_and_sync(srv: "Server", timeout: float = 10.0
                     ) -> tuple[Conn, WorldPoint]:
    """Open a connection without joining and wait for the server's
    next NWC_CHECK broadcast, which carries a verifiable checklist."""
    conn = Conn(srv)
    # nwc_check_t: client_id u32 | sync_step u32 | map_counter u32 |
    # checklist 14B | server_sync_step u32
    body = conn.wait_for(NWC_CHECK, timeout)
    if body is None:
        conn.sock.close()
        raise AssertionError("no NWC_CHECK from server; stderr tail:\n"
                             + srv.read_stderr_tail())
    map_counter = struct.unpack("<I", body[8:12])[0]
    server_step = struct.unpack("<I", body[26:30])[0]
    return conn, WorldPoint(server_step, map_counter, _checklist(body[12:26]))


def join(srv: "Server", nickname: str = "wire-test", timeout: float = 20.0
         ) -> tuple[Conn, WorldPoint, int]:
    """Join the way a game client does: NWC_JOIN, then take the
    NWC_SYNC / NWC_GAME transfer until NWC_READY and the NWC_AUTH_PLAYER
    that follows.  Returns the connection, the world point from
    NWC_READY and the `player_unlocked` bitmask the server granted."""
    conn = Conn(srv)
    try:
        # network_command_t: client_id u32 | nick str | client_id u32 |
        # answer u8
        conn.send(pack_header(NWC_JOIN, struct.pack("<I", 0)
                              + _pack_str(nickname) + struct.pack("<IB", 0, 0)))
        ready = conn.wait_for(NWC_READY, timeout)
        if ready is None:
            raise AssertionError("no NWC_READY after NWC_JOIN; stderr tail:\n"
                                 + srv.read_stderr_tail())
        auth = conn.wait_for(NWC_AUTH_PLAYER, 5.0)
        if auth is None:
            raise AssertionError("no NWC_AUTH_PLAYER after NWC_READY")
    except BaseException:
        conn.sock.close()
        raise
    sync_step, map_counter = struct.unpack("<II", ready[4:12])
    unlocked = struct.unpack("<H", auth[25:27])[0]
    return conn, WorldPoint(sync_step, map_counter,
                            _checklist(ready[12:26])), unlocked


class ServerTestCase:
    """Mixin: fresh `Server` per test method on `self.srv`.  Per-test
    isolation is deliberate — a packet that crashes the server must
    not poison the next test."""

    def setUp(self):
        super().setUp()
        self.srv = self.enterContext(Server())
