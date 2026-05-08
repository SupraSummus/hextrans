/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 */

#include <stdio.h>
#include "../simdebug.h"
#include "../simconst.h"
#include "ribi.h"
#include "koord.h"
#include "koord3d.h"

// since we have now a dummy function instead an array
const ribi_t::_nesw ribi_t::nesw;

// Building layout → primary hex-edge direction.  Indices 0..3
// preserve the old square `layout_to_ribi[4]` semantic by rename
// only: old south/east/north/west are the same displacement vectors
// as hex south/southeast/north/northwest, so 4-rotation callers
// (`[layout & 3]`) continue to work on the same 4 visual rotations.
// Indices 4..5 extend with the two hex-only directions (NE, SW) for
// future 6-rotation building support.
const ribi_t::ribi ribi_t::layout_to_ribi[6] = {
	south,     // was old south  (0, 1)
	southeast, // was old east   (1, 0)
	north,     // was old north  (0,-1)
	northwest, // was old west   (-1,0)
	northeast, // hex-only       (1,-1)
	southwest, // hex-only       (-1,1)
};


// Runtime-initialised name table: see ribi.h for the column layout.
char ribi_t::names[64][7] = {};

namespace {
	struct ribi_names_init {
		ribi_names_init()
		{
			for (int i = 0; i < 64; i++) {
				ribi_t::names[i][0] = (i & ribi_t::southeast) ? 'e' : ' ';
				ribi_t::names[i][1] = (i & ribi_t::south)     ? 'S' : ' ';
				ribi_t::names[i][2] = (i & ribi_t::southwest) ? 'w' : ' ';
				ribi_t::names[i][3] = (i & ribi_t::northwest) ? 'W' : ' ';
				ribi_t::names[i][4] = (i & ribi_t::north)     ? 'N' : ' ';
				ribi_t::names[i][5] = (i & ribi_t::northeast) ? 'E' : ' ';
				ribi_t::names[i][6] = 0;
			}
		}
	};
	ribi_names_init _ribi_names_init;
}


ribi_t::dir ribi_t::get_dir(ribi x)
{
	switch (x) {
		case north:     return dir_north;
		case northeast: return dir_northeast;
		case southeast: return dir_southeast;
		case south:     return dir_south;
		case southwest: return dir_southwest;
		case northwest: return dir_northwest;
		default:        return dir_invalid;
	}
}


// Slope → direction of travel that goes UP this slope.  Convention
// "going X walks up slope named after low-edge opposite(X)": e.g.
// going north walks up slope_t::south_narrow (N corners raised).  Two
// kinds of slope share each direction: 2-corner narrow edge and the
// 4-corner wide variant.  Track on either climbs the same 0→1 path
// along the axis; off-axis ground shape varies.  Planar double-height
// (`*_double`) slopes face the same direction with a 0→2 climb.
// Legacy square diagonals (::east / ::west) project onto the closest
// hex direction.
ribi_t::ribi ribi_type(slope_t::type hang)
{
	switch (hang) {
		case slope_t::north_narrow:     case slope_t::north_wide:     case slope_t::north_double:     return ribi_t::south;
		case slope_t::south_narrow:     case slope_t::south_wide:     case slope_t::south_double:     return ribi_t::north;
		case slope_t::northeast_narrow: case slope_t::northeast_wide: case slope_t::northeast_double: return ribi_t::southwest;
		case slope_t::southwest_narrow: case slope_t::southwest_wide: case slope_t::southwest_double: return ribi_t::northeast;
		case slope_t::southeast_narrow: case slope_t::southeast_wide: case slope_t::southeast_double: return ribi_t::northwest;
		case slope_t::northwest_narrow: case slope_t::northwest_wide: case slope_t::northwest_double: return ribi_t::southeast;
		case slope_t::east:                                                       return ribi_t::northwest;
		case slope_t::west:                                                       return ribi_t::southeast;
		default:                                                                    return ribi_t::none;
	}
}


ribi_t::ribi ribi_typ_intern(sint16 dx, sint16 dy)
{
	// koord displacement → ribi bit.
	//
	// Unit-step fast path (the 6 hex neighbours, bit positions match
	// koord::neighbours[]): SE=(1,0), S=(0,1), SW=(-1,1), NW=(-1,0),
	// N=(0,-1), NE=(1,-1).
	//
	// Multi-step: if the displacement lies on one of the 3 hex axes
	// — N-S (dx=0), NW-SE (dy=0), NE-SW (dx+dy=0) — return the
	// matching direction bit.  Matches the old 4-bit `ribi_typ_intern`
	// which OR-ed sign(dx) and sign(dy) bits so a straight multi-tile
	// displacement gave a single-axis ribi (runway builder, etc.
	// rely on this).  Off-axis multi-step inputs return none;
	// previously they gave the combo of two sign bits, which under
	// hex has no clean meaning.
	if (dx == 0 && dy == 0) return ribi_t::none;
	if (dy == 0) return dx > 0 ? ribi_t::southeast : ribi_t::northwest;
	if (dx == 0) return dy > 0 ? ribi_t::south     : ribi_t::north;
	if (dx + dy == 0) return dx > 0 ? ribi_t::northeast : ribi_t::southwest;
	// Off-axis displacement — (1,1) and (-1,-1) are the old-square
	// diagonals that have no hex-neighbour equivalent, and anything
	// further off-axis isn't on a single hex axis either.
	return ribi_t::none;
}


ribi_t::ribi ribi_type(const koord& dir)
{
	return ribi_typ_intern(dir.x, dir.y);
}


ribi_t::ribi ribi_type(const koord3d& dir)
{
	return ribi_typ_intern(dir.x, dir.y);
}


/**
 * Check if two directions are orthogonal under hex geometry.  Hex
 * has 3 axes (N-S, NE-SW, NW-SE); "perpendicular" is a 2-axis
 * concept with no strict hex equivalent.  We approximate: two
 * straight ribis (single direction or axis-pair) are perpendicular
 * iff together they span more than one hex axis.  This correctly
 * returns false for (N, S) and (N, N|S) — same axis, parallel — and
 * true for any (single, single-on-different-axis) pair.  Callers
 * that used this for square-era 4-way cross-axis logic will see
 * different truth values on the 3rd hex axis (NE-SW) — a gameplay
 * landmine mid-port tracked in TODO.md.
 */
bool ribi_t::is_perpendicular(ribi x, ribi y)
{
	return is_straight(x) && is_straight(y) && !is_straight((ribi)(x | y));
}


sint16 get_sloping_upwards(const slope_t::type slope, const ribi_t::ribi from)
{
	// 1 if @p slope is the narrow / wide ramp facing direction @p from
	// (single-step climb), 2 if it's the planar double facing the same
	// direction (two-step climb), 0 otherwise.  The narrow/double pair
	// is identified by ribi_type(slope) returning the same uphill
	// direction; the height tier is then narrow vs planar-double.
	const slope_t::type narrow = slope_type(from);
	if (narrow == slope) return 1;
	if (slope_t::is_planar_double_edge(slope) && ribi_type(slope) == ribi_type(narrow)) return 2;
	return 0;
}


slope_t::type slope_type(koord dir)
{
	// Direction → slope walking which UP goes in that direction
	// (corners on the destination side raised).  Three hex axes:
	// N-S (dx==0), NW-SE (dy==0), NE-SW (dx+dy==0).  Mirrors the
	// ribi overload below.
	if (dir.x == 0) {
		if (dir.y < 0) return slope_t::south_narrow; // toward N
		if (dir.y > 0) return slope_t::north_narrow; // toward S
	}
	if (dir.y == 0) {
		if (dir.x < 0) return slope_t::southeast_narrow; // toward NW
		if (dir.x > 0) return slope_t::northwest_narrow; // toward SE
	}
	if (dir.x + dir.y == 0) {
		if (dir.x > 0) return slope_t::southwest_narrow; // toward NE
		if (dir.x < 0) return slope_t::northeast_narrow; // toward SW
	}
	return slope_t::flat;
}


slope_t::type slope_type(ribi_t::ribi r)
{
	// Single hex-edge direction → slope whose low edge is opposite
	// that direction (i.e. the destination corners are raised).
	switch (r) {
		case ribi_t::north:     return slope_t::south_narrow;
		case ribi_t::south:     return slope_t::north_narrow;
		case ribi_t::northeast: return slope_t::southwest_narrow;
		case ribi_t::southwest: return slope_t::northeast_narrow;
		case ribi_t::northwest: return slope_t::southeast_narrow;
		case ribi_t::southeast: return slope_t::northwest_narrow;
		default:                return slope_t::flat;
	}
}
