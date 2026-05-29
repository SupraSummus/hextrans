//
// This file is part of the Simutrans project under the Artistic License.
// (see LICENSE.txt)
//


//
// Tests for scenario rules/conditions
//

function test_scenario_rules_allow_forbid_tool()
{
	local pl = player_x(0)

	{
		ASSERT_EQUAL(command_x.grid_raise(pl, coord3d(4, 2, 0)), null) // FIXME this should fail
		ASSERT_EQUAL(command_x.grid_lower(pl, coord3d(4, 2, 1)), null)
	}

	// clean up
	RESET_ALL_PLAYER_FUNDS()
}


function test_scenario_rules_allow_forbid_way_tool()
{
	local waybuilder = command_x(tool_build_way)
	local road = way_desc_x.get_available_ways(wt_road, st_flat)[0]
	local rail = way_desc_x.get_available_ways(wt_rail, st_flat)[0]
	local pl = player_x(0)

	rules.forbid_way_tool_rect(0, tool_build_way, wt_road, road.get_name(), coord(2, 2), coord(5, 5), "Foo Bar")

	// Fully in forbiden zone
	{
		ASSERT_EQUAL(command_x.build_way(pl, coord3d(2, 2, 0), coord3d(5, 5, 0), road, true), "")
		ASSERT_WAY_PATTERN(wt_road, coord3d(0, 0, 0),
			[
				"........",
				"........",
				"........",
				"........",
				"........",
				"........",
				"........",
				"........"
			])
	}

	// Ending in forbidden zone
	{
		ASSERT_EQUAL(command_x.build_way(pl, coord3d(0, 2, 0), coord3d(2, 2, 0), road, true), "")
		ASSERT_WAY_PATTERN(wt_road, coord3d(0, 0, 0),
			[
				"........",
				"........",
				"........",
				"........",
				"........",
				"........",
				"........",
				"........"
			])
	}

	// Starting in forbidden zone
	{
		ASSERT_EQUAL(command_x.build_way(pl, coord3d(2, 2, 0), coord3d(0, 2, 0), road, true), "")
		ASSERT_WAY_PATTERN(wt_road, coord3d(0, 0, 0),
			[
				"........",
				"........",
				"........",
				"........",
				"........",
				"........",
				"........",
				"........"
			])
	}

	// make sure we can build other ways
	{
		ASSERT_EQUAL(command_x.build_way(pl, coord3d(2, 2, 0), coord3d(0, 2, 0), rail, true), null)
		ASSERT_WAY_PATTERN(wt_rail, coord3d(0, 0, 0),
			[
				"........",
				"........",
				"2A8.....",
				"........",
				"........",
				"........",
				"........",
				"........"
			])

		ASSERT_EQUAL(command_x(tool_remove_way).work(pl, coord3d(2, 2, 0), coord3d(0, 2, 0), "" + wt_rail), null)
	}

	// clean up
	rules.clear()
	RESET_ALL_PLAYER_FUNDS()
}


// Blank 8x8 board: every tile asserted to carry no way of the queried
// type.  Shared by the rect/cube forbid checks below.
local empty_pattern = [
	[0, 0, 0, 0, 0, 0, 0, 0],
	[0, 0, 0, 0, 0, 0, 0, 0],
	[0, 0, 0, 0, 0, 0, 0, 0],
	[0, 0, 0, 0, 0, 0, 0, 0],
	[0, 0, 0, 0, 0, 0, 0, 0],
	[0, 0, 0, 0, 0, 0, 0, 0],
	[0, 0, 0, 0, 0, 0, 0, 0],
	[0, 0, 0, 0, 0, 0, 0, 0],
]

function test_scenario_rules_allow_forbid_way_tool_rect()
{
	local road = way_desc_x.get_available_ways(wt_road, st_flat)[0]
	local rail = way_desc_x.get_available_ways(wt_rail, st_flat)[0]
	local pl = player_x(0)

	rules.forbid_way_tool_rect(0, tool_build_way, wt_road, "", coord(2, 2), coord(5, 5), "Foo Bar")

	// Fully in forbiden zone
	{
		ASSERT_EQUAL(command_x.build_way(pl, coord3d(2, 2, 0), coord3d(5, 5, 0), road, true), "")
		ASSERT_WAY_PATTERN(wt_road, coord3d(0, 0, 0), empty_pattern)
	}

	// Ending in forbidden zone
	{
		ASSERT_EQUAL(command_x.build_way(pl, coord3d(0, 2, 0), coord3d(2, 2, 0), road, true), "")
		ASSERT_WAY_PATTERN(wt_road, coord3d(0, 0, 0), empty_pattern)
	}

	// Starting in forbidden zone
	{
		ASSERT_EQUAL(command_x.build_way(pl, coord3d(2, 2, 0), coord3d(0, 2, 0), road, true), "")
		ASSERT_WAY_PATTERN(wt_road, coord3d(0, 0, 0), empty_pattern)
	}

	// make sure we can build other ways: rail along y=2 (the SE-NW hex
	// axis), (0,2)=SE, (1,2)=NW|SE, (2,2)=NW
	{
		ASSERT_EQUAL(command_x.build_way(pl, coord3d(2, 2, 0), coord3d(0, 2, 0), rail, true), null)
		ASSERT_WAY_PATTERN(wt_rail, coord3d(0, 0, 0),
			[
				[0, 0, 0, 0, 0, 0, 0, 0],
				[0, 0, 0, 0, 0, 0, 0, 0],
				[1, 9, 8, 0, 0, 0, 0, 0],
				[0, 0, 0, 0, 0, 0, 0, 0],
				[0, 0, 0, 0, 0, 0, 0, 0],
				[0, 0, 0, 0, 0, 0, 0, 0],
				[0, 0, 0, 0, 0, 0, 0, 0],
				[0, 0, 0, 0, 0, 0, 0, 0],
			])

		ASSERT_EQUAL(command_x(tool_remove_way).work(pl, coord3d(2, 2, 0), coord3d(0, 2, 0), "" + wt_rail), null)
	}

	// clean up
	rules.clear()
	RESET_ALL_PLAYER_FUNDS()
}


function test_scenario_rules_allow_forbid_way_tool_cube()
{
	local setslope = command_x.set_slope
	local road = way_desc_x.get_available_ways(wt_road, st_flat)[0]
	// the has_double_slopes-opted-in test-pak road, for the 2-step ramp
	// subtest below (pak64's stock roads ship the opt-in off)
	local double_road = null
	foreach (r in way_desc_x.get_available_ways(wt_road, st_flat)) {
		if (r.has_double_slopes()) double_road = r
	}
	ASSERT_TRUE(double_road != null)
	local pl = player_x(0)

	rules.forbid_way_tool_cube(0, tool_build_way, wt_road, "", coord3d(2, 2, 1), coord3d(5, 5, 2), "Foo Bar")

	// build below the forbidden cube (z=0): straight SE-NW chord on y=2
	{
		ASSERT_EQUAL(command_x.build_way(pl, coord3d(2, 2, 0), coord3d(0, 2, 0), road, true), null)
		ASSERT_WAY_PATTERN(wt_road, coord3d(0, 0, 0),
			[
				[0, 0, 0, 0, 0, 0, 0, 0],
				[0, 0, 0, 0, 0, 0, 0, 0],
				[1, 9, 8, 0, 0, 0, 0, 0],
				[0, 0, 0, 0, 0, 0, 0, 0],
				[0, 0, 0, 0, 0, 0, 0, 0],
				[0, 0, 0, 0, 0, 0, 0, 0],
				[0, 0, 0, 0, 0, 0, 0, 0],
				[0, 0, 0, 0, 0, 0, 0, 0],
			])

		ASSERT_EQUAL(command_x(tool_remove_way).work(pl, coord3d(2, 2, 0), coord3d(0, 2, 0), "" + wt_road), null)
	}

	// build into forbidden zone: a ramp climbing to (3,4,1) lands inside
	// the cube and is rejected, leaving the map empty
	{
		ASSERT_EQUAL(setslope(pl, coord3d(3, 4, 0), slope.all_up_slope), null)
		ASSERT_EQUAL(setslope(pl, coord3d(3, 3, 0), slope.north_narrow), null)

		ASSERT_EQUAL(command_x.build_way(pl, coord3d(3, 0, 0), coord3d(3, 4, 1), road, true), "")
		ASSERT_WAY_PATTERN(wt_road, coord3d(0, 0, 0), empty_pattern)

		ASSERT_EQUAL(setslope(pl, coord3d(3, 4, 1), slope.all_down_slope), null)
		ASSERT_EQUAL(setslope(pl, coord3d(3, 3, 0), slope.flat), null)
	}

	rules.clear()
	rules.forbid_way_tool_cube(0, tool_build_way, wt_road, "", coord3d(0, 0, 1), coord3d(0, 0, 1), "Foo Bar")

	// build over a planar double-height slope outside the forbidden cube:
	// SE-NW chord on y=1 with (1,1) a SE-low 2-step ramp; (1,1)=SE,
	// (2,1)=NW.  Uses the has_double_slopes test-pak road -- the hex
	// successor to the square-era 2*slope.east double slope.
	{
		ASSERT_EQUAL(setslope(pl, coord3d(1, 1, 0), slope.southeast_double), null)
		ASSERT_EQUAL(command_x.build_way(pl, coord3d(2, 1, 0), coord3d(1, 1, 0), double_road, true), null)
		ASSERT_WAY_PATTERN(wt_road, coord3d(0, 0, 0),
			[
				[0, 0, 0, 0, 0, 0, 0, 0],
				[0, 1, 8, 0, 0, 0, 0, 0],
				[0, 0, 0, 0, 0, 0, 0, 0],
				[0, 0, 0, 0, 0, 0, 0, 0],
				[0, 0, 0, 0, 0, 0, 0, 0],
				[0, 0, 0, 0, 0, 0, 0, 0],
				[0, 0, 0, 0, 0, 0, 0, 0],
				[0, 0, 0, 0, 0, 0, 0, 0],
			])

		ASSERT_EQUAL(command_x(tool_remove_way).work(pl, coord3d(2, 1, 0), coord3d(1, 1, 0), "" + wt_road), null)
		ASSERT_EQUAL(setslope(pl, coord3d(1, 1, 0), slope.flat), null)
	}

	// clean up
	rules.clear()
	RESET_ALL_PLAYER_FUNDS()
}


// Road from (5,0) to (0,5) is the SW hex axis; with the inner block
// forbidden the router hugs the allowed border ring as a hex staircase
// (down the x=0 / y=0 edges, cutting the corner directly (1,0)->(0,1)).
local stacked_outer_ring = [
	[ 0,  5,  9,  9,  9,  8, 0, 0],
	[34,  0,  0,  0,  0,  0, 0, 0],
	[18,  0,  0,  0,  0,  0, 0, 0],
	[18,  0,  0,  0,  0,  0, 0, 0],
	[18,  0,  0,  0,  0,  0, 0, 0],
	[16,  0,  0,  0,  0,  0, 0, 0],
	[ 0,  0,  0,  0,  0,  0, 0, 0],
	[ 0,  0,  0,  0,  0,  0, 0, 0],
]
// Same ring plus the (5,2)->(2,5) straight SW chord through the inner
// region, buildable once that region is allowed.
local stacked_inner_chord = [
	[ 0,  5,  9,  9,  9,  8, 0, 0],
	[34,  0,  0,  0,  0,  0, 0, 0],
	[18,  0,  0,  0,  0,  4, 0, 0],
	[18,  0,  0,  0, 36,  0, 0, 0],
	[18,  0,  0, 36,  0,  0, 0, 0],
	[16,  0, 32,  0,  0,  0, 0, 0],
	[ 0,  0,  0,  0,  0,  0, 0, 0],
	[ 0,  0,  0,  0,  0,  0, 0, 0],
]

function test_scenario_rules_allow_forbid_tool_stacked_rect()
{
	local pl = player_x(0)
	local road_desc = way_desc_x.get_available_ways(wt_road, st_flat)[0]

	rules.forbid_way_tool_rect(0, tool_build_way, wt_road, "", coord(1, 1), coord(14, 14), "Foo Bar 1")

	// build in outer allowed ring, near map border
	{
		ASSERT_EQUAL(command_x.build_way(pl, coord3d(5, 0, 0), coord3d(0, 5, 0), road_desc, false), null)
		ASSERT_WAY_PATTERN(wt_road, coord3d(0, 0, 0), stacked_outer_ring)
	}

	// build in outer forbidden ring -> rejected, ring unchanged
	{
		ASSERT_EQUAL(command_x.build_way(pl, coord3d(5, 1, 0), coord3d(1, 5, 0), road_desc, false), "")
		ASSERT_WAY_PATTERN(wt_road, coord3d(0, 0, 0), stacked_outer_ring)
	}

	rules.allow_way_tool_rect(0, tool_build_way, wt_road, "", coord(2, 2), coord(13, 13))

	// inner block is now allowed -> the SW chord builds
	{
		ASSERT_EQUAL(command_x.build_way(pl, coord3d(5, 2, 0), coord3d(2, 5, 0), road_desc, false), null)
		ASSERT_WAY_PATTERN(wt_road, coord3d(0, 0, 0), stacked_inner_chord)
	}

	// from a still-forbidden ring tile into the allowed block -> rejected
	{
		ASSERT_EQUAL(command_x.build_way(pl, coord3d(5, 1, 0), coord3d(2, 5, 0), road_desc, false), "")
		ASSERT_WAY_PATTERN(wt_road, coord3d(0, 0, 0), stacked_inner_chord)
	}

	// from outside the allowed block across the border -> rejected
	{
		ASSERT_EQUAL(command_x.build_way(pl, coord3d(0, 0, 0), coord3d(2, 5, 0), road_desc, false), "")
		ASSERT_WAY_PATTERN(wt_road, coord3d(0, 0, 0), stacked_inner_chord)
	}

	rules.clear()

	ASSERT_EQUAL(command_x(tool_remove_way).work(pl, coord3d(5, 0, 0), coord3d(0, 5, 0), "" + wt_road), null)
	ASSERT_EQUAL(command_x(tool_remove_way).work(pl, coord3d(5, 2, 0), coord3d(2, 5, 0), "" + wt_road), null)
	RESET_ALL_PLAYER_FUNDS()
}


// Identical to the _rect case but the outer forbid is a cube (z range
// 0..0) rather than a flat rect; same border-hugging routes result.
function test_scenario_rules_allow_forbid_tool_stacked_cube()
{
	local pl = player_x(0)
	local road_desc = way_desc_x.get_available_ways(wt_road, st_flat)[0]

	rules.forbid_way_tool_cube(0, tool_build_way, wt_road, "", coord3d(1, 1, 0), coord3d(14, 14, 0), "Foo Bar 1")

	// build in outer allowed ring, near map border
	{
		ASSERT_EQUAL(command_x.build_way(pl, coord3d(5, 0, 0), coord3d(0, 5, 0), road_desc, false), null)
		ASSERT_WAY_PATTERN(wt_road, coord3d(0, 0, 0), stacked_outer_ring)
	}

	// build in outer forbidden ring -> rejected, ring unchanged
	{
		ASSERT_EQUAL(command_x.build_way(pl, coord3d(5, 1, 0), coord3d(1, 5, 0), road_desc, false), "")
		ASSERT_WAY_PATTERN(wt_road, coord3d(0, 0, 0), stacked_outer_ring)
	}

	rules.allow_way_tool_rect(0, tool_build_way, wt_road, "", coord(2, 2), coord(13, 13))

	// inner block is now allowed -> the SW chord builds
	{
		ASSERT_EQUAL(command_x.build_way(pl, coord3d(5, 2, 0), coord3d(2, 5, 0), road_desc, false), null)
		ASSERT_WAY_PATTERN(wt_road, coord3d(0, 0, 0), stacked_inner_chord)
	}

	// from a still-forbidden ring tile into the allowed block -> rejected
	{
		ASSERT_EQUAL(command_x.build_way(pl, coord3d(5, 1, 0), coord3d(2, 5, 0), road_desc, false), "")
		ASSERT_WAY_PATTERN(wt_road, coord3d(0, 0, 0), stacked_inner_chord)
	}

	// from outside the allowed block across the border -> rejected
	{
		ASSERT_EQUAL(command_x.build_way(pl, coord3d(0, 0, 0), coord3d(2, 5, 0), road_desc, false), "")
		ASSERT_WAY_PATTERN(wt_road, coord3d(0, 0, 0), stacked_inner_chord)
	}

	rules.clear()

	ASSERT_EQUAL(command_x(tool_remove_way).work(pl, coord3d(5, 0, 0), coord3d(0, 5, 0), "" + wt_road), null)
	ASSERT_EQUAL(command_x(tool_remove_way).work(pl, coord3d(5, 2, 0), coord3d(2, 5, 0), "" + wt_road), null)
	RESET_ALL_PLAYER_FUNDS()
}
