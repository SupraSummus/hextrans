/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 */

#include "network_cmd_ingame.h"
#include "network.h"
#include "network_file_transfer.h"
#include "network_packet.h"
#include "network_socket_list.h"
#include "network_cmp_pakset.h"
#include "network_cmd_scenario.h"

#include "../dataobj/loadsave.h"
#include "../dataobj/gameinfo.h"
#include "../dataobj/scenario.h"
#include "../tool/simmenu.h"
#include "../simversion.h"
#include "../gui/simwin.h"
#include "../simmesg.h"
#include "../sys/simsys.h"
#include "../dataobj/environment.h"
#include "../player/simplay.h"
#include "../gui/player_frame.h"
#include "../utils/simrandom.h"
#include "../utils/cbuffer.h"
#include "../utils/csv.h"
#include "../display/viewport.h"
#include "../script/script.h" // callback for calls to tools


network_command_t* network_command_t::read_from_packet(packet_t *p)
{
	// check data
	if (p==NULL  ||  p->has_failed()  ||  !p->check_version()) {
		delete p;
		dbg->warning("network_command_t::read_from_packet", "error in packet");
		return NULL;
	}
	network_command_t* nwc = NULL;
	switch (p->get_id()) {
		case NWC_GAMEINFO:    nwc = new nwc_gameinfo_t(); break;
		case NWC_NICK:        nwc = new nwc_nick_t(); break;
		case NWC_CHAT:        nwc = new nwc_chat_t(); break;
		case NWC_JOIN:        nwc = new nwc_join_t(); break;
		case NWC_SYNC:        nwc = new nwc_sync_t(); break;
		case NWC_GAME:        nwc = new nwc_game_t(); break;
		case NWC_READY:       nwc = new nwc_ready_t(); break;
		case NWC_TOOL:        nwc = new nwc_tool_t(); break;
		case NWC_CHECK:       nwc = new nwc_check_t(); break;
#ifndef NETTOOL
		case NWC_PAKSETINFO:  nwc = new nwc_pakset_info_t(); break;
#endif
		case NWC_SERVICE:     nwc = new nwc_service_t(); break;
		case NWC_AUTH_PLAYER: nwc = new nwc_auth_player_t(); break;
		case NWC_CHG_PLAYER:  nwc = new nwc_chg_player_t(); break;
#ifndef NETTOOL
		case NWC_SCENARIO:    nwc = new nwc_scenario_t(); break;
		case NWC_SCENARIO_RULES:
		                      nwc = new nwc_scenario_rules_t(); break;
#endif
		case NWC_STEP:        nwc = new nwc_step_t(); break;
		default:
			dbg->warning("network_command_t::read_from_socket", "received unknown packet id %d", p->get_id());
	}
	if (nwc == NULL) {
		// Unknown / unsupported wire id: free p before returning so a
		// malicious peer can't drive the server out of memory by
		// flooding it with junk-id packets (callers null out their
		// packet pointer unconditionally after this returns, so they
		// won't free it either).
		delete p;
		return NULL;
	}
	if (!nwc->receive(p) ||  p->has_failed()) {
		dbg->warning("network_command_t::read_from_packet", "error while reading cmd from packet");
		delete nwc;
		nwc = NULL;
	}
	else if (env_t::server) {
		// The wire-supplied our_client_id is attacker-controlled.
		// Identify the sender by its socket instead, so any later
		// auth check (nwc_auth_player_t, nwc_chg_player_t,
		// nwc_tool_t) reads the real slot and cannot be tricked
		// into looking up someone else's player_unlocked bitmap
		// or indexing past the socket list.
		nwc->our_client_id = socket_list_t::get_client_id(p->get_sender());
	}
	return nwc;
}


void nwc_gameinfo_t::rdwr()
{
	network_command_t::rdwr();
	packet->rdwr_long(len);

}




void nwc_nick_t::rdwr()
{
	network_command_t::rdwr();
	packet->rdwr_str(nickname);

	if (packet->is_loading() && env_t::server && id == NWC_NICK) {
		const SOCKET sock = packet->get_sender();
		const socket_info_t &client = socket_list_t::get_client(socket_list_t::get_client_id(sock));

		if (client.state != socket_info_t::playing) {
			packet->failed();
		}
	}
}




void nwc_chat_t::rdwr()
{
	network_command_t::rdwr();
	packet->rdwr_str( message );
	packet->rdwr_byte( player_nr );
	packet->rdwr_str( clientname );
	packet->rdwr_str( destination );
	packet->rdwr_byte( channel_nr );
	packet->rdwr_short(pos.x);
	packet->rdwr_short(pos.y);

	if (packet->is_loading() && env_t::server) {
		const SOCKET sock = packet->get_sender();
		const socket_info_t &client = socket_list_t::get_client(socket_list_t::get_client_id(sock));

		if (client.state != socket_info_t::playing) {
			packet->failed();
		}
	}

	DBG_MESSAGE("nwc_chat_t::rdwr", "rdwr message=%s plnr=%d clientname=%s destination=%s", message.c_str(), player_nr, clientname.c_str(), destination.c_str());
}





SOCKET nwc_join_t::pending_join_client = INVALID_SOCKET;

void nwc_join_t::rdwr()
{
	nwc_nick_t::rdwr();
	packet->rdwr_long(client_id);
	packet->rdwr_byte(answer);
}




/**
 * saves the history of map counters
 * the current one is at index zero, the older ones behind
 */
#define MAX_MAP_COUNTERS (7)
vector_tpl<uint32> nwc_ready_t::all_map_counters(MAX_MAP_COUNTERS);


void nwc_ready_t::append_map_counter(uint32 map_counter_)
{
	if (all_map_counters.get_count() == MAX_MAP_COUNTERS) {
		all_map_counters.pop_back();
	}
	all_map_counters.insert_at(0, map_counter_);
}


void nwc_ready_t::clear_map_counters()
{
	all_map_counters.clear();
}




void nwc_ready_t::rdwr()
{
	network_command_t::rdwr();
	packet->rdwr_long(sync_step);
	packet->rdwr_long(map_counter);
	checklist.rdwr(packet);
}


void nwc_game_t::rdwr()
{
	network_command_t::rdwr();
	packet->rdwr_long(len);

	if (packet->is_loading() && env_t::server) {
		packet->failed();
	}
}






network_world_command_t::network_world_command_t(uint16 id, uint32 sync_step, uint32 map_counter)
: network_command_t(id)
{
	this->sync_step = sync_step;
	this->map_counter = map_counter;
}


void network_world_command_t::rdwr()
{
	network_command_t::rdwr();
	packet->rdwr_long(sync_step);
	packet->rdwr_long(map_counter);
}




void nwc_sync_t::rdwr()
{
	network_world_command_t::rdwr();
	packet->rdwr_long(client_id);
	packet->rdwr_long(new_map_counter);

	if (packet->is_loading() && env_t::server) {
		packet->failed();
	}
}




void nwc_check_t::rdwr()
{
	network_world_command_t::rdwr();
	server_checklist.rdwr(packet);
	packet->rdwr_long(server_sync_step);
	if (packet->is_loading()  &&  env_t::server) {
		// server does not receive nwc_check_t-commands
		packet->failed();
	}
}


void network_broadcast_world_command_t::rdwr()
{
	network_world_command_t::rdwr();
	packet->rdwr_bool(exec);

	if (packet->is_loading()  &&  env_t::server  &&  exec) {
		// server does not receive exec-commands
		packet->failed();
	}
}




nwc_chg_player_t::~nwc_chg_player_t()
{
	delete pending_company_creator;
}


void nwc_chg_player_t::rdwr()
{
	network_broadcast_world_command_t::rdwr();
	packet->rdwr_byte(cmd);
	packet->rdwr_byte(player_nr);
	packet->rdwr_short(param);
	packet->rdwr_bool(scripted_call);
}




connection_info_t* nwc_chg_player_t::company_creator[PLAYER_UNOWNED] = {
		NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL};

slist_tpl<connection_info_t> nwc_chg_player_t::company_active_clients[PLAYER_UNOWNED];




nwc_tool_t::nwc_tool_t() : network_broadcast_world_command_t(NWC_TOOL, 0, 0),
	init(false),
	custom_data(custom_data_buf, lengthof(custom_data_buf), true)
{
	tool = NULL;
}




nwc_tool_t::nwc_tool_t(const nwc_tool_t &nwt)
	: network_broadcast_world_command_t(NWC_TOOL, nwt.get_sync_step(), nwt.get_map_counter()),
	custom_data(custom_data_buf, lengthof(custom_data_buf), true)
{
	pos = nwt.pos;
	player_nr = nwt.player_nr;
	tool_id = nwt.tool_id;
	wt = nwt.wt;
	default_param = nwt.default_param;
	init = nwt.init;
	tool_client_id = nwt.our_client_id;
	flags = nwt.flags;
	callback_id = nwt.callback_id;
	// copy custom data of tool to our internal buffer
	custom_data.append(nwt.custom_data);
	tool = NULL;
}


nwc_tool_t::~nwc_tool_t()
{
	delete tool;
}


void nwc_tool_t::rdwr()
{
	network_broadcast_world_command_t::rdwr();
	packet->rdwr_long(last_sync_step);
	last_checklist.rdwr(packet);
	packet->rdwr_byte(player_nr);
	sint16 posx = pos.x; packet->rdwr_short(posx); pos.x = posx;
	sint16 posy = pos.y; packet->rdwr_short(posy); pos.y = posy;
	sint8  posz = pos.z; packet->rdwr_byte(posz);  pos.z = posz;
	packet->rdwr_short(tool_id);
	packet->rdwr_short(wt);
	packet->rdwr_str(default_param);
	packet->rdwr_bool(init);
	packet->rdwr_long(tool_client_id);
	packet->rdwr_byte(flags);
	packet->rdwr_long(callback_id);
	// copy custom data of tool to/from packet
	if (packet->is_saving()) {
		// write to packet
		packet->append(custom_data);
	}
	else {
		// read from packet
		custom_data.append_tail(*packet);
	}

	DBG_MESSAGE("nwc_tool_t::rdwr", "rdwr id=%d client=%d plnr=%d pos=%s tool_id=%s defpar=%s init=%d flags=%d",
		id, tool_client_id, player_nr, pos.get_str(), tool_t::id_to_string(tool_id), default_param.c_str(), init, flags);
}


// Pre-auth parse surface: clone() calls this with attacker-controlled
// tool_id before any player / client_id auth check has run. The four
// current rdwr_custom_data overrides all consume only fixed-size
// primitives (two_click_tool_t: 6 B, tool_raise_lower_base_t: 1 B,
// tool_build_bridge_t: 7 B, tool_build_roadsign_t: 9 B) and the base
// is empty; custom_data_buf is hard-capped at 256 B and
// memory_rw_t::rdwr clamps writes. A new override that reads
// variable-length data, rdwr_str into a fixed buffer, or any int*int
// size math re-opens a pre-auth corruption surface here and needs a
// security review before landing.


bool nwc_tool_t::ignore_old_events() const
{
	// messages are allowed to arrive at any time (return true if message)
	return tool_id==(GENERAL_TOOL|TOOL_ADD_MESSAGE);
}





