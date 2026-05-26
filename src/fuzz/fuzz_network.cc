/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 */

/*
 * libFuzzer harness for the network-command wire parser.
 *
 * The fuzzer's bytes go through the production wire path verbatim: we
 * stand up a unix socketpair, write the bytes into one end, close it
 * to signal EOF, hand the other end to `packet_t(SOCKET)`, and drive
 * `recv()` in a loop until the packet is ready or has failed.  This
 * means the fuzzer exercises the *exact* function the in-game network
 * thread runs against a remote peer — no parallel parser
 * implementation, no harness/production divergence.  Bonus: the
 * incremental-read state machine inside `recv()` also gets fuzzed.
 *
 * Links the nettool source subset, covering the parser surface that doesn't
 * require a `karte_t` / running game: base `network_command_t::rdwr`,
 * `nwc_auth_player_t::rdwr`, and `nwc_service_t::rdwr` (which in turn
 * exercises `socket_list_t::rdwr` and `address_list_t::rdwr`).
 *
 * Build with -fsanitize=fuzzer,address,undefined to catch the
 * memory/UB regressions a malformed wire packet can drive.
 */

#include <stddef.h>
#include <stdint.h>
#include <sys/socket.h>
#include <unistd.h>

#include "../simutrans/network/network_cmd.h"
#include "../simutrans/network/network_packet.h"
#include "../simutrans/simdebug.h"
#include "../simutrans/utils/log.h"


// nettool ships its own stub for read_from_packet; we need an
// equivalent because network_socket_list.cc references it from a path
// the fuzzer never reaches.
network_command_t *network_command_t::read_from_packet(packet_t *p)
{
	delete p;
	return NULL;
}


namespace {

network_command_t *make_nwc(uint16 id)
{
	switch (id) {
		case NWC_SERVICE:     return new nwc_service_t();
		case NWC_AUTH_PLAYER: return new nwc_auth_player_t();
		default:              return NULL;
	}
}

} // namespace


extern "C" int LLVMFuzzerInitialize(int * /*argc*/, char *** /*argv*/)
{
	// Silent logger: NULL filename + log_console=false → no I/O per
	// fuzz iteration, which keeps execs/sec high.
	dbg = new log_t(NULL, false, false, false, NULL, NULL);
	return 0;
}


extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	// Stand up a unix socketpair; write the fuzz bytes to one end and
	// close it.  The other end sees the bytes followed by EOF, which
	// drives `packet_t::recv()` to the same has_failed()/is_ready()
	// terminal state it would reach against a real remote peer.
	int sv[2];
	if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) != 0) {
		return 0;
	}
	if (size > 0) {
		ssize_t n = write(sv[0], data, size);
		(void)n;
	}
	close(sv[0]);

	packet_t *p = new packet_t(sv[1]);
	while (!p->has_failed()  &&  !p->is_ready()) {
		p->recv();
	}
	close(sv[1]);

	if (p->has_failed()  ||  !p->check_version()) {
		delete p;
		return 0;
	}

	network_command_t *nwc = make_nwc(p->get_id());
	if (nwc == NULL) {
		delete p;
		return 0;
	}

	// receive() takes ownership of the packet — stashed in
	// network_command_t::packet, freed by the destructor.
	nwc->receive(p);
	delete nwc;
	return 0;
}
