//
// This file is part of the Simutrans project under the Artistic License.
// (see LICENSE.txt)
//


//
// Tests for rail way building / removal
//


// Tracks on partially-sloped tiles: a way crossing a non-flat tile
// builds iff every edge it traverses on that tile is internally level
// (both endpoint corners at the same height) — a half-raised edge
// runs the slope sideways across the rail body, which has no flat or
// single-ramp pose on it.
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
	local nw_edge   = HEX_SLOPE(1, 1, 0, 0, 0, 0) // E + SE = NW-SE ramp

	local ns_pattern = [
	    [0,  2, 0],
	    [0, 18, 0],
	    [0, 16, 0],
	]

	// Saddle, NS axis: N edge (NW, NE) and S edge (SE, SW) are both
	// fully flat at 0; the raised E and W corners sit off the axis.
	check_admit(coord3d(4, 4, 0), ew_saddle,
	            coord3d(4, 3, 0), coord3d(4, 5, 0),
	            coord3d(3, 3, 0), ns_pattern)

	// Saddle, NW-SE axis: both crossed edges (NW: W=1, NW=0; SE: E=1,
	// SE=0) are half-raised — the slope runs sideways across the rail
	// on both, no flat or ramp pose fits.
	check_reject(coord3d(8, 4, 0), ew_saddle,
	             coord3d(7, 4, 0), coord3d(9, 4, 0))

	// Single raised corner off the N-S axis: corner E sits between the
	// NE and SE edges, neither of which the N-S ribi crosses; both
	// crossed edges (N, S) are flat at 0.
	check_admit(coord3d(12, 4, 0), e_corner,
	            coord3d(12, 3, 0), coord3d(12, 5, 0),
	            coord3d(11, 3, 0), ns_pattern)

	// Single raised corner sitting at one endpoint of an N-S axis edge:
	// that edge becomes half-raised — slope sideways — and the build
	// must reject.  Sampled on the SE corner; the SW / NW / NE mirrors
	// are mechanically the same.
	check_reject(coord3d(4, 8, 0), se_corner,
	             coord3d(4, 7, 0), coord3d(4, 9, 0))

	// nw_edge ramp, NE-SW axis (perpendicular to its natural ramp):
	// NE edge (NE=0, E=1) is half-raised, so the side-chord pose the
	// old chord-overlap rule admitted is rejected — the rail can't sit
	// flat across the slope's gradient.
	check_reject(coord3d(12, 6, 0), nw_edge,
	             coord3d(11, 7, 0), coord3d(13, 5, 0))

	// nw_edge ramp, NW-SE axis (the slope's gradient axis): NW edge
	// (W=0, NW=0) flat at 0, SE edge (E=1, SE=1) flat at 1 — adjacent
	// heights, the body ramps uniformly.  The slope itself admits, but
	// the cross-tile vmove check rejects: the SE neighbour (13,6) is
	// flat at z=0, not z=1.
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


// Raising a single shared vertex lifts a corner on each of the three
// tiles around it.  T = (5, 5), SW neighbour = (4, 6); they share the
// W vertex of T (= NE vertex of (4, 6) = SE vertex of (4, 5)).  After
// raising it, T has only its W corner up and (4, 6) has only its NE
// corner up — and that vertex sits at one endpoint of the SW edge of
// T / NE edge of (4, 6) that the rail would cross.  The rail's body
// would have to bridge a flat corner and a raised corner along its
// own edge crossing on both tiles, which no flat or single-ramp pose
// satisfies, so the build must be rejected.
function test_way_rail_build_across_half_raised_edge()
{
	local pl   = player_x(0)
	local rail = way_desc_x.get_available_ways(wt_rail, st_flat)[0]
	ASSERT_TRUE(rail != null)

	local T  = coord3d(5, 5, 0)
	local SW = coord3d(4, 6, 0) // T + neighbours[2]

	ASSERT_EQUAL(command_x.grid_raise_at_corner(pl, T, hex_corner.W), null)
	ASSERT_EQUAL(tile_x(T.x,  T.y,  T.z ).get_slope(), HEX_SLOPE(0, 0, 0, 1, 0, 0))
	ASSERT_EQUAL(tile_x(SW.x, SW.y, SW.z).get_slope(), HEX_SLOPE(0, 0, 0, 0, 0, 1))

	ASSERT_EQUAL(command_x.build_way(pl, T, SW, rail, true), "")
	ASSERT_EQUAL(tile_x(T.x,  T.y,  T.z ).get_way_dirs(wt_rail), 0)
	ASSERT_EQUAL(tile_x(SW.x, SW.y, SW.z).get_way_dirs(wt_rail), 0)

	// cleanup
	ASSERT_EQUAL(command_x.grid_lower_at_corner(pl, T, hex_corner.W), null)

	RESET_ALL_PLAYER_FUNDS()
}
