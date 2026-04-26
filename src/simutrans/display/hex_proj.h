/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 */

#ifndef DISPLAY_HEX_PROJ_H
#define DISPLAY_HEX_PROJ_H


#include <cmath>

#include "scr_coord.h"
#include "../dataobj/koord.h"
#include "../simtypes.h"


/**
 * Hex screen projection (flat-top axial → 2D screen).
 *
 * The lattice the renderer port runs on is a clean integer
 * approximation of hex iso, with unit `u = W/4` (where `W` is the
 * current tile raster width):
 *
 *     +q step (column): ( 3·u,  u)
 *     +r step (row):    (   0, 2·u)
 *
 * It is NOT a regular hex tiling — adjacent tiles along the +q axis
 * are at screen distance `u·√10` while adjacent +r tiles are at
 * `2·u`, ~14% off true regular hex geometry.  What it preserves is
 * (1) the existing `IMG_SIZE × IMG_SIZE/2` bounding box, so the
 * legacy diamond sprites overlay on the right footprint until pakset
 * art lands; (2) the iso 2:1 row/column y-ratio
 * (row-step-y = 2·column-step-y), so vertical motion and z-elevation
 * keep the angles vehicles already expect; (3) all multipliers being
 * integer fractions of `W`, so the math stays in fixed-point like
 * the rest of the renderer.  See `TODO.md` → "Renderer port" for
 * the alternatives considered and why this one was picked.
 *
 * All helpers here take a tile `(q, r)` delta from a reference tile
 * and return screen pixel offsets; callers add fine scrolling,
 * raster-offset and z-elevation terms on top.
 */


/**
 * Sprite-canvas visible-centre anchor.
 *
 * Pakset sprites (cursor.pak, buildings, vehicles, trees) are
 * authored against the legacy "tile diamond inscribed in the bottom
 * half of a `W × W` canvas" layout: drawing at anchor `(X, Y)`
 * places the visible content centre at `(X + W/2, Y + 3W/4)`.  The
 * picker subtracts that offset on click → tile lookup; the synth
 * ground rasteriser places its `W × W/2` bbox so its centre lands
 * on the same y.  Both pin to this constant so the convention has
 * one source of truth.  Tested by
 * `tools/hex_proj_test/hex_proj_test.cc :: test_canvas_anchor_convention`.
 */
inline sint16 hex_visible_centre_y(sint16 W) { return 3 * W / 4; }


/// Screen-x of axial `+q` step in pixels (independent of `r`).
inline sint32 hex_screen_dx(sint32 dq, sint16 W)
{
	const sint32 u = (sint32)(W / 4);
	return dq * 3 * u;
}


/// Screen-y of an axial `(+q, +r)` step in pixels.
inline sint32 hex_screen_dy(sint32 dq, sint32 dr, sint16 W)
{
	const sint32 u = (sint32)(W / 4);
	return (dq + 2 * dr) * u;
}


/// Inverse: screen pixel offset → fractional axial `(q, r)`.  Used by
/// mouse picking; round to the nearest integer hex with
/// `hex_round_to_axial`.
inline void hex_screen_to_fractional(sint32 sx, sint32 sy, sint16 W,
                                     double &q_f, double &r_f)
{
	q_f = (double)sx * 4.0 / (3.0 * (double)W);
	r_f = ((double)sy * 4.0 / (double)W - q_f) * 0.5;
}


/// Round a fractional axial `(q_f, r_f)` to the screen-closest hex.
/// Two-stage: an initial cube-round to land within one cube step of
/// the answer, then a 7-hex screen-distance refinement (rounded hex
/// + its 6 neighbours).  The refinement is mandatory — this lattice
/// is irregular (column step 3u vs row step 2u, see file header), so
/// the cube-Voronoi cell and the screen-Voronoi cell don't match;
/// near cell boundaries the cube round can pick a hex that is
/// strictly farther in screen pixels than its neighbour.  The
/// screen-closest answer always lies within 1 cube step of the cube
/// round, so the 7-hex window is exact.  Returns axial in a `koord`
/// (`.x = q`, `.y = r`).
inline koord hex_round_to_axial(double q_f, double r_f)
{
	const double x = q_f, z = r_f, y = -x - z;
	double rx = std::round(x);
	double ry = std::round(y);
	double rz = std::round(z);
	const double dx = std::fabs(rx - x);
	const double dy = std::fabs(ry - y);
	const double dz = std::fabs(rz - z);
	if (dx > dy && dx > dz) {
		rx = -ry - rz;
	}
	else if (dy > dz) {
		ry = -rx - rz;
	}
	else {
		rz = -rx - ry;
	}

	// Refinement: scan the 7-hex window (cube-round + 6 neighbours)
	// and keep the screen-closest.  Squared screen distance from
	// fractional `(q_f, r_f)` to integer `(q, r)` in u² units is
	// `(3·dq)² + (dq + 2·dr)²` (= `hex_screen_dx/dy` with u = 1).
	// The 7 offsets are `koord::neighbours[]` (SE, S, SW, NW, N, NE)
	// preceded by (0, 0) for the centre — duplicated locally because
	// hex_proj.h ships with a standalone test that doesn't link
	// `koord.cc`.
	static const sint8 dq_window[7] = {  0,  1,  0, -1, -1,  0,  1 };
	static const sint8 dr_window[7] = {  0,  0,  1,  1,  0, -1, -1 };
	double best_q = rx, best_r = rz, best_d2 = HUGE_VAL;
	for (int i = 0; i < 7; i++) {
		const double cq = rx + dq_window[i];
		const double cr = rz + dr_window[i];
		const double dq = cq - q_f, dr = cr - r_f;
		const double d2 = (3.0 * dq) * (3.0 * dq) + (dq + 2.0 * dr) * (dq + 2.0 * dr);
		if (d2 < best_d2) {
			best_d2 = d2;
			best_q = cq;
			best_r = cr;
		}
	}
	return koord((sint16)best_q, (sint16)best_r);
}


/// Centre-relative object-offset units for picking the nearest drawn
/// hex corner.  Object offsets use a 64-unit tile basis
/// (`tile_raster_scale_*`), so these values are raster-size
/// independent.  Match the generated hex ground/marker vertices from
/// `synth_hex_geometry`: the footprint is rasterized into a half-open
/// 64x32 bbox, so the right and bottom vertices land one pixel inside
/// the ideal lattice edge.
inline koord hex_corner_centre_offset(hex_corner_t::type c)
{
	static const koord offsets[hex_corner_t::count] = {
		koord( 31,   0), // E
		koord( 16,  15), // SE
		koord(-16,  15), // SW
		koord(-32,   0), // W
		koord(-16, -16), // NW
		koord( 16, -16), // NE
	};
	return offsets[c];
}


/// Anchor-relative object-offset units for drawing a cursor at a hex
/// corner.  `viewport_t::get_screen_coord` applies object offsets from
/// the tile sprite anchor, not from the visible centre used by the
/// picker, so add the legacy anchor → visible-centre displacement.
inline koord hex_corner_cursor_draw_offset(hex_corner_t::type c)
{
	const koord o = hex_corner_centre_offset(c);
	return koord(o.x + 32, o.y + hex_visible_centre_y(64));
}


/// Pick the hex corner closest to a sub-tile fractional axial offset
/// `(dq, dr)` (residual after rounding to the tile centre with
/// `hex_round_to_axial`).  Uses screen distance to the drawn cursor
/// vertices, so the picked corner and displayed arrow stay aligned.
inline hex_corner_t::type hex_pick_nearest_corner(double dq, double dr)
{
	constexpr double u = 16.0; // W/4 in the 64-unit object-offset basis.
	const double sx = dq * 3.0 * u;
	const double sy = (dq + 2.0 * dr) * u;
	double best_d2 = HUGE_VAL;
	hex_corner_t::type best = hex_corner_t::E;
	for (uint8 i = 0; i < 6; i++) {
		const koord offset = hex_corner_centre_offset((hex_corner_t::type)i);
		const double dx = sx - offset.x;
		const double dy = sy - offset.y;
		const double d2 = dx * dx + dy * dy;
		if (d2 < best_d2) {
			best_d2 = d2;
			best = (hex_corner_t::type)i;
		}
	}
	return best;
}


/**
 * Render-loop iteration over the hex lattice.
 *
 * The display loops in `simview.cc` walk screen rows in `W/4`-pixel
 * y-steps and need to know, for each row `y` (in W/4 units), the
 * starting `x` (also in W/4 units) and step that visit every hex
 * centre on that row exactly once.  Hex centres on row `y` lie at
 * `x ≡ 3·y (mod 6)`, so two distinct y-rows can host hex centres:
 * even-y rows hold even-q tiles (x mod 6 == 0), odd-y rows hold
 * odd-q tiles (x mod 6 == 3).  Adjacent tiles on the same row sit
 * `6` x-units apart (= 1.5·W in pixels).  The constants below name
 * those steps so the loop and the visible-tile bbox calc agree on
 * one source.
 *
 * Recovery of `(q, r)` from `(x, y)`: `q = x/3` (exact because x is
 * a multiple of 3 by construction), `r = (y - q)/2` (exact because
 * `y - q` is even by construction, since q has the same parity as y).
 */

/// Smallest valid render-loop x for a given screen row `y`, in W/4
/// units.  Picks the largest x ≤ -3 with the right phase, leaving at
/// least one tile of margin to the left of the strip.
inline sint16 hex_render_x_start(sint16 y)
{
	return (y & 1) ? -3 : -6;
}

/// Step between adjacent hex centres on the same screen row, in W/4
/// units.  The render loop increments `x` by this amount.
inline sint16 hex_render_x_step()
{
	return 6;
}


/// Strip-clipped variant of `hex_render_x_start`: smallest x (in W/4
/// units) on row `y` such that the corresponding tile's right edge
/// `xpos + W` is past `lt_x`, where `xpos = x*(W/4) + const_x_off`.
/// Same phase as `hex_render_x_start(y)` (so `q = x/3` and `r = (y-q)/2`
/// stay exact).  Used by the multi-thread render loop to skip
/// iterations that would draw entirely to the left of the strip.
/// Clamped from below by `hex_render_x_start(y)` so callers whose
/// `lt_x` falls left of the global margin still get the conservative
/// start.
inline sint32 hex_render_x_start_clipped(sint16 y, sint32 lt_x,
                                         sint32 const_x_off, sint16 W)
{
	const sint32 u = W / 4;
	// smallest x with x*u + const_x_off + W > lt_x
	//   <=> x*u > lt_x - const_x_off - W
	//   <=> x > rhs / u
	const sint32 rhs = lt_x - const_x_off - (sint32)W;
	// floor(rhs / u) for positive `u` (C++ integer division
	// truncates toward zero, not toward -inf).
	sint32 x = rhs / u;
	if (rhs < 0 && rhs % u != 0) {
		x -= 1;
	}
	x += 1;
	// Bump to the right phase: x ≡ 3*y (mod 6), i.e. 0 for even y, 3
	// for odd y.
	const sint32 phase = (y & 1) ? 3 : 0;
	sint32 mod = x % 6;
	if (mod < 0) mod += 6;
	x += (phase - mod + 6) % 6;
	// Don't undershoot the global start (callers with lt_x far left
	// of screen — e.g. thread 0's pre-padded strip).
	const sint32 global = hex_render_x_start(y);
	if (x < global) {
		x = global;
	}
	return x;
}


#endif
