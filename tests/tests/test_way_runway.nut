//
// This file is part of the Simutrans project under the Artistic License.
// (see LICENSE.txt)
//


//
// tests for runways/taxiways
//


function test_way_runway_build_rw_flat()
{
	local pl = player_x(0)
	local wayremover = command_x(tool_remove_way)
	local runway = way_desc_x.get_available_ways(wt_air, st_elevated)[0]
	local road = way_desc_x.get_available_ways(wt_road, st_flat)[0]

	// preconditions
	ASSERT_TRUE(runway != null)
	ASSERT_TRUE(road != null)

	// Build runway next to map border, should fail (causes glitches with planes taking off/landing)
	{
		ASSERT_EQUAL(command_x.build_way(pl, coord3d(4, 3, 0), coord3d(4, 5, 0), runway, true), "")
		ASSERT_WAY_PATTERN(wt_air, coord3d(0, 0, 0),
			[
				[0, 0, 0, 0, 0, 0, 0, 0],
				[0, 0, 0, 0, 0, 0, 0, 0],
				[0, 0, 0, 0, 0, 0, 0, 0],
				[0, 0, 0, 0, 0, 0, 0, 0],
				[0, 0, 0, 0, 0, 0, 0, 0],
				[0, 0, 0, 0, 0, 0, 0, 0],
				[0, 0, 0, 0, 0, 0, 0, 0],
				[0, 0, 0, 0, 0, 0, 0, 0],
			])
	}

	// Straight runway away from map border, along the S axis (S=2, N=16, N|S=18).
	{
		ASSERT_EQUAL(command_x.build_way(pl, coord3d(6, 6, 0), coord3d(6, 8, 0), runway, true), null)
		ASSERT_WAY_PATTERN(wt_air, coord3d(5, 5, 0),
			[
				[0,  0, 0, 0, 0, 0, 0, 0],
				[0,  2, 0, 0, 0, 0, 0, 0],
				[0, 18, 0, 0, 0, 0, 0, 0],
				[0, 16, 0, 0, 0, 0, 0, 0],
				[0,  0, 0, 0, 0, 0, 0, 0],
				[0,  0, 0, 0, 0, 0, 0, 0],
				[0,  0, 0, 0, 0, 0, 0, 0],
				[0,  0, 0, 0, 0, 0, 0, 0],
			])
	}

	// Cross runway with existing one along the SE axis (SE=1, NW=8).  The
	// crossing tile (6,7) carries both full axes: S|N|SE|NW = 27.
	{
		ASSERT_EQUAL(command_x.build_way(pl, coord3d(5, 7, 0), coord3d(7, 7, 0), runway, true), null)
		ASSERT_WAY_PATTERN(wt_air, coord3d(5, 5, 0),
			[
				[0,  0, 0, 0, 0, 0, 0, 0],
				[0,  2, 0, 0, 0, 0, 0, 0],
				[1, 27, 8, 0, 0, 0, 0, 0],
				[0, 16, 0, 0, 0, 0, 0, 0],
				[0,  0, 0, 0, 0, 0, 0, 0],
				[0,  0, 0, 0, 0, 0, 0, 0],
				[0,  0, 0, 0, 0, 0, 0, 0],
				[0,  0, 0, 0, 0, 0, 0, 0],
			])

		ASSERT_EQUAL(wayremover.work(pl, coord3d(5, 7, 0), coord3d(7, 7, 0), "" + wt_air), null)
		ASSERT_WAY_PATTERN(wt_air, coord3d(5, 5, 0),
			[
				[0,  0, 0, 0, 0, 0, 0, 0],
				[0,  2, 0, 0, 0, 0, 0, 0],
				[0, 18, 0, 0, 0, 0, 0, 0],
				[0, 16, 0, 0, 0, 0, 0, 0],
				[0,  0, 0, 0, 0, 0, 0, 0],
				[0,  0, 0, 0, 0, 0, 0, 0],
				[0,  0, 0, 0, 0, 0, 0, 0],
				[0,  0, 0, 0, 0, 0, 0, 0],
			])

		ASSERT_EQUAL(wayremover.work(pl, coord3d(6, 6, 0), coord3d(6, 8, 0), "" + wt_air), null)
	}

	// no runways across ways: road along the SE axis (SE=1, NW=8, SE|NW=9),
	// runway crossing it on the S axis must fail.
	{
		ASSERT_EQUAL(command_x.build_way(pl, coord3d(8, 8, 0), coord3d(10, 8, 0), road, true), null)
		ASSERT_EQUAL(command_x.build_way(pl, coord3d(9, 7, 0), coord3d(9, 9, 0), runway, true), "")

		ASSERT_WAY_PATTERN(wt_road, coord3d(6, 6, 0),
			[
				[0, 0, 0, 0, 0, 0, 0, 0],
				[0, 0, 0, 0, 0, 0, 0, 0],
				[0, 0, 1, 9, 8, 0, 0, 0],
				[0, 0, 0, 0, 0, 0, 0, 0],
				[0, 0, 0, 0, 0, 0, 0, 0],
				[0, 0, 0, 0, 0, 0, 0, 0],
				[0, 0, 0, 0, 0, 0, 0, 0],
				[0, 0, 0, 0, 0, 0, 0, 0],
			])

		ASSERT_WAY_PATTERN(wt_air, coord3d(6, 6, 0),
			[
				[0, 0, 0, 0, 0, 0, 0, 0],
				[0, 0, 0, 0, 0, 0, 0, 0],
				[0, 0, 0, 0, 0, 0, 0, 0],
				[0, 0, 0, 0, 0, 0, 0, 0],
				[0, 0, 0, 0, 0, 0, 0, 0],
				[0, 0, 0, 0, 0, 0, 0, 0],
				[0, 0, 0, 0, 0, 0, 0, 0],
				[0, 0, 0, 0, 0, 0, 0, 0],
			])

		ASSERT_EQUAL(wayremover.work(pl, coord3d(8, 8, 0), coord3d(10, 8, 0), "" + wt_road), null)
	}

	ASSERT_EQUAL(pl.get_current_maintenance(), 0)
	RESET_ALL_PLAYER_FUNDS()
}


function test_way_runway_build_tw_flat()
{
	local pl = player_x(0)
	local wayremover = command_x(tool_remove_way)
	local taxiway = way_desc_x.get_available_ways(wt_air, st_flat)[0]
	local road = way_desc_x.get_available_ways(wt_road, st_flat)[0]

	// preconditions
	ASSERT_TRUE(taxiway != null)
	ASSERT_TRUE(road != null)

	// Build taxiway next to map border, should succeed (contrary to runway).
	// Along the S axis (S=2, N=16, N|S=18).
	{
		ASSERT_EQUAL(command_x.build_way(pl, coord3d(4, 3, 0), coord3d(4, 5, 0), taxiway, true), null)
		ASSERT_WAY_PATTERN(wt_air, coord3d(0, 0, 0),
			[
				[0, 0, 0, 0,  0, 0, 0, 0],
				[0, 0, 0, 0,  0, 0, 0, 0],
				[0, 0, 0, 0,  0, 0, 0, 0],
				[0, 0, 0, 0,  2, 0, 0, 0],
				[0, 0, 0, 0, 18, 0, 0, 0],
				[0, 0, 0, 0, 16, 0, 0, 0],
				[0, 0, 0, 0,  0, 0, 0, 0],
				[0, 0, 0, 0,  0, 0, 0, 0],
			])

		ASSERT_EQUAL(wayremover.work(pl, coord3d(4, 3, 0), coord3d(4, 5, 0), "" + wt_air), null)
	}

	// Straight taxiway away from map border
	{
		ASSERT_EQUAL(command_x.build_way(pl, coord3d(6, 6, 0), coord3d(6, 8, 0), taxiway, true), null)
		ASSERT_WAY_PATTERN(wt_air, coord3d(5, 5, 0),
			[
				[0,  0, 0, 0, 0, 0, 0, 0],
				[0,  2, 0, 0, 0, 0, 0, 0],
				[0, 18, 0, 0, 0, 0, 0, 0],
				[0, 16, 0, 0, 0, 0, 0, 0],
				[0,  0, 0, 0, 0, 0, 0, 0],
				[0,  0, 0, 0, 0, 0, 0, 0],
				[0,  0, 0, 0, 0, 0, 0, 0],
				[0,  0, 0, 0, 0, 0, 0, 0],
			])
	}

	// Cross taxiway with existing one along the SE axis; crossing tile = 27.
	{
		ASSERT_EQUAL(command_x.build_way(pl, coord3d(5, 7, 0), coord3d(7, 7, 0), taxiway, true), null)
		ASSERT_WAY_PATTERN(wt_air, coord3d(5, 5, 0),
			[
				[0,  0, 0, 0, 0, 0, 0, 0],
				[0,  2, 0, 0, 0, 0, 0, 0],
				[1, 27, 8, 0, 0, 0, 0, 0],
				[0, 16, 0, 0, 0, 0, 0, 0],
				[0,  0, 0, 0, 0, 0, 0, 0],
				[0,  0, 0, 0, 0, 0, 0, 0],
				[0,  0, 0, 0, 0, 0, 0, 0],
				[0,  0, 0, 0, 0, 0, 0, 0],
			])

		ASSERT_EQUAL(wayremover.work(pl, coord3d(5, 7, 0), coord3d(7, 7, 0), "" + wt_air), null)
		ASSERT_WAY_PATTERN(wt_air, coord3d(5, 5, 0),
			[
				[0,  0, 0, 0, 0, 0, 0, 0],
				[0,  2, 0, 0, 0, 0, 0, 0],
				[0, 18, 0, 0, 0, 0, 0, 0],
				[0, 16, 0, 0, 0, 0, 0, 0],
				[0,  0, 0, 0, 0, 0, 0, 0],
				[0,  0, 0, 0, 0, 0, 0, 0],
				[0,  0, 0, 0, 0, 0, 0, 0],
				[0,  0, 0, 0, 0, 0, 0, 0],
			])

		ASSERT_EQUAL(wayremover.work(pl, coord3d(6, 6, 0), coord3d(6, 8, 0), "" + wt_air), null)
	}

	// no taxiways across ways: road on the SE axis (SE=1, NW=8, SE|NW=9).
	{
		ASSERT_EQUAL(command_x.build_way(pl, coord3d(8, 8, 0), coord3d(10, 8, 0), road, true), null)
		ASSERT_EQUAL(command_x.build_way(pl, coord3d(9, 7, 0), coord3d(9, 9, 0), taxiway, true), "")

		ASSERT_WAY_PATTERN(wt_road, coord3d(6, 6, 0),
			[
				[0, 0, 0, 0, 0, 0, 0, 0],
				[0, 0, 0, 0, 0, 0, 0, 0],
				[0, 0, 1, 9, 8, 0, 0, 0],
				[0, 0, 0, 0, 0, 0, 0, 0],
				[0, 0, 0, 0, 0, 0, 0, 0],
				[0, 0, 0, 0, 0, 0, 0, 0],
				[0, 0, 0, 0, 0, 0, 0, 0],
				[0, 0, 0, 0, 0, 0, 0, 0],
			])

		ASSERT_WAY_PATTERN(wt_air, coord3d(6, 6, 0),
			[
				[0, 0, 0, 0, 0, 0, 0, 0],
				[0, 0, 0, 0, 0, 0, 0, 0],
				[0, 0, 0, 0, 0, 0, 0, 0],
				[0, 0, 0, 0, 0, 0, 0, 0],
				[0, 0, 0, 0, 0, 0, 0, 0],
				[0, 0, 0, 0, 0, 0, 0, 0],
				[0, 0, 0, 0, 0, 0, 0, 0],
				[0, 0, 0, 0, 0, 0, 0, 0],
			])

		ASSERT_EQUAL(wayremover.work(pl, coord3d(8, 8, 0), coord3d(10, 8, 0), "" + wt_road), null)
	}

	ASSERT_EQUAL(pl.get_current_maintenance(), 0)
	RESET_ALL_PLAYER_FUNDS()
}


function test_way_runway_build_mixed_flat()
{
	local pl = player_x(0)
	local wayremover = command_x(tool_remove_way)
	local taxiway = way_desc_x.get_available_ways(wt_air, st_flat)[0]
	local runway = way_desc_x.get_available_ways(wt_air, st_flat)[0]

	// t junction: runway on the S axis (S=2, N=16), taxiway joining from the
	// NW edge.  The join tile (8,9) carries S|N|NW = 26.
	{
		ASSERT_EQUAL(command_x.build_way(pl, coord3d(8, 8, 0), coord3d(8, 10, 0), runway, true), null)
		ASSERT_EQUAL(command_x.build_way(pl, coord3d(7, 9, 0), coord3d(8, 9, 0), taxiway, true), null)

		ASSERT_WAY_PATTERN(wt_air, coord3d(6, 6, 0),
			[
				[0, 0,  0, 0, 0, 0, 0, 0],
				[0, 0,  0, 0, 0, 0, 0, 0],
				[0, 0,  2, 0, 0, 0, 0, 0],
				[0, 1, 26, 0, 0, 0, 0, 0],
				[0, 0, 16, 0, 0, 0, 0, 0],
				[0, 0,  0, 0, 0, 0, 0, 0],
				[0, 0,  0, 0, 0, 0, 0, 0],
				[0, 0,  0, 0, 0, 0, 0, 0],
			])
	}

	// crossing: taxiway extends through, crossing tile (8,9) becomes 27.
	{
		ASSERT_EQUAL(command_x.build_way(pl, coord3d(7, 9, 0), coord3d(9, 9, 0), taxiway, true), null)

		ASSERT_WAY_PATTERN(wt_air, coord3d(6, 6, 0),
			[
				[0, 0,  0, 0, 0, 0, 0, 0],
				[0, 0,  0, 0, 0, 0, 0, 0],
				[0, 0,  2, 0, 0, 0, 0, 0],
				[0, 1, 27, 8, 0, 0, 0, 0],
				[0, 0, 16, 0, 0, 0, 0, 0],
				[0, 0,  0, 0, 0, 0, 0, 0],
				[0, 0,  0, 0, 0, 0, 0, 0],
				[0, 0,  0, 0, 0, 0, 0, 0],
			])

		ASSERT_EQUAL(wayremover.work(pl, coord3d(7, 9, 0), coord3d(9, 9, 0), "" + wt_air), null)
		ASSERT_EQUAL(wayremover.work(pl, coord3d(8, 8, 0), coord3d(8, 10, 0), "" + wt_air), null)
	}

	ASSERT_EQUAL(pl.get_current_maintenance(), 0)
	RESET_ALL_PLAYER_FUNDS()
}
