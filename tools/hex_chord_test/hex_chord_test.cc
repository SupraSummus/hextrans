// Self-test for src/simutrans/vehicle/screen_chord.h: where a moving object
// is drawn inside the tile it crosses.
//
// The reference geometry comes from display/hex_proj.h (tile centres on
// screen, and hex_screen_to_fractional for the way back) and from the
// regular hex on the ground: axial (q, r) lies at (q sqrt(3) / 2, q / 2 + r)
// tile pitches, with x right and y down the screen.  Nothing here restates
// the per-direction numbers screen_chord.h produces.

#include <cmath>
#include <cstdio>
#include <cstdlib>

#include "simutrans/vehicle/screen_chord.h"
#include "simutrans/display/hex_proj.h"
#include "simutrans/dataobj/koord.h"


static int failures = 0;

#define CHECK(cond, ...) do { \
	if (!(cond)) { \
		std::printf("FAIL %s:%d: ", __FILE__, __LINE__); \
		std::printf(__VA_ARGS__); \
		std::printf("\n"); \
		failures++; \
	} \
} while (0)


// Raster widths the engine zooms through that split into W/8 pixels.
static const sint16 widths[] = { 16, 32, 48, 64, 96, 128, 192, 256 };

struct vec { double x, y; };

// Centre of the tile at axial offset k, in pixels at raster width W.
static vec centre(koord k, sint16 W)
{
	return { (double)hex_screen_dx(k.x, W), (double)hex_screen_dy(k.x, k.y, W) };
}

// Middle of the edge towards neighbour e: halfway to its centre.
static vec edge_middle(koord e, sint16 W)
{
	const vec c = centre(e, W);
	return { c.x / 2, c.y / 2 };
}

// A screen vector at raster width W on the ground, in tile pitches.
static vec ground(vec s, sint16 W)
{
	double q, r;
	hex_screen_to_fractional((sint32)std::lround(s.x * 1024), (sint32)std::lround(s.y * 1024), W, q, r);
	q /= 1024;
	r /= 1024;
	return { q * std::sqrt(3.0) / 2, q / 2 + r };
}

static double len(vec v) { return std::hypot(v.x, v.y); }

static vec offset_px(koord off64, sint16 W)
{
	return { (double)tile_raster_scale_x(off64.x, W), (double)tile_raster_scale_y(off64.y, W) };
}


// Every pair of distinct edges: the object starts on the middle of the edge
// it enters by, ends on the middle of the edge it leaves by, and moves on
// the straight line between them in steps of one lattice vector.
static void test_chord_ends()
{
	int pairs = 0;
	for (const koord &in : koord::neighbours) {
		for (const koord &out : koord::neighbours) {
			if (in == out) {
				continue;
			}
			pairs++;
			const koord span = chord_span(in, out);
			const koord step = chord_screen_step(span);
			const uint16 tile_steps = chord_tile_steps(span);
			const koord entry = chord_entry_offset(in, step);
			for (const sint16 W : widths) {
				const vec a = offset_px(entry, W);
				const vec want_a = edge_middle(in, W);
				CHECK(a.x == want_a.x && a.y == want_a.y,
				      "in (%d,%d) out (%d,%d) W %d: starts at (%g,%g), edge middle (%g,%g)",
				      in.x, in.y, out.x, out.y, W, a.x, a.y, want_a.x, want_a.y);

				const koord end = chord_screen_offset(step, tile_steps, tile_steps, W);
				const vec want_b = edge_middle(out, W);
				CHECK(a.x + end.x == want_b.x && a.y + end.y == want_b.y,
				      "in (%d,%d) out (%d,%d) W %d: ends at (%g,%g), edge middle (%g,%g)",
				      in.x, in.y, out.x, out.y, W, a.x + end.x, a.y + end.y, want_b.x, want_b.y);

				const vec chord = { want_b.x - want_a.x, want_b.y - want_a.y };
				koord prev(0, 0);
				for (uint32 s = 0; s <= tile_steps; s++) {
					const koord o = chord_screen_offset(step, s, tile_steps, W);
					CHECK(o.x * chord.y - o.y * chord.x == 0,
					      "in (%d,%d) out (%d,%d) W %d step %u: (%d,%d) off the chord",
					      in.x, in.y, out.x, out.y, W, s, o.x, o.y);
					CHECK(std::abs(o.x) >= std::abs(prev.x) && std::abs(o.y) >= std::abs(prev.y),
					      "in (%d,%d) out (%d,%d) W %d step %u: moves back", in.x, in.y, out.x, out.y, W, s);
					prev = o;
				}
				const koord first = chord_screen_offset(step, 0, tile_steps, W);
				CHECK(first.x == 0 && first.y == 0, "step 0 is not the start");
			}
		}
	}
	CHECK(pairs == 30, "%d edge pairs", pairs);
	std::printf("chord ends: %d edge pairs OK\n", pairs);
}


// A straight run crosses on the line through the two neighbouring centres
// and the tile's own; a tile takes as many steps as its chord is long on
// the ground, VEHICLE_STEPS_PER_TILE for a whole pitch, so every edge is
// crossed in the same number of steps.
static void test_lengths()
{
	const sint16 W = 64;
	for (const koord &in : koord::neighbours) {
		for (const koord &out : koord::neighbours) {
			if (in == out) {
				continue;
			}
			const koord span = chord_span(in, out);
			const vec a = edge_middle(in, W), b = edge_middle(out, W);
			const vec chord = { b.x - a.x, b.y - a.y };
			const double pitches = len(ground(chord, W));
			const double steps = chord_tile_steps(span);
			CHECK(std::fabs(steps - pitches * VEHICLE_STEPS_PER_TILE) < 1.0,
			      "in (%d,%d) out (%d,%d): %g steps for %g pitches", in.x, in.y, out.x, out.y, steps, pitches);
			if (out == -in) {
				const vec c_prev = centre(in, W), c_next = centre(out, W);
				CHECK(chord.x * c_prev.y - chord.y * c_prev.x == 0 && chord.x * c_next.y - chord.y * c_next.x == 0,
				      "straight (%d,%d): chord off the centre line", out.x, out.y);
				CHECK(steps == VEHICLE_STEPS_PER_TILE, "straight (%d,%d): %g steps", out.x, out.y, steps);
			}
		}
	}
	std::printf("lengths: OK\n");
}


// The objects with no tile before or after, or that turn back on a tile,
// cross it straight, centred on the tile.
static void test_degenerate()
{
	const koord zero(0, 0);
	const sint16 W = 64;
	for (const koord &e : koord::neighbours) {
		CHECK(chord_span(zero, e) == e * 2, "no tile before");
		CHECK(chord_span(e, e) == e * 2, "turning back");
		CHECK(chord_span(e, zero) == -e * 2, "no tile after");
		for (const koord &to_prev : { zero, e }) {
			const koord step = chord_screen_step(chord_span(to_prev, e));
			const vec a = offset_px(chord_entry_offset(to_prev, step), W);
			const koord end = chord_screen_offset(step, VEHICLE_STEPS_PER_TILE, VEHICLE_STEPS_PER_TILE, W);
			const vec want = edge_middle(e, W);
			CHECK(a.x + end.x == want.x && a.y + end.y == want.y && a.x == -want.x && a.y == -want.y,
			      "towards (%d,%d) from (%d,%d): not centred", e.x, e.y, to_prev.x, to_prev.y);
		}
	}
	std::printf("degenerate: OK\n");
}


// chord_span_of_screen_step inverts chord_screen_step, and a chord and its
// start turn with the map like the tiles around them.
static void test_inverse_and_rotation()
{
	for (const koord d : hex_disc(4)) {
		koord back;
		CHECK(chord_span_of_screen_step(chord_screen_step(d), back) && back == d, "(%d,%d) does not invert", d.x, d.y);
	}
	koord dummy;
	CHECK(!chord_span_of_screen_step(koord(1, 0), dummy), "(1,0) is not a chord");

	for (const koord &in : koord::neighbours) {
		for (const koord &out : koord::neighbours) {
			if (in == out) {
				continue;
			}
			for (int r = 1; r < 6; r++) {
				koord step = chord_screen_step(chord_span(in, out));
				koord entry = chord_entry_offset(in, step);
				CHECK(rotate_screen_lattice_vector(step, 1, r) && rotate_screen_lattice_vector(entry, 4, r), "not on the lattice");
				const koord rin = hex_axial_rotate(in, r), rout = hex_axial_rotate(out, r);
				const koord want_step = chord_screen_step(chord_span(rin, rout));
				CHECK(step == want_step, "chord in (%d,%d) out (%d,%d) turned %d", in.x, in.y, out.x, out.y, r);
				CHECK(entry == chord_entry_offset(rin, want_step), "start in (%d,%d) out (%d,%d) turned %d", in.x, in.y, out.x, out.y, r);
			}
		}
	}
	// square-era offsets are not on the lattice and stay put
	koord legacy(16, 8);
	CHECK(!rotate_screen_lattice_vector(legacy, 4, 1) && legacy == koord(16, 8), "legacy offset turned");
	std::printf("inverse and rotation: OK\n");
}


// The far side of the road: square to the heading on the ground, to its
// left, and VEHICLE_STEPS_PER_TILE steps make a tile pitch.
static void test_left()
{
	const sint16 W = 256;
	for (const koord &in : koord::neighbours) {
		for (const koord &out : koord::neighbours) {
			if (in == out) {
				continue;
			}
			const koord step = chord_screen_step(chord_span(in, out));
			const koord l = chord_left_offset(step, VEHICLE_STEPS_PER_TILE, W);
			const vec h = ground({ (double)step.x, (double)step.y }, W);
			const vec g = ground({ (double)l.x, (double)l.y }, W);
			const double hl = len(h);
			CHECK(std::fabs(len(g) - 1.0) < 0.01, "in (%d,%d) out (%d,%d): %g pitches aside", in.x, in.y, out.x, out.y, len(g));
			CHECK(std::fabs((h.x * g.x + h.y * g.y) / hl) < 0.01, "in (%d,%d) out (%d,%d): not square", in.x, in.y, out.x, out.y);
			// y down: heading south, left is east
			CHECK(h.x * g.y - h.y * g.x < 0, "in (%d,%d) out (%d,%d): not on the left", in.x, in.y, out.x, out.y);
		}
	}
	const koord south = chord_left_offset(chord_screen_step(koord(0, 2)), VEHICLE_STEPS_PER_TILE, W);
	CHECK(south.x > 0 && south.y == 0, "heading S: left is (%d,%d)", south.x, south.y);
	std::printf("left: OK\n");
}


int main()
{
	test_chord_ends();
	test_lengths();
	test_degenerate();
	test_inverse_and_rotation();
	test_left();
	if (failures) {
		std::printf("hex_chord_test: %d failure(s)\n", failures);
		return EXIT_FAILURE;
	}
	std::printf("hex_chord_test: all OK\n");
	return EXIT_SUCCESS;
}
