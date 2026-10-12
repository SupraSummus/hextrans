//
// This file is part of the Simutrans project under the Artistic License.
// (see LICENSE.txt)
//


//
// Tests for the back walls of a tile.
//
// A tile draws walls against the tiles behind it, its NW, N and NE
// neighbours (grund_t::calc_back_image).  When a tile changes height,
// slope or building, the tiles to its SE, S and SW have to be redrawn
// (grund_t::recalc_back_wall_dependants), or they keep a wall to a tile
// that is no longer there, or miss one.  tile_x::get_back_walls gives
// the walls the engine computed last and needs no images.
//
// Plain terrain never has walls: a vertex has one height for all three
// tiles that share it, so tiles meet edge to edge.  Walls come from
// artificial heights, a foundation levelled up or a slope set by hand,
// which these tests use to get a cliff next to flat ground.
//


// The wall of tile `tile` against its NW (wall 0), N (1) or NE (2)
// neighbour: 0 for none, else the wall's image index.  The walls are
// the base 11 digits of the state; a fence is coded from 11^3 up and
// is no wall.
function back_wall_digit(tile, wall)
{
	local code = tile.get_back_walls()
	if (code < 0) {
		code = -code
	}
	if (code >= 1331) {
		return 0
	}
	for (local i = 0; i < wall; i++) {
		code = code / 11
	}
	return code % 11
}


// get_back_image_from_diff in grund.cc: the wall image for the heights
// of the two corners of the tile behind above the corners of this one.
function wall_digit_from_diff(h1, h2)
{
	local min_diff = h1 < h2 ? h1 : h2
	while (min_diff > 2  ||  (min_diff > 0  &&  h1 != h2)) {
		local step = min_diff > 1 ? 2 : 1
		h1 -= step
		h2 -= step
		min_diff -= 2
	}
	if (h1 * h2 < 0) {
		return h1 < 0 ? 9 : 10
	}
	return (h1 > 0 ? h1 : 0) + (h2 > 0 ? h2 : 0) * 3
}


function slope_corner(s, corner)
{
	return (s >> (2 * corner)) & 3
}


// The tiles that draw a wall against tile (q, r): its SE, S and SW
// neighbours.  Each has the wall that faces (q, r), its own two corners
// `ta` and `tb` on the shared edge and the corners `a` and `b` of the
// tile (q, r) at the same two points, in the order calc_back_image
// reads them (back_wall_geometry in grund.cc).
back_wall_fronts <- [
	{ name = "SE", dq =  1, dr = 0, wall = 0, ta = hex_corner.W,  tb = hex_corner.NW, a = hex_corner.SE, b = hex_corner.E  },
	{ name = "S",  dq =  0, dr = 1, wall = 1, ta = hex_corner.NW, tb = hex_corner.NE, a = hex_corner.SW, b = hex_corner.SE },
	{ name = "SW", dq = -1, dr = 1, wall = 2, ta = hex_corner.NE, tb = hex_corner.E,  a = hex_corner.W,  b = hex_corner.SW }
]


// The wall that `front` draws against `tile` behind it: calc_back_image
// restated over the heights and slopes of the two tiles.
function expected_back_wall(front, tile, f)
{
	local this_a = slope_corner(front.get_slope(), f.ta)
	local this_b = slope_corner(front.get_slope(), f.tb)
	local back_height = this_a < this_b ? this_a : this_b
	local left = tile.z - back_height
	local diff_a = left + slope_corner(tile.get_slope(), f.a) - front.z
	local diff_b = left + slope_corner(tile.get_slope(), f.b) - front.z
	if ((diff_a - (this_a - back_height) > 0  ||  diff_b - (this_b - back_height) > 0)
			&&  (diff_a > 0  ||  diff_b > 0)) {
		return wall_digit_from_diff(diff_a, diff_b)
	}
	return 0
}


// Check the walls that the SE, S and SW neighbours of (q, r) draw
// against it.  With `visible` all three have to be real walls, so that
// a neighbour left without a redraw cannot pass by having no wall
// either way.
function assert_front_walls(q, r, visible)
{
	local tile = square_x(q, r).get_ground_tile()
	foreach (f in back_wall_fronts) {
		local front = square_x(q + f.dq, r + f.dr).get_ground_tile()
		local expected = expected_back_wall(front, tile, f)
		if (visible) {
			ASSERT_TRUE(expected > 0)
		}

		local actual = back_wall_digit(front, f.wall)
		if (actual != expected) {
			throw "wall of the " + f.name + " neighbour of (" + q + ", " + r + "): "
				+ actual + " instead of " + expected
		}
	}
}


// A building on a slope is levelled up to the high side, which puts a
// wall on the tiles in front of it; removing it takes the walls away.
function test_back_wall_house_on_slope()
{
	local pl = player_x(0)
	local public_pl = player_x(1)

	// a house needs a city
	ASSERT_EQUAL(command_x(tool_add_city).work(public_pl, coord3d(12, 12, 0), "0"), null)

	// one raised corner on (6, 6): its foundation is flat at height 1
	ASSERT_EQUAL(command_x.grid_raise(pl, coord3d(6, 6, 0)), null)
	assert_front_walls(6, 6, false)

	ASSERT_EQUAL(command_x(tool_build_house).work(public_pl, coord3d(6, 6, 0), "10Lighthouse"), null)
	ASSERT_EQUAL(square_x(6, 6).get_ground_tile().z, 1)
	assert_front_walls(6, 6, true)

	ASSERT_EQUAL(command_x(tool_remover).work(public_pl, coord3d(6, 6, 1)), null)
	ASSERT_EQUAL(square_x(6, 6).get_ground_tile().z, 0)
	assert_front_walls(6, 6, false)

	ASSERT_EQUAL(command_x.grid_lower(pl, coord3d(6, 6, 1)), null)

	// clean up
	ASSERT_EQUAL(command_x(tool_remover).work(public_pl, coord3d(12, 12, 0)), null); // remove city
	clear_seed_roads(public_pl, 12, 12)
	RESET_ALL_PLAYER_FUNDS()
}


// The same for a house a city builds, which takes another path.
function test_back_wall_city_house_on_slope()
{
	local pl = player_x(0)
	local public_pl = player_x(1)

	ASSERT_EQUAL(command_x(tool_add_city).work(public_pl, coord3d(12, 12, 0), "0"), null)
	ASSERT_EQUAL(command_x.grid_raise(pl, coord3d(6, 6, 0)), null)

	ASSERT_EQUAL(command_x(tool_build_house).work(public_pl, coord3d(6, 6, 0), "10RES_01_23"), null)
	ASSERT_EQUAL(square_x(6, 6).get_ground_tile().z, 1)
	assert_front_walls(6, 6, true)

	ASSERT_EQUAL(command_x(tool_remover).work(public_pl, coord3d(6, 6, 1)), null)
	assert_front_walls(6, 6, false)

	ASSERT_EQUAL(command_x.grid_lower(pl, coord3d(6, 6, 1)), null)

	ASSERT_EQUAL(command_x(tool_remover).work(public_pl, coord3d(12, 12, 0)), null); // remove city
	clear_seed_roads(public_pl, 12, 12)
	RESET_ALL_PLAYER_FUNDS()
}


// A tile set one level up stands out of the ground with a wall on each
// of its three front sides; set down again the walls are gone.
function test_back_wall_set_slope()
{
	local pl = player_x(0)

	ASSERT_EQUAL(command_x.set_slope(pl, coord3d(6, 6, 0), slope.all_up_slope), null)
	ASSERT_EQUAL(square_x(6, 6).get_ground_tile().z, 1)
	assert_front_walls(6, 6, true)

	ASSERT_EQUAL(command_x.set_slope(pl, coord3d(6, 6, 1), slope.all_down_slope), null)
	ASSERT_EQUAL(square_x(6, 6).get_ground_tile().z, 0)
	assert_front_walls(6, 6, false)

	RESET_ALL_PLAYER_FUNDS()
}


// A road across a double slope levels it, which changes the tile's
// corners on its front edges.  The road runs from N to S through (4, 4),
// so its SW neighbour is not on the route and is redrawn only as a
// neighbour.  The slope is pinned only as the input, the walls afterwards
// follow from the slope the way builder leaves.
function test_back_wall_road_terraform()
{
	local pl        = player_x(0)
	local road_desc = way_desc_x.get_available_ways(wt_road, st_flat)[0]
	local remover   = command_x(tool_remove_way)

	ASSERT_EQUAL(command_x.set_slope(pl, coord3d(4, 4, 0), slope.northeast_double), null)
	assert_front_walls(4, 4, true)
	local before = square_x(3, 5).get_ground_tile().get_back_walls()

	ASSERT_EQUAL(command_x.build_way(pl, coord3d(4, 3, 0), coord3d(4, 5, 0), road_desc, true), "Slope is too steep")
	ASSERT_EQUAL(command_x.build_way(pl, coord3d(4, 3, 0), coord3d(4, 5, 0), road_desc, true, true), null)
	ASSERT_TRUE(square_x(4, 4).get_ground_tile().get_slope() != slope.northeast_double)
	assert_front_walls(4, 4, false)
	// the SW neighbour in particular has another wall than before
	ASSERT_TRUE(square_x(3, 5).get_ground_tile().get_back_walls() != before)

	ASSERT_EQUAL(remover.work(pl, coord3d(4, 3, 0), coord3d(4, 5, 0), "" + wt_road), null)
	ASSERT_EQUAL(command_x.set_slope(pl, coord3d(4, 4, 0), slope.all_down_slope), null)
	RESET_ALL_PLAYER_FUNDS()
}
