/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 */

#include "koord.h"
#include "loadsave.h"
#include "../display/scr_coord.h"
#include "../utils/simrandom.h"
#include "../simconst.h"


// default: close and far away does not matter
uint32 koord::locality_factor = 10000;


const scr_coord scr_coord::invalid(-1, -1);

const scr_size scr_size::invalid(-1, -1);
const scr_size scr_size::inf(0x7fffffff, 0x7fffffff);

const koord koord::invalid(-1, -1);
const koord koord::north(    0, -1);
const koord koord::east(     1,  0);
const koord koord::south(    0,  1);
const koord koord::west(   -1,  0);
const koord koord::nesw[] = {
	koord( 0, -1),
	koord( 1,  0),
	koord( 0,  1),
	koord(-1,  0)
};

// Flat-top hex axial neighbours, clockwise starting from the SE
// neighbour. The axial +q axis points 30° south of due-east in screen
// space, so axial (1, 0) is the SE neighbour, NOT a due-east one
// (flat-top hexes have no due-east neighbour — east is a vertex).
//   SE, S, SW, NW, N, NE
// Iterate with
//   for (size_t i = 0; i < lengthof(koord::neighbours); i++)
const koord koord::neighbours[] = {
	koord(  1,  0 ), // SE
	koord(  0,  1 ), // S
	koord( -1,  1 ), // SW
	koord( -1,  0 ), // NW
	koord(  0, -1 ), // N
	koord(  1, -1 ), // NE
};

const koord koord::from_ribi[] = {
	koord( 0,  0), // none
	koord( 0, -1), // north             (1)
	koord( 1,  0), // east              (2)
	koord( 1, -1), // north-east        (3)
	koord( 0,  1), // south             (4)
	koord( 0,  0), // north-south       (5)
	koord( 1,  1), // south-east        (6)
	koord( 1,  0), // north-south-east  (7)
	koord(-1,  0), // west              (8)
	koord(-1, -1), // north-west        (9)
	koord( 0,  0), // east-west         (10)
	koord( 0, -1), // north-east-west   (11)
	koord(-1,  1), // south-west        (12)
	koord(-1,  0), // north-south-west  (13)
	koord( 0,  1), // south-east-west   (14)
	koord( 0,  0)  // all
};


// Step toward the raised side of a slope.  Used by bridge / tunnel /
// ramp builders as "which way does this slope face uphill".  Under
// the 6-corner base-3 encoding this is a short switch — only the
// 4 square-named 2-corner slopes at heights 1 and 2 have a koord
// direction representable in the 4-bit ribi model; the 4 hex-only
// edge slopes are zero until ribi widens and the bridge builders
// gain hex-direction awareness.
koord::koord(slope_t::type slope) : x(0), y(0)
{
	switch (slope) {
		case slope_t::north:     case 2 * slope_t::north: y =  1; break; // uphill S
		case slope_t::south:     case 2 * slope_t::south: y = -1; break; // uphill N
		case slope_t::east:      case 2 * slope_t::east:  x = -1; break; // uphill hex-NW ≈ W
		case slope_t::west:      case 2 * slope_t::west:  x =  1; break; // uphill hex-SE ≈ E
		default: break;
	}
}


void koord::rdwr(loadsave_t *file)
{
	xml_tag_t k( file, "koord" );
	file->rdwr_short(x);
	file->rdwr_short(y);
}


// Hex vertex topology — see koord.h for the convention. This is the
// production version of the spike's vertex_owners() in
// tools/hex_spike/hex_spike.cc.
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
