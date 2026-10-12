#!/usr/bin/env python3
# Connections that have not sent NWC_JOIN have no nickname: the
# nickname in their socket_info_t is NULL.  Readers that assumed a
# nickname crashed the server for any peer that merely held a TCP
# connection open:
#
# * socket_list_t::get_all_nicks appended every connection's nickname
#   to a std::string.  nwc_nick_t::server_tools calls it when a client
#   finishes joining (WELCOME) and when one leaves (FAREWELL).  The
#   list goes out as an NWC_CHAT on channel -2, its message the
#   nicknames separated by tabs.
# * connection_info_t::operator== ran strcmp over the nicknames.
#   nwc_tool_t::clone records which clients acted for a company with
#   append_unique, so a second client acting for the same company from
#   the same address compared against the first.  Unlocking a company
#   with an empty password needs no join.

import struct
import unittest

from . import wire
from .test_auth_player import build as build_auth
from .test_tool import build as build_tool
from .test_tool_local import TOOL_ADD_MESSAGE, read_until_relayed


def chat_message(body: bytes) -> str:
    """Message text of an NWC_CHAT body: our_client_id u32, then the
    message as a u16-length string."""
    n = struct.unpack_from("<H", body, 4)[0]
    return body[6:6 + n].decode(errors="replace")


class NicklessConnectionTest(unittest.TestCase):

    def check_serving(self, srv: wire.Server, lurker: wire.Conn,
                      what: str) -> None:
        ticks = lurker.drain(1.0)
        self.assertIsNone(
            srv.proc.poll(),
            f"server exited {what}; stderr tail:\n" + srv.read_stderr_tail())
        self.assertFalse(lurker.eof,
                         f"server dropped the idle connection {what}")
        self.assertIn(wire.NWC_STEP, [i for i, _ in ticks],
                      f"server stopped ticking {what}")

    def test_join_with_connection_without_nickname(self):
        with wire.Server() as srv:
            lurker, _ = wire.connect_and_sync(srv)
            with lurker:
                conn, _, _ = wire.join(srv, "nick-list-joiner")
                with conn:
                    body = conn.wait_for(wire.NWC_CHAT, 5.0)
                    self.assertIsNotNone(
                        body, "no nickname list after a join; stderr tail:\n"
                        + srv.read_stderr_tail())
                    nicks = chat_message(body).split("\t")
                    self.assertIn("nick-list-joiner", nicks)
                    self.assertNotIn("", nicks,
                                     "connection without nickname listed")
                    self.check_serving(srv, lurker, "after a join")

    def test_leave_with_connection_without_nickname(self):
        with wire.Server() as srv:
            conn, _, _ = wire.join(srv, "nick-list-leaver")
            lurker, _ = wire.connect_and_sync(srv)
            with lurker:
                conn.sock.close()
                body = lurker.wait_for(wire.NWC_CHAT, 5.0)
                self.assertIsNotNone(
                    body, "no nickname list after a leave; stderr tail:\n"
                    + srv.read_stderr_tail())
                self.assertNotIn("nick-list-leaver",
                                 chat_message(body).split("\t"))
                self.check_serving(srv, lurker, "after a leave")

    def test_company_client_log_with_connection_without_nickname(self):
        def tool(wp: wire.WorldPoint, param: str) -> bytes:
            return build_tool(
                our_client_id=0, tool_id=TOOL_ADD_MESSAGE, player_nr=0,
                default_param=param.encode(), init=False, exec_flag=False,
                last_sync_step=wp.sync_step, checklist=wp.checklist,
                map_counter=wp.map_counter)

        with wire.Server() as srv:
            first, first_wp, _ = wire.join(srv, "company-log-first")
            with first:
                first.send(tool(first_wp, "0,first"))
                self.assertIsNotNone(
                    read_until_relayed(first, "0,first", 5.0),
                    "tool from the joined client was not relayed")

                # Company 0 has no password, so the empty hash unlocks it
                # without a join.
                lurker, lurker_wp = wire.connect_and_sync(srv)
                with lurker:
                    lurker.send(build_auth(player_nr=0))
                    auth = lurker.wait_for(wire.NWC_AUTH_PLAYER, 5.0)
                    self.assertIsNotNone(auth, "no reply to NWC_AUTH_PLAYER")
                    self.assertTrue(
                        struct.unpack("<H", auth[25:27])[0] & 1,
                        "company 0 not unlocked for the unjoined connection")

                    lurker.send(tool(lurker_wp, "0,lurker"))
                    self.assertIsNotNone(
                        read_until_relayed(lurker, "0,lurker", 5.0),
                        "tool from the unjoined connection was not "
                        "relayed; stderr tail:\n" + srv.read_stderr_tail())
                    self.check_serving(srv, lurker, "after its tool")


if __name__ == "__main__":
    unittest.main()
