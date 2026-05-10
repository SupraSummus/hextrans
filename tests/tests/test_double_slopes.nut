//
// This file is part of the Simutrans project under the Artistic License.
// (see LICENSE.txt)
//

//
// Tests for the `has_double_slopes` way-descriptor opt-in.  Exercises
// every available road, with one of each polarity guaranteed: pak64
// roads ship the opt-in off; `tests/test-pak/test_road_double.dat`
// ships it on.  See the addon-pak rig in tools/test.py.
//


// Iterates every available road and exercises simtool.cc:1334: a
// second consecutive `setslope all_up_slope` on a way-bearing tile
// must be accepted iff the way's `has_double_slopes()` is true.  The
// `saw_double` guard turns "test-pak isn't loading" from a silent
// pass (every pak64 road has the flag off, all match the rejection
// branch) into a loud one.
function test_double_slopes_setslope_gate()
{
	local pl       = player_x(0)
	local setslope = command_x.set_slope
	local remover  = command_x(tool_remove_way)

	local saw_double = false
	foreach (road in way_desc_x.get_available_ways(wt_road, st_flat)) {
		local has_double = road.has_double_slopes()
		saw_double = saw_double || has_double

		// 2-tile road gives a single S-pointing ribi at (3,2), which
		// is what simtool.cc's is_single(ribis) gate requires.
		ASSERT_EQUAL(command_x.build_way(pl, coord3d(3, 2, 0), coord3d(3, 3, 0), road, true), null)
		ASSERT_EQUAL(setslope(pl, coord3d(3, 2, 0), slope.all_up_slope), null)
		ASSERT_EQUAL(setslope(pl, coord3d(3, 2, 0), slope.all_up_slope), has_double ? null : "Tile not empty.")
		ASSERT_EQUAL(tile_x(3, 2, 0).get_slope(), has_double ? slope.south_double : slope.south_narrow)

		// Undo every slope step we actually took, then remove the way.
		ASSERT_EQUAL(setslope(pl, coord3d(3, 2, 0), slope.all_down_slope), null)
		if (has_double) {
			ASSERT_EQUAL(setslope(pl, coord3d(3, 2, 0), slope.all_down_slope), null)
		}
		ASSERT_EQUAL(remover.work(pl, coord3d(3, 2, 0), coord3d(3, 3, 0), "" + wt_road), null)
	}

	ASSERT_TRUE(saw_double)
	RESET_ALL_PLAYER_FUNDS()
}
