#!/usr/bin/env python3
# NWC_TOOL from a client for tools flagged WFL_LOCAL.  tool_quit_t,
# tool_load_scenario_t and tool_work_world_t (quit, new map, load,
# save, scenario) are single-player control flow: simmenu.h says a
# WFL_LOCAL call "will not be sent to server".  nwc_tool_t::clone used
# to relay them anyway, and do_command then ran init() on the server,
# so any connected peer could stop the server or make it load / save.
#
# test_tool.py sends exec=True, which the server rejects before
# clone().  Here the packets are what a client sends (exec=False).
# The server relays an accepted tool to every client, the sender
# included, so a harmless relayed tool on either side of the packet
# under test shows what clone() did with it: the first proves this
# packet shape reaches clone() at all, the second is a marker that
# everything the server relayed before it has been seen.

import struct
import unittest
from pathlib import Path

from . import wire
from .test_tool import build as build_tool

PLAYER_UNOWNED = 15
# Offset of tool_id in an NWC_TOOL body (see test_tool.build).
TOOL_ID_OFFSET = 37

TOOL_ADD_MESSAGE = wire.tool_id("TOOL_ADD_MESSAGE")
TOOL_QUIT = wire.tool_id("TOOL_QUIT")
TOOL_LOAD_SCENARIO = wire.tool_id("TOOL_LOAD_SCENARIO")
TOOL_WORK_WORLD = wire.tool_id("TOOL_WORK_WORLD")

Packets = list[tuple[int, bytes]]


def relayed(packets: Packets) -> list[tuple[int, str]]:
    """(tool_id, default_param) of each NWC_TOOL in `packets`."""
    out = []
    for pkt_id, body in packets:
        if pkt_id != wire.NWC_TOOL:
            continue
        tid, _wt, n = struct.unpack_from("<HhH", body, TOOL_ID_OFFSET)
        start = TOOL_ID_OFFSET + 6
        out.append((tid, body[start:start + n].decode(errors="replace")))
    return out


def map_counters(packets: Packets) -> set[int]:
    return {struct.unpack_from("<I", body, 8)[0]
            for pkt_id, body in packets
            if pkt_id in (wire.NWC_STEP, wire.NWC_CHECK)}


def read_until_relayed(conn: wire.Conn, param: str,
                       timeout: float) -> Packets | None:
    """Packets up to and including the relay of ADD_MESSAGE `param`,
    or None if it does not arrive."""
    seen: Packets = []
    while True:
        pkt = conn.next_packet(timeout)
        if pkt is None:
            return None
        seen.append(pkt)
        if (TOOL_ADD_MESSAGE, param) in relayed([pkt]):
            return seen


class LocalToolTest(unittest.TestCase):

    def check_rejected(self, tool_id: int, default_param: str, *,
                       joined: bool, player_nr: int = PLAYER_UNOWNED,
                       flags: int = 0, marker: Path | None = None) -> None:
        with wire.Server() as srv:
            if joined:
                conn, wp, unlocked = wire.join(srv)
                if player_nr < PLAYER_UNOWNED:
                    self.assertTrue(
                        unlocked & (1 << player_nr),
                        f"player {player_nr} not unlocked ({unlocked:#x}); "
                        "pick a player the joined client may act as")
            else:
                conn, wp = wire.connect_and_sync(srv)

            def packet(tid: int, param: str, fl: int = 0) -> bytes:
                return build_tool(
                    our_client_id=0, tool_id=tid, player_nr=player_nr,
                    default_param=param.encode(), init=False, flags=fl,
                    exec_flag=False, last_sync_step=wp.sync_step,
                    checklist=wp.checklist, map_counter=wp.map_counter)

            what = f"tool {tool_id:#x} {default_param!r}"
            with conn:
                conn.send(packet(TOOL_ADD_MESSAGE, "0,before"))
                self.assertIsNotNone(
                    read_until_relayed(conn, "0,before", 5.0),
                    "control tool was not relayed, so this packet shape "
                    "never reaches clone(); stderr tail:\n"
                    + srv.read_stderr_tail())

                conn.send(packet(tool_id, default_param, flags))
                conn.send(packet(TOOL_ADD_MESSAGE, "0,after"))
                seen = read_until_relayed(conn, "0,after", 5.0)
                self.assertIsNotNone(
                    seen,
                    f"server stopped relaying after {what}; stderr tail:\n"
                    + srv.read_stderr_tail())
                self.assertNotIn(
                    tool_id, [tid for tid, _ in relayed(seen)],
                    f"server relayed WFL_LOCAL {what} to clients")

                # Execution follows the relay by a sync step or so.
                ticks = conn.drain(0.7)
                seen += ticks
                self.assertTrue(
                    srv.alive(),
                    f"server exited after {what}; stderr tail:\n"
                    + srv.read_stderr_tail())
                self.assertFalse(conn.eof,
                                 f"server dropped the sender after {what}")
                self.assertIn(wire.NWC_STEP, [i for i, _ in ticks],
                              f"server stopped ticking after {what}")
                self.assertEqual(map_counters(seen) - {wp.map_counter},
                                 set(), f"world replaced by {what}")
            if marker is not None:
                self.assertEqual(
                    list(srv.userdir.rglob(marker.name + "*")), [],
                    f"server wrote {marker.name}* for a client's save")

    def test_quit_from_joined_client(self):
        for player_nr in (PLAYER_UNOWNED, 0):
            with self.subTest(player_nr=player_nr):
                self.check_rejected(TOOL_QUIT, "", joined=True,
                                    player_nr=player_nr)

    def test_quit_from_connection_without_join(self):
        self.check_rejected(TOOL_QUIT, "", joined=False)

    def test_quit_ignores_wire_flags(self):
        """WFL_LOCAL | WFL_NO_CHK is what the real tool carries; the
        wire copy is peer-controlled, so the verdict must not hinge
        on it."""
        self.check_rejected(TOOL_QUIT, "", joined=True, flags=0x14)

    def test_new_world_from_client(self):
        self.check_rejected(TOOL_QUIT, "x", joined=True)

    def test_work_world_new_map_from_client(self):
        self.check_rejected(TOOL_WORK_WORLD, "n", joined=True)

    def test_work_world_load_from_client(self):
        self.check_rejected(TOOL_WORK_WORLD, "l0save/empty", joined=True)

    def test_work_world_save_from_client(self):
        self.check_rejected(TOOL_WORK_WORLD, "swire-test-client-save",
                            joined=True,
                            marker=Path("wire-test-client-sav"))

    def test_load_scenario_from_client(self):
        self.check_rejected(TOOL_LOAD_SCENARIO, "0,nonexistent", joined=True)
