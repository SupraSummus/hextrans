// Self-test for sprite_dir_of_heading() in
// src/simutrans/descriptor/sprite_dir.h: every heading a vehicle can have
// must show the sprite whose screen heading is nearest to its own.
//
// The hex side comes from display/hex_proj.h.  The sprite side comes from the
// square grid the art is drawn for: slot `dir_X` shows an object heading along
// the square step X, which the square 2:1 dimetric view puts on screen at
// ((i - j) * W/2, (i + j) * W/4) (upstream viewport_t::get_screen_coord).

#include <cmath>
#include <cstdio>
#include <cstdlib>

#include "simutrans/descriptor/sprite_dir.h"
#include "simutrans/display/hex_proj.h"
#include "simutrans/dataobj/koord.h"
#include "simutrans/dataobj/ribi.h"


static constexpr sint16 W = 64;
static const double PI = std::acos(-1.0);

static int failures = 0;

#define CHECK(cond, ...) do { \
	if (!(cond)) { \
		std::printf("FAIL %s:%d: ", __FILE__, __LINE__); \
		std::printf(__VA_ARGS__); \
		std::printf("\n"); \
		failures++; \
	} \
} while (0)


struct vec { double x, y; };

static vec hex_screen(koord step)
{
	return { (double)hex_screen_dx(step.x, W), (double)hex_screen_dy(step.x, step.y, W) };
}

// Square step shown by each slot, indexed by ribi_t::dir.
static const koord slot_square_step[8] = {
	koord( 0,  1), // dir_south
	koord(-1,  0), // dir_west
	koord(-1,  1), // dir_southwest
	koord( 1,  1), // dir_southeast
	koord( 0, -1), // dir_north
	koord( 1,  0), // dir_east
	koord( 1, -1), // dir_northeast
	koord(-1, -1), // dir_northwest
};
static const char *const slot_name[8] = { "S", "W", "SW", "SE", "N", "E", "NE", "NW" };

static vec slot_screen(int slot)
{
	const koord s = slot_square_step[slot];
	return { (double)((s.x - s.y) * W / 2), (double)((s.x + s.y) * W / 4) };
}

static double angle_deg(vec a, vec b)
{
	const double c = (a.x * b.x + a.y * b.y) / (std::hypot(a.x, a.y) * std::hypot(b.x, b.y));
	return std::acos(std::fmax(-1.0, std::fmin(1.0, c))) * 180.0 / PI;
}

static double deg_atan(double t) { return std::atan(t) * 180.0 / PI; }


// The slot nearest to a screen heading; fails if two slots come within 1°
// of a tie, which would make the table a matter of taste.
static int nearest_slot(vec h, double &err)
{
	int best = -1;
	double best_err = 1e9, second_err = 1e9;
	for (int s = 0; s < 8; s++) {
		const double e = angle_deg(h, slot_screen(s));
		if (e < best_err) {
			second_err = best_err;
			best_err = e;
			best = s;
		}
		else if (e < second_err) {
			second_err = e;
		}
	}
	CHECK(second_err - best_err > 1.0, "heading (%g,%g): slots tie", h.x, h.y);
	err = best_err;
	return best;
}


// The 6 edges: nearest slot, at most atan(1/7) = 8.13° off (NE, SE, SW, NW
// fall between a square axis and a diagonal; N and S hit a diagonal).
static void test_edges()
{
	double max_err = 0;
	for (const koord &step : koord::neighbours) {
		double err;
		const int want = nearest_slot(hex_screen(step), err);
		const ribi_t::dir got = sprite_dir_of_heading(step);
		CHECK(got == want, "edge (%d,%d): slot %s, nearest is %s (%.2f°)",
		      step.x, step.y, slot_name[got], slot_name[want], err);
		max_err = std::fmax(max_err, err);
	}
	CHECK(std::fabs(max_err - deg_atan(1.0 / 7)) < 1e-9, "edges: max error %.4f°", max_err);
	std::printf("edges:  max error %.2f°\n", max_err);
}


// The step a vehicle crossing a 60 degree bend heads along, as
// vehicle_base_t::calc_chord_direction stores it: the edge whose step has the
// chord's signs, or for (1,1) and (-1,-1), which have no such edge, the chord.
// Straights and 120 degree bends have a chord on an edge and are covered by
// test_edges.
static koord stored_step(koord chord)
{
	auto sgn = [](sint16 v) { return (v > 0) - (v < 0); };
	for (const koord &e : koord::neighbours) {
		if (sgn(e.x) == sgn(chord.x) && sgn(e.y) == sgn(chord.y)) {
			return e;
		}
	}
	return chord;
}

// The chords of the 6 bends: nearest slot when the chord is stored as itself,
// and at most atan(1/2) = 26.57° off when it is stored as an edge.
static void test_bend_chords()
{
	double max_err = 0;
	for (int i = 0; i < 6; i++) {
		const koord a = koord::neighbours[i];
		const koord b = koord::neighbours[(i + 1) % 6];
		const koord chord(a.x + b.x, a.y + b.y);
		const koord stored = stored_step(chord);
		const ribi_t::dir got = sprite_dir_of_heading(stored);
		double nearest_err;
		const int want = nearest_slot(hex_screen(chord), nearest_err);
		if (stored.x == chord.x && stored.y == chord.y) {
			CHECK(got == want, "chord (%d,%d): slot %s, nearest is %s",
			      chord.x, chord.y, slot_name[got], slot_name[want]);
		}
		const double err = angle_deg(hex_screen(chord), slot_screen(got));
		max_err = std::fmax(max_err, err);
	}
	CHECK(std::fabs(max_err - deg_atan(1.0 / 2)) < 1e-9, "chords: max error %.4f°", max_err);
	std::printf("chords: max error %.2f°\n", max_err);
}


int main()
{
	test_edges();
	test_bend_chords();
	if (failures) {
		std::printf("hex_dir_test: %d failure(s)\n", failures);
		return EXIT_FAILURE;
	}
	std::printf("hex_dir_test: all OK\n");
	return EXIT_SUCCESS;
}
