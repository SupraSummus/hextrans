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


function test_city_population_ledger()
{
	local pl = player_x(1)

	// recompute the city population ledger from the map: every tile of a
	// residential city building houses level*10 citizens, every tile of a
	// commercial/industrial one provides level*20 jobs.
	// stadt_t::build_city_house() maintains the same numbers incrementally.
	local check_ledger = function(city, where) {
		local sum_res = 0
		local sum_jobs = 0
		for (local x = 0; x < 16; x++) {
			for (local y = 0; y < 16; y++) {
				local b = square_x(x, y).get_ground_tile().find_object(mo_building)
				if (b == null) {
					continue
				}
				local d = b.get_desc()
				local t = d.get_type()
				if (t == building_desc_x.city_res) {
					sum_res += d.get_level()
				}
				else if (t == building_desc_x.city_com || t == building_desc_x.city_ind) {
					sum_jobs += d.get_level()
				}
			}
		}
		ASSERT_EQUAL("housing " + where + ": " + city.get_housing(), "housing " + where + ": " + (sum_res * 10))
		ASSERT_EQUAL("jobs " + where + ": " + city.get_jobs(), "jobs " + where + ": " + (sum_jobs * 20))
	}

	ASSERT_EQUAL(command_x(tool_add_city).work(pl, coord3d(8, 8, 0), "0"), null)
	local city = city_x(8, 8)
	check_ledger(city, "after founding")

	// plant the test-only 2x2 residential building; this also goes through
	// stadt_t::build_city_house and must keep the ledger consistent
	ASSERT_EQUAL(command_x(tool_build_house).work(pl, coord3d(4, 8, 0), "1ATEST_RES_2x2"), null)
	check_ledger(city, "after planting 2x2")

	// grow the city until renovation replaces a TEST_RES_2x2 by a smaller
	// building.  TEST_RES_2x2 is the only multi-tile city building
	// available, so a renovation of it necessarily goes through the
	// leftover-tile conversion in build_city_house.
	local count_2x2_tiles = function() {
		local n = 0
		for (local x = 0; x < 16; x++) {
			for (local y = 0; y < 16; y++) {
				local b = square_x(x, y).get_ground_tile().find_object(mo_building)
				if (b != null && b.get_desc().get_name() == "TEST_RES_2x2") {
					n++
				}
			}
		}
		return n
	}

	local shrunk = false
	local seen_max = count_2x2_tiles()
	for (local i = 0; i < 300 && !shrunk; i++) {
		city.change_size(100)
		local n = count_2x2_tiles()
		if (n < seen_max) {
			// one of the 2x2 buildings was replaced by something smaller
			shrunk = true
		}
		else if (n > seen_max) {
			seen_max = n
		}
	}
	ASSERT_TRUE(shrunk) // if this fails, growth never renovated a 2x2; test setup issue, not a ledger bug

	check_ledger(city, "after renovation")

	// clean up: removing the townhall removes the city and all its buildings
	local cp = city.get_pos()
	local th_tile = square_x(cp.x, cp.y).get_ground_tile()
	ASSERT_EQUAL(command_x(tool_remover).work(pl, coord3d(cp.x, cp.y, th_tile.z)), null)
	for (local x = 0; x < 16; x++) {
		for (local y = 0; y < 16; y++) {
			local tile = square_x(x, y).get_ground_tile()
			if (tile.get_way(wt_road) != null) {
				local p = coord3d(x, y, tile.z)
				command_x(tool_remove_way).work(pl, p, p, "" + wt_road)
			}
		}
	}
	RESET_ALL_PLAYER_FUNDS()
}
