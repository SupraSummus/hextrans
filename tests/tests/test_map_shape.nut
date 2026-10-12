//
// This file is part of the Simutrans project under the Artistic License.
// (see LICENSE.txt)
//


//
// Tests for the W x H x D map hexagon (documentation/map-shape.md).
//
// They read the shape from the settings, so they run on the suite's
// 16x16 fixture (D = 1, nothing cut) and, through `tools/test.py --map`,
// on tests/empty-hex-9x7x5.sve.  That fixture was written by the
// `-flatmap 9,7,5` hook and is loaded by the scenario, so the shape and
// tile assertions also check that save and load keep W, H, D and skip
// the off-map cells in step.
//


// map_shape_t::contains: the bounding rhombus minus the two cut corners
function map_shape_contains(w, h, d, x, y)
{
	local t = d - 1
	return x >= 0  &&  y >= 0  &&  x < w + t  &&  y < h + t
		&&  x + y >= t  &&  x + y <= w + h + t - 2
}


function map_shape_has_ground(x, y)
{
	return square_x(x, y).is_valid()
}


function test_map_shape_extents()
{
	local w = settings.get_map_side_w()
	local h = settings.get_map_side_h()
	local d = settings.get_map_side_d()
	ASSERT_TRUE(w >= 1)
	ASSERT_TRUE(h >= 1)
	ASSERT_TRUE(d >= 1)

	local size = world.get_size()
	ASSERT_EQUAL(size.x, w + d - 1)
	ASSERT_EQUAL(size.y, h + d - 1)
}


// Every on-map cell has ground, every off-map cell (the cut corners and
// one ring around the bounding box) has none, and the tile count is
// WH + HD + DW - W - H - D + 1.
function test_map_shape_tiles()
{
	local w = settings.get_map_side_w()
	local h = settings.get_map_side_h()
	local d = settings.get_map_side_d()
	local size = world.get_size()

	local count = 0
	for (local y = -1; y <= size.y; y++) {
		for (local x = -1; x <= size.x; x++) {
			local on_map = map_shape_contains(w, h, d, x, y)
			ASSERT_EQUAL(map_shape_has_ground(x, y), on_map)
			// lookup of any height off the map is NULL
			ASSERT_EQUAL(tile_x(x, y, 0).is_valid(), on_map)
			if (on_map) {
				count++
			}
		}
	}
	ASSERT_EQUAL(count, w*h + h*d + d*w - w - h - d + 1)
}


// A road from a tile on a cut line (or on the map edge, for D = 1) into
// the off-map cell next to it is refused and builds nothing.
function test_map_shape_no_way_into_cut()
{
	local pl = player_x(0)
	local desc = way_desc_x.get_available_ways(wt_road, st_flat)[0]
	ASSERT_TRUE(desc != null)

	local w = settings.get_map_side_w()
	local h = settings.get_map_side_h()
	local d = settings.get_map_side_d()
	local t = d - 1
	local size = world.get_size()

	// near cut: (t, 0) is on-map, its NW neighbour (t-1, 0) is not;
	// far cut: (size.x-1, size.y-1-t) is on-map, its S neighbour is not
	foreach (pair in [ [[t, 0], [t - 1, 0]], [[size.x - 1, size.y - 1 - t], [size.x - 1, size.y - t]] ]) {
		local from = pair[0]
		local to = pair[1]
		ASSERT_TRUE(map_shape_contains(w, h, d, from[0], from[1]))
		ASSERT_FALSE(map_shape_contains(w, h, d, to[0], to[1]))

		local z = square_x(from[0], from[1]).get_ground_tile().z
		local err = command_x.build_way(pl, coord3d(from[0], from[1], z), coord3d(to[0], to[1], z), desc, true)
		ASSERT_TRUE(err != null)
		ASSERT_EQUAL(tile_x(from[0], from[1], z).get_way_dirs(wt_road), 0)
	}

	RESET_ALL_PLAYER_FUNDS()
}


// The raise and lower tools refuse a grid point whose tile is in a cut
// corner (inside the grid limits, but without ground) instead of reading
// its missing ground.  With D = 1 nothing is cut.
function test_map_shape_no_terraform_in_cut()
{
	local pl = player_x(0)
	local w = settings.get_map_side_w()
	local h = settings.get_map_side_h()
	local d = settings.get_map_side_d()
	local size = world.get_size()
	if (d == 1) {
		return
	}

	foreach (c in [ [0, 0], [size.x - 1, size.y - 1] ]) {
		ASSERT_FALSE(map_shape_contains(w, h, d, c[0], c[1]))
		ASSERT_TRUE(command_x.grid_raise(pl, coord3d(c[0], c[1], 0)) != null)
		ASSERT_TRUE(command_x.grid_lower(pl, coord3d(c[0], c[1], 0)) != null)
	}

	RESET_ALL_PLAYER_FUNDS()
}


// Terraforming a tile redraws its SE, S and SW neighbours where they
// are on the map.  (0, t) has no SW neighbour, the last tile of the far
// cut line (or the map's last tile, for D = 1) no S and SE neighbour.
function test_map_shape_terraform_at_edge()
{
	local pl = player_x(0)
	local w = settings.get_map_side_w()
	local h = settings.get_map_side_h()
	local d = settings.get_map_side_d()
	local t = d - 1
	local size = world.get_size()

	foreach (c in [ [0, t], [size.x - 1, size.y - 1 - t] ]) {
		ASSERT_TRUE(map_shape_contains(w, h, d, c[0], c[1]))
		local z = square_x(c[0], c[1]).get_ground_tile().z
		ASSERT_EQUAL(command_x.grid_raise(pl, coord3d(c[0], c[1], z)), null)
		ASSERT_EQUAL(command_x.grid_lower(pl, coord3d(c[0], c[1], z + 1)), null)
		ASSERT_EQUAL(square_x(c[0], c[1]).get_ground_tile().get_slope(), slope.flat)
	}

	RESET_ALL_PLAYER_FUNDS()
}


// The enlarge-map dialog opens on this map, a rhombus or a hexagon, and
// draws its preview of the enlarged one.  Nothing here can look at the
// picture: what it finds is a crash.
function test_map_shape_enlarge_dialog()
{
	ASSERT_TRUE(debug.enlarge_dialog(true))
	sleep()
	sleep()
	ASSERT_FALSE(debug.enlarge_dialog(false))
}


//
// Enlarging: W and H grow, D stays (karte_t::enlarge_map).  The old
// hexagon keeps its tiles, so the new ones are the new hexagon minus the old.
//
// These two tests change the map under every test after them, and the
// scenario runner does not reset the world between tests: they are the
// last in all_tests.nut.  debug.enlarge_map returns the shift that moved
// the script's coordinates of the old tiles, nonzero only on a map turned
// by 180 degrees (tools/test.py --rotate), where the new part lies at the
// script's near sides.
//


function enlarge_city_count()
{
	local n = 0
	foreach (city in city_list_x()) {
		n++
	}
	return n
}


// the E, SE, SW, W, NW, NE corner heights of a tile
function enlarge_corner_heights(t)
{
	local s = t.get_slope()
	local hs = []
	for (local c = 0; c < 6; c++) {
		hs.append(t.z + ((s >> (2*c)) & 3))
	}
	return hs
}


// koord::neighbours: SE, S, SW, NW, N, NE
ENLARGE_DIRS_Q <- [1, 0, -1, -1, 0, 1]
ENLARGE_DIRS_R <- [0, 1, 1, 0, -1, -1]


// What the script sees of the on-map tiles of the shape (w, h, d), keyed
// "x,y"; with `ground_only` just the height and slope of the ground tile,
// not its climate, water or what stands on it.
function enlarge_snapshot(w, h, d, ground_only)
{
	local snap = {}
	local size = world.get_size()
	for (local y = 0; y < size.y; y++) {
		for (local x = 0; x < size.x; x++) {
			if (!map_shape_contains(w, h, d, x, y)) {
				continue
			}
			local sq = square_x(x, y)
			local g = sq.get_ground_tile()
			local line = "z=" + g.z + " slope=" + g.get_slope()
			if (!ground_only) {
				line = "climate=" + sq.get_climate()
				for (local z = g.z - 4; z <= g.z + 4; z++) {
					local t = sq.get_tile_at_height(z)
					if (t != null) {
						line += " [" + rotate_snapshot_tile(t) + "]"
					}
				}
			}
			snap[x + "," + y] <- line
		}
	}
	return snap
}


// Every pair of neighbouring land tiles agrees on the heights of the two
// corners they share: the terrain is one surface across the old and the
// generated part.  Water tiles report the water surface, not the bottom.
function ASSERT_ENLARGE_SURFACE_CONTINUOUS(w, h, d)
{
	local size = world.get_size()
	for (local y = 0; y < size.y; y++) {
		for (local x = 0; x < size.x; x++) {
			if (!map_shape_contains(w, h, d, x, y)) {
				continue
			}
			local a = square_x(x, y).get_ground_tile()
			if (a.is_water()) {
				continue
			}
			local ha = enlarge_corner_heights(a)
			// the SE, S and SW neighbours: every pair once
			foreach (dir in [0, 1, 2]) {
				local nx = x + ENLARGE_DIRS_Q[dir]
				local ny = y + ENLARGE_DIRS_R[dir]
				if (!map_shape_contains(w, h, d, nx, ny)) {
					continue
				}
				local b = square_x(nx, ny).get_ground_tile()
				if (b.is_water()) {
					continue
				}
				local hb = enlarge_corner_heights(b)
				// corner dir of a is corner dir+4 of b, corner dir+1 is dir+3
				ASSERT_EQUAL(ha[dir], hb[(dir + 4) % 6])
				ASSERT_EQUAL(ha[(dir + 1) % 6], hb[(dir + 3) % 6])
			}
		}
	}
}


// Something to carry across the seam: a hill with its six neighbours
// farthest east, farthest south and farthest along x+y (the far cut), and
// a road along the last row.  Every hill has all its neighbours on the map.
function enlarge_prepare(pl, w, h, d)
{
	local size = world.get_size()
	local best = [null, null, null]
	local score = [-1000000, -1000000, -1000000]
	for (local y = 1; y < size.y - 1; y++) {
		for (local x = 1; x < size.x - 1; x++) {
			local inside = map_shape_contains(w, h, d, x, y)
			for (local dir = 0; inside  &&  dir < 6; dir++) {
				inside = map_shape_contains(w, h, d, x + ENLARGE_DIRS_Q[dir], y + ENLARGE_DIRS_R[dir])
			}
			if (!inside) {
				continue
			}
			// the middle row on ties for the east, the nearest column for the south
			local cand = [x*1000 - abs(2*y - size.y), y*1000 - x, (x + y)*1000 - abs(x - y)]
			foreach (i, c in cand) {
				if (c > score[i]) {
					score[i] = c
					best[i] = [x, y]
				}
			}
		}
	}
	local hills = []
	foreach (p in best) {
		local seen = false
		foreach (q in hills) {
			seen = seen  ||  (q[0] == p[0]  &&  q[1] == p[1])
		}
		if (!seen) {
			hills.append(p)
		}
	}
	foreach (p in hills) {
		raise_hex_tile(pl, p[0], p[1], square_x(p[0], p[1]).get_ground_tile().z)
	}

	// a road along the last row of the map, on four flat tiles
	local y = size.y - 1
	local road = way_desc_x.get_available_ways(wt_road, st_flat)[0]
	for (local x0 = 0; x0 + 3 < size.x; x0++) {
		local flat = true
		for (local x = x0; x <= x0 + 3; x++) {
			flat = flat  &&  map_shape_contains(w, h, d, x, y)  &&  square_x(x, y).get_ground_tile().get_slope() == slope.flat
		}
		if (flat) {
			local z = square_x(x0, y).get_ground_tile().z
			ASSERT_EQUAL(command_x.build_way(pl, coord3d(x0, y, z), coord3d(x0 + 3, y, z), road, true), null)
			return
		}
	}
	ASSERT_TRUE(false)  // no room for the road
}


// Grows the map to (w2, h2) and checks the new hexagon: the extents and
// the sides, ground on every on-map cell and none off the map, the tile
// count.  Returns the shift of the script's coordinates.
function enlarge_and_check(w, h, d, w2, h2, cities)
{
	local cities_before = enlarge_city_count()

	local shift = debug.enlarge_map(w2, h2, cities, 160)
	ASSERT_TRUE(shift != null)

	ASSERT_EQUAL(settings.get_map_side_w(), w2)
	ASSERT_EQUAL(settings.get_map_side_h(), h2)
	ASSERT_EQUAL(settings.get_map_side_d(), d)
	local size = world.get_size()
	ASSERT_EQUAL(size.x, w2 + d - 1)
	ASSERT_EQUAL(size.y, h2 + d - 1)

	// the old hexagon is a part of the new one, moved by the shift
	local count = 0
	local new_count = 0
	for (local y = -1; y <= size.y; y++) {
		for (local x = -1; x <= size.x; x++) {
			local on_map = map_shape_contains(w2, h2, d, x, y)
			ASSERT_EQUAL(map_shape_has_ground(x, y), on_map)
			if (on_map) {
				count++
				if (!map_shape_contains(w, h, d, x - shift.x, y - shift.y)) {
					new_count++
				}
			}
		}
	}
	ASSERT_EQUAL(count, w2*h2 + h2*d + d*w2 - w2 - h2 - d + 1)
	ASSERT_EQUAL(new_count, count - (w*h + h*d + d*w - w - h - d + 1))
	ASSERT_TRUE(enlarge_city_count() >= cities_before)
	return shift
}


// The old tiles are as the snapshot `before` has them, except where
// `spared(x, y)` (of the new coordinates) is true.
function ASSERT_ENLARGE_KEPT(before, w2, h2, d, shift, ground_only, spared)
{
	local after = enlarge_snapshot(w2, h2, d, ground_only)
	foreach (key, line in before) {
		local xy = split(key, ",")
		local x = xy[0].tointeger() + shift.x
		local y = xy[1].tointeger() + shift.y
		ASSERT_TRUE((x + "," + y) in after)
		if (spared == null  ||  !spared(x, y)) {
			ASSERT_EQUAL(after[x + "," + y], line)
		}
	}
}


// Two enlargements of the current map: the first grows both sides, the
// second only W.  The new part has mountains, but no cities.  The first
// leaves dry land alone but for the vertices it shares with the new part;
// the second finds water in the old part, and an enlargement beaches the
// water tiles within 19 of the far sides, so it keeps the old ground only.
function test_map_shape_enlarge()
{
	local pl = player_x(0)
	local w = settings.get_map_side_w()
	local h = settings.get_map_side_h()
	local d = settings.get_map_side_d()

	enlarge_prepare(pl, w, h, d)
	ASSERT_ENLARGE_SURFACE_CONTINUOUS(w, h, d)

	local before = enlarge_snapshot(w, h, d, false)
	local shift = enlarge_and_check(w, h, d, w + 4, h + 3, 0)
	ASSERT_ENLARGE_KEPT(before, w + 4, h + 3, d, shift, false, null)
	ASSERT_ENLARGE_SURFACE_CONTINUOUS(w + 4, h + 3, d)

	before = enlarge_snapshot(w + 4, h + 3, d, true)
	shift = enlarge_and_check(w + 4, h + 3, d, w + 7, h + 3, 0)
	ASSERT_ENLARGE_KEPT(before, w + 7, h + 3, d, shift, true, null)
	ASSERT_ENLARGE_SURFACE_CONTINUOUS(w + 7, h + 3, d)

	// shrinking is refused, not done
	ASSERT_EQUAL(debug.enlarge_map(w, h, 0, -1), null)
	ASSERT_EQUAL(settings.get_map_side_w(), w + 7)
}


// Cities of an enlargement stand in the new part.  Their houses and roads
// spread over the old tiles next to it and level them, so only the ground
// of the old tiles away from every new city is compared.
function test_map_shape_enlarge_cities()
{
	local w = settings.get_map_side_w()
	local h = settings.get_map_side_h()
	local d = settings.get_map_side_d()

	local old_cities = {}
	foreach (city in city_list_x()) {
		old_cities[city.get_name()] <- true
	}

	local before = enlarge_snapshot(w, h, d, true)
	local shift = enlarge_and_check(w, h, d, w + 6, h + 6, 3)

	// The position of a city moves as it grows, and its houses cross the
	// seam, so a new city is one whose area (and not its position) has a
	// new tile in it.
	local boxes = []
	foreach (city in city_list_x()) {
		if (!(city.get_name() in old_cities)) {
			local p = city.get_pos()
			ASSERT_TRUE(map_shape_contains(w + 6, h + 6, d, p.x, p.y))
			local nw = city.get_pos_nw()
			local se = city.get_pos_se()
			local box = [min(nw.x, se.x) - 3, min(nw.y, se.y) - 3, max(nw.x, se.x) + 3, max(nw.y, se.y) + 3]
			local touches_new = false
			for (local y = box[1]; y <= box[3]; y++) {
				for (local x = box[0]; x <= box[2]; x++) {
					touches_new = touches_new  ||  (map_shape_contains(w + 6, h + 6, d, x, y)  &&  !map_shape_contains(w, h, d, x - shift.x, y - shift.y))
				}
			}
			ASSERT_TRUE(touches_new)
			boxes.append(box)
		}
	}
	ASSERT_ENLARGE_KEPT(before, w + 6, h + 6, d, shift, true, function(x, y) {
		foreach (b in boxes) {
			if (x >= b[0]  &&  y >= b[1]  &&  x <= b[2]  &&  y <= b[3]) {
				return true
			}
		}
		return false
	})
}
