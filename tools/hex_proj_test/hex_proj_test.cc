// Self-test for the hex projection helpers in
// src/simutrans/display/hex_proj.h — pure-function math, header-only,
// included directly.  No bodies duplicated, no linkage.
//
// Exercises the invariants:
//   1. Forward at origin / unit steps matches the documented lattice
//      (column step (3·u, u), row step (0, 2·u) with u = W/4).
//   2. Forward of each koord::neighbours[i] axial step lands at the
//      expected screen displacement.
//   3. Inverse round-trip: hex_round_to_axial(fractional(forward(q,r)))
//      recovers (q, r) for every (q, r) in a representative range.
//   4. Inverse is stable under sub-pixel noise around hex centres.
//   5. Inscribed-hex vertex layout (`w × w/2`) tiles the lattice with
//      no shared-corner gap larger than 1 px; `w × w` does not.
//   6. Render-loop iteration (hex_render_x_start + hex_render_x_step
//      with q=x/3, r=(y-q)/2) is a bijection between (x, y) lattice
//      points and the (q, r) tiles in a y-bounded rectangle — every
//      visible hex is visited exactly once.
//   7. Hex vertex naming (same table and formulas as koord.cc).  The three
//      tile/corner names of one world vertex form a closed walk; the test
//      carries its own copy of neighbours[] so this binary stays one TU.

#include <cassert>
#include <cmath>
#include <cstdio>
#include <set>
#include <utility>

#include "simutrans/display/hex_proj.h"
#include "simutrans/dataobj/koord.h"
#include "simutrans/dataobj/ribi.h"


// Use a representative raster width.  Must be a multiple of 4 so the
// W/4 unit divides evenly (simutrans rasters are always powers of 2;
// 64 is the default).
static constexpr sint16 W = 64;
static constexpr sint16 U = W / 4; // = 16

static const koord hex_corner_centre_offset_test[hex_corner_t::count] = {
	koord( 31,   0), koord( 16,  15), koord(-16,  15),
	koord(-32,   0), koord(-16, -16), koord( 16, -16),
};

static const koord hex_corner_cursor_offset_test[hex_corner_t::count] = {
	koord( 31,  16), koord( 16,  31), koord(-16,  31),
	koord(-32,  16), koord(-16,   0), koord( 16,   0),
};

// Must match `koord::neighbours` in koord.cc (SE, S, SW, NW, N, NE).
static const koord hex_neighbours_test[6] = {
	koord(  1,  0 ), koord(  0,  1 ), koord( -1,  1 ),
	koord( -1,  0 ), koord(  0, -1 ), koord(  1, -1 ),
};


// ---- 1. Forward at origin / unit steps -------------------------------------

static void test_forward_unit_steps()
{
	assert(hex_screen_dx(0, W) == 0);
	assert(hex_screen_dy(0, 0, W) == 0);

	assert(hex_screen_dx(1, W) == 3 * U);          // column step x = 3·u
	assert(hex_screen_dy(1, 0, W) == U);           // column step y = u
	assert(hex_screen_dx(0, W) == 0);              // row step x = 0
	assert(hex_screen_dy(0, 1, W) == 2 * U);       // row step y = 2·u

	// Linearity: arbitrary (q, r) is q·column + r·row.
	for (sint16 q = -7; q <= 7; q++) {
		for (sint16 r = -7; r <= 7; r++) {
			assert(hex_screen_dx(q, W) == q * 3 * U);
			assert(hex_screen_dy(q, r, W) == (q + 2 * r) * U);
		}
	}
}


// ---- 2. Forward of the 6 hex neighbours ------------------------------------

static void test_forward_neighbours()
{
	static const char *const names[] = { "SE", "S", "SW", "NW", "N", "NE" };
	static const sint32 exp_sx[] = { 3 * U, 0, -3 * U, -3 * U, 0, 3 * U };
	static const sint32 exp_sy[] = { U, 2 * U, U, -U, -2 * U, -U };
	for (int i = 0; i < 6; i++) {
		const sint16 q = hex_neighbours_test[i].x;
		const sint16 r = hex_neighbours_test[i].y;
		const sint32 sx = hex_screen_dx(q, W);
		const sint32 sy = hex_screen_dy(q, r, W);
		if (sx != exp_sx[i] || sy != exp_sy[i]) {
			std::fprintf(stderr,
				"neighbour %s: forward(%d,%d) = (%d,%d), want (%d,%d)\n",
				names[i], q, r, sx, sy, exp_sx[i], exp_sy[i]);
			std::abort();
		}
	}
}


// ---- 3. Inverse round-trip on integer hex centres --------------------------

static void test_round_trip()
{
	for (sint16 q = -50; q <= 50; q++) {
		for (sint16 r = -50; r <= 50; r++) {
			const sint32 sx = hex_screen_dx(q, W);
			const sint32 sy = hex_screen_dy(q, r, W);
			double q_f, r_f;
			hex_screen_to_fractional(sx, sy, W, q_f, r_f);
			const koord got = hex_round_to_axial(q_f, r_f);
			if (got.x != q || got.y != r) {
				std::fprintf(stderr,
					"round-trip (%d,%d): forward=(%d,%d) inverse_frac=(%g,%g) round=(%d,%d)\n",
					q, r, sx, sy, q_f, r_f, got.x, got.y);
				std::abort();
			}
		}
	}
}


// ---- 4. Inverse stability under sub-pixel noise ----------------------------

static void test_inverse_noise()
{
	// For every hex centre, sweep the screen coord through the full
	// Voronoi-cell inscribed disc and check the rounded hex is the same
	// one.  On this lattice the closest neighbours are N and S at
	// distance 2u, so the inscribed-circle radius is u — perturbations
	// strictly inside ±u must round back to the centre.  Sample at every
	// integer pixel; cheap (~1M points) and catches asymmetric rounding
	// near cell edges that a 3×3 grid can miss.
	const sint32 noise = U - 1;
	for (sint16 q = -10; q <= 10; q++) {
		for (sint16 r = -10; r <= 10; r++) {
			const sint32 cx = hex_screen_dx(q, W);
			const sint32 cy = hex_screen_dy(q, r, W);
			for (sint32 dx = -noise; dx <= noise; dx++) {
				for (sint32 dy = -noise; dy <= noise; dy++) {
					double q_f, r_f;
					hex_screen_to_fractional(cx + dx, cy + dy, W, q_f, r_f);
					const koord got = hex_round_to_axial(q_f, r_f);
					if (got.x != q || got.y != r) {
						std::fprintf(stderr,
							"noise (%d,%d) +(%d,%d): rounded to (%d,%d)\n",
							q, r, dx, dy, got.x, got.y);
						std::abort();
					}
				}
			}
		}
	}
}


// ---- 4b. Inverse picks the screen-closest hex ------------------------------

static void test_inverse_picks_screen_closest()
{
	// Mouse picking promises the hex closest to the click in screen
	// pixels — but `hex_round_to_axial` picks closest in cube-axial
	// space, which on a regular hex lattice equals screen distance and
	// on this irregular (3u, 2u) lattice might not.  Sweep a dense grid
	// of screen points and check the rounded hex matches the actual
	// screen-closest hex centre by brute force.  Squared distance
	// against a small candidate set keeps it cheap.
	for (sint32 sx = -3 * U; sx <= 3 * U; sx++) {
		for (sint32 sy = -2 * U; sy <= 2 * U; sy++) {
			double q_f, r_f;
			hex_screen_to_fractional(sx, sy, W, q_f, r_f);
			const koord got = hex_round_to_axial(q_f, r_f);

			// Brute-force scan a 5×5 axial window around `got` for any
			// hex that is *strictly* closer in squared screen distance.
			// Equidistant ties (cell-boundary clicks) are fine — mouse
			// picking only promises the closest, not a particular tie
			// break.  Window size: cube-rounding inside
			// `hex_round_to_axial` always lands within 1 cube step of
			// the screen-closest hex, and even a buggy refinement
			// can't move `got` further than 1 more step away — so any
			// closer candidate fits in axial Manhattan ≤ 2 from `got`.
			const sint64 got_dx = hex_screen_dx(got.x, W) - sx;
			const sint64 got_dy = hex_screen_dy(got.x, got.y, W) - sy;
			const sint64 got_d2 = got_dx * got_dx + got_dy * got_dy;
			for (sint16 dq = -2; dq <= 2; dq++) {
				for (sint16 dr = -2; dr <= 2; dr++) {
					const sint16 q = got.x + dq;
					const sint16 r = got.y + dr;
					const sint64 dx = hex_screen_dx(q, W) - sx;
					const sint64 dy = hex_screen_dy(q, r, W) - sy;
					const sint64 d2 = dx * dx + dy * dy;
					if (d2 < got_d2) {
						std::fprintf(stderr,
							"screen-closest (%d,%d): rounded to (%d,%d) (d²=%lld) but (%d,%d) is closer (d²=%lld)\n",
							sx, sy, got.x, got.y, (long long)got_d2,
							q, r, (long long)d2);
						std::abort();
					}
				}
			}
		}
	}
}


// ---- Corner picker ---------------------------------------------------------

static void test_pick_corner_at_exact_offsets()
{
	// At the canonical axial offset of each corner (the average of the
	// 3 tile centres that share the vertex), the picker must return
	// that corner.  Offsets are (2,-1)/3, (1,1)/3, (-1,2)/3, (-2,1)/3,
	// (-1,-1)/3, (1,-2)/3 in axial, ordered E, SE, SW, W, NW, NE.
	struct { hex_corner_t::type c; double dq, dr; } cases[] = {
		{ hex_corner_t::E,   2.0/3.0, -1.0/3.0 },
		{ hex_corner_t::SE,  1.0/3.0,  1.0/3.0 },
		{ hex_corner_t::SW, -1.0/3.0,  2.0/3.0 },
		{ hex_corner_t::W,  -2.0/3.0,  1.0/3.0 },
		{ hex_corner_t::NW, -1.0/3.0, -1.0/3.0 },
		{ hex_corner_t::NE,  1.0/3.0, -2.0/3.0 },
	};
	for (const auto &t : cases) {
		const hex_corner_t::type got = hex_pick_nearest_corner(t.dq, t.dr);
		if (got != t.c) {
			std::fprintf(stderr,
				"hex_pick_nearest_corner: at (%.3f, %.3f) expected corner %d, got %d\n",
				t.dq, t.dr, (int)t.c, (int)got);
			std::abort();
		}
	}
}


static void test_pick_corner_matches_screen_closest()
{
	// Sweep sub-tile axial offsets and verify the picker's choice
	// equals the brute-force screen-closest drawn cursor vertex.  Sweep range
	// covers the unit cell with margin so we hit boundaries between
	// adjacent corners.  Skip equidistant ties (mouse-pick boundary)
	// where two corners share the minimum to within float noise.
	for (int i = -50; i <= 50; i++) {
		for (int j = -50; j <= 50; j++) {
			const double dq = i / 100.0, dr = j / 100.0;
			const hex_corner_t::type got = hex_pick_nearest_corner(dq, dr);
			const double sx = dq * 3.0 * U;
			const double sy = (dq + 2.0 * dr) * U;

			// Brute-force best in screen distance.
			int best_i = 0;
			double best_d2 = HUGE_VAL;
			bool tied = false;
			for (int k = 0; k < 6; k++) {
				const double dx = sx - hex_corner_centre_offset_test[k].x;
				const double dy = sy - hex_corner_centre_offset_test[k].y;
				const double d2 = dx * dx + dy * dy;
				if (d2 < best_d2 - 1e-9) {
					best_d2 = d2;
					best_i = k;
					tied = false;
				}
				else if (d2 < best_d2 + 1e-9) {
					tied = true;
				}
			}
			if (tied) continue;
			if ((int)got != best_i) {
				std::fprintf(stderr,
					"hex_pick_nearest_corner: at (%.3f, %.3f) picked %d, screen-closest is %d\n",
					dq, dr, (int)got, best_i);
				std::abort();
			}
		}
	}
}


static void test_corner_draw_offsets_match_terraform_arrow_tip()
{
	for (int i = 0; i < hex_corner_t::count; i++) {
		const hex_corner_t::type c = (hex_corner_t::type)i;
		const koord got = hex_terraform_cursor_draw_offset(c);
		if (got != hex_corner_cursor_offset_test[i]) {
			std::fprintf(stderr,
				"hex_corner_cursor_offset(%d) = (%d,%d), want (%d,%d)\n",
				i, got.x, got.y,
				hex_corner_cursor_offset_test[i].x,
				hex_corner_cursor_offset_test[i].y);
			std::abort();
		}
	}
}


static void test_corner_cursor_offsets()
{
	// Visual offsets are in the 64-unit tile basis used by
	// tile_raster_scale_*.  For W=64 the units equal pixels; compare
	// against the same half-open 64x32 footprint vertices that synth
	// ground/marker sprites rasterize.
	for (uint8 i = 0; i < 6; i++) {
		const koord offset = hex_corner_centre_offset((hex_corner_t::type)i);
		if (offset.x != hex_corner_centre_offset_test[i].x || offset.y != hex_corner_centre_offset_test[i].y) {
			std::fprintf(stderr,
				"hex_corner_centre_offset(%d) = (%d,%d), want (%d,%d)\n",
				(int)i, offset.x, offset.y, hex_corner_centre_offset_test[i].x, hex_corner_centre_offset_test[i].y);
			std::abort();
		}
	}
}


// ---- 5. Render-loop strip clipping -----------------------------------------

static void test_render_loop_strip_clipped()
{
	// hex_render_x_start_clipped should be the smallest x with the
	// right phase such that the tile's right edge `xpos + W` is past
	// `lt_x`, where `xpos = x*(W/4) + const_x_off`.  Verify:
	//   - the returned x has the right phase (`3y mod 6`)
	//   - `xpos + W > lt_x` at that x (the tile is at least partially
	//     in the strip)
	//   - `xpos_prev + W <= lt_x` at `x - hex_render_x_step()`, so
	//     stepping back would skip a tile that should not have been
	//     skipped (no off-by-one)
	//   - returned x is always >= `hex_render_x_start(y)` (clamp)
	const sint32 step_pixels = (sint32)hex_render_x_step() * U;
	for (sint16 y = -8; y <= 8; y++) {
		const sint16 phase = (y & 1) ? 3 : 0;
		for (sint32 const_x_off = -200; const_x_off <= 200; const_x_off += 37) {
			for (sint32 lt_x = -300; lt_x <= 600; lt_x += 13) {
				const sint32 x = hex_render_x_start_clipped(y, lt_x, const_x_off, W);
				// phase
				sint32 mod = x % 6;
				if (mod < 0) mod += 6;
				assert(mod == phase);
				// clamp
				assert(x >= hex_render_x_start(y));
				// The tile at x is at least partially in the strip,
				// UNLESS the clamp kicked in (x == hex_render_x_start
				// and lt_x is far left of even the global start).
				const sint32 xpos = x * U + const_x_off;
				const bool clamped = (x == hex_render_x_start(y));
				if (!clamped) {
					if (!(xpos + (sint32)W > lt_x)) {
						std::fprintf(stderr,
							"strip start y=%d lt_x=%d xoff=%d returned x=%d, but xpos+W=%d <= lt_x=%d (skips visible tile)\n",
							y, lt_x, const_x_off, x, xpos + (sint32)W, lt_x);
						std::abort();
					}
					// And the previous step would NOT have been in the
					// strip — otherwise we skipped a visible tile.
					const sint32 xpos_prev = xpos - step_pixels;
					if (xpos_prev + (sint32)W > lt_x) {
						std::fprintf(stderr,
							"strip start y=%d lt_x=%d xoff=%d returned x=%d, but x-step has xpos_prev+W=%d > lt_x=%d (skipped visible tile)\n",
							y, lt_x, const_x_off, x, xpos_prev + (sint32)W, lt_x);
						std::abort();
					}
				}
			}
		}
	}
}


// ---- 6. slope_t::project_to_square invariants ------------------------------

static void test_slope_project_to_square_invariants()
{
	// Totality: every of the 729 hex slopes projects to a 4-corner
	// subset (E and W flat) with each square corner at height 0 or 1.
	for (slope_t::type s = 0; s < slope_t::max_slopes; s++) {
		const slope_t::type p = slope_t::project_to_square(s);
		assert(corner_e(p) == 0);
		assert(corner_w(p) == 0);
		assert(corner_se(p) <= 1);
		assert(corner_ne(p) <= 1);
		assert(corner_sw(p) <= 1);
		assert(corner_nw(p) <= 1);
	}
}

static void test_slope_project_to_square_identity_on_canonicals()
{
	// For every 4-corner-only single-height slope (E = W = 0,
	// max_diff <= 1), the projection is the identity — square pakset
	// art renders unchanged.
	for (uint8 sw = 0; sw < 2; sw++)
	for (uint8 se = 0; se < 2; se++)
	for (uint8 ne = 0; ne < 2; ne++)
	for (uint8 nw = 0; nw < 2; nw++) {
		const slope_t::type s = encode_corners(sw, se, ne, nw);
		assert(slope_t::project_to_square(s) == s);
	}
}

static void test_slope_project_to_square_hex_edges()
{
	// The 4 hex-only single-edge slopes collapse pairwise: both
	// east-side hex edges (NE-edge = E+NE, SE-edge = E+SE) project to
	// the square "west" slope (NE+SE raised); both west-side hex edges
	// (NW-edge = W+NW, SW-edge = W+SW) project to "east" (NW+SW).
	const slope_t::type sq_west = slope_t::raised_NE + slope_t::raised_SE;
	const slope_t::type sq_east = slope_t::raised_NW + slope_t::raised_SW;
	assert(slope_t::project_to_square(slope_t::raised_E + slope_t::raised_NE) == sq_west);
	assert(slope_t::project_to_square(slope_t::raised_E + slope_t::raised_SE) == sq_west);
	assert(slope_t::project_to_square(slope_t::raised_W + slope_t::raised_NW) == sq_east);
	assert(slope_t::project_to_square(slope_t::raised_W + slope_t::raised_SW) == sq_east);
}

static void test_slope_project_to_square_clamping()
{
	// Double-height slopes clamp to single height.
	const slope_t::type sq_north_single = slope_t::raised_SE + slope_t::raised_SW;
	assert(slope_t::project_to_square(2 * slope_t::north_narrow) == sq_north_single);
	// all_up_one and all_up_two both saturate every square corner.
	const slope_t::type sq_all_up = slope_t::raised_SW + slope_t::raised_SE
	                              + slope_t::raised_NE + slope_t::raised_NW;
	assert(slope_t::project_to_square(slope_t::all_up_one) == sq_all_up);
	assert(slope_t::project_to_square(slope_t::all_up_two) == sq_all_up);
}


// ---- 7. Hex tile inscription -----------------------------------------------

// Inscribed-hex vertex positions for a tile bbox of size `w × h`.
// Order matches `hex_corner_t::type` (E, SE, SW, W, NW, NE).
static void inscribed_hex_vertices(sint32 w, sint32 h, sint32 vx[6], sint32 vy[6])
{
	const sint32 q = w / 4;
	const sint32 mid = h / 2;
	const sint32 bot = h - 1;
	vx[hex_corner_t::E ] = w - 1;  vy[hex_corner_t::E ] = mid;
	vx[hex_corner_t::SE] = q * 3;  vy[hex_corner_t::SE] = bot;
	vx[hex_corner_t::SW] = q;      vy[hex_corner_t::SW] = bot;
	vx[hex_corner_t::W ] = 0;      vy[hex_corner_t::W ] = mid;
	vx[hex_corner_t::NW] = q;      vy[hex_corner_t::NW] = 0;
	vx[hex_corner_t::NE] = q * 3;  vy[hex_corner_t::NE] = 0;
}

// Worst x/y gap (in pixels) between any pair of shared corners across
// tile (0,0)'s 6 edges, given a tile bbox of `w × h`.  Returns 0 when
// the inscription tiles pixel-perfect; the `(w-1, h-1)` half-open
// inscription slips by 1 px regardless of dimensions, anything larger
// is a real gap from `w × h` not matching the lattice.
//
// Across each edge `e` (CW, e=0 is the E↔SE edge), self-corner `e`
// meets neighbour corner `(e+4)%6` and self-corner `(e+1)%6` meets
// neighbour `(e+3)%6` — the neighbour's shared edge runs CCW relative
// to ours along the shared edge, so the corner indices reverse.
// Neighbour axial steps come from `hex_neighbours_test[]` (koord.cc order).
static sint32 max_shared_corner_gap(sint32 w, sint32 h)
{
	sint32 vx[6], vy[6];
	inscribed_hex_vertices(w, h, vx, vy);
	sint32 worst = 0;
	for (int e = 0; e < 6; e++) {
		const sint32 nx = hex_screen_dx(hex_neighbours_test[e].x, w);
		const sint32 ny = hex_screen_dy(hex_neighbours_test[e].x, hex_neighbours_test[e].y, w);
		const int self_corners [2] = { e,           (e + 1) % 6 };
		const int neigh_corners[2] = { (e + 4) % 6, (e + 3) % 6 };
		for (int k = 0; k < 2; k++) {
			const sint32 dx = vx[self_corners[k]] - (nx + vx[neigh_corners[k]]);
			const sint32 dy = vy[self_corners[k]] - (ny + vy[neigh_corners[k]]);
			const sint32 ax = dx < 0 ? -dx : dx;
			const sint32 ay = dy < 0 ? -dy : dy;
			if (ax > worst) worst = ax;
			if (ay > worst) worst = ay;
		}
	}
	return worst;
}


static void test_inscribed_hex_tiles_lattice()
{
	// `w × w/2` is the only inscription that tiles the lattice (the
	// `w-1, h-1` close still leaves a 1-pixel half-open slip).
	// Inscribing in `w × w` gaps by W/4 px per edge — the visible
	// black cracks an early port revision shipped before this test
	// pinned the contract.
	const sint32 W = 64;
	if (max_shared_corner_gap(W, W / 2) > 1) {
		std::fprintf(stderr, "lattice tiling: w=%d h=%d should tile, doesn't\n", W, W / 2);
		std::abort();
	}
	if (max_shared_corner_gap(W, W) <= 1) {
		std::fprintf(stderr, "lattice tiling: w=%d h=%d should NOT tile, does — invariant broken\n", W, W);
		std::abort();
	}
}


// ---- 7b. Visible-centre anchor matches legacy iso convention ---------------

static void test_canvas_anchor_convention()
{
	// Pakset sprites (cursor.pak, buildings, vehicles) are authored
	// against the legacy "tile content in bottom half of W×W canvas"
	// layout: anchor (X, Y) places visible centre at (X+W/2, Y+3W/4).
	// Synth overlay sprites and picker both pin to this y; drift here
	// was the pak128 bug where the looking-glass cursor drew on the
	// south neighbour of the highlighted hex.
	assert(hex_visible_centre_y(W) == 3 * W / 4);
}


// ---- 8. Render-loop iteration is a bijection -------------------------------

static void test_render_loop_bijection()
{
	// Walk the render loop's (y, x) lattice across a y-range, decode
	// (q, r) at each step, and verify each (q, r) is produced exactly
	// once and that decoding is exact (q*3 == x, q+2r == y).
	const sint16 y_lo = -20, y_hi = 20;
	const sint16 x_hi = 60; // arbitrary right bound

	std::set<std::pair<sint16, sint16>> seen;
	for (sint16 y = y_lo; y < y_hi; y++) {
		for (sint16 x = hex_render_x_start(y); x < x_hi; x += hex_render_x_step()) {
			const sint16 q = x / 3;
			const sint16 r = (y - q) / 2;
			// Decoding must be exact (the loop's invariants).
			assert(q * 3 == x);
			assert(q + 2 * r == y);
			// Every (q, r) visited exactly once.
			const auto key = std::make_pair(q, r);
			if (seen.count(key)) {
				std::fprintf(stderr, "duplicate visit: (q,r)=(%d,%d) at (x,y)=(%d,%d)\n",
					q, r, x, y);
				std::abort();
			}
			seen.insert(key);
		}
	}
	// Sanity: we should have visited a non-trivial number of hexes.
	assert(seen.size() > 100);
}


// ---- 9. Vertex-owner walk (must match koord.cc::vertex_owners) ------------
// This duplicates the implementation body — the test cannot link koord.cc
// in this one-TU build, so drift is caught by review + TODO.md, not by the linker.

static void vertex_neighbours_test(hex_vertex_t v, hex_vertex_t out[3])
{
	// v must be canonical (E or SE); produces canonical outputs
	if (v.corner == hex_corner_t::E) {
		out[0] = { v.tile,                                     hex_corner_t::SE };
		out[1] = { koord(v.tile.x,     v.tile.y - 1),          hex_corner_t::SE };
		out[2] = { koord(v.tile.x + 1, v.tile.y - 1),          hex_corner_t::SE };
	}
	else {
		out[0] = { v.tile,                                     hex_corner_t::E };
		out[1] = { koord(v.tile.x - 1, v.tile.y + 1),          hex_corner_t::E };
		out[2] = { koord(v.tile.x,     v.tile.y + 1),          hex_corner_t::E };
	}
}


static void vertex_owners_test(koord tile, hex_corner_t::type c, hex_vertex_t out[3])
{
	const uint8 dir_a = (uint8)(((uint8)c + 5) % 6);
	const uint8 dir_b = (uint8)c;
	out[0].tile   = tile;
	out[0].corner = c;
	out[1].tile   = tile + hex_neighbours_test[dir_a];
	out[1].corner = (hex_corner_t::type)(((uint8)c + 2) % 6);
	out[2].tile   = tile + hex_neighbours_test[dir_b];
	out[2].corner = (hex_corner_t::type)(((uint8)c + 4) % 6);
}


static void test_vertex_neighbours_closure()
{
	// Every vertex v must appear in the neighbour list of each of its own
	// 3 neighbours (closure / symmetry of the adjacency relation).
	const koord k(10, 10);
	for (uint8 c0 = 0; c0 < (uint8)hex_corner_t::count; c0++) {
		const hex_corner_t::type c = (hex_corner_t::type)c0;
		// start from a canonical vertex
		hex_vertex_t v;
		if (c == hex_corner_t::E || c == hex_corner_t::SE) {
			v = { k, c };
		}
		else {
			continue; // only E/SE are canonical; others fold onto one of these
		}
		hex_vertex_t nb[3];
		vertex_neighbours_test(v, nb);
		for (int i = 0; i < 3; i++) {
			hex_vertex_t nb2[3];
			vertex_neighbours_test(nb[i], nb2);
			bool found = false;
			for (int j = 0; j < 3; j++) {
				if (nb2[j].tile == v.tile && nb2[j].corner == v.corner) {
					found = true;
					break;
				}
			}
			if (!found) {
				std::fprintf(stderr,
					"vertex_neighbours closure: v(%d,%d,%d) not in neighbours of nb[%d](%d,%d,%d)\n",
					(int)v.tile.x, (int)v.tile.y, (int)v.corner,
					i, (int)nb[i].tile.x, (int)nb[i].tile.y, (int)nb[i].corner);
				std::abort();
			}
		}
	}
}


static void test_vertex_owners_neighbour_closure()
{
	const koord k(10, 10);
	for (uint8 c = 0; c < (uint8)hex_corner_t::count; c++) {
		hex_vertex_t o[3];
		vertex_owners_test(k, (hex_corner_t::type)c, o);
		assert(o[0].tile == k);
		assert(o[0].corner == (hex_corner_t::type)c);
		for (int i = 0; i < 3; i++) {
			const hex_vertex_t a = o[i];
			const hex_vertex_t b = o[(i + 1) % 3];
			const uint8 dir = (uint8)(((uint8)a.corner + 5) % 6);
			assert(b.tile == a.tile + hex_neighbours_test[dir]);
			assert(b.corner == (hex_corner_t::type)(((uint8)a.corner + 2) % 6));
		}
	}
}

int main()
{
	test_forward_unit_steps();
	test_forward_neighbours();
	test_round_trip();
	test_inverse_noise();
	test_inverse_picks_screen_closest();
	test_pick_corner_at_exact_offsets();
	test_pick_corner_matches_screen_closest();
	test_corner_draw_offsets_match_terraform_arrow_tip();
	test_corner_cursor_offsets();
	test_render_loop_strip_clipped();
	test_slope_project_to_square_invariants();
	test_slope_project_to_square_identity_on_canonicals();
	test_slope_project_to_square_hex_edges();
	test_slope_project_to_square_clamping();
	test_inscribed_hex_tiles_lattice();
	test_canvas_anchor_convention();
	test_render_loop_bijection();
	test_vertex_neighbours_closure();
	test_vertex_owners_neighbour_closure();

	std::printf("hex_proj_test: all checks passed\n");
	return 0;
}
