//
// This file is part of the Simutrans project under the Artistic License.
// (see LICENSE.txt)
//


//
// Tests for bridge building / removal
//


// Flat-ground N-S-axis bridge: build (3,5,0)→(3,2,0), verify, remove.
// The three square-era "should fail" subcases (build over bridgehead,
// slope under bridge, bridge crossing) now build a stacked bridge
// under hex's art-permissive double-height policy; see
// `test_way_bridge_build_stacked` for the pinned behaviour.
function test_way_bridge_build_ground()
{
	local pl          = player_x(0)
	local bridge_desc = bridge_desc_x.get_available_bridges(wt_road)[0]
	local remover     = command_x(tool_remove_way)
	ASSERT_TRUE(bridge_desc != null)

	ASSERT_EQUAL(command_x.build_bridge(pl, coord3d(3, 5, 0), coord3d(3, 2, 0), bridge_desc), null)

	{
		local t = tile_x(3, 5, 0)
		local bridge = t.find_object(mo_bridge)

		ASSERT_TRUE(bridge != null)
		ASSERT_EQUAL(bridge.get_desc().get_name(), bridge_desc.get_name())
	}
	ASSERT_TRUE(tile_x(3, 2, 0).find_object(mo_bridge) != null)

	// z=1 spans both endpoints and the auto-extended ramp tile beyond
	// each end at (3,1) / (3,6).  S=2, N=16, S|N=18.
	ASSERT_WAY_PATTERN(wt_road, coord3d(0, 0, 1),
		[
			[0, 0, 0,  0, 0, 0, 0, 0],
			[0, 0, 0,  2, 0, 0, 0, 0],
			[0, 0, 0, 18, 0, 0, 0, 0],
			[0, 0, 0, 18, 0, 0, 0, 0],
			[0, 0, 0, 18, 0, 0, 0, 0],
			[0, 0, 0, 18, 0, 0, 0, 0],
			[0, 0, 0, 16, 0, 0, 0, 0],
			[0, 0, 0,  0, 0, 0, 0, 0],
		])

	ASSERT_EQUAL(remover.work(pl, tile_x(3, 1, 0), tile_x(3, 6, 0), "" + wt_road), null)
	ASSERT_WAY_PATTERN(wt_road, coord3d(0, 0, 0),
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
	RESET_ALL_PLAYER_FUNDS()
}


// Start ramp pre-set to slope.south_narrow (low edge S, build extends
// toward S).  Five failure subcases on the end slope, two success
// subcases (flat-cliff at z=1, matching north_narrow ramp).
function test_way_bridge_build_at_slope()
{
	local start_pos = coord3d(2, 1, 0)
	local end_pos = coord3d(2, 6, 0)
	local remover = command_x(tool_remove_way)
	local setslope = command_x.set_slope
	local pl = player_x(0)
	local bridge_desc = bridge_desc_x.get_available_bridges(wt_road)[0]

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

	// Bridge span at z=1, column x=2.  S=2, N=16, S|N=18.
	local span_z1 = [
		[0, 0,  0, 0, 0, 0, 0, 0],
		[0, 0,  2, 0, 0, 0, 0, 0],
		[0, 0, 18, 0, 0, 0, 0, 0],
		[0, 0, 18, 0, 0, 0, 0, 0],
		[0, 0, 18, 0, 0, 0, 0, 0],
		[0, 0, 18, 0, 0, 0, 0, 0],
		[0, 0, 16, 0, 0, 0, 0, 0],
		[0, 0,  0, 0, 0, 0, 0, 0],
	]

	ASSERT_EQUAL(setslope(pl, start_pos, slope.south_narrow), null)

	{
		// down slope: no matching ramp within range
		local err = command_x.build_bridge_at(pl, start_pos, bridge_desc)
		ASSERT_EQUAL(err, "Bridge is too long for this type!\n")
		ASSERT_WAY_PATTERN(wt_road, coord3d(0, 0, 1), empty_8x8)
	}

	{
		// wrong single-height slope (legacy square diagonal `west`,
		// admitted as a way slope but not a hex axis the bridge can end on)
		ASSERT_EQUAL(setslope(pl, end_pos, slope.west), null)
		local err = command_x.build_bridge(pl, start_pos, end_pos, bridge_desc)
		ASSERT_EQUAL(err, "")
		ASSERT_WAY_PATTERN(wt_road, coord3d(0, 0, 1), empty_8x8)
	}

	{
		// planar double-height slope: north_double (122100); bridge is
		// single-height so it cannot land on a double-height ramp
		ASSERT_EQUAL(setslope(pl, end_pos, HEX_SLOPE(1, 2, 2, 1, 0, 0)), null)
		local err = command_x.build_bridge(pl, start_pos, end_pos, bridge_desc)
		ASSERT_EQUAL(err, "")
		ASSERT_WAY_PATTERN(wt_road, coord3d(0, 0, 1), empty_8x8)
	}

	{
		// off-axis planar double-height slope: northeast_double (012210)
		ASSERT_EQUAL(setslope(pl, end_pos, HEX_SLOPE(0, 1, 2, 2, 1, 0)), null)
		local err = command_x.build_bridge(pl, start_pos, end_pos, bridge_desc)
		ASSERT_EQUAL(err, "")
		ASSERT_WAY_PATTERN(wt_road, coord3d(0, 0, 1), empty_8x8)
	}

	{
		// 2-step flat cliff at end: too high for single-height bridge
		ASSERT_EQUAL(setslope(pl, end_pos, slope.all_up_slope), null)
		ASSERT_EQUAL(setslope(pl, end_pos + coord3d(0, 0, 1), slope.all_up_slope), null)
		local err = command_x.build_bridge_at(pl, start_pos, bridge_desc)
		ASSERT_EQUAL(err, "Bridge is too long for this type!\n")
		ASSERT_WAY_PATTERN(wt_road, coord3d(0, 0, 1), empty_8x8)

		ASSERT_EQUAL(setslope(pl, end_pos + coord3d(0, 0, 2), slope.all_down_slope), null)
		ASSERT_EQUAL(setslope(pl, end_pos + coord3d(0, 0, 1), slope.all_down_slope), null)
	}

	{
		// flat cliff at z=1: end tile sits at z=1, bridge lands on it flat
		ASSERT_EQUAL(setslope(pl, end_pos, slope.all_up_slope), null)
		local err = command_x.build_bridge_at(pl, start_pos, bridge_desc)
		ASSERT_EQUAL(err, null)
		ASSERT_TRUE(tile_x(2, 1, 0).find_object(mo_bridge) != null)
		ASSERT_WAY_PATTERN(wt_road, coord3d(0, 0, 1), span_z1)

		ASSERT_EQUAL(remover.work(pl, tile_x(2, 1, 0), tile_x(2, 6, 1), "" + wt_road), null)
		ASSERT_EQUAL(setslope(pl, end_pos + coord3d(0, 0, 1), slope.all_down_slope), null)
	}

	{
		// matching ramp slope at end: north_narrow (low edge N)
		ASSERT_EQUAL(setslope(pl, end_pos, slope.north_narrow), null)
		local err = command_x.build_bridge_at(pl, start_pos, bridge_desc)
		ASSERT_EQUAL(err, null)
		ASSERT_TRUE(tile_x(2, 1, 0).find_object(mo_bridge) != null)
		ASSERT_TRUE(tile_x(2, 6, 0).find_object(mo_bridge) != null)
		ASSERT_WAY_PATTERN(wt_road, coord3d(0, 0, 1), span_z1)

		ASSERT_EQUAL(remover.work(pl, tile_x(2, 1, 0), tile_x(2, 6, 0), "" + wt_road), null)
		ASSERT_EQUAL(setslope(pl, end_pos, slope.flat), null)
	}

	ASSERT_EQUAL(setslope(pl, start_pos, slope.flat), null)
	RESET_ALL_PLAYER_FUNDS()
}


// test_way_bridge_build_at_slope_stacked: HEX-PORT PENDING.
function test_way_bridge_build_at_slope_stacked()
{
	local remover = command_x(tool_remove_way)
	local setslope = command_x.set_slope
	local pl = player_x(0)
	local bridge_desc = bridge_desc_x.get_available_bridges(wt_road)[0]

	{
		ASSERT_EQUAL(setslope(pl, coord3d(3, 2, 0), slope.south_narrow), null)
		ASSERT_EQUAL(setslope(pl, coord3d(3, 5, 0), slope.north_narrow), null)

		ASSERT_EQUAL(command_x.build_bridge_at(pl, coord3d(3, 2, 0), bridge_desc), null)

		ASSERT_WAY_PATTERN(wt_road, coord3d(0, 0, 1),
			[
				"........",
				"........",
				"...4....",
				"...5....",
				"...5....",
				"...1....",
				"........",
				"........"
			])


		// second bridge layer
		ASSERT_EQUAL(setslope(pl, coord3d(3, 1, 0), slope.all_up_slope), null)
		ASSERT_EQUAL(setslope(pl, coord3d(3, 1, 1), slope.south_narrow), null)
		ASSERT_EQUAL(setslope(pl, coord3d(3, 6, 0), slope.all_up_slope), null)
		ASSERT_EQUAL(setslope(pl, coord3d(3, 6, 1), slope.north_narrow), null)

		ASSERT_EQUAL(command_x.build_bridge_at(pl, coord3d(3, 1, 1), bridge_desc), null)

		ASSERT_WAY_PATTERN(wt_road, coord3d(0, 0, 1),
			[
				"........",
				"...4....",
				"...4....",
				"...5....",
				"...5....",
				"...1....",
				"...1....",
				"........"
			])

		ASSERT_WAY_PATTERN(wt_road, coord3d(0, 0, 2),
			[
				"........",
				"...4....",
				"...5....",
				"...5....",
				"...5....",
				"...5....",
				"...1....",
				"........"
			])
	}

	{
		// remove lower bridge and rebuild it
		ASSERT_EQUAL(remover.work(pl, coord3d(3, 2, 0), coord3d(3, 5, 0), "" + wt_road), null)

		ASSERT_EQUAL(command_x.build_bridge_at(pl, coord3d(3, 2, 0), bridge_desc), null)

		ASSERT_WAY_PATTERN(wt_road, coord3d(0, 0, 1),
			[
				"........",
				"...4....",
				"...4....",
				"...5....",
				"...5....",
				"...1....",
				"...1....",
				"........"
			])

		ASSERT_WAY_PATTERN(wt_road, coord3d(0, 0, 2),
			[
				"........",
				"...4....",
				"...5....",
				"...5....",
				"...5....",
				"...5....",
				"...1....",
				"........"
			])

		ASSERT_EQUAL(remover.work(pl, coord3d(3, 1, 1), coord3d(3, 6, 1), "" + wt_road), null)
		ASSERT_EQUAL(setslope(pl, coord3d(3, 1, 1), slope.flat), null)
		ASSERT_EQUAL(setslope(pl, coord3d(3, 6, 1), slope.flat), null)
		ASSERT_EQUAL(setslope(pl, coord3d(3, 1, 1), slope.all_down_slope), null)
		ASSERT_EQUAL(setslope(pl, coord3d(3, 6, 1), slope.all_down_slope), null)
	}

	{
		// second bridge layer
		ASSERT_EQUAL(setslope(pl, coord3d(1, 3, 0), slope.all_up_slope), null)
		ASSERT_EQUAL(setslope(pl, coord3d(1, 3, 1), slope.east), null)
		ASSERT_EQUAL(setslope(pl, coord3d(6, 3, 0), slope.all_up_slope), null)
		ASSERT_EQUAL(setslope(pl, coord3d(6, 3, 1), slope.west), null)

		ASSERT_EQUAL(command_x.build_bridge_at(pl, coord3d(1, 3, 1), bridge_desc), null)

		ASSERT_WAY_PATTERN(wt_road, coord3d(0, 0, 1),
			[
				"........",
				"........",
				"...4....",
				".2.5..8.",
				"...5....",
				"...1....",
				"........",
				"........"
			])

		ASSERT_WAY_PATTERN(wt_road, coord3d(0, 0, 2),
			[
				"........",
				"........",
				"...4....",
				".2AAAA8.",
				"........",
				"...1....",
				"........",
				"........"
			])
	}

	{
		// remove lower bridge and rebuild it
		ASSERT_EQUAL(remover.work(pl, coord3d(3, 5, 0), coord3d(3, 2, 0), "" + wt_road), null)
		ASSERT_EQUAL(command_x.build_bridge_at(pl, coord3d(3, 5, 0), bridge_desc), null)

		ASSERT_WAY_PATTERN(wt_road, coord3d(0, 0, 1),
			[
				"........",
				"........",
				"...4....",
				".2.5..8.",
				"...5....",
				"...1....",
				"........",
				"........"
			])

		ASSERT_WAY_PATTERN(wt_road, coord3d(0, 0, 2),
			[
				"........",
				"........",
				"...4....",
				".2AAAA8.",
				"........",
				"...1....",
				"........",
				"........"
			])

	}

	// clean up
	ASSERT_EQUAL(remover.work(pl, coord3d(3, 2, 0), coord3d(3, 5, 0), "" + wt_road), null)
	ASSERT_EQUAL(remover.work(pl, coord3d(1, 3, 1), coord3d(6, 3, 1), "" + wt_road), null)
	ASSERT_EQUAL(setslope(pl, coord3d(1, 3, 1), slope.all_down_slope), null)
	ASSERT_EQUAL(setslope(pl, coord3d(1, 3, 1), slope.all_down_slope), null)
	ASSERT_EQUAL(setslope(pl, coord3d(6, 3, 1), slope.all_down_slope), null)
	ASSERT_EQUAL(setslope(pl, coord3d(6, 3, 1), slope.all_down_slope), null)
	ASSERT_EQUAL(setslope(pl, coord3d(3, 2, 0), slope.all_down_slope), null)
	ASSERT_EQUAL(setslope(pl, coord3d(3, 5, 0), slope.all_down_slope), null)

	RESET_ALL_PLAYER_FUNDS()
}


// Stacked bridge crossing: lay a lower N-S bridge from flat ground
// with span at z=1, then build a crossing NW-SE bridge from flat
// ground.  The crossing must clear the lower bridge: h=0 fails (flat
// kartenboden at z=0 under the crossing's middle tile) and h=1 fails
// (lower bridge span at z=1), so the picker selects h=2 and the
// crossing ramps go delta-2 from flat ground at both ends.  Pins the
// `narrow_to_double` ramp arithmetic; the prior `slope_type(zv) * 2`
// produced a kinked-cliff slope with no sprite and tripped the
// `bruecke_t` constructor assertion.
function test_way_bridge_build_stacked()
{
	local pl = player_x(0)
	local bridge_desc = bridge_desc_x.get_available_bridges(wt_road)[0]
	ASSERT_TRUE(bridge_desc != null)

	// Lower N-S bridge, span at z=1.
	ASSERT_EQUAL(command_x.build_bridge(pl, coord3d(3, 2, 0), coord3d(3, 5, 0), bridge_desc), null)

	// Crossing NW-SE bridge from flat ground; span lands at z=2 over the lower bridge.
	ASSERT_EQUAL(command_x.build_bridge(pl, coord3d(2, 3, 0), coord3d(4, 3, 0), bridge_desc), null)
	ASSERT_TRUE(tile_x(3, 3, 2).find_object(mo_bridge) != null)

	// Cleanup is asymmetric: the lower bridge's delta-1 ramps trigger
	// the auto-extension at brueckenbauer.cc:905 (road stubs at
	// (3,1,0)/(3,6,0)), the crossing's delta-2 ramps don't (see
	// TODO.md → "build_bridge auto-extension misses delta-2 ramps").
	ASSERT_EQUAL(command_x(tool_remover).work(pl, coord3d(2, 3, 0)), null)
	local remover = command_x(tool_remove_way)
	ASSERT_EQUAL(remover.work(pl, coord3d(3, 1, 0), coord3d(3, 6, 0), "" + wt_road), null)
	RESET_ALL_PLAYER_FUNDS()
}


function test_way_bridge_build_flat_ground_nw_se()
{
	// Flat-ground bridge along the NW-SE axis (constant y), the
	// hex axis whose ramp slope path is exercised by neither the
	// N-S `_planner` nor the NE-SW `_planar_double_slope` cases.
	local pl = player_x(0)
	local bridge_desc = bridge_desc_x.get_available_bridges(wt_road)[0]
	local remover = command_x(tool_remove_way)
	ASSERT_TRUE(bridge_desc != null)

	ASSERT_EQUAL(command_x.build_bridge(pl, coord3d(2, 4, 0), coord3d(5, 4, 0), bridge_desc), null)
	ASSERT_TRUE(tile_x(2, 4, 0).find_object(mo_bridge) != null)
	ASSERT_TRUE(tile_x(5, 4, 0).find_object(mo_bridge) != null)

	ASSERT_EQUAL(remover.work(pl, tile_x(1, 4, 0), tile_x(6, 4, 0), "" + wt_road), null)
	RESET_ALL_PLAYER_FUNDS()
}


function test_way_bridge_build_at_planar_double_slope()
{
	local pl = player_x(0)
	local bridge_desc = bridge_desc_x.get_available_bridges(wt_road)[0]
	local setslope = command_x.set_slope

	ASSERT_TRUE(bridge_desc != null)

	local start = coord3d(4, 4, 0)
	local middle = coord3d(5, 3, 2)
	local end_base = coord3d(6, 2, 0)
	local end = coord3d(6, 2, 2)
	local start_slope = HEX_SLOPE(0, 1, 2, 2, 1, 0)

	// Active hex counterpart to the disabled double-slope coverage in
	// test_way_bridge_build_at_slope.  Corner heights E,SE,SW,W,NW,NE
	// = 012210: uphill is SW, so build_bridge_at should search and
	// build toward the low NE side.
	ASSERT_EQUAL(setslope(pl, start, start_slope), null)
	ASSERT_EQUAL(setslope(pl, end_base, slope.all_up_slope), null)
	ASSERT_EQUAL(setslope(pl, end_base + coord3d(0, 0, 1), slope.all_up_slope), null)

	ASSERT_EQUAL(command_x.build_bridge_at(pl, start, bridge_desc), null)
	ASSERT_TRUE(tile_x(middle.x, middle.y, middle.z).find_object(mo_bridge) != null)

	// clean up
	ASSERT_EQUAL(command_x(tool_remover).work(pl, start), null)
	ASSERT_EQUAL(setslope(pl, start, slope.flat), null)
	ASSERT_EQUAL(setslope(pl, end, slope.all_down_slope), null)
	ASSERT_EQUAL(setslope(pl, end_base + coord3d(0, 0, 1), slope.all_down_slope), null)
	RESET_ALL_PLAYER_FUNDS()
}


// S-axis road bridge crossing a SE-axis road built first underneath.
// The bridge ramps at y=2,5 along x=3; the road runs along y=3 on the
// SE-NW axis.  Both axes are distinct hex axes, so the two cross at 60°.
function test_way_bridge_build_above_way()
{
	local remover = command_x(tool_remove_way)
	local setslope = command_x.set_slope
	local pl = player_x(0)
	local bridge_desc = bridge_desc_x.get_available_bridges(wt_road)[0]
	local way_desc = way_desc_x.get_available_ways(wt_road, st_flat)[0]

	ASSERT_TRUE(bridge_desc != null)
	ASSERT_TRUE(way_desc != null)

	// At z=0: ramps at (3,2) S=2 and (3,5) N=16; ground road on
	// SE axis at y=3 with SE=1 / SE|NW=9 / NW=8.
	local ground = [
		[0, 0, 0,  0, 0, 0, 0, 0],
		[0, 0, 0,  0, 0, 0, 0, 0],
		[0, 0, 0,  2, 0, 0, 0, 0],
		[0, 0, 1,  9, 8, 0, 0, 0],
		[0, 0, 0,  0, 0, 0, 0, 0],
		[0, 0, 0, 16, 0, 0, 0, 0],
		[0, 0, 0,  0, 0, 0, 0, 0],
		[0, 0, 0,  0, 0, 0, 0, 0],
	]

	// At z=1: bridge span column x=3 from y=2..5.  S=2, N=16, N|S=18.
	local span = [
		[0, 0, 0,  0, 0, 0, 0, 0],
		[0, 0, 0,  0, 0, 0, 0, 0],
		[0, 0, 0,  2, 0, 0, 0, 0],
		[0, 0, 0, 18, 0, 0, 0, 0],
		[0, 0, 0, 18, 0, 0, 0, 0],
		[0, 0, 0, 16, 0, 0, 0, 0],
		[0, 0, 0,  0, 0, 0, 0, 0],
		[0, 0, 0,  0, 0, 0, 0, 0],
	]

	{
		ASSERT_EQUAL(setslope(pl, coord3d(3, 2, 0), slope.south_narrow), null)
		ASSERT_EQUAL(setslope(pl, coord3d(3, 5, 0), slope.north_narrow), null)

		ASSERT_EQUAL(command_x.build_way(pl, coord3d(2, 3, 0), coord3d(4, 3, 0), way_desc, true), null)
		ASSERT_EQUAL(command_x.build_bridge_at(pl, coord3d(3, 2, 0), bridge_desc), null)

		ASSERT_WAY_PATTERN(wt_road, coord3d(0, 0, 0), ground)
		ASSERT_WAY_PATTERN(wt_road, coord3d(0, 0, 1), span)
	}

	{
		// remove way under bridge, rebuild it.  Bridge span (and its
		// ramps) must survive the removal, and the rebuilt ground road
		// must come back in its original shape.
		ASSERT_EQUAL(remover.work(pl, coord3d(2, 3, 0), coord3d(4, 3, 0), "" + wt_road), null)
		ASSERT_WAY_PATTERN(wt_road, coord3d(0, 0, 1), span)
		ASSERT_EQUAL(command_x.build_way(pl, coord3d(2, 3, 0), coord3d(4, 3, 0), way_desc, true), null)
		ASSERT_WAY_PATTERN(wt_road, coord3d(0, 0, 0), ground)
		ASSERT_WAY_PATTERN(wt_road, coord3d(0, 0, 1), span)
	}

	ASSERT_EQUAL(remover.work(pl, coord3d(2, 3, 0), coord3d(4, 3, 0), "" + wt_road), null)
	ASSERT_EQUAL(remover.work(pl, coord3d(3, 2, 0), coord3d(3, 5, 0), "" + wt_road), null)
	ASSERT_EQUAL(setslope(pl, coord3d(3, 2, 0), slope.all_down_slope), null)
	ASSERT_EQUAL(setslope(pl, coord3d(3, 5, 0), slope.all_down_slope), null)

	RESET_ALL_PLAYER_FUNDS()
}


// Flat-ground SE-axis road bridge (6,8)→(8,8) crossing a S-axis
// taxiway (7,7)→(7,9).  Then repeat with a runway in place of the
// taxiway: bridge build refuses (no bridges over runways).
function test_way_bridge_build_above_runway()
{
	local pl = player_x(0)
	local wayremover = command_x(tool_remove_way)
	local taxiway = way_desc_x.get_available_ways(wt_air, st_flat)[0]
	local runway = way_desc_x.get_available_ways(wt_air, st_elevated)[0]
	local bridge = bridge_desc_x.get_available_bridges(wt_road)[0]

	// Taxiway / runway column x=7 along S axis.  S=2, N=16, N|S=18.
	local air_column = [
		[0, 0, 0,  0, 0, 0, 0, 0],
		[0, 0, 0,  0, 0, 0, 0, 0],
		[0, 0,  2, 0, 0, 0, 0, 0],
		[0, 0, 18, 0, 0, 0, 0, 0],
		[0, 0, 16, 0, 0, 0, 0, 0],
		[0, 0, 0,  0, 0, 0, 0, 0],
		[0, 0, 0,  0, 0, 0, 0, 0],
		[0, 0, 0,  0, 0, 0, 0, 0],
	]

	// build bridge across taxiway
	{
		ASSERT_EQUAL(command_x.build_way(pl, coord3d(7, 7, 0), coord3d(7, 9, 0), taxiway, true), null)
		ASSERT_EQUAL(command_x.build_bridge(pl, coord3d(6, 8, 0), coord3d(8, 8, 0), bridge), null)

		ASSERT_WAY_PATTERN(wt_air, coord3d(5, 5, 0), air_column)

		// Bridge row y=8 along SE axis: auto-extended stubs at x=5,9,
		// ramps at x=6,8, span at z=1 over (7,8).  SE=1, NW=8, SE|NW=9.
		ASSERT_WAY_PATTERN(wt_road, coord3d(5, 5, 0),
			[
				[0, 0, 0, 0, 0, 0, 0, 0],
				[0, 0, 0, 0, 0, 0, 0, 0],
				[0, 0, 0, 0, 0, 0, 0, 0],
				[1, 9, 0, 9, 8, 0, 0, 0],
				[0, 0, 0, 0, 0, 0, 0, 0],
				[0, 0, 0, 0, 0, 0, 0, 0],
				[0, 0, 0, 0, 0, 0, 0, 0],
				[0, 0, 0, 0, 0, 0, 0, 0],
			])

		ASSERT_EQUAL(wayremover.work(pl, coord3d(7, 7, 0), coord3d(7, 9, 0), "" + wt_air), null)
		ASSERT_EQUAL(wayremover.work(pl, coord3d(5, 8, 0), coord3d(9, 8, 0), "" + wt_road), null)
	}

	// build bridge across runway, should fail
	{
		ASSERT_EQUAL(command_x.build_way(pl, coord3d(7, 7, 0), coord3d(7, 9, 0), runway, true), null)
		ASSERT_EQUAL(command_x.build_bridge(pl, coord3d(6, 8, 0), coord3d(8, 8, 0), bridge), "No bridges over runways!")

		ASSERT_WAY_PATTERN(wt_air, coord3d(5, 5, 0), air_column)

		ASSERT_EQUAL(wayremover.work(pl, coord3d(7, 7, 0), coord3d(7, 9, 0), "" + wt_air), null)
	}

	// clean up
	ASSERT_EQUAL(pl.get_current_maintenance(), 0)
	RESET_ALL_PLAYER_FUNDS()
}


function test_way_bridge_planner()
{
	local pl = player_x(0)
	local start_pos = coord3d(12, 3, 0)
	local end_pos = coord3d(12, 6, 0)
	local bridge_desc = bridge_desc_x.get_available_bridges(wt_road)[0]

	local working_slopes = [ slope.north_narrow, slope.north_wide ]

	ASSERT_EQUAL(command_x.set_slope(pl, start_pos, slope.south_narrow), null)

	{
		foreach (sl in interesting_slopes()) {
			command_x.set_slope(pl, end_pos, sl)

			local end = bridge_planner_x.find_end(pl, start_pos, dir.south, bridge_desc, 0)
			local expected_end = (working_slopes.find(sl) != null) ? end_pos : coord3d(-1, -1, -1)

			ASSERT_EQUAL(end.tostring(), expected_end.tostring())
		}

		ASSERT_EQUAL(command_x.set_slope(pl, end_pos, slope.all_up_slope), null)
		local end = bridge_planner_x.find_end(pl, start_pos, dir.south, bridge_desc, 0)
		ASSERT_EQUAL(end.tostring(), (end_pos + coord3d(0, 0, 1)).tostring())

		// clean up
		ASSERT_EQUAL(command_x.set_slope(pl, start_pos, slope.flat), null)
		ASSERT_EQUAL(command_x.set_slope(pl, end_pos + coord3d(0, 0, 1), slope.all_down_slope), null)
	}

	// min length
	{
		ASSERT_EQUAL(command_x.set_slope(pl, coord3d(2, 1, 0), slope.south_narrow), null)
		ASSERT_EQUAL(command_x.set_slope(pl, coord3d(3, 1, 0), slope.south_narrow), null)

		ASSERT_EQUAL(command_x.set_slope(pl, coord3d(2, 2, 0), slope.north_narrow), null)
		ASSERT_EQUAL(command_x.set_slope(pl, coord3d(3, 3, 0), slope.north_narrow), null)

		ASSERT_EQUAL(bridge_planner_x.find_end(pl, coord3d(2, 1, 0), dir.south, bridge_desc, 0).tostring(), coord3d(2, 2, 0).tostring())
		ASSERT_EQUAL(bridge_planner_x.find_end(pl, coord3d(2, 1, 0), dir.south, bridge_desc, 1).tostring(), coord3d(2, 2, 0).tostring())

		ASSERT_EQUAL(bridge_planner_x.find_end(pl, coord3d(3, 1, 0), dir.south, bridge_desc, 0).tostring(), coord3d(3, 3, 0).tostring())
		ASSERT_EQUAL(bridge_planner_x.find_end(pl, coord3d(3, 1, 0), dir.south, bridge_desc, 1).tostring(), coord3d(3, 3, 0).tostring())
		ASSERT_EQUAL(bridge_planner_x.find_end(pl, coord3d(3, 1, 0), dir.south, bridge_desc, 2).tostring(), coord3d(3, 3, 0).tostring())
		ASSERT_EQUAL(bridge_planner_x.find_end(pl, coord3d(3, 1, 0), dir.south, bridge_desc, 3).tostring(), coord3d(-1, -1, -1).tostring())

		ASSERT_EQUAL(command_x.set_slope(pl, coord3d(2, 1, 0), slope.flat), null)
		ASSERT_EQUAL(command_x.set_slope(pl, coord3d(3, 1, 0), slope.flat), null)
		ASSERT_EQUAL(command_x.set_slope(pl, coord3d(2, 2, 0), slope.flat), null)
		ASSERT_EQUAL(command_x.set_slope(pl, coord3d(3, 3, 0), slope.flat), null)
	}

	ASSERT_EQUAL(pl.get_current_maintenance(), 0)
	RESET_ALL_PLAYER_FUNDS()
}
