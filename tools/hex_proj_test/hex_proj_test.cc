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
//   5. Render-loop iteration (hex_render_x_start + hex_render_x_step
//      with q=x/3, r=(y-q)/2) is a bijection between (x, y) lattice
//      points and the (q, r) tiles in a y-bounded rectangle — every
//      visible hex is visited exactly once.

#include <cassert>
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
	// koord::neighbours is ordered SE, S, SW, NW, N, NE.  See koord.cc.
	const struct { sint16 q, r; sint32 sx, sy; const char *name; } cases[] = {
		{  1,  0,  3 * U,        U,  "SE" },
		{  0,  1,      0,    2 * U,  "S"  },
		{ -1,  1, -3 * U,        U,  "SW" },
		{ -1,  0, -3 * U,       -U,  "NW" },
		{  0, -1,      0,   -2 * U,  "N"  },
		{  1, -1,  3 * U,       -U,  "NE" },
	};
	for (const auto &c : cases) {
		const sint32 sx = hex_screen_dx(c.q, W);
		const sint32 sy = hex_screen_dy(c.q, c.r, W);
		if (sx != c.sx || sy != c.sy) {
			std::fprintf(stderr,
				"neighbour %s: forward(%d,%d) = (%d,%d), want (%d,%d)\n",
				c.name, c.q, c.r, sx, sy, c.sx, c.sy);
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
	assert(slope_t::project_to_square(2 * slope_t::north) == sq_north_single);
	// all_up_one and all_up_two both saturate every square corner.
	const slope_t::type sq_all_up = slope_t::raised_SW + slope_t::raised_SE
	                              + slope_t::raised_NE + slope_t::raised_NW;
	assert(slope_t::project_to_square(slope_t::all_up_one) == sq_all_up);
	assert(slope_t::project_to_square(slope_t::all_up_two) == sq_all_up);
}


// ---- 7. Render-loop iteration is a bijection -------------------------------

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


int main()
{
	test_forward_unit_steps();
	test_forward_neighbours();
	test_round_trip();
	test_inverse_noise();
	test_inverse_picks_screen_closest();
	test_render_loop_strip_clipped();
	test_slope_project_to_square_invariants();
	test_slope_project_to_square_identity_on_canonicals();
	test_slope_project_to_square_hex_edges();
	test_slope_project_to_square_clamping();
	test_render_loop_bijection();
	std::printf("hex_proj_test: all checks passed\n");
	return 0;
}
