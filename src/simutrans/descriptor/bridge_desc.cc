/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 */

#include "../simdebug.h"

#include "bridge_desc.h"
#include "ground_desc.h"
#include "../network/checksum.h"


/**
 * Returns image index of a straight piece (excluding start pieces).
 *
 * Maps the way's straight ribi onto one of the 3 hex axes via
 * `ribi_t::straight_axis` and selects the matching segment image.
 * @p ribi must be a straight ribi (single direction or axis-pair);
 * non-straight inputs hit the assert below.
 */
bridge_desc_t::img_t bridge_desc_t::get_straight(ribi_t::ribi ribi, uint8 height) const
{
	const ribi_t::ribi axis = ribi_t::straight_axis(ribi);
	assert(axis != ribi_t::none);
	const bool use_double = (height > 1)
	    && get_background(NS_Segment2, 0) != IMG_EMPTY;

	if (axis == ribi_t::northeast) {
		return use_double ? NE_SW_Segment2 : NE_SW_Segment;
	}
	if (axis == ribi_t::northwest) {
		return use_double ? NW_SE_Segment2 : NW_SE_Segment;
	}
	// axis == ribi_t::north — N-S axis.
	return use_double ? NS_Segment2 : NS_Segment;
}


// ditto for pillars
bridge_desc_t::img_t bridge_desc_t::get_pillar(ribi_t::ribi ribi)
{
	const ribi_t::ribi axis = ribi_t::straight_axis(ribi);
	assert(axis != ribi_t::none);
	if (axis == ribi_t::northeast) return NE_SW_Pillar;
	if (axis == ribi_t::northwest) return NW_SE_Pillar;
	return NS_Pillar;
}


/**
 * Returns image index of a straight bridge-start piece (on slope).
 *
 * Each hex edge slope (low-edge naming: the bridge starts on this
 * slope and points outward through the named low edge) maps to the
 * matching `*_Start` image.  Narrow (2-corner) and wide (4-corner)
 * variants of the same axis edge share the start image — bridge
 * geometry follows the low edge regardless of the off-axis ground.
 * Planar 012210 hex double ramps use the `*_Start2` slots for the
 * matching low edge.
 */
bridge_desc_t::img_t bridge_desc_t::get_start(slope_t::type slope) const
{
#define SLOPE_CASE(SLOPE, WIDE, IMG) \
	case slope_t::SLOPE: case slope_t::WIDE: return IMG;
#define DOUBLE_SLOPE_CASE(PLANAR, IMG) \
	case slope_t::PLANAR: return IMG;

	switch (slope) {
		SLOPE_CASE(north_narrow,     north_wide,     N_Start)
		SLOPE_CASE(south_narrow,     south_wide,     S_Start)
		SLOPE_CASE(northeast_narrow, northeast_wide, NE_Start)
		SLOPE_CASE(southeast_narrow, southeast_wide, SE_Start)
		SLOPE_CASE(southwest_narrow, southwest_wide, SW_Start)
		SLOPE_CASE(northwest_narrow, northwest_wide, NW_Start)
		DOUBLE_SLOPE_CASE(north_double,     N_Start2)
		DOUBLE_SLOPE_CASE(south_double,     S_Start2)
		DOUBLE_SLOPE_CASE(northeast_double, NE_Start2)
		DOUBLE_SLOPE_CASE(southeast_double, SE_Start2)
		DOUBLE_SLOPE_CASE(southwest_double, SW_Start2)
		DOUBLE_SLOPE_CASE(northwest_double, NW_Start2)
	}
#undef DOUBLE_SLOPE_CASE
#undef SLOPE_CASE
	return img_t_count;
}


/**
 * Returns image index of a ramp piece.
 *
 * Ramp images are named for the **high** end of the ramp, opposite
 * to the bridge's outward direction.  On a slope where the south
 * corners are raised (`slope_t::north_narrow`, low edge N) the bridge points
 * north; the ramp's high end is to the south, so it's an `S_Ramp`.
 * Pattern carries over to the four hex-only edges.
 */
bridge_desc_t::img_t bridge_desc_t::get_ramp(slope_t::type slope) const
{
#define SLOPE_CASE(SLOPE, WIDE, IMG) \
	case slope_t::SLOPE: case slope_t::WIDE: return IMG;
#define DOUBLE_SLOPE_CASE(PLANAR, IMG) \
	case slope_t::PLANAR: return IMG;

	switch (slope) {
		SLOPE_CASE(north_narrow,     north_wide,     S_Ramp)
		SLOPE_CASE(south_narrow,     south_wide,     N_Ramp)
		SLOPE_CASE(northeast_narrow, northeast_wide, SW_Ramp)
		SLOPE_CASE(southeast_narrow, southeast_wide, NW_Ramp)
		SLOPE_CASE(southwest_narrow, southwest_wide, NE_Ramp)
		SLOPE_CASE(northwest_narrow, northwest_wide, SE_Ramp)
		DOUBLE_SLOPE_CASE(north_double,     S_Ramp2)
		DOUBLE_SLOPE_CASE(south_double,     N_Ramp2)
		DOUBLE_SLOPE_CASE(northeast_double, SW_Ramp2)
		DOUBLE_SLOPE_CASE(southeast_double, NW_Ramp2)
		DOUBLE_SLOPE_CASE(southwest_double, NE_Ramp2)
		DOUBLE_SLOPE_CASE(northwest_double, SE_Ramp2)
	}
#undef DOUBLE_SLOPE_CASE
#undef SLOPE_CASE
	return img_t_count;
}


/**
 * returns image index for appropriate ramp or start image given ground and way slopes
 */
bridge_desc_t::img_t bridge_desc_t::get_end(slope_t::type test_slope, slope_t::type ground_slope, slope_t::type way_slope) const
{
	img_t end_image;
	if(  test_slope == slope_t::flat  ) {
		end_image = get_ramp( way_slope );
	}
	else {
		end_image = get_start( ground_slope );
	}
	return end_image;
}


/**
 * returns whether desc supports double-height ramps / starts.  During
 * the hex port the engine accepts double-height geometry whenever the
 * desc isn't explicitly capped via `max_height`, regardless of whether
 * the pakset ships matching art (single-edge art is reused; AGENTS.md
 * pakset-art-out-of-scope).  Missing art renders empty / wrong-height,
 * but the engine builds rather than refuses.
 */
bool bridge_desc_t::has_double_ramp() const
{
	return max_height == 0  ||  max_height >= 2;
}

bool bridge_desc_t::has_double_start() const
{
	return max_height == 0  ||  max_height >= 2;
}


void bridge_desc_t::calc_checksum(checksum_t *chk) const
{
	obj_desc_transport_infrastructure_t::calc_checksum(chk);
	chk->input(pillars_every);
	chk->input(pillars_asymmetric);
	chk->input(max_length);
	chk->input(max_height);
}
