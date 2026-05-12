#!/usr/bin/env python3
"""
Probe a Simutrans server by sending a forged NWC_TOOL packet with
our_client_id = 0xFFFFFFFF and observing what the server does.

The packet is sent on a freshly opened TCP connection without prior
NWC_JOIN handshake. Pure black-box probe: this script encodes only
what's needed to put bytes on the wire and dumps anything the server
sends back (or notes when it closes the socket).

Wire format reference (from src/simutrans/network/, NETWORK_VERSION=1):

  Header (6 bytes, little-endian):
    u16 size      -- total packet size including header
    u16 version   -- 1
    u16 id        -- 8 for NWC_TOOL

  network_command_t::rdwr():
    u32 our_client_id

  network_world_command_t::rdwr():
    u32 sync_step
    u32 map_counter

  network_broadcast_world_command_t::rdwr():
    u8  exec

  nwc_tool_t::rdwr():
    u32 last_sync_step
    checklist_t:
      u32 hash
      u32 random_seed
      u16 halt_entry
      u16 line_entry
      u16 convoy_entry
    u8  player_nr
    i16 pos.x
    i16 pos.y
    i8  pos.z
    u16 tool_id
    i16 wt
    str default_param         -- u16 length, then bytes (no NUL)
    u8  init                  -- bool
    u32 tool_client_id        -- second copy of client id inside the tool
    u8  flags
    u32 callback_id
    -- followed by tool-specific custom_data appended verbatim
"""

import argparse
import socket
import struct
import sys
import time


NWC_TOOL = 8
NETWORK_VERSION = 1
HEADER_SIZE = 6


def encode_str(s: bytes) -> bytes:
    return struct.pack("<H", len(s)) + s


def build_nwc_tool(
    our_client_id: int,
    tool_id: int,
    player_nr: int,
    pos_x: int,
    pos_y: int,
    pos_z: int,
    wt: int,
    default_param: bytes,
    init: bool,
    tool_client_id: int,
    flags: int,
    callback_id: int,
    sync_step: int,
    map_counter: int,
    exec_flag: bool,
    last_sync_step: int,
    custom_data: bytes,
) -> bytes:
    body = b""
    # network_command_t
    body += struct.pack("<I", our_client_id)
    # network_world_command_t
    body += struct.pack("<II", sync_step, map_counter)
    # network_broadcast_world_command_t
    body += struct.pack("<B", 1 if exec_flag else 0)
    # nwc_tool_t
    body += struct.pack("<I", last_sync_step)
    # checklist_t (hash, random_seed, halt, line, convoy)
    body += struct.pack("<IIHHH", 0, 0, 0, 0, 0)
    body += struct.pack("<B", player_nr & 0xFF)
    body += struct.pack("<hhb", pos_x, pos_y, pos_z)
    body += struct.pack("<Hh", tool_id, wt)
    body += encode_str(default_param)
    body += struct.pack("<B", 1 if init else 0)
    body += struct.pack("<I", tool_client_id)
    body += struct.pack("<B", flags & 0xFF)
    body += struct.pack("<I", callback_id)
    body += custom_data

    size = HEADER_SIZE + len(body)
    header = struct.pack("<HHH", size, NETWORK_VERSION, NWC_TOOL)
    return header + body


def hexdump(data: bytes, label: str = "") -> None:
    if label:
        print(f"---- {label} ({len(data)} bytes) ----")
    for off in range(0, len(data), 16):
        chunk = data[off : off + 16]
        hexp = " ".join(f"{b:02x}" for b in chunk)
        asci = "".join(chr(b) if 32 <= b < 127 else "." for b in chunk)
        print(f"{off:08x}  {hexp:<48}  {asci}")


def try_decode_header(data: bytes) -> None:
    """If reply starts with a Simutrans packet header, decode it."""
    if len(data) < HEADER_SIZE:
        return
    size, version, pkt_id = struct.unpack("<HHH", data[:HEADER_SIZE])
    names = {
        0: "NWC_INVALID",
        1: "NWC_GAMEINFO",
        2: "NWC_NICK",
        3: "NWC_CHAT",
        4: "NWC_JOIN",
        5: "NWC_SYNC",
        6: "NWC_GAME",
        7: "NWC_READY",
        8: "NWC_TOOL",
        9: "NWC_CHECK",
        10: "NWC_PAKSETINFO",
        11: "NWC_SERVICE",
        12: "NWC_AUTH_PLAYER",
        13: "NWC_CHG_PLAYER",
        14: "NWC_SCENARIO",
        15: "NWC_SCENARIO_RULES",
        16: "NWC_STEP",
    }
    print(
        f"  parsed header: size={size} version={version} "
        f"id={pkt_id} ({names.get(pkt_id, '?')})"
    )


def recv_all(
    sock: socket.socket, idle_timeout: float, max_time: float, max_bytes: int
) -> bytes:
    """Drain bytes until idle for `idle_timeout`s, or `max_time`s wall
    clock has passed, or `max_bytes` accumulated, or the socket closes."""
    sock.setblocking(False)
    buf = b""
    start = time.monotonic()
    idle_deadline = start + idle_timeout
    hard_deadline = start + max_time
    while True:
        now = time.monotonic()
        if now >= hard_deadline:
            print(f"  hit max-recv-time ({max_time}s)")
            break
        if now >= idle_deadline:
            print(f"  idle for {idle_timeout}s, stopping")
            break
        try:
            chunk = sock.recv(4096)
        except BlockingIOError:
            time.sleep(0.05)
            continue
        except (ConnectionResetError, OSError) as e:
            print(f"  socket error during recv: {e}")
            break
        if not chunk:
            print("  server closed connection")
            break
        buf += chunk
        idle_deadline = time.monotonic() + idle_timeout
        if len(buf) >= max_bytes:
            print(f"  hit max-bytes ({max_bytes})")
            break
    return buf


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("host", help="Simutrans server hostname/IP")
    p.add_argument("--port", type=int, default=13353)
    p.add_argument(
        "--client-id",
        type=lambda s: int(s, 0),
        default=0xFFFFFFFF,
        help="our_client_id to forge (default 0xFFFFFFFF)",
    )
    p.add_argument(
        "--tool-id",
        type=lambda s: int(s, 0),
        default=0x0000,
        help="tool_id (combine SIMPLE_TOOL/GENERAL_TOOL flag with tool number; "
             "0 means create_tool() will return NULL on server",
    )
    p.add_argument("--player-nr", type=int, default=1)
    p.add_argument("--pos-x", type=int, default=0)
    p.add_argument("--pos-y", type=int, default=0)
    p.add_argument("--pos-z", type=int, default=0)
    p.add_argument("--wt", type=int, default=-1)
    p.add_argument("--default-param", default="")
    p.add_argument("--init", action="store_true", default=True)
    p.add_argument("--work", dest="init", action="store_false")
    p.add_argument("--flags", type=lambda s: int(s, 0), default=0)
    p.add_argument("--callback-id", type=lambda s: int(s, 0), default=0)
    p.add_argument("--sync-step", type=lambda s: int(s, 0), default=0)
    p.add_argument("--map-counter", type=lambda s: int(s, 0), default=0)
    p.add_argument("--last-sync-step", type=lambda s: int(s, 0), default=0)
    p.add_argument("--exec", dest="exec_flag", action="store_true",
                   help="set exec=true (server normally rejects this)")
    p.add_argument(
        "--tool-client-id",
        type=lambda s: int(s, 0),
        default=None,
        help="inner tool_client_id (defaults to --client-id)",
    )
    p.add_argument(
        "--custom-data",
        default="",
        help="hex bytes appended after the fixed tool fields",
    )
    p.add_argument("--recv-timeout", type=float, default=3.0,
                   help="idle timeout: stop after this many seconds without data")
    p.add_argument("--max-recv-time", type=float, default=10.0,
                   help="wall-clock cap on total receive duration")
    p.add_argument("--max-recv-bytes", type=int, default=65536,
                   help="cap on bytes accumulated before stopping")
    p.add_argument("--connect-timeout", type=float, default=5.0)
    p.add_argument(
        "--dry-run",
        action="store_true",
        help="build and print the packet but don't send",
    )
    args = p.parse_args()

    tool_client_id = (
        args.tool_client_id if args.tool_client_id is not None else args.client_id
    )
    custom = bytes.fromhex(args.custom_data) if args.custom_data else b""

    pkt = build_nwc_tool(
        our_client_id=args.client_id,
        tool_id=args.tool_id,
        player_nr=args.player_nr,
        pos_x=args.pos_x,
        pos_y=args.pos_y,
        pos_z=args.pos_z,
        wt=args.wt,
        default_param=args.default_param.encode("utf-8"),
        init=args.init,
        tool_client_id=tool_client_id,
        flags=args.flags,
        callback_id=args.callback_id,
        sync_step=args.sync_step,
        map_counter=args.map_counter,
        exec_flag=args.exec_flag,
        last_sync_step=args.last_sync_step,
        custom_data=custom,
    )

    print(f"target: {args.host}:{args.port}")
    print(
        f"forged NWC_TOOL: our_client_id=0x{args.client_id:08x} "
        f"tool_id=0x{args.tool_id:04x} player_nr={args.player_nr} "
        f"init={args.init} exec={args.exec_flag}"
    )
    hexdump(pkt, "outgoing packet")

    if args.dry_run:
        return 0

    try:
        sock = socket.create_connection(
            (args.host, args.port), timeout=args.connect_timeout
        )
    except OSError as e:
        print(f"connect failed: {e}")
        return 1

    with sock:
        try:
            sock.sendall(pkt)
        except OSError as e:
            print(f"send failed: {e}")
            return 1
        print(
            f"sent {len(pkt)} bytes, draining (idle={args.recv_timeout}s "
            f"max={args.max_recv_time}s cap={args.max_recv_bytes}B)..."
        )

        reply = recv_all(
            sock, args.recv_timeout, args.max_recv_time, args.max_recv_bytes
        )
        if not reply:
            print("no data received before timeout/close")
            return 0
        hexdump(reply, "server reply")
        try_decode_header(reply)
        # if the reply contains more than one packet, try to chain
        off = 0
        while off + HEADER_SIZE <= len(reply):
            size, version, pkt_id = struct.unpack(
                "<HHH", reply[off : off + HEADER_SIZE]
            )
            if size < HEADER_SIZE or size > len(reply) - off:
                break
            print(f"  packet @ +{off}: size={size} version={version} id={pkt_id}")
            off += size

    return 0


if __name__ == "__main__":
    sys.exit(main())