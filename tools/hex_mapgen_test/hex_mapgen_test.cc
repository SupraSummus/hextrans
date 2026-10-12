// Checks climate_smooth_cell in dataobj/mapgen_shape.h: the weights of a
// climate smoothing pass in karte_t::calc_climate_map_region add up to their
// divisor, so a field of one climate stays that climate, and every
// neighbour of a tile weighs the same.  The legacy weights, cut from the
// square grid's eight neighbours to six, are checked to fail that.

#include <cstdio>

#include "simutrans/dataobj/mapgen_shape.h"


static int failures = 0;

#define CHECK(cond) do { \
	if (!(cond)) { \
		std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
		failures++; \
	} \
} while (0)


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


int main()
{
	test_smoothing();

	if (failures) {
		std::fprintf(stderr, "hex_mapgen_test: %d check(s) failed\n", failures);
		return 1;
	}
	std::printf("hex_mapgen_test: all checks passed\n");
	return 0;
}
