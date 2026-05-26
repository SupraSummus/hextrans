/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 */

/*
 * Stubs for the fuzz_network NETTOOL build.
 *
 * Pulling network_cmd_ingame.cc into the fuzz build pulls every
 * parser-side rdwr() with it.  The executor side lives in the sister
 * file network_cmd_ingame_execute.cc, which we deliberately don't
 * link here (it would drag karte_t / env_t / GUI / loadsave / scripts
 * into the harness).  What's still needed is:
 *
 *   - vtable anchors: each NWC_* class declares execute() before
 *     rdwr() in the header, so execute() is the vtable's key
 *     function (Itanium C++ ABI).  Without a definition for
 *     execute(), the vtable isn't emitted and any `new nwc_foo_t()`
 *     hits an "undefined reference to vtable" link error.  The
 *     no-op execute()/do_command()/clone() bodies below are the
 *     minimum needed to anchor each vtable.
 *
 *   - a few globals the rdwr() bodies still reach for
 *     (env_t::server, koord::invalid, koord3d::invalid + ::get_str,
 *     tool_t::id_to_string).  Stubbing them in this single file is
 *     much smaller than dragging environment.cc / simmenu.cc / etc.
 *     into the link.
 *
 * Everything here is never called from the rdwr() path — these
 * stubs exist only so the .o files link.
 */

#include "../simutrans/simtypes.h"
#include "../simutrans/dataobj/koord.h"
#include "../simutrans/dataobj/koord3d.h"
#include "../simutrans/network/network_cmd.h"
#include "../simutrans/network/network_cmd_ingame.h"

class karte_t;


const koord   koord::invalid(-1, -1);
const koord3d koord3d::invalid(-1, -1, -1);


// env_t::server is declared in environment.h as a const reference into
// network_server_port (so the env_t hide-the-storage idiom works).  We
// don't link environment.cc here; the linker needs the symbol layout
// to match (8-byte pointer slot, not 2-byte uint16), or callers'
// 8-byte loads run past the end of a 2-byte stub.  Mirror the real
// declaration with a local backing.
class env_t {
public:
	static const uint16 &server;
};
static uint16 stub_network_server_port = 0;
const uint16 &env_t::server = stub_network_server_port;


// tool_t::id_to_string is called from nwc_tool_t::rdwr()'s DBG_MESSAGE.
// We don't link the real tool registry under NETTOOL; this stub class
// shares only the symbol's name-mangling with the real declaration in
// tool/simmenu.h.
class tool_t {
public:
	static const char* id_to_string(uint16);
};
const char* tool_t::id_to_string(uint16) { return "<tool>"; }


// koord3d::get_str is called from the same DBG_MESSAGE.  The real impl
// uses a static buffer; the stub returns a placeholder.
const char* koord3d::get_str() const { return "<koord3d>"; }


// Vtable anchors.  Each derived class has execute() declared first
// in the header, making it the vtable's key function.  Production
// builds get the real bodies from network_cmd_ingame_execute.cc;
// here we just need definitions the linker can resolve.
bool nwc_gameinfo_t::execute(karte_t*)       { return false; }
bool nwc_nick_t::execute(karte_t*)           { return false; }
bool nwc_chat_t::execute(karte_t*)           { return false; }
bool nwc_join_t::execute(karte_t*)           { return false; }
bool nwc_ready_t::execute(karte_t*)          { return false; }
bool network_world_command_t::execute(karte_t*)           { return false; }
bool network_broadcast_world_command_t::execute(karte_t*) { return false; }
void nwc_sync_t::do_command(karte_t*)        {}
void nwc_chg_player_t::do_command(karte_t*)  {}
void nwc_tool_t::do_command(karte_t*)        {}
network_broadcast_world_command_t* nwc_chg_player_t::clone(karte_t*) { return NULL; }
network_broadcast_world_command_t* nwc_tool_t::clone(karte_t*)       { return NULL; }
