#!/usr/bin/env python3
# Regenerate the well-formed smoke seeds under nettool/ and
# command_preauth/.  These exist so CI exercises the fuzzer wiring
# (build + libFuzzer init + dispatch) on every push, even when no real
# crash repro lives in the corpus yet.  They also act as starting
# points for active mutation campaigns — a campaign that begins from
# well-formed nwc_* packets reaches the deeper handler surface much
# faster than one starting from /dev/urandom.  Crash reproducers come
# from -minimize_crash=1 on actual fuzzer findings, not from this
# script.  Full corpus-commit policy: documentation/fuzz-corpora.md.

import struct
from pathlib import Path

NETWORK_VERSION = 1
HEADER_SIZE = 6

# Mirror network_cmd.h.  Keep numbering aligned with
# tools/nwc_protocol_test/wire.py.
NWC_GAMEINFO     = 1
NWC_NICK         = 2
NWC_CHAT         = 3
NWC_JOIN         = 4
NWC_SYNC         = 5
NWC_GAME         = 6
NWC_READY        = 7
NWC_TOOL         = 8
NWC_CHECK        = 9
NWC_PAKSETINFO   = 10
NWC_SERVICE      = 11
NWC_AUTH_PLAYER  = 12
NWC_CHG_PLAYER   = 13
NWC_SCENARIO     = 14
NWC_SCENARIO_RULES = 15
NWC_STEP         = 16

# nwc_service_t SRVC_* sub-id.
SRVC_LOGIN_ADMIN = 0

# Mirror simmenu.h.  Tool ids in nwc_tool_t are OR'd with the family bit.
GENERAL_TOOL        = 0x1000
TOOL_REMOVER        = 1
TOOL_RAISE_LAND     = 2
TOOL_BUILD_WAY      = 14
TOOL_BUILD_BRIDGE   = 15
TOOL_BUILD_ROADSIGN = 20


def packet(pkt_id: int, body: bytes) -> bytes:
    size = HEADER_SIZE + len(body)
    return struct.pack("<HHH", size, NETWORK_VERSION, pkt_id) + body


def nwc_str(s: bytes) -> bytes:
    return struct.pack("<H", len(s)) + s


def nwc_auth_player_body() -> bytes:
    # our_client_id (u32) + hash (20 bytes) + player_nr (u8) + player_unlocked (u16)
    return struct.pack("<I", 0) + b"\x00" * 20 + struct.pack("<BH", 0, 0)


def nwc_service_login_body() -> bytes:
    # our_client_id (u32) + flag (u32) + number (u32) + text (u16 length + bytes)
    return struct.pack("<III", 0, SRVC_LOGIN_ADMIN, 0) + nwc_str(b"")


def nwc_tool_body(tool_id: int, custom_data: bytes) -> bytes:
    # Wire layout, derived from the rdwr chain
    # (network_cmd.cc:network_command_t::rdwr;
    #  network_cmd_ingame.cc:network_world_command_t::rdwr +
    #                       network_broadcast_world_command_t::rdwr +
    #                       nwc_tool_t::rdwr;
    #  utils/checklist.cc:checklist_t::rdwr):
    #   network_command_t      our_client_id (u32)
    #   network_world_command  sync_step (u32) + map_counter (u32)
    #   network_broadcast..    exec (u8 bool)
    #   nwc_tool_t             last_sync_step (u32)
    #                          checklist: hash (u32) + seed (u32)
    #                                     + halt (u16) + line (u16) + cnvy (u16)
    #                          player_nr (u8)
    #                          pos.x (s16) + pos.y (s16) + pos.z (s8)
    #                          tool_id (u16) + wt (u16)
    #                          default_param (u16 len + bytes)
    #                          init (u8 bool)
    #                          tool_client_id (u32)
    #                          flags (u8)
    #                          callback_id (u32)
    #                          custom_data tail (consumed by
    #                          tool->rdwr_custom_data — fixed-size per
    #                          override, see network_cmd_ingame.cc:1133)
    body  = struct.pack("<I", 0)                  # client_id
    body += struct.pack("<II", 0, 0)              # sync_step, map_counter
    body += struct.pack("<B", 0)                  # exec
    body += struct.pack("<I", 0)                  # last_sync_step
    body += struct.pack("<IIHHH", 0, 0, 0, 0, 0)  # checklist
    body += struct.pack("<B", 0)                  # player_nr
    body += struct.pack("<hhb", 0, 0, 0)          # pos
    body += struct.pack("<HH", tool_id, 0)        # tool_id, wt
    body += nwc_str(b"")                          # default_param
    body += struct.pack("<B", 1)                  # init = true
    body += struct.pack("<I", 0)                  # tool_client_id
    body += struct.pack("<B", 0)                  # flags
    body += struct.pack("<I", 0)                  # callback_id
    body += custom_data
    return body


# Per-tool custom_data payloads for the four rdwr_custom_data overrides
# documented at network_cmd_ingame.cc:1133.  Sizes match the comment.

# tool_t::rdwr_custom_data (base — empty)
CUSTOM_BASE = b""

# two_click_tool_t::rdwr_custom_data (simmenu.cc:1341):
#   first_click_var (bool) + start.x (s16) + start.y (s16) + start.z (s8)
CUSTOM_TWO_CLICK = struct.pack("<Bhhb", 0, 0, 0, 0)

# tool_raise_lower_base_t::rdwr_custom_data (simtool.cc:1080):
#   cursor_corner (u8) — clamped against hex_corner_t::count
CUSTOM_RAISE_LOWER = struct.pack("<B", 0)

# tool_build_bridge_t::rdwr_custom_data (simtool.cc:3113):
#   two_click + ribi (u8)
CUSTOM_BRIDGE = CUSTOM_TWO_CLICK + struct.pack("<B", 0)

# tool_build_roadsign_t::rdwr_custom_data (simtool.cc:5365):
#   two_click + spacing (u8) + remove_intermediate (bool) + replace_other (bool)
CUSTOM_ROADSIGN = CUSTOM_TWO_CLICK + struct.pack("<BBB", 0, 0, 0)


def nwc_chat_body() -> bytes:
    # our_client_id (u32) + message (str) + player_nr (u8)
    #   + clientname (str) + destination (str) + channel_nr (u8)
    #   + pos.x (s16) + pos.y (s16)
    body  = struct.pack("<I", 0)
    body += nwc_str(b"hi")
    body += struct.pack("<B", 0)
    body += nwc_str(b"")
    body += nwc_str(b"")
    body += struct.pack("<Bhh", 0, 0, 0)
    return body


def nwc_chg_player_body() -> bytes:
    # network_broadcast_world_command_t header (4+4+4+1)
    #   + cmd (u8) + player_nr (u8) + param (u16) + scripted_call (u8)
    body  = struct.pack("<I", 0)
    body += struct.pack("<II", 0, 0)
    body += struct.pack("<B", 0)
    body += struct.pack("<BBHB", 0, 0, 0, 0)
    return body


def nwc_pakset_info_body() -> bytes:
    # our_client_id (u32) + flag (u8) + name (str) + has_info (u8 bool)
    body  = struct.pack("<I", 0)
    body += struct.pack("<B", 0)
    body += nwc_str(b"")
    body += struct.pack("<B", 0)
    return body


def nwc_gameinfo_body() -> bytes:
    # our_client_id (u32) + len (u32)
    return struct.pack("<II", 0, 0)


def nwc_nick_body() -> bytes:
    # our_client_id (u32) + nickname (str)
    return struct.pack("<I", 0) + nwc_str(b"alice")


def nwc_join_body() -> bytes:
    # nwc_nick_t::rdwr + client_id (u32) + answer (u8)
    return struct.pack("<I", 0) + nwc_str(b"alice") + struct.pack("<IB", 0, 0)


def nwc_sync_body() -> bytes:
    # network_world_command_t::rdwr + client_id (u32) + new_map_counter (u32)
    return struct.pack("<III", 0, 0, 0) + struct.pack("<II", 0, 0)


def nwc_game_body() -> bytes:
    # our_client_id (u32) + len (u32)
    return struct.pack("<II", 0, 0)


def nwc_ready_body() -> bytes:
    # our_client_id (u32) + sync_step (u32) + map_counter (u32) + checklist (14)
    return struct.pack("<III", 0, 0, 0) + struct.pack("<IIHHH", 0, 0, 0, 0, 0)


def nwc_check_body() -> bytes:
    # network_world_command_t::rdwr + checklist (14) + server_sync_step (u32)
    return (struct.pack("<III", 0, 0, 0)
            + struct.pack("<IIHHH", 0, 0, 0, 0, 0)
            + struct.pack("<I", 0))


def nwc_step_body() -> bytes:
    # network_world_command_t::rdwr (own rdwr is trivial)
    return struct.pack("<III", 0, 0, 0)


def nwc_scenario_body() -> bytes:
    # our_client_id (u32) + what (s16) + won (s16) + lost (s16)
    #   + function (str) + result (str)
    body  = struct.pack("<I", 0)
    body += struct.pack("<hhh", 0, 0, 0)
    body += nwc_str(b"")
    body += nwc_str(b"")
    return body


def nwc_scenario_rules_body() -> bytes:
    # network_broadcast_world_command_t::rdwr (4+4+4+1)
    #   + scenario_t::forbidden_t::rdwr:
    #       type (u8) + toolnr (s16) + waytype (s16) + parameter_hash (s32)
    #       + pos_nw.x/y (s16) + pos_se.x/y (s16)
    #       + hmin (s8) + hmax (s8) + error (str)
    #   + forbid (u8) + player_nr (s16)
    body  = struct.pack("<I", 0)
    body += struct.pack("<II", 0, 0)
    body += struct.pack("<B", 0)
    body += struct.pack("<BhhihhhhbB", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0)
    body += nwc_str(b"")
    body += struct.pack("<Bh", 0, 0)
    return body


def main() -> None:
    out_dir = Path(__file__).resolve().parent / "nettool"
    out_dir.mkdir(parents=True, exist_ok=True)
    (out_dir / "smoke_auth_player").write_bytes(
        packet(NWC_AUTH_PLAYER, nwc_auth_player_body())
    )
    (out_dir / "smoke_service_login").write_bytes(
        packet(NWC_SERVICE, nwc_service_login_body())
    )

    # One seed per nwc_tool_t rdwr_custom_data override (the four that
    # `network_cmd_ingame.cc:1133` enumerates), plus a tool that uses
    # only the empty base override.  Starting libFuzzer from well-formed
    # NWC_TOOL packets covering every tool family it can construct
    # short-circuits the wire-format discovery phase and gets the
    # mutator into the per-tool branch logic.
    preauth_tool_seeds = [
        ("smoke_tool_remover",        TOOL_REMOVER,        CUSTOM_BASE),
        ("smoke_tool_raise_land",     TOOL_RAISE_LAND,     CUSTOM_RAISE_LOWER),
        ("smoke_tool_build_way",      TOOL_BUILD_WAY,      CUSTOM_TWO_CLICK),
        ("smoke_tool_build_bridge",   TOOL_BUILD_BRIDGE,   CUSTOM_BRIDGE),
        ("smoke_tool_build_roadsign", TOOL_BUILD_ROADSIGN, CUSTOM_ROADSIGN),
    ]

    preauth_dir = Path(__file__).resolve().parent / "command_preauth"
    preauth_dir.mkdir(parents=True, exist_ok=True)
    for name, tool_id, custom in preauth_tool_seeds:
        (preauth_dir / name).write_bytes(
            packet(NWC_TOOL, nwc_tool_body(GENERAL_TOOL | tool_id, custom))
        )
    # One seed per remaining read_from_packet dispatch arm.  Some of
    # these (sync, game, check, ...) are guarded with a `failed()` on
    # the server side, but the rdwr still runs before that flag is
    # checked so the fuzz coverage is real.
    preauth_other_seeds = [
        ("smoke_gameinfo",       NWC_GAMEINFO,       nwc_gameinfo_body()),
        ("smoke_nick",           NWC_NICK,           nwc_nick_body()),
        ("smoke_chat",           NWC_CHAT,           nwc_chat_body()),
        ("smoke_join",           NWC_JOIN,           nwc_join_body()),
        ("smoke_sync",           NWC_SYNC,           nwc_sync_body()),
        ("smoke_game",           NWC_GAME,           nwc_game_body()),
        ("smoke_ready",          NWC_READY,          nwc_ready_body()),
        ("smoke_pakset_info",    NWC_PAKSETINFO,     nwc_pakset_info_body()),
        ("smoke_check",          NWC_CHECK,          nwc_check_body()),
        # nwc_service_t::rdwr only parses past the early return when
        # env_t::server is true (the harness sets it).  Otherwise the
        # service body is unreachable from this dispatch.
        ("smoke_service_login",  NWC_SERVICE,        nwc_service_login_body()),
        ("smoke_chg_player",     NWC_CHG_PLAYER,     nwc_chg_player_body()),
        ("smoke_scenario",       NWC_SCENARIO,       nwc_scenario_body()),
        ("smoke_scenario_rules", NWC_SCENARIO_RULES, nwc_scenario_rules_body()),
        ("smoke_step",           NWC_STEP,           nwc_step_body()),
    ]
    for name, pkt_id, body in preauth_other_seeds:
        (preauth_dir / name).write_bytes(packet(pkt_id, body))


if __name__ == "__main__":
    main()
