/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 */

#ifndef DESCRIPTOR_SPRITE_DIR_H
#define DESCRIPTOR_SPRITE_DIR_H


#include "../dataobj/koord.h"
#include "../dataobj/ribi.h"


/**
 * Slot of the 8-direction sprite table (ribi_t::_dir) for an object heading
 * along the axial step @p step.
 *
 * Vehicle, city car, pedestrian and animal art comes in 8 directions named
 * after the square grid it was drawn for, and each name is a heading on
 * screen in that 2:1 dimetric view: N runs up the right-hand diagonal
 * (2,-1), E (2,1), S (-2,1), W (-2,-1), NE straight right, SE straight
 * down, SW left, NW up.  A hex heading takes the slot whose screen heading
 * is nearest to its own under display/hex_proj.h, not the slot of the same
 * name: hex N runs straight up the screen and shows the NW sprite.
 *
 *     heading               step     on screen  slot  off by
 *     N                     ( 0,-1)  ( 0,-2)    NW     0°
 *     NE                    ( 1,-1)  ( 3,-1)    N      8.1°
 *     SE                    ( 1, 0)  ( 3, 1)    E      8.1°
 *     S                     ( 0, 1)  ( 0, 2)    SE     0°
 *     SW                    (-1, 1)  (-3, 1)    S      8.1°
 *     NW                    (-1, 0)  (-3,-1)    W      8.1°
 *     SE-S bend chord       ( 1, 1)  ( 3, 3)    E     18.4°
 *     NW-N bend chord       (-1,-1)  (-3,-3)    W     18.4°
 *
 * Those are all the steps a vehicle can head along: the other four chords
 * of 60 degree bends are stored as NE or SW
 * (vehicle_base_t::calc_chord_direction) and show that edge's slot, up to
 * 26.6° off.  Any other step, (0,0) included, gives dir_invalid.
 * tools/hex_dir_test derives the table from hex_proj.h.
 */
inline ribi_t::dir sprite_dir_of_heading(koord step)
{
	static const struct { sint16 q, r; ribi_t::dir dir; } table[] = {
		{  0, -1, ribi_t::dir_northwest },
		{  1, -1, ribi_t::dir_north },
		{  1,  0, ribi_t::dir_east },
		{  0,  1, ribi_t::dir_southeast },
		{ -1,  1, ribi_t::dir_south },
		{ -1,  0, ribi_t::dir_west },
		{  1,  1, ribi_t::dir_east },
		{ -1, -1, ribi_t::dir_west },
	};
	for(  const auto &e : table  ) {
		if(  e.q == step.x  &&  e.r == step.y  ) {
			return e.dir;
		}
	}
	return ribi_t::dir_invalid;
}

#endif
