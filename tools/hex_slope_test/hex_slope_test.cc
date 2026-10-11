// Self-test for slope_t::raise_base in src/simutrans/dataobj/ribi.h —
// the clamp that map generation and correct_water apply to shoreline
// tiles.  Pure inline function, header included directly, no engine link.
//
// The reference is the absolute corner height: a tile at base b whose
// corner c stands at b + corner_c(s), under water at w, shows that
// corner at max(b + corner_c(s), w).  Slope s' = raise_base(s, w - b)
// on base w must reproduce it for all 6 corners.  The legacy 4-corner
// build left E and W flat, so those two are checked on their own too.

#include <cstdio>

#include "simutrans/dataobj/ribi.h"


static int failures = 0;

#define CHECK(cond) do { \
	if (!(cond)) { \
		std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
		failures++; \
	} \
} while (0)


static int corner(slope_t::type s, int c)
{
	switch (c) {
		case hex_corner_t::E:  return corner_e(s);
		case hex_corner_t::SE: return corner_se(s);
		case hex_corner_t::SW: return corner_sw(s);
		case hex_corner_t::W:  return corner_w(s);
		case hex_corner_t::NW: return corner_nw(s);
		default:               return corner_ne(s);
	}
}


// Every 6-corner slope (4096 base-4 values) against every water level
// from the base up to past the highest corner.
static void test_corner_heights_follow_water()
{
	const sint8 base = -2;
	for (int s = 0; s < 4096; s++) {
		for (sint8 water = base; water <= base + 4; water++) {
			const slope_t::type got = slope_t::raise_base((slope_t::type)s, (sint8)(water - base));
			for (int c = 0; c < hex_corner_t::count; c++) {
				const int want = max(base + corner((slope_t::type)s, c), (int)water);
				CHECK(water + corner(got, c) == want);
			}
		}
	}
}


// No water above the base leaves the slope alone.
static void test_no_change_at_base()
{
	for (int s = 0; s < 4096; s++) {
		CHECK(slope_t::raise_base((slope_t::type)s, 0) == s);
	}
}


// The two hex-only corners: E or W alone above a flat tile survives a
// level of 0 and is submerged by a level of 1.
static void test_e_and_w_corners()
{
	CHECK(slope_t::raise_base(slope_t::raised_E, 0) == slope_t::raised_E);
	CHECK(slope_t::raise_base(slope_t::raised_W, 0) == slope_t::raised_W);
	CHECK(slope_t::raise_base(slope_t::raised_E, 1) == slope_t::flat);
	CHECK(slope_t::raise_base(slope_t::raised_W, 1) == slope_t::flat);

	// a one-high tile with E and W up and the rest down, under water at
	// its own base: unchanged; a double-high one loses a level
	const slope_t::type ew = slope_t::raised_E + slope_t::raised_W;
	CHECK(slope_t::raise_base(ew, 0) == ew);
	CHECK(slope_t::raise_base(2 * ew, 1) == ew);
}


int main()
{
	test_corner_heights_follow_water();
	test_no_change_at_base();
	test_e_and_w_corners();

	if (failures) {
		std::fprintf(stderr, "%d failure(s)\n", failures);
		return 1;
	}
	std::puts("hex_slope_test: ok");
	return 0;
}
