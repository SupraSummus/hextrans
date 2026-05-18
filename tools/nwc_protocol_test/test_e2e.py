#!/usr/bin/env python3
# Black-box e2e tests for the multiplayer wire protocol implemented in
# src/simutrans/network/network_cmd*.cc.
#
# Each test spawns a headless simutrans server, drives a single
# hand-rolled packet over TCP, and asserts on the byte-exact reply
# observed by the attacker / client side.  The tests serve two
# overlapping purposes:
#
#   1. **Protocol pin.**  The reply structure for NWC_AUTH_PLAYER is
#      documented in network_cmd.h but only enforced by what the server
#      actually emits.  These tests freeze it as a byte sequence so a
#      future refactor that drops a field, reorders one, or skips the
#      reply entirely surfaces as a CI failure rather than as a silent
#      client compatibility break.
#
#   2. **Security regression tripwire.**  The NULL-player-slot crash
#      (see network_cmd_ingame.cc:nwc_auth_player_t::execute, fixed in
#      this same commit) manifests on the wire as "server consumes the
#      packet then closes the TCP connection without replying".  The
#      reply assertion catches both the crash itself and any
#      regression where the server stops sending the reply on the
#      NULL-slot path for some other reason.
#
# Run:
#   tools/nwc_protocol_test/test_e2e.py             # all tests
#   tools/nwc_protocol_test/test_e2e.py auth_player # name-substring filter
#
# Requires:
#   - A built simutrans binary (cmake or autoconf).
#   - simutrans/pak/ populated with a pak64-compatible pakset.
#   - tests/empty-16x16.sve in the tree (default starter map; leaves
#     player slots 2..14 unfilled, which is the precondition the
#     NULL-slot reply test relies on).

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
NWC_AUTH_PLAYER = 12

# nwc_auth_player_t wire layout (NETWORK_VERSION=1):
#   header  : u16 size | u16 version | u16 id          (6 bytes)
#   base    : u32 our_client_id                        (4 bytes)
#   payload : u8[20] hash | u8 player_nr | u16 player_unlocked  (23 bytes)
# Total: 33 bytes both ways.
AUTH_PLAYER_PACKET_SIZE = 33


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


class Server:
    """Context manager that spawns a headless server on `port`.

    Uses a per-test temp userdir so concurrent runs don't collide with
    each other or with a developer's real ~/simutrans/save tree.  The
    starter savegame is staged in that userdir's save/ subdir under the
    name `empty.sve`.
    """

    def __init__(self, port: int):
        self.port = port
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


def build_nwc_auth_player(*, player_nr: int,
                          our_client_id: int = 0,
                          hash20: bytes = b"\x00" * 20,
                          player_unlocked: int = 0) -> bytes:
    assert len(hash20) == 20
    body = struct.pack("<I", our_client_id)
    body += hash20
    body += struct.pack("<B", player_nr & 0xFF)
    body += struct.pack("<H", player_unlocked & 0xFFFF)
    size = HEADER_SIZE + len(body)
    return struct.pack("<HHH", size, NETWORK_VERSION, NWC_AUTH_PLAYER) + body


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


def parse_auth_player_reply(reply: bytes) -> dict:
    """Decode a NWC_AUTH_PLAYER packet into its fields.  The assertions
    here are the on-wire spec the test pins."""
    assert len(reply) == AUTH_PLAYER_PACKET_SIZE, (
        f"length {len(reply)} != {AUTH_PLAYER_PACKET_SIZE}; raw: {reply.hex()}"
    )
    size, version, pkt_id = struct.unpack("<HHH", reply[:HEADER_SIZE])
    assert size == AUTH_PLAYER_PACKET_SIZE, f"header size={size}"
    assert version == NETWORK_VERSION, f"header version={version}"
    assert pkt_id == NWC_AUTH_PLAYER, f"header id={pkt_id} (expected 12)"
    return dict(
        our_client_id=struct.unpack("<I", reply[6:10])[0],
        hash=reply[10:30],
        player_nr=reply[30],
        player_unlocked=struct.unpack("<H", reply[31:33])[0],
    )


def check_auth_reply(srv: Server, *, player_nr: int,
                     expected_unlocked: int) -> str | None:
    """Send one NWC_AUTH_PLAYER, assert on the byte-exact reply.
    Returns None on pass, or a failure description."""
    pkt = build_nwc_auth_player(player_nr=player_nr)
    with socket.create_connection(("127.0.0.1", srv.port), timeout=2.0) as s:
        s.sendall(pkt)
        reply = recv_exact(s, AUTH_PLAYER_PACKET_SIZE, timeout=3.0)
    if not reply:
        return ("no reply: server closed the connection without "
                "emitting any bytes. stderr tail:\n" + srv.read_stderr_tail())
    try:
        decoded = parse_auth_player_reply(reply)
    except AssertionError as e:
        return f"malformed reply: {e}"
    if decoded["player_unlocked"] != expected_unlocked:
        return (f"player_unlocked={decoded['player_unlocked']:#x}, "
                f"expected {expected_unlocked:#x}")
    if not srv.alive():
        return "reply was well-formed but server died after"
    return None


# ---- Test cases --------------------------------------------------------


def test_auth_player_null_slot():
    """NWC_AUTH_PLAYER targeting an unfilled player slot must elicit a
    standard 33-byte reply with player_unlocked=0 (silent-fail
    semantics) and leave the server alive.  Pre-fix this segfaults the
    server inside `welt->get_player(3)->access_password_hash()`."""
    with Server(13355) as srv:
        return check_auth_reply(srv, player_nr=3, expected_unlocked=0)


def test_auth_player_filled_slot_empty_password():
    """NWC_AUTH_PLAYER targeting public service (player_nr=1) with an
    all-zero hash matches the default empty password and unlocks the
    slot — reply carries bit 1 set in player_unlocked.  Doubles as the
    negative control: a filled slot must not crash, pre- or post-fix."""
    with Server(13356) as srv:
        return check_auth_reply(srv, player_nr=1, expected_unlocked=1 << 1)


TESTS = {
    "test_auth_player_null_slot": test_auth_player_null_slot,
    "test_auth_player_filled_slot_empty_password":
        test_auth_player_filled_slot_empty_password,
}


def main(argv):
    patterns = argv[1:]
    selected = {n: f for n, f in TESTS.items()
                if not patterns or any(p in n for p in patterns)}
    if not selected:
        sys.exit(f"no tests match {patterns}; available: {list(TESTS)}")

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


if __name__ == "__main__":
    main(sys.argv)
