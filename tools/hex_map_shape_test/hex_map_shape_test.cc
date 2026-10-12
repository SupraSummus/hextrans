// Checks map_shape_t in dataobj/map_shape.h against enumeration of the
// bounding rhombus and against the vertex topology in
// dataobj/koord_hex.cc, and the map rotations that take a map extent
// (koord::rotate90, koord3d::rotate90, array2d_tpl::rotate90) against
// map_shape_t::rotate_tile.  Also the relief image frame, the vertex slot
// band and the noise cache bound.  See documentation/map-shape.md.

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <set>
#include <utility>
#include <vector>

#include "simutrans/dataobj/koord3d.h"
#include "simutrans/dataobj/map_shape.h"
#include "simutrans/tpl/array2d_tpl.h"


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

	// The branchless form surface_t::is_within_limits caches, over a
	// margin around the rhombus wide enough to cross both cut lines.
	for (sint32 r = -R - 2; r <= 2 * R + 2; r++) {
		for (sint32 q = -Q - 2; q <= 2 * Q + 2; q++) {
			CHECK(s.contains(q, r) == map_shape_t::contains_cached(q, r, Q - 1, R - 1, s.get_cut_lo(), s.get_cut_hi()));
		}
	}
}


// The per-row range is exactly the on-map part of the row.  The edge
// distance of an on-map tile is one less than the hex distance to the
// nearest off-map cell (all of which neighbour the map inside the ring
// around the rhombus), and negative off the map.  A parallelogram
// footprint's minimum sits at one of its two extreme corners, which is
// what surface_t::get_edge_distance(pos, size) reads.
static void test_rows_and_edges(const map_shape_t &s)
{
	const sint32 Q = s.get_size_q(), R = s.get_size_r();
	for (sint32 r = 0; r < R; r++) {
		CHECK(s.get_q_begin(r) < s.get_q_end(r));
		for (sint32 q = -1; q <= Q; q++) {
			CHECK(s.contains(q, r) == (s.get_q_begin(r) <= q  &&  q < s.get_q_end(r)));
		}
	}

	std::vector<koord> off;
	for (sint16 r = -1; r <= R; r++) {
		for (sint16 q = -1; q <= Q; q++) {
			if (!s.contains(q, r)) {
				off.push_back(koord(q, r));
				CHECK(s.edge_distance(koord(q, r)) < 0);
			}
		}
	}
	for (koord k : tiles_of(s)) {
		sint32 nearest = 1 << 20;
		for (koord o : off) {
			nearest = std::min(nearest, (sint32)koord_distance(k, o));
		}
		CHECK(s.edge_distance(k) == nearest - 1);

		for (sint16 sy = 1; sy <= 3; sy++) {
			for (sint16 sx = 1; sx <= 3; sx++) {
				sint32 lowest = 1 << 20;
				for (sint16 y = 0; y < sy; y++) {
					for (sint16 x = 0; x < sx; x++) {
						lowest = std::min(lowest, s.edge_distance(k + koord(x, y)));
					}
				}
				CHECK(lowest == std::min(s.edge_distance(k), s.edge_distance(k + koord((sint16)(sx - 1), (sint16)(sy - 1)))));
			}
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


// The canonical slots of the per-vertex height arrays: one step
// permutes the on-map vertices of s onto those of rotated(), through
// rotate_vertex and canonical_vertex, and the slots stay inside the
// storage range of the rotated extents.  Six steps return every slot
// of the world, whichever shape the cycle passes through.
static void test_vertex_slots(const map_shape_t &s)
{
	typedef std::pair<std::pair<int, int>, int> slot_t;
	auto key = [](hex_vertex_t v) {
		return std::make_pair(std::make_pair((int)v.tile.x, (int)v.tile.y), (int)v.corner);
	};

	std::vector<hex_vertex_t> before;
	{
		std::set<slot_t> seen;
		for (koord k : tiles_of(s)) {
			for (int c = 0; c < hex_corner_t::count; c++) {
				const hex_vertex_t cv = canonical_vertex({ k, (hex_corner_t::type)c });
				CHECK(cv.corner == hex_corner_t::E  ||  cv.corner == hex_corner_t::SE);
				if (seen.insert(key(cv)).second) {
					before.push_back(cv);
				}
			}
		}
	}

	std::vector<hex_vertex_t> cur = before;
	map_shape_t cur_shape = s;
	for (int step = 1; step <= 6; step++) {
		const map_shape_t next_shape = cur_shape.rotated();
		const sint32 Q = next_shape.get_size_q(), R = next_shape.get_size_r();
		std::set<slot_t> image;
		std::set<uint32> indices;
		for (hex_vertex_t &v : cur) {
			CHECK(cur_shape.contains_vertex(v));
			v = canonical_vertex(cur_shape.rotate_vertex(v));
			CHECK(v.corner == hex_corner_t::E  ||  v.corner == hex_corner_t::SE);
			CHECK(next_shape.contains_vertex(v));
			CHECK(v.tile.x >= -1  &&  v.tile.x <= Q - 1  &&  v.tile.y >= -1  &&  v.tile.y <= R);
			image.insert(key(v));
			const uint32 slot = vertex_slot_index(v, (sint16)Q);
			CHECK(slot < vertex_slot_count((sint16)Q, (sint16)R));
			indices.insert(slot);
		}
		// Injective, and onto the on-map vertices of the rotated shape.
		CHECK(image.size() == before.size());
		CHECK(indices.size() == before.size());
		std::set<slot_t> expect;
		for (koord k : tiles_of(next_shape)) {
			for (int c = 0; c < hex_corner_t::count; c++) {
				expect.insert(key(canonical_vertex({ k, (hex_corner_t::type)c })));
			}
		}
		CHECK(image == expect);
		cur_shape = next_shape;
	}
	for (size_t i = 0; i < before.size(); i++) {
		CHECK(same_vertex(cur[i], before[i]));
	}
}


// The relief image frame: column q, row r + floor((q - t) / 2).  Tile
// centres sit at row r + q/2 - t/2 of the plane, whose columns with
// q - t odd are half a row below the integer pixel row.  Every on-map
// tile has its own pixel inside the image, the pixels that map back
// onto the map are exactly the tiles, and the image size gives the
// shape back.
static void test_image_frame(const map_shape_t &s)
{
	const sint32 cols = s.get_image_cols(), rows = s.get_image_rows();
	const sint32 t = s.get_cut_lo();
	CHECK(cols == s.get_size_q());
	CHECK(rows == s.h + (s.get_size_q() / 2));

	std::set<std::pair<int, int> > pixels;
	for (koord k : tiles_of(s)) {
		const sint32 j = s.get_image_row(k.x, k.y);
		CHECK(j >= 0  &&  j < rows);
		pixels.insert(std::make_pair((int)k.x, (int)j));
		CHECK(s.get_tile_row(k.x, j) == k.y);

		// the plane row of the centre, from the physical position
		const double plane_row = k.y + 0.5 * k.x - 0.5 * t;
		CHECK(plane_row - j == ((k.x - t) & 1 ? 0.5 : 0.0));
	}
	CHECK((sint64)pixels.size() == s.get_tile_count());

	sint64 on_map = 0;
	for (sint32 j = 0; j < rows; j++) {
		for (sint32 i = 0; i < cols; i++) {
			on_map += s.contains(i, s.get_tile_row(i, j));
		}
	}
	CHECK(on_map == s.get_tile_count());

	// w follows from the columns and the cut, h from the rows
	const map_shape_t back = map_shape_t::from_image_size(cols, rows, s.d);
	CHECK(back.w == s.w  &&  back.h == s.h  &&  back.d == s.d);
	// the rows do not depend on d, the columns minus d - 1 give w
	for (sint32 d = 1; d <= cols + 1; d++) {
		const map_shape_t o = map_shape_t::from_image_size(cols, rows, d);
		CHECK(o.h == s.h);
		CHECK(o.w == cols - d + 1);
		CHECK(o.is_valid() == (d <= cols));
	}

	// a regular hexagon has a square image
	if (s.w == s.h  &&  s.h == s.d) {
		CHECK(cols == rows);
	}
}


// slot_near_map() skips only canonical slots whose vertex is off the
// map and has no on-map neighbour, everything at d == 1, and it keeps
// every slot of an on-map vertex.
static void test_slot_band(const map_shape_t &s)
{
	const sint32 Q = s.get_size_q(), R = s.get_size_r();
	int skipped = 0;
	for (sint32 r = -1; r <= R; r++) {
		for (sint32 q = -1; q < Q; q++) {
			CHECK(s.slot_near_map(q, r) == map_shape_t::slot_near_map_cached(q, r, s.get_cut_lo(), s.get_cut_hi()));
			for (int c = 0; c < 2; c++) {
				const hex_vertex_t v = { koord((sint16)q, (sint16)r), c ? hex_corner_t::SE : hex_corner_t::E };
				if (s.slot_near_map(q, r)) {
					continue;
				}
				skipped++;
				CHECK(!s.contains_vertex(v));
				hex_vertex_t nb[3];
				vertex_neighbours(v, nb);
				for (const hex_vertex_t &n : nb) {
					CHECK(!s.contains_vertex(n));
				}
			}
		}
	}
	for (koord k : tiles_of(s)) {
		for (int c = 0; c < hex_corner_t::count; c++) {
			const hex_vertex_t cv = canonical_vertex({ k, (hex_corner_t::type)c });
			CHECK(s.slot_near_map(cv.tile.x, cv.tile.y));
		}
	}
	CHECK((s.d == 1) == (skipped == 0));
}


// The noise cache bound of get_vertex_pos_bound(): above every slot
// generation fills, exact in y, within one tile in x.
static void test_noise_bound(const map_shape_t &s)
{
	const sint32 Q = s.get_size_q(), R = s.get_size_r();
	const hex_pos_t bound = s.get_vertex_pos_bound();
	double max_x = 0.0, max_y = 0.0;
	for (sint32 r = -1; r <= R; r++) {
		for (sint32 q = -1; q < Q; q++) {
			if (!s.slot_near_map(q, r)) {
				continue;
			}
			for (int c = 0; c < 2; c++) {
				const hex_pos_t p = hex_vertex_pos({ koord((sint16)q, (sint16)r), c ? hex_corner_t::SE : hex_corner_t::E });
				max_x = std::max(max_x, p.x);
				max_y = std::max(max_y, p.y);
			}
		}
	}
	CHECK(max_x <= bound.x  &&  bound.x - max_x <= 1.0);
	CHECK(max_y <= bound.y + 1e-9  &&  bound.y - max_y <= 1e-9);
}


// The density scale is the side of the square map with the same tiles.
static void test_side_equivalent()
{
	for (sint32 w = 1; w <= 40; w++) {
		for (sint32 h = 1; h <= 40; h++) {
			CHECK(map_shape_t(w, h, 1).get_side_equivalent() == std::sqrt((double)w * h));
		}
	}
	// the default 150 x 150 x 150 hexagon has the tiles of a 259 x 259 map
	CHECK(map_shape_t(150, 150, 150).get_tile_count() == 67051);
	CHECK(std::fabs(map_shape_t(150, 150, 150).get_side_equivalent() - 258.94) < 0.01);
	CHECK(std::fabs(map_shape_t(150, 150, 150).get_side_equivalent() / map_shape_t(259, 259, 1).get_side_equivalent() - 1.0) < 0.001);
	CHECK(std::fabs(map_shape_t(48, 40, 32).get_side_equivalent() - std::sqrt(48.0*40 + 40.0*32 + 32.0*48 - 48 - 40 - 32 + 1)) < 1e-9);
}


// koord::rotate90( y_size ) is the d == 1 spelling of rotate_tile, which
// shifts by -(d - 1) along r on top: y_size = h + d - 2 for any shape.
// koord3d keeps its height, and a step takes SE to S as for the ribis.
static void test_rotate90_forms(const map_shape_t &s)
{
	for (koord k : tiles_of(s)) {
		koord k1 = k;
		k1.rotate90((sint16)(s.h + s.d - 2));
		k1.y = (sint16)(k1.y - (s.d - 1));
		CHECK(k1 == s.rotate_tile(k));

		if (s.d == 1) {
			koord k2 = k;
			k2.rotate90((sint16)(s.h - 1));
			CHECK(k2 == s.rotate_tile(k));
			for (sint8 z : { (sint8)-3, (sint8)0, (sint8)7 }) {
				koord3d p(k, z);
				p.rotate90((sint16)(s.h - 1));
				CHECK(p.get_2d() == s.rotate_tile(k)  &&  p.z == z);
			}
		}
	}
}


// array2d_tpl::rotate90 on a w x h array, one cell per tile of the
// rhombus (w, h, 1): each cell moves with rotate_tile into the bounding
// rhombus of the rotated hexagon, and the cells off the hexagon are
// empty.
static void test_array_rotate(sint32 w, sint32 h)
{
	const map_shape_t s(w, h, 1);
	const map_shape_t s1 = s.rotated();
	array2d_tpl<uint32> a((unsigned)w, (unsigned)h);
	for (sint32 y = 0; y < h; y++) {
		for (sint32 x = 0; x < w; x++) {
			const_cast<uint32 *>(a.to_array())[x + y * w] = (uint32)(1 + x + y * w);
		}
	}

	a.rotate90();
	CHECK((sint32)a.get_width() == s1.get_size_q());
	CHECK((sint32)a.get_height() == s1.get_size_r());
	std::set<uint32> hexagon;
	for (sint32 y = 0; y < h; y++) {
		for (sint32 x = 0; x < w; x++) {
			const koord k = s.rotate_tile(koord((sint16)x, (sint16)y));
			CHECK(s1.contains(k));
			CHECK(a.to_array()[k.x + k.y * a.get_width()] == (uint32)(1 + x + y * w));
			hexagon.insert((uint32)(k.x + k.y * a.get_width()));
		}
	}
	CHECK((sint64)hexagon.size() == s1.get_tile_count());
	for (unsigned i = 0; i < a.get_width() * a.get_height(); i++) {
		CHECK((a.to_array()[i] == 0) == (hexagon.count(i) == 0));
	}
}


int main()
{
	for (const map_shape_t &s : sample_shapes()) {
		test_shape(s);
		test_rows_and_edges(s);
		test_rotation(s);
		test_vertices(s);
		test_vertex_slots(s);
		test_rotate90_forms(s);
		test_image_frame(s);
		test_slot_band(s);
		test_noise_bound(s);
	}
	test_side_equivalent();
	CHECK(!map_shape_t::from_image_size(5, 2, 1).is_valid());
	for (sint32 w = 1; w <= 7; w++) {
		for (sint32 h = 1; h <= 7; h++) {
			test_array_rotate(w, h);
		}
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
