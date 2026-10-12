// Checks the site-search geometry of the computer players in
// player/ai_site.h against brute force and against models of what
// player/ai.cc did on the square grid.
//
// An axial (q, r) rectangle is a parallelogram, so the rectangle grown by
// the station coverage, which ai_t::suche_platz used to walk, reaches
// 2 * coverage steps from the factory at two of its corners.  A station
// there never gets the factory's goods.

#include <cstdio>

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


int main()
{
	test_window_is_the_hex_dilation();
	test_legacy_window_overreaches();

	if (failures != 0) {
		std::fprintf(stderr, "%d check(s) failed\n", failures);
		return 1;
	}
	std::printf("hex_ai_site_test: ok\n");
	return 0;
}
