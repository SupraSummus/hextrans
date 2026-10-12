// Checks the map generation shapes in dataobj/mapgen_shape.h: that they are
// round (isotropic) in the physical plane, which axial-grid formulas are not.
// The axial basis e_q, e_r is 60 degrees apart, so a set of tiles that is
// round in (q, r) has a physical gyration tensor with eigenvalues in the
// ratio 3 : 1 (an aspect of sqrt(3)).  Each check pairs the hex-aware shape
// with the legacy axial formula, so that a test that cannot fail would show.
//
// Covers karte_t::calc_climate_map_region (smoothing weights, patch shape).

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

#include "simutrans/dataobj/koord.h"
#include "simutrans/dataobj/mapgen_shape.h"


static int failures = 0;

#define CHECK(cond) do { \
	if (!(cond)) { \
		std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
		failures++; \
	} \
} while (0)


// Physical position of an axial offset, flat-top basis e_q = (sqrt3/2, 1/2), e_r = (0, 1).
static double phys_x(koord k) { return std::sqrt(3.0) / 2.0 * k.x; }
static double phys_y(koord k) { return 0.5 * k.x + k.y; }


// sqrt(l1 / l2) of the gyration tensor of the tiles in the physical
// plane: 1 for a round set, sqrt(3) for an axial circle.
static double physical_aspect(const std::vector<koord> &tiles)
{
	double cx = 0, cy = 0;
	for (koord k : tiles) {
		cx += phys_x(k);
		cy += phys_y(k);
	}
	cx /= tiles.size();
	cy /= tiles.size();
	double a = 0, b = 0, c = 0;
	for (koord k : tiles) {
		const double dx = phys_x(k) - cx, dy = phys_y(k) - cy;
		a += dx * dx;
		b += dx * dy;
		c += dy * dy;
	}
	const double tr = a + c;
	const double disc = std::sqrt((a - c) * (a - c) + 4 * b * b);
	return std::sqrt((tr + disc) / (tr - disc));
}


static std::vector<koord> patch_tiles(const climate_patch_t &patch)
{
	std::vector<koord> tiles;
	for (sint16 dr = -patch.reach_r(); dr <= patch.reach_r(); dr++) {
		for (sint16 dq = -patch.reach_q(); dq <= patch.reach_q(); dq++) {
			if (patch.contains(dq, dr)) {
				tiles.push_back(koord(dq, dr));
			}
		}
	}
	return tiles;
}


// The patch test of the square-grid generator: an ellipse on the axial
// offsets inside the wx by wy box, with the square root and the integer
// division of the original.
static bool legacy_axial_patch_contains(sint32 wx, sint32 wy, sint32 x_off, sint32 y_off)
{
	if (x_off < -(wx >> 1)  ||  x_off > wx - 1 - (wx >> 1)  ||  y_off < -(wy >> 1)  ||  y_off > wy - 1 - (wy >> 1)) {
		return false;
	}
	const uint64 distance = 1 + (uint64)std::sqrt((double)((uint64)x_off * x_off * (wx * wx) + (uint64)y_off * y_off * (wy * wy)));
	return (8 * (uint32)(wx * wx + wy * wy)) / distance > 40;
}


// The smoothing of the square grid with its weights cut to six neighbours:
// alternate neighbours weigh 1 and 2, which sum to 13, over 16.
static sint32 legacy_smooth_cell(sint32 centre, const sint32 (&neighbour)[6])
{
	sint32 sum = 4 * centre;
	for (int i = 0; i < 6; i++) {
		sum += (i & 1) ? 2 * neighbour[i] : neighbour[i];
	}
	return sum / 16;
}


// climate_smooth_cell: an average, the same in every direction.
static void test_smoothing()
{
	// a field of one climate is a fixed point
	for (sint32 c = 0; c <= 7; c++) {
		const sint32 same[6] = { c, c, c, c, c, c };
		CHECK(climate_smooth_cell(c, same) == c);
	}
	// the legacy weights are not: every pass drags temperate (4) toward
	// water (0), which is how the humidity generator turned land to sea
	{
		const sint32 same[6] = { 4, 4, 4, 4, 4, 4 };
		CHECK(legacy_smooth_cell(4, same) == 3);
	}

	// each neighbour pulls the tile by the same amount, whichever it is
	for (int i = 0; i < 6; i++) {
		sint32 impulse[6] = { 0, 0, 0, 0, 0, 0 };
		impulse[i] = 16;
		CHECK(climate_smooth_cell(0, impulse) == 2);
	}
	// ... and so does the tile itself
	{
		const sint32 none[6] = { 0, 0, 0, 0, 0, 0 };
		CHECK(climate_smooth_cell(16, none) == 4);
	}
	// the weights are a mean: the result stays between the extremes
	for (sint32 lo = 0; lo <= 7; lo++) {
		for (sint32 hi = lo; hi <= 7; hi++) {
			for (int mask = 0; mask < 64; mask++) {
				sint32 nb[6];
				for (int i = 0; i < 6; i++) {
					nb[i] = (mask >> i) & 1 ? hi : lo;
				}
				for (sint32 centre = lo; centre <= hi; centre += hi > lo ? hi - lo : 1) {
					const sint32 v = climate_smooth_cell(centre, nb);
					CHECK(v >= lo  &&  v <= hi);
				}
			}
		}
	}
}


// climate_patch_t: round in the physical plane, symmetric under the
// symmetries of the physical plane, and bounded by its reach.
static void test_patch()
{
	for (sint32 w = 4; w <= 80; w++) {
		const climate_patch_t patch(w, w);
		const std::vector<koord> tiles = patch_tiles(patch);
		CHECK(!tiles.empty());

		// the patch is the disc of the physical metric of koord.h ...
		const sint64 limit = (8 * 2 * (sint64)w * w) / 41;
		for (sint16 dr = -(sint16)w; dr <= w; dr++) {
			for (sint16 dq = -(sint16)w; dq <= w; dq++) {
				const bool in_disc = (sint64)physical_distance_sq(koord(dq, dr), koord(0, 0)) * w * w < limit * limit;
				CHECK(patch.contains(dq, dr) == in_disc);
			}
		}
		// ... so it is invariant under the 60 degree rotation
		for (koord k : tiles) {
			const koord rot = hex_axial_rotate(k, 1);
			CHECK(patch.contains(rot.x, rot.y));
		}

		if (w >= 8) {
			// round in the physical plane, to the resolution of the lattice
			CHECK(physical_aspect(tiles) < 1.05);

			// the axial ellipse of the legacy test is 3:1 elongated there
			std::vector<koord> legacy;
			for (sint16 dr = -(sint16)w; dr <= w; dr++) {
				for (sint16 dq = -(sint16)w; dq <= w; dq++) {
					if (legacy_axial_patch_contains(w, w, dq, dr)) {
						legacy.push_back(koord(dq, dr));
					}
				}
			}
			CHECK(physical_aspect(legacy) > 1.5);
		}
	}

	// any size ratio: symmetric under the half turn and both mirrors of
	// the physical plane, and nothing lies beyond the reach
	for (sint32 wx = 2; wx <= 40; wx += 3) {
		for (sint32 wy = 2; wy <= 40; wy += 3) {
			const climate_patch_t patch(wx, wy);
			const sint16 span = (sint16)(wx + wy);
			sint32 count = 0;
			for (sint16 dr = -span; dr <= span; dr++) {
				for (sint16 dq = -span; dq <= span; dq++) {
					const bool in = patch.contains(dq, dr);
					count += in;
					CHECK(in == patch.contains(-dq, -dr));    // half turn
					CHECK(in == patch.contains(-dq, dr + dq)); // x -> -x
					CHECK(in == patch.contains(dq, -dq - dr)); // y -> -y
					if (in) {
						CHECK(std::abs(dq) <= patch.reach_q()  &&  std::abs(dr) <= patch.reach_r());
					}
				}
			}
			CHECK(count > 0);
		}
	}

	// about as many tiles as the legacy patch, a little more because a
	// tile covers less of the physical plane than a square does
	{
		const std::vector<koord> now = patch_tiles(climate_patch_t(20, 20));
		sint32 legacy = 0;
		for (sint16 dr = -20; dr <= 20; dr++) {
			for (sint16 dq = -20; dq <= 20; dq++) {
				legacy += legacy_axial_patch_contains(20, 20, dq, dr);
			}
		}
		std::printf("patch 20x20: %d tiles, legacy axial %d\n", (int)now.size(), (int)legacy);
		CHECK((sint32)now.size() * 10 < legacy * 14  &&  (sint32)now.size() * 10 > legacy * 9);
	}
}


int main()
{
	test_smoothing();
	test_patch();

	if (failures) {
		std::fprintf(stderr, "hex_mapgen_test: %d check(s) failed\n", failures);
		return 1;
	}
	std::printf("hex_mapgen_test: all checks passed\n");
	return 0;
}
