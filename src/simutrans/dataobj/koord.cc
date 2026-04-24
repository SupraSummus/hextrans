/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 */

#include "koord.h"
#include "loadsave.h"
#include "../display/scr_coord.h"
#include "../utils/simrandom.h"
#include "../simconst.h"
#include "../simdebug.h"


// default: close and far away does not matter
uint32 koord::locality_factor = 10000;


const scr_coord scr_coord::invalid(-1, -1);

const scr_size scr_size::invalid(-1, -1);
const scr_size scr_size::inf(0x7fffffff, 0x7fffffff);

const koord koord::invalid(-1, -1);

// Flat-top hex axial neighbours, clockwise starting from the SE
// neighbour. The axial +q axis points 30° south of due-east in screen
// space, so axial (1, 0) is the SE neighbour, NOT a due-east one
// (flat-top hexes have no due-east neighbour — east is a vertex).
// Bit position in ribi_t::ribi matches this index: bit i set ↔
// neighbours[i] is part of the ribi.
//   SE, S, SW, NW, N, NE
// Iterate with
//   for (size_t i = 0; i < lengthof(koord::neighbours); i++)
const koord koord::neighbours[] = {
	koord(  1,  0 ), // SE  (bit 0)
	koord(  0,  1 ), // S   (bit 1)
	koord( -1,  1 ), // SW  (bit 2)
	koord( -1,  0 ), // NW  (bit 3)
	koord(  0, -1 ), // N   (bit 4)
	koord(  1, -1 ), // NE  (bit 5)
};


// ribi → koord displacement: sum of neighbours[i] for each set bit.
// Single-bit ribi returns the matching neighbour step; multi-bit
// returns the vector sum (which is (0, 0) for any straight pair).
// Replaces the old from_ribi[16] lookup table.
koord::koord(ribi_t::ribi r) : x(0), y(0)
{
	for (int i = 0; i < 6; i++) {
		if (r & (1 << i)) {
			*this += koord::neighbours[i];
		}
	}
}


// Step toward the raised side of a slope.  Used by bridge / tunnel /
// ramp builders as "which way does this slope face uphill".  The 6
// hex edge slopes (2 adjacent corners raised, at single and double
// height) each map to one of the 6 hex neighbours: edge between
// corners i and (i+1)%6 of the centre tile has the neighbour at
// koord::neighbours[i] across it, and that neighbour direction is
// the uphill step.  The 2 legacy "square diagonal" slopes (east =
// NW+SW, west = NE+SE) kept as-is for backwards compatibility with
// square callers until those get audited.
koord::koord(slope_t::type slope) : x(0), y(0)
{
	switch (slope) {
		// 6 hex edge slopes (slope.north / .south are aliases for
		// raised_SE+raised_SW / raised_NW+raised_NE respectively).
		case slope_t::raised_E  + slope_t::raised_SE:
		case 2 * (slope_t::raised_E  + slope_t::raised_SE): x =  1;          break; // uphill SE (neighbours[0])
		case slope_t::north:     case 2 * slope_t::north:   y =  1;          break; // uphill S  (neighbours[1])
		case slope_t::raised_SW + slope_t::raised_W:
		case 2 * (slope_t::raised_SW + slope_t::raised_W):  x = -1; y =  1;  break; // uphill SW (neighbours[2])
		case slope_t::raised_W  + slope_t::raised_NW:
		case 2 * (slope_t::raised_W  + slope_t::raised_NW): x = -1;          break; // uphill NW (neighbours[3])
		case slope_t::south:     case 2 * slope_t::south:   y = -1;          break; // uphill N  (neighbours[4])
		case slope_t::raised_NE + slope_t::raised_E:
		case 2 * (slope_t::raised_NE + slope_t::raised_E):  x =  1; y = -1;  break; // uphill NE (neighbours[5])

		// Legacy square-diagonal slopes (NW+SW and NE+SE): kept for
		// square callers; approximate uphill on the hex grid.
		case slope_t::east:      case 2 * slope_t::east:  x = -1; break; // uphill hex-NW ≈ W
		case slope_t::west:      case 2 * slope_t::west:  x =  1; break; // uphill hex-SE ≈ E
		default: break;
	}
}

// Static factory matching koord(ribi_t::ribi); see koord.h.
koord koord::step(ribi_t::ribi r)
{
	return koord(r);
}


// See the comment at the declaration in koord.h.
void koord::rotate90(sint16)
{
	dbg->fatal("koord::rotate90",
		"90° is not a hex lattice symmetry — the rotation cascade "
		"(karte_t::rotate90 and every obj_t::rotate90 override) "
		"needs redesigning for the hex port.  See TODO.md.");
}


void koord::rdwr(loadsave_t *file)
{
	xml_tag_t k( file, "koord" );
	file->rdwr_short(x);
	file->rdwr_short(y);
}


// Hex vertex topology — see koord.h for the convention.
void vertex_owners(koord tile, hex_corner_t::type c, hex_vertex_t out[3])
{
	const uint8 dir_a = (uint8)(((uint8)c + 5) % 6);
	const uint8 dir_b = (uint8)c;
	out[0].tile   = tile;
	out[0].corner = c;
	out[1].tile   = tile + koord::neighbours[dir_a];
	out[1].corner = (hex_corner_t::type)(((uint8)c + 2) % 6);
	out[2].tile   = tile + koord::neighbours[dir_b];
	out[2].corner = (hex_corner_t::type)(((uint8)c + 4) % 6);
}


// Canonical lex-min (tile, corner) for a world vertex.  See
// documentation/hex-vertex-storage.md — the 6-way case table below
// is a closed form of running vertex_owners() and picking the
// lex-smallest tile.
hex_vertex_t canonical_vertex(hex_vertex_t v)
{
	switch (v.corner) {
		case hex_corner_t::E:  return v;
		case hex_corner_t::SE: return v;
		case hex_corner_t::SW: return { v.tile + koord(-1,  1), hex_corner_t::E  };
		case hex_corner_t::W:  return { v.tile + koord(-1,  0), hex_corner_t::SE };
		case hex_corner_t::NW: return { v.tile + koord(-1,  0), hex_corner_t::E  };
		case hex_corner_t::NE: return { v.tile + koord( 0, -1), hex_corner_t::SE };
		default: break; // unreachable
	}
	return v;
}


uint32 vertex_slot_count(sint16 W, sint16 H)
{
	return (uint32)(W + 1) * (uint32)(H + 2) * 2u;
}


uint32 vertex_slot_index(hex_vertex_t v, sint16 W)
{
	// row-major: tile-x ranges fast with stride (W+1); shift tile
	// coords from [-1, W-1] x [-1, H] onto [0, W] x [0, H+1].
	const uint32 q = (uint32)(v.tile.x + 1);
	const uint32 r = (uint32)(v.tile.y + 1);
	const uint32 w = (uint32)(W + 1);
	const uint32 corner_bit = (v.corner == hex_corner_t::SE) ? 1u : 0u;
	return (q + r * w) * 2u + corner_bit;
}


hex_pos_t hex_vertex_pos(hex_vertex_t v)
{
	static constexpr double HEX_SQRT3 = 1.7320508075688772;
	// Per-corner offsets (R = 1) at angles c * 60° clockwise from E.
	static constexpr double OX[hex_corner_t::count] = {  1.0,  0.5, -0.5, -1.0, -0.5,  0.5 };
	static constexpr double OY[hex_corner_t::count] = {  0.0,  HEX_SQRT3 * 0.5,  HEX_SQRT3 * 0.5,  0.0, -HEX_SQRT3 * 0.5, -HEX_SQRT3 * 0.5 };
	const double cx = 1.5 * v.tile.x;
	const double cy = HEX_SQRT3 * (v.tile.y + 0.5 * v.tile.x);
	return { cx + OX[v.corner], cy + OY[v.corner] };
}


// for debug messages...
const char *koord::get_str() const
{
	static char pos_str[32];
	if(x==-1  &&  y==-1) {
		return "koord invalid";
	}
	sprintf( pos_str, "%i,%i", x, y );
	return pos_str;
}


const char *koord::get_fullstr() const
{
	static char pos_str[32];
	if(x==-1  &&  y==-1) {
		return "koord invalid";
	}
	sprintf( pos_str, "(%i,%i)", x, y );
	return pos_str;
}

// obey order of simrand among different compilers
koord koord::koord_random( uint16 xrange, uint16 yrange )
{
	koord ret;
	ret.x = simrand(xrange);
	ret.y = simrand(yrange);
	return ret;
}
