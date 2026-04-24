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
 * to the EDGE convention above.  Bit position in ribi_t::ribi matches
 * the neighbours[] index — iterating bits 0..5 visits neighbours in
 * the same order.
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
	koord(ribi_t::ribi ribi);   // defined in koord.cc
	koord(slope_t::type slope); // uphill direction of a slope (koord.cc)

	// Static factory for the displacement of a hex-edge ribi.  Enum
	// literals (e.g. `ribi_t::northwest`) convert to both `ribi_t::ribi`
	// (uint8) and `slope_t::type` (sint16), leaving the two single-arg
	// constructors above ambiguous when called with a literal.  Use
	// this factory at sites that pass an enum literal directly; the
	// constructor still works for variables of type `ribi_t::ribi`.
	static koord step(ribi_t::ribi r);

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

	// 90° is not a hex lattice symmetry; calling this is a
	// `dbg->fatal`.  Top-level callers are gated; see TODO.md.
	void rotate90( sint16 y_size );

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
	// 6 hex neighbours (flat-top axial), clockwise starting from the SE
	// neighbour: SE, S, SW, NW, N, NE. Iterate with
	//   for (size_t i = 0; i < lengthof(koord::neighbours); i++).
	// Bit position in ribi_t::ribi matches the index: bit i set ↔
	// neighbours[i] is part of the ribi.  koord::nesw[4] and
	// koord::north/south/east/west retired with the ribi widening —
	// use koord::neighbours[i] directly, or koord(ribi_t::north) for
	// a named direction step.
	static const koord neighbours[6];
};


/**
 * Hex vertex topology.
 *
 * Flat-top hex tiles have 6 CORNERS (vertices), indexed 0..5 clockwise
 * from due-east. Flat-top hexes have NO due-N or due-S corner; the 6
 * vertices sit at angles 0°, 60°, 120°, 180°, 240°, 300° from the tile
 * centre. See AGENTS.md → "Direction naming convention".
 *
 * Every world vertex is shared by exactly 3 tiles (vs 4 for a square
 * corner), so 3 distinct (tile, corner) pairs refer to the same vertex;
 * use vertex_owners() to enumerate the canonical set.
 *
 * These primitives are the foundation that per-vertex height storage,
 * the slope_t rewrite (to 6-corner encoding) and the viewport
 * projection all build on.
 */
struct hex_corner_t {
	enum type : uint8 {
		E  = 0,
		SE = 1,
		SW = 2,
		W  = 3,
		NW = 4,
		NE = 5,
		count = 6
	};
};


/// A tile-corner name for a world vertex. Since each hex vertex is
/// shared by 3 tiles, 3 distinct (tile, corner) pairs refer to the same
/// world vertex — see vertex_owners().
struct hex_vertex_t {
	koord tile;
	hex_corner_t::type corner;
};


/**
 * Writes the 3 (tile, corner) names of the world vertex at corner @p c
 * of tile @p k into @p out. The first entry is (k, c) itself.
 *
 * Geometry: corner c is between the neighbours reached through edges
 * (c+5)%6 and c in koord::neighbours ordering; seen from each of those
 * two neighbouring tiles, the same world vertex sits at a corner index
 * rotated by 2 positions.
 */
void vertex_owners(koord tile, hex_corner_t::type c, hex_vertex_t out[3]);


/**
 * Canonical (tile, corner) name for a world vertex.
 *
 * Every world vertex has 3 (tile, corner) names (see vertex_owners).
 * The canonical one is the one whose tile is lex-smallest under (x, y)
 * ordering; this always lands on corner E or SE of some tile, so the
 * canonical corner is one of {E, SE}.  See
 * documentation/hex-vertex-storage.md for the derivation.
 *
 * Passing an already-canonical vertex returns it unchanged.
 */
hex_vertex_t canonical_vertex(hex_vertex_t v);


/**
 * Number of per-vertex storage slots for a map of @p W x @p H tiles.
 *
 * Canonical tiles span `q ∈ [-1, W-1], r ∈ [-1, H]` — `(W+1) * (H+2)`
 * positions, 2 corners (E, SE) each.  The `q = -1` / `r = -1`
 * phantom row+column host vertices along the north-west map edges
 * whose canonical owner falls one tile off-map; the `r = H` phantom
 * row hosts vertices at the SW corner of the south-edge tiles,
 * which canonicalise to `(q-1, r+1)` with `r+1 = H`.  The asymmetry
 * is real — flat-top hex geometry shifts `+r` in the SW direction
 * but never shifts `+q`, so the south edge needs an extra row that
 * the east edge does not.
 */
uint32 vertex_slot_count(sint16 W, sint16 H);


/**
 * Flat slot index for a canonical vertex in the per-vertex height
 * array of a map @p W tiles wide.
 *
 * Row-major layout matching the legacy `grid_hgts[x + y*(W+1)]`
 * convention: tile-x ranges fast, tile-y slow, stride is `(W+1)`.
 * Each canonical tile owns a contiguous pair (E, SE), so the old
 * grid-point index `i = x + y*(W+1)` maps to the E canonical slot at
 * `i*2` and the SE slot at `i*2 + 1`.
 *
 * Precondition: @p v is canonical (i.e. `v.corner` is E or SE), and
 * `v.tile` lies in `[-1, W-1] x [-1, H]`.  Call canonical_vertex()
 * first on any (tile, corner) pair that is not already known to be
 * canonical.
 */
uint32 vertex_slot_index(hex_vertex_t v, sint16 W);


/// 2D position in the hex world plane, in units of hex side length.
struct hex_pos_t {
	double x;
	double y;
};


/**
 * World XY of a hex world vertex, flat-top geometry, R = 1.  Tile
 * centre sits at `(1.5*q, sqrt(3)*(r + q/2))`; corner `c` is the
 * offset `(cos(c*60°), sin(c*60°))` from centre.  All three
 * (tile, corner) names of a shared world vertex produce the same
 * result — this is the geometric fact that lets noise sampled at
 * vertex positions be self-consistent across tile boundaries
 * without any reconciliation pass.
 */
hex_pos_t hex_vertex_pos(hex_vertex_t v);


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
