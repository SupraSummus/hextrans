#!/usr/bin/env python3
# NWC_CHG_PLAYER wire pin.  Regression coverage for:
#   - memory_rw_t::rdwr_bool no longer reading the uninitialised
#     `scripted_call` field on the load path (UBSAN trip pre-fix).
#   - nwc_chg_player_t::clone trusting the read_from_packet override
#     of our_client_id; a regression that let the wire value through
#     would trip socket_list_t::get_client's assert on OOR values.

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

    def test_client_id_sweep_does_not_crash(self):
        """NWC_CHG_PLAYER has no synchronous reply; heartbeat-after
        is all we get.  Sweep legitimate (0), in-range-impersonation
        (1), and out-of-range forged (0xFFFFFFFF) values — if the
        read_from_packet override regressed, the OOR value would trip
        get_client's assert and the in-range impersonation would
        diverge from the legitimate path's slot lookup.  Heartbeat
        can't prove the packet reached clone(), only that nothing
        crashed; the auth_player reply-equality test is the canary
        for parse-time silent swallowing."""
        for client_id in (0, 1, 0xFFFFFFFF):
            with self.subTest(our_client_id=client_id):
                wire.assert_heartbeat(self.srv, build(our_client_id=client_id))
