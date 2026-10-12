/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 */

#ifndef PLAYER_AI_SITE_H
#define PLAYER_AI_SITE_H

// Geometry of the computer players' site search (ai.cc, ai_goods.cc).
// Depends on dataobj/koord.h alone, so tools/hex_ai_site_test compiles it
// as is.

#include "../dataobj/koord.h"


/**
 * Whether a station whose catchment has radius @p cov, standing on @p tile,
 * reaches the footprint @p pos .. @p pos + @p size - (1,1).  The tiles for
 * which this holds are the hex dilation of the footprint.  The axial
 * rectangle grown by @p cov is not that: its (+cov,+cov) and (-cov,-cov)
 * corners are 2*cov steps away.
 */
static inline bool ai_station_reaches(const koord &tile, const koord &pos, const koord &size, uint16 cov)
{
	return koord_distance_to_rect(tile, pos, size) <= cov;
}


/**
 * A straight run of tiles, as the vector from its first to its last tile:
 * a hex direction times a length.  The length is not abs(x + y), which is 0
 * along the NE-SW axis.
 */
static inline sint16 ai_run_length(const koord &run)
{
	return (sint16)koord_distance(koord(0, 0), run);
}


/// The hex direction of a run (see ai_run_length), (0,0) for a single tile.
static inline koord ai_run_step(const koord &run)
{
	return koord((sint16)sgn(run.x), (sint16)sgn(run.y));
}


/**
 * Calls @p f(tile) for each tile of the run from @p place to place + @p run,
 * both ends included.  The axial rectangle spanned by the two ends is the
 * run only along the N-S and SE-NW axes.
 */
template<class F>
static inline void ai_for_each_run_tile(const koord &place, const koord &run, F f)
{
	const koord step = ai_run_step(run);
	const sint16 length = ai_run_length(run);
	for (sint16 i = 0; i <= length; i++) {
		f(place + step * i);
	}
}


/**
 * The direction, besides @p uphill, in which to try a station behind a shore
 * tile whose slope rises towards @p uphill.  It is one of the two neighbours
 * of @p uphill on the compass, at 60 degrees from it and so away from the
 * water, whichever points closer to @p toward (the offset to the destination).
 * Swapping x and y, as the square grid did, turns NE into SW and back: along
 * the NE-SW axis it points at the water.
 */
static inline koord ai_shore_second_dir(const koord &uphill, const koord &toward)
{
	const koord cw  = hex_axial_rotate(uphill, 1);
	const koord ccw = hex_axial_rotate(uphill, -1);
	return physical_dot_x2(ccw, toward) > physical_dot_x2(cw, toward) ? ccw : cw;
}

#endif
