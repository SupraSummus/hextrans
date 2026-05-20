#!/usr/bin/env python3
# Reproduction for the memory_rw_t::rdwr_bool uninit-read UB.
# nwc_chg_player_t's default ctor (used by read_from_packet) leaves
# `scripted_call` uninitialised; pre-fix rdwr_bool loaded it before
# overwriting from the wire, tripping UBSAN inside
# network_command_t::receive before any consumer ran.  One
# well-formed NWC_CHG_PLAYER is enough — plain-Debug builds passed
# the bad code, clang+ASAN+UBSAN fails.

import struct
import unittest

from . import wire


# nwc_chg_player_t wire layout (NETWORK_VERSION=1):
#   header  : u16 size | u16 version | u16 id          (6 bytes)
#   base    : u32 our_client_id                        (4 bytes)
#   world   : u32 sync_step | u32 map_counter          (8 bytes)
#   bcast   : u8 exec                                  (1 byte)
#   payload : u8 cmd | u8 player_nr | u16 param | u8 scripted_call  (5 bytes)
# Total: 24 bytes on the wire.
def build(*,
          our_client_id: int = 0,
          cmd: int = 0,
          player_nr: int = 1,
          param: int = 0,
          scripted_call: bool = False,
          sync_step: int = 0,
          map_counter: int = 0,
          exec_flag: bool = False) -> bytes:
    body = struct.pack("<I", our_client_id)                  # network_command_t
    body += struct.pack("<II", sync_step, map_counter)       # network_world_command_t
    body += struct.pack("<B", 1 if exec_flag else 0)         # network_broadcast_world_command_t
    body += struct.pack("<BBHB", cmd & 0xFF, player_nr & 0xFF,
                        param & 0xFFFF, 1 if scripted_call else 0)
    return wire.pack_header(wire.NWC_CHG_PLAYER, body)


class ChgPlayerTest(wire.ServerTestCase, unittest.TestCase):

    def test_rdwr_bool_does_not_read_uninit_destination(self):
        """One well-formed NWC_CHG_PLAYER, heartbeat survives.  Under
        UBSAN the bad rdwr_bool aborts here before the consumer."""
        wire.assert_heartbeat(self.srv, build())
