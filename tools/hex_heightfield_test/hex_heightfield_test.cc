// Self-test for the relief sampler in
// src/simutrans/dataobj/height_field_sample.h, built against it and
// dataobj/koord_hex.cc directly.  The references are independent of the
// sampler's own arithmetic: the axial offset of each tile corner as a
// table of thirds, and ramps whose bilinear value at a point is that
// point's own coordinate.

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


// Axial offset of corner c from its tile centre, in thirds of a tile:
// the corner at angle c*60° sits at (cos, sin) from the centre, and
// the centre-to-corner vector (1, 0) is (2/3, -1/3) in (q, r).
static const int CORNER_DQ3[6] = {  2,  1, -1, -2, -1,  1 };
static const int CORNER_DR3[6] = { -1,  1,  2,  1, -1, -2 };


static int round_half_up(double x)
{
	return (int)std::floor(x + 0.5);
}


// Tile centres map to their own pixel exactly: the pixel at (i, j) is
// the value at axial (i, j), for any field.
static void test_pixel_centres()
{
	const sint16 w = 7, h = 5;
	std::vector<sint8> field(w * h);
	for (int i = 0; i < w * h; i++) {
		field[i] = (sint8)((i * 37) % 23 - 11);
	}
	for (int j = 0; j < h; j++) {
		for (int i = 0; i < w; i++) {
			CHECK(height_field_at_axial(field.data(), w, h, i, j) == field[i + j * w]);
		}
	}
}


// The corner positions of hex_vertex_pos(), taken back to axial
// coordinates the way the sampler does, are the tile plus the table
// of thirds.  A ramp field makes the sampled value equal q (or r), so
// the vertex heights come out as the rounded axial coordinate plus 1.
static void test_vertex_axial_position()
{
	const sint16 w = 40, h = 40;
	std::vector<sint8> ramp_q(w * h), ramp_r(w * h);
	for (int j = 0; j < h; j++) {
		for (int i = 0; i < w; i++) {
			ramp_q[i + j * w] = (sint8)i;
			ramp_r[i + j * w] = (sint8)j;
		}
	}
	for (sint16 r = -1; r <= h; r++) {
		for (sint16 q = -1; q < w; q++) {
			for (int c = 0; c < hex_corner_t::count; c++) {
				const hex_vertex_t v = { koord(q, r), (hex_corner_t::type)c };
				// ramp value at the vertex, clamped to the image
				const double fq = q + CORNER_DQ3[c] / 3.0;
				const double fr = r + CORNER_DR3[c] / 3.0;
				const double cq = fq < 0 ? 0 : (fq > w - 1 ? w - 1 : fq);
				const double cr = fr < 0 ? 0 : (fr > h - 1 ? h - 1 : fr);
				CHECK(height_field_vertex_height(ramp_q.data(), w, h, v) == round_half_up(cq) + 1);
				CHECK(height_field_vertex_height(ramp_r.data(), w, h, v) == round_half_up(cr) + 1);
			}
		}
	}
}


// The 3 names of a world vertex give one height, for any field.
static void test_shared_vertices_agree()
{
	const sint16 w = 12, h = 9;
	std::vector<sint8> field(w * h);
	for (int i = 0; i < w * h; i++) {
		field[i] = (sint8)((i * 53) % 31 - 15);
	}
	for (sint16 r = -1; r <= h; r++) {
		for (sint16 q = -1; q < w; q++) {
			for (int c = 0; c < hex_corner_t::count; c++) {
				hex_vertex_t owners[3];
				vertex_owners(koord(q, r), (hex_corner_t::type)c, owners);
				const sint8 want = height_field_vertex_height(field.data(), w, h, owners[0]);
				CHECK(height_field_vertex_height(field.data(), w, h, owners[1]) == want);
				CHECK(height_field_vertex_height(field.data(), w, h, owners[2]) == want);
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
		for (sint16 r = -1; r <= h; r++) {
			for (sint16 q = -1; q < w; q++) {
				for (int c = 0; c < hex_corner_t::count; c++) {
					CHECK(height_field_vertex_height(field.data(), w, h, { koord(q, r), (hex_corner_t::type)c }) == value + 1);
				}
			}
		}
	}
	// the top of the sint8 range does not wrap
	const std::vector<sint8> top(4, 127);
	CHECK(height_field_vertex_height(top.data(), 2, 2, { koord(0, 0), hex_corner_t::E }) == 127);
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
			for (sint16 r = -3; r <= h + 3; r++) {
				for (sint16 q = -3; q < w + 3; q++) {
					for (int c = 0; c < hex_corner_t::count; c++) {
						const sint8 got = height_field_vertex_height(field, w, h, { koord(q, r), (hex_corner_t::type)c });
						CHECK(got >= 1  &&  got <= 5);
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
	test_vertex_axial_position();
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
