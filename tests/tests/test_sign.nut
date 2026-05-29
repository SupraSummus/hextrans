//
// This file is part of the Simutrans project under the Artistic License.
// (see LICENSE.txt)
//


//
// Tests for signs/signals
//


function test_sign_build_oneway()
{
	local pl = player_x(0)
	local public_pl = player_x(1)
	local wayremover = command_x(tool_remove_way)
	local remover = command_x(tool_remover)
	local road = way_desc_x("dirt_road")
	local rail = way_desc_x("sand_track")
	local sign = sign_desc_x.get_available_signs(wt_road).filter(@(idx, sign) sign.is_one_way())[0]

	ASSERT_TRUE(road != null)
	ASSERT_TRUE(sign != null)
	ASSERT_TRUE(sign.is_one_way())

	// on ground without way
	{
		local error_caught = false
		try {
			ASSERT_EQUAL(command_x.build_sign_at(pl, coord3d(2, 3, 0), sign), "")
		}
		catch (e) {
			error_caught = true
			ASSERT_EQUAL(e, "Tool has no effects")
		}
		ASSERT_TRUE(error_caught)
	}

	// in the air
	{
		local error_caught = false
		try {
			ASSERT_EQUAL(command_x.build_sign_at(pl, coord3d(2, 3, 1), sign), "")
		}
		catch (e) {
			error_caught = true
			ASSERT_EQUAL(e, "Tool has no effects")
		}
		ASSERT_TRUE(error_caught)
	}

	// build way
	ASSERT_EQUAL(command_x.build_way(pl, coord3d(2, 1, 0), coord3d(2, 4, 0), road, true), null)

	{
		ASSERT_EQUAL(command_x.build_sign_at(pl, coord3d(2, 3, 0), sign), null)
		ASSERT_EQUAL(tile_x(2, 3, 0).find_object(mo_signal), null)

		local s = tile_x(2, 3, 0).find_object(mo_roadsign)
		ASSERT_TRUE(s != null)
		ASSERT_TRUE(s.is_valid())
		ASSERT_TRUE(s.can_pass(pl))
		ASSERT_TRUE(s.can_pass(public_pl))

		local w = tile_x(2, 3, 0).find_object(mo_way)
		ASSERT_TRUE(w != null)
		ASSERT_EQUAL(w.get_dirs(), dir.northsouth)
		ASSERT_EQUAL(w.get_dirs_masked(), dir.north)

		// change direction of sign
		ASSERT_EQUAL(command_x.build_sign_at(pl, coord3d(2, 3, 0), sign), null)
		ASSERT_TRUE(s.is_valid())
		ASSERT_TRUE(s.can_pass(pl))
		ASSERT_TRUE(s.can_pass(public_pl))

		ASSERT_EQUAL(w.get_dirs(), dir.northsouth)
		ASSERT_EQUAL(w.get_dirs_masked(), dir.south)

		// change direction again, should have original direction
		ASSERT_EQUAL(command_x.build_sign_at(pl, coord3d(2, 3, 0), sign), null)
		ASSERT_TRUE(s.is_valid())
		ASSERT_TRUE(s.can_pass(pl))
		ASSERT_TRUE(s.can_pass(public_pl))

		ASSERT_EQUAL(w.get_dirs(), dir.northsouth)
		ASSERT_EQUAL(w.get_dirs_masked(), dir.north)
	}

	// Crossing subcases split out as test_sign_build_oneway_at_crossing
	// — see "Sign / traffic-light 2-axis FSM" in TODO.md.

	// Cannot remove signs of other players (except public player)
	{
		ASSERT_EQUAL(command_x.build_sign_at(public_pl, coord3d(2, 2, 0), sign), null)
		ASSERT_EQUAL(command_x.build_sign_at(pl,        coord3d(2, 3, 0), sign), null)

		ASSERT_EQUAL(command_x(tool_remover).work(pl, coord3d(2, 2, 0)), "Der Besitzer erlaubt das Entfernen nicht")
		ASSERT_EQUAL(command_x(tool_remover).work(public_pl, coord3d(2, 2, 0)), null)
		ASSERT_EQUAL(command_x(tool_remover).work(public_pl, coord3d(2, 3, 0)), null)
	}

	// remove stuff
	ASSERT_EQUAL(wayremover.work(pl, coord3d(2, 1, 0), coord3d(2, 4, 0), "" + wt_road), null)

	RESET_ALL_PLAYER_FUNDS()
}


function test_sign_build_oneway_at_crossing()
{
	// Sign behavior at multi-axis intersections: placement allowed on
	// a road-rail crossing (way is a 2-way after subtracting the
	// crossing axis), rejected once a road-road crossing turns the
	// junction into 3+ ways.  Road column runs the S axis (constant x,
	// +y; S=2, N|S=18, N=16); the rail/road cross runs the SE axis
	// (+x; SE=1, SE|NW=9, NW=8).  60° hex crossing pair, replacing the
	// square N-S × E-W setup.
	local pl = player_x(0)
	local wayremover = command_x(tool_remove_way)
	local remover = command_x(tool_remover)
	local road = way_desc_x("dirt_road")
	local rail = way_desc_x("sand_track")
	local sign = sign_desc_x.get_available_signs(wt_road).filter(@(idx, sign) sign.is_one_way())[0]

	// Rail/road cross at y=3, x=1..3 along the SE axis.
	local cross_row = [
		[0, 0, 0, 0, 0, 0, 0, 0],
		[0, 0, 0, 0, 0, 0, 0, 0],
		[0, 0, 0, 0, 0, 0, 0, 0],
		[0, 1, 9, 8, 0, 0, 0, 0],
		[0, 0, 0, 0, 0, 0, 0, 0],
		[0, 0, 0, 0, 0, 0, 0, 0],
		[0, 0, 0, 0, 0, 0, 0, 0],
		[0, 0, 0, 0, 0, 0, 0, 0],
	]

	// Road column at x=2, y=1..4 along the S axis, unmasked: S=2,
	// N|S=18 (mid), N=16 (end).  At the crossing tile (2,3) the road
	// alone is a straight 2-way (18) — this is the topology that makes
	// the one-way sign valid; the sign only narrows the *masked* dirs.
	local road_col_full = [
		[0, 0,  0, 0, 0, 0, 0, 0],
		[0, 0,  2, 0, 0, 0, 0, 0],
		[0, 0, 18, 0, 0, 0, 0, 0],
		[0, 0, 18, 0, 0, 0, 0, 0],
		[0, 0, 16, 0, 0, 0, 0, 0],
		[0, 0,  0, 0, 0, 0, 0, 0],
		[0, 0,  0, 0, 0, 0, 0, 0],
		[0, 0,  0, 0, 0, 0, 0, 0],
	]

	// As road_col_full, but the SE-axis road arm has merged into the
	// column at (2,3): N|S|SE|NW = 16|2|1|8 = 27, with SE=1 / NW=8 arms.
	local road_cross_full = [
		[0, 0,  0, 0, 0, 0, 0, 0],
		[0, 0,  2, 0, 0, 0, 0, 0],
		[0, 0, 18, 0, 0, 0, 0, 0],
		[0, 1, 27, 8, 0, 0, 0, 0],
		[0, 0, 16, 0, 0, 0, 0, 0],
		[0, 0,  0, 0, 0, 0, 0, 0],
		[0, 0,  0, 0, 0, 0, 0, 0],
		[0, 0,  0, 0, 0, 0, 0, 0],
	]

	ASSERT_EQUAL(command_x.build_way(pl, coord3d(2, 1, 0), coord3d(2, 4, 0), road, true), null)
	ASSERT_EQUAL(command_x.build_sign_at(pl, coord3d(2, 3, 0), sign), null)

	// build road/rail crossing over sign, should succeed if crossing is possible without sign
	{
		ASSERT_EQUAL(command_x.build_way(pl, coord3d(1, 3, 0), coord3d(3, 3, 0), rail, true), null)

		// Unmasked: the road is a straight 2-way N|S column even at the
		// crossing tile (18 at y=3); the rail occupies its own SE axis.
		ASSERT_WAY_PATTERN(wt_road, coord3d(0, 0, 0), road_col_full)
		ASSERT_WAY_PATTERN(wt_rail, coord3d(0, 0, 0), cross_row)

		// Masked: default one-way orientation masks out the S half,
		// leaving N=16 (matches the first build in test_sign_build_oneway).
		ASSERT_WAY_PATTERN_MASKED(wt_road, coord3d(0, 0, 0),
			[
				[0, 0,  0, 0, 0, 0, 0, 0],
				[0, 0,  2, 0, 0, 0, 0, 0],
				[0, 0, 18, 0, 0, 0, 0, 0],
				[0, 0, 16, 0, 0, 0, 0, 0],
				[0, 0, 16, 0, 0, 0, 0, 0],
				[0, 0,  0, 0, 0, 0, 0, 0],
				[0, 0,  0, 0, 0, 0, 0, 0],
				[0, 0,  0, 0, 0, 0, 0, 0],
			])

		ASSERT_WAY_PATTERN_MASKED(wt_rail, coord3d(0, 0, 0), cross_row)

		local w = tile_x(2, 3, 0).find_object(mo_way)
		ASSERT_TRUE(w.is_crossing())

		// change direction of sign on rail-road crossing, should succeed
		ASSERT_EQUAL(command_x.build_sign_at(pl, coord3d(2, 3, 0), sign), null)

		// Flipping the sign leaves the full topology untouched (still a
		// 2-way N|S column) and only flips the mask to the S=2 half.
		ASSERT_WAY_PATTERN(wt_road, coord3d(0, 0, 0), road_col_full)
		ASSERT_WAY_PATTERN_MASKED(wt_road, coord3d(0, 0, 0),
			[
				[0, 0,  0, 0, 0, 0, 0, 0],
				[0, 0,  2, 0, 0, 0, 0, 0],
				[0, 0, 18, 0, 0, 0, 0, 0],
				[0, 0,  2, 0, 0, 0, 0, 0],
				[0, 0, 16, 0, 0, 0, 0, 0],
				[0, 0,  0, 0, 0, 0, 0, 0],
				[0, 0,  0, 0, 0, 0, 0, 0],
				[0, 0,  0, 0, 0, 0, 0, 0],
			])

		ASSERT_WAY_PATTERN_MASKED(wt_rail, coord3d(0, 0, 0), cross_row)

		// and remove rail
		ASSERT_EQUAL(wayremover.work(pl, coord3d(1, 3, 0), coord3d(3, 3, 0), "" + wt_rail), null)
	}

	// build road/road crossing over sign, should succeed
	{
		ASSERT_EQUAL(command_x.build_way(pl, coord3d(1, 3, 0), coord3d(3, 3, 0), road, true), null)

		ASSERT_WAY_PATTERN(wt_road, coord3d(0, 0, 0), road_cross_full)

		local w = tile_x(2, 3, 0).find_object(mo_way)
		ASSERT_FALSE(w.is_crossing())
		// One-way mask survives the road merge: still facing S (from
		// the second build above), so the opposite N half is dropped
		// from the 27 junction, leaving SE|S|NW = 1|2|8 = 11.
		ASSERT_WAY_PATTERN_MASKED(wt_road, coord3d(0, 0, 0),
			[
				[0, 0,  0, 0, 0, 0, 0, 0],
				[0, 0,  2, 0, 0, 0, 0, 0],
				[0, 0, 18, 0, 0, 0, 0, 0],
				[0, 1, 11, 8, 0, 0, 0, 0],
				[0, 0, 16, 0, 0, 0, 0, 0],
				[0, 0,  0, 0, 0, 0, 0, 0],
				[0, 0,  0, 0, 0, 0, 0, 0],
				[0, 0,  0, 0, 0, 0, 0, 0],
			])

		// change direction of sign on road crossing, should fail
		local error_caught = false
		try {
			ASSERT_EQUAL(command_x.build_sign_at(pl, coord3d(2, 3, 0), sign), null)
		}
		catch (e) {
			ASSERT_EQUAL(e, "Tool has no effects")
			error_caught = true
		}
		ASSERT_TRUE(error_caught)
	}

	// remove sign, try to build again (should fail because of crossing)
	{
		ASSERT_EQUAL(remover.work(pl, coord3d(2, 3, 0)), null)
		// Removing the sign drops the mask; the road junction itself is
		// unchanged (still the 27 cross).
		ASSERT_WAY_PATTERN(wt_road, coord3d(0, 0, 0), road_cross_full)

		ASSERT_EQUAL(tile_x(2, 3, 0).find_object(mo_roadsign), null)

		local error_caught = false
		try {
			ASSERT_EQUAL(command_x.build_sign_at(pl, coord3d(2, 3, 0), sign), null)
		}
		catch (e) {
			ASSERT_EQUAL(e, "Tool has no effects")
			error_caught = true
		}
		ASSERT_TRUE(error_caught)
	}

	ASSERT_EQUAL(wayremover.work(pl, coord3d(1, 3, 0), coord3d(3, 3, 0), "" + wt_road), null)
	ASSERT_EQUAL(wayremover.work(pl, coord3d(2, 1, 0), coord3d(2, 4, 0), "" + wt_road), null)

	RESET_ALL_PLAYER_FUNDS()
}


function test_sign_build_trafficlight()
{
	// Traffic light needs a 3+ way junction.  Road column runs the S
	// axis (constant x, +y); the crossing arm runs the SE axis (+x).
	// 3-way at (2,3) = N|S|NW (16|2|8 = 26, a T-junction); the 4-way
	// adds the SE arm for N|S|NW|SE (27).  Replaces the square
	// N-S × E-W setup — same 60° hex crossing pair as
	// test_sign_build_oneway_at_crossing.
	local pl = player_x(0)
	local public_pl = player_x(1)
	local wayremover = command_x(tool_remove_way)
	local remover = command_x(tool_remover)
	local road = way_desc_x.get_available_ways(wt_road, st_flat)[0]
	local rail = way_desc_x.get_available_ways(wt_rail, st_flat)[0]
	local trafficlight = sign_desc_x.get_available_signs(wt_road).filter(@(idx, sign) sign.is_traffic_light())[0]

	ASSERT_TRUE(trafficlight != null)

	// on ground
	{
		local error_caught = false
		try {
			ASSERT_EQUAL(command_x.build_sign_at(pl, coord3d(2, 3, 0), trafficlight), "")
		}
		catch (e) {
			error_caught = true
			ASSERT_EQUAL(e, "Tool has no effects")
		}
		ASSERT_TRUE(error_caught)
	}

	ASSERT_EQUAL(command_x.build_way(pl, coord3d(2, 1, 0), coord3d(2, 4, 0), road, true), null)

	// end of way
	{
		local error_caught = false
		try {
			ASSERT_EQUAL(command_x.build_sign_at(pl, coord3d(2, 1, 0), trafficlight), "")
		}
		catch (e) {
			error_caught = true
			ASSERT_EQUAL(e, "Tool has no effects")
		}
		ASSERT_TRUE(error_caught)
	}

	// 2-way
	{
		local error_caught = false
		try {
			ASSERT_EQUAL(command_x.build_sign_at(pl, coord3d(2, 3, 0), trafficlight), "")
		}
		catch (e) {
			error_caught = true
			ASSERT_EQUAL(e, "Tool has no effects")
		}
		ASSERT_TRUE(error_caught)
	}

	ASSERT_EQUAL(command_x.build_way(pl, coord3d(1, 3, 0), coord3d(2, 3, 0), road, true), null)

	// 3-way
	{
		ASSERT_EQUAL(command_x.build_sign_at(pl, coord3d(2, 3, 0), trafficlight), null)

		local tl = tile_x(2, 3, 0).find_object(mo_roadsign)
		ASSERT_TRUE(tl != null)
		ASSERT_TRUE(tl.can_pass(pl))
		ASSERT_TRUE(tl.can_pass(public_pl))
		ASSERT_TRUE(tl.get_desc().is_traffic_light())

		ASSERT_WAY_PATTERN(wt_road, coord3d(0, 0, 0),
			[
				[0, 0,  0, 0, 0, 0, 0, 0],
				[0, 0,  2, 0, 0, 0, 0, 0],
				[0, 0, 18, 0, 0, 0, 0, 0],
				[0, 1, 26, 0, 0, 0, 0, 0],
				[0, 0, 16, 0, 0, 0, 0, 0],
				[0, 0,  0, 0, 0, 0, 0, 0],
				[0, 0,  0, 0, 0, 0, 0, 0],
				[0, 0,  0, 0, 0, 0, 0, 0],
			])

		ASSERT_EQUAL(tile_x(2, 3, 0).get_way_dirs_masked(wt_road), dir.north | dir.south | dir.northwest)
	}


	// Make 4-way from 3-way
	{
		ASSERT_EQUAL(command_x.build_way(pl, coord3d(2, 3, 0), coord3d(3, 3, 0), road, true), null)
		ASSERT_WAY_PATTERN(wt_road, coord3d(0, 0, 0),
			[
				[0, 0,  0, 0, 0, 0, 0, 0],
				[0, 0,  2, 0, 0, 0, 0, 0],
				[0, 0, 18, 0, 0, 0, 0, 0],
				[0, 1, 27, 8, 0, 0, 0, 0],
				[0, 0, 16, 0, 0, 0, 0, 0],
				[0, 0,  0, 0, 0, 0, 0, 0],
				[0, 0,  0, 0, 0, 0, 0, 0],
				[0, 0,  0, 0, 0, 0, 0, 0],
			])

		ASSERT_EQUAL(tile_x(2, 3, 0).get_way_dirs_masked(wt_road), dir.north | dir.south | dir.northwest | dir.southeast)
	}

	// remove traffic light on crossing via wayremover
	{
		ASSERT_EQUAL(wayremover.work(pl, coord3d(2, 1, 0), coord3d(2, 4, 0), "" + wt_road), null)
		ASSERT_EQUAL(tile_x(2, 3, 0).find_object(mo_roadsign), null)
	}

	ASSERT_EQUAL(wayremover.work(pl, coord3d(1, 3, 0), coord3d(3, 3, 0), "" + wt_road), null)
	RESET_ALL_PLAYER_FUNDS()
}


function test_sign_remove_trafficlight()
{
	local pl = player_x(0)
	local public_pl = player_x(1)
	local wayremover = command_x(tool_remove_way)
	local road = way_desc_x.get_available_ways(wt_road, st_flat)[0]
	local trafficlight = sign_desc_x.get_available_signs(wt_road).filter(@(idx, sign) sign.is_traffic_light())[0]

	ASSERT_EQUAL(command_x.build_way(pl, coord3d(2, 1, 0), coord3d(2, 3, 0), road, true), null)
	ASSERT_EQUAL(command_x.build_way(pl, coord3d(1, 2, 0), coord3d(3, 2, 0), road, true), null)
	ASSERT_EQUAL(command_x.build_way(pl, coord3d(5, 4, 0), coord3d(5, 6, 0), road, true), null)
	ASSERT_EQUAL(command_x.build_way(pl, coord3d(4, 5, 0), coord3d(6, 5, 0), road, true), null)

	ASSERT_EQUAL(command_x.build_sign_at(pl, coord3d(2, 2, 0), trafficlight), null)
	ASSERT_EQUAL(command_x.build_sign_at(pl, coord3d(5, 5, 0), trafficlight), null)

	// Note that both traffic lights must have the same direction for the test to work
	// So the second traffic light must not change direction between the two wayremover calls
	{
		ASSERT_EQUAL(wayremover.work(pl, coord3d(2, 1, 0), coord3d(2, 3, 0), "" + wt_road), null)
		ASSERT_EQUAL(wayremover.work(pl, coord3d(4, 5, 0), coord3d(6, 5, 0), "" + wt_road), null)

		ASSERT_EQUAL(tile_x(2, 2, 0).find_object(mo_signal), null)
		ASSERT_EQUAL(tile_x(5, 5, 0).find_object(mo_signal), null)
	}

	ASSERT_EQUAL(wayremover.work(pl, coord3d(1, 2, 0), coord3d(3, 2, 0), "" + wt_road), null)
	ASSERT_EQUAL(wayremover.work(pl, coord3d(5, 4, 0), coord3d(5, 6, 0), "" + wt_road), null)

	RESET_ALL_PLAYER_FUNDS()
}


function test_sign_build_private_way()
{
	// Road column runs the S axis (constant x, +y; S=2, N|S=18, N=16);
	// the rail/road cross runs the SE axis (+x; SE=1, SE|NW=9, NW=8).
	// Private-way signs do not mask direction, so the masked road dirs
	// equal the full topology throughout — same 60° hex crossing pair
	// as test_sign_build_oneway_at_crossing.
	local pl = player_x(0)
	local public_pl = player_x(1)
	local wayremover = command_x(tool_remove_way)
	local remover = command_x(tool_remover)

	local road = way_desc_x("dirt_road")
	local rail = way_desc_x("sand_track")
	local sign = sign_desc_x.get_available_signs(wt_road).filter(@(idx, sign) sign.is_private_way())[0]

	// Road alone: a straight N|S column, 18 even at the crossing tile.
	local road_col_full = [
		[0, 0,  0, 0, 0, 0, 0, 0],
		[0, 0,  2, 0, 0, 0, 0, 0],
		[0, 0, 18, 0, 0, 0, 0, 0],
		[0, 0, 18, 0, 0, 0, 0, 0],
		[0, 0, 16, 0, 0, 0, 0, 0],
		[0, 0,  0, 0, 0, 0, 0, 0],
		[0, 0,  0, 0, 0, 0, 0, 0],
		[0, 0,  0, 0, 0, 0, 0, 0],
	]
	// Road+road junction at (2,3): N|S|SE|NW = 27, with SE / NW arms.
	local road_cross_full = [
		[0, 0,  0, 0, 0, 0, 0, 0],
		[0, 0,  2, 0, 0, 0, 0, 0],
		[0, 0, 18, 0, 0, 0, 0, 0],
		[0, 1, 27, 8, 0, 0, 0, 0],
		[0, 0, 16, 0, 0, 0, 0, 0],
		[0, 0,  0, 0, 0, 0, 0, 0],
		[0, 0,  0, 0, 0, 0, 0, 0],
		[0, 0,  0, 0, 0, 0, 0, 0],
	]

	ASSERT_TRUE(road != null)
	ASSERT_TRUE(sign != null)
	ASSERT_TRUE(sign.is_private_way())

	// on ground
	{
		local error_caught = false
		try {
			ASSERT_EQUAL(command_x.build_sign_at(pl, coord3d(2, 3, 0), sign), "")
		}
		catch (e) {
			error_caught = true
			ASSERT_EQUAL(e, "Tool has no effects")
		}
		ASSERT_TRUE(error_caught)
	}

	// in the air
	{
		local error_caught = false
		try {
			ASSERT_EQUAL(command_x.build_sign_at(pl, coord3d(2, 3, 1), sign), "")
		}
		catch (e) {
			error_caught = true
			ASSERT_EQUAL(e, "Tool has no effects")
		}
		ASSERT_TRUE(error_caught)
	}

	// build way
	ASSERT_EQUAL(command_x.build_way(pl, coord3d(2, 1, 0), coord3d(2, 4, 0), road, true), null)

	{
		ASSERT_EQUAL(command_x.build_sign_at(pl, coord3d(2, 3, 0), sign), null)
		ASSERT_EQUAL(tile_x(2, 3, 0).find_object(mo_signal), null)

		local s = tile_x(2, 3, 0).find_object(mo_roadsign)
		ASSERT_TRUE(s != null)
		ASSERT_TRUE(s.is_valid())
		ASSERT_TRUE(s.can_pass(pl))
		ASSERT_FALSE(s.can_pass(public_pl))

		local w = tile_x(2, 3, 0).find_object(mo_way)
		ASSERT_TRUE(w != null)
		ASSERT_EQUAL(w.get_dirs(), dir.northsouth)
		ASSERT_EQUAL(w.get_dirs_masked(), dir.northsouth)

		// change direction of sign
		ASSERT_EQUAL(command_x.build_sign_at(pl, coord3d(2, 3, 0), sign), null)
		ASSERT_TRUE(s.is_valid())
		ASSERT_TRUE(s.can_pass(pl))
		ASSERT_FALSE(s.can_pass(public_pl))

		ASSERT_EQUAL(w.get_dirs(), dir.northsouth)
		ASSERT_EQUAL(w.get_dirs_masked(), dir.northsouth)

		// change direction again, should have original direction
		ASSERT_EQUAL(command_x.build_sign_at(pl, coord3d(2, 3, 0), sign), null)
		ASSERT_TRUE(s.is_valid())
		ASSERT_TRUE(s.can_pass(pl))
		ASSERT_FALSE(s.can_pass(public_pl))

		ASSERT_EQUAL(w.get_dirs(), dir.northsouth)
		ASSERT_EQUAL(w.get_dirs_masked(), dir.northsouth)
	}

	// build road/rail crossing over sign, should succeed if crossing is possible without sign
	{
		ASSERT_EQUAL(command_x.build_way(pl, coord3d(1, 3, 0), coord3d(3, 3, 0), rail, true), null)

		// Masked == full: a private-way sign leaves direction unmasked.
		ASSERT_WAY_PATTERN(wt_road, coord3d(0, 0, 0), road_col_full)
		ASSERT_WAY_PATTERN_MASKED(wt_road, coord3d(0, 0, 0), road_col_full)

		ASSERT_WAY_PATTERN_MASKED(wt_rail, coord3d(0, 0, 0),
			[
				[0, 0, 0, 0, 0, 0, 0, 0],
				[0, 0, 0, 0, 0, 0, 0, 0],
				[0, 0, 0, 0, 0, 0, 0, 0],
				[0, 1, 9, 8, 0, 0, 0, 0],
				[0, 0, 0, 0, 0, 0, 0, 0],
				[0, 0, 0, 0, 0, 0, 0, 0],
				[0, 0, 0, 0, 0, 0, 0, 0],
				[0, 0, 0, 0, 0, 0, 0, 0],
			])

		ASSERT_TRUE(tile_x(2, 3, 0).find_object(mo_way).is_crossing())

		// and remove rail
		ASSERT_EQUAL(wayremover.work(pl, coord3d(1, 3, 0), coord3d(3, 3, 0), "" + wt_rail), null)
	}

	// build road/road crossing over sign, should succeed
	{
		ASSERT_EQUAL(command_x.build_way(pl, coord3d(1, 3, 0), coord3d(3, 3, 0), road, true), null)

		ASSERT_WAY_PATTERN(wt_road, coord3d(0, 0, 0), road_cross_full)

		local w = tile_x(2, 3, 0).find_object(mo_way)
		ASSERT_FALSE(w.is_crossing())
		ASSERT_EQUAL(w.get_dirs_masked(), dir.north | dir.south | dir.northwest | dir.southeast)

		// change direction of sign on road crossing, should fail
		local error_caught = false
		try {
			ASSERT_EQUAL(command_x.build_sign_at(pl, coord3d(2, 3, 0), sign), null)
		}
		catch (e) {
			ASSERT_EQUAL(e, "Tool has no effects")
			error_caught = true
		}
		ASSERT_TRUE(error_caught)
	}

	// remove sign, try to build again (should fail because of crossing)
	{
		ASSERT_EQUAL(remover.work(pl, coord3d(2, 3, 0)), null)
		// Removing the sign drops the mask; the road junction is unchanged.
		ASSERT_WAY_PATTERN(wt_road, coord3d(0, 0, 0), road_cross_full)

		ASSERT_EQUAL(tile_x(2, 3, 0).find_object(mo_roadsign), null)

		local error_caught = false
		try {
			ASSERT_EQUAL(command_x.build_sign_at(pl, coord3d(2, 3, 0), sign), null)
		}
		catch (e) {
			ASSERT_EQUAL(e, "Tool has no effects")
			error_caught = true
		}
		ASSERT_TRUE(error_caught)
	}

	// remove stuff
	ASSERT_EQUAL(wayremover.work(pl, coord3d(1, 3, 0), coord3d(3, 3, 0), "" + wt_road), null)
	ASSERT_EQUAL(wayremover.work(pl, coord3d(2, 1, 0), coord3d(2, 4, 0), "" + wt_road), null)

	RESET_ALL_PLAYER_FUNDS()
}


function test_sign_build_signal()
{
	local pl = player_x(0)
	local public_pl = player_x(1)
	local wayremover = command_x(tool_remove_way)
	local remover = command_x(tool_remover)
	local rail = way_desc_x.get_available_ways(wt_rail, st_flat)[0]
	local road = way_desc_x.get_available_ways(wt_road, st_flat)[0]
	local signal     = sign_desc_x.get_available_signs(wt_rail).filter(@(idx, sign) sign.is_signal())[0]
	local presignal  = sign_desc_x.get_available_signs(wt_rail).filter(@(idx, sign) sign.is_pre_signal())[0]
	local priosignal = sign_desc_x.get_available_signs(wt_rail).filter(@(idx, sign) sign.is_priority_signal())[0]
	local lbsignal   = sign_desc_x.get_available_signs(wt_rail).filter(@(idx, sign) sign.is_longblock_signal())[0]
	local chsignal   = sign_desc_x.get_available_signs(wt_rail).filter(@(idx, sign) sign.is_choose_sign())[0]
	local eocsignal  = sign_desc_x.get_available_signs(wt_rail).filter(@(idx, sign) sign.is_end_choose_signal())[0]

	local all_signals = [ signal, presignal, priosignal, lbsignal, chsignal, eocsignal ]

	foreach (s in all_signals) {
		ASSERT_TRUE(s != null)
	}

	{
		// Build signs on flat ground, should fail
		foreach (s in all_signals) {
			local error_caught = false
			try {
				ASSERT_EQUAL(command_x.build_sign_at(pl, coord3d(2, 3, 0), s), "")
			}
			catch (e) {
				error_caught = true
				ASSERT_EQUAL(e, "Tool has no effects")
			}
			ASSERT_TRUE(error_caught)
			ASSERT_EQUAL(tile_x(2, 3, 0).find_object(mo_signal), null)
		}
	}

	ASSERT_EQUAL(command_x.build_way(pl, coord3d(2, 0, 0), coord3d(2, 7, 0), road, true), null)

	{
		// Rail signals on road, should fail
		foreach (s in all_signals) {
			local error_caught = false
			try {
				ASSERT_EQUAL(command_x.build_sign_at(pl, coord3d(2, 3, 0), s), "")
			}
			catch (e) {
				error_caught = true
				ASSERT_EQUAL(e, "Tool has no effects")
			}
			ASSERT_TRUE(error_caught)
			ASSERT_EQUAL(tile_x(2, 3, 0).find_object(mo_signal), null)
		}
	}

	ASSERT_EQUAL(wayremover.work(pl, coord3d(2, 0, 0), coord3d(2, 7, 0), "" + wt_road), null)

	// Six straight 3-tile rails, all carrying a signal at the middle
	// tile.  Along +x they run on the SE axis (SE=1 at the low-x end,
	// NW=8 at the high-x end, SE|NW=9 in the middle); along +y on the S
	// axis (S=2 top, N=16 bottom, N|S=18 middle).  A freshly-placed
	// (2-way) signal leaves the masked pattern equal to the unmasked one.
	local se_2way = [
		[1, 9, 8, 0, 0, 0, 0, 0],
		[1, 9, 8, 0, 0, 0, 0, 0],
		[1, 9, 8, 0, 0, 0, 0, 0],
		[1, 9, 8, 0, 0, 0, 0, 0],
		[1, 9, 8, 0, 0, 0, 0, 0],
		[1, 9, 8, 0, 0, 0, 0, 0],
		[0, 0, 0, 0, 0, 0, 0, 0],
		[0, 0, 0, 0, 0, 0, 0, 0],
	]
	local s_2way = [
		[2,  2,  2,  2,  2,  2,  0, 0],
		[18, 18, 18, 18, 18, 18, 0, 0],
		[16, 16, 16, 16, 16, 16, 0, 0],
		[0, 0, 0, 0, 0, 0, 0, 0],
		[0, 0, 0, 0, 0, 0, 0, 0],
		[0, 0, 0, 0, 0, 0, 0, 0],
		[0, 0, 0, 0, 0, 0, 0, 0],
		[0, 0, 0, 0, 0, 0, 0, 0],
	]

	// build signals
	{
		foreach (i, s in all_signals) {
			ASSERT_EQUAL(command_x.build_way(pl, coord3d(0, i, 0), coord3d(2, i, 0), rail, true), null)
			ASSERT_EQUAL(command_x.build_sign_at(pl, coord3d(1, i, 0), s), null)

			if (s == eocsignal) {
				ASSERT_TRUE(tile_x(1, i, 0).find_object(mo_roadsign) != null) // for end-of-choose
			}
			else {
				ASSERT_TRUE(tile_x(1, i, 0).find_object(mo_signal) != null)
			}
		}

		ASSERT_WAY_PATTERN(wt_rail, coord3d(0, 0, 0), se_2way)
		ASSERT_WAY_PATTERN_MASKED(wt_rail, coord3d(0, 0, 0), se_2way)
	}

	// make signals directional
	{
		foreach (i, s in all_signals) {
			ASSERT_EQUAL(command_x.build_way(pl, coord3d(0, i, 0), coord3d(2, i, 0), rail, true), null)
			ASSERT_EQUAL(command_x.build_sign_at(pl, coord3d(1, i, 0), s), null)

			if (s == eocsignal) {
				ASSERT_TRUE(tile_x(1, i, 0).find_object(mo_roadsign) != null) // for end-of-choose
			}
			else {
				ASSERT_TRUE(tile_x(1, i, 0).find_object(mo_signal) != null)
			}
		}

		ASSERT_WAY_PATTERN(wt_rail, coord3d(0, 0, 0), se_2way)

		// 1-way signal drops SE, leaving NW=8 (the eocsignal in row 5 is
		// an end-of-choose roadsign and blocks nothing).
		ASSERT_WAY_PATTERN_MASKED(wt_rail, coord3d(0, 0, 0),
			[
				[1, 8, 8, 0, 0, 0, 0, 0],
				[1, 8, 8, 0, 0, 0, 0, 0],
				[1, 8, 8, 0, 0, 0, 0, 0],
				[1, 8, 8, 0, 0, 0, 0, 0],
				[1, 8, 8, 0, 0, 0, 0, 0],
				[1, 9, 8, 0, 0, 0, 0, 0],
				[0, 0, 0, 0, 0, 0, 0, 0],
				[0, 0, 0, 0, 0, 0, 0, 0],
			])
	}

	// signals into different direction
	{
		foreach (i, s in all_signals) {
			ASSERT_EQUAL(command_x.build_way(pl, coord3d(0, i, 0), coord3d(2, i, 0), rail, true), null)
			ASSERT_EQUAL(command_x.build_sign_at(pl, coord3d(1, i, 0), s), null)

			if (s == eocsignal) {
				ASSERT_TRUE(tile_x(1, i, 0).find_object(mo_roadsign) != null) // for end-of-choose
			}
			else {
				ASSERT_TRUE(tile_x(1, i, 0).find_object(mo_signal) != null)
			}
		}

		ASSERT_WAY_PATTERN(wt_rail, coord3d(0, 0, 0), se_2way)

		// the next re-click flips to the other 1-way state, dropping NW
		// and leaving SE=1.
		ASSERT_WAY_PATTERN_MASKED(wt_rail, coord3d(0, 0, 0),
			[
				[1, 1, 8, 0, 0, 0, 0, 0],
				[1, 1, 8, 0, 0, 0, 0, 0],
				[1, 1, 8, 0, 0, 0, 0, 0],
				[1, 1, 8, 0, 0, 0, 0, 0],
				[1, 1, 8, 0, 0, 0, 0, 0],
				[1, 9, 8, 0, 0, 0, 0, 0],
				[0, 0, 0, 0, 0, 0, 0, 0],
				[0, 0, 0, 0, 0, 0, 0, 0],
			])
	}

	// build way across signal
	{
		ASSERT_EQUAL(command_x.build_way(pl, coord3d(1, 0, 0), coord3d(1, all_signals.len()-1, 0), rail, true), null)

		// the x=1 column now runs along +y (S axis), so each (1,i) is a
		// junction: SE|NW horizontal plus the vertical N/S.  Top end
		// (y=0) has S only, bottom end (y=5) N only, middle has both.
		ASSERT_WAY_PATTERN(wt_rail, coord3d(0, 0, 0),
			[
				[1, 11, 8, 0, 0, 0, 0, 0],
				[1, 27, 8, 0, 0, 0, 0, 0],
				[1, 27, 8, 0, 0, 0, 0, 0],
				[1, 27, 8, 0, 0, 0, 0, 0],
				[1, 27, 8, 0, 0, 0, 0, 0],
				[1, 25, 8, 0, 0, 0, 0, 0],
				[0, 0, 0, 0, 0, 0, 0, 0],
				[0, 0, 0, 0, 0, 0, 0, 0],
			])

		// the 1-way signals (facing SE) drop NW on the junction; the
		// vertical axis stays open.
		ASSERT_WAY_PATTERN_MASKED(wt_rail, coord3d(0, 0, 0),
			[
				[1, 3, 8, 0, 0, 0, 0, 0],
				[1, 19, 8, 0, 0, 0, 0, 0],
				[1, 19, 8, 0, 0, 0, 0, 0],
				[1, 19, 8, 0, 0, 0, 0, 0],
				[1, 19, 8, 0, 0, 0, 0, 0],
				[1, 25, 8, 0, 0, 0, 0, 0],
				[0, 0, 0, 0, 0, 0, 0, 0],
				[0, 0, 0, 0, 0, 0, 0, 0],
			])
	}

	// remove everything
	{
		foreach (i, s in all_signals) {
			ASSERT_EQUAL(wayremover.work(pl, coord3d(0, i, 0), coord3d(2, i, 0), "" + wt_rail), null)
		}

		ASSERT_EQUAL(wayremover.work(pl, coord3d(1, 0, 0), coord3d(1, all_signals.len()-1, 0), "" + wt_rail), null)
	}

	// build signals, dir 2
	{
		foreach (i, s in all_signals) {
			ASSERT_EQUAL(command_x.build_way(pl, coord3d(i, 0, 0), coord3d(i, 2, 0), rail, true), null)
			ASSERT_EQUAL(command_x.build_sign_at(pl, coord3d(i, 1, 0), s), null)

			if (s == eocsignal) {
				ASSERT_TRUE(tile_x(i, 1, 0).find_object(mo_roadsign) != null) // for end-of-choose
			}
			else {
				ASSERT_TRUE(tile_x(i, 1, 0).find_object(mo_signal) != null)
			}
		}

		ASSERT_WAY_PATTERN(wt_rail, coord3d(0, 0, 0), s_2way)
		ASSERT_WAY_PATTERN_MASKED(wt_rail, coord3d(0, 0, 0), s_2way)
	}

	// make signals directional, dir 2
	{
		foreach (i, s in all_signals) {
			ASSERT_EQUAL(command_x.build_sign_at(pl, coord3d(i, 1, 0), s), null)

			if (s == eocsignal) {
				ASSERT_TRUE(tile_x(i, 1, 0).find_object(mo_roadsign) != null) // for end-of-choose
			}
			else {
				ASSERT_TRUE(tile_x(i, 1, 0).find_object(mo_signal) != null)
			}
		}

		ASSERT_WAY_PATTERN(wt_rail, coord3d(0, 0, 0), s_2way)

		// 1-way signal drops S, leaving N=16 (eocsignal column 5 stays full).
		ASSERT_WAY_PATTERN_MASKED(wt_rail, coord3d(0, 0, 0),
			[
				[2,  2,  2,  2,  2,  2,  0, 0],
				[16, 16, 16, 16, 16, 18, 0, 0],
				[16, 16, 16, 16, 16, 16, 0, 0],
				[0, 0, 0, 0, 0, 0, 0, 0],
				[0, 0, 0, 0, 0, 0, 0, 0],
				[0, 0, 0, 0, 0, 0, 0, 0],
				[0, 0, 0, 0, 0, 0, 0, 0],
				[0, 0, 0, 0, 0, 0, 0, 0],
			])
	}

	// signals into different direction, dir 2
	{
		foreach (i, s in all_signals) {
			ASSERT_EQUAL(command_x.build_sign_at(pl, coord3d(i, 1, 0), s), null)

			if (s == eocsignal) {
				ASSERT_TRUE(tile_x(i, 1, 0).find_object(mo_roadsign) != null) // for end-of-choose
			}
			else {
				ASSERT_TRUE(tile_x(i, 1, 0).find_object(mo_signal) != null)
			}
		}

		ASSERT_WAY_PATTERN(wt_rail, coord3d(0, 0, 0), s_2way)

		// the next re-click flips to the other 1-way state, dropping N
		// and leaving S=2.
		ASSERT_WAY_PATTERN_MASKED(wt_rail, coord3d(0, 0, 0),
			[
				[2,  2,  2,  2,  2,  2,  0, 0],
				[2,  2,  2,  2,  2,  18, 0, 0],
				[16, 16, 16, 16, 16, 16, 0, 0],
				[0, 0, 0, 0, 0, 0, 0, 0],
				[0, 0, 0, 0, 0, 0, 0, 0],
				[0, 0, 0, 0, 0, 0, 0, 0],
				[0, 0, 0, 0, 0, 0, 0, 0],
				[0, 0, 0, 0, 0, 0, 0, 0],
			])
	}

	// build way across signal
	{
		ASSERT_EQUAL(command_x.build_way(pl, coord3d(0, 1, 0), coord3d(all_signals.len()-1, 1, 0), rail, true), null)

		// the y=1 row now runs along +x (SE axis), so each (i,1) is a
		// junction: N|S vertical plus the horizontal SE/NW.  Left end
		// (x=0) has SE only, right end (x=5) NW only.
		ASSERT_WAY_PATTERN(wt_rail, coord3d(0, 0, 0),
			[
				[2,  2,  2,  2,  2,  2,  0, 0],
				[19, 27, 27, 27, 27, 26, 0, 0],
				[16, 16, 16, 16, 16, 16, 0, 0],
				[0, 0, 0, 0, 0, 0, 0, 0],
				[0, 0, 0, 0, 0, 0, 0, 0],
				[0, 0, 0, 0, 0, 0, 0, 0],
				[0, 0, 0, 0, 0, 0, 0, 0],
				[0, 0, 0, 0, 0, 0, 0, 0],
			])

		// the 1-way signals (facing S, from the previous block) drop N
		// on the junction; the horizontal axis stays open.
		ASSERT_WAY_PATTERN_MASKED(wt_rail, coord3d(0, 0, 0),
			[
				[2,  2,  2,  2,  2,  2,  0, 0],
				[3,  11, 11, 11, 11, 26, 0, 0],
				[16, 16, 16, 16, 16, 16, 0, 0],
				[0, 0, 0, 0, 0, 0, 0, 0],
				[0, 0, 0, 0, 0, 0, 0, 0],
				[0, 0, 0, 0, 0, 0, 0, 0],
				[0, 0, 0, 0, 0, 0, 0, 0],
				[0, 0, 0, 0, 0, 0, 0, 0],
			])
	}

	// remove everything again — clear the signs first: way removal
	// routes along the masked ribi, so it cannot pass a 1-way signal
	// against its facing (here the columns face S, the removal runs N).
	{
		foreach (i, s in all_signals) {
			ASSERT_EQUAL(remover.work(pl, coord3d(i, 1, 0)), null)
		}

		foreach (i, s in all_signals) {
			ASSERT_EQUAL(wayremover.work(pl, coord3d(i, 2, 0), coord3d(i, 0, 0), "" + wt_rail), null)
		}

		ASSERT_EQUAL(wayremover.work(pl, coord3d(0, 1, 0), coord3d(all_signals.len()-1, 1, 0), "" + wt_rail), null)
	}

	// clean up
	RESET_ALL_PLAYER_FUNDS()
}


function test_sign_build_signal_multiple()
{
	local pl = player_x(0)
	local rail = way_desc_x.get_available_ways(wt_rail, st_flat)[0]
	local signal = sign_desc_x.get_available_signs(wt_rail).filter(@(idx, sign) sign.is_signal())[0]

	// starts on way end, not possible
	{
		ASSERT_EQUAL(command_x.build_way(pl, coord3d(0, 2, 0), coord3d(7, 2, 0), rail, true), null)

		ASSERT_EQUAL(command_x(tool_build_roadsign).work(pl, coord3d(0, 2, 0), coord3d(7, 2, 0), signal.get_name()),
			"Hier kann kein\nSignal aufge-\nstellt werden!\n")

		for (local x = 2; x <= 7; ++x) {
			ASSERT_EQUAL(tile_x(x, 2, 0).find_object(mo_signal), null)
		}

		// straight rail along +x is the SE axis: SE=1 at the low-x end,
		// NW=8 at the high-x end, SE|NW=9 through the middle.
		ASSERT_WAY_PATTERN_MASKED(wt_rail, coord3d(0, 0, 0),
			[
				[0, 0, 0, 0, 0, 0, 0, 0],
				[0, 0, 0, 0, 0, 0, 0, 0],
				[1, 9, 9, 9, 9, 9, 9, 8],
				[0, 0, 0, 0, 0, 0, 0, 0],
				[0, 0, 0, 0, 0, 0, 0, 0],
				[0, 0, 0, 0, 0, 0, 0, 0],
				[0, 0, 0, 0, 0, 0, 0, 0],
				[0, 0, 0, 0, 0, 0, 0, 0],
			])

		ASSERT_EQUAL(command_x(tool_remove_way).work(pl, coord3d(0, 2, 0), coord3d(7, 2, 0), "" + wt_rail), null)
	}

	// FIXME click-and-drag for tools using script is not supported
	{
		ASSERT_EQUAL(command_x.build_way(pl, coord3d(0, 2, 0), coord3d(7, 2, 0), rail, true), null)

		local error_caught = false
		try {
			command_x(tool_build_roadsign).work(pl, coord3d(1, 2, 0), coord3d(6, 2, 0), signal.get_name())
		}
		catch (e) {
			ASSERT_EQUAL(e, "First click has side effects")
			error_caught = true
		}
		ASSERT_EQUAL(error_caught, true)

		ASSERT_EQUAL(command_x(tool_remove_way).work(pl, coord3d(0, 2, 0), coord3d(7, 2, 0), "" + wt_rail), null)
	}

	RESET_ALL_PLAYER_FUNDS()
}


function test_sign_replace_signal()
{
	local pl = player_x(0)
	local rail = way_desc_x.get_available_ways(wt_rail, st_flat)[0]
	local signal = sign_desc_x.get_available_signs(wt_rail).filter(@(idx, sign) sign.is_signal())[0]
	local presignal = sign_desc_x.get_available_signs(wt_rail).filter(@(idx, sign) sign.is_pre_signal())[0]

	// preconditions
	ASSERT_TRUE(signal != null)
	ASSERT_TRUE(presignal != null)
	ASSERT_TRUE(rail != null)

	ASSERT_EQUAL(command_x.build_way(pl, coord3d(4, 3, 0), coord3d(6, 3, 0), rail, true), null)
	ASSERT_EQUAL(command_x(tool_build_roadsign).work(pl, coord3d(5, 3, 0), signal.name), null)

	// replace two-directional signals
	{
		local old_cash = pl.current_cash * 100
		local old_maint = pl.current_maintenance
		command_x(tool_build_roadsign).work(pl, coord3d(5, 3, 0), presignal.name)

		local s = sign_x(5, 3, 0)

		ASSERT_TRUE(s != null)
		ASSERT_EQUAL(s.desc.name, presignal.name)
		ASSERT_EQUAL(pl.current_cash * 100,   old_cash  - signal.cost        - presignal.cost)
		ASSERT_EQUAL(pl.current_maintenance,  old_maint + signal.maintenance - presignal.maintenance)
		// rail along +x is the SE axis (SE=1, NW=8, SE|NW=9); a 2-way
		// signal leaves both directions in the masked pattern.
		ASSERT_WAY_PATTERN_MASKED(wt_rail, coord3d(0, 0, 0),
			[
				[0, 0, 0, 0, 0, 0, 0, 0],
				[0, 0, 0, 0, 0, 0, 0, 0],
				[0, 0, 0, 0, 0, 0, 0, 0],
				[0, 0, 0, 0, 1, 9, 8, 0],
				[0, 0, 0, 0, 0, 0, 0, 0],
				[0, 0, 0, 0, 0, 0, 0, 0],
				[0, 0, 0, 0, 0, 0, 0, 0],
				[0, 0, 0, 0, 0, 0, 0, 0],
			])
	}

	// make 2-way signal 1-way
	ASSERT_EQUAL(command_x(tool_build_roadsign).work(pl, coord3d(5, 3, 0), presignal.name), null)

	// and one-directional signals
	{
		local old_cash = pl.current_cash * 100
		local old_maint = pl.current_maintenance

		command_x(tool_build_roadsign).work(pl, coord3d(5, 3, 0), signal.name)

		local s = sign_x(5, 3 , 0)

		ASSERT_TRUE(s != null)
		ASSERT_EQUAL(s.desc.name, signal.name)
		ASSERT_EQUAL(pl.current_cash * 100,   old_cash  - signal.cost        - presignal.cost)
		ASSERT_EQUAL(pl.current_maintenance,  old_maint - signal.maintenance + presignal.maintenance)
		// a 1-way signal drops the SE direction on the tile it sits on,
		// leaving NW=8.
		ASSERT_WAY_PATTERN_MASKED(wt_rail, coord3d(0, 0, 0),
			[
				[0, 0, 0, 0, 0, 0, 0, 0],
				[0, 0, 0, 0, 0, 0, 0, 0],
				[0, 0, 0, 0, 0, 0, 0, 0],
				[0, 0, 0, 0, 1, 8, 8, 0],
				[0, 0, 0, 0, 0, 0, 0, 0],
				[0, 0, 0, 0, 0, 0, 0, 0],
				[0, 0, 0, 0, 0, 0, 0, 0],
				[0, 0, 0, 0, 0, 0, 0, 0],
			])
	}

	// clean up
	ASSERT_EQUAL(command_x(tool_remove_way).work(pl, coord3d(6, 3, 0), coord3d(4, 3, 0), "" + wt_rail), null)
	RESET_ALL_PLAYER_FUNDS()
}


function test_sign_signal_when_player_removed()
{
	local pl = player_x(0)
	local public_pl = player_x(1)
	local rail = way_desc_x.get_available_ways(wt_rail, st_flat)[0]
	local signal = sign_desc_x.get_available_signs(wt_rail).filter(@(idx, sign) sign.is_signal())[0]

	ASSERT_EQUAL(command_x.build_way(pl, coord3d(4, 3, 0), coord3d(6, 3, 0), rail, true), null)
	ASSERT_EQUAL(command_x(tool_build_roadsign).work(public_pl, coord3d(5, 3, 0), signal.get_name()), null)

	{
		world.remove_player(pl)
		ASSERT_EQUAL(tile_x(5, 3, 0).find_object(mo_way), null)
		ASSERT_TRUE(tile_x(5, 3, 0).find_object(mo_signal) != null)
	}

	ASSERT_EQUAL(command_x(tool_remover).work(public_pl, coord3d(5, 3, 0)), null)
	ASSERT_TRUE(tile_x(5, 3, 0).find_object(mo_signal) == null)

	RESET_ALL_PLAYER_FUNDS()
}
