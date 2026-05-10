/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 */

#include "way_image_keys.h"


namespace way_image_keys {

	// Single-bit hex-edge names ordered to match ribi_t bit positions:
	// SE=0, S=1, SW=2, NW=3, N=4, NE=5.
	static const char* const ribi_bit_short_name[6] = {
		"se", "s", "sw", "nw", "n", "ne"
	};


	std::string ribi_key(uint8 ribi)
	{
		if (ribi == 0) return "-";
		std::string s;
		for (uint8 b = 0; b < 6; b++) {
			if (!(ribi & (1u << b))) continue;
			if (!s.empty()) s += '_';
			s += ribi_bit_short_name[b];
		}
		return s;
	}


	// Slope-up slot keys, indexed by `way_desc_t::get_slope_image_id`'s
	// `nr`: 6 narrow (clockwise from north), 6 wide, 6 double.  The
	// writer emits `imageup[<key>]` in this order; the engine reads
	// the same order.
	const char* const slope_slot_keys[18] = {
		"n",        "ne",        "se",        "s",        "sw",        "nw",
		"n_wide",   "ne_wide",   "se_wide",   "s_wide",   "sw_wide",   "nw_wide",
		"n_double", "ne_double", "se_double", "s_double", "sw_double", "nw_double"
	};


	const char* slope_key(slope_t::type s)
	{
		switch (s) {
			case slope_t::north_narrow:     return slope_slot_keys[0];
			case slope_t::northeast_narrow: return slope_slot_keys[1];
			case slope_t::southeast_narrow: return slope_slot_keys[2];
			case slope_t::south_narrow:     return slope_slot_keys[3];
			case slope_t::southwest_narrow: return slope_slot_keys[4];
			case slope_t::northwest_narrow: return slope_slot_keys[5];
			case slope_t::north_wide:       return slope_slot_keys[6];
			case slope_t::northeast_wide:   return slope_slot_keys[7];
			case slope_t::southeast_wide:   return slope_slot_keys[8];
			case slope_t::south_wide:       return slope_slot_keys[9];
			case slope_t::southwest_wide:   return slope_slot_keys[10];
			case slope_t::northwest_wide:   return slope_slot_keys[11];
			default:                        break;
		}
		// Planar-double constants are constexpr sint16, not enum members.
		if (s == slope_t::north_double)     return slope_slot_keys[12];
		if (s == slope_t::northeast_double) return slope_slot_keys[13];
		if (s == slope_t::southeast_double) return slope_slot_keys[14];
		if (s == slope_t::south_double)     return slope_slot_keys[15];
		if (s == slope_t::southwest_double) return slope_slot_keys[16];
		if (s == slope_t::northwest_double) return slope_slot_keys[17];
		return NULL;
	}
}
