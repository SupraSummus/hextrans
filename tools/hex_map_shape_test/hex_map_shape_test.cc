// Checks map_shape_t in dataobj/map_shape.h against enumeration of the
// bounding rhombus and against the vertex topology in
// dataobj/koord_hex.cc.  See documentation/map-shape.md.

#include <cstdio>
#include <set>
#include <utility>
#include <vector>

#include "simutrans/dataobj/map_shape.h"


static int failures = 0;

#define CHECK(cond) do { \
	if (!(cond)) { \
		std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
		failures++; \
	} \
} while (0)


static std::vector<map_shape_t> sample_shapes()
{
	std::vector<map_shape_t> shapes;
	for (sint32 w = 1; w <= 6; w++) {
		for (sint32 h = 1; h <= 6; h++) {
			for (sint32 d = 1; d <= 6; d++) {
				shapes.push_back(map_shape_t(w, h, d));
			}
		}
	}
	shapes.push_back(map_shape_t(17, 9, 4));
	shapes.push_back(map_shape_t(5, 23, 11));
	return shapes;
}

static std::vector<koord> tiles_of(const map_shape_t &s)
{
	std::vector<koord> tiles;
	for (sint16 r = 0; r < s.get_size_r(); r++) {
		for (sint16 q = 0; q < s.get_size_q(); q++) {
			if (s.contains(q, r)) {
				tiles.push_back(koord(q, r));
			}
		}
	}
	return tiles;
}

static bool same_vertex(hex_vertex_t a, hex_vertex_t b)
{
	const hex_vertex_t ca = canonical_vertex(a), cb = canonical_vertex(b);
	return ca.tile == cb.tile  &&  ca.corner == cb.corner;
}


// Tile count, side lengths, the d == 1 rhombus and central symmetry.
static void test_shape(const map_shape_t &s)
{
	const sint32 Q = s.get_size_q(), R = s.get_size_r();
	CHECK(s.is_valid());
	CHECK((sint64)tiles_of(s).size() == s.get_tile_count());
	CHECK(s.get_tile_count() == (sint64)s.w*s.h + (sint64)s.h*s.d + (sint64)s.d*s.w - s.w - s.h - s.d + 1);

	// Nothing outside the bounding rhombus.
	for (sint32 i = -1; i <= Q; i++) {
		CHECK(!s.contains(i, -1)  &&  !s.contains(i, R));
	}
	for (sint32 j = -1; j <= R; j++) {
		CHECK(!s.contains(-1, j)  &&  !s.contains(Q, j));
	}

	// Side lengths: the N side (r = 0) has w tiles, the NW side
	// (q = 0) h tiles, the near cut (q + r = cut_lo) d tiles.
	int top = 0, left = 0, cut = 0;
	for (koord k : tiles_of(s)) {
		top  += k.y == 0;
		left += k.x == 0;
		cut  += k.x + k.y == s.get_cut_lo();
	}
	CHECK(top == s.w);
	CHECK(left == s.h);
	CHECK(cut == s.d);

	if (s.d == 1) {
		CHECK(s.get_tile_count() == (sint64)Q * R);
	}

	for (sint32 r = 0; r < R; r++) {
		for (sint32 q = 0; q < Q; q++) {
			CHECK(s.contains(q, r) == s.contains(Q - 1 - q, R - 1 - r));
		}
	}
}


// One step is a bijection onto rotated() that turns every neighbour
// direction one place clockwise; three steps are the central
// reflection, six the identity.
static void test_rotation(const map_shape_t &s)
{
	const map_shape_t s1 = s.rotated();
	CHECK(s1.w == s.d  &&  s1.h == s.w  &&  s1.d == s.h);
	CHECK(s1.get_size_q() == s.h + s.d - 1);
	CHECK(s1.get_size_r() == s.w + s.h - 1);

	std::set<std::pair<int, int>> image;
	for (koord k : tiles_of(s)) {
		const koord k1 = s.rotate_tile(k);
		CHECK(s1.contains(k1));
		image.insert(std::make_pair((int)k1.x, (int)k1.y));
		for (int i = 0; i < 6; i++) {
			CHECK(s.rotate_tile(k + koord::neighbours[i]) == k1 + koord::neighbours[(i + 1) % 6]);
		}
	}
	CHECK((sint64)image.size() == s1.get_tile_count());

	for (koord k : tiles_of(s)) {
		map_shape_t cur = s;
		koord p = k;
		for (int step = 1; step <= 6; step++) {
			p = cur.rotate_tile(p);
			cur = cur.rotated();
			if (step == 3) {
				CHECK(p == koord((sint16)(s.get_size_q() - 1 - k.x), (sint16)(s.get_size_r() - 1 - k.y)));
			}
		}
		CHECK(cur.w == s.w  &&  cur.h == s.h  &&  cur.d == s.d);
		CHECK(p == k);
	}
}


// Vertices of on-map tiles are on-map, keep their slots inside the
// storage range of documentation/hex-vertex-storage.md, and rotate as
// one vertex whichever of their three names is rotated.
static void test_vertices(const map_shape_t &s)
{
	const sint32 Q = s.get_size_q(), R = s.get_size_r();
	const map_shape_t s1 = s.rotated();
	for (koord k : tiles_of(s)) {
		for (int c = 0; c < hex_corner_t::count; c++) {
			const hex_vertex_t v = { k, (hex_corner_t::type)c };
			CHECK(s.contains_vertex(v));
			const hex_vertex_t cv = canonical_vertex(v);
			CHECK(cv.tile.x >= -1  &&  cv.tile.x <= Q - 1);
			CHECK(cv.tile.y >= -1  &&  cv.tile.y <= R);

			hex_vertex_t owners[3];
			vertex_owners(k, (hex_corner_t::type)c, owners);
			const hex_vertex_t v1 = s.rotate_vertex(v);
			CHECK(s1.contains_vertex(v1));
			for (const hex_vertex_t &o : owners) {
				CHECK(same_vertex(s.rotate_vertex(o), v1));
			}
		}
	}

	// A tile two steps outside the bounding rhombus has no on-map vertex.
	for (int c = 0; c < hex_corner_t::count; c++) {
		CHECK(!s.contains_vertex({ koord(-2, 0), (hex_corner_t::type)c }));
		CHECK(!s.contains_vertex({ koord((sint16)(Q + 1), (sint16)(R - 1)), (hex_corner_t::type)c }));
	}
}


int main()
{
	for (const map_shape_t &s : sample_shapes()) {
		test_shape(s);
		test_rotation(s);
		test_vertices(s);
	}

	// Regular hexagon: the hex disc of radius n-1 around the centre.
	for (sint32 n = 1; n <= 8; n++) {
		const map_shape_t s(n, n, n);
		CHECK(s.get_tile_count() == 3 * n * n - 3 * n + 1);
		const koord centre((sint16)(n - 1), (sint16)(n - 1));
		for (sint16 r = -1; r <= 2 * n; r++) {
			for (sint16 q = -1; q <= 2 * n; q++) {
				CHECK(s.contains(q, r) == (koord_distance(koord(q, r), centre) <= (uint32)(n - 1)));
			}
		}
	}

	CHECK(!map_shape_t(0, 4, 4).is_valid());

	if (failures) {
		std::fprintf(stderr, "hex_map_shape_test: %d check(s) failed\n", failures);
		return 1;
	}
	std::printf("hex_map_shape_test: all checks passed\n");
	return 0;
}
