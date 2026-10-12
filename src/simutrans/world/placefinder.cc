/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 */

#include <stdio.h>

#include "simworld.h"
#include "../simintr.h"
#include "../simtypes.h"
#include "placefinder.h"

/*
 * Checks if all tiles are suitable for the building to be built
 * Includes boundary tiles around the building
 */
bool placefinder_t::is_area_ok(koord pos, sint16 w, sint16 h, climate_bits cl) const
{
	if(!welt->is_within_limits(pos)) {
		return false;
	}
	koord k(w, h);

	while(k.y-- > 0) {
		k.x = w;
		while(k.x-- > 0) {
			if(!is_tile_ok(pos, k, cl)) {
				return false;
			}
		}
	}
	return true;
}

/*
 * Checks if the tile is part of the building or if it's a boundary tile
 * @return false, when tile is part of the building
 * @return true, when tile is the boundary tile of the building
 */
bool placefinder_t::is_boundary_tile(koord d) const
{
	return d.x == 0 || d.x == w - 1 || d.y == 0 || d.y == h - 1;
}

/*
 * Checks if the tile is suitable for building, used by is_area_ok()
 * This function is replaced for building types that require rules
 */
bool placefinder_t::is_tile_ok(koord /*pos*/, koord /*d*/, climate_bits /*cl*/) const
{
	return true;
}

koord placefinder_t::find_place(koord start, sint16 w, sint16 h, climate_bits cl, bool *r)
{
	// without a limit the disc has to reach the far acute corner of the map from any start
	sint16 radius = max_radius > 0 ? max_radius : (sint16)min(16000, welt->get_size().x + welt->get_size().y);
	pos_list_wh_t results_straight(radius, w, h);

	this->w = w;
	this->h = h;

	koord rel1, rel2;

	if((r && *r) && w != h) {
		//
		// Here we also search for the rotated positions.
		//
		pos_list_wh_t results_rotated(radius, h, w);

		if(is_area_ok(start, w, h, cl)) {
			*r = false;
			return start;
		}
		if(is_area_ok(start, h, w, cl)) {
			*r = true;
			return start;
		}
		while(results_straight.get_next_pos(rel1) && results_rotated.get_next_pos(rel2)) {
			if(is_area_ok(start + rel1, w, h, cl)) {
				*r = false;
				return start + rel1;
			}
			if(is_area_ok(start + rel2, h, w, cl)) {
				*r = true;
				return start + rel2;
			}
			INT_CHECK("simcity 1313");
		}
	}
	else {
		//
		// Search without rotated positions.
		//
		if(is_area_ok(start, w, h, cl)) {
			return start;
		}
		while(results_straight.get_next_pos(rel1)) {
			if(is_area_ok(start + rel1, w, h, cl)) {
				if(r) {
					*r = false;
				}
				return start + rel1;
			}
			INT_CHECK("simcity 1314");
		}
	}
	return koord::invalid;
}
