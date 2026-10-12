/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 */

#ifndef DATAOBJ_HEIGHT_FIELD_SAMPLE_H
#define DATAOBJ_HEIGHT_FIELD_SAMPLE_H

// Samples an imported relief (height_map_loader_t) at hex world
// vertices, in the image frame of documentation/map-shape.md.  Pure functions over koord.h, so tools/hex_heightfield_test
// compiles them (and dataobj/koord_hex.cc) as they are.

#include <cassert>
#include <cmath>

#include "koord.h"


/**
 * Height field value at the continuous pixel position (@p x, @p y).
 *
 * @p field holds one value per pixel, @p w x @p h, row-major; pixel
 * (i, j) sits at integer (x, y) = (i, j).  Between pixel centres the
 * value is interpolated bilinearly; past the image edge it keeps the
 * edge pixel's value.
 */
static inline double height_field_at_pixel(const sint8 *field, sint16 w, sint16 h, double x, double y)
{
	assert(w > 0  &&  h > 0);
	const double fx = x < 0.0 ? 0.0 : (x > w - 1 ? w - 1 : x);
	const double fy = y < 0.0 ? 0.0 : (y > h - 1 ? h - 1 : y);
	const sint32 i0 = (sint32)std::floor(fx);
	const sint32 j0 = (sint32)std::floor(fy);
	const sint32 i1 = i0 + 1 < w ? i0 + 1 : i0;
	const sint32 j1 = j0 + 1 < h ? j0 + 1 : j0;
	const double tx = fx - i0;
	const double ty = fy - j0;
	const double top    = field[i0 + j0 * (sint32)w] * (1.0 - tx) + field[i1 + j0 * (sint32)w] * tx;
	const double bottom = field[i0 + j1 * (sint32)w] * (1.0 - tx) + field[i1 + j1 * (sint32)w] * tx;
	return top * (1.0 - ty) + bottom * ty;
}


/**
 * Height of world vertex @p v for a map imported from @p field
 * (@p w x @p h pixels) into the hexagon of cut @p cut = D - 1.
 *
 * The vertex is placed with hex_vertex_pos(), the position the noise
 * generator samples at, then taken to the image frame of
 * map_shape_t::get_image_row(): the physical plane in offset
 * coordinates, pixel column x = p.x / 1.5 and pixel row
 * y = p.y / sqrt(3) - cut / 2.  The three (tile, corner) names of a
 * vertex give the same position, so they give the same height.  The
 * result is rounded and raised by one: welt_gui_t's preview draws water
 * up to groundwater - 1, and a vertex counts as water at groundwater.
 *
 * Between pixels the value is a multiple of 1/6 of a level, so a tie
 * at .5 is exact; the double arithmetic lands on either side of it, and
 * the nudge makes ties round up on every platform.
 */
static inline sint8 height_field_vertex_height(const sint8 *field, sint16 w, sint16 h, hex_vertex_t v, sint32 cut)
{
	const hex_pos_t p = hex_vertex_pos(v);
	const double x = p.x / 1.5;
	const double y = p.y / 1.7320508075688772 - 0.5 * cut;
	const sint32 height = (sint32)std::floor(height_field_at_pixel(field, w, h, x, y) + 0.5 + 1e-9) + 1;
	return (sint8)(height > 127 ? 127 : height);
}

#endif
