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
 * 2× start slopes return the `(img_t)-1` sentinel and render as
 * empty; the `*_Start2` / `*_Ramp2` enum slots are still consumed by
 * `has_double_start()` / `has_double_ramp()` (called from
 * `brueckenbauer.cc` to gate 2× *start* geometry, separate from
 * way-buildability) — see TODO.md.
 */
bridge_desc_t::img_t bridge_desc_t::get_start(slope_t::type slope) const
{
#define SLOPE_CASE(SLOPE, WIDE, IMG) \
	case slope_t::SLOPE: case slope_t::WIDE: return IMG;

	switch (slope) {
		SLOPE_CASE(north,   north_wide, N_Start)
		SLOPE_CASE(south,   south_wide, S_Start)
		SLOPE_CASE(ne_edge, ne_wide,    NE_Start)
		SLOPE_CASE(se_edge, se_wide,    SE_Start)
		SLOPE_CASE(sw_edge, sw_wide,    SW_Start)
		SLOPE_CASE(nw_edge, nw_wide,    NW_Start)
	}
#undef SLOPE_CASE
	return (img_t) - 1;
}


/**
 * Returns image index of a ramp piece.
 *
 * Ramp images are named for the **high** end of the ramp, opposite
 * to the bridge's outward direction.  On a slope where the south
 * corners are raised (`slope_t::north`, low edge N) the bridge points
 * north; the ramp's high end is to the south, so it's an `S_Ramp`.
 * Pattern carries over to the four hex-only edges.
 */
bridge_desc_t::img_t bridge_desc_t::get_ramp(slope_t::type slope) const
{
#define SLOPE_CASE(SLOPE, WIDE, IMG) \
	case slope_t::SLOPE: case slope_t::WIDE: return IMG;

	switch (slope) {
		SLOPE_CASE(north,   north_wide, S_Ramp)
		SLOPE_CASE(south,   south_wide, N_Ramp)
		SLOPE_CASE(ne_edge, ne_wide,    SW_Ramp)
		SLOPE_CASE(se_edge, se_wide,    NW_Ramp)
		SLOPE_CASE(sw_edge, sw_wide,    NE_Ramp)
		SLOPE_CASE(nw_edge, nw_wide,    SE_Ramp)
	}
#undef SLOPE_CASE
	return (img_t) - 1;
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
 * returns whether desc has double height images for ramps
 */
bool bridge_desc_t::has_double_ramp() const
{
	return (get_background(bridge_desc_t::N_Ramp2, 0)!=IMG_EMPTY || get_foreground(bridge_desc_t::N_Ramp2, 0)!=IMG_EMPTY);
}

bool bridge_desc_t::has_double_start() const
{
	return (get_background(bridge_desc_t::N_Start2, 0) != IMG_EMPTY  ||  get_foreground(bridge_desc_t::N_Start2, 0) != IMG_EMPTY);
}


void bridge_desc_t::calc_checksum(checksum_t *chk) const
{
	obj_desc_transport_infrastructure_t::calc_checksum(chk);
	chk->input(pillars_every);
	chk->input(pillars_asymmetric);
	chk->input(max_length);
	chk->input(max_height);
}
