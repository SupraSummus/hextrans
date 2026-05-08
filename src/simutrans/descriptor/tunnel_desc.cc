/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 */

#include <stdio.h>

#include "../dataobj/ribi.h"
#include "tunnel_desc.h"


// Slope → tunnel-entrance sprite index.  Edge index matches
// `hex_keys::edge_names[6]` = {n, s, ne, se, sw, nw}, low-edge
// naming (matching bridge's start/ramp convention): `slope_t::north_narrow`
// is "low edge N, mountain rises south, portal points outward through
// N" → index 0.  This INVERTS the upstream tunnel convention (where
// the key was named for the high edge / portal-facing direction),
// so pre-port pak64 tunnel images load at slots with N↔S and E↔W
// permuted; new hex art needs to follow the low-edge convention.
// Narrow and wide variants of the same axis edge share the portal
// slot — the portal hugs the same low edge regardless of whether the
// off-axis side corners are also raised.  Double-height slopes are
// no longer way-buildable so they're gone from this table.
int tunnel_desc_t::slope_index(slope_t::type slope)
{
	switch (slope) {
		case slope_t::north_narrow:     case slope_t::north_wide:     return 0; // n
		case slope_t::south_narrow:     case slope_t::south_wide:     return 1; // s
		case slope_t::northeast_narrow: case slope_t::northeast_wide: return 2; // ne
		case slope_t::southeast_narrow: case slope_t::southeast_wide: return 3; // se
		case slope_t::southwest_narrow: case slope_t::southwest_wide: return 4; // sw
		case slope_t::northwest_narrow: case slope_t::northwest_wide: return 5; // nw
		default: return -1;
	}
}


waytype_t tunnel_desc_t::get_finance_waytype() const
{
	return ((get_way_desc() && (get_way_desc()->get_styp() == type_tram)) ? tram_wt : get_waytype()) ;
}
