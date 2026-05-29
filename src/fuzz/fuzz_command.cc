/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 */

/*
 * libFuzzer harness for network command *handling* — the surface past
 * the wire parser fuzz_network.cc covers.
 *
 * A multiplayer client can send a well-formed packet whose handling
 * crashes the server.  The target is nwc_tool_t::do_command
 * (network_cmd_ingame.cc), which ends in tool->init/work against the
 * live world.  Tool behaviour depends on the world state at `pos`, so
 * the input is a *sequence* of commands applied to one world: each
 * reaches deeper but still-reachable state through the same interface a
 * real client uses, so every crash is reproducible by an actual client.
 *
 * Like fuzz_pak.cc it links the full simutrans source set (tools self-
 * register; world standup needs the engine) and routes dbg->fatal
 * through a longjmp so input-driven fatals read as rejection.  A
 * command-reachable dbg->fatal is itself a candidate server-crash bug;
 * FUZZ_COMMAND_FATAL_ABORTS=1 turns those into hard findings instead.
 *
 * Needs a real pakset + base files on disk (it renders nothing, but the
 * tools read descriptors): SIMUTRANS_FUZZ_BASE → data dir holding
 * config/ and text/ (default "simutrans"); SIMUTRANS_FUZZ_PAK → pakset
 * dir under it (default "pak"; run tools/test.py once to fetch pak64).
 * Run with ASAN_OPTIONS=detect_leaks=0 — per-world teardown is
 * incomplete and the longjmp path drops the in-flight command, so leak
 * counts are noise here; ASAN/UBSAN are the real signal.  Why a real
 * campaign wants a fork-after-init engine rather than in-process
 * per-input world reset: see TODO.md.
 */

#include <setjmp.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <string>

#include "../simutrans/dataobj/environment.h"
#include "../simutrans/dataobj/settings.h"
#include "../simutrans/dataobj/tabfile.h"
#include "../simutrans/dataobj/translator.h"
#include "../simutrans/dataobj/pakset_manager.h"
#include "../simutrans/dataobj/koord3d.h"

#include "../simutrans/world/simworld.h"
#include "../simutrans/world/simcity.h"
#include "../simutrans/builder/vehikelbauer.h"
#include "../simutrans/tool/simmenu.h"
#include "../simutrans/simskin.h"
#include "../simutrans/simsound.h"
#include "../simutrans/simconvoi.h"
#include "../simutrans/simline.h"
#include "../simutrans/simhalt.h"
#include "../simutrans/player/simplay.h"

#include "../simutrans/display/simgraph.h"
#include "../simutrans/sys/simsys.h"

#include "../simutrans/network/network.h"
#include "../simutrans/network/network_cmd_ingame.h"

#include "../simutrans/simdebug.h"
#include "../simutrans/utils/log.h"


// Set to 1 to treat any command-driven dbg->fatal as a hard finding
// (re-raise so libFuzzer/ASAN records a crash) instead of recovering and
// continuing.  Off by default so hex-port tripwires and not-yet-ported
// paths don't drown the run; flip it for a focused fatal-reachability
// sweep.
#ifndef FUZZ_COMMAND_FATAL_ABORTS
#define FUZZ_COMMAND_FATAL_ABORTS 0
#endif

// Cap commands per input so one pathological sequence can't stall the
// fuzzer; the sequence shape is what reaches deep state, not raw length.
static const int MAX_COMMANDS_PER_INPUT = 32;

// Small map keeps the per-input world reset cheap.
static const sint32 FUZZ_MAP_SIZE = 32;


static jmp_buf       fuzz_recovery;
static volatile bool fuzz_in_iteration = false;


static void fuzz_fatal_hook(const char * /*buffer*/)
{
#if FUZZ_COMMAND_FATAL_ABORTS
	// Re-raise: let the default fatal path abort so the input is
	// recorded as a crashing case.
	log_t::set_fatal_hook(NULL);
	dbg->fatal("fuzz_command", "command-reachable fatal (see prior message)");
#else
	if (fuzz_in_iteration) {
		longjmp(fuzz_recovery, 1);
	}
	// Fatal during one-time standup (misconfigured pak/base dirs):
	// nothing to recover to — fall through to the default abort.
#endif
}


namespace {

// Minimal FuzzedDataProvider-style reader.  Consumes from the front;
// returns zeros once exhausted so decoding never reads OOB.
struct byte_reader_t {
	const uint8_t *p;
	size_t         n;

	byte_reader_t(const uint8_t *d, size_t s) : p(d), n(s) {}

	uint8_t u8()
	{
		if (n == 0) return 0;
		n--;
		return *p++;
	}

	uint16_t u16() { uint16_t lo = u8(); return lo | (uint16_t(u8()) << 8); }

	// length-prefixed string for default_param
	std::string str()
	{
		uint8_t len = u8();
		std::string s;
		s.reserve(len);
		for (uint8_t i = 0; i < len; i++) {
			s.push_back((char)u8());
		}
		return s;
	}

	bool empty() const { return n == 0; }
};


// Stand up the engine far enough that create_tool() yields real tools
// and karte_t::init() can build a world.  Mirrors the relevant slice of
// simu_main() (simmain.cc) — see that function for the canonical order.
// Returns false only if dirs are unusable; genuine setup failures fatal
// (the hook lets them abort, since there is no per-iteration recovery
// point during standup).
bool standup_engine()
{
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
		dbg->warning("standup_engine", "cannot chdir to base_dir '%s'", env_t::base_dir);
		return false;
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
	// fork-safe for a future fork-after-init engine (no threads holding
	// locks across the fork).
	env_t::num_threads = 1;

	convoihandle_t::init(1024);
	linehandle_t::init(1024);
	halthandle_t::init(1024);

	sound_set_mute(true);

	if (!translator::load()) {
		dbg->warning("standup_engine", "no language files under '%s' — check SIMUTRANS_FUZZ_BASE", env_t::base_dir);
		return false;
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
		dbg->warning("standup_engine", "no menuconf.tab under '%s' — is SIMUTRANS_FUZZ_PAK a real pakset?", env_t::pak_dir.c_str());
		return false;
	}

	return true;
}


// Decode and apply a fuzzed sequence of nwc_tool_t commands to a live world.
void apply_command_sequence(karte_t *welt, const uint8_t *data, size_t size)
{
	byte_reader_t r(data, size);

	for (int cmd_count = 0; cmd_count < MAX_COMMANDS_PER_INPUT && !r.empty(); cmd_count++) {
		uint16_t    tool_id = r.u16();
		uint8_t     pl      = r.u8() & 1;            // public(1)/human(0)
		sint16      x       = (sint16)((r.u8() % (FUZZ_MAP_SIZE + 4)) - 2); // allow slight OOB
		sint16      y       = (sint16)((r.u8() % (FUZZ_MAP_SIZE + 4)) - 2);
		std::string param   = r.str();
		bool        is_init = (r.u8() & 1) != 0;

		tool_t *t = create_tool(tool_id);
		if (t == NULL) {
			continue; // invalid/dialog id the dispatcher rejects
		}
		t->set_default_param(param.empty() ? NULL : param.c_str());

		// Tools that keep game state aren't broadcast (clone() drops
		// them), so they never reach a server do_command — skip to stay
		// faithful to the real handler surface.
		const bool keeps = is_init ? t->is_init_keeps_game_state()
		                           : t->is_work_keeps_game_state();
		if (keeps) {
			delete t;
			continue;
		}

		player_t *player = welt->get_player(pl);

		fuzz_in_iteration = true;
		if (setjmp(fuzz_recovery) == 0) {
			// Constructor copies tool_id / default_param / custom_data
			// off `t` (reading the global world); do_command then
			// re-creates the tool from that and runs init/work.
			nwc_tool_t cmd(player, t, koord3d(x, y, 0), 0,
			               welt->get_map_counter(), is_init);
			cmd.do_command(welt);
			delete t;
		}
		else {
			// Recovered from a command-reachable fatal; `t` and the
			// in-flight command leak (detect_leaks=0).  Keep going.
		}
		fuzz_in_iteration = false;
	}
}

} // namespace


extern "C" int LLVMFuzzerInitialize(int * /*argc*/, char *** /*argv*/)
{
	// Silent logger keeps per-iteration I/O out of the hot loop.
	dbg = new log_t(NULL, false, false, false, NULL, NULL);
	gfx = simgraph_select(SIMGRAPH_TYPE_NULL);

	if (!standup_engine()) {
		// Misconfigured environment: better to fail loudly here than
		// to "fuzz" an engine that can't build a world.
		dbg->fatal("LLVMFuzzerInitialize",
			"engine standup failed; set SIMUTRANS_FUZZ_BASE to the simutrans "
			"data dir and SIMUTRANS_FUZZ_PAK to a pakset (run tools/test.py "
			"once to fetch pak64)");
	}

	// Install the recovery hook only after standup, so standup errors
	// abort normally rather than longjmp to an unset target.
	log_t::set_fatal_hook(fuzz_fatal_hook);
	return 0;
}


extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	// Fresh world per input.  Generation passes that don't affect the
	// handler surface (cities, factories, tourist attractions) are off
	// to keep the reset cheap.
	settings_t sets = env_t::default_settings;
	sets.set_size(FUZZ_MAP_SIZE, FUZZ_MAP_SIZE);
	sets.set_city_count(0);
	sets.set_factory_count(0);
	sets.set_tourist_attractions(0);

	karte_t *welt = new karte_t();   // sets the global karte_t::world
	welt->init(&sets, NULL);

	apply_command_sequence(welt, data, size);

	// ~karte_t() runs destroy() and clears the global karte_t::world.
	delete welt;

	return 0;
}
