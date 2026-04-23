/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 */

#ifndef DATAOBJ_KOORD_H
#define DATAOBJ_KOORD_H


#include "ribi.h"
#include "../simtypes.h"

#include <stdlib.h>

class loadsave_t;

/**
 * 2D Coordinates.
 *
 * HEX-PORT NOTE: as of the hex-grid port, koord stores axial hex
 * coordinates (q, r) on a flat-top hex grid. The struct fields are
 * still named x and y for byte-compatibility (save format,
 * planquadrat_t indexing, etc. all unchanged), but the *operations*
 * defined below — distance, neighbour iteration — use hex semantics.
 *
 * Naming convention (pin this; getting it wrong gives a silent
 * 30°-off grid that compiles fine):
 *   - EDGES (and the 6 neighbour directions reached through them):
 *       N, NE, SE, S, SW, NW.
 *     Flat-top hexes have due-N and due-S edges, so neighbour
 *     directions DO include N and S.
 *   - VERTICES / CORNERS (6 per tile):
 *       E, SE, SW, W, NW, NE.
 *     Flat-top hexes have NO due-N or due-S corner; the 6 vertices
 *     sit at angles 0°, 60°, 120°, 180°, 240°, 300° from the centre.
 * koord::neighbours[] is ordered clockwise starting from the SE
 * neighbour, see koord.cc.  Edge labels in the comments there refer
 * to the EDGE convention above.
 *
 * koord::nesw[4] and the koord::north/south/east/west constants still
 * encode the legacy 4-direction (square) model and remain tied to the
 * ribi system, which is still 4-bit; they will be retired when ribi
 * is widened to 6 hex directions.
 */
class koord
{
public:
	// this is set by einstelugen_t
	static uint32 locality_factor;

	// (q, r) axial hex coords; struct field names kept as x/y for
	// save-format / array-indexing compatibility.
	sint16 x;
	sint16 y;

	koord() : x(0), y(0) {}

	koord(sint16 xp, sint16 yp) : x(xp), y(yp) {}
	koord(ribi_t::ribi ribi) { *this = from_ribi[ribi]; }
	koord(slope_t::type slope) { *this = from_hang[slope]; }

	// use this instead of koord(simrand(x),simrand(y)) to avoid
	// different order on different compilers
	static koord koord_random(uint16 xrange, uint16 yrange);

	void rdwr(loadsave_t *file);

	const char *get_str() const;
	const char *get_fullstr() const; // including brackets

	const koord& operator += (const koord & k)
	{
		x += k.x;
		y += k.y;
		return *this;
	}

	const koord& operator -= (const koord & k)
	{
		x -= k.x;
		y -= k.y;
		return *this;
	}

	void rotate90( sint16 y_size )
	{
		if(  (x&y)<0  ) {
			// do not rotate illegal coordinates
			return;
		}
		sint16 new_x = y_size-y;
		y = x;
		x = new_x;
	}

	inline void clip_min( koord k_min )
	{
		if (x < k_min.x) {
			x = k_min.x;
		}
		if (y < k_min.y) {
			y = k_min.y;
		}
	}

	inline void clip_max( koord k_max )
	{
		if (x > k_max.x) {
			x = k_max.x;
		}
		if (y > k_max.y) {
			y = k_max.y;
		}
	}

	static const koord invalid;
	static const koord north;
	static const koord south;
	static const koord east;
	static const koord west;
	// Legacy 4-cardinal directions, tied to the (still-square) ribi
	// system: koord::nesw[i] is the displacement matching the ribi
	// direction at ribi_t::nesw[i]. Will be retired with the ribi
	// widening.
	static const koord nesw[4];
	// 6 hex neighbours (flat-top axial), clockwise starting from the SE
	// neighbour: SE, S, SW, NW, N, NE. Iterate with
	// for (size_t i = 0; i < lengthof(koord::neighbours); i++).
	static const koord neighbours[6];

private:
	static const koord from_ribi[16];
	static const koord from_hang[81];
};


// Hex axial distance (cube formula: (|dx|+|dy|+|dz|)/2 with x+y+z=0).
// Replaces the legacy Manhattan distance; semantics are hex-grid steps
// between two axial coords.
static inline uint32 koord_distance(const koord &a, const koord &b)
{
	const sint32 dx = a.x - b.x;
	const sint32 dz = a.y - b.y;
	const sint32 dy = -dx - dz;
	return (uint32)((abs(dx) + abs(dy) + abs(dz)) / 2);
}

// Same metric as koord_distance for hex grids — there is no separate
// "with diagonals" notion on a hex grid because all 6 neighbours are
// at distance 1. Kept as a separate function so callers that
// semantically wanted "shortest reachable steps" still read clearly;
// any callers that used it specifically for sqrt(2)-weighted ordinal
// distance need a different metric and should be revisited.
static inline uint32 shortest_distance(const koord &a, const koord &b)
{
	return koord_distance(a, b);
}

// multiply the value by the distance weight
static inline uint32 weight_by_distance(const sint32 value, const uint32 distance)
{
	return value<=0 ? 0 : 1+(uint32)( ( ((sint64)value<<8) * koord::locality_factor ) / ( (sint64)koord::locality_factor + (sint64)(distance < 4u ? 4u : distance) ) );
}

static inline koord operator * (const koord &k, const sint16 m)
{
	return koord(k.x * m, k.y * m);
}


static inline koord operator / (const koord &k, const sint16 m)
{
	return koord(k.x / m, k.y / m);
}


static inline bool operator == (const koord &a, const koord &b)
{
	// only this works with O3 optimisation!
	return ((a.x-b.x)|(a.y-b.y))==0;
}


static inline bool operator != (const koord &a, const koord &b)
{
	// only this works with O3 optimisation!
	return ((a.x-b.x)|(a.y-b.y))!=0;
}


static inline koord operator + (const koord &a, const koord &b)
{
	return koord(a.x + b.x, a.y + b.y);
}


static inline koord operator - (const koord &a, const koord &b)
{
	return koord(a.x - b.x, a.y - b.y);
}


static inline koord operator - (const koord &a)
{
	return koord(-a.x, -a.y);
}
#endif
