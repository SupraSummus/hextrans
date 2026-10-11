// Checks the per-vertex storage helpers in dataobj/koord_hex.cc against
// independent references: the lex-min over vertex_owners() for
// canonical_vertex(), and Euclidean positions from hex_vertex_pos() for
// the vertex topology.

#include <cmath>
#include <cstdio>
#include <set>

#include "simutrans/dataobj/koord.h"


static int failures = 0;

#define CHECK(cond) do { \
	if (!(cond)) { \
		std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
		failures++; \
	} \
} while (0)


static bool same(hex_vertex_t a, hex_vertex_t b)
{
	return a.tile == b.tile  &&  a.corner == b.corner;
}

static double distance(hex_vertex_t a, hex_vertex_t b)
{
	const hex_pos_t p = hex_vertex_pos(a), q = hex_vertex_pos(b);
	return std::hypot(p.x - q.x, p.y - q.y);
}

static bool lex_less(koord a, koord b)
{
	return a.x < b.x  ||  (a.x == b.x  &&  a.y < b.y);
}

template<class F> static void for_each_name_near_origin(F f)
{
	for (sint16 q = -3; q <= 3; q++) {
		for (sint16 r = -3; r <= 3; r++) {
			for (int c = 0; c < hex_corner_t::count; c++) {
				f(hex_vertex_t{ koord(q, r), (hex_corner_t::type)c });
			}
		}
	}
}


// The 3 names of a world vertex sit on mutually adjacent tiles, name one
// point and share one canonical name: the lex-min owner, always E or SE.
static void test_owners_and_canonical()
{
	for_each_name_near_origin([](hex_vertex_t v) {
		hex_vertex_t owners[3];
		vertex_owners(v.tile, v.corner, owners);
		const hex_vertex_t cv = canonical_vertex(v);
		hex_vertex_t lex_min = owners[0];
		for (int i = 0; i < 3; i++) {
			CHECK(koord_distance(owners[i].tile, owners[(i + 1) % 3].tile) == 1);
			CHECK(distance(owners[i], v) < 1e-9);
			CHECK(same(canonical_vertex(owners[i]), cv));
			if (lex_less(owners[i].tile, lex_min.tile)) {
				lex_min = owners[i];
			}
		}
		CHECK(same(cv, lex_min));
		CHECK(cv.corner == hex_corner_t::E  ||  cv.corner == hex_corner_t::SE);
	});
}


// Every vertex has exactly 3 neighbours, one hex side away.
static void test_vertex_neighbours()
{
	for_each_name_near_origin([](hex_vertex_t v) {
		hex_vertex_t nb[3];
		vertex_neighbours(v, nb);
		for (int i = 0; i < 3; i++) {
			CHECK(std::fabs(distance(nb[i], v) - 1.0) < 1e-9);
			CHECK(same(canonical_vertex(nb[i]), nb[i]));
			CHECK(!same(nb[i], nb[(i + 1) % 3]));
		}
	});
}


// surface_t indexes any canonical tile in [-1, W-1] x [-1, H]: that range
// must fill the allocation one-to-one and hold every map tile's corners.
static void test_slot_layout()
{
	const sint16 W = 6, H = 5;
	const uint32 count = vertex_slot_count(W, H);
	std::set<uint32> slots;
	for (sint16 q = -1; q < W; q++) {
		for (sint16 r = -1; r <= H; r++) {
			for (hex_corner_t::type c : { hex_corner_t::E, hex_corner_t::SE }) {
				const uint32 slot = vertex_slot_index({ koord(q, r), c }, W);
				CHECK(slot < count);
				slots.insert(slot);
			}
		}
	}
	CHECK(slots.size() == count);

	for (sint16 q = 0; q < W; q++) {
		for (sint16 r = 0; r < H; r++) {
			for (int c = 0; c < hex_corner_t::count; c++) {
				const koord t = canonical_vertex({ koord(q, r), (hex_corner_t::type)c }).tile;
				CHECK(t.x >= -1  &&  t.x < W  &&  t.y >= -1  &&  t.y <= H);
			}
		}
	}
}


int main()
{
	test_owners_and_canonical();
	test_vertex_neighbours();
	test_slot_layout();

	if (failures) {
		std::fprintf(stderr, "hex_vertex_test: %d check(s) failed\n", failures);
		return 1;
	}
	std::printf("hex_vertex_test: all checks passed\n");
	return 0;
}
