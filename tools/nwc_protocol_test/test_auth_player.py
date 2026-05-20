#!/usr/bin/env python3
# NWC_AUTH_PLAYER wire pin + tripwire for the NULL-player-slot crash
# in nwc_auth_player_t::execute (src/simutrans/network/network_cmd_ingame.cc).
# Pre-fix, a peer-supplied player_nr in [2, 14] dereferenced a NULL
# player slot in the default starter map.  The fix returns the same
# wrong-password reply shape, so a byte-exact reply pin doubles as
# the regression check.

import struct
import unittest

from . import wire


# nwc_auth_player_t wire layout (NETWORK_VERSION=1):
#   header  : u16 size | u16 version | u16 id          (6 bytes)
#   base    : u32 our_client_id                        (4 bytes)
#   payload : u8[20] hash | u8 player_nr | u16 player_unlocked  (23 bytes)
# Total: 33 bytes both ways.
PACKET_SIZE = 33


def build(*, player_nr: int,
          our_client_id: int = 0,
          hash20: bytes = b"\x00" * 20,
          player_unlocked: int = 0) -> bytes:
    if len(hash20) != 20:
        raise ValueError(f"hash20 must be 20 bytes, got {len(hash20)}")
    body = struct.pack("<I", our_client_id)
    body += hash20
    body += struct.pack("<B", player_nr & 0xFF)
    body += struct.pack("<H", player_unlocked & 0xFFFF)
    return wire.pack_header(wire.NWC_AUTH_PLAYER, body)


class AuthPlayerTest(wire.ServerTestCase, unittest.TestCase):

    def check_reply(self, *, player_nr: int, expected_unlocked: int,
                    hash20: bytes = b"\x00" * 20) -> None:
        pkt = build(player_nr=player_nr, hash20=hash20)
        reply = wire.send_and_recv_exact(self.srv, pkt, PACKET_SIZE)
        if len(reply) != PACKET_SIZE:
            self.fail(
                f"reply length {len(reply)} != {PACKET_SIZE}; "
                f"raw: {reply.hex()}; stderr tail:\n"
                + self.srv.read_stderr_tail()
            )
        size, version, pkt_id = wire.unpack_header(reply)
        self.assertEqual(size, PACKET_SIZE)
        self.assertEqual(version, wire.NETWORK_VERSION)
        self.assertEqual(pkt_id, wire.NWC_AUTH_PLAYER)
        player_unlocked = struct.unpack("<H", reply[31:33])[0]
        self.assertEqual(
            player_unlocked, expected_unlocked,
            f"player_unlocked={player_unlocked:#x}, "
            f"expected {expected_unlocked:#x}",
        )
        self.assertTrue(
            self.srv.alive(),
            "reply was well-formed but server died after",
        )

    def test_null_slot(self):
        """Targeting an unfilled slot must return the wrong-password
        shape (player_unlocked=0), not crash inside
        welt->get_player(...)->access_password_hash()."""
        self.check_reply(player_nr=3, expected_unlocked=0)

    def test_filled_slot_empty_password(self):
        """Public service (slot 1) with an all-zero hash matches the
        default empty password — bit 1 set in player_unlocked."""
        self.check_reply(player_nr=1, expected_unlocked=1 << 1)

    def test_filled_slot_wrong_password(self):
        """Wrong password on a filled slot returns player_unlocked=0
        — the silent-fail shape the NULL-slot path now mirrors."""
        self.check_reply(player_nr=1, expected_unlocked=0,
                         hash20=b"\xff" * 20)

    def test_forged_client_id_yields_legitimate_reply(self):
        """nwc_auth_player_t::execute reads the override-stamped
        our_client_id, not the wire value (b900cf7b — "never trust
        wire-supplied our_client_id"), so every forged id must
        reproduce the our_client_id=0 reply byte-for-byte.  The
        baseline send_and_recv_exact also doubles as a liveness
        check: if parse-time rejection ever swallowed AUTH_PLAYER
        packets, the baseline would time out before any subTest ran.
        Multiple forged values guard against one being benign by
        coincidence."""
        pkt = build(player_nr=1, our_client_id=0)
        legitimate = wire.send_and_recv_exact(self.srv, pkt, PACKET_SIZE)
        self.assertEqual(
            len(legitimate), PACKET_SIZE,
            f"baseline reply length {len(legitimate)} != {PACKET_SIZE}; "
            f"raw: {legitimate.hex()}; stderr tail:\n"
            + self.srv.read_stderr_tail(),
        )
        for forged in (1, 0x80000000, 0xDEADBEEF, 0xFFFFFFFF):
            with self.subTest(our_client_id=forged):
                pkt = build(player_nr=1, our_client_id=forged)
                reply = wire.send_and_recv_exact(self.srv, pkt, PACKET_SIZE)
                self.assertEqual(
                    reply, legitimate,
                    f"forged our_client_id={forged:#x} reply diverged "
                    f"from the our_client_id=0 baseline; "
                    f"forged: {reply.hex()} baseline: {legitimate.hex()}",
                )
                self.assertTrue(
                    self.srv.alive(),
                    f"reply matched but server died after forged "
                    f"our_client_id={forged:#x}; stderr tail:\n"
                    + self.srv.read_stderr_tail(),
                )
