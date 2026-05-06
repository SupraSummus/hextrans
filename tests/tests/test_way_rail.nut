//
// This file is part of the Simutrans project under the Artistic License.
// (see LICENSE.txt)
//


//
// Tests for rail way building / removal
//


// Tracks on partially-sloped tiles: a way crossing a non-flat tile
// builds when the simulator's get_vmove() samples agree at both
// endpoints AND those heights match the neighbour ground.
function test_way_rail_build_through_partial_slope()
{
	local pl      = player_x(0)
	local rail    = way_desc_x.get_available_ways(wt_rail, st_flat)[0]
	local remover = command_x(tool_remove_way)

	ASSERT_TRUE(rail != null)

	// Set a slope, attempt a build that's expected to succeed, verify
	// the planted ribi pattern, then roll back the way and the slope.
	local check_admit = function(slope_pos, slope_val, from, to, anchor, pattern) {
		ASSERT_EQUAL(command_x.set_slope(pl, slope_pos, slope_val), null)
		ASSERT_EQUAL(command_x.build_way(pl, from, to, rail, true), null)
		ASSERT_WAY_PATTERN(wt_rail, anchor, pattern)
		ASSERT_EQUAL(remover.work(pl, tile_x(from.x, from.y, from.z), to, "" + wt_rail), null)
		ASSERT_EQUAL(command_x.set_slope(pl, slope_pos, slope.flat), null)
	}

	// Set a slope, attempt a build that's expected to fail, verify
	// nothing was planted on either endpoint, then roll back the slope.
	local check_reject = function(slope_pos, slope_val, from, to) {
		ASSERT_EQUAL(command_x.set_slope(pl, slope_pos, slope_val), null)
		ASSERT_EQUAL(command_x.build_way(pl, from, to, rail, true), "")
		ASSERT_EQUAL(tile_x(from.x, from.y, from.z).get_way_dirs(wt_rail), 0)
		ASSERT_EQUAL(tile_x(to.x,   to.y,   to.z  ).get_way_dirs(wt_rail), 0)
		ASSERT_EQUAL(command_x.set_slope(pl, slope_pos, slope.flat), null)
	}

	local ew_saddle = HEX_SLOPE(1, 0, 0, 1, 0, 0) // E, W corners raised
	local e_corner  = HEX_SLOPE(1, 0, 0, 0, 0, 0) // only E corner raised
	local se_corner = HEX_SLOPE(0, 1, 0, 0, 0, 0) // only SE corner raised
	local sw_corner = HEX_SLOPE(0, 0, 1, 0, 0, 0) // only SW corner raised
	local nw_corner = HEX_SLOPE(0, 0, 0, 0, 1, 0) // only NW corner raised
	local ne_corner = HEX_SLOPE(0, 0, 0, 0, 0, 1) // only NE corner raised
	local nw_edge   = HEX_SLOPE(1, 1, 0, 0, 0, 0) // E + SE = NW-SE ramp

	local ns_pattern = [
	    [0,  2, 0],
	    [0, 18, 0],
	    [0, 16, 0],
	]

	// Saddle, NS axis: NW and SE samples both at 0 → flat chord at z=0.
	check_admit(coord3d(4, 4, 0), ew_saddle,
	            coord3d(4, 3, 0), coord3d(4, 5, 0),
	            coord3d(3, 3, 0), ns_pattern)

	// Saddle, NW-SE axis: both edge intervals span [0..1] (each edge has
	// one raised vertex and one flat one).  The chord rule picks the
	// lowest H that fits — z=0 — so the way stays flat with the flat
	// neighbours, threading between the two raised vertices.
	check_admit(coord3d(8, 4, 0), ew_saddle,
	            coord3d(7, 4, 0), coord3d(9, 4, 0),
	            coord3d(7, 4, 0), [[1, 9, 8]])

	// Single raised corner, NS axis: corner E sits off the way; NW and
	// SE samples agree at 0.
	check_admit(coord3d(12, 4, 0), e_corner,
	            coord3d(12, 3, 0), coord3d(12, 5, 0),
	            coord3d(11, 3, 0), ns_pattern)

	// Single raised corner on the N-S axis edge (NE / NW / SE / SW): the
	// raised vertex sits at one endpoint of either the N or S edge, so
	// the way's body — running through the tile centre — passes by it
	// without touching.  All four mirror-related cases must admit a flat
	// chord at z=0; before the chord rule was made symmetric across the
	// two corners of each edge, NW and SE rejected while NE and SW
	// admitted, even though the configurations are mirror images of one
	// another across the N-S axis.
	check_admit(coord3d(4, 8, 0), se_corner,
	            coord3d(4, 7, 0), coord3d(4, 9, 0),
	            coord3d(3, 7, 0), ns_pattern)
	check_admit(coord3d(8, 8, 0), sw_corner,
	            coord3d(8, 7, 0), coord3d(8, 9, 0),
	            coord3d(7, 7, 0), ns_pattern)
	check_admit(coord3d(12, 8, 0), nw_corner,
	            coord3d(12, 7, 0), coord3d(12, 9, 0),
	            coord3d(11, 7, 0), ns_pattern)
	check_admit(coord3d(4, 11, 0), ne_corner,
	            coord3d(4, 10, 0), coord3d(4, 12, 0),
	            coord3d(3, 10, 0), ns_pattern)

	// nw_edge ramp, NE-SW axis (perpendicular to its natural ramp):
	// admitted as a side chord — NE and SW samples both at 0.
	check_admit(coord3d(12, 6, 0), nw_edge,
	            coord3d(11, 7, 0), coord3d(13, 5, 0),
	            coord3d(11, 5, 0), [
	                [ 0,  0,  4],
	                [ 0, 36,  0],
	                [32,  0,  0],
	            ])

	// nw_edge ramp, NW-SE axis: NW sample at 0, SE sample at 1 — the
	// way ramps from z=0 to z=1, but the SE neighbour is flat at z=0.
	check_reject(coord3d(12, 6, 0), nw_edge,
	             coord3d(11, 6, 0), coord3d(13, 6, 0))

	RESET_ALL_PLAYER_FUNDS()
}


// Rail building onto a sloped tile from a direction that doesn't sit
// on the slope's gradient or any of its admissible chord axes.  Raise
// T's NW and W vertices: T ends up with W + NW raised (gradient runs
// NW high → SE low).  Rail T → NW lies on the NW-SE ramp axis and
// builds.  Rail S → T joining the existing T → NW would leave T
// carrying ribi NW + S — a bend mixing the NW ramp axis with the N
// chord axis — which slope_allows_ribi rejects (the body can't be
// flat at z=0 for the chord and tilted on the ramp at the same time).
function test_way_rail_terminate_on_slope_off_gradient()
{
	local pl      = player_x(0)
	local rail    = way_desc_x.get_available_ways(wt_rail, st_flat)[0]
	local remover = command_x(tool_remove_way)
	ASSERT_TRUE(rail != null)

	local T  = coord3d(8, 8, 0)
	local NW = coord3d(7, 8, 0) // T + neighbours[3]
	local S  = coord3d(8, 9, 0) // T + neighbours[1]

	ASSERT_EQUAL(command_x.grid_raise_at_corner(pl, T, hex_corner.NW), null)
	ASSERT_EQUAL(command_x.grid_raise_at_corner(pl, T, hex_corner.W),  null)
	ASSERT_EQUAL(tile_x(T.x, T.y, T.z).get_slope(), HEX_SLOPE(0, 0, 0, 1, 1, 0))

	ASSERT_EQUAL(command_x.build_way(pl, T, NW, rail, true), null)
	ASSERT_EQUAL(command_x.build_way(pl, S, T, rail, true), "")

	// cleanup
	ASSERT_EQUAL(remover.work(pl, tile_x(T.x, T.y, T.z), NW, "" + wt_rail), null)
	ASSERT_EQUAL(command_x.grid_lower_at_corner(pl, T, hex_corner.NW), null)
	ASSERT_EQUAL(command_x.grid_lower_at_corner(pl, T, hex_corner.W),  null)

	RESET_ALL_PLAYER_FUNDS()
}


// Same setup as the bend rejection above, but no prior rail: a stub
// from the S neighbour into T's S edge sits at z=0 on the flat half
// of T (S, SE, SW corners all at 0).  The rail body is flat on the
// chord at z=0; the raised NW half of T is just unoccupied terrain
// the way doesn't touch.  Player-intuitively buildable, and the
// rule has to admit it: a half-chord stub is geometrically fine on
// its own, even though combining it with a ramp on the other axis
// is not.
function test_way_rail_terminate_on_slope_chord_stub()
{
	local pl      = player_x(0)
	local rail    = way_desc_x.get_available_ways(wt_rail, st_flat)[0]
	local remover = command_x(tool_remove_way)
	ASSERT_TRUE(rail != null)

	local T = coord3d(8, 8, 0)
	local S = coord3d(8, 9, 0)

	ASSERT_EQUAL(command_x.grid_raise_at_corner(pl, T, hex_corner.NW), null)
	ASSERT_EQUAL(command_x.grid_raise_at_corner(pl, T, hex_corner.W),  null)
	ASSERT_EQUAL(tile_x(T.x, T.y, T.z).get_slope(), HEX_SLOPE(0, 0, 0, 1, 1, 0))

	ASSERT_EQUAL(command_x.build_way(pl, S, T, rail, true), null)

	// cleanup
	ASSERT_EQUAL(remover.work(pl, tile_x(S.x, S.y, S.z), T, "" + wt_rail), null)
	ASSERT_EQUAL(command_x.grid_lower_at_corner(pl, T, hex_corner.NW), null)
	ASSERT_EQUAL(command_x.grid_lower_at_corner(pl, T, hex_corner.W),  null)

	RESET_ALL_PLAYER_FUNDS()
}
