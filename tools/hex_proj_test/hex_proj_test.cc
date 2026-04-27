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
//   5. Slope_t corner heights, the hex projection lattice and synth
//      ground geometry agree at shared rendered slope edges.
//   5b. `synth_ground_lambert_face_normal` (called from `build_ground`) matches
//      an independent cross product from `geom.vy`; a local buggy reference
//      (unlifted screen Y) diverges on sloped tiles.
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
#include "simutrans/descriptor/synth_geometry.h"
#include "simutrans/descriptor/synth_plane_partition.h"


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
	assert(slope_t::project_to_square(2 * slope_t::north) == sq_north_single);
	// all_up_one and all_up_two both saturate every square corner.
	const slope_t::type sq_all_up = slope_t::raised_SW + slope_t::raised_SE
	                              + slope_t::raised_NE + slope_t::raised_NW;
	assert(slope_t::project_to_square(slope_t::all_up_one) == sq_all_up);
	assert(slope_t::project_to_square(slope_t::all_up_two) == sq_all_up);
}


// ---- 7. Hex tile inscription / synth vector geometry -----------------------

// Inscribed-hex vertex positions for a tile bbox of size `w × h`,
// matching the formula in `descriptor/synth_overlay.cc`
// (rasterise_outline / build_ground) for a flat slope.  If that
// formula drifts, this copy stops representing reality — keep the
// two in sync.
//
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


// Half-open edge rule used by `synth_overlay::fill_polygon`: the flat
// hex bottom SE–SW is horizontal (no crossings), and centre→SE /
// centre→SW both use y_hi == bot_y so half-open contributes zero hits
// on that row — build_ground closes it with an explicit horizontal chord.
static int synth_halfopen_edge_hits(sint32 ya, sint32 yb, sint32 y)
{
	if (ya == yb) {
		return 0;
	}
	const sint32 y_lo = ya < yb ? ya : yb;
	const sint32 y_hi = ya < yb ? yb : ya;
	return (y >= y_lo && y < y_hi) ? 1 : 0;
}

static void test_synth_flat_bottom_rim_has_no_halfopen_edge_hits()
{
	constexpr sint32 W = 64;
	const synth_overlay::synth_hex_geometry_t geom =
		synth_overlay::synth_hex_geometry(W / 4, 16);
	const sint32 cy = geom.mid_y;
	const sint32 vy_se = geom.vy(slope_t::flat, hex_corner_t::SE);
	const sint32 vy_sw = geom.vy(slope_t::flat, hex_corner_t::SW);
	const sint32 y_bot = vy_se;
	assert(vy_sw == y_bot);

	const int ho = synth_halfopen_edge_hits(cy, vy_se, y_bot)
	             + synth_halfopen_edge_hits(cy, vy_sw, y_bot);
	if (ho != 0) {
		std::fprintf(stderr,
			"synth bottom rim: expected 0 half-open edge hits on bot row, got %d (y_bot=%d cy=%d)\n",
			ho, (int)y_bot, (int)cy);
		std::abort();
	}
}

// Monotone vertex safety: inclusive y on slanted edges can yield an odd
// number of crossings on one scanline (pair-fill drops a span).
// Half-open parity must stay even on every row of each wedge — sample
// raised_E (one corner up), which stresses centre vs rim geometry.
static void test_synth_raised_E_wedges_have_even_halfopen_hits()
{
	constexpr sint32 W = 64;
	const synth_overlay::synth_hex_geometry_t geom =
		synth_overlay::synth_hex_geometry(W / 4, 16);
	const slope_t::type slope = slope_t::raised_E;
	sint32 vy[6];
	for (int i = 0; i < 6; i++) {
		vy[i] = geom.vy(slope, (hex_corner_t::type)i);
	}
	sint32 sum_h = 0;
	for (int i = 0; i < 6; i++) {
		sum_h += synth_overlay::hex_corner_height(slope, (hex_corner_t::type)i);
	}
	const sint32 cz = (sum_h * geom.lift) / 6;
	const sint32 cy = geom.mid_y - cz;

	for (int f = 0; f < 6; f++) {
		const int a = f;
		const int b = (f + 1) % 6;
		sint32 y_min = cy, y_max = cy;
		if (vy[a] < y_min) y_min = vy[a];
		if (vy[a] > y_max) y_max = vy[a];
		if (vy[b] < y_min) y_min = vy[b];
		if (vy[b] > y_max) y_max = vy[b];
		if (y_min < 0) y_min = 0;
		if (y_max >= geom.h) y_max = geom.h - 1;
		for (sint32 y = y_min; y <= y_max; y++) {
			const int h = synth_halfopen_edge_hits(cy, vy[a], y)
			            + synth_halfopen_edge_hits(vy[a], vy[b], y)
			            + synth_halfopen_edge_hits(vy[b], cy, y);
			if ((h & 1) != 0) {
				std::fprintf(stderr,
					"synth raised_E wedge f=%d: odd half-open hit count %d at y=%d\n",
					f, h, (int)y);
				std::abort();
			}
		}
	}
}


static void test_inscribed_hex_tiles_lattice()
{
	// `w × w/2` is the only inscription that tiles the lattice (the
	// `w-1, h-1` close still leaves a 1-pixel half-open slip).
	// Inscribing in `w × w` — what synth_overlay used to do when it
	// inherited `tmpl->h` from pak64's 64×64 marker — gaps by W/4 px
	// per edge, the visible black cracks the user reported.
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

static void test_synth_slope_bbox_contains_lifted_vertices()
{
	// Graphics-behaviour contract for synth_overlay's vector sprites:
	// every lifted slope vertex must fit in the generated image bbox, and
	// the added headroom must not move the flat footprint's screen anchor.
	// The old `W x W/2` bbox clipped raised N-side vertices at local y=0;
	// endpoint-continuity tests stayed green because the clipped vector
	// vertices still agreed mathematically before rasterisation.
	const sint32 W = 64;
	const sint32 old_img_y = hex_visible_centre_y(W) - W / 4;
	const synth_overlay::synth_hex_geometry_t geom =
		synth_overlay::synth_hex_geometry(W / 4, 16);
	const sint32 img_y = geom.image_y();
	sint32 flat_x[6], flat_y[6];
	inscribed_hex_vertices(W, W / 2, flat_x, flat_y);

	for (slope_t::type slope = 0; slope < slope_t::max_slopes; slope++) {
		for (int c = 0; c < 6; c++) {
			const sint32 vx = geom.vx[c];
			const sint32 vy = geom.vy(slope, (hex_corner_t::type)c);
			if (vx < 0 || vx >= W || vy < 0 || vy >= geom.h) {
				std::fprintf(stderr,
					"synth slope bbox: slope=%d corner=%d vertex=(%d,%d) outside %dx%d\n",
					(int)slope, c, vx, vy, W, geom.h);
				std::abort();
			}
		}
	}

	for (int c = 0; c < 6; c++) {
		const sint32 vy = geom.vy(slope_t::flat, (hex_corner_t::type)c);
		if (img_y + vy != old_img_y + flat_y[c]) {
			std::fprintf(stderr,
				"synth slope anchor: flat corner=%d screen_y=%d, want %d\n",
				c, img_y + vy, old_img_y + flat_y[c]);
			std::abort();
		}
	}
}


// Pre-fix Lambert bug: vy_base / mid_y only (no corner lift in screen Y,
// centre ignores cz).  Kept in the test TU only — production helper has no
// flag so `build_ground` cannot accidentally pass `false`.
static void lambert_face_normal_unlifted_screen_y_bug(
	const synth_overlay::synth_hex_geometry_t &geom,
	slope_t::type slope,
	sint32 cz,
	uint8 corner_a,
	uint8 corner_b,
	double *nx, double *ny, double *nz)
{
	uint8 ch[hex_corner_t::count];
	for (int i = 0; i < hex_corner_t::count; i++) {
		ch[i] = synth_overlay::hex_corner_height(slope, (hex_corner_t::type)i);
	}
	const sint32 cx = geom.w / 2;
	const sint32 cy_centre = geom.mid_y;
	const double ax = (double)(geom.vx[corner_a] - cx);
	const double ay = (double)(geom.vy_base[corner_a] - cy_centre);
	const double az = (double)((sint32)ch[corner_a] * geom.lift - cz);
	const double bx = (double)(geom.vx[corner_b] - cx);
	const double by = (double)(geom.vy_base[corner_b] - cy_centre);
	const double bz = (double)((sint32)ch[corner_b] * geom.lift - cz);
	*nx = ay * bz - az * by;
	*ny = az * bx - ax * bz;
	*nz = ax * by - ay * bx;
}


static sint32 synth_test_centre_z(
	const synth_overlay::synth_hex_geometry_t &geom,
	slope_t::type slope)
{
	sint32 sum_h = 0;
	for (int i = 0; i < hex_corner_t::count; i++) {
		sum_h += synth_overlay::hex_corner_height(slope, (hex_corner_t::type)i);
	}
	return (sum_h * geom.lift) / hex_corner_t::count;
}


static void synth_test_brightness_minmax(
	const synth_overlay::synth_hex_geometry_t &geom,
	slope_t::type slope,
	sint32 *lo,
	sint32 *hi)
{
	*lo = 10000;
	*hi = -10000;
	const sint32 cz = synth_test_centre_z(geom, slope);
	for (int f = 0; f < hex_corner_t::count; f++) {
		double nx, ny, nz;
		synth_overlay::synth_ground_lambert_face_normal(
			geom, slope, cz, (uint8)f, (uint8)((f + 1) % hex_corner_t::count),
			&nx, &ny, &nz);
		const sint32 brightness = synth_overlay::synth_ground_lambert_brightness(nx, ny, nz);
		if (brightness < *lo) *lo = brightness;
		if (brightness > *hi) *hi = brightness;
	}
}


// `build_ground` calls `synth_ground_lambert_face_normal` — assert that helper
// matches an independent cross product built from `geom.vy` (same Y as
// fill_polygon), and that the unlifted reference diverges on a raised-corner
// slope.
static void test_synth_build_ground_normal_uses_lifted_screen_y()
{
	constexpr sint32 W = 64;
	constexpr sint32 lift = 16;
	const synth_overlay::synth_hex_geometry_t geom =
		synth_overlay::synth_hex_geometry(W / 4, lift);

	const slope_t::type slope = slope_t::raised_NE;
	const sint32 cz = synth_test_centre_z(geom, slope);
	const sint32 cx = geom.w / 2;
	const sint32 cy = geom.mid_y - cz;

	auto cross3 = [](double ax, double ay, double az,
	                 double bx, double by, double bz,
	                 double *ox, double *oy, double *oz) {
		*ox = ay * bz - az * by;
		*oy = az * bx - ax * bz;
		*oz = ax * by - ay * bx;
	};
	auto norm2 = [](double x, double y, double z) {
		return x * x + y * y + z * z;
	};

	double best_sin2_false = 0.0;
	for (int f = 0; f < hex_corner_t::count; f++) {
		const uint8 a = (uint8)f;
		const uint8 b = (uint8)((f + 1) % hex_corner_t::count);

		double nh_x, nh_y, nh_z;
		synth_overlay::synth_ground_lambert_face_normal(
			geom, slope, cz, a, b, &nh_x, &nh_y, &nh_z);

		const double rax = (double)(geom.vx[a] - cx);
		const double ray = (double)(geom.vy(slope, (hex_corner_t::type)a) - cy);
		const double raz = (double)(
			(sint32)synth_overlay::hex_corner_height(slope, (hex_corner_t::type)a) * geom.lift - cz);
		const double rbx = (double)(geom.vx[b] - cx);
		const double rby = (double)(geom.vy(slope, (hex_corner_t::type)b) - cy);
		const double rbz = (double)(
			(sint32)synth_overlay::hex_corner_height(slope, (hex_corner_t::type)b) * geom.lift - cz);
		double nr_x, nr_y, nr_z;
		cross3(rax, ray, raz, rbx, rby, rbz, &nr_x, &nr_y, &nr_z);

		const double nh2 = norm2(nh_x, nh_y, nh_z);
		const double nr2 = norm2(nr_x, nr_y, nr_z);
		if (nh2 < 1e-20 || nr2 < 1e-20) {
			continue;
		}
		double cx_, cy_, cz_;
		cross3(nh_x, nh_y, nh_z, nr_x, nr_y, nr_z, &cx_, &cy_, &cz_);
		const double sin2_hr = norm2(cx_, cy_, cz_) / (nh2 * nr2);
		if (sin2_hr > 1e-10) {
			std::fprintf(stderr,
				"synth_ground_lambert_face_normal != geom.vy cross product "
				"(face %d sin^2=%g)\n",
				f, sin2_hr);
			std::abort();
		}

		double nb_x, nb_y, nb_z;
		lambert_face_normal_unlifted_screen_y_bug(
			geom, slope, cz, a, b, &nb_x, &nb_y, &nb_z);
		const double nb2 = norm2(nb_x, nb_y, nb_z);
		if (nb2 < 1e-20) {
			continue;
		}
		cross3(nh_x, nh_y, nh_z, nb_x, nb_y, nb_z, &cx_, &cy_, &cz_);
		const double sin2_hf = norm2(cx_, cy_, cz_) / (nh2 * nb2);
		if (sin2_hf > best_sin2_false) {
			best_sin2_false = sin2_hf;
		}
	}

	if (best_sin2_false < 1e-8) {
		std::fprintf(stderr,
			"synth_ground_lambert_face_normal vs unlifted reference did not diverge "
			"(sin^2 max=%g)\n",
			best_sin2_false);
		std::abort();
	}
}


static void test_synth_ground_shading_calibrates_flat_plane()
{
	constexpr sint32 W = 64;
	constexpr sint32 lift = 16;
	const synth_overlay::synth_hex_geometry_t geom =
		synth_overlay::synth_hex_geometry(W / 4, lift);

	sint32 lo, hi;
	synth_test_brightness_minmax(geom, slope_t::flat, &lo, &hi);
	if (lo != 256 || hi != 256) {
		std::fprintf(stderr,
			"synth flat shading: expected all faces at 256, got min=%d max=%d\n",
			(int)lo, (int)hi);
		std::abort();
	}

	synth_test_brightness_minmax(geom, slope_t::raised_SW, &lo, &hi);
	if (lo >= 192 || hi <= 288 || hi - lo < 160) {
		std::fprintf(stderr,
			"synth raised_SW shading: expected visible light/dark relief, got min=%d max=%d\n",
			(int)lo, (int)hi);
		std::abort();
	}
}


// ---- 7b. Visible-centre anchor matches legacy iso convention ---------------

static void test_canvas_anchor_convention()
{
	// Pakset sprites (cursor.pak, buildings, vehicles) are authored
	// against the legacy "tile content in bottom half of W×W canvas"
	// layout: anchor (X, Y) places visible centre at (X+W/2, Y+3W/4).
	// Synth ground rasteriser and picker both pin to this y; drift
	// here was the pak128 bug where the looking-glass cursor drew on
	// the south neighbour of the highlighted hex.
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

// ---- 10. Plane partition solver invariants ---------------------------------
// These are regression checks over the shared solver implementation
// (`synth_plane_partition.h`), not an independent oracle against a
// second implementation.

static void test_plane_partition_known_cases()
{
	struct tc_t { uint8 h[6]; uint8 n; };
	static const tc_t cases[] = {
		{{0,0,0,0,0,0}, 1},
		{{2,2,2,2,2,2}, 1},
		{{0,0,0,1,1,1}, 3},
		{{0,0,0,0,0,1}, 2},
		{{0,0,0,0,1,1}, 2},
		{{0,1,0,1,0,1}, 3},
		{{0,1,0,0,2,1}, 4},
	};
	for(  size_t i = 0;  i < sizeof(cases)/sizeof(cases[0]);  i++  ) {
		synth_overlay::plane_partition::hex_partition_t p;
		const bool ok = synth_overlay::plane_partition::find_min_partition(cases[i].h, p);
		if(  !ok || p.region_count != cases[i].n  ) {
			std::fprintf(stderr, "partition known-case %zu failed: got %d, want %d\n",
			             i, ok ? (int)p.region_count : -1, (int)cases[i].n);
			std::abort();
		}
	}
}


static void test_plane_partition_disambiguates_000111_by_flat_area()
{
	static const uint8 base[6] = {0,0,0,1,1,1};
	for(  uint8 rot = 0;  rot < 6;  rot++  ) {
		uint8 h[6];
		for(  uint8 i = 0;  i < 6;  i++  ) {
			h[(i + rot) % 6] = base[i];
		}

		synth_overlay::plane_partition::hex_partition_t p;
		const bool ok = synth_overlay::plane_partition::find_min_partition(h, p);
		const uint8 flat_area2 = ok ? synth_overlay::plane_partition::partition_flat_projected_area2(p, h) : 0;
		if(  !ok || p.region_count != 3 || flat_area2 != 2  ) {
			std::fprintf(stderr,
			             "partition 000111 rotation %d failed: regions=%d flat_area2=%d\n",
			             (int)rot, ok ? (int)p.region_count : -1, (int)flat_area2);
			std::abort();
		}
	}
}


static bool hex_corner_neighbour_diffs_within_one(const uint8 h[6])
{
	for(  uint8 i = 0;  i < 6;  i++  ) {
		const uint8 j = (uint8)((i + 1) % 6);
		if(  h[i] > h[j] + 1 || h[j] > h[i] + 1  ) {
			return false;
		}
	}
	return true;
}


static bool hex_corner_has_zero_height(const uint8 h[6])
{
	for(  uint8 i = 0;  i < 6;  i++  ) {
		if(  h[i] == 0  ) {
			return true;
		}
	}
	return false;
}


static bool hex_corner_matches_rotation(const uint8 h[6], const uint8 pattern[6])
{
	for(  uint8 rot = 0;  rot < 6;  rot++  ) {
		bool match = true;
		for(  uint8 i = 0;  i < 6;  i++  ) {
			if(  h[(i + rot) % 6] != pattern[i]  ) {
				match = false;
				break;
			}
		}
		if(  match  ) {
			return true;
		}
	}
	return false;
}


static bool hex_corner_known_equal_score_tie_shape(const uint8 h[6])
{
	static const uint8 p110100[6] = {1,1,0,1,0,0};
	static const uint8 p101100[6] = {1,0,1,1,0,0};
	static const uint8 p101010[6] = {1,0,1,0,1,0};
	return hex_corner_matches_rotation(h, p110100)
	    || hex_corner_matches_rotation(h, p101100)
	    || hex_corner_matches_rotation(h, p101010);
}


struct plane_partition_score_t {
	uint8 regions;
	uint8 flat_area2;
	uint16 count;
};


static plane_partition_score_t plane_partition_best_score_for_heights(const uint8 h[6])
{
	plane_partition_score_t best = {255, 0, 0};
	for(  uint16 mask = 0;  mask < (1u << 9);  mask++  ) {
		bool ok = true;
		for(  uint8 i = 0;  i < 9 && ok;  i++  ) {
			if(  (mask & (uint16)(1u << i)) == 0  ) { continue; }
			for(  uint8 j = i + 1;  j < 9;  j++  ) {
				if(  (mask & (uint16)(1u << j)) == 0  ) { continue; }
				if(  synth_overlay::plane_partition::partition_chords_cross(synth_overlay::plane_partition::HEX_ALL_CHORDS[i],
				                                                             synth_overlay::plane_partition::HEX_ALL_CHORDS[j])  ) {
					ok = false;
					break;
				}
			}
		}
		if(  !ok  ) {
			continue;
		}
		synth_overlay::plane_partition::hex_partition_t candidate;
		if(  !synth_overlay::plane_partition::compute_regions_from_chord_mask(mask, candidate)  ) {
			continue;
		}
		for(  uint8 r = 0;  r < candidate.region_count;  r++  ) {
			if(  !synth_overlay::plane_partition::region_coplanar(candidate.region[r], h)  ) {
				ok = false;
				break;
			}
		}
		if(  !ok  ) {
			continue;
		}
		const uint8 flat_area2 = synth_overlay::plane_partition::partition_flat_projected_area2(candidate, h);
		if(  candidate.region_count < best.regions
		  ||  (candidate.region_count == best.regions && flat_area2 > best.flat_area2)  ) {
			best.regions = candidate.region_count;
			best.flat_area2 = flat_area2;
			best.count = 1;
		}
		else if(  candidate.region_count == best.regions && flat_area2 == best.flat_area2  ) {
			best.count++;
		}
	}
	return best;
}


static void test_plane_partition_exhaustive_ternary()
{
	// Domain is canonicalized to plausible ground-local shapes: no edge
	// jump over one height unit, and at least one zero-height corner
	// (otherwise all corners can shift down).  Even there, 14 cases are
	// true equal-score ties after min-region/max-flat-area selection:
	// the rotations of 110100, 101100, and the two rotations of 101010.
	// The solver therefore promises an optimal score, not uniqueness.
	uint16 known_tie_shapes = 0;
	uint16 unique_shapes = 0;
	for(  uint8 h0 = 0;  h0 < 3;  h0++  )
	for(  uint8 h1 = 0;  h1 < 3;  h1++  )
	for(  uint8 h2 = 0;  h2 < 3;  h2++  )
	for(  uint8 h3 = 0;  h3 < 3;  h3++  )
	for(  uint8 h4 = 0;  h4 < 3;  h4++  )
	for(  uint8 h5 = 0;  h5 < 3;  h5++  ) {
		const uint8 h[6] = { h0, h1, h2, h3, h4, h5 };
		if(  !hex_corner_neighbour_diffs_within_one(h) || !hex_corner_has_zero_height(h)  ) {
			continue;
		}

		const plane_partition_score_t best = plane_partition_best_score_for_heights(h);
		synth_overlay::plane_partition::hex_partition_t p;
		if(  !synth_overlay::plane_partition::find_min_partition(h, p)  ) {
			std::fprintf(stderr, "partition missing for heights [%d,%d,%d,%d,%d,%d]\n",
			             h0, h1, h2, h3, h4, h5);
			std::abort();
		}

		const uint8 flat_area2 = synth_overlay::plane_partition::partition_flat_projected_area2(p, h);
		if(  p.region_count < 1 || p.region_count > 4  ) {
			std::fprintf(stderr, "partition count out of range (%d) for heights [%d,%d,%d,%d,%d,%d]\n",
			             (int)p.region_count, h0, h1, h2, h3, h4, h5);
			std::abort();
		}
		if(  p.region_count != best.regions || flat_area2 != best.flat_area2  ) {
			std::fprintf(stderr, "partition score mismatch for heights [%d,%d,%d,%d,%d,%d]: got regions=%d flat_area2=%d want regions=%d flat_area2=%d\n",
			             h0, h1, h2, h3, h4, h5,
			             (int)p.region_count, (int)flat_area2, (int)best.regions, (int)best.flat_area2);
			std::abort();
		}

		if(  hex_corner_known_equal_score_tie_shape(h)  ) {
			if(  best.count <= 1  ) {
				std::fprintf(stderr, "expected known tie shape [%d,%d,%d,%d,%d,%d] to have multiple best partitions\n",
				             h0, h1, h2, h3, h4, h5);
				std::abort();
			}
			known_tie_shapes++;
			continue;
		}
		if(  best.count != 1  ) {
			std::fprintf(stderr, "unexpected equal-score partition tie for heights [%d,%d,%d,%d,%d,%d]: count=%d\n",
			             h0, h1, h2, h3, h4, h5, (int)best.count);
			std::abort();
		}
		unique_shapes++;

		for(  uint8 r = 0;  r < p.region_count;  r++  ) {
			if(  !synth_overlay::plane_partition::region_coplanar(p.region[r], h)  ) {
				std::fprintf(stderr, "non-coplanar region in partition for heights [%d,%d,%d,%d,%d,%d]\n",
				             h0, h1, h2, h3, h4, h5);
				std::abort();
			}
		}
	}
	if(  known_tie_shapes != 14 || unique_shapes != 121  ) {
		std::fprintf(stderr, "partition domain count mismatch: known_ties=%d unique=%d\n",
		             (int)known_tie_shapes, (int)unique_shapes);
		std::abort();
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
	test_synth_flat_bottom_rim_has_no_halfopen_edge_hits();
	test_synth_raised_E_wedges_have_even_halfopen_hits();
	test_inscribed_hex_tiles_lattice();
	test_synth_slope_bbox_contains_lifted_vertices();
	test_synth_build_ground_normal_uses_lifted_screen_y();
	test_synth_ground_shading_calibrates_flat_plane();
	test_canvas_anchor_convention();
	test_render_loop_bijection();
	test_vertex_owners_neighbour_closure();
	test_plane_partition_known_cases();
	test_plane_partition_disambiguates_000111_by_flat_area();
	test_plane_partition_exhaustive_ternary();
	std::printf("hex_proj_test: all checks passed\n");
	return 0;
}
