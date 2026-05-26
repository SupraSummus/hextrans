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
 * Compiles with `NETTOOL=1`, covering every wire-parser surface that
 * doesn't depend on `karte_t` / a running game: dispatched through the
 * real `network_command_t::read_from_packet`, which gives every NWC_*
 * type's rdwr() (except scenario and pakset, which pull in the script
 * VM and pakset comparator) a fuzz path.
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

	// read_from_packet takes ownership of p: it dispatches on the wire
	// id, constructs the matching nwc, calls receive(p) (which calls
	// rdwr()), and frees both on failure.  On success it returns the
	// nwc, which we delete.
	network_command_t *nwc = network_command_t::read_from_packet(p);
	delete nwc;
	return 0;
}
