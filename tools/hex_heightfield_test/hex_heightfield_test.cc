// Self-test for the relief sampler in
// src/simutrans/dataobj/height_field_sample.h, built against it and
// dataobj/koord_hex.cc directly.  The references are independent of the
// sampler's own arithmetic: the offset of each tile corner in the image
// frame of documentation/map-shape.md as tables of thirds (columns) and
// halves (rows), computed in integers, and ramps whose bilinear value at
// a point is that point's own pixel coordinate.

#include <cstdio>
#include <vector>

#include <sys/mman.h>
#include <unistd.h>

#include "simutrans/dataobj/height_field_sample.h"


static int failures = 0;

#define CHECK(cond) do { \
	if (!(cond)) { \
		std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
		failures++; \
	} \
} while (0)


// Image column of corner c in thirds of a pixel from its tile's column
// (x = 1.5 q + cos(c*60°)), and image row in halves of a pixel from the
// tile's plane row r + (q - cut) / 2 (y = sqrt(3) * (r + q/2) +
// sin(c*60°) * sqrt(3)/2, taken to rows by dividing out sqrt(3)).
static const int CORNER_DX3[6] = {  2,  1, -1, -2, -1,  1 };
static const int CORNER_DY2[6] = {  0,  1,  1,  0, -1, -1 };

// The cuts D - 1 to test: the rhombus, small cuts, the cut of a 9x7x5 world.
static const int CUTS[4] = { 0, 1, 4, 8 };


static int floor_div(int a, int b)
{
	return a >= 0 ? a / b : -((-a + b - 1) / b);
}

static int clamp(int v, int lo, int hi)
{
	return v < lo ? lo : (v > hi ? hi : v);
}


// A pixel centre is its own value, for any field.
static void test_pixel_centres()
{
	const sint16 w = 7, h = 5;
	std::vector<sint8> field(w * h);
	for (int i = 0; i < w * h; i++) {
		field[i] = (sint8)((i * 37) % 23 - 11);
	}
	for (int j = 0; j < h; j++) {
		for (int i = 0; i < w; i++) {
			CHECK(height_field_at_pixel(field.data(), w, h, i, j) == field[i + j * w]);
		}
	}
}


// The corner positions of hex_vertex_pos(), taken to the image frame
// the way the sampler does, are the tile's column plus thirds and its
// plane row plus halves.  A ramp field makes the sampled value equal
// the column (or row), so the vertex heights come out as the rounded
// pixel coordinate plus 1; every vertex on a half row is a tie, which
// rounds up.  The columns with q - cut odd are half a row down.
static void test_vertex_pixel_position()
{
	const sint16 w = 40, h = 40;
	std::vector<sint8> ramp_x(w * h), ramp_y(w * h);
	for (int j = 0; j < h; j++) {
		for (int i = 0; i < w; i++) {
			ramp_x[i + j * w] = (sint8)i;
			ramp_y[i + j * w] = (sint8)j;
		}
	}
	for (int cut : CUTS) {
		for (sint16 r = -1; r <= h; r++) {
			for (sint16 q = -1; q <= w; q++) {
				for (int c = 0; c < hex_corner_t::count; c++) {
					const hex_vertex_t v = { koord(q, r), (hex_corner_t::type)c };
					// ramp value at the vertex, clamped to the image, rounded half up
					const int x3 = clamp(3 * q + CORNER_DX3[c], 0, 3 * (w - 1));
					const int y2 = clamp(2 * r + (q - cut) + CORNER_DY2[c], 0, 2 * (h - 1));
					CHECK(height_field_vertex_height(ramp_x.data(), w, h, v, cut) == floor_div(2 * x3 + 3, 6) + 1);
					CHECK(height_field_vertex_height(ramp_y.data(), w, h, v, cut) == floor_div(y2 + 1, 2) + 1);
				}
			}
		}
	}
}


// The 3 names of a world vertex give one height, for any field and cut.
static void test_shared_vertices_agree()
{
	const sint16 w = 12, h = 9;
	std::vector<sint8> field(w * h);
	for (int i = 0; i < w * h; i++) {
		field[i] = (sint8)((i * 53) % 31 - 15);
	}
	for (int cut : CUTS) {
		for (sint16 r = -1; r <= h; r++) {
			for (sint16 q = -1; q < w; q++) {
				for (int c = 0; c < hex_corner_t::count; c++) {
					hex_vertex_t owners[3];
					vertex_owners(koord(q, r), (hex_corner_t::type)c, owners);
					const sint8 want = height_field_vertex_height(field.data(), w, h, owners[0], cut);
					CHECK(height_field_vertex_height(field.data(), w, h, owners[1], cut) == want);
					CHECK(height_field_vertex_height(field.data(), w, h, owners[2], cut) == want);
				}
			}
		}
	}
}


// A uniform field gives a uniform map, one level above the pixel value.
static void test_constant_field()
{
	for (int value = -20; value <= 20; value += 10) {
		const sint16 w = 6, h = 4;
		std::vector<sint8> field(w * h, (sint8)value);
		for (int cut : CUTS) {
			for (sint16 r = -1; r <= h; r++) {
				for (sint16 q = -1; q < w; q++) {
					for (int c = 0; c < hex_corner_t::count; c++) {
						CHECK(height_field_vertex_height(field.data(), w, h, { koord(q, r), (hex_corner_t::type)c }, cut) == value + 1);
					}
				}
			}
		}
	}
	// the top of the sint8 range does not wrap
	const std::vector<sint8> top(4, 127);
	CHECK(height_field_vertex_height(top.data(), 2, 2, { koord(0, 0), hex_corner_t::E }, 0) == 127);
}


// Nothing outside the pixel array is read, vertices off the map
// included.  The array sits flush against a PROT_NONE page, once at
// its end and once at its start, so a stray read, even one with
// interpolation weight 0, kills the test with SIGSEGV.
static void test_no_read_outside_image()
{
	const size_t page = (size_t)sysconf(_SC_PAGESIZE);
	for (int at_end = 0; at_end <= 1; at_end++) {
		for (int dim = 1; dim <= 5; dim++) {
			const sint16 w = (sint16)dim, h = (sint16)(6 - dim);
			const size_t n = (size_t)w * h;
			char *base = (char *)mmap(NULL, 3 * page, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
			CHECK(base != MAP_FAILED);
			if (base == MAP_FAILED) {
				return;
			}
			mprotect(base, page, PROT_NONE);
			mprotect(base + 2 * page, page, PROT_NONE);
			sint8 *field = (sint8 *)(at_end ? base + 2 * page - n : base + page);
			for (size_t i = 0; i < n; i++) {
				field[i] = (sint8)(i % 5);
			}
			for (int cut : CUTS) {
				for (sint16 r = -3; r <= h + 3; r++) {
					for (sint16 q = -3; q < w + 3; q++) {
						for (int c = 0; c < hex_corner_t::count; c++) {
							const sint8 got = height_field_vertex_height(field, w, h, { koord(q, r), (hex_corner_t::type)c }, cut);
							CHECK(got >= 1  &&  got <= 5);
						}
					}
				}
			}
			munmap(base, 3 * page);
		}
	}
}


int main()
{
	test_pixel_centres();
	test_vertex_pixel_position();
	test_shared_vertices_agree();
	test_constant_field();
	test_no_read_outside_image();

	if (failures) {
		std::fprintf(stderr, "%d failure(s)\n", failures);
		return 1;
	}
	std::puts("hex_heightfield_test: ok");
	return 0;
}
