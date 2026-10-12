/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 */

#ifndef VEHICLE_SCREEN_CHORD_H
#define VEHICLE_SCREEN_CHORD_H


#include <cmath>
#include <cstdlib>

#include "../dataobj/koord.h"
#include "../display/hex_proj.h"
#include "../simconst.h"
#include "../simtypes.h"
#include "../simunits.h"


/**
 * Where a moving object is drawn inside the tile it crosses.
 *
 * An object crosses a tile on the chord from the middle of the edge it
 * enters by to the middle of the edge it leaves by.  Between neighbours
 * `prev` and `next` of the tile, the chord is half of the two-tile
 * span `next - prev`: a straight run crosses one tile pitch, a 60 degree
 * turn (edges 120 degrees apart) a chord cos 30 of that, a 120 degree turn
 * (edges 60 degrees apart) half of it.
 *
 * vehicle_base_t keeps the screen vector of the chord in dx, dy and the
 * offset of the entry edge middle in xoff, yoff; both are projections
 * through display/hex_proj.h of integer axial vectors.  The functions here
 * are the only place that turns axial chords into screen offsets.
 * tools/hex_chord_test checks them against hex_proj.h.
 */


/// Steps to cross a tile on a 60 degree bend: VEHICLE_STEPS_PER_TILE cos 30.
static const uint16 STEPS_PER_WIDE_BEND = 222;
/// Steps to cross a tile on a 120 degree bend: half a tile.
static const uint16 STEPS_PER_SHARP_BEND = VEHICLE_STEPS_PER_TILE / 2;


/// Axial span `next - prev` of the chord through a tile, from the steps
/// @p to_prev and @p to_next out of it.  A tile entered from nowhere
/// (to_prev zero) or turned on (to_prev equal to to_next) is crossed
/// straight towards next, and a tile ended on (to_next zero) straight on
/// from prev.
inline koord chord_span(koord to_prev, koord to_next)
{
	const koord zero(0, 0);
	if(  to_prev == zero  ||  to_prev == to_next  ) {
		return to_next * 2;
	}
	if(  to_next == zero  ) {
		return -to_prev * 2;
	}
	return to_next - to_prev;
}


/// Steps to cross a tile on the chord with axial @p span: a whole tile when
/// it is twice an edge step (straight) or zero, half when it is one edge
/// step, and STEPS_PER_WIDE_BEND for the off-axis span of a 60 degree bend.
inline uint16 chord_tile_steps(koord span)
{
	const uint32 len = koord_distance(span, koord(0, 0));
	if(  len == 1  ) {
		return STEPS_PER_SHARP_BEND;
	}
	const bool on_axis = span.x == 0  ||  span.y == 0  ||  span.x == -span.y;
	if(  len == 2  &&  !on_axis  ) {
		return STEPS_PER_WIDE_BEND;
	}
	return VEHICLE_STEPS_PER_TILE;
}


/// Screen vector of the chord with axial @p span, in eighths of the raster
/// width.  hex_proj.h at raster width 4 (unit u = 1) gives the span in u,
/// which is the chord, half the span, in u/2 = W/8.  The longest, a straight
/// NE or SE run, is (6, +-2).
inline koord chord_screen_step(koord span)
{
	return koord((sint16)hex_screen_dx(span.x, 4), (sint16)hex_screen_dy(span.x, span.y, 4));
}


/// Inverse of chord_screen_step(): the axial span whose chord is @p step.
/// False if no integer span projects to it.
inline bool chord_span_of_screen_step(koord step, koord &span)
{
	const sint16 q = step.x / 3;
	if(  (step.y - q) % 2 != 0  ) {
		return false;
	}
	span = koord(q, (sint16)((step.y - q) / 2));
	return chord_screen_step(span) == step;
}


/// Offset, in the 64-unit object basis of xoff and yoff, of the middle of
/// the tile edge towards the neighbour @p edge_step.
inline koord edge_middle_offset(koord edge_step)
{
	const sint16 W = OBJECT_OFFSET_STEPS * 4;
	return koord((sint16)(hex_screen_dx(edge_step.x, W) / 2), (sint16)(hex_screen_dy(edge_step.x, edge_step.y, W) / 2));
}


/// Offset, in the 64-unit object basis, of the start of the chord @p step
/// (as from chord_screen_step) for an object that entered the tile from
/// the neighbour @p to_prev.  That is the middle of the edge towards
/// to_prev when the chord leads from there to the middle of another edge,
/// else the point half the chord before the tile centre, so the chord
/// runs symmetrically through it.
inline koord chord_entry_offset(koord to_prev, koord step)
{
	koord span;
	if(  koord_distance(to_prev, koord(0, 0)) == 1  &&  chord_span_of_screen_step(step, span)  ) {
		const koord to_next = span + to_prev;
		if(  to_next != to_prev  &&  koord_distance(to_next, koord(0, 0)) == 1  ) {
			return edge_middle_offset(to_prev);
		}
	}
	// chord in W/8 is 8 units of the 64-unit basis; start half of it back
	return koord((sint16)(-4 * step.x), (sint16)(-4 * step.y));
}


/// Pixel offset, at raster width @p W, of an object @p s steps along the
/// chord @p step of a tile @p tile_steps long.  The offset moves in whole
/// multiples of the chord's smallest lattice vector, so x and y change in
/// lockstep and the object does not wobble across its line.
inline koord chord_screen_offset(koord step, uint32 s, uint32 tile_steps, sint16 W)
{
	sint32 g = std::abs(step.x);
	for(  sint32 b = std::abs(step.y);  b != 0;  ) {
		const sint32 t = g % b;
		g = b;
		b = t;
	}
	if(  g == 0  ) {
		return koord(0, 0);
	}
	// The chord is g lattice vectors of step / g, each W / 8 pixels long
	const sint32 n = (sint32)((s * (uint32)g * (uint32)W) / (8u * tile_steps));
	return koord((sint16)(n * (step.x / g)), (sint16)(n * (step.y / g)));
}


/// Pixel offset, at raster width @p W, @p s steps to the left of an object
/// heading along the chord @p step, VEHICLE_STEPS_PER_TILE steps making the
/// distance between neighbouring tile centres.  Square to the heading on
/// the ground, which hex_proj.h draws with x stretched by the factor that
/// gives a SE and an S step the same length.
inline koord chord_left_offset(koord step, sint32 s, sint16 W)
{
	// screen of one S and one SE step, in units of W / 4
	const double s_y = hex_screen_dy(0, 1, 4);
	const double se_x = hex_screen_dx(1, 4), se_y = hex_screen_dy(1, 0, 4);
	// ground length of a screen vector (x, y) is |(x / k, y)|
	const double k = se_x / std::sqrt(s_y * s_y - se_y * se_y);
	const double len = std::sqrt((step.x / k) * (step.x / k) + (double)step.y * step.y);
	if(  len == 0.0  ) {
		return koord(0, 0);
	}
	// left of heading (x, y) on the ground is (y, -x); a tile pitch is s_y
	// in W/4, so per step it is s_y * W / (4 * VEHICLE_STEPS_PER_TILE)
	const double scale = (double)s * s_y * W / (4.0 * VEHICLE_STEPS_PER_TILE * len);
	return koord((sint16)std::lround(step.y * k * scale), (sint16)std::lround(-step.x / k * scale));
}


/// Turns a vector of the 64-unit object basis or a chord step (as from
/// chord_screen_step) along with the map, @p steps * 60 degrees as
/// hex_axial_rotate.  @p unit is 4 for the object basis, 1 for a chord.
/// False, leaving @p v unchanged, if it is not one of those projections.
inline bool rotate_screen_lattice_vector(koord &v, sint16 unit, int steps)
{
	if(  v.x % unit != 0  ||  v.y % unit != 0  ) {
		return false;
	}
	koord span;
	if(  !chord_span_of_screen_step(koord(v.x / unit, v.y / unit), span)  ) {
		return false;
	}
	const koord r = chord_screen_step(hex_axial_rotate(span, steps));
	v = koord((sint16)(r.x * unit), (sint16)(r.y * unit));
	return true;
}


#endif
