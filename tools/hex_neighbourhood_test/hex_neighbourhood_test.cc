// Checks koord_distance_to_rect and hex_disc::at in dataobj/koord.h against
// a brute-force search over the footprint and against the union of hex discs
// around its tiles.  An axial (q, r) rectangle is a parallelogram: the square
// "(-1..size)^2 ring" that square-grid neighbour loops walk around it has two
// corners, (-1,-1) and (size.x,size.y), that are not adjacent.
//
// Also pins pos_list_t / pos_list_wh_t in world/pos_list.h, the outward spiral
// of the placefinder: the hex disc, once each, closest first in the map plane.

#include <cstdio>
#include <set>
#include <utility>

#include "simutrans/dataobj/koord.h"
#include "simutrans/world/pos_list.h"


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


// pos_list_t visits the hex disc of radius M around (0, 0) once each, never
// moving to a nearer tile: physical_distance_sq() does not decrease.
static void test_pos_list_is_sorted_disc()
{
	for (sint16 m = 0; m <= 14; m++) {
		pos_list_t list(m);
		std::set<std::pair<int, int> > seen;
		koord k;
		uint32 prev = 0;
		bool first = true;

		while (list.get_next_pos(k)) {
			CHECK(seen.insert(std::make_pair((int)k.x, (int)k.y)).second);
			CHECK(koord_distance(koord(0, 0), k) <= (uint32)m);

			const uint32 d = physical_distance_sq(koord(0, 0), k);
			CHECK(d >= prev);
			prev = d;

			if (first) {
				CHECK(k == koord(0, 0));
				first = false;
			}
		}
		CHECK(seen.size() == hex_disc(m).size());
	}
}


// ... and restart() starts over, so one list serves several searches.
static void test_pos_list_restart()
{
	pos_list_t list(5);
	koord k;
	int first_run = 0;
	while (list.get_next_pos(k)) {
		first_run++;
	}
	list.restart();
	int second_run = 0;
	while (list.get_next_pos(k)) {
		second_run++;
	}
	CHECK(first_run == (int)hex_disc(5).size());
	CHECK(second_run == first_run);
}


// pos_list_wh_t anchors a w x h box so that the tile of the box nearest the
// origin along each axis, clamp(0, anchor, anchor + size - 1), walks the disc:
// every box position whose nearest tile is within M steps, once each.
static void test_pos_list_wh_anchors_every_box_once()
{
	const sint16 m = 6;
	for (sint16 w = 1; w <= 3; w++) {
		for (sint16 h = 1; h <= 3; h++) {
			pos_list_wh_t list(m, w, h);
			std::set<std::pair<int, int> > seen;
			koord k;
			while (list.get_next_pos(k)) {
				CHECK(seen.insert(std::make_pair((int)k.x, (int)k.y)).second);
			}

			std::set<std::pair<int, int> > expected;
			for (sint16 y = -3 * m; y <= 3 * m; y++) {
				for (sint16 x = -3 * m; x <= 3 * m; x++) {
					const koord nearest(clamp<sint16>(0, x, x + w - 1), clamp<sint16>(0, y, y + h - 1));
					if (koord_distance(koord(0, 0), nearest) <= (uint32)m) {
						expected.insert(std::make_pair((int)x, (int)y));
					}
				}
			}
			CHECK(seen == expected);
		}
	}
}


int main()
{
	test_pos_list_is_sorted_disc();
	test_pos_list_restart();
	test_pos_list_wh_anchors_every_box_once();
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
