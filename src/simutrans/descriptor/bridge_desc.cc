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
 * matching `*_Start` image.  The legacy 2-corner ::east / ::west
 * diagonals project onto the NW-SE axis (per `slope_t::is_way_nw_se`)
 * and route to SE / NW respectively, matching the pak128 rename
 * convention (legacy "east" ↔ hex SE, legacy "west" ↔ hex NW).
 */
bridge_desc_t::img_t bridge_desc_t::get_start(slope_t::type slope) const
{
	const bool use_double = ground_desc_t::double_grounds
	    && get_background(N_Start2, 0) != IMG_EMPTY;

#define SLOPE_CASE(SLOPE, IMG) \
	case slope_t::SLOPE:        return IMG;                       \
	case slope_t::SLOPE * 2:    return use_double ? IMG##2 : IMG;

	switch (slope) {
		SLOPE_CASE(north,   N_Start)
		SLOPE_CASE(south,   S_Start)
		SLOPE_CASE(ne_edge, NE_Start)
		SLOPE_CASE(se_edge, SE_Start)
		SLOPE_CASE(sw_edge, SW_Start)
		SLOPE_CASE(nw_edge, NW_Start)
		// Legacy 2-corner diagonals project onto NW-SE (see header).
		SLOPE_CASE(east, SE_Start)
		SLOPE_CASE(west, NW_Start)
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
	const bool use_double = ground_desc_t::double_grounds && has_double_ramp();

#define SLOPE_CASE(SLOPE, IMG) \
	case slope_t::SLOPE:        return IMG;                       \
	case slope_t::SLOPE * 2:    return use_double ? IMG##2 : IMG;

	switch (slope) {
		SLOPE_CASE(north,   S_Ramp)
		SLOPE_CASE(south,   N_Ramp)
		SLOPE_CASE(ne_edge, SW_Ramp)
		SLOPE_CASE(se_edge, NW_Ramp)
		SLOPE_CASE(sw_edge, NE_Ramp)
		SLOPE_CASE(nw_edge, SE_Ramp)
		// Legacy 2-corner diagonals project onto NW-SE (see get_start).
		SLOPE_CASE(east, NW_Ramp)
		SLOPE_CASE(west, SE_Ramp)
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
