/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 */

/*
 * Fuzz harness for savegame loading.
 *
 * A multiplayer server sends its savegame to every client that joins,
 * so the loader parses untrusted bytes.  Each input is a whole save file,
 * taken the way karte_t::load(const char*) takes it: loadsave_t::rd_open
 * reads the header and picks the format, the same version checks apply,
 * and karte_t::load(loadsave_t*) builds the world from a buffered read.
 * The windows that function opens are left out, and so is its copy of the
 * loaded settings into env_t::default_settings.  A world that loads then
 * runs a few steps and is torn down, as a client runs it and later quits
 * or loads another game.
 *
 * The bytes reach rd_open through a memfd, so the whole file path runs
 * with nothing on disk.
 *
 * The game exits on dbg->fatal, and so does this harness, so a fatal is a
 * rejected input and nothing touches the half-built world afterwards.
 * That makes it one input per process: mutate with AFL++, which forks a
 * child per input after the standup when AFL_FUZZER_LOOPCOUNT=1, and
 * replay with libFuzzer one file per run.
 */

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

#include "fuzz_engine.h"

#include "../simutrans/dataobj/loadsave.h"
#include "../simutrans/world/simworld.h"
#include "../simutrans/siminteraction.h"
#include "../simutrans/simdebug.h"
#include "../simutrans/utils/log.h"


static const int STEPS_AFTER_LOAD = 4;

static karte_t       *welt;
static interaction_t *events; // sync_step polls it, and it holds welt's viewport
static char           save_path[32];
static int            save_fd;


static void fuzz_fatal_hook(const char * /*buffer*/)
{
	_exit(0);
}


static void new_world()
{
	welt = new karte_t();
	events = new interaction_t(welt->get_viewport());
	welt->set_eventmanager(events);
}


extern "C" int LLVMFuzzerInitialize(int * /*argc*/, char *** /*argv*/)
{
	fuzz_standup_engine();
	new_world();

	save_fd = memfd_create("fuzz.sve", 0);
	if (save_fd < 0) {
		dbg->fatal("fuzz_savegame", "memfd_create failed");
	}
	snprintf(save_path, sizeof(save_path), "/proc/self/fd/%d", save_fd);

	log_t::set_fatal_hook(fuzz_fatal_hook);
	return 0;
}


extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	if (ftruncate(save_fd, 0) != 0  ||  pwrite(save_fd, data, size, 0) != (ssize_t)size) {
		abort();
	}

	loadsave_t file;
	if (file.rd_open(save_path) != loadsave_t::FILE_STATUS_OK  ||  file.is_version_less(84, 6)) {
		return 0;
	}
	file.set_buffered(true);
	welt->load(&file);

	for (int i = 0; i < STEPS_AFTER_LOAD; i++) {
		welt->step_unattended();
	}
	delete events;
	delete welt;
	new_world();
	return 0;
}
