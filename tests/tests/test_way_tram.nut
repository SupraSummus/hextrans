//
// This file is part of the Simutrans project under the Artistic License.
// (see LICENSE.txt)
//


//
// Tests for tramways
//

function test_way_tram_build_flat()
{
	local pl = player_x(0)
	local tramway = way_desc_x.get_available_ways(wt_rail, st_tram)[0]

	// build straight
	{
		ASSERT_EQUAL(command_x.build_way(pl, coord3d(3, 3, 0), coord3d(3, 5, 0), tramway, true), null)

		ASSERT_EQUAL(command_x(tool_remove_way).work(pl, coord3d(3, 3, 0), coord3d(3, 5, 0), "" + wt_rail), null)
	}

	RESET_ALL_PLAYER_FUNDS()
}


function test_way_tram_build_parallel()
{
	local pl   = player_x(0)
	local rail_desc = way_desc_x.get_available_ways(wt_rail, st_tram)[0]
	local remover = command_x(tool_remove_way)

	local straight_se = [
		[1, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 8],
		[1, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 8],
		[1, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 8],
		[1, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 8],
		[1, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 8],
		[1, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 8],
		[1, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 8],
		[1, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 8],
		[1, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 8],
		[1, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 8],
		[1, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 8],
		[1, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 8],
		[1, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 8],
		[1, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 8],
		[1, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 8],
		[1, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 8],
	]

	// Same straight-chord pattern with and without ctrl: ctrl forces it
	// trivially, ctrl-free relies on prefer_parallel in `schiene_tram`.
	foreach (straight in [true, false]) {
		for (local i = 0; i < 16; ++i) {
			ASSERT_EQUAL(command_x.build_way(pl, coord3d(0, i, 0), coord3d(15, i, 0), rail_desc, straight), null)
		}
		ASSERT_WAY_PATTERN(wt_rail, coord3d(0, 0, 0), straight_se)
		for (local i = 0; i < 16; ++i) {
			ASSERT_EQUAL(remover.work(pl, coord3d(0, i, 0), coord3d(15, i, 0), "" + wt_rail), null)
		}
		RESET_ALL_PLAYER_FUNDS()
	}
}


// Tram laid along the S axis on top of an existing N-S road, then a
// crossing case where the tram runs along the SE axis through the
// road's middle tile.
function test_way_tram_build_on_road()
{
	local pl = player_x(0)
	local tramway = way_desc_x.get_available_ways(wt_rail, st_tram)[0]
	local road = way_desc_x.get_available_ways(wt_road, st_flat)[0]

	ASSERT_EQUAL(command_x.build_way(pl, coord3d(3, 3, 0), coord3d(3, 5, 0), road, true), null)

	// Column at x=3, y=3..5 along S axis: S=2, N|S=18, N=16.
	local ns_col = [
		[0, 0, 0,  0, 0, 0, 0, 0],
		[0, 0, 0,  0, 0, 0, 0, 0],
		[0, 0, 0,  0, 0, 0, 0, 0],
		[0, 0, 0,  2, 0, 0, 0, 0],
		[0, 0, 0, 18, 0, 0, 0, 0],
		[0, 0, 0, 16, 0, 0, 0, 0],
		[0, 0, 0,  0, 0, 0, 0, 0],
		[0, 0, 0,  0, 0, 0, 0, 0],
	]
	local empty_8x8 = [
		[0, 0, 0, 0, 0, 0, 0, 0],
		[0, 0, 0, 0, 0, 0, 0, 0],
		[0, 0, 0, 0, 0, 0, 0, 0],
		[0, 0, 0, 0, 0, 0, 0, 0],
		[0, 0, 0, 0, 0, 0, 0, 0],
		[0, 0, 0, 0, 0, 0, 0, 0],
		[0, 0, 0, 0, 0, 0, 0, 0],
		[0, 0, 0, 0, 0, 0, 0, 0],
	]

	// build fully on existing road
	{
		ASSERT_EQUAL(command_x.build_way(pl, coord3d(3, 3, 0), coord3d(3, 5, 0), tramway, true), null)
		ASSERT_WAY_PATTERN(wt_road, coord3d(0, 0, 0), ns_col)
		ASSERT_WAY_PATTERN(wt_rail, coord3d(0, 0, 0), ns_col)

		ASSERT_EQUAL(command_x(tool_remove_way).work(pl, coord3d(3, 3, 0), coord3d(3, 5, 0), "" + wt_rail), null)
	}

	// cross existing road
	{
		ASSERT_EQUAL(command_x.build_way(pl, coord3d(2, 4, 0), coord3d(4, 4, 0), tramway, true), null)

		ASSERT_WAY_PATTERN(wt_road, coord3d(0, 0, 0), ns_col)

		// Row at y=4, x=2..4 along SE axis: SE=1, SE|NW=9, NW=8.
		ASSERT_WAY_PATTERN(wt_rail, coord3d(0, 0, 0),
			[
				[0, 0, 0, 0, 0, 0, 0, 0],
				[0, 0, 0, 0, 0, 0, 0, 0],
				[0, 0, 0, 0, 0, 0, 0, 0],
				[0, 0, 0, 0, 0, 0, 0, 0],
				[0, 0, 1, 9, 8, 0, 0, 0],
				[0, 0, 0, 0, 0, 0, 0, 0],
				[0, 0, 0, 0, 0, 0, 0, 0],
				[0, 0, 0, 0, 0, 0, 0, 0],
			])

		ASSERT_EQUAL(command_x(tool_remove_way).work(pl, coord3d(2, 4, 0), coord3d(4, 4, 0), "" + wt_rail), null)
	}

	ASSERT_EQUAL(command_x(tool_remove_way).work(pl, coord3d(3, 3, 0), coord3d(3, 5, 0), "" + wt_road), null)
	ASSERT_WAY_PATTERN(wt_road, coord3d(0, 0, 0), empty_8x8)
	RESET_ALL_PLAYER_FUNDS()
}


// Bridge runs N-S between two narrow-edge ramps; the column at z=1
// carries S=2 / N|S=18 / N=16 (north ramp, span, south ramp), each
// end-tile reporting only the direction pointing into the bridge.
function test_way_tram_build_across_road_bridge()
{
	local pl = player_x(0)
	local bridge = bridge_desc_x.get_available_bridges(wt_road)[0]
	local setslope = command_x.set_slope
	local tramway = way_desc_x.get_available_ways(wt_rail, st_tram)[0]

	// build bridge
	ASSERT_EQUAL(setslope(pl, coord3d(3, 3, 0), slope.south_narrow), null)
	ASSERT_EQUAL(setslope(pl, coord3d(3, 5, 0), slope.north_narrow), null)
	ASSERT_EQUAL(command_x.build_bridge_at(pl, coord3d(3, 3, 0), bridge), null)

	local bridge_col = [
		[0, 0, 0,  0, 0, 0, 0, 0],
		[0, 0, 0,  0, 0, 0, 0, 0],
		[0, 0, 0,  0, 0, 0, 0, 0],
		[0, 0, 0,  2, 0, 0, 0, 0],
		[0, 0, 0, 18, 0, 0, 0, 0],
		[0, 0, 0, 16, 0, 0, 0, 0],
		[0, 0, 0,  0, 0, 0, 0, 0],
		[0, 0, 0,  0, 0, 0, 0, 0],
	]

	// and build tram track
	{
		ASSERT_EQUAL(command_x.build_way(pl, coord3d(3, 3, 0), coord3d(3, 5, 0), tramway, true), null)
		ASSERT_WAY_PATTERN(wt_road, coord3d(0, 0, 1), bridge_col)
		ASSERT_WAY_PATTERN(wt_rail, coord3d(0, 0, 1), bridge_col)

		// do not remove tram tracks here, this is done by tool_remover below
	}

	ASSERT_EQUAL(command_x(tool_remover).work(pl, coord3d(3, 3, 0)), null)
	ASSERT_EQUAL(setslope(pl, coord3d(3, 3, 0), slope.flat), null)
	ASSERT_EQUAL(setslope(pl, coord3d(3, 5, 0), slope.flat), null)

	RESET_ALL_PLAYER_FUNDS()
}


function test_way_tram_build_across_crossing()
{
	local pl = player_x(0)

	local tramways = way_desc_x.get_available_ways(wt_rail, st_tram)
	tramways.sort(@(a, b) a.get_topspeed() <=> b.get_topspeed())
	local tramway = tramways[0]

	local roads = way_desc_x.get_available_ways(wt_road, st_flat)
	roads.sort(@(a, b) a.get_topspeed() <=> b.get_topspeed())
	local road = roads[0]

	local rails = way_desc_x.get_available_ways(wt_rail, st_flat)
	rails.sort(@(a, b) a.get_topspeed() <=> b.get_topspeed())
	rails.filter(@(idx, desc) desc.get_topspeed() >= tramway.get_topspeed())
	local rail = rails[0]

	// build crossing
	ASSERT_EQUAL(command_x.build_way(pl, coord3d(3, 3, 0), coord3d(3, 5, 0), rail, true), null)
	ASSERT_EQUAL(command_x.build_way(pl, coord3d(2, 4, 0), coord3d(4, 4, 0), road, true), null)

	// build tram track
	{
		ASSERT_EQUAL(command_x.build_way(pl, coord3d(2, 4, 0), coord3d(4, 4, 0), tramway, true), null)

		// crossing should have been replaced by tramway

		ASSERT_EQUAL(command_x(tool_remove_way).work(pl, coord3d(2, 4, 0), coord3d(4, 4, 0), "" + wt_rail), null)
	}

	ASSERT_EQUAL(command_x(tool_remove_way).work(pl, coord3d(3, 3, 0), coord3d(3, 5, 0), "" + wt_rail), null)
	ASSERT_EQUAL(command_x(tool_remove_way).work(pl, coord3d(2, 4, 0), coord3d(4, 4, 0), "" + wt_road), null)
	RESET_ALL_PLAYER_FUNDS()
}


// 1-tile hex hill at (3,2): road tunnel S-axis through (3,1)<->(3,3),
// tram crosses via ctrl-dug rail-tunnel mouths on the SE-axis at
// (2,2)<->(4,2).  Buried tile (3,2) carries road N|S=18 and, after
// the cross, tram N|S|NW|SE=27.
function test_way_tram_build_in_tunel()
{
	local pl = player_x(0)
	local tramway = way_desc_x.get_available_ways(wt_rail, st_tram)[0]
	local road_tunnel = tunnel_desc_x.get_available_tunnels(wt_road)[0]
	local rail_tunnel = tunnel_desc_x.get_available_tunnels(wt_rail)[0]

	raise_hex_tile(pl, 3, 2, 0)

	// road tunnel auto-finds end: (3,1) mouth S=2, (3,2) buried N|S=18, (3,3) mouth N=16
	ASSERT_EQUAL(command_x(tool_build_tunnel).work(pl, coord3d(3, 1, 0), road_tunnel.get_name()), null)

	local s_axis_col = [
		[0, 0, 0,  0, 0, 0, 0, 0],
		[0, 0, 0,  2, 0, 0, 0, 0],
		[0, 0, 0, 18, 0, 0, 0, 0],
		[0, 0, 0, 16, 0, 0, 0, 0],
		[0, 0, 0,  0, 0, 0, 0, 0],
		[0, 0, 0,  0, 0, 0, 0, 0],
		[0, 0, 0,  0, 0, 0, 0, 0],
		[0, 0, 0,  0, 0, 0, 0, 0],
	]

	local empty_8x8 = [
		[0, 0, 0, 0, 0, 0, 0, 0],
		[0, 0, 0, 0, 0, 0, 0, 0],
		[0, 0, 0, 0, 0, 0, 0, 0],
		[0, 0, 0, 0, 0, 0, 0, 0],
		[0, 0, 0, 0, 0, 0, 0, 0],
		[0, 0, 0, 0, 0, 0, 0, 0],
		[0, 0, 0, 0, 0, 0, 0, 0],
		[0, 0, 0, 0, 0, 0, 0, 0],
	]

	ASSERT_WAY_PATTERN(wt_road, coord3d(0, 0, 0), s_axis_col)
	ASSERT_TRUE(tile_x(3, 1, 0).find_object(mo_tunnel) != null)
	ASSERT_TRUE(tile_x(3, 3, 0).find_object(mo_tunnel) != null)

	// build tramway through road tunnel: tram coexists with road on
	// both mouths and the buried tile.
	{
		ASSERT_EQUAL(command_x.build_way(pl, coord3d(3, 1, 0), coord3d(3, 3, 0), tramway, true), null)
		ASSERT_WAY_PATTERN(wt_rail, coord3d(0, 0, 0), s_axis_col)
		ASSERT_WAY_PATTERN(wt_road, coord3d(0, 0, 0), s_axis_col)

		// remove tram only: road tunnel and its mouths stay intact.
		ASSERT_EQUAL(command_x(tool_remove_way).work(pl, coord3d(3, 1, 0), coord3d(3, 3, 0), "" + wt_rail), null)
		ASSERT_WAY_PATTERN(wt_rail, coord3d(0, 0, 0), empty_8x8)
		ASSERT_WAY_PATTERN(wt_road, coord3d(0, 0, 0), s_axis_col)
		ASSERT_TRUE(tile_x(3, 1, 0).find_object(mo_tunnel) != null)
		ASSERT_TRUE(tile_x(3, 3, 0).find_object(mo_tunnel) != null)
	}

	// cross road tunnel with tramway via ctrl-dug rail-tunnel mouths
	// on the SE-axis.  (2,2) and (4,2) sit on the SE/NW edges of the
	// hill at (3,2); ctrl-dig leaves them as isolated ribi=0 mouths
	// until the tram-build joins them through the buried tile.
	{
		local tunnel_builder = command_x(tool_build_tunnel)
		tunnel_builder.set_flags(2) // ctrl
		ASSERT_EQUAL(command_x.build_way(pl, coord3d(3, 1, 0), coord3d(3, 3, 0), tramway, true), null)
		ASSERT_EQUAL(tunnel_builder.work(pl, coord3d(2, 2, 0), rail_tunnel.get_name()), null)
		ASSERT_EQUAL(tunnel_builder.work(pl, coord3d(4, 2, 0), rail_tunnel.get_name()), null)
		tunnel_builder.set_flags(0)

		// ctrl-dug rail mouths have ribi=0 at (2,2) and (4,2); the
		// tram in the road tunnel is unchanged on the S-axis column;
		// the road tunnel itself is also unchanged.
		ASSERT_WAY_PATTERN(wt_rail, coord3d(0, 0, 0), s_axis_col)
		ASSERT_WAY_PATTERN(wt_road, coord3d(0, 0, 0), s_axis_col)
		ASSERT_TRUE(tile_x(2, 2, 0).find_object(mo_tunnel) != null)
		ASSERT_TRUE(tile_x(4, 2, 0).find_object(mo_tunnel) != null)

		// connect the rail-tunnel mouths through the buried tile.
		// (2,2) mouth gets ribi SE=1, (4,2) mouth gets ribi NW=8;
		// (3,2) buried tile gains the NW-SE cross on top of N-S, so
		// tram ribi = 16|2|8|1 = 27.
		ASSERT_EQUAL(command_x.build_way(pl, coord3d(2, 2, 0), coord3d(4, 2, 0), tramway, true), null)
		ASSERT_WAY_PATTERN(wt_rail, coord3d(0, 0, 0),
			[
				[0, 0, 0,  0, 0, 0, 0, 0],
				[0, 0, 0,  2, 0, 0, 0, 0],
				[0, 0, 1, 27, 8, 0, 0, 0],
				[0, 0, 0, 16, 0, 0, 0, 0],
				[0, 0, 0,  0, 0, 0, 0, 0],
				[0, 0, 0,  0, 0, 0, 0, 0],
				[0, 0, 0,  0, 0, 0, 0, 0],
				[0, 0, 0,  0, 0, 0, 0, 0],
			])
		// road tunnel buried tile unchanged on its own waytype.
		ASSERT_WAY_PATTERN(wt_road, coord3d(0, 0, 0), s_axis_col)

		// remove tram cross: rail-tunnel mouths come out with the way
		// (their only inhabitant), buried-tile tram drops back to N|S.
		ASSERT_EQUAL(command_x(tool_remove_way).work(pl, coord3d(2, 2, 0), coord3d(4, 2, 0), "" + wt_rail), null)
		ASSERT_WAY_PATTERN(wt_rail, coord3d(0, 0, 0), s_axis_col)
		ASSERT_WAY_PATTERN(wt_road, coord3d(0, 0, 0), s_axis_col)
		ASSERT_TRUE(tile_x(2, 2, 0).find_object(mo_tunnel) == null)
		ASSERT_TRUE(tile_x(4, 2, 0).find_object(mo_tunnel) == null)

		// remove tram from road tunnel: road tunnel mouths still stand.
		ASSERT_EQUAL(command_x(tool_remove_way).work(pl, coord3d(3, 1, 0), coord3d(3, 3, 0), "" + wt_rail), null)
		ASSERT_WAY_PATTERN(wt_rail, coord3d(0, 0, 0), empty_8x8)
		ASSERT_WAY_PATTERN(wt_road, coord3d(0, 0, 0), s_axis_col)
		ASSERT_TRUE(tile_x(3, 1, 0).find_object(mo_tunnel) != null)
		ASSERT_TRUE(tile_x(3, 3, 0).find_object(mo_tunnel) != null)
	}

	// remove road tunnel: mouths and buried way all gone.
	ASSERT_EQUAL(command_x(tool_remove_way).work(pl, coord3d(3, 1, 0), coord3d(3, 3, 0), "" + wt_road), null)
	ASSERT_WAY_PATTERN(wt_road, coord3d(0, 0, 0), empty_8x8)
	ASSERT_TRUE(tile_x(3, 1, 0).find_object(mo_tunnel) == null)
	ASSERT_TRUE(tile_x(3, 3, 0).find_object(mo_tunnel) == null)

	lower_hex_tile(pl, 3, 2, 0)
	RESET_ALL_PLAYER_FUNDS()
}
