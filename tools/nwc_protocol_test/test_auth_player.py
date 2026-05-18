#!/usr/bin/env python3
# NWC_AUTH_PLAYER wire pin + D9 null-slot regression tripwire.
#
# nwc_auth_player_t::execute (src/simutrans/network/network_cmd_ingame.cc)
# previously reached `welt->get_player(player_nr)->access_password_hash()`
# with no NULL guard.  A peer-supplied player_nr in [2, 14] (slots that
# are NULL in the default starter map) crashed the server.  The fix
# treats a NULL slot like a wrong-password slot — same reply shape, no
# crash — so the byte-exact reply pin doubles as the regression check.

import struct

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
    assert len(hash20) == 20
    body = struct.pack("<I", our_client_id)
    body += hash20
    body += struct.pack("<B", player_nr & 0xFF)
    body += struct.pack("<H", player_unlocked & 0xFFFF)
    return wire.pack_header(wire.NWC_AUTH_PLAYER, body)


def parse(reply: bytes) -> dict:
    """Decode a NWC_AUTH_PLAYER packet into its fields.  The assertions
    here are the on-wire spec the test pins."""
    assert len(reply) == PACKET_SIZE, (
        f"length {len(reply)} != {PACKET_SIZE}; raw: {reply.hex()}"
    )
    size, version, pkt_id = wire.unpack_header(reply)
    assert size == PACKET_SIZE, f"header size={size}"
    assert version == wire.NETWORK_VERSION, f"header version={version}"
    assert pkt_id == wire.NWC_AUTH_PLAYER, (
        f"header id={pkt_id} (expected {wire.NWC_AUTH_PLAYER})"
    )
    return dict(
        our_client_id=struct.unpack("<I", reply[6:10])[0],
        hash=reply[10:30],
        player_nr=reply[30],
        player_unlocked=struct.unpack("<H", reply[31:33])[0],
    )


def check_reply(srv: wire.Server, *, player_nr: int,
                expected_unlocked: int,
                hash20: bytes = b"\x00" * 20) -> str | None:
    """Send one NWC_AUTH_PLAYER, assert on the byte-exact reply.
    Returns None on pass, or a failure description."""
    pkt = build(player_nr=player_nr, hash20=hash20)
    reply = wire.send_and_recv_exact(srv, pkt, PACKET_SIZE)
    if not reply:
        return ("no reply: server closed the connection without "
                "emitting any bytes. stderr tail:\n" + srv.read_stderr_tail())
    try:
        decoded = parse(reply)
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
    with wire.Server() as srv:
        return check_reply(srv, player_nr=3, expected_unlocked=0)


def test_auth_player_filled_slot_empty_password():
    """NWC_AUTH_PLAYER targeting public service (player_nr=1) with an
    all-zero hash matches the default empty password and unlocks the
    slot — reply carries bit 1 set in player_unlocked.  Doubles as the
    negative control: a filled slot must not crash, pre- or post-fix."""
    with wire.Server() as srv:
        return check_reply(srv, player_nr=1, expected_unlocked=1 << 1)


def test_auth_player_filled_slot_wrong_password():
    """NWC_AUTH_PLAYER targeting public service with a non-zero hash
    does not match — reply has player_unlocked=0.  This is the
    silent-fail shape the D9 fix mirrors for unfilled slots: from the
    client's POV, "wrong password" and "no such slot" are
    indistinguishable, so any future divergence would surface here."""
    with wire.Server() as srv:
        return check_reply(srv, player_nr=1, expected_unlocked=0,
                           hash20=b"\xff" * 20)


TESTS = {
    "test_auth_player_null_slot": test_auth_player_null_slot,
    "test_auth_player_filled_slot_empty_password":
        test_auth_player_filled_slot_empty_password,
    "test_auth_player_filled_slot_wrong_password":
        test_auth_player_filled_slot_wrong_password,
}
