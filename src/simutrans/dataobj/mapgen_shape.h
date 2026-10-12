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


#endif
