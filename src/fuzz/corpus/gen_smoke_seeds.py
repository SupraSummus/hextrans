#!/usr/bin/env python3
# Regenerate the well-formed smoke seeds under network/.  These exist
# so CI exercises the fuzzer wiring (build + libFuzzer init + dispatch)
# on every push, even when no real crash repro lives in the corpus
# yet.  Crash reproducers come from -minimize_crash=1 on actual fuzzer
# findings, not from this script.

import struct
from pathlib import Path

NETWORK_VERSION = 1
HEADER_SIZE = 6

# Mirror network_cmd.h.  Keep numbering aligned with
# tools/nwc_protocol_test/wire.py.
NWC_SERVICE      = 11
NWC_AUTH_PLAYER  = 12

# nwc_service_t SRVC_* sub-id.
SRVC_LOGIN_ADMIN = 0


def packet(pkt_id: int, body: bytes) -> bytes:
    size = HEADER_SIZE + len(body)
    return struct.pack("<HHH", size, NETWORK_VERSION, pkt_id) + body


def nwc_auth_player_body() -> bytes:
    # our_client_id (u32) + hash (20 bytes) + player_nr (u8) + player_unlocked (u16)
    return struct.pack("<I", 0) + b"\x00" * 20 + struct.pack("<BH", 0, 0)


def nwc_service_login_body() -> bytes:
    # our_client_id (u32) + flag (u32) + number (u32) + text (u16 length + bytes)
    return struct.pack("<III", 0, SRVC_LOGIN_ADMIN, 0) + struct.pack("<H", 0)


def main() -> None:
    out_dir = Path(__file__).resolve().parent / "network"
    out_dir.mkdir(parents=True, exist_ok=True)
    (out_dir / "smoke_auth_player").write_bytes(
        packet(NWC_AUTH_PLAYER, nwc_auth_player_body())
    )
    (out_dir / "smoke_service_login").write_bytes(
        packet(NWC_SERVICE, nwc_service_login_body())
    )


if __name__ == "__main__":
    main()
