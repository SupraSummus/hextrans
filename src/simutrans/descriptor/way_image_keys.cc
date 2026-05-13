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


	// Slope-up slot keys.  First block (0..17) is full-axis crossings
	// (6 narrow clockwise from north, 6 wide in the same order, 6
	// double in the same order) used when the way ribi spans the
	// whole slope axis.  Second block (18..35) is low-half stubs —
	// single-bit ribi terminating on the slope's low edge, way drawn
	// only over the half nearest that edge.  Third block (36..53) is
	// high-half stubs (single-bit ribi terminating on the high edge).
	// Each half block mirrors the full block's axis order: slot 18 is
	// the low half of `north_narrow` (full slot 0), slot 36 is the
	// high half of `north_narrow`, and so on.
	const char* const slope_slot_keys[SLOPE_SLOTS_TOTAL] = {
		"n",        "ne",        "se",        "s",        "sw",        "nw",
		"n_wide",   "ne_wide",   "se_wide",   "s_wide",   "sw_wide",   "nw_wide",
		"n_double", "ne_double", "se_double", "s_double", "sw_double", "nw_double",
		"n_low_half",        "ne_low_half",        "se_low_half",        "s_low_half",        "sw_low_half",        "nw_low_half",
		"n_wide_low_half",   "ne_wide_low_half",   "se_wide_low_half",   "s_wide_low_half",   "sw_wide_low_half",   "nw_wide_low_half",
		"n_double_low_half", "ne_double_low_half", "se_double_low_half", "s_double_low_half", "sw_double_low_half", "nw_double_low_half",
		"n_high_half",        "ne_high_half",        "se_high_half",        "s_high_half",        "sw_high_half",        "nw_high_half",
		"n_wide_high_half",   "ne_wide_high_half",   "se_wide_high_half",   "s_wide_high_half",   "sw_wide_high_half",   "nw_wide_high_half",
		"n_double_high_half", "ne_double_high_half", "se_double_high_half", "s_double_high_half", "sw_double_high_half", "nw_double_high_half"
	};


	// Return the 0..17 full-slope slot index, or -1 for non-ramp slopes.
	static int full_slope_slot(slope_t::type s)
	{
		switch (s) {
			case slope_t::north_narrow:     return 0;
			case slope_t::northeast_narrow: return 1;
			case slope_t::southeast_narrow: return 2;
			case slope_t::south_narrow:     return 3;
			case slope_t::southwest_narrow: return 4;
			case slope_t::northwest_narrow: return 5;
			case slope_t::north_wide:       return 6;
			case slope_t::northeast_wide:   return 7;
			case slope_t::southeast_wide:   return 8;
			case slope_t::south_wide:       return 9;
			case slope_t::southwest_wide:   return 10;
			case slope_t::northwest_wide:   return 11;
			default:                        break;
		}
		// Planar-double constants are constexpr sint16, not enum members.
		if (s == slope_t::north_double)     return 12;
		if (s == slope_t::northeast_double) return 13;
		if (s == slope_t::southeast_double) return 14;
		if (s == slope_t::south_double)     return 15;
		if (s == slope_t::southwest_double) return 16;
		if (s == slope_t::northwest_double) return 17;
		return -1;
	}


	const char* slope_key(slope_t::type s)
	{
		const int nr = full_slope_slot(s);
		return nr < 0 ? NULL : slope_slot_keys[nr];
	}


	int slope_half_slot(slope_t::type s, bool high_half)
	{
		const int nr = full_slope_slot(s);
		if (nr < 0) return -1;
		return SLOPE_SLOTS_FULL + nr + (high_half ? SLOPE_SLOTS_FULL : 0);
	}
}
