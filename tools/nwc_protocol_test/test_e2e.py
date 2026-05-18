#!/usr/bin/env python3
# Black-box e2e tests for the multiplayer wire protocol implemented in
# src/simutrans/network/network_cmd*.cc.
#
# Each test spawns a headless simutrans server, drives a single
# hand-rolled packet over TCP, and asserts on the parsed reply.  The
# tests serve two overlapping purposes:
#
#   1. **Protocol pin.**  Reply shapes for NWC_AUTH_PLAYER and the
#      heartbeat NWC_STEP stream are documented in network_cmd.h but
#      only enforced by what the server actually emits.  These tests
#      freeze them on the wire so a future refactor that drops a
#      field, reorders one, or skips the reply entirely surfaces as a
#      CI failure rather than as a silent client compatibility break.
#
#   2. **Security regression tripwire.**  Several network fixes
#      manifest on the wire as "server consumes the packet then
#      crashes / closes the socket without replying".  The
#      heartbeat-after-malicious-input assertion catches that whole
#      class — see test_auth_player.py (D9), test_tool.py (D8) and
#      test_service.py (D5) for the per-finding wiring.
#
# Layout (one file per NWC_* command):
#
#   wire.py              -- shared Server harness, header pack/unpack,
#                           stream parser, heartbeat assertion, CLI runner.
#   test_<cmd>.py        -- per-command build() / parse() / TESTS.
#   test_e2e.py          -- this file: aggregates each module's TESTS
#                           and delegates to wire.run_tests().  Adding
#                           a new NWC_* pin means dropping in another
#                           test_<cmd>.py and one import line below.
#
# Run:
#   python3 -m tools.nwc_protocol_test.test_e2e            # all tests
#   python3 -m tools.nwc_protocol_test.test_e2e auth       # name filter
#
# Requires:
#   - A built simutrans binary (cmake or autoconf).
#   - simutrans/pak/ populated with a pak64-compatible pakset.
#   - tests/empty-16x16.sve in the tree (default starter map; leaves
#     player slots 2..14 unfilled, which the D9 NULL-slot test relies
#     on).

import sys

from . import wire
from . import test_auth_player
from . import test_service
from . import test_tool


TESTS: dict = {}
for module in (test_auth_player, test_tool, test_service):
    overlap = TESTS.keys() & module.TESTS.keys()
    assert not overlap, f"duplicate test name(s): {overlap}"
    TESTS.update(module.TESTS)


if __name__ == "__main__":
    wire.run_tests(TESTS, sys.argv)
