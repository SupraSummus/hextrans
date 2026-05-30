/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 */

/*
 * libFuzzer harness for the .pak descriptor reader.
 *
 * Fuzz bytes go in via `fmemopen` and are handed to
 * `pakset_manager_t::load_pak_from_fp` — the same entry the in-game
 * loader uses per file after `dr_fopen`.  Readers self-register at
 * static init via `OBJ_READER_DEF`, so the type table is populated
 * before `LLVMFuzzerTestOneInput` runs.
 *
 * Defensive `dbg->fatal` sites inside the readers would otherwise
 * abort on bad input and drown real findings under expected aborts;
 * we route them through `log_t::set_fatal_hook` to a C++ exception
 * that unwinds back to the per-iteration recovery point, so they read
 * as input rejection instead.  Throwing (rather than longjmp) runs the
 * destructors of stack objects live across the fatal — notably the
 * reader's `node_body` buffer — so they don't leak under detect_leaks=1.
 */

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "../simutrans/dataobj/pakset_manager.h"
#include "../simutrans/display/simgraph.h"
#include "../simutrans/simdebug.h"
#include "../simutrans/utils/log.h"


namespace { struct fuzz_fatal {}; }


static void fuzz_fatal_hook(const char * /*buffer*/)
{
	throw fuzz_fatal{};
}


extern "C" int LLVMFuzzerInitialize(int * /*argc*/, char *** /*argv*/)
{
	dbg = new log_t(NULL, false, false, false, NULL, NULL);
	// image_reader_t calls gfx->register_image() on every IMG node;
	// bind the null renderer so it doesn't NPE.
	gfx = simgraph_select(SIMGRAPH_TYPE_NULL);
	log_t::set_fatal_hook(fuzz_fatal_hook);
	// Descriptor tracking is compiled in via TRACK_DESCRIPTORS (always set on
	// this target); obj_desc_t::operator new/delete record every node this
	// process builds, ready for the per-iteration free_all_descriptors below.
	return 0;
}


extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	FILE *fp = fmemopen(const_cast<uint8_t *>(data), size, "rb");
	if (fp == NULL) {
		return 0;
	}

	try {
		pakset_manager_t::load_pak_from_fp(fp, "<fuzz>");
	}
	catch (const fuzz_fatal &) {
		// reader rejected the input via dbg->fatal; the throw unwound the
		// reader stack (freeing node_body buffers etc.) on the way here.
	}

	// Delete the descriptors this input built (including those orphaned by
	// a fatal unwinding out of a reader) and drop the load registries, so the
	// next iteration starts from a clean slate and LeakSanitizer flags only
	// genuinely new leaks rather than this baseline retention.
	pakset_manager_t::free_all_descriptors();

	fclose(fp);
	return 0;
}
