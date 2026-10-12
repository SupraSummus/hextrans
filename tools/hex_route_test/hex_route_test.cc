// Checks the A* distance heuristic of way_builder_t::calc_distance, which is
// koord_distance_to_rect in dataobj/koord.h over the bounding box of the
// targets, against a breadth-first search over the six hex neighbours.
//
// An A* heuristic must never exceed the true number of steps to the target
// (admissible), and must not drop by more than one per step (consistent), or
// the first route found is not the cheapest.  The box is the bounding box of
// the targets in axial (q, r), so the true distance is the fewest steps to
// any tile of the box.

#include <cstdio>
#include <cstdlib>
#include <map>
#include <queue>
#include <utility>

#include "simutrans/dataobj/koord.h"


static int failures = 0;

#define CHECK(cond) do { \
	if (!(cond)) { \
		std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
		failures++; \
	} \
} while (0)


typedef std::pair<int, int> cell_t;


// Fewest hex steps from every tile within `limit` steps of the box to the
// nearest box tile, by BFS from all box tiles at once.
static std::map<cell_t, int> steps_to_box(const koord &mini, const koord &maxi, int limit)
{
	std::map<cell_t, int> dist;
	std::queue<cell_t> todo;
	for (int q = mini.x; q <= maxi.x; q++) {
		for (int r = mini.y; r <= maxi.y; r++) {
			dist[cell_t(q, r)] = 0;
			todo.push(cell_t(q, r));
		}
	}
	while (!todo.empty()) {
		const cell_t c = todo.front();
		todo.pop();
		const int d = dist[c];
		if (d == limit) {
			continue;
		}
		for (int k = 0; k < 6; k++) {
			const cell_t n(c.first + koord::neighbours[k].x, c.second + koord::neighbours[k].y);
			if (dist.find(n) == dist.end()) {
				dist[n] = d + 1;
				todo.push(n);
			}
		}
	}
	return dist;
}


// The call way_builder_t::calc_distance makes for the box [mini, maxi].
static uint32 box_distance(const koord &pos, const koord &mini, const koord &maxi)
{
	return koord_distance_to_rect(pos, mini, maxi - mini + koord(1, 1));
}


// What way_builder_t::calc_distance computed before the hex port reached it:
// the axial Manhattan length of the out-of-box deltas.
static uint32 legacy_manhattan_to_box(const koord &pos, const koord &mini, const koord &maxi)
{
	uint32 dist = 0;
	if     (pos.x < mini.x) { dist += mini.x - pos.x; }
	else if(pos.x > maxi.x) { dist += pos.x - maxi.x; }
	if     (pos.y < mini.y) { dist += mini.y - pos.y; }
	else if(pos.y > maxi.y) { dist += pos.y - maxi.y; }
	return dist;
}


// A box of one tile up to 4x3 at a few corners, and every tile within 9 steps.
static void for_each_case(void (*check)(const koord &pos, const koord &mini, const koord &maxi, int true_steps, const std::map<cell_t, int> &dist))
{
	const int limit = 12;
	for (int q = -2; q <= 1; q++) {
		for (int r = -2; r <= 1; r++) {
			for (int w = 0; w < 4; w++) {
				for (int h = 0; h < 3; h++) {
					const koord mini(q, r);
					const koord maxi(q + w, r + h);
					const std::map<cell_t, int> dist = steps_to_box(mini, maxi, limit);
					for (const koord &d : hex_disc(9)) {
						const koord pos = mini + d;
						check(pos, mini, maxi, dist.at(cell_t(pos.x, pos.y)), dist);
					}
				}
			}
		}
	}
}


static int legacy_overestimates = 0;
static int legacy_exact_cases = 0;


static void check_case(const koord &pos, const koord &mini, const koord &maxi, int true_steps, const std::map<cell_t, int> &dist)
{
	const int h = (int)box_distance(pos, mini, maxi);

	// the heuristic is the true number of steps, so it is admissible
	CHECK(h == true_steps);

	// consistent: one step toward or away changes it by at most one
	for (int k = 0; k < 6; k++) {
		const koord n = pos + koord::neighbours[k];
		const std::map<cell_t, int>::const_iterator it = dist.find(cell_t(n.x, n.y));
		if (it != dist.end()) {
			CHECK(h <= 1 + (int)box_distance(n, mini, maxi));
		}
	}

	// negative control: the formula it replaced is inadmissible, so the
	// checks above can tell the two apart
	const int old_h = (int)legacy_manhattan_to_box(pos, mini, maxi);
	CHECK(old_h >= true_steps);
	if (old_h > true_steps) {
		legacy_overestimates++;
	}
	else {
		legacy_exact_cases++;
	}
}


// A displacement along NE-SW is its own hex distance, twice its Manhattan one.
static void test_ne_sw_axis()
{
	for (int k = 1; k <= 40; k++) {
		const koord target(5, 5);
		CHECK(box_distance(target + koord(k, -k), target, target) == (uint32)k);
		CHECK(box_distance(target + koord(-k, k), target, target) == (uint32)k);
		CHECK(legacy_manhattan_to_box(target + koord(k, -k), target, target) == (uint32)(2 * k));
		// the other two axes agree with the legacy formula
		CHECK(box_distance(target + koord(k, 0), target, target) == (uint32)k);
		CHECK(box_distance(target + koord(0, -k), target, target) == (uint32)k);
	}
}


// Inside the box it is zero, and on its rim one step out it is one.
static void test_inside_and_rim()
{
	const koord mini(2, 3), maxi(6, 5);
	for (int q = mini.x; q <= maxi.x; q++) {
		for (int r = mini.y; r <= maxi.y; r++) {
			CHECK(box_distance(koord(q, r), mini, maxi) == 0);
		}
	}
	CHECK(box_distance(koord(7, 4), mini, maxi) == 1);
	CHECK(box_distance(koord(1, 4), mini, maxi) == 1);
	CHECK(box_distance(koord(4, 2), mini, maxi) == 1);
	CHECK(box_distance(koord(4, 6), mini, maxi) == 1);
}


int main()
{
	for_each_case(check_case);
	test_ne_sw_axis();
	test_inside_and_rim();

	// the old formula is wrong on a large part of the plane; if this ever
	// reads zero the negative control no longer tests anything
	CHECK(legacy_overestimates > 1000);
	CHECK(legacy_exact_cases > 1000);

	if (failures) {
		std::fprintf(stderr, "%d check(s) failed\n", failures);
		return 1;
	}
	std::printf("hex_route_test: ok (legacy overestimates %d of %d cases)\n",
		legacy_overestimates, legacy_overestimates + legacy_exact_cases);
	return 0;
}
