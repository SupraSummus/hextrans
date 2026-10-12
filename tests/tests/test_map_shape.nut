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
