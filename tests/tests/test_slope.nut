//
// This file is part of the Simutrans project under the Artistic License.
// (see LICENSE.txt)
//

//
// Test set_/can_set_slope
//

function test_slope_to_dir()
{
	// slope.to_dir(sl) returns the ribi that walks UP the slope — see
	// ribi_type(slope_t::type) in ribi.cc.  Each of the 6 hex axis edges
	// carries a ribi mapping shared by its narrow (2-corner) and wide
	// (4-corner) variants.  The 2 legacy square diagonals (slope.east /
	// slope.west) keep their projection onto the closest hex direction.
	// Flat, the 6 single-corner raises, and any other multi-corner
	// slope map to dir.none.
	local edges = {}
	edges[slope.north_narrow]   <- { dir = dir.south,     wide = slope.north_wide }
	edges[slope.south_narrow]   <- { dir = dir.north,     wide = slope.south_wide }
	edges[slope.northeast_narrow] <- { dir = dir.southwest, wide = slope.northeast_wide }
	edges[slope.southeast_narrow] <- { dir = dir.northwest, wide = slope.southeast_wide }
	edges[slope.southwest_narrow] <- { dir = dir.northeast, wide = slope.southwest_wide }
	edges[slope.northwest_narrow] <- { dir = dir.southeast, wide = slope.northwest_wide }

	local expected = {}
	foreach (sl, e in edges) {
		expected[sl]      <- e.dir
		expected[e.wide]  <- e.dir
	}
	expected[slope.east]     <- dir.northwest  // W corners raised → uphill = NW hex edge (legacy)
	expected[slope.west]     <- dir.southeast  // E corners raised → uphill = SE hex edge (legacy)

	foreach (sl, d in expected) {
		ASSERT_EQUAL(slope.to_dir(sl), d)
	}
	foreach (sl in interesting_slopes()) {
		if (sl in expected) continue
		ASSERT_EQUAL(slope.to_dir(sl), dir.none)
	}
}


function test_slope_can_set()
{
	local pl = player_x(0)
	local pos = coord3d(2, 3, 0)

	foreach (sl in interesting_slopes()) {
		command_x.set_slope(pl, pos, sl)
		ASSERT_EQUAL(tile_x(2, 3, 0).get_slope(), sl)
		RESET_ALL_PLAYER_FUNDS()

		ASSERT_EQUAL(command_x.can_set_slope(pl, pos, sl), "")

		local sq = square_x(2, 3)
		ASSERT_TRUE(sq != null && sq.is_valid())
		ASSERT_EQUAL(sq.get_climate(), cl_mediterran)

		local tile = sq.get_tile_at_height(0)
		ASSERT_TRUE(tile != null && tile.is_valid())
		ASSERT_EQUAL(tile.get_slope(), sl)

		ASSERT_EQUAL(pl.get_current_cash(),        200000)     // get_current_cash is in credits (returns float)
		ASSERT_EQUAL(pl.get_current_net_wealth(),  200000*100) // get_current_net_wealth is in 1/100 credits
	}

	// reset to normal slope
	command_x.set_slope(pl, pos + coord3d(0, 0, 1), slope.all_down_slope)
	command_x.set_slope(pl, pos,                    slope.all_down_slope)
	RESET_ALL_PLAYER_FUNDS()

	ASSERT_EQUAL(command_x.can_set_slope(pl, pos + coord3d(0, 0, 1), slope.all_up_slope), "")
	ASSERT_EQUAL(command_x.can_set_slope(pl, pos - coord3d(0, 0, 1), slope.all_down_slope), "")

	ASSERT_EQUAL(pl.get_current_cash(),        200000)     // get_current_cash is in credits (returns float)
	ASSERT_EQUAL(pl.get_current_net_wealth(),  200000*100) // get_current_net_wealth is in 1/100 credits

	RESET_ALL_PLAYER_FUNDS()
}


function test_slope_set_and_restore()
{
	local pl = player_x(0)
	local setslope = command_x.set_slope
	local restoreslope = command_x(tool_restoreslope)

	{
		ASSERT_EQUAL(setslope(pl, coord3d(2, 3, 0), slope.north_narrow), null)
		ASSERT_EQUAL(tile_x(2, 3, 0).get_slope(), slope.north_narrow)

		ASSERT_EQUAL(setslope(pl, coord3d(2, 3, 0), slope.south_narrow), null)
		ASSERT_EQUAL(tile_x(2, 3, 0).get_slope(), slope.south_narrow)

		ASSERT_EQUAL(restoreslope.work(pl, coord3d(2, 3, 0)), null)
		ASSERT_EQUAL(tile_x(2, 3, 0).get_slope(), slope.flat)
	}

	{
		ASSERT_EQUAL(setslope(pl, coord3d(2, 3, 0), slope.all_up_slope), null)
		ASSERT_TRUE(tile_x(2, 3, 0).is_valid())
		ASSERT_TRUE(tile_x(2, 3, 1).is_valid())
		ASSERT_EQUAL(tile_x(2, 3, 1).get_slope(), slope.flat)

		// fails as expected because ground is 1 unit higher
		ASSERT_EQUAL(setslope(pl, coord3d(2, 3, 0), slope.all_up_slope), "")

		// TODO check tile height
		ASSERT_EQUAL(restoreslope.work(pl, coord3d(2, 3, 0)), "")
		ASSERT_TRUE(tile_x(2, 3, 0).is_valid())
		ASSERT_EQUAL(tile_x(2, 3, 0).get_slope(), slope.flat)

		ASSERT_EQUAL(setslope(pl, coord3d(2, 3, 1), slope.all_down_slope), null)
		ASSERT_EQUAL(tile_x(2, 3, 1).get_slope(), slope.flat)
	}

	RESET_ALL_PLAYER_FUNDS()
}


function test_slope_set_near_map_border()
{
	local pl = player_x(0)
	local setslope = command_x.set_slope

	// map edge
	{
		for (local sl = 0; sl < slope.raised; ++sl) {
			ASSERT_EQUAL(setslope(pl, coord3d(0, 3, 0), sl), "Zu nah am Kartenrand")
		}
	}

	// map corner
	{
		for (local sl = 0; sl < slope.raised; ++sl) {
			ASSERT_EQUAL(setslope(pl, coord3d(0, 0, 0), sl), "Zu nah am Kartenrand")
		}
	}

	RESET_ALL_PLAYER_FUNDS()
}


function test_slope_max_height_diff()
{
	local pl = player_x(0)
	local setslope = command_x.set_slope

	// build upwards, height difference = 4
	{
		ASSERT_EQUAL(setslope(pl, coord3d(3, 2, 0), slope.all_up_slope), null)
		ASSERT_EQUAL(setslope(pl, coord3d(3, 2, 1), slope.all_up_slope), null)
		ASSERT_EQUAL(setslope(pl, coord3d(3, 2, 2), slope.all_up_slope), null)
		ASSERT_EQUAL(setslope(pl, coord3d(3, 2, 3), slope.all_up_slope), null)
		ASSERT_EQUAL(setslope(pl, coord3d(3, 2, 4), slope.all_up_slope), "Maximum tile height difference reached.")
	}

	// far-apart tiles have no max-diff constraint between them
	{
		ASSERT_EQUAL(setslope(pl, coord3d(6, 6,  0), slope.all_down_slope), null)
		ASSERT_EQUAL(setslope(pl, coord3d(6, 6, -1), slope.all_down_slope), null)
		ASSERT_EQUAL(setslope(pl, coord3d(6, 6, -2), slope.all_down_slope), null)
		ASSERT_EQUAL(setslope(pl, coord3d(6, 6, -3), slope.all_down_slope), null)
		ASSERT_EQUAL(setslope(pl, coord3d(6, 6, -4), slope.all_down_slope), "Maximum tile height difference reached.")
	}

	// and clean up
	ASSERT_EQUAL(setslope(pl, coord3d(3, 2, 4), slope.all_down_slope), null)
	ASSERT_EQUAL(setslope(pl, coord3d(3, 2, 3), slope.all_down_slope), null)
	ASSERT_EQUAL(setslope(pl, coord3d(3, 2, 2), slope.all_down_slope), null)
	ASSERT_EQUAL(setslope(pl, coord3d(3, 2, 1), slope.all_down_slope), null)

	ASSERT_EQUAL(setslope(pl, coord3d(6, 6, -4), slope.all_up_slope), null)
	ASSERT_EQUAL(setslope(pl, coord3d(6, 6, -3), slope.all_up_slope), null)
	ASSERT_EQUAL(setslope(pl, coord3d(6, 6, -2), slope.all_up_slope), null)
	ASSERT_EQUAL(setslope(pl, coord3d(6, 6, -1), slope.all_up_slope), null)

	RESET_ALL_PLAYER_FUNDS()
}


function test_slope_get_price()
{
	local pl = player_x(0)
	foreach (sl in interesting_slopes()) {
		ASSERT_EQUAL(command_x.slope_get_price(sl), 2000 * 100)
	}

	local restore_slope = 4099 // RESTORE_SLOPE from simconst.h
	ASSERT_EQUAL(command_x.slope_get_price(restore_slope), 1500 * 100)

	ASSERT_EQUAL(pl.get_current_cash(),        200000)     // get_current_cash is in credits (returns float)
	ASSERT_EQUAL(pl.get_current_net_wealth(),  200000*100) // get_current_net_wealth is in 1/100 credits
	RESET_ALL_PLAYER_FUNDS()
}


function test_slope_restore_on_foundation()
{
	local pl = player_x(0)

	ASSERT_EQUAL(command_x(tool_add_city).work(player_x(1), coord3d(8, 8, 0), "0"), null)
	ASSERT_EQUAL(command_x(tool_build_house).work(pl, coord3d(4, 2, 0), "11RES_01_23"), null)

	{
		ASSERT_EQUAL(command_x(tool_restoreslope).work(pl, coord3d(4, 2, 0)), "No suitable ground!")
	}

	// clean up
	ASSERT_EQUAL(command_x(tool_remover).work(player_x(1), coord3d(4, 2, 0)), null)
	ASSERT_EQUAL(command_x(tool_remover).work(player_x(1), coord3d(8, 8, 0)), null); // remove city
	ASSERT_EQUAL(command_x(tool_remove_way).work(player_x(1), coord3d(7, 9, 0), coord3d(9, 9, 0), "" + wt_road), null);
	RESET_ALL_PLAYER_FUNDS();
}


function test_slope_restore_on_bridge()
{
	local pl = player_x(0)
	local rail_bridge = bridge_desc_x.get_available_bridges(wt_rail)[0]

	ASSERT_TRUE(rail_bridge != null)

	ASSERT_EQUAL(command_x.set_slope(pl, coord3d(4, 2, 0), slope.south_narrow), null)
	ASSERT_EQUAL(command_x.set_slope(pl, coord3d(4, 4, 0), slope.north_narrow), null)
	ASSERT_EQUAL(command_x.build_bridge_at(pl, coord3d(4, 2, 0), rail_bridge), null)

	{
		ASSERT_EQUAL(command_x(tool_restoreslope).work(pl, coord3d(4, 2, 0)), "No suitable ground!")
	}

	// clean up
	ASSERT_EQUAL(command_x(tool_remover).work(pl, coord3d(4, 2, 0)), null)
	ASSERT_EQUAL(command_x.set_slope(pl, coord3d(4, 2, 0), slope.flat), null)
	ASSERT_EQUAL(command_x.set_slope(pl, coord3d(4, 4, 0), slope.flat), null)
	RESET_ALL_PLAYER_FUNDS()
}


function test_slope_restore_on_label()
{
	local pl = player_x(0)

	ASSERT_EQUAL(command_x.set_slope(pl, coord3d(4, 2, 0), slope.south_narrow), null)
	ASSERT_EQUAL(label_x.create(coord(4, 2), pl, "foo"), null)

	{
		ASSERT_EQUAL(command_x(tool_restoreslope).work(pl, coord3d(4, 2, 0)), "Tile not empty.")
	}

	// clean up
	ASSERT_EQUAL(command_x(tool_remover).work(pl, coord3d(4, 2, 0)), null)
	ASSERT_EQUAL(command_x.set_slope(pl, coord3d(4, 2, 0), slope.flat), null)
	RESET_ALL_PLAYER_FUNDS()
}


// Raising / lowering a corner propagates "≥ H − 1" / "≤ H + 1"
// to the vertex-neighbours of the picked corner; the terraformer
// adds a node for every tile owning one of those vertices and
// runs `can_*_plan_to` on each.  On flat ground the propagation
// target equals the tile's existing corner heights, so no height
// actually changes — the plan-changeable gate must not fire on
// those drive-by tiles, otherwise a way two vertex-steps from
// the cursor vetoes the operation with "Tile not empty.".
//
// Geometry: corner SW of (6, 7) has canonical vertex (5, 8, E);
// its 3 vertex-neighbours are SE of (5, 8), (5, 7) and (6, 7).
// Tile (5, 7) is reached only via the propagation step, not as
// an owner of SW((6, 7)) itself.  Place a rail there and check
// the operation still goes through.
function test_slope_raise_lower_corner_near_way()
{
	local pl   = player_x(0)
	local rail = way_desc_x.get_available_ways(wt_rail, st_flat)[0]
	ASSERT_TRUE(rail != null)

	local R       = coord3d(5, 7, 0)
	local R_NE    = coord3d(6, 6, 0) // R + neighbours[5]
	local pivot   = coord3d(6, 7, 0)

	ASSERT_EQUAL(command_x.build_way(pl, R, R_NE, rail, true), null)

	ASSERT_EQUAL(command_x.grid_raise_at_corner(pl, pivot, hex_corner.SW), null)
	ASSERT_EQUAL(command_x.grid_lower_at_corner(pl, pivot, hex_corner.SW), null)

	// cleanup
	ASSERT_EQUAL(command_x(tool_remove_way).work(pl, R, R_NE, "" + wt_rail), null)
	RESET_ALL_PLAYER_FUNDS()
}
