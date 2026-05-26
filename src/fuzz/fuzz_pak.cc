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
 * we route them through `log_t::set_fatal_hook` to a `longjmp` back
 * into the per-iteration recovery point so they read as input
 * rejection instead.  See `AGENTS.md` → "fuzz_pak" for the
 * accompanying per-iteration-leak caveat.
 */

#include <setjmp.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "../simutrans/dataobj/pakset_manager.h"
#include "../simutrans/display/simgraph.h"
#include "../simutrans/simdebug.h"
#include "../simutrans/utils/log.h"


static jmp_buf fuzz_recovery;


static void fuzz_fatal_hook(const char * /*buffer*/)
{
	longjmp(fuzz_recovery, 1);
}


extern "C" int LLVMFuzzerInitialize(int * /*argc*/, char *** /*argv*/)
{
	dbg = new log_t(NULL, false, false, false, NULL, NULL);
	// image_reader_t calls gfx->register_image() on every IMG node;
	// bind the null renderer so it doesn't NPE.
	gfx = simgraph_select(SIMGRAPH_TYPE_NULL);
	log_t::set_fatal_hook(fuzz_fatal_hook);
	return 0;
}


extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	FILE *fp = fmemopen(const_cast<uint8_t *>(data), size, "rb");
	if (fp == NULL) {
		return 0;
	}

	if (setjmp(fuzz_recovery) == 0) {
		pakset_manager_t::load_pak_from_fp(fp, "<fuzz>");
	}

	fclose(fp);
	return 0;
}
