// Checks koord_distance_to_rect and hex_disc::at in dataobj/koord.h against
// a brute-force search over the footprint and against the union of hex discs
// around its tiles.  An axial (q, r) rectangle is a parallelogram: the square
// "(-1..size)^2 ring" that square-grid neighbour loops walk around it has two
// corners, (-1,-1) and (size.x,size.y), that are not adjacent.

#include <cstdio>
#include <set>
#include <utility>

#include "simutrans/dataobj/koord.h"


static int failures = 0;

#define CHECK(cond) do { \
	if (!(cond)) { \
		std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
		failures++; \
	} \
} while (0)


static uint32 brute_distance_to_rect(koord p, koord pos, koord size)
{
	uint32 best = 0xFFFFFFFFu;
	for (sint16 y = 0; y < size.y; y++) {
		for (sint16 x = 0; x < size.x; x++) {
			const uint32 d = koord_distance(p, pos + koord(x, y));
			if (d < best) {
				best = d;
			}
		}
	}
	return best;
}


// Clamping into the rectangle finds the nearest footprint tile.
static void test_matches_brute_force()
{
	const koord pos(3, -2);
	for (sint16 w = 1; w <= 5; w++) {
		for (sint16 h = 1; h <= 5; h++) {
			const koord size(w, h);
			for (sint16 y = -14; y <= 14; y++) {
				for (sint16 x = -14; x <= 14; x++) {
					const koord p(x, y);
					CHECK(koord_distance_to_rect(p, pos, size) == brute_distance_to_rect(p, pos, size));
				}
			}
		}
	}
}


// The tiles within n steps of the footprint are the union of hex discs of
// radius n around its tiles, and no other.
static void test_is_minkowski_sum_with_hex_disc()
{
	const koord pos(0, 0);
	for (sint16 w = 1; w <= 4; w++) {
		for (sint16 h = 1; h <= 4; h++) {
			const koord size(w, h);
			for (sint16 n = 0; n <= 5; n++) {
				std::set<std::pair<int, int> > grown;
				for (sint16 y = 0; y < h; y++) {
					for (sint16 x = 0; x < w; x++) {
						for (koord d : hex_disc(n)) {
							const koord t = koord(x, y) + d;
							grown.insert(std::make_pair((int)t.x, (int)t.y));
						}
					}
				}
				for (sint16 y = -12; y <= 12; y++) {
					for (sint16 x = -12; x <= 12; x++) {
						const bool within = koord_distance_to_rect(koord(x, y), pos, size) <= (uint32)n;
						CHECK(within == (grown.count(std::make_pair((int)x, (int)y)) == 1));
					}
				}
			}
		}
	}
}


// The ring one step out from a b x h footprint has 2(b+h)+2 tiles: the
// axial square ring minus its (-1,-1) and (b,h) corners.  For one tile
// that is the six neighbours and not (1,1) or (-1,-1).
static void test_adjacent_ring()
{
	const koord one(1, 1);
	CHECK(koord_distance_to_rect(koord( 1,  1), koord(0, 0), one) == 2);
	CHECK(koord_distance_to_rect(koord(-1, -1), koord(0, 0), one) == 2);
	CHECK(koord_distance_to_rect(koord( 1, -1), koord(0, 0), one) == 1);
	CHECK(koord_distance_to_rect(koord(-1,  1), koord(0, 0), one) == 1);

	for (sint16 w = 1; w <= 5; w++) {
		for (sint16 h = 1; h <= 5; h++) {
			const koord size(w, h);
			int ring = 0;
			for (sint16 y = -1; y <= h; y++) {
				for (sint16 x = -1; x <= w; x++) {
					if (koord_distance_to_rect(koord(x, y), koord(0, 0), size) == 1) {
						ring++;
					}
				}
			}
			CHECK(ring == 2 * (w + h) + 2);
			CHECK(koord_distance_to_rect(koord(-1, -1), koord(0, 0), size) == 2);
			CHECK(koord_distance_to_rect(koord(w, h), koord(0, 0), size) == 2);
			CHECK(koord_distance_to_rect(koord(w, -1), koord(0, 0), size) == 1);
			CHECK(koord_distance_to_rect(koord(-1, h), koord(0, 0), size) == 1);
		}
	}
}


// size() and at() index the disc in iteration order, so one random number
// picks a uniformly random tile of the disc.
static void test_disc_indexing()
{
	for (sint16 r = 0; r <= 12; r++) {
		const hex_disc disc(r);
		uint32 n = 0;
		for (koord d : disc) {
			CHECK(n < disc.size());
			CHECK(disc.at(n) == d);
			CHECK(koord_distance(koord(0, 0), d) <= (uint32)r);
			n++;
		}
		CHECK(n == disc.size());
		CHECK(disc.size() == 3u * r * (r + 1) + 1);
	}
}


int main()
{
	test_disc_indexing();
	test_matches_brute_force();
	test_is_minkowski_sum_with_hex_disc();
	test_adjacent_ring();

	if (failures) {
		std::fprintf(stderr, "%d failure(s)\n", failures);
		return 1;
	}
	std::printf("hex_neighbourhood_test: all checks passed\n");
	return 0;
}
