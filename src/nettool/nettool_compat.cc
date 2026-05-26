/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 *
 * nettool-side definitions of symbols the shared network/ TUs reach
 * into. Counterparts in simutrans live in network_cmd_ingame.cc,
 * log_fatal_display.cc, and sys/simsys_*.cc.
 */

#include <stdio.h>
#include <stdlib.h>

#include "../simutrans/dataobj/environment.h"
#include "../simutrans/dataobj/translator.h"
#include "../simutrans/network/network.h"
#include "../simutrans/network/network_cmd.h"
#include "../simutrans/network/network_cmd_ingame.h"
#include "../simutrans/network/network_cmp_pakset.h"
#include "../simutrans/simloadingscreen.h"
#include "../simutrans/utils/log.h"


std::string env_t::nickname;
bool env_t::networkmode = false;
uint8 env_t::network_heavy_mode = 0;
log_t::level_t env_t::verbose_debug = log_t::LEVEL_FATAL;
bool env_t::pakset_debug = false;
vector_tpl<std::string> env_t::listen;
uint16 const &env_t::server = network_server_port;

SOCKET nwc_join_t::pending_join_client = INVALID_SOCKET;
SOCKET nwc_pakset_info_t::server_receiver = INVALID_SOCKET;

void nwc_ready_t::clear_map_counters() {}

int dr_remove(const char *path) { return remove(path); }
FILE *dr_fopen(const char *filename, const char *mode) { return fopen(filename, mode); }

const char *translator::translate(const char *str) { return str; }

loadingscreen_t::loadingscreen_t(const char *, uint32, bool, bool) {}
loadingscreen_t::~loadingscreen_t() {}
void loadingscreen_t::set_progress(uint32) {}

void log_t_platform_fatal_exit(char *buffer)
{
	puts(buffer);
	exit(1);
}

// nettool never executes inbound commands — it sends requests and
// parses responses — so these are unreachable at runtime.  They exist
// to satisfy the vtable.
bool nwc_service_t::execute(karte_t *) { return true; }
bool nwc_auth_player_t::execute(karte_t *) { return true; }
void nwc_auth_player_t::init_player_lock_server(karte_t *) {}

bool is_admin_endpoint() { return true; }
