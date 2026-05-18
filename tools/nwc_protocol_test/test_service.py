#!/usr/bin/env python3
# NWC_SERVICE wire pin + D5 pre-auth client-list memory-pressure
# regression tripwire.
#
# nwc_service_t::rdwr (src/simutrans/network/network_cmd.cc) is the
# admin-tool <-> server channel.  Pre-fix, the server-side rdwr was
# wire-symmetric, so a forged SRVC_GET_CLIENT_LIST with count=0xFFFFFFFF
# drove socket_list_t::rdwr through ~4 billion `new socket_info_t()`
# allocations before std::bad_alloc terminated the server.  No
# admin-auth gate fired: execute()'s gate only runs after rdwr returns.
#
# Fix: server-side reads of the list-shaped flags (SRVC_GET_CLIENT_LIST,
# SRVC_GET_BLACK_LIST) are now no-ops; only the saving (response) leg
# touches socket_list_t / address_list_t.  The wire-controlled count
# never reaches the allocator.
#
# These tests forge the same packet the D5 PoC used and assert the
# server keeps ticking — heartbeat continues, new TCP connections still
# succeed — instead of OOM-crashing within seconds.

import struct

from . import wire


SRVC_LOGIN_ADMIN     = 0
SRVC_ANNOUNCE_SERVER = 1
SRVC_GET_CLIENT_LIST = 2
SRVC_GET_BLACK_LIST  = 5


# nwc_service_t wire layout (NETWORK_VERSION=1), trimmed to the
# request leg (admin -> server).  Body for the list-shaped flags
# pre-fix used to continue with a u32 count; post-fix the server
# ignores any trailing bytes on the loading side.
def build(*, flag: int, number: int = 0, count: int = 0,
          our_client_id: int = 0) -> bytes:
    body = struct.pack("<I", our_client_id)
    body += struct.pack("<II", flag, number)
    # Append the wire-controlled count for the list-shaped flags.
    # The server must ignore this post-fix; pre-fix it drove the
    # allocator.  We always send it so the packet shape exercises the
    # same code path as the D5 PoC, regardless of flag.
    body += struct.pack("<I", count)
    return wire.pack_header(wire.NWC_SERVICE, body)


# ---- Test cases --------------------------------------------------------


def test_service_get_client_list_huge_count_does_not_crash():
    """D5: pre-auth SRVC_GET_CLIENT_LIST with count=0xFFFFFFFF must
    not crash the server.  Pre-fix, socket_list_t::rdwr ran a 2^32
    allocation loop until std::bad_alloc; post-fix, the server's
    loading branch is a no-op so the count is never read."""
    with wire.Server() as srv:
        pkt = build(flag=SRVC_GET_CLIENT_LIST, count=0xFFFFFFFF)
        return wire.expect_heartbeat(srv, pkt)


def test_service_get_black_list_huge_count_does_not_crash():
    """D5 sibling: SRVC_GET_BLACK_LIST has the same shape and the
    same pre-fix unbounded allocation through address_list_t::rdwr.
    The fix patched both flags together; this test pins both halves."""
    with wire.Server() as srv:
        pkt = build(flag=SRVC_GET_BLACK_LIST, count=0xFFFFFFFF)
        return wire.expect_heartbeat(srv, pkt)


def test_service_unknown_flag_does_not_crash():
    """Generic negative-control: a NWC_SERVICE packet with a flag the
    server doesn't recognise must also be absorbed silently.  Catches
    a regression that swaps "no-op on loading" for "fatal on
    loading" — the post-fix path treats malformed pre-auth
    NWC_SERVICE the same way it treats well-formed ones."""
    with wire.Server() as srv:
        pkt = build(flag=0xDEAD, count=0xFFFFFFFF)
        return wire.expect_heartbeat(srv, pkt)


TESTS = {
    "test_service_get_client_list_huge_count_does_not_crash":
        test_service_get_client_list_huge_count_does_not_crash,
    "test_service_get_black_list_huge_count_does_not_crash":
        test_service_get_black_list_huge_count_does_not_crash,
    "test_service_unknown_flag_does_not_crash":
        test_service_unknown_flag_does_not_crash,
}
