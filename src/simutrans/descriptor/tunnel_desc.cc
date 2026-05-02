/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 */

#include <stdio.h>

#include "../dataobj/ribi.h"
#include "tunnel_desc.h"


// Slope → tunnel-entrance sprite index.  Edge index matches
// `hex_keys::edge_names[6]` = {n, s, ne, se, sw, nw}, low-edge
// naming (matching bridge's start/ramp convention): `slope_t::north`
// is "low edge N, mountain rises south, portal points outward through
// N" → index 0.  This INVERTS the upstream tunnel convention (where
// the key was named for the high edge / portal-facing direction),
// so pre-port pak64 tunnel images load at slots with N↔S and E↔W
// permuted; new hex art needs to follow the low-edge convention.
// Double-height slopes share the single-height slot — there is no
// double-height tunnel portal art.  Legacy 2-corner diagonals
// (`slope_t::east` / `west`) project onto the NW-SE axis per
// `slope_t::is_way_nw_se`, matching bridge; retirement bound to the
// wider `is_way_nw_se` shim retirement.
int tunnel_desc_t::slope_index(slope_t::type slope)
{
	switch (slope) {
		case slope_t::north:    case 2 * slope_t::north:   return 0; // n
		case slope_t::south:    case 2 * slope_t::south:   return 1; // s
		case slope_t::ne_edge:  case 2 * slope_t::ne_edge: return 2; // ne
		case slope_t::se_edge:  case 2 * slope_t::se_edge: return 3; // se
		case slope_t::sw_edge:  case 2 * slope_t::sw_edge: return 4; // sw
		case slope_t::nw_edge:  case 2 * slope_t::nw_edge: return 5; // nw
		// Legacy 2-corner diagonals project onto NW-SE.
		case slope_t::east:     case 2 * slope_t::east:    return 3; // se
		case slope_t::west:     case 2 * slope_t::west:    return 5; // nw
		default: return -1;
	}
}


waytype_t tunnel_desc_t::get_finance_waytype() const
{
	return ((get_way_desc() && (get_way_desc()->get_styp() == type_tram)) ? tram_wt : get_waytype()) ;
}
