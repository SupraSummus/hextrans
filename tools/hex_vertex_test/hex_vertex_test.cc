// Checks the per-vertex storage helpers in dataobj/koord_hex.cc against
// independent references: the lex-min over vertex_owners() for
// canonical_vertex(), and Euclidean positions from hex_vertex_pos() for
// the vertex topology.  Also checks the map-rotation primitive
// hex_axial_rotate against the same positions and against the
// rotations the engine already has (ribi_t::rotate60, slope_t::rotate60).

#include <cmath>
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


// The rotation is a group action: six steps are the identity, steps
// add, and the inverse undoes it.  Three steps are the point reflection.
static void test_rotate_group()
{
	CHECK(hex_axial_rotate(koord(0, 0), 1) == koord(0, 0));
	for (sint16 q = -6; q <= 6; q++) {
		for (sint16 r = -6; r <= 6; r++) {
			const koord c(q, r);
			CHECK(hex_axial_rotate(c, 0) == c);
			CHECK(hex_axial_rotate(c, 6) == c);
			CHECK(hex_axial_rotate(c, 3) == koord(-q, -r));
			for (int a = -7; a <= 7; a++) {
				CHECK(hex_axial_rotate_inv(hex_axial_rotate(c, a), a) == c);
				CHECK(hex_axial_rotate(hex_axial_rotate_inv(c, a), a) == c);
				CHECK(hex_axial_rotate(c, -a) == hex_axial_rotate_inv(c, a));
				for (int b = -7; b <= 7; b++) {
					CHECK(hex_axial_rotate(hex_axial_rotate(c, a), b) == hex_axial_rotate(c, a + b));
				}
			}
		}
	}
}


// A step takes SE to S and S to SW: neighbours[i] to neighbours[i + 1],
// the way ribi_t::rotate60 shifts a ribi bit and slope_t::rotate60 the
// uphill direction of an edge slope.  Every other tile moves on a
// 6-orbit and keeps its hex distance from the origin.
static void test_rotate_orbits()
{
	for (int i = 0; i < 6; i++) {
		for (int k = -6; k <= 12; k++) {
			CHECK(hex_axial_rotate(koord::neighbours[i], k) == koord::neighbours[(((i + k) % 6) + 6) % 6]);
		}
	}

	for (int r = 0; r < 64; r++) {
		CHECK(koord(ribi_t::rotate60((ribi_t::ribi)r)) == hex_axial_rotate(koord((ribi_t::ribi)r), 1));
	}

	const slope_t::type edge_slopes[] = {
		slope_t::northwest_narrow, slope_t::northwest_wide, slope_t::northwest_double,
		slope_t::north_narrow,     slope_t::north_wide,     slope_t::north_double,
		slope_t::northeast_narrow, slope_t::northeast_wide, slope_t::northeast_double,
		slope_t::southeast_narrow, slope_t::southeast_wide, slope_t::southeast_double,
		slope_t::south_narrow,     slope_t::south_wide,     slope_t::south_double,
		slope_t::southwest_narrow, slope_t::southwest_wide, slope_t::southwest_double,
	};
	for (slope_t::type sl : edge_slopes) {
		CHECK(koord(slope_t::rotate60(sl)) == hex_axial_rotate(koord(sl), 1));
	}

	for (koord c : hex_disc(8)) {
		std::set<std::pair<int, int>> orbit;
		for (int k = 0; k < 6; k++) {
			const koord rc = hex_axial_rotate(c, k);
			orbit.insert(std::make_pair((int)rc.x, (int)rc.y));
			CHECK(koord_distance(koord(0, 0), rc) == koord_distance(koord(0, 0), c));
			CHECK(koord_distance(hex_axial_rotate(koord(2, -1), k), rc) == koord_distance(koord(2, -1), c));
		}
		CHECK(orbit.size() == (c == koord(0, 0) ? 1u : 6u));
	}
}


static hex_pos_t rotate_60_clockwise(hex_pos_t p)
{
	// Screen y points down, so a positive angle turns clockwise.
	const double c = 0.5, s = 0.8660254037844386;
	return { p.x * c - p.y * s, p.x * s + p.y * c };
}

// A step is a rotation of the plane by 60° clockwise, about the centre
// of tile (0, 0), on every tile corner: no convention of ours enters.
static void test_rotate_physical()
{
	for_each_name_near_origin([](hex_vertex_t v) {
		const hex_pos_t expect = rotate_60_clockwise(hex_vertex_pos(v));
		const hex_pos_t got = hex_vertex_pos(hex_axial_rotate(v, 1));
		CHECK(std::hypot(got.x - expect.x, got.y - expect.y) < 1e-9);
		CHECK(hex_axial_rotate(v, 1).corner == (v.corner + 1) % hex_corner_t::count);
	});
}


// The canonical slots (corner E or SE) of the tiles in a disc are
// closed under the rotation of their names: each step permutes them,
// no vertex is fixed (the centre is a tile centre), the three names of
// a vertex stay together, and six steps return every slot.
static void test_rotate_canonical_slots()
{
	std::set<std::pair<std::pair<int, int>, int>> slots;
	auto key = [](hex_vertex_t v) {
		return std::make_pair(std::make_pair((int)v.tile.x, (int)v.tile.y), (int)v.corner);
	};
	for (koord t : hex_disc(5)) {
		for (int c = 0; c < hex_corner_t::count; c++) {
			slots.insert(key(canonical_vertex({ t, (hex_corner_t::type)c })));
		}
	}

	for (int k = 1; k <= 6; k++) {
		std::set<std::pair<std::pair<int, int>, int>> image;
		for (koord t : hex_disc(5)) {
			for (int c = 0; c < hex_corner_t::count; c++) {
				const hex_vertex_t v = { t, (hex_corner_t::type)c };
				const hex_vertex_t cv = canonical_vertex(v);
				const hex_vertex_t rv = canonical_vertex(hex_axial_rotate(v, k));
				CHECK(same(rv, canonical_vertex(hex_axial_rotate(cv, k))));
				CHECK(rv.corner == hex_corner_t::E  ||  rv.corner == hex_corner_t::SE);
				CHECK(slots.count(key(rv)) == 1);
				CHECK(k == 6  ||  !same(rv, cv));
				CHECK(k != 6  ||  same(rv, cv));
				image.insert(key(rv));
			}
		}
		CHECK(image == slots);
	}

	// Stepping one slot at a time lands on the same slot as turning by k.
	for_each_name_near_origin([](hex_vertex_t v) {
		hex_vertex_t cur = canonical_vertex(v);
		for (int k = 1; k <= 6; k++) {
			cur = canonical_vertex(hex_axial_rotate(cur, 1));
			CHECK(same(cur, canonical_vertex(hex_axial_rotate(v, k))));
		}
		CHECK(same(cur, canonical_vertex(v)));
	});
}


int main()
{
	test_owners_and_canonical();
	test_vertex_neighbours();
	test_slot_layout();
	test_rotate_group();
	test_rotate_orbits();
	test_rotate_physical();
	test_rotate_canonical_slots();

	if (failures) {
		std::fprintf(stderr, "hex_vertex_test: %d check(s) failed\n", failures);
		return 1;
	}
	std::printf("hex_vertex_test: all checks passed\n");
	return 0;
}
