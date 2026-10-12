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

#endif
