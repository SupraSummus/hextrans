//
// This file is part of the Simutrans project under the Artistic License.
// (see LICENSE.txt)
//


//
// Tests for the build toolkit (script/hm_toolkit_v3.nut) that the scripts of
// the script generator tool run on.
//


function test_toolkit_slope_constants()
{
	include("hm_toolkit_v3")

	// the toolkit constants are the slopes of the engine, not a table of their own
	ASSERT_EQUAL(hm_slope.FL, slope.flat)
	ASSERT_EQUAL(hm_slope.N1, slope.north_narrow)
	ASSERT_EQUAL(hm_slope.N2, slope.north_double)
	ASSERT_EQUAL(hm_slope.S1, slope.south_narrow)
	ASSERT_EQUAL(hm_slope.S2, slope.south_double)
	ASSERT_EQUAL(hm_slope.E1, slope.east)
	ASSERT_EQUAL(hm_slope.E2, 2 * slope.east)
	ASSERT_EQUAL(hm_slope.W1, slope.west)
	ASSERT_EQUAL(hm_slope.W2, 2 * slope.west)
	ASSERT_EQUAL(hm_slope.UP, slope.all_up_slope)
	ASSERT_EQUAL(hm_slope.DOWN, slope.all_down_slope)
}


// The constants have to do in the engine what the toolkit says they do.
function test_toolkit_slope_commands()
{
	include("hm_toolkit_v3")

	local pl = player_x(0)
	local origin = coord3d(0, 0, 0)
	local pos = coord3d(2, 3, 0)

	// raising and lowering a tile by one level
	hm_commands.clear()
	ASSERT_EQUAL(hm_slope_tl(hm_slope.UP, [pos.x, pos.y, 0]).exec(pl, origin), null)
	ASSERT_TRUE(tile_x(pos.x, pos.y, 1).is_valid())
	ASSERT_EQUAL(tile_x(pos.x, pos.y, 1).get_slope(), slope.flat)
	ASSERT_EQUAL(hm_slope_tl(hm_slope.DOWN, [pos.x, pos.y, 1]).exec(pl, origin), null)
	ASSERT_TRUE(tile_x(pos.x, pos.y, 0).is_valid())
	ASSERT_EQUAL(tile_x(pos.x, pos.y, 0).get_slope(), slope.flat)

	// every named slope on the ground
	local named = [
		[hm_slope.N1, slope.north_narrow],
		[hm_slope.N2, slope.north_double],
		[hm_slope.S1, slope.south_narrow],
		[hm_slope.S2, slope.south_double],
		[hm_slope.E1, slope.east],
		[hm_slope.E2, 2 * slope.east],
		[hm_slope.W1, slope.west],
		[hm_slope.W2, 2 * slope.west],
	]
	foreach (sl in named) {
		ASSERT_EQUAL(hm_slope_tl(sl[0], [pos.x, pos.y, 0]).exec(pl, origin), null)
		ASSERT_EQUAL(tile_x(pos.x, pos.y, 0).get_slope(), sl[1])
		ASSERT_EQUAL(command_x(tool_restoreslope).work(pl, pos), null)
		ASSERT_EQUAL(tile_x(pos.x, pos.y, 0).get_slope(), slope.flat)
	}

	// hm_ground_tl first brings the tile to the requested height
	ASSERT_EQUAL(hm_ground_tl(hm_slope.S1, [pos.x, pos.y, 1]).exec(pl, origin), null)
	ASSERT_EQUAL(tile_x(pos.x, pos.y, 1).get_slope(), slope.south_narrow)
	ASSERT_EQUAL(command_x(tool_restoreslope).work(pl, coord3d(pos.x, pos.y, 1)), null)
	ASSERT_EQUAL(hm_slope_tl(hm_slope.DOWN, [pos.x, pos.y, 1]).exec(pl, origin), null)

	hm_commands.clear()
	RESET_ALL_PLAYER_FUNDS()
}
