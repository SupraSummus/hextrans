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


// Two parallel S-axis road bridges stacked vertically on the same column.
// Lower bridge spans (3,3)/(3,4) at z=1 between south_narrow ramp (3,2,0)
// and north_narrow ramp (3,5,0).  Upper bridge spans (3,2)..(3,5) at z=2
// between two 2-step ramps: (3,1) and (3,6) raised to z=1 (all_up_slope)
// then sloped south_narrow / north_narrow at z=1.  The cross-axis stacked
// case (perpendicular bridges) is covered by `test_way_bridge_build_stacked`.
function test_way_bridge_build_at_slope_stacked()
{
	local remover = command_x(tool_remove_way)
	local setslope = command_x.set_slope
	local pl = player_x(0)
	local bridge_desc = bridge_desc_x.get_available_bridges(wt_road)[0]

	ASSERT_EQUAL(setslope(pl, coord3d(3, 2, 0), slope.south_narrow), null)
	ASSERT_EQUAL(setslope(pl, coord3d(3, 5, 0), slope.north_narrow), null)

	ASSERT_EQUAL(command_x.build_bridge_at(pl, coord3d(3, 2, 0), bridge_desc), null)

	// Lower bridge alone: S=2 at (3,2), N|S=18 over (3,3)/(3,4), N=16 at (3,5).
	ASSERT_WAY_PATTERN(wt_road, coord3d(0, 0, 1),
		[
			[0, 0, 0,  0, 0, 0, 0, 0],
			[0, 0, 0,  0, 0, 0, 0, 0],
			[0, 0, 0,  2, 0, 0, 0, 0],
			[0, 0, 0, 18, 0, 0, 0, 0],
			[0, 0, 0, 18, 0, 0, 0, 0],
			[0, 0, 0, 16, 0, 0, 0, 0],
			[0, 0, 0,  0, 0, 0, 0, 0],
			[0, 0, 0,  0, 0, 0, 0, 0],
		])

	// Upper bridge: 2-step ramps at (3,1) and (3,6) (all_up_slope at z=0
	// flattens the tile to z=1, then south_narrow / north_narrow at z=1
	// gives the ramp surface that the bridge at z=2 attaches to).
	ASSERT_EQUAL(setslope(pl, coord3d(3, 1, 0), slope.all_up_slope), null)
	ASSERT_EQUAL(setslope(pl, coord3d(3, 1, 1), slope.south_narrow), null)
	ASSERT_EQUAL(setslope(pl, coord3d(3, 6, 0), slope.all_up_slope), null)
	ASSERT_EQUAL(setslope(pl, coord3d(3, 6, 1), slope.north_narrow), null)

	ASSERT_EQUAL(command_x.build_bridge_at(pl, coord3d(3, 1, 1), bridge_desc), null)

	// z=1: now both upper ramps (3,1)/(3,6) and the lower bridge appear.
	ASSERT_WAY_PATTERN(wt_road, coord3d(0, 0, 1),
		[
			[0, 0, 0,  0, 0, 0, 0, 0],
			[0, 0, 0,  2, 0, 0, 0, 0],
			[0, 0, 0,  2, 0, 0, 0, 0],
			[0, 0, 0, 18, 0, 0, 0, 0],
			[0, 0, 0, 18, 0, 0, 0, 0],
			[0, 0, 0, 16, 0, 0, 0, 0],
			[0, 0, 0, 16, 0, 0, 0, 0],
			[0, 0, 0,  0, 0, 0, 0, 0],
		])

	// z=2: upper bridge span (3,2)..(3,5) plus its (3,1)/(3,6) ramp tops.
	ASSERT_WAY_PATTERN(wt_road, coord3d(0, 0, 2),
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

	// clean up
	ASSERT_EQUAL(remover.work(pl, coord3d(3, 1, 1), coord3d(3, 6, 1), "" + wt_road), null)
	ASSERT_EQUAL(remover.work(pl, coord3d(3, 2, 0), coord3d(3, 5, 0), "" + wt_road), null)
	ASSERT_EQUAL(setslope(pl, coord3d(3, 1, 1), slope.all_down_slope), null)
	ASSERT_EQUAL(setslope(pl, coord3d(3, 1, 1), slope.all_down_slope), null)
	ASSERT_EQUAL(setslope(pl, coord3d(3, 6, 1), slope.all_down_slope), null)
	ASSERT_EQUAL(setslope(pl, coord3d(3, 6, 1), slope.all_down_slope), null)
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
	// the auto-extension in bridge_builder_t::build_bridge (road stubs
	// at (3,1,0)/(3,6,0)), the crossing's delta-2 ramps don't (see
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


function test_way_bridge_over_building()
{
	local remover = command_x(tool_remove_way)
	local setslope = command_x.set_slope
	local pl = player_x(0)
	local public_pl = player_x(1)
	local bridge_desc = bridge_desc_x.get_available_bridges(wt_road)[0]
	ASSERT_TRUE(bridge_desc != null)

	// same geometry as test_way_bridge_build_above_way, but with a tall building on
	// the valley floor: the deck leaves less clearance than the building needs
	ASSERT_EQUAL(command_x(tool_add_city).work(public_pl, coord3d(12, 12, 0), "0"), null)

	ASSERT_EQUAL(setslope(pl, coord3d(3, 2, 0), slope.south_narrow), null)
	ASSERT_EQUAL(setslope(pl, coord3d(3, 5, 0), slope.north_narrow), null)

	// control: over an empty valley the same bridge builds fine
	ASSERT_EQUAL(command_x.build_bridge(pl, coord3d(3, 2, 0), coord3d(3, 5, 0), bridge_desc), null)
	ASSERT_EQUAL(remover.work(pl, coord3d(3, 2, 0), coord3d(3, 5, 0), "" + wt_road), null)

	ASSERT_EQUAL(command_x(tool_build_house).work(public_pl, coord3d(3, 3, 0), "1#Theatre"), null)
	ASSERT_EQUAL(command_x.build_bridge(pl, coord3d(3, 2, 0), coord3d(3, 5, 0), bridge_desc), "")
	// no deck may have been left over the building at any height
	ASSERT_TRUE(square_x(3, 3).get_tile_at_height(1) == null)
	ASSERT_TRUE(square_x(3, 4).get_tile_at_height(1) == null)
	ASSERT_TRUE(square_x(3, 3).get_tile_at_height(2) == null)
	ASSERT_TRUE(square_x(3, 4).get_tile_at_height(2) == null)

	ASSERT_EQUAL(command_x(tool_remover).work(public_pl, coord3d(3, 3, 0)), null)

	ASSERT_EQUAL(setslope(pl, coord3d(3, 2, 0), slope.all_down_slope), null)
	ASSERT_EQUAL(setslope(pl, coord3d(3, 5, 0), slope.all_down_slope), null)
	clear_townhall_and_roads(public_pl, 12, 12) // the hex townhall seeds its road on a random face

	RESET_ALL_PLAYER_FUNDS()
}


function test_way_bridge_over_fitting_stacked_building()
{
	local remover = command_x(tool_remove_way)
	local setslope = command_x.set_slope
	local pl = player_x(0)
	local bridge_desc = bridge_desc_x.get_available_bridges(wt_road)[0]
	local station_desc = building_desc_x.get_available_stations(building_desc_x.station, wt_road, good_desc_x.passenger)[0]
	ASSERT_TRUE(bridge_desc != null)
	ASSERT_TRUE(station_desc != null)

	// lower bridge, east-west, with its deck at z=1
	ASSERT_EQUAL(setslope(pl, coord3d(4, 9, 0), slope.southeast_narrow), null)
	ASSERT_EQUAL(setslope(pl, coord3d(7, 9, 0), slope.northwest_narrow), null)
	ASSERT_EQUAL(command_x.build_bridge(pl, coord3d(4, 9, 0), coord3d(7, 9, 0), bridge_desc), null)

	// raised endpoints let a north-south bridge try z=1, then z=2
	ASSERT_EQUAL(setslope(pl, coord3d(5, 7, 0), slope.all_up_slope), null)
	ASSERT_EQUAL(setslope(pl, coord3d(5, 11, 0), slope.all_up_slope), null)

	// control: the upper bridge builds over the empty lower deck
	ASSERT_EQUAL(command_x.build_bridge(pl, coord3d(5, 7, 1), coord3d(5, 11, 1), bridge_desc), null)
	ASSERT_EQUAL(remover.work(pl, coord3d(5, 7, 1), coord3d(5, 11, 1), "" + wt_road), null)

	// a station that fits exactly in the one-level gap must not be overblocked
	ASSERT_EQUAL(command_x.build_station(pl, coord3d(5, 9, 1), station_desc), null)
	ASSERT_EQUAL(command_x.build_bridge(pl, coord3d(5, 7, 1), coord3d(5, 11, 1), bridge_desc), null)
	ASSERT_TRUE(square_x(5, 9).get_tile_at_height(2) != null)

	ASSERT_EQUAL(remover.work(pl, coord3d(5, 7, 1), coord3d(5, 11, 1), "" + wt_road), null)
	ASSERT_EQUAL(command_x(tool_remover).work(pl, coord3d(5, 9, 1)), null)
	ASSERT_EQUAL(remover.work(pl, coord3d(4, 9, 0), coord3d(7, 9, 0), "" + wt_road), null)
	ASSERT_EQUAL(setslope(pl, coord3d(4, 9, 0), slope.all_down_slope), null)
	ASSERT_EQUAL(setslope(pl, coord3d(7, 9, 0), slope.all_down_slope), null)
	ASSERT_EQUAL(setslope(pl, coord3d(5, 7, 1), slope.all_down_slope), null)
	ASSERT_EQUAL(setslope(pl, coord3d(5, 11, 1), slope.all_down_slope), null)

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


// helper for the bridge planner tests: puts a ramp at the end of the corridor,
// asks for the end of the bridge and restores the ground again
function BRIDGE_FIND_END(pl, bridge_desc, start_pos, len, max_length, flat_ends)
{
	local end_pos = start_pos + coord3d(0, len, 0)
	local has_ramp = len > 0
	if (has_ramp) {
		ASSERT_EQUAL(command_x.set_slope(pl, end_pos, slope.north_narrow), null)
	}

	local res
	if (max_length == null) {
		// the call as it was available before max_length/flat_ends were added
		res = bridge_planner_x.find_end(pl, start_pos, dir.south, bridge_desc, 0)
	}
	else if (flat_ends == null) {
		res = bridge_planner_x.find_end(pl, start_pos, dir.south, bridge_desc, 0, max_length)
	}
	else {
		res = bridge_planner_x.find_end(pl, start_pos, dir.south, bridge_desc, 0, max_length, flat_ends)
	}

	if (has_ramp) {
		ASSERT_EQUAL(command_x.set_slope(pl, end_pos, slope.flat), null)
	}
	return res.tostring()
}


function test_way_bridge_planner_max_length()
{
	local pl          = player_x(0)
	local start_pos   = coord3d(11, 1, 0)
	local bridge_desc = bridge_desc_x.get_available_bridges(wt_road)[0]
	local invalid     = coord3d(-1, -1, -1).tostring()

	// this test needs a bridge that can span more than ten tiles
	ASSERT_TRUE(bridge_desc != null)
	ASSERT_TRUE(bridge_desc.get_max_length() >= 12)

	ASSERT_EQUAL(command_x.set_slope(pl, start_pos, slope.south_narrow), null)

	// the old five parameter call keeps its length limit of ten tiles
	ASSERT_EQUAL(BRIDGE_FIND_END(pl, bridge_desc, start_pos,  5, null, null), coord3d(11,  6, 0).tostring())
	ASSERT_EQUAL(BRIDGE_FIND_END(pl, bridge_desc, start_pos, 10, null, null), coord3d(11, 11, 0).tostring())
	ASSERT_EQUAL(BRIDGE_FIND_END(pl, bridge_desc, start_pos, 11, null, null), invalid)

	// with an explicit maximum length longer bridges are found
	ASSERT_EQUAL(BRIDGE_FIND_END(pl, bridge_desc, start_pos, 11, 11, null), coord3d(11, 12, 0).tostring())
	ASSERT_EQUAL(BRIDGE_FIND_END(pl, bridge_desc, start_pos, 12, 12, null), coord3d(11, 13, 0).tostring())

	// ... but the maximum length is obeyed
	ASSERT_EQUAL(BRIDGE_FIND_END(pl, bridge_desc, start_pos, 12, 11, null), invalid)

	// zero means: as long as this bridge allows
	ASSERT_EQUAL(BRIDGE_FIND_END(pl, bridge_desc, start_pos, 12, 0, null), coord3d(11, 13, 0).tostring())

	// min_length larger than max_length finds nothing
	ASSERT_EQUAL(command_x.set_slope(pl, coord3d(11, 6, 0), slope.north_narrow), null)
	ASSERT_EQUAL(bridge_planner_x.find_end(pl, start_pos, dir.south, bridge_desc, 8, 5).tostring(), invalid)
	// same query twice gives the same answer
	local first  = bridge_planner_x.find_end(pl, start_pos, dir.south, bridge_desc, 0, 12).tostring()
	local second = bridge_planner_x.find_end(pl, start_pos, dir.south, bridge_desc, 0, 12).tostring()
	ASSERT_EQUAL(first, coord3d(11, 6, 0).tostring())
	ASSERT_EQUAL(second, first)
	ASSERT_EQUAL(command_x.set_slope(pl, coord3d(11, 6, 0), slope.flat), null)

	// clean up
	ASSERT_EQUAL(command_x.set_slope(pl, start_pos, slope.flat), null)
	ASSERT_EQUAL(pl.get_current_maintenance(), 0)
	RESET_ALL_PLAYER_FUNDS()
}


function test_way_bridge_planner_desc_limits()
{
	local pl        = player_x(0)
	local start_pos = coord3d(11, 1, 0)
	local invalid   = coord3d(-1, -1, -1).tostring()

	// a bridge without a length limit of its own, and a short one
	local unlimited = null
	local short_one = null
	foreach (b in bridge_desc_x.get_available_bridges(wt_road)) {
		if (unlimited == null  &&  b.get_max_length() == 0) {
			unlimited = b
		}
		if (short_one == null  &&  b.get_max_length() > 0  &&  b.get_max_length() < 10) {
			short_one = b
		}
	}
	ASSERT_TRUE(unlimited != null)
	ASSERT_TRUE(short_one != null)

	ASSERT_EQUAL(command_x.set_slope(pl, start_pos, slope.south_narrow), null)

	// a bridge without own limit is only bounded by what the caller asks for
	ASSERT_EQUAL(BRIDGE_FIND_END(pl, unlimited, start_pos, 12, 0, null), coord3d(11, 13, 0).tostring())
	ASSERT_EQUAL(BRIDGE_FIND_END(pl, unlimited, start_pos, 12, 10, null), invalid)

	// a bridge with own limit spans get_max_length()+1 tiles, no matter what the caller asks for
	local max_span = short_one.get_max_length() + 1
	ASSERT_EQUAL(BRIDGE_FIND_END(pl, short_one, start_pos, max_span,     20, null), (start_pos + coord3d(0, max_span, 0)).tostring())
	ASSERT_EQUAL(BRIDGE_FIND_END(pl, short_one, start_pos, max_span + 1, 20, null), invalid)

	// clean up
	ASSERT_EQUAL(command_x.set_slope(pl, start_pos, slope.flat), null)
	ASSERT_EQUAL(pl.get_current_maintenance(), 0)
	RESET_ALL_PLAYER_FUNDS()
}


function test_way_bridge_planner_flat_ends()
{
	local pl          = player_x(0)
	local start_pos   = coord3d(9, 12, 0)
	local bridge_desc = bridge_desc_x.get_available_bridges(wt_road)[0]
	local invalid     = coord3d(-1, -1, -1).tostring()

	ASSERT_EQUAL(command_x.set_slope(pl, start_pos, slope.south_narrow), null)

	// there is no ramp in the corridor, only flat ground
	ASSERT_EQUAL(bridge_planner_x.find_end(pl, start_pos, dir.south, bridge_desc, 2, 3, false).tostring(), invalid)
	ASSERT_EQUAL(bridge_planner_x.find_end(pl, start_pos, dir.south, bridge_desc, 2, 3, true).tostring(), coord3d(9, 14, 0).tostring())

	// clean up
	ASSERT_EQUAL(command_x.set_slope(pl, start_pos, slope.flat), null)
	ASSERT_EQUAL(pl.get_current_maintenance(), 0)
	RESET_ALL_PLAYER_FUNDS()
}


function test_way_bridge_planner_flat_start_over_obstacle()
{
	local pl          = player_x(0)
	local start_pos   = coord3d(9, 12, 0)
	local blocked     = coord3d(9, 13, 0)
	local end_pos     = coord3d(9, 14, 0)
	local bridge_desc = bridge_desc_x.get_available_bridges(wt_road)[0]
	local invalid     = coord3d(-1, -1, -1).tostring()

	if (!player_x(2).is_valid()) {
		world.create_player(2, 1)
	}
	local other = player_x(2)

	// the tile next to the start carries an object of another player, so no bridge can end there
	ASSERT_EQUAL(label_x.create(blocked, other, "Foo Bar"), null)

	// the start is flat, so the search for a slope end already matches the blocked neighbour
	ASSERT_EQUAL(bridge_planner_x.find_end(pl, start_pos, dir.south, bridge_desc, 1, 4, false).tostring(), invalid)
	// asking for flat ends has to keep searching behind it
	ASSERT_EQUAL(bridge_planner_x.find_end(pl, start_pos, dir.south, bridge_desc, 1, 4, true).tostring(), end_pos.tostring())

	// clean up
	ASSERT_EQUAL(command_x(tool_remover).work(other, blocked), null)
	ASSERT_EQUAL(pl.get_current_maintenance(), 0)
	RESET_ALL_PLAYER_FUNDS()
}


function test_way_bridge_planner_ownership()
{
	local pl          = player_x(0)
	local start_pos   = coord3d(11, 1, 0)

	// player 1 is the public player, its property may be used by everybody
	if (!player_x(2).is_valid()) {
		world.create_player(2, 1)
	}
	local other = player_x(2)

	local bridge_desc = bridge_desc_x.get_available_bridges(wt_road)[0]
	local remover     = command_x(tool_remover)
	local invalid     = coord3d(-1, -1, -1).tostring()

	ASSERT_TRUE(other.is_valid())
	ASSERT_TRUE(bridge_desc != null)

	ASSERT_EQUAL(command_x.set_slope(pl, start_pos, slope.south_narrow), null)
	ASSERT_EQUAL(command_x.set_slope(pl, coord3d(11, 6, 0), slope.north_narrow), null)

	// the start tile now carries an object of another player
	ASSERT_EQUAL(label_x.create(start_pos, other, "Foo Bar"), null)

	ASSERT_EQUAL(bridge_planner_x.find_end(pl,    start_pos, dir.south, bridge_desc, 0, 12).tostring(), invalid)
	ASSERT_EQUAL(bridge_planner_x.find_end(other, start_pos, dir.south, bridge_desc, 0, 12).tostring(), coord3d(11, 6, 0).tostring())

	// clean up
	ASSERT_EQUAL(remover.work(other, start_pos), null)
	ASSERT_EQUAL(command_x.set_slope(pl, coord3d(11, 6, 0), slope.flat), null)
	ASSERT_EQUAL(command_x.set_slope(pl, start_pos, slope.flat), null)
	RESET_ALL_PLAYER_FUNDS()
}


function test_way_bridge_planner_forbidden_by_scenario()
{
	local pl          = player_x(0)
	local start_pos   = coord3d(11, 1, 0)
	local bridge_desc = bridge_desc_x.get_available_bridges(wt_road)[0]
	local invalid     = coord3d(-1, -1, -1).tostring()

	ASSERT_EQUAL(command_x.set_slope(pl, start_pos, slope.south_narrow), null)
	ASSERT_EQUAL(command_x.set_slope(pl, coord3d(11, 6, 0), slope.north_narrow), null)

	ASSERT_EQUAL(bridge_planner_x.find_end(pl, start_pos, dir.south, bridge_desc, 0, 12).tostring(), coord3d(11, 6, 0).tostring())

	rules.forbid_way_tool_rect(0, tool_build_bridge, wt_road, "", coord(11, 0), coord(11, 15), "Foo Bar")
	ASSERT_EQUAL(bridge_planner_x.find_end(pl, start_pos, dir.south, bridge_desc, 0, 12).tostring(), invalid)
	rules.clear()

	ASSERT_EQUAL(bridge_planner_x.find_end(pl, start_pos, dir.south, bridge_desc, 0, 12).tostring(), coord3d(11, 6, 0).tostring())

	// clean up
	ASSERT_EQUAL(command_x.set_slope(pl, coord3d(11, 6, 0), slope.flat), null)
	ASSERT_EQUAL(command_x.set_slope(pl, start_pos, slope.flat), null)
	ASSERT_EQUAL(pl.get_current_maintenance(), 0)
	RESET_ALL_PLAYER_FUNDS()
}


// counts the tiles of the deck of a bridge that starts at @p start_pos and runs south
function BRIDGE_DECK_LEN(start_pos)
{
	local len = 0
	while (true) {
		local sq = square_x(start_pos.x, start_pos.y + len + 1)
		if (!sq.is_valid()) {
			break
		}
		local tile = sq.get_tile_at_height(start_pos.z + 1)
		if (tile == null  ||  !tile.is_bridge()) {
			break
		}
		len++
	}
	return len
}


// helper for the build_bridge_at tests: puts a ramp at the end of the corridor,
// builds the bridge and reports how far it really got, then removes it again.
// Returns the length of the bridge on success and the error message on failure.
function BRIDGE_BUILD_AT(pl, bridge_desc, start_pos, len, max_length)
{
	local end_pos = start_pos + coord3d(0, len, 0)
	ASSERT_EQUAL(command_x.set_slope(pl, end_pos, slope.north_narrow), null)

	local cash = pl.get_current_cash()
	local err
	if (max_length == null) {
		// the call as it was available before max_length was added
		err = command_x.build_bridge_at(pl, start_pos, bridge_desc)
	}
	else {
		err = command_x.build_bridge_at(pl, start_pos, bridge_desc, max_length)
	}

	local span = BRIDGE_DECK_LEN(start_pos) + 1

	if (err == null) {
		// the bridge is there, it was paid for, and it can be removed again
		ASSERT_TRUE(tile_x(end_pos.x, end_pos.y, end_pos.z).is_bridge())
		ASSERT_TRUE(pl.get_current_cash() < cash)
		ASSERT_EQUAL(command_x(tool_remove_way).work(pl, start_pos, start_pos + coord3d(0, span, 0), "" + wt_road), null)
	}
	else {
		// nothing was built and nothing was paid
		ASSERT_EQUAL(BRIDGE_DECK_LEN(start_pos), 0)
		ASSERT_FALSE(tile_x(start_pos.x, start_pos.y, start_pos.z).is_bridge())
		ASSERT_EQUAL(pl.get_current_cash(), cash)
	}

	ASSERT_EQUAL(command_x.set_slope(pl, end_pos, slope.flat), null)
	return err == null ? span : err
}


function test_way_bridge_build_at_max_length()
{
	local pl          = player_x(0)
	local start_pos   = coord3d(11, 1, 0)
	local bridge_desc = bridge_desc_x.get_available_bridges(wt_road)[0]
	local too_long    = "Bridge is too long for this type!\n"

	// this test needs a bridge that can span more than ten tiles
	ASSERT_TRUE(bridge_desc != null)
	ASSERT_TRUE(bridge_desc.get_max_length() >= 12)

	ASSERT_EQUAL(command_x.set_slope(pl, start_pos, slope.south_narrow), null)

	// the old three parameter call keeps its length limit of ten tiles
	ASSERT_EQUAL(BRIDGE_BUILD_AT(pl, bridge_desc, start_pos, 10, null), 10)
	ASSERT_EQUAL(BRIDGE_BUILD_AT(pl, bridge_desc, start_pos, 11, null), too_long)
	ASSERT_EQUAL(BRIDGE_BUILD_AT(pl, bridge_desc, start_pos, 12, null), too_long)

	// asking for ten explicitly is the same as not asking at all
	ASSERT_EQUAL(BRIDGE_BUILD_AT(pl, bridge_desc, start_pos, 10, 10), 10)
	ASSERT_EQUAL(BRIDGE_BUILD_AT(pl, bridge_desc, start_pos, 11, 10), too_long)

	// with an explicit maximum length longer bridges are built
	ASSERT_EQUAL(BRIDGE_BUILD_AT(pl, bridge_desc, start_pos, 11, 11), 11)
	ASSERT_EQUAL(BRIDGE_BUILD_AT(pl, bridge_desc, start_pos, 12, 12), 12)

	// ... but the maximum length is obeyed
	ASSERT_EQUAL(BRIDGE_BUILD_AT(pl, bridge_desc, start_pos, 12, 11), too_long)

	// clean up
	ASSERT_EQUAL(command_x.set_slope(pl, start_pos, slope.flat), null)
	ASSERT_EQUAL(pl.get_current_maintenance(), 0)
	RESET_ALL_PLAYER_FUNDS()
}


function test_way_bridge_build_at_max_length_zero()
{
	local pl          = player_x(0)
	local start_pos   = coord3d(11, 1, 0)
	local bridge_desc = bridge_desc_x.get_available_bridges(wt_road)[0]
	local too_long    = "Bridge is too long for this type!\n"

	// a bridge with a limit of its own, to show that zero is not "unlimited"
	local short_one = null
	foreach (b in bridge_desc_x.get_available_bridges(wt_road)) {
		if (short_one == null  &&  b.get_max_length() > 0  &&  b.get_max_length() < 10) {
			short_one = b
		}
	}
	ASSERT_TRUE(short_one != null)
	ASSERT_TRUE(bridge_desc.get_max_length() >= 12)

	ASSERT_EQUAL(command_x.set_slope(pl, start_pos, slope.south_narrow), null)

	// zero means: as long as this bridge and the settings allow, negative is the same
	ASSERT_EQUAL(BRIDGE_BUILD_AT(pl, bridge_desc, start_pos, 12,  0), 12)
	ASSERT_EQUAL(BRIDGE_BUILD_AT(pl, bridge_desc, start_pos, 12, -1), 12)

	// ... it does not lift the limit of the bridge itself
	local max_span = short_one.get_max_length() + 1
	ASSERT_EQUAL(BRIDGE_BUILD_AT(pl, short_one, start_pos, max_span,     0), max_span)
	ASSERT_EQUAL(BRIDGE_BUILD_AT(pl, short_one, start_pos, max_span + 1, 0), too_long)

	// clean up
	ASSERT_EQUAL(command_x.set_slope(pl, start_pos, slope.flat), null)
	ASSERT_EQUAL(pl.get_current_maintenance(), 0)
	RESET_ALL_PLAYER_FUNDS()
}


function test_way_bridge_build_at_planner_parity()
{
	local pl          = player_x(0)
	local start_pos   = coord3d(11, 1, 0)
	local bridge_desc = bridge_desc_x.get_available_bridges(wt_road)[0]
	local invalid     = coord3d(-1, -1, -1).tostring()

	ASSERT_TRUE(bridge_desc.get_max_length() >= 12)
	ASSERT_EQUAL(command_x.set_slope(pl, start_pos, slope.south_narrow), null)

	foreach (span in [5, 10, 11, 12]) {
		foreach (max_length in [10, 12, 0]) {
			local planned = BRIDGE_FIND_END(pl, bridge_desc, start_pos, span, max_length, null)
			local built   = BRIDGE_BUILD_AT(pl, bridge_desc, start_pos, span, max_length)

			if (planned == invalid) {
				// what the planner refuses to plan is not built either
				ASSERT_EQUAL(built, "Bridge is too long for this type!\n")
			}
			else {
				// ... and where it puts the end, there the bridge really ends
				ASSERT_TRUE(typeof built == "integer")
				ASSERT_EQUAL((start_pos + coord3d(0, built, 0)).tostring(), planned)
			}
		}
	}

	// clean up
	ASSERT_EQUAL(command_x.set_slope(pl, start_pos, slope.flat), null)
	ASSERT_EQUAL(pl.get_current_maintenance(), 0)
	RESET_ALL_PLAYER_FUNDS()
}


function test_way_bridge_build_at_no_end()
{
	local pl          = player_x(0)
	local start_pos   = coord3d(11, 1, 0)
	local bridge_desc = bridge_desc_x.get_available_bridges(wt_road)[0]
	local too_long    = "Bridge is too long for this type!\n"

	ASSERT_EQUAL(command_x.set_slope(pl, start_pos, slope.south_narrow), null)

	// there is no ramp anywhere in the corridor
	local cash = pl.get_current_cash()
	ASSERT_EQUAL(command_x.build_bridge_at(pl, start_pos, bridge_desc),     too_long)
	ASSERT_EQUAL(command_x.build_bridge_at(pl, start_pos, bridge_desc, 12), too_long)
	ASSERT_EQUAL(command_x.build_bridge_at(pl, start_pos, bridge_desc,  0), too_long)

	ASSERT_EQUAL(pl.get_current_cash(), cash)
	ASSERT_EQUAL(BRIDGE_DECK_LEN(start_pos), 0)
	ASSERT_FALSE(tile_x(start_pos.x, start_pos.y, start_pos.z).is_bridge())

	// clean up
	ASSERT_EQUAL(command_x.set_slope(pl, start_pos, slope.flat), null)
	ASSERT_EQUAL(pl.get_current_maintenance(), 0)
	RESET_ALL_PLAYER_FUNDS()
}
