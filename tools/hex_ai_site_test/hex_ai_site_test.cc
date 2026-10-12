// Checks the site-search geometry of the computer players in
// player/ai_site.h against brute force and against models of what
// player/ai.cc did on the square grid.
//
// An axial (q, r) rectangle is a parallelogram, so the rectangle grown by
// the station coverage, which ai_t::suche_platz used to walk, reaches
// 2 * coverage steps from the factory at two of its corners.  A station
// there never gets the factory's goods.  The other square-grid habits
// checked here, on stations along the NE-SW axis, are a run's length taken as
// abs(x + y), its tiles as the rectangle spanned by its ends, and the second
// try behind a shore as the first with x and y swapped.

#include <cstdio>
#include <vector>

#include "simutrans/player/ai_site.h"


static int failures = 0;

#define CHECK(cond) do { \
	if (!(cond)) { \
		std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
		failures++; \
	} \
} while (0)


// Does the catchment of a station on tile reach any tile of the footprint?
static bool brute_reaches(koord tile, koord pos, koord size, uint16 cov)
{
	for (sint16 y = 0; y < size.y; y++) {
		for (sint16 x = 0; x < size.x; x++) {
			if (koord_distance(tile, pos + koord(x, y)) <= cov) {
				return true;
			}
		}
	}
	return false;
}


// The tiles the old search walked: the footprint grown by cov in q and r.
static bool legacy_window(koord tile, koord pos, koord size, uint16 cov)
{
	return tile.x >= pos.x - cov  &&  tile.x < pos.x + size.x + cov
	    && tile.y >= pos.y - cov  &&  tile.y < pos.y + size.y + cov;
}


static void test_window_is_the_hex_dilation()
{
	const koord pos(4, -3);
	for (sint16 w = 1; w <= 5; w++) {
		for (sint16 h = 1; h <= 5; h++) {
			const koord size(w, h);
			for (uint16 cov = 0; cov <= 4; cov++) {
				for (sint16 y = -16; y <= 16; y++) {
					for (sint16 x = -16; x <= 16; x++) {
						const koord t(x, y);
						CHECK(ai_station_reaches(t, pos, size, cov) == brute_reaches(t, pos, size, cov));
					}
				}
			}
		}
	}
}


// The old window holds every tile that reaches the factory, plus two
// triangles of tiles that do not, at the (-,-) and (+,+) corners.
static void test_legacy_window_overreaches()
{
	const koord pos(0, 0);
	for (sint16 w = 1; w <= 4; w++) {
		for (sint16 h = 1; h <= 4; h++) {
			const koord size(w, h);
			for (uint16 cov = 1; cov <= 4; cov++) {
				int surplus = 0;
				for (sint16 y = -10; y <= 10; y++) {
					for (sint16 x = -10; x <= 10; x++) {
						const koord t(x, y);
						if (ai_station_reaches(t, pos, size, cov)) {
							CHECK(legacy_window(t, pos, size, cov));
						}
						else if (legacy_window(t, pos, size, cov)) {
							surplus++;
							const bool before = t.x < 0  &&  t.y < 0;
							const bool beyond = t.x >= w  &&  t.y >= h;
							CHECK(before || beyond);
						}
					}
				}
				// cov * (cov + 1) / 2 tiles in each corner
				CHECK(surplus == (int)(cov * (cov + 1)));
			}
		}
	}

	// A 1x1 factory at coverage 3: the (3,3) corner of the old window is
	// 6 steps away, whereas the (3,-3) corner is on the rim of the disc.
	CHECK(legacy_window(koord(3, 3), koord(0, 0), koord(1, 1), 3));
	CHECK(!ai_station_reaches(koord(3, 3), koord(0, 0), koord(1, 1), 3));
	CHECK(koord_distance(koord(3, 3), koord(0, 0)) == 6);
	CHECK(ai_station_reaches(koord(3, -3), koord(0, 0), koord(1, 1), 3));
}


// A run is a hex direction times a length: koord::neighbours[i] * n.  The
// old length, abs(x + y), is 0 for the NE and SW directions.
static void test_run_length_and_step()
{
	for (int i = 0; i < 6; i++) {
		const koord d = koord::neighbours[i];
		for (sint16 n = 0; n <= 8; n++) {
			CHECK(ai_run_length(d * n) == n);
			CHECK(ai_run_length(d * (sint16)-n) == n);
			CHECK(ai_run_step(d * n) == (n == 0 ? koord(0, 0) : d));
		}
		const bool along_ne_sw = d.x + d.y == 0;
		CHECK((abs(d.x * 3 + d.y * 3) == 0) == along_ne_sw);
	}
	CHECK(ai_run_length(koord(0, 0)) == 0);
	// NE and SW are the two directions of the third axis
	CHECK(abs(koord::neighbours[5].x * 4 + koord::neighbours[5].y * 4) == 0);
	CHECK(ai_run_length(koord::neighbours[5] * 4) == 4);
	CHECK(ai_run_length(koord::neighbours[2] * 4) == 4);
}


// The tiles of a run are n + 1 hex neighbours in a row.  The axial rectangle
// between its ends, which the markers used to cover, has (n + 1)^2 tiles
// along the NE-SW axis.
static void test_run_tiles()
{
	const koord place(7, -2);
	for (int i = 0; i < 6; i++) {
		const koord d = koord::neighbours[i];
		for (sint16 n = 0; n <= 6; n++) {
			std::vector<koord> tiles;
			ai_for_each_run_tile(place, d * n, [&](koord t) { tiles.push_back(t); });
			CHECK((int)tiles.size() == n + 1);
			CHECK(tiles.front() == place);
			CHECK(tiles.back() == place + d * n);
			for (size_t k = 1; k < tiles.size(); k++) {
				CHECK(koord_distance(tiles[k - 1], tiles[k]) == 1);
				CHECK(tiles[k] - tiles[k - 1] == d);
			}

			const koord last = place + d * n;
			const int rect = (abs(last.x - place.x) + 1) * (abs(last.y - place.y) + 1);
			const bool along_ne_sw = d.x + d.y == 0;
			CHECK(along_ne_sw ? rect == (n + 1) * (n + 1) : rect == n + 1);
		}
	}
}


// A station behind a shore tile whose slope rises towards uphill: the
// second try runs at 60 degrees from uphill, away from the water, and
// leans towards the destination.
static void test_shore_second_dir()
{
	for (int i = 0; i < 6; i++) {
		const koord uphill = koord::neighbours[i];
		const koord cw  = koord::neighbours[(i + 1) % 6];
		const koord ccw = koord::neighbours[(i + 5) % 6];
		const koord water = koord(-uphill.x, -uphill.y);

		for (sint16 y = -9; y <= 9; y++) {
			for (sint16 x = -9; x <= 9; x++) {
				const koord second = ai_shore_second_dir(uphill, koord(x, y));
				CHECK(second == cw || second == ccw);
				CHECK(koord_distance(second, uphill) == 1);
				CHECK(physical_dot_x2(second, uphill) > 0);
				CHECK(physical_dot_x2(second, water) < 0);
				const sint32 dot_cw  = physical_dot_x2(cw, koord(x, y));
				const sint32 dot_ccw = physical_dot_x2(ccw, koord(x, y));
				CHECK(physical_dot_x2(second, koord(x, y)) == (dot_cw > dot_ccw ? dot_cw : dot_ccw)
				      || dot_cw == dot_ccw);
			}
		}
		CHECK(ai_shore_second_dir(uphill, cw * 5) == cw);
		CHECK(ai_shore_second_dir(uphill, ccw * 5) == ccw);

		// the legacy second direction swapped x and y
		const koord swapped(uphill.y, uphill.x);
		const bool along_ne_sw = uphill.x + uphill.y == 0;
		CHECK(along_ne_sw == (swapped == water));
		CHECK(along_ne_sw == (physical_dot_x2(swapped, uphill) < 0));
	}
}


int main()
{
	test_window_is_the_hex_dilation();
	test_legacy_window_overreaches();
	test_run_length_and_step();
	test_run_tiles();
	test_shore_second_dir();

	if (failures != 0) {
		std::fprintf(stderr, "%d check(s) failed\n", failures);
		return 1;
	}
	std::printf("hex_ai_site_test: ok\n");
	return 0;
}
