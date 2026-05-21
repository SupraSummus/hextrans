// Self-test for the back-image hide-test helpers in
// src/simutrans/ground/back_image_decode.h — pure functions, header
// included directly, no engine link.  Covers both the bb digit
// decoder and the per-neighbour contribution projection.

#include <cassert>
#include <cstdio>

#include "simutrans/ground/back_image_decode.h"


// Match grund_t::WALL_IMAGE_COUNT.  Hard-coding so the test stays
// stand-alone — if the engine constant changes the test will diverge
// loudly until updated, which is the intended signal.
static constexpr uint16 W = 11;
static constexpr sint32 FENCE_OFFSET = (sint32)W * W * W;  // = 1331


// Build a hex slope from per-corner heights (each 0..3).  Layout matches
// the corner_xx macros in dataobj/ribi.h: E=bits 0..1, SE=2..3, SW=4..5,
// W=6..7, NW=8..9, NE=10..11.
static slope_t::type make_slope(uint8 e, uint8 se, uint8 sw, uint8 w, uint8 nw, uint8 ne)
{
	return (slope_t::type)(
		(e  & 3)       |
		((se & 3) << 2) |
		((sw & 3) << 4) |
		((w  & 3) << 6) |
		((nw & 3) << 8) |
		((ne & 3) << 10));
}


static void check(const char *label, back_image_wall_floors_t got, sint8 lh, sint8 mh, sint8 rh)
{
	if (got.lh != lh || got.mh != mh || got.rh != rh) {
		std::fprintf(stderr,
			"%s: expected (%d, %d, %d), got (%d, %d, %d)\n",
			label, lh, mh, rh, got.lh, got.mh, got.rh);
		std::abort();
	}
}


// ---- 1. Range gates --------------------------------------------------------

static void test_range_gates()
{
	const slope_t::type s = make_slope(0, 0, 0, 0, 0, 0); // flat
	// bb == 0: no walls
	check("bb=0", decode_back_image_wall_floors(0, s, W), 2, 2, 2);
	// bb < 0: defensive — caller passes abs(), but be robust.
	check("bb=-1", decode_back_image_wall_floors(-1, s, W), 2, 2, 2);
	check("bb=-100", decode_back_image_wall_floors(-100, s, W), 2, 2, 2);
	// bb at and above fence offset: fence range, no walls.
	check("bb=fence", decode_back_image_wall_floors(FENCE_OFFSET, s, W), 2, 2, 2);
	check("bb=fence+1", decode_back_image_wall_floors(FENCE_OFFSET + 1, s, W), 2, 2, 2);
	check("bb=fence+5", decode_back_image_wall_floors(FENCE_OFFSET + 5, s, W), 2, 2, 2);
}


// ---- 2. Single-wall presence -----------------------------------------------

static void test_single_wall_presence()
{
	// Pick a slope so each "wall present" result differs from the default
	// 2 — otherwise a wall-decoding regression silently passes.  With
	// corner_sw=1, corner_nw=3, corner_ne=0: lh=1, mh=0, rh=0, all ≠ 2.
	const slope_t::type s = make_slope(0, 0, /*sw*/1, 0, /*nw*/3, /*ne*/0);
	const sint8 lh_active = 1; // min(corner_sw=1, corner_nw=3)
	const sint8 mh_active = 0; // min(corner_ne=0, corner_nw=3)
	const sint8 rh_active = 0; // min(corner_ne=0, corner_nw=3)

	// Wall 0 only — first base-W digit non-zero.
	check("wall0 bb=1", decode_back_image_wall_floors(1, s, W), lh_active, 2, 2);
	check("wall0 bb=5", decode_back_image_wall_floors(5, s, W), lh_active, 2, 2);
	check("wall0 bb=W-1", decode_back_image_wall_floors(W - 1, s, W), lh_active, 2, 2);

	// Wall 1 only — second base-W digit non-zero, first zero.
	check("wall1 bb=W", decode_back_image_wall_floors(W, s, W), 2, mh_active, 2);
	check("wall1 bb=2W", decode_back_image_wall_floors(2 * W, s, W), 2, mh_active, 2);
	check("wall1 bb=W*(W-1)", decode_back_image_wall_floors(W * (W - 1), s, W), 2, mh_active, 2);

	// Wall 2 only — third base-W digit non-zero, others zero.
	check("wall2 bb=W*W", decode_back_image_wall_floors(W * W, s, W), 2, 2, rh_active);
	check("wall2 bb=5*W*W", decode_back_image_wall_floors(5 * W * W, s, W), 2, 2, rh_active);
}


// ---- 3. Combinations -------------------------------------------------------

static void test_combinations()
{
	// Same discrimination trick as in test_single_wall_presence:
	// lh = 1, mh = rh = 0, all ≠ default 2.
	const slope_t::type s = make_slope(0, 0, /*sw*/1, 0, /*nw*/3, /*ne*/0);
	check("walls 0+1+2",  decode_back_image_wall_floors(1 + W + W*W,    s, W), 1, 0, 0);
	check("walls 0+1",    decode_back_image_wall_floors(1 + W,          s, W), 1, 0, 2);
	check("walls 0+2",    decode_back_image_wall_floors(1 + W*W,        s, W), 1, 2, 0);
	check("walls 1+2",    decode_back_image_wall_floors(W + W*W,        s, W), 2, 0, 0);
	// Max non-fence digit (10 + 10*W + 10*W*W) — every digit non-zero, all walls present.
	check("walls all-max", decode_back_image_wall_floors(FENCE_OFFSET - 1, s, W), 1, 0, 0);
}


// ---- 4. Slope inputs flow to formulas --------------------------------------

static void test_slope_inputs()
{
	// All three walls present — exercises all three formulas with one bb.
	const sint16 all_walls = 1 + W + W*W;

	// flat slope: all corners 0 → all clamps to 0.
	{
		const slope_t::type s = make_slope(0, 0, 0, 0, 0, 0);
		check("flat", decode_back_image_wall_floors(all_walls, s, W), 0, 0, 0);
	}
	// corner_sw=3, corner_nw=1: lh = min(3,1)=1; mh = min(0,1)=0; rh = min(0,1)=0.
	{
		const slope_t::type s = make_slope(0, 0, 3, 0, 1, 0);
		check("sw=3 nw=1", decode_back_image_wall_floors(all_walls, s, W), 1, 0, 0);
	}
	// corner_ne=3, corner_nw=1 (avoid mh==2 collision with the default):
	// lh = min(0,1)=0; mh = min(3,1)=1; rh = min(3,1)=1.
	{
		const slope_t::type s = make_slope(0, 0, 0, 0, 1, 3);
		check("ne=3 nw=1", decode_back_image_wall_floors(all_walls, s, W), 0, 1, 1);
	}
	// Corner heights at the hex base-4 max (3) flow through unchanged —
	// the decoder doesn't clamp against the legacy "2" sentinel.
	{
		const slope_t::type s = make_slope(0, 0, 3, 0, 3, 3);
		check("sw=3 nw=3 ne=3", decode_back_image_wall_floors(all_walls, s, W), 3, 3, 3);
	}
}


// ---- 5. mh == rh identity --------------------------------------------------

static void test_mh_rh_identity()
{
	// Sweep every slope in the base-4 6-corner space (4^6 = 4096) and
	// every (wall1, wall2) combination — confirm that whenever both
	// walls are present, they produce the same floor value.  This is
	// the legacy-square-approximation quirk the consumer relies on:
	// `wall_floor = min(lh, mh, rh)` happens to fold redundantly under
	// this formula.  If a future port distinguishes wall 1 from wall 2
	// the assert below fails loudly.
	for (uint16 sl = 0; sl < 4096; sl++) {
		const slope_t::type s = (slope_t::type)sl;
		// Walls 1 and 2 both present, wall 0 absent.
		const back_image_wall_floors_t got = decode_back_image_wall_floors(W + W*W, s, W);
		if (got.mh != got.rh) {
			std::fprintf(stderr,
				"mh != rh for slope=0x%x: mh=%d rh=%d\n",
				sl, got.mh, got.rh);
			std::abort();
		}
	}
}


// ---- 6. hide-test contribution geometry ------------------------------------
//
// Test the pure contribution-projection function.  We can't validate
// the *correctness* of the legacy approximation (no ground truth), but
// we can pin the algebraic shape so refactors stay equivalent.

static void test_contribution_flat_slope()
{
	// Flat slope, no walls, step=0, fixed scales: every corner reads 0,
	// floors default to 2 → wall_floor = 2.
	const slope_t::type s = make_slope(0, 0, 0, 0, 0, 0);
	const back_image_wall_floors_t floors = { 2, 2, 2 };
	const sint16 h = 100, scale_z = 16, scale_y = 32, step = 0;

	// i=0 in a 4-corner layout: back_off = (4-1-0)*32 = 96, fore_off = 0.
	// left  = h + min(0,2)*16 + 96 + 0 = 100 + 96 = 196
	// mid   = h + min(0*16, min(0,2)*16 + 32) + 0 = 100 + min(0, 32) = 100
	// right = h + min(0,2)*16 + 0 + 0 = 100
	const hide_test_contribution_t c0 = compute_hide_test_contribution(h, s, floors, 0, 4, scale_z, scale_y, step);
	assert(c0.left == 196);
	assert(c0.mid == 100);
	assert(c0.right == 100);

	// i=3 (rightmost): back_off = 0, fore_off = 96.
	const hide_test_contribution_t c3 = compute_hide_test_contribution(h, s, floors, 3, 4, scale_z, scale_y, step);
	assert(c3.left == 100);
	assert(c3.mid == 100);
	assert(c3.right == 100 + 96);
}


static void test_contribution_step_advances_by_scale_y()
{
	// step contributes `step * scale_y` uniformly across all three
	// slots — the back-walk shift.
	const slope_t::type s = make_slope(0, 0, 0, 0, 0, 0);
	const back_image_wall_floors_t floors = { 2, 2, 2 };
	const sint16 h = 0, scale_z = 16, scale_y = 32;

	const hide_test_contribution_t a = compute_hide_test_contribution(h, s, floors, 1, 4, scale_z, scale_y, 0);
	const hide_test_contribution_t b = compute_hide_test_contribution(h, s, floors, 1, 4, scale_z, scale_y, 1);
	assert(b.left  - a.left  == scale_y);
	assert(b.mid   - a.mid   == scale_y);
	assert(b.right - a.right == scale_y);

	const hide_test_contribution_t e = compute_hide_test_contribution(h, s, floors, 1, 4, scale_z, scale_y, 5);
	assert(e.left  - a.left  == 5 * scale_y);
	assert(e.mid   - a.mid   == 5 * scale_y);
	assert(e.right - a.right == 5 * scale_y);
}


static void test_contribution_floor_clamp()
{
	// Wall floors clamp the projected corner heights.  All four south
	// corners at 3, all walls floored at lh=1 / rh=2 → lh clamps sw, rh
	// clamps ne, wall_floor=min(1,2,2)=1 clamps nw in the mid slot.
	const slope_t::type s = make_slope(0, 3, 3, 0, 3, 3);
	const back_image_wall_floors_t floors = { 1, 2, 2 };
	const sint16 h = 0, scale_z = 16, scale_y = 32, step = 0;
	const hide_test_contribution_t c = compute_hide_test_contribution(h, s, floors, 1, 4, scale_z, scale_y, step);

	// i=1: back_off = (4-1-1)*32 = 64, fore_off = 32.
	// left  = 0 + min(3,1)*16 + 64 = 16 + 64 = 80
	// mid   = 0 + min(3*16, min(3,1)*16 + 32) = min(48, 48) = 48
	// right = 0 + min(3,2)*16 + 32 = 2*16 + 32 = 64
	assert(c.left == 80);
	assert(c.mid == 48);
	assert(c.right == 64);

	// Cross-check: setting corner_se to 0 lets corner_se dominate the
	// mid min and the mid result drops to 0 — pins the corner_se-as-min
	// arm of the formula.
	const slope_t::type s2 = make_slope(0, 0, 3, 0, 3, 3);
	const hide_test_contribution_t c2 = compute_hide_test_contribution(h, s2, floors, 1, 4, scale_z, scale_y, step);
	assert(c2.mid == 0);
}


static void test_contribution_back_fore_offset_endpoints()
{
	// Symmetry pin: back_off at i=0 equals fore_off at i=corner_count-1,
	// both `(corner_count-1) * scale_y`.  This invariant is what makes
	// the projection consistent across the new W and E corners under
	// hex (BACK_CORNER_COUNT = 4) without baking in the legacy literal 2.
	const slope_t::type s = make_slope(0, 0, 0, 0, 0, 0);
	const back_image_wall_floors_t floors = { 2, 2, 2 };
	const sint16 h = 0, scale_z = 16, scale_y = 32, step = 0;
	for (uint cc = 3; cc <= 6; cc++) {
		const hide_test_contribution_t left_end  = compute_hide_test_contribution(h, s, floors, 0, cc, scale_z, scale_y, step);
		const hide_test_contribution_t right_end = compute_hide_test_contribution(h, s, floors, cc - 1, cc, scale_z, scale_y, step);
		assert(left_end.left  == (sint16)(cc - 1) * scale_y);
		assert(right_end.right == (sint16)(cc - 1) * scale_y);
	}
}


int main()
{
	test_range_gates();
	test_single_wall_presence();
	test_combinations();
	test_slope_inputs();
	test_mh_rh_identity();
	test_contribution_flat_slope();
	test_contribution_step_advances_by_scale_y();
	test_contribution_floor_clamp();
	test_contribution_back_fore_offset_endpoints();
	std::printf("hex_back_image_test: all checks passed\n");
	return 0;
}
