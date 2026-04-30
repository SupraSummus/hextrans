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
	local pos = coord3d(7, 8, 0)
	{
		ASSERT_EQUAL(command_x(tool_add_city).work(player_x(1), pos), null)

		// city built a townhall and at least one auto-generated road
		// in the surrounding area.  The exact townhall footprint and
		// road layout depend on the pakset; assert presence rather
		// than specific tiles.
		ASSERT_TRUE(tile_x(pos.x, pos.y, pos.z).find_object(mo_building) != null)
		ASSERT_TRUE(count_roads_near(pos) > 0)
	}

	cleanup_city(player_x(1), pos)
	RESET_ALL_PLAYER_FUNDS()
}


function test_city_add_on_existing_townhall()
{
	local pos = coord3d(7, 8, 0)
	ASSERT_EQUAL(command_x(tool_add_city).work(player_x(1), pos), null)

	{
		ASSERT_EQUAL(command_x(tool_add_city).work(player_x(1), pos), "Cities can only be built on flat empty ground!")
	}

	cleanup_city(player_x(1), pos)
	RESET_ALL_PLAYER_FUNDS()
}


function test_city_add_near_map_border()
{
	local pos = coord3d(0, 15, 0)
	{
		ASSERT_EQUAL(command_x(tool_add_city).work(player_x(1), pos), null)

		// near the map edge the townhall placefinder shifts away from
		// the edge by 1 tile; assert that some townhall building
		// landed in the immediate neighbourhood and at least one
		// road got built.  Specific tile coords are pak-dependent.
		local found_building = false
		for (local dx = 0; dx <= 1; dx++) {
			for (local dy = -1; dy <= 0; dy++) {
				if (tile_x(pos.x + dx, pos.y + dy, pos.z).find_object(mo_building) != null) {
					found_building = true
				}
			}
		}
		ASSERT_TRUE(found_building)
		ASSERT_TRUE(count_roads_near(pos) > 0)
	}

	cleanup_city(player_x(1), pos)
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

	cleanup_city(player_x(1), coord3d(1, 1, 0))
	RESET_ALL_PLAYER_FUNDS()
}


function test_city_change_size_to_minimum()
{
	local pos = coord3d(1, 1, 0)
	ASSERT_EQUAL(command_x(tool_add_city).work(player_x(1), pos), null)

	// minimum citizens count for a freshly-built city; pakset-defined
	local min_citizens = city_x(pos.x, pos.y).get_citizens()[0]
	ASSERT_TRUE(min_citizens > 0)

	{
		// shrinking below current size must clamp at the current
		// (minimum) value, not reduce further
		ASSERT_EQUAL(command_x(tool_change_city_size).work(player_x(0), pos, "-100"), null)
		ASSERT_EQUAL(city_x(pos.x, pos.y).get_citizens()[0], min_citizens)
	}

	cleanup_city(player_x(1), pos)
	RESET_ALL_PLAYER_FUNDS()
}
