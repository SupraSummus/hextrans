/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 */

#include "fuzz_engine.h"

#include <stdlib.h>
#include <string.h>
#include <string>

#include "../simutrans/dataobj/environment.h"
#include "../simutrans/dataobj/settings.h"
#include "../simutrans/dataobj/tabfile.h"
#include "../simutrans/dataobj/translator.h"
#include "../simutrans/dataobj/pakset_manager.h"

#include "../simutrans/world/simcity.h"
#include "../simutrans/builder/vehikelbauer.h"
#include "../simutrans/tool/simmenu.h"
#include "../simutrans/simskin.h"
#include "../simutrans/simsound.h"
#include "../simutrans/simconvoi.h"
#include "../simutrans/simline.h"
#include "../simutrans/simhalt.h"

#include "../simutrans/display/simgraph.h"
#include "../simutrans/sys/simsys.h"

#include "../simutrans/simdebug.h"
#include "../simutrans/utils/log.h"


static const char *const HINT =
	"set SIMUTRANS_FUZZ_BASE to the simutrans data dir and SIMUTRANS_FUZZ_PAK "
	"to a pakset (run tools/test.py once to fetch pak64)";


// Mirrors the relevant slice of simu_main() (simmain.cc).
void fuzz_standup_engine()
{
	// Silent logger keeps per-iteration I/O out of the hot loop.
	dbg = new log_t(NULL, false, false, false, NULL, NULL);
	gfx = simgraph_select(SIMGRAPH_TYPE_NULL);

	const char *base = getenv("SIMUTRANS_FUZZ_BASE");
	if (base == NULL || base[0] == 0) base = "simutrans";
	const char *pak  = getenv("SIMUTRANS_FUZZ_PAK");
	if (pak == NULL || pak[0] == 0) pak = "pak";

	env_t::init();

	// base_dir / user_dir: keep everything under the data dir.
	strncpy(env_t::base_dir, base, sizeof(env_t::base_dir) - 2);
	env_t::base_dir[sizeof(env_t::base_dir) - 2] = 0;
	size_t bl = strlen(env_t::base_dir);
	if (bl == 0 || env_t::base_dir[bl - 1] != PATH_SEPARATOR[0]) {
		strcat(env_t::base_dir, PATH_SEPARATOR);
	}
	strcpy(env_t::user_dir, env_t::base_dir);

	if (dr_chdir(env_t::base_dir) != 0) {
		dbg->fatal("fuzz_standup_engine", "cannot chdir to '%s'; %s", env_t::base_dir, HINT);
	}

	// pak_dir is relative to base_dir, with a trailing separator;
	// pak_name is the leaf (some pakset config keys off it).
	env_t::pak_dir  = std::string(pak) + PATH_SEPARATOR;
	env_t::pak_name = pak;

	// Pull pakset-independent defaults from the base simuconf.
	tabfile_t simuconf;
	if (simuconf.open((std::string("config") + PATH_SEPARATOR + "simuconf.tab").c_str())) {
		env_t::default_settings.parse_simuconf(simuconf);
		simuconf.close();
	}

	// Single-threaded for determinism, and so the process stays
	// fork-safe (no threads holding locks across a fork).
	env_t::num_threads = 1;

	convoihandle_t::init(1024);
	linehandle_t::init(1024);
	halthandle_t::init(1024);

	sound_set_mute(true);

	if (!translator::load()) {
		dbg->fatal("fuzz_standup_engine", "no language files under '%s'; %s", env_t::base_dir, HINT);
	}
	// A language must be selected: karte_t::init -> init_custom_names
	// indexes lang_info[get_language()], and the default -1 is OOB.
	translator::set_language("en");

	stadt_t::cityrules_init();
	vehicle_builder_t::speedbonus_init();
	tool_t::init_menu();

	skinverwaltung_t::restore_all_skins();
	pakset_manager_t::load_pakset(false);
	skinverwaltung_t::save_all_skins();

	if (!tool_t::read_menu(env_t::pak_dir + "config" + PATH_SEPARATOR + "menuconf.tab")) {
		dbg->fatal("fuzz_standup_engine", "no menuconf.tab under '%s'; %s", env_t::pak_dir.c_str(), HINT);
	}
}
