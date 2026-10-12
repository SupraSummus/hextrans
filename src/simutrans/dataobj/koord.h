/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 */

#ifndef DATAOBJ_KOORD_H
#define DATAOBJ_KOORD_H


#include "ribi.h"
#include "../macros.h"
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
 * neighbour, see koord_hex.cc.  Edge labels in the comments there refer
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
	koord(ribi_t::ribi ribi);   // defined in koord_hex.cc
	koord(slope_t::type slope); // uphill direction of a slope (koord_hex.cc)

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

	/**
	 * One 60° step of the whole map, clockwise on screen, for a map
	 * that is a plain w x h rhombus (map_shape_t d == 1) with
	 * @p y_size = h - 1.  Same tile as `map_shape_t(w, h, 1).rotate_tile`
	 * for any w, i.e. hex_axial_rotate(*this, 1) shifted by (y_size, 0).
	 *
	 * The tile lands in the rotated hexagon (1, w, h), whose bounding
	 * rhombus is h x (w + h - 1): one step does not map a rhombus onto
	 * itself, only three do.  Worlds with d > 1, and several steps in
	 * a row, go through map_shape_t::rotate_tile.
	 * The top-level rotation is still gated off; see TODO.md.
	 */
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
 * The 3 world vertices adjacent to @p v via hex edges, in canonical form.
 *
 * Each world vertex has degree 3 in the edge graph (it sits at the
 * junction of 3 hex edges).  The bipartite E/SE structure means an
 * E-type canonical vertex's 3 neighbours are all SE-type, and vice versa.
 * @p v need not be canonical; it is canonicalised internally.
 */
void vertex_neighbours(hex_vertex_t v, hex_vertex_t out[3]);


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


/**
 * Rotation about the centre of tile (0, 0) by @p steps * 60° clockwise
 * on screen: one step takes SE to S and S to SW, i.e. koord::neighbours[i]
 * to neighbours[(i + 1) % 6], the direction of ribi_t::rotate60 and
 * slope_t::rotate60.  The step is linear, (q, r) -> (-r, q + r).
 * Negative @p steps rotate counter-clockwise; six steps are the
 * identity and three the point reflection (-q, -r).
 *
 * This is the one definition of the map-rotation geometry.  Shifting
 * it into the rotated map's rhombus is map_shape_t::rotate_tile's job.
 *
 * The vertex form rotates the name too: corner c becomes corner c + steps,
 * and all three names of a world vertex rotate to names of one world vertex.
 */
koord hex_axial_rotate(koord c, int steps);

/// Inverse of hex_axial_rotate: @p steps * 60° counter-clockwise.
koord hex_axial_rotate_inv(koord c, int steps);

hex_vertex_t hex_axial_rotate(hex_vertex_t v, int steps);


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

// Squared *physical* (Euclidean) distance between two axial coords, as
// integer so the sim stays cross-platform deterministic.  Distinct from
// koord_distance above: that counts hex steps (a hexagonal "circle"),
// this is the round circle in screen space after the flat-top basis
// e_q=(sqrt3/2,1/2), e_r=(0,1).  |q*e_q + r*e_r|^2 expands to the integer
// form below.  Use it wherever "how round in physical space" is the
// question (city-footprint shaping, real-distance gameplay metrics) and
// koord_distance's hexagonal balls would skew the answer.
static inline uint32 physical_distance_sq(const koord &a, const koord &b)
{
	const sint32 dq = a.x - b.x;
	const sint32 dr = a.y - b.y;
	return (uint32)(dq * dq + dq * dr + dr * dr);
}

// Twice the *physical* dot product of two axial offsets, as an integer:
// (q1*e_q + r1*e_r) . (q2*e_q + r2*e_r) = q1*q2 + (q1*r2 + r1*q2)/2 + r1*r2
// for the basis above, so physical_distance_sq(a, b) is half of
// physical_dot_x2(a - b, a - b).  Its sign is the sign of the cosine of
// the physical angle between the offsets (negative: obtuse).  The
// square-grid q1*q2 + r1*r2 is not that cosine: e_q and e_r are 60 degrees
// apart, not 90.
static inline sint32 physical_dot_x2(const koord &a, const koord &b)
{
	return 2 * a.x * b.x + a.x * b.y + a.y * b.x + 2 * a.y * b.y;
}

// How sharply a route turns where it steps into a tile by `in` and out of
// it by `out` (two neighbour steps), in 60 degree units: 0 straight on,
// 1 a 60 degree turn, 2 a 120 degree turn, 3 turning back.  Neighbour
// steps are unit vectors in the physical plane, so physical_dot_x2 of two
// of them is twice the cosine of the turn: 2, 1, -1, -2.
//
// The sum of the two steps (the delta between the tiles before and after)
// does not tell: it is twice a step, so on a hex axis, for straight on, and
// one step along the axis in between for a 120 degree turn, and off the axes
// only for a 60 degree turn.  ribi_type(sum) calls a 120 degree turn straight.
static inline uint32 hex_turn_steps(const koord &in, const koord &out)
{
	switch (physical_dot_x2(in, out)) {
		case  2: return 0;
		case  1: return 1;
		case -1: return 2;
		default: return 3;
	}
}

// Hex distance from tile p to the nearest tile of the axial rectangle
// pos .. pos+size-1 (size.x by size.y tiles).  A rectangle in (q, r) is
// a parallelogram, so the tiles within n steps of it are the rectangle
// grown by n minus two corner triangles: the (-n,-n) and (+n,+n) corners
// of the grown rectangle are 2n steps away.  Clamping p into the
// rectangle picks the nearest tile on every side.
//
//   koord_distance_to_rect(p, pos, size) == 1   p touches the footprint
static inline uint32 koord_distance_to_rect(const koord &p, const koord &pos, const koord &size)
{
	return koord_distance(p, koord(clamp<sint16>(p.x, pos.x, pos.x + size.x - 1),
	                               clamp<sint16>(p.y, pos.y, pos.y + size.y - 1)));
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


// All axial offsets within hex distance r of the origin — the hex disc
// koord_distance(koord(0, 0), d) <= r.  A plain (-r..r)^2 double loop
// is NOT a disc in axial coords: it reaches 2r hex steps along the
// third (NE–SW) axis.  Rows run dy = -r..r; within a row, dx covers
// the axial square clipped by |dx + dy| <= r.
//
//   for (koord d : hex_disc(radius)) { ... center + d ... }
class hex_disc
{
	sint16 r;

	static sint16 min_dx(sint16 dy, sint16 r) { return dy < 0 ? -r - dy : -r; }
	static sint16 max_dx(sint16 dy, sint16 r) { return dy > 0 ? r - dy : r; }

public:
	explicit hex_disc(sint16 radius) : r(radius) {}

	/// Number of offsets in the disc, 3r(r+1)+1.
	uint32 size() const { return 3u * (uint32)r * ((uint32)r + 1u) + 1u; }

	/// The index-th offset in iteration order, 0 <= index < size():
	/// a uniformly random tile of the disc from one random number.
	koord at(uint32 index) const
	{
		sint16 dy = -r;
		for(;;) {
			const uint32 row = 2u * (uint32)r + 1u - (uint32)(dy < 0 ? -dy : dy);
			if (index < row) {
				break;
			}
			index -= row;
			dy++;
		}
		return koord((sint16)(min_dx(dy, r) + (sint32)index), dy);
	}

	class iterator
	{
		koord d;
		sint16 r;

	public:
		iterator(koord start, sint16 radius) : d(start), r(radius) {}
		koord operator * () const { return d; }
		bool operator != (const iterator &other) const { return d != other.d; }
		iterator &operator ++ ()
		{
			if (d.x < max_dx(d.y, r)) {
				d.x++;
			}
			else {
				d.y++;
				d.x = min_dx(d.y, r);
			}
			return *this;
		}
	};

	iterator begin() const { return iterator(koord(min_dx(-r, r), -r), r); }
	iterator end()   const { return iterator(koord(min_dx(r + 1, r), r + 1), r); }
};
#endif
