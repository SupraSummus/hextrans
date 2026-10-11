//
// This file is part of the Simutrans project under the Artistic License.
// (see LICENSE.txt)
//

//
// Tests for adding, removing and deleting cities
//


function test_city_add_invalid()
{
	ASSERT_EQUAL(command_x(tool_add_city).work(player_x(1), coord3d(-1, -1, 0)), "")
}


function test_city_add_cannot_afford()
{
	ASSERT_EQUAL(command_x(tool_add_city).work(player_x(0), coord3d(7, 8, 0)), "Insufficient funds!")

	// clean up
	RESET_ALL_PLAYER_FUNDS()
}


function test_city_add_by_public_player()
{
	{
		ASSERT_EQUAL(command_x(tool_add_city).work(player_x(1), coord3d(7, 8, 0)), null)
		assert_townhall_with_road(7, 8)
	}

	// clean up
	clear_townhall_and_roads(player_x(1), 7, 8)
	RESET_ALL_PLAYER_FUNDS()
}


function test_city_add_on_existing_townhall()
{
	ASSERT_EQUAL(command_x(tool_add_city).work(player_x(1), coord3d(7, 8, 0)), null)

	{
		ASSERT_EQUAL(command_x(tool_add_city).work(player_x(1), coord3d(7, 8, 0)), "Cities can only be built on flat empty ground!")
	}

	// clean up
	clear_townhall_and_roads(player_x(1), 7, 8)
	RESET_ALL_PLAYER_FUNDS()
}


function test_city_add_near_map_border()
{
	{
		ASSERT_EQUAL(command_x(tool_add_city).work(player_x(1), coord3d(0, 15, 0)), null)
		assert_townhall_with_road(0, 15)
	}

	// clean up
	clear_townhall_and_roads(player_x(1), 0, 15)
	RESET_ALL_PLAYER_FUNDS()
}


function test_city_change_size_invalid_params()
{
	local pl = player_x(0)

	// not a city
	{
		ASSERT_EQUAL(command_x(tool_change_city_size).work(pl, coord3d(1, 1, 0), "100"), "")
	}

	ASSERT_EQUAL(command_x(tool_add_city).work(player_x(1), coord3d(1, 1, 0)), null)

	// invalid pos
	{
		ASSERT_EQUAL(command_x(tool_change_city_size).work(pl, coord3d(-1,-1,-1), "100"), "")
	}

	// Null default_param
	{
		local error_caught = false
		try {
			ASSERT_EQUAL(command_x(tool_change_city_size).work(pl, coord3d(1, 1, 0), null), "")
		}
		catch (e) {
			error_caught = true
			ASSERT_EQUAL(e, "Error during initializing tool")
		}
		ASSERT_TRUE(error_caught)
	}

	// Empty default_param
	{
		local error_caught = false
		try {
			ASSERT_EQUAL(command_x(tool_change_city_size).work(pl, coord3d(1, 1, 0), ""), "")
		}
		catch (e) {
			error_caught = true
			ASSERT_EQUAL(e, "Error during initializing tool")
		}
		ASSERT_TRUE(error_caught)
	}

	// Wrong default_param
	{
		local error_caught = false
		try {
			ASSERT_EQUAL(command_x(tool_change_city_size).work(pl, coord3d(1, 1, 0), "bla"), "")
		}
		catch (e) {
			error_caught = true
			ASSERT_EQUAL(e, "Error during initializing tool")
		}
		ASSERT_TRUE(error_caught)
	}

	// clean up
	clear_townhall_and_roads(player_x(1), 1, 1)
	RESET_ALL_PLAYER_FUNDS()
}


// Exercises the stadt_t::~stadt_t() multi-tile-building path: build a
// city, grow it until check_bau_townhall picks a multi-tile townhall
// (pak64 lands on 04_CITY 2x2 around bev=20000), tear down every
// non-townhall building first to keep the destructor's iteration
// focused on the multi-tile townhall, then remove the townhall.
// Without the destructor fix in this commit, the cascade through
// gebaeude_t::cleanup -> stadt_t::remove_gebaeude_from_stadt asserts
// when it walks back to the already-popped head tile.
function test_city_remove_with_multitile_townhall()
{
	local pl = player_x(1)
	local townhall_pos = coord3d(8, 8, 0)

	ASSERT_EQUAL(command_x(tool_add_city).work(pl, townhall_pos, "0"), null)
	ASSERT_EQUAL(command_x(tool_change_city_size).work(player_x(0), townhall_pos, "20000"), null)

	// Tear down every non-townhall building.  Capture the townhall's
	// position on the way through.
	local size = world.get_size()
	local th_corner = null
	for (local x = 0; x < size.x; x++) {
		for (local y = 0; y < size.y; y++) {
			local b = tile_x(x, y, 0).find_object(mo_building)
			if (b == null) continue
			if (b.is_townhall()) {
				if (th_corner == null) th_corner = coord3d(x, y, 0)
				continue
			}
			command_x(tool_remover).work(pl, coord3d(x, y, 0))
		}
	}

	// Fail loudly if the pakset never upgraded the townhall to multi-
	// tile; otherwise the test would silently pass without exercising
	// the destructor path we care about.
	local th_size = tile_x(th_corner.x, th_corner.y, 0).find_object(mo_building).get_desc().get_size(0)
	ASSERT_TRUE(th_size.x > 1 || th_size.y > 1)

	ASSERT_EQUAL(command_x(tool_remover).work(pl, th_corner), null)

	// ~stadt_t() tears down everything in stadt::buildings (the
	// townhall, plus monuments/attractions added via
	// add_gebaeude_to_stadt) but not the auto-generated road network;
	// sweep what's left so RESET_ALL_PLAYER_FUNDS sees zero
	// maintenance.
	for (local x = 0; x < size.x; x++) {
		for (local y = 0; y < size.y; y++) {
			local tile = tile_x(x, y, 0)
			if (tile.get_way(wt_road) != null) {
				local p = coord3d(x, y, 0)
				command_x(tool_remove_way).work(pl, p, p, "" + wt_road)
			}
		}
	}

	RESET_ALL_PLAYER_FUNDS()
}


function test_city_change_size_to_minimum()
{
	ASSERT_EQUAL(command_x(tool_add_city).work(player_x(1), coord3d(1, 1, 0)), null)
	ASSERT_EQUAL(city_x(1, 1).get_citizens()[0], 126)

	{
		ASSERT_EQUAL(command_x(tool_change_city_size).work(player_x(0),  coord3d(1, 1, 0), "-100"), null)
		ASSERT_EQUAL(city_x(1, 1).get_citizens()[0], 126)
	}

	// clean up
	clear_townhall_and_roads(player_x(1), 1, 1)
	RESET_ALL_PLAYER_FUNDS()
}


// A 1x1 townhall seeds its road on one neighbour tile.  With every
// neighbour but `seed` flooded the seed lands on `seed`, and on the
// townhall's NE or SW face that goes through stadt_t::build_road, which
// decides what the new city road joins.  `seed` touches a stop or depot on
// a dead end, built from a road that leaves along `road_to`.
//   kind      "depot", or the name of a stop
//   road_to   direction of the building's road
//   seed_dir  direction from the building to the seed tile
//   town_dir  direction from the seed tile to the townhall (NE or SW)
//   joins     whether the seed road is to connect to the building
function check_city_seed_next_to_building(kind, road_to, seed_dir, town_dir, joins)
{
	local pl = player_x(0)
	local public_pl = player_x(1)
	local road = get_slow_and_fast_road()[0]
	local site = coord3d(8, 8, 0)
	local out = site + dir.to_coord(road_to)
	local seed = site + dir.to_coord(seed_dir)
	local townhall = seed + dir.to_coord(town_dir)

	ASSERT_EQUAL(command_x.build_way(pl, site, out, road, true), null)
	if (kind == "depot") {
		ASSERT_EQUAL(command_x.build_depot(pl, site, get_depot_by_wt(wt_road)), null)
	}
	else {
		ASSERT_EQUAL(command_x(tool_build_station).work(pl, site, kind), null)
	}
	ASSERT_EQUAL(way_dirs_at(site, wt_road), road_to)

	local flooded = []
	foreach (d in dir.nsew) {
		local c = townhall + dir.to_coord(d)
		if (c.x == seed.x && c.y == seed.y) continue
		ASSERT_EQUAL(command_x(tool_set_climate).work(pl, c, c, "" + cl_water), null)
		flooded.append(c)
	}

	ASSERT_EQUAL(command_x(tool_add_city).work(public_pl, townhall, "0"), null)

	ASSERT_TRUE(tile_x(seed.x, seed.y, seed.z).get_way(wt_road) != null)
	if (joins) {
		ASSERT_EQUAL(way_dirs_at(site, wt_road), road_to | seed_dir)
		ASSERT_EQUAL(way_dirs_at(seed, wt_road), dir.backward(seed_dir))
	}
	else {
		ASSERT_EQUAL(way_dirs_at(site, wt_road), road_to)
		ASSERT_EQUAL(way_dirs_at(seed, wt_road), 0)
	}

	// clean up
	clear_townhall_and_roads(public_pl, townhall.x, townhall.y)
	foreach (c in flooded) {
		ASSERT_EQUAL(command_x(tool_set_climate).work(pl, c, c, "" + cl_mediterran), null)
	}
	ASSERT_EQUAL(command_x(tool_remover).work(pl, site), null)
	ASSERT_EQUAL(command_x(tool_remove_way).work(pl, site, out, "" + wt_road), null)
}


// building_desc_t::adjust_layout folds a NE terminal stop onto layout 0
// (S) and a SW one onto layout 1 (SE), so the stored layout names the
// wrong side.  The city road has to go by the way: it must not bend the
// terminal from the folded side.  A through stop on a NE-SW dead end takes
// the city road at its open end, and not across the platform.
function test_city_road_seed_next_to_stop()
{
	check_city_seed_next_to_building("CarStop", dir.northeast, dir.south,     dir.southwest, false)
	check_city_seed_next_to_building("CarStop", dir.southwest, dir.southeast, dir.northeast, false)
	check_city_seed_next_to_building("BusStop", dir.southwest, dir.northeast, dir.northeast, true)
	check_city_seed_next_to_building("BusStop", dir.southwest, dir.southeast, dir.northeast, false)

	RESET_ALL_PLAYER_FUNDS()
}


function test_city_road_seed_next_to_depot()
{
	check_city_seed_next_to_building("depot", dir.northeast, dir.south,     dir.southwest, false)
	check_city_seed_next_to_building("depot", dir.southwest, dir.southeast, dir.northeast, false)

	RESET_ALL_PLAYER_FUNDS()
}
