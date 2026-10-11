/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 */

#ifndef DATAOBJ_HEIGHT_FIELD_SAMPLE_H
#define DATAOBJ_HEIGHT_FIELD_SAMPLE_H

// Samples an imported relief (height_map_loader_t) at hex world
// vertices.  Pure functions over koord.h, so tools/hex_heightfield_test
// compiles them (and dataobj/koord_hex.cc) as they are.

#include <cassert>
#include <cmath>

#include "koord.h"


/**
 * Height field value at the continuous axial position (@p q, @p r).
 *
 * @p field holds one value per pixel, @p w x @p h, row-major; pixel
 * (i, j) belongs to tile (q = i, r = j) and sits at its centre, as the
 * relief preview in welt_gui_t draws it.  Between pixel centres the
 * value is interpolated bilinearly; past the image edge it keeps the
 * edge pixel's value.
 */
static inline double height_field_at_axial(const sint8 *field, sint16 w, sint16 h, double q, double r)
{
	assert(w > 0  &&  h > 0);
	const double fq = q < 0.0 ? 0.0 : (q > w - 1 ? w - 1 : q);
	const double fr = r < 0.0 ? 0.0 : (r > h - 1 ? h - 1 : r);
	const sint32 i0 = (sint32)std::floor(fq);
	const sint32 j0 = (sint32)std::floor(fr);
	const sint32 i1 = i0 + 1 < w ? i0 + 1 : i0;
	const sint32 j1 = j0 + 1 < h ? j0 + 1 : j0;
	const double tq = fq - i0;
	const double tr = fr - j0;
	const double top    = field[i0 + j0 * (sint32)w] * (1.0 - tq) + field[i1 + j0 * (sint32)w] * tq;
	const double bottom = field[i0 + j1 * (sint32)w] * (1.0 - tq) + field[i1 + j1 * (sint32)w] * tq;
	return top * (1.0 - tr) + bottom * tr;
}


/**
 * Height of world vertex @p v for a map imported from @p field
 * (@p w x @p h pixels, the map is @p w x @p h tiles).
 *
 * The vertex is placed with hex_vertex_pos(), the position the noise
 * generator samples at, then taken back to axial coordinates through the
 * tile-centre formula documented there, (1.5*q, sqrt(3)*(r + q/2)).
 * The three (tile, corner) names of a vertex give the same position, so
 * they give the same height.  The result is rounded and raised by one:
 * welt_gui_t's preview draws water up to groundwater - 1, and a vertex
 * counts as water at groundwater.
 */
static inline sint8 height_field_vertex_height(const sint8 *field, sint16 w, sint16 h, hex_vertex_t v)
{
	const hex_pos_t p = hex_vertex_pos(v);
	const double q = p.x / 1.5;
	const double r = p.y / 1.7320508075688772 - q / 2.0;
	const sint32 height = (sint32)std::floor(height_field_at_axial(field, w, h, q, r) + 0.5) + 1;
	return (sint8)(height > 127 ? 127 : height);
}

#endif
