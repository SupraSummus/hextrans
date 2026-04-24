/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 */

#include <stdio.h>

#include "../dataobj/ribi.h"
#include "tunnel_desc.h"


// HEX-PORT: slope → tunnel-entrance sprite index.  Pakset tunnels
// currently have 4 directional sprites (N=1, S=0, E=3, W=2); they're
// still addressed by the 4 square-named edge slopes at single and
// double height.  The 4 hex-only edge slopes have no sprite art yet
// and return -1 (no tunnel possible).  With 4-bit ribi still in
// place, the hex-only directions can't be built in any case.
int tunnel_desc_t::slope_index(slope_t::type slope)
{
	switch (slope) {
		case slope_t::south:     case 2 * slope_t::south: return 0;
		case slope_t::north:     case 2 * slope_t::north: return 1;
		case slope_t::west:      case 2 * slope_t::west:  return 2;
		case slope_t::east:      case 2 * slope_t::east:  return 3;
		default: return -1;
	}
}


waytype_t tunnel_desc_t::get_finance_waytype() const
{
	return ((get_way_desc() && (get_way_desc()->get_styp() == type_tram)) ? tram_wt : get_waytype()) ;
}
