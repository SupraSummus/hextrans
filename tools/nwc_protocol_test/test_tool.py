#!/usr/bin/env python3
# NWC_TOOL wire pin + D8 wire-supplied-client-id regression tripwire.
#
# nwc_tool_t::clone (src/simutrans/network/network_cmd_ingame.cc) used
# to feed the wire-supplied our_client_id straight into
# socket_list_t::get_client.  A peer-supplied 0xFFFFFFFF crashed the
# server on vector_tpl bounds; any in-use slot number let the sender
# impersonate that client and inherit its player_unlocked bitmap.
#
# Two fixes landed together:
#
#   1. network_command_t::read_from_packet now overrides our_client_id
#      with the socket-derived id on the server side, so the wire value
#      is informational only.
#   2. nwc_tool_t::clone bounds-checks our_client_id before indexing
#      (defense-in-depth — slot can go inactive between receive and
#      execute).
#
# The wire pin here exercises (1) directly: a forged 0xFFFFFFFF on the
# wire must not crash the server, and the post-fix server keeps
# emitting its NWC_STEP heartbeat as if nothing happened.

import struct

from . import wire


# nwc_tool_t wire layout (NETWORK_VERSION=1), as built by
# src/simutrans/network/network_cmd_ingame.cc:nwc_tool_t::rdwr.
# Used here only to forge well-shaped packets — no inbound NWC_TOOL
# parser is needed because the server doesn't reply with NWC_TOOL on
# the malicious paths the test exercises.
def build(*,
          our_client_id: int,
          tool_id: int = 0x1000,
          player_nr: int = 1,
          pos_x: int = 0,
          pos_y: int = 0,
          pos_z: int = 0,
          wt: int = -1,
          default_param: bytes = b"",
          init: bool = True,
          tool_client_id: int | None = None,
          flags: int = 0,
          callback_id: int = 0,
          sync_step: int = 0,
          map_counter: int = 0,
          exec_flag: bool = True,
          last_sync_step: int = 0,
          custom_data: bytes = b"") -> bytes:
    if tool_client_id is None:
        tool_client_id = our_client_id
    body = struct.pack("<I", our_client_id)                  # network_command_t
    body += struct.pack("<II", sync_step, map_counter)       # network_world_command_t
    body += struct.pack("<B", 1 if exec_flag else 0)         # network_broadcast_world_command_t
    body += struct.pack("<I", last_sync_step)                # nwc_tool_t
    body += struct.pack("<IIHHH", 0, 0, 0, 0, 0)             # checklist_t
    body += struct.pack("<B", player_nr & 0xFF)
    body += struct.pack("<hhb", pos_x, pos_y, pos_z)
    body += struct.pack("<Hh", tool_id, wt)
    body += struct.pack("<H", len(default_param)) + default_param
    body += struct.pack("<B", 1 if init else 0)
    body += struct.pack("<I", tool_client_id)
    body += struct.pack("<B", flags & 0xFF)
    body += struct.pack("<I", callback_id)
    body += custom_data
    return wire.pack_header(wire.NWC_TOOL, body)


# ---- Test cases --------------------------------------------------------


def test_tool_forged_client_id_does_not_crash():
    """D8: pre-auth NWC_TOOL with our_client_id=0xFFFFFFFF must not
    crash the server.  Pre-fix, nwc_tool_t::clone fed the bogus id
    into socket_list_t::get_client and tripped vector_tpl's bounds
    fatal on the server.  Post-fix, read_from_packet overrides
    our_client_id with the socket-derived id before any consumer
    reads it, so the wire value is informational only — and the
    server keeps emitting its NWC_STEP heartbeat.

    The same path also closed an impersonation primitive (any
    valid-other-slot id let a peer inherit that slot's
    player_unlocked bitmap).  We can't observe the impersonation
    flag externally without a fully authenticated peer in the same
    test, so the crash-and-keep-ticking pin is what's enforced here;
    the impersonation half is covered by the network_cmd_ingame.cc
    override itself."""
    with wire.Server() as srv:
        pkt = build(our_client_id=0xFFFFFFFF)
        return wire.expect_heartbeat(srv, pkt)


def test_tool_zero_client_id_does_not_crash():
    """Negative control: the same forged packet shape with
    our_client_id=0 (the legitimate "no client" sentinel many clients
    send before the first NWC_READY) must also leave the server
    ticking.  Catches a regression that over-aggressively rejects
    legitimate pre-auth NWC_TOOL framing while still trying to plug
    the D8 path."""
    with wire.Server() as srv:
        pkt = build(our_client_id=0)
        return wire.expect_heartbeat(srv, pkt)


TESTS = {
    "test_tool_forged_client_id_does_not_crash":
        test_tool_forged_client_id_does_not_crash,
    "test_tool_zero_client_id_does_not_crash":
        test_tool_zero_client_id_does_not_crash,
}
