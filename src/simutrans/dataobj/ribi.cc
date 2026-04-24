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
const ribi_t::_nesw ribi_t::nesw;;

// same like the layouts of buildings
const ribi_t::ribi ribi_t::layout_to_ribi[4] = {
	south,
	east,
	north,
	west
};

const int ribi_t::flags[16] = {
	0,                    // none
	single | straight_ns, // north
	single | straight_ew, // east
	bend | twoway,        // north-east
	single | straight_ns, // south
	straight_ns | twoway, // north-south
	bend | twoway,        // south-east
	threeway,             // north-south-east
	single | straight_ew, // west
	bend | twoway,        // north-west
	straight_ew | twoway, // east-west
	threeway,             // north-east-west
	bend | twoway,        // south-west
	threeway,             // north-south-west
	threeway,             // south-east-west
	threeway,             // all
};

const ribi_t::ribi ribi_t::backwards[16] = {
	all,        // none
	south,      // north
	west,       // east
	southwest,  // north-east
	north,      // south
	northsouth, // north-south
	northwest,  // south-east
	west,       // north-south-east
	east,       // west
	southeast,  // north-west
	eastwest,   // east-west
	south,      // north-east-west
	northeast,  // south-west
	east,       // north-south-west
	north,      // south-east-west
	none        // all
};

const ribi_t::ribi ribi_t::doppelr[16] = {
	none,       // none
	northsouth, // north
	eastwest,   // east
	none,       // north-east
	northsouth, // south
	northsouth, // north-south
	none,       // south-east
	none,       // north-south-east
	eastwest,   // west
	none,       // north-west
	eastwest,   // east-west
	none,       // north-east-west
	none,       // south-west
	none,       // north-south-west
	none,       // south-east-west
	none        // all
};




// HEX-PORT: the old slope_t::flags[81] table is gone.  Under the 6-corner
// base-3 encoding there are 729 slopes and hand-enumerating flags for each
// is silly; the predicates (is_single, is_way, is_all_up, max_diff, ...)
// are computed inline in ribi.h from the slope value itself.


const slope_t::type slope_from_ribi[16] = {
	0,
	slope_t::south,
	slope_t::west,
	0,
	slope_t::north,
	0,
	0,
	0,
	slope_t::east,
	0,
	0,
	0,
	0,
	0,
	0,
	0
};


const ribi_t::dir ribi_t::dirs[16] = {
	dir_invalid,   // none
	dir_north,     // north
	dir_east,      // east
	dir_northeast, // north-east
	dir_south,     // south
	dir_invalid,   // north-south
	dir_southeast, // south-east
	dir_invalid,   // north-south-east
	dir_west,      // west
	dir_northwest, // north-west
	dir_invalid,   // east-west
	dir_invalid,   // north-east-west
	dir_southwest, // south-west
	dir_invalid,   // north-south-west
	dir_invalid,   // south-east-west
	dir_invalid    // all
};

const char ribi_t::names[16][5] = {
	"    ",
	"N   ",
	" E  ",
	"NE  ",
	"  S ",
	"N S ",
	" ES ",
	"NES ",
	"   W",
	"N  W",
	" E W",
	"NE W",
	"  SW",
	"N SW",
	" ESW",
	"NESW",
};


// north slope (low edge N, S corners raised) → ribi_t::south (the
// high side is south).  Under hex with 6-corner base-3 encoding only
// the 4 square-named 2-corner slopes at heights 1 and 2 have a
// 4-bit-ribi representation; the 4 hex-only edge slopes (NE/NW/SE/SW)
// return none until ribi widens.
ribi_t::ribi ribi_type(slope_t::type hang)
{
	switch (hang) {
		case slope_t::north:       return ribi_t::south;
		case 2 * slope_t::north:   return ribi_t::south;
		case slope_t::south:       return ribi_t::north;
		case 2 * slope_t::south:   return ribi_t::north;
		case slope_t::east:        return ribi_t::west;
		case 2 * slope_t::east:    return ribi_t::west;
		case slope_t::west:        return ribi_t::east;
		case 2 * slope_t::west:    return ribi_t::east;
		default:                   return ribi_t::none;
	}
}


ribi_t::ribi ribi_typ_intern(sint16 dx, sint16 dy)
{
	ribi_t::ribi ribi = ribi_t::none;

	if(dx<0) {
		ribi |= ribi_t::west;
	}
	else if(dx>0) {
		ribi |= ribi_t::east;
	}

	if(dy<0) {
		ribi |= ribi_t::north;
	}
	else if(dy>0) {
		ribi |= ribi_t::south;
	}
	return ribi;
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
 * check, if two directions are orthogonal
 * works with diagonals too
 */
bool ribi_t::is_perpendicular(ribi x, ribi y)
{
	// for straight direction x use doppelr lookup table
	if(is_straight(x)) {
		return (doppelr[x] | doppelr[y]) == all;
	}
	// now diagonals (more tricky)
	if(x!=y) {
		return ((x-y)%3)==0;
	}
	// ok, then they are not orthogonal
	return false;
}


sint16 get_sloping_upwards(const slope_t::type slope, const ribi_t::ribi from)
{
	// slope upwards relative to direction 'from'
	const slope_t::type from_slope = slope_type(from);

	if (from_slope == slope) {
		return 1;
	}
	else if (2*from_slope == slope) {
		return 2;
	}
	return 0;
}


slope_t::type slope_type(koord dir)
{
	if(dir.x == 0) {
		if(dir.y < 0) {            // north direction -> south slope
			return slope_t::south;
		}
		if(dir.y > 0) {
			return slope_t::north; // south direction -> north slope
		}
	}
	if(dir.y == 0) {
		if(dir.x < 0) {
			return slope_t::east;  // west direction -> east slope
		}
		if(dir.x > 0) {
			return slope_t::west;  // east direction -> west slope
		}
	}
	return slope_t::flat;          // ???
}


slope_t::type slope_type(ribi_t::ribi r)
{
	return slope_from_ribi[r];
}
