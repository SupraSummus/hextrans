/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 */

#ifndef DATAOBJ_MAPGEN_SHAPE_H
#define DATAOBJ_MAPGEN_SHAPE_H


#include "koord.h"


// Shapes the map generator draws on the hex grid, kept apart from
// karte_t so tools/hex_mapgen_test can compile and check them.
// Header-only over koord.h.


/**
 * Weights of one climate smoothing pass in
 * karte_t::calc_climate_map_region: the tile itself and each of its six
 * neighbours, over their total.  All six neighbours are equivalent on the
 * hex grid, so they weigh the same; the weights must add up to the
 * divisor or a field of one climate does not stay that climate.
 */
enum {
	CLIMATE_SMOOTH_CENTRE    = 4,
	CLIMATE_SMOOTH_NEIGHBOUR = 2,
	CLIMATE_SMOOTH_TOTAL     = 16
};

static_assert(CLIMATE_SMOOTH_CENTRE + 6 * CLIMATE_SMOOTH_NEIGHBOUR == CLIMATE_SMOOTH_TOTAL,
              "the smoothing weights must add up to the divisor");


/// One smoothing pass over a tile's climate value and those of its six neighbours.
static inline sint32 climate_smooth_cell(sint32 centre, const sint32 (&neighbour)[6])
{
	sint32 sum = CLIMATE_SMOOTH_CENTRE * centre;
	for(  int i = 0;  i < 6;  i++  ) {
		sum += CLIMATE_SMOOTH_NEIGHBOUR * neighbour[i];
	}
	return sum / CLIMATE_SMOOTH_TOTAL;
}


/**
 * The tiles around its start tile that a patch of the height based
 * climate generator covers.  wx and wy are the patch's extent in tiles
 * along the physical x and y axes (screen horizontal and vertical, see
 * physical_distance_sq in koord.h), so patches of equal wx and wy are
 * round in the physical plane whatever their direction on the grid.
 *
 * The patch is the box of wx by wy around its start tile cut down to
 * an ellipse.  The ellipse is the legacy test, with the physical x and y
 * offsets in place of the axial ones: a tile belongs when
 *   threshold = 8 (wx^2 + wy^2) / distance > 40
 *   with distance = 1 + sqrt(x^2 wx^2 + y^2 wy^2),
 * and the square root drops out as `x^2 wx^2 + y^2 wy^2 < limit^2`.
 */
class climate_patch_t
{
	sint32 wx, wy;
	sint64 limit_sq_x4; // 4 * limit^2, to keep the physical y offset (q + 2r) / 2 integral

public:
	climate_patch_t(sint32 wx_, sint32 wy_) : wx(wx_), wy(wy_)
	{
		// distance <= 8 (wx^2 + wy^2) / 41 is threshold > 40; the tile
		// itself has distance 1, so isqrt(x^2 wx^2 + y^2 wy^2) < limit
		const sint64 limit = (8 * ((sint64)wx * wx + (sint64)wy * wy)) / 41;
		limit_sq_x4 = 4 * limit * limit;
	}

	/// |dq| <= reach_q() and |dr| <= reach_r() for every tile of the patch.
	sint16 reach_q() const { return (sint16)((5 * wx) / 8); }          // wx / sqrt(3) < 5 wx / 8
	sint16 reach_r() const { return (sint16)((wy + reach_q()) / 2); }  // |dq + 2 dr| <= wy

	/// Is the tile at axial offset (dq, dr) from the start tile in the patch?
	bool contains(sint16 dq, sint16 dr) const
	{
		// physical offset of (dq, dr): x = sqrt(3) dq / 2, y = (dq + 2 dr) / 2
		const sint64 x2 = 3 * (sint64)dq * dq;           // 4 x^2
		const sint64 y2 = (sint64)(dq + 2 * dr) * (dq + 2 * dr);  // 4 y^2
		return x2 <= (sint64)wx * wx
		    && y2 <= (sint64)wy * wy
		    && x2 * wx * wx + y2 * wy * wy < limit_sq_x4;
	}
};


#endif
