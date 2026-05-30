/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 */

/*
 * libFuzzer harness for the pre-auth network-command surface.
 *
 * Drives fuzz bytes through `packet_t::recv()` → `read_from_packet`
 * like fuzz_nettool, but links the full simutrans source set so the
 * dispatch reaches `NWC_TOOL` and the other nwc_* types outside
 * NETTOOL=1.  On `NWC_TOOL` the harness then calls `init_tool()`
 * directly — the same call `nwc_tool_t::clone()` makes before its
 * auth check, exposing `create_tool(attacker_tool_id)`
 * and the per-tool `rdwr_custom_data` overrides to attacker bytes.
 * See the "Pre-auth parse surface" note above `nwc_tool_t::init_tool`
 * in network_cmd_ingame.cc for the current overrides.
 *
 * No `karte_t`, no pakset.  Standup is `dbg` + null `gfx`, plus the
 * minimum global state a real server exposes to the wire parser:
 * `env_t::server` set (so server-side rdwr branches enable), and the
 * per-iteration sender socket registered via `socket_list_t::add_client`
 * (so paths that look the sender up don't trip an assert).
 * `do_command` / `init` / `work` need a world and live in
 * fuzz_command.cc instead.
 *
 * `dbg->fatal` is routed through a C++ throw so an input-driven fatal
 * reads as rejection (same recovery idiom as fuzz_pak).  The production
 * parse path is exception-safe — `read_from_packet` owns the in-flight
 * command via a unique_ptr, so unwinding frees it and its packet — so
 * the harness just calls the real entry point and catches.
 * `FUZZ_COMMAND_PREAUTH_FATAL_ABORTS=1` flips the hook to abort for
 * security-focused sweeps.
 */

#include <memory>
#include <stddef.h>
#include <stdint.h>
#include <sys/socket.h>
#include <unistd.h>

#include "../simutrans/dataobj/environment.h"
#include "../simutrans/display/simgraph.h"
#include "../simutrans/network/network_cmd.h"
#include "../simutrans/network/network_cmd_ingame.h"
#include "../simutrans/network/network_packet.h"
#include "../simutrans/network/network_socket_list.h"
#include "../simutrans/simdebug.h"
#include "../simutrans/utils/log.h"


#ifndef FUZZ_COMMAND_PREAUTH_FATAL_ABORTS
#define FUZZ_COMMAND_PREAUTH_FATAL_ABORTS 0
#endif


// `env_t::server` is a const ref into this global; we set it
// indirectly so the const interface stays untouched.
extern uint16 network_server_port;


namespace { struct fuzz_fatal {}; }


static void fuzz_fatal_hook(const char * /*buffer*/)
{
#if FUZZ_COMMAND_PREAUTH_FATAL_ABORTS
	log_t::set_fatal_hook(NULL);
	dbg->fatal("fuzz_command_preauth", "pre-auth fatal (see prior message)");
#else
	throw fuzz_fatal{};
#endif
}


extern "C" int LLVMFuzzerInitialize(int * /*argc*/, char *** /*argv*/)
{
	dbg = new log_t(NULL, false, false, false, NULL, NULL);
	// Tool construction / static-init paths reach the renderer
	// pointer; bind the null backend so they don't NPE.
	gfx = simgraph_select(SIMGRAPH_TYPE_NULL);

	// Model an ingame multiplayer server.  Without this `env_t::server`
	// stays false and `nwc_service_t::rdwr` short-circuits at
	// `network_cmd.cc:140` before the body parse; the post-rdwr
	// `packet->failed()` calls in nwc_nick_t / nwc_chat_t / nwc_sync_t /
	// nwc_game_t / nwc_check_t are also gated on this flag, but those
	// fire *after* the fields have been read so coverage is preserved.
	network_server_port = 1;

	log_t::set_fatal_hook(fuzz_fatal_hook);
	return 0;
}


extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	// Drive bytes through packet_t::recv() via an EOF-closed
	// socketpair (same shape as fuzz_nettool).
	int sv[2];
	if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) != 0) {
		return 0;
	}
	if (size > 0) {
		ssize_t n = write(sv[0], data, size);
		(void)n;
	}
	close(sv[0]);

	// Register the receiving end as a connected client.  Multiple rdwr
	// paths look the sender up in socket_list_t and assert; in a real
	// server the socket has been added on connect.  A genuine
	// `get_client(get_client_id(sender))` OOB on a missing sender is
	// tracked separately — TODO.md → "Pre-auth get_client lookup gap".
	socket_list_t::add_client(sv[1]);

	packet_t *p = new packet_t(sv[1]);
	while (!p->has_failed()  &&  !p->is_ready()) {
		p->recv();
	}

	// Call the real read_from_packet (it consumes p) and hold the result
	// in a unique_ptr, so a fatal-turned-throw — in the parse, or in the
	// NWC_TOOL init_tool() path clone() runs pre-auth — unwinds leak-clean.
	try {
		std::unique_ptr<network_command_t> nwc(network_command_t::read_from_packet(p));
		if (nwc != nullptr  &&  nwc->get_id() == NWC_TOOL) {
			static_cast<nwc_tool_t *>(nwc.get())->init_tool();
		}
	}
	catch (const fuzz_fatal &) {
		// rejected via a throwing dbg->fatal; the unwind already freed it.
	}

	// remove_client closes the socket via network_close_socket(); no
	// explicit close(sv[1]) here would be a double-close.  reset() then
	// deletes the (now inactive) slot so the per-iteration accounting
	// balances — without it socket_list_t::list retains one socket_info_t
	// for the process lifetime and detect_leaks=1 flags the standing slot.
	socket_list_t::remove_client(sv[1]);
	socket_list_t::reset();

	return 0;
}
