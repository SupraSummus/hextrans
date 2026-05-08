//
// This file is part of the Simutrans project under the Artistic License.
// (see LICENSE.txt)
//


//
// Tests for terraforming terrain
//


function test_terraform_raise_lower_land()
{

	// TODO check terrain height

	local err = command_x.grid_raise(player_x(0), coord(3, 2))
	ASSERT_EQUAL(err, "Zu nah am Kartenrand") // TODO Fix error message?

	// Raise + lower at one vertex never round-trips: raise's "≥ H − 1"
	// propagation pushes the 3 hex-vertex-neighbours up, but lower's
	// "≤ H + 1" propagation never pulls them back (same as upstream
	// square).  Pull the cone back to ground explicitly.  V = NW of
	// (3, 2) is canonical (2, 2) E; vertex_neighbours puts its 3
	// neighbours at the SE corners of (2, 2), (2, 1), (3, 1).
	ASSERT_EQUAL(command_x.grid_raise(player_x(0), coord3d(3, 2, 100)), null)
	ASSERT_EQUAL(command_x.grid_raise(player_x(0), coord3d(3, 2, 0)), null)
	ASSERT_EQUAL(command_x.grid_lower(player_x(0), coord3d(3, 2, 0)), null)
	ASSERT_EQUAL(command_x.grid_lower(player_x(0), coord3d(3, 2, 0)), null)
	ASSERT_EQUAL(command_x.grid_lower_at_corner(player_x(0), coord3d(2, 2, 0), hex_corner.SE), null)
	ASSERT_EQUAL(command_x.grid_lower_at_corner(player_x(0), coord3d(2, 1, 0), hex_corner.SE), null)
	ASSERT_EQUAL(command_x.grid_lower_at_corner(player_x(0), coord3d(3, 1, 0), hex_corner.SE), null)

	RESET_ALL_PLAYER_FUNDS()
}


function test_terraform_raise_lower_land_at_map_border()
{

	// TODO Check terrain height

	ASSERT_EQUAL(command_x.grid_raise(player_x(0), coord(0, 0)), "Zu nah am Kartenrand")
	ASSERT_EQUAL(command_x.grid_raise(player_x(0), coord3d(0, 0, 100)), null)
	ASSERT_EQUAL(command_x.grid_raise(player_x(0), coord3d(0, 0, 0)), null)
	ASSERT_EQUAL(command_x.grid_raise(player_x(0), coord3d(0, 0, 0)), null)

	ASSERT_EQUAL(command_x.grid_lower(player_x(0), coord3d(0, 0, 0)), null)
	ASSERT_EQUAL(command_x.grid_lower(player_x(0), coord3d(0, 0, 1)), null)
	ASSERT_EQUAL(command_x.grid_lower(player_x(0), coord3d(0, 0, 1)), null)
	ASSERT_EQUAL(command_x.grid_lower(player_x(0), coord3d(0, 0, 1)), null)
	ASSERT_EQUAL(command_x.grid_lower(player_x(0), coord3d(0, 0, 1)), null)

	// clean up
	ASSERT_EQUAL(command_x.grid_raise(player_x(0), coord3d(0, 0, 0)), null)
	ASSERT_EQUAL(command_x.grid_raise(player_x(0), coord3d(0, 0, 0)), null)

	RESET_ALL_PLAYER_FUNDS()
}


function test_terraform_raise_lower_land_at_water_center()
{
	local clim  = command_x(tool_set_climate)

	// Lowering the NW vertex of (3,3) — inside the water block — keeps
	// the four tiles water across repeated lower calls.  (Upstream
	// labelled the first block "raise" but called grid_lower in both;
	// under hex grid_raise here would lift (3,3) out of water, so the
	// idempotent-lower invariant is the one that actually survives.)
	for (local pass = 0; pass < 2; ++pass) {
		clim.work(player_x(0), coord3d(2, 3, 0), coord3d(3, 2, 0), "" + cl_water)

		ASSERT_EQUAL(command_x.grid_lower(player_x(0), coord3d(3, 3, 0)), "")

		ASSERT_TRUE(tile_x(2, 2, 0).is_water())
		ASSERT_TRUE(tile_x(3, 2, 0).is_water())
		ASSERT_TRUE(tile_x(2, 3, 0).is_water())
		ASSERT_TRUE(tile_x(3, 3, 0).is_water())
	}

	// Drain the water block back to flat land for the next test.  The
	// grid_lower above returned "" (interior-vertex lower into water is
	// gated), so terrain is unchanged — but `clim.work` to cl_mediterran
	// converts the wasser_t tiles to boden_t, and set_slope ensures the
	// final shape regardless.
	clim.work(player_x(0), coord3d(2, 3, 0), coord3d(3, 2, 0), "" + cl_mediterran)
	for (local q = 2; q <= 3; ++q) {
		for (local r = 2; r <= 3; ++r) {
			command_x.set_slope(player_x(0), coord3d(q, r, 0), slope.flat)
		}
	}

	RESET_ALL_PLAYER_FUNDS()
}


// Test scaffold shared by `_at_water_corner` (1-of-3 owner vertices)
// and `_at_water_edge` (2-of-3).  The 2x2 axial water block is at
// (2,2)..(3,3); each vertex touching it has 3 owner tiles, of which
// 1, 2, or 3 lie inside the block.  The 3-of-3 case is covered by
// `_at_water_center`.
//
// Each case names a vertex via (`tile`, `corner`) and the per-block-tile
// effect of raising it: `hit` lists [block_tile, raised_corner] pairs,
// one per block-internal owner.  We re-flatten the 4x4 enclosing region
// (prior cascades may have lifted neighbour corners), re-water the 2x2,
// raise, and check that each `hit` tile shows exactly the named single
// corner up while the unlisted block tiles stay flat water.
function check_water_block_raise(cases)
{
	local clim = command_x(tool_set_climate)
	local pl   = player_x(0)

	// Hex corner index → slope value for "single-corner up by 1".
	// Indexed by `hex_corner_t` (E=0, SE=1, SW=2, W=3, NW=4, NE=5).
	local single_corner_up = [
		slope.raised_E, slope.raised_SE, slope.raised_SW,
		slope.raised_W, slope.raised_NW, slope.raised_NE,
	]

	foreach (c in cases) {
		flatten_water_block_region(pl)
		clim.work(pl, coord3d(2, 3, 0), coord3d(3, 2, 0), "" + cl_water)

		command_x.grid_raise_at_corner(pl, c.tile, c.corner)

		for (local q = 2; q <= 3; ++q) {
			for (local r = 2; r <= 3; ++r) {
				local hit_corner = -1
				foreach (h in c.hit) {
					if (h[0].x == q && h[0].y == r) hit_corner = h[1]
				}
				local tile = tile_x(q, r, 0)
				if (hit_corner < 0) {
					ASSERT_TRUE(tile.is_water())
				}
				else {
					ASSERT_FALSE(tile.is_water())
					ASSERT_EQUAL(tile.get_slope(), single_corner_up[hit_corner])
				}
			}
		}
	}

	// Restore flat mediterran for the next test: drop any leftover
	// raised corners and convert the still-wasser_t tiles to boden.
	flatten_water_block_region(pl)
	ASSERT_EQUAL(clim.work(pl, coord3d(2, 3, 0), coord3d(3, 2, 0), "" + cl_mediterran), null)

	RESET_ALL_PLAYER_FUNDS()
}


function flatten_water_block_region(pl)
{
	for (local q = 1; q <= 4; ++q) {
		for (local r = 1; r <= 4; ++r) {
			command_x.set_slope(pl, coord3d(q, r, 0), slope.flat)
		}
	}
}


function test_terraform_raise_lower_land_at_water_corner()
{
	// 1-of-3 vertices: each touches exactly one block tile.  NW of (q, r)
	// is the vertex (q, r)NW = (q-1, r)E = (q, r-1)SW; pick (q, r) so
	// neither neighbour lies inside the block.
	check_water_block_raise([
		{ tile = coord3d(2, 2, 0), corner = hex_corner.NW, hit = [[coord(2, 2), hex_corner.NW]] },
		{ tile = coord3d(4, 2, 0), corner = hex_corner.NW, hit = [[coord(3, 2), hex_corner.E ]] },
		{ tile = coord3d(2, 4, 0), corner = hex_corner.NW, hit = [[coord(2, 3), hex_corner.SW]] },
		{ tile = coord3d(4, 3, 0), corner = hex_corner.NW, hit = [[coord(3, 3), hex_corner.E ]] },
	])
}


function test_terraform_raise_lower_land_at_water_edge()
{
	// 2-of-3 vertices: each touches two block tiles.  The 2x2 axial block
	// has 4 such vertices — two NW-of-block-tile (between SE-adjacent
	// pairs) and two SE-of-block-tile (between S-adjacent pairs).
	check_water_block_raise([
		{ tile = coord3d(3, 2, 0), corner = hex_corner.NW, hit = [
			[coord(3, 2), hex_corner.NW],
			[coord(2, 2), hex_corner.E ],
		] },
		{ tile = coord3d(2, 3, 0), corner = hex_corner.NW, hit = [
			[coord(2, 3), hex_corner.NW],
			[coord(2, 2), hex_corner.SW],
		] },
		{ tile = coord3d(3, 2, 0), corner = hex_corner.SE, hit = [
			[coord(3, 2), hex_corner.SE],
			[coord(3, 3), hex_corner.NE],
		] },
		{ tile = coord3d(2, 3, 0), corner = hex_corner.SE, hit = [
			[coord(2, 3), hex_corner.SE],
			[coord(3, 3), hex_corner.W ],
		] },
	])
}


function test_terraform_raise_lower_land_below_way()
{
	local pl = player_x(0)
	local road_desc = way_desc_x.get_available_ways(wt_road, st_flat)[0]
	local setslope = command_x.set_slope

	ASSERT_EQUAL(command_x.build_way(pl, coord3d(4, 2, 0), coord3d(4, 4, 0), road_desc, true), null)

	// HEX-PORT: grid_raise(gx, gy) lowers the NW corner of tile (gx, gy)
	// on hex, so it touches a way tile when (gx, gy) is one of the way
	// tiles OR a direct neighbour through the hex propagation rules.
	// The SE-of-way grid-points (5, 5) no longer land on a way tile
	// under hex because hex vertex-sharing is 3-wide, not 4-wide.

	// raise below way
	{
		ASSERT_EQUAL(command_x.grid_raise(pl, coord3d(4, 2, 0)), "Tile not empty.")
		ASSERT_EQUAL(command_x.grid_raise(pl, coord3d(4, 3, 0)), "Tile not empty.")
		ASSERT_EQUAL(command_x.grid_raise(pl, coord3d(4, 4, 0)), "Tile not empty.")
		ASSERT_EQUAL(command_x.grid_raise(pl, coord3d(4, 5, 0)), "Tile not empty.")
		ASSERT_EQUAL(command_x.grid_raise(pl, coord3d(5, 2, 0)), "Tile not empty.")
		ASSERT_EQUAL(command_x.grid_raise(pl, coord3d(5, 3, 0)), "Tile not empty.")
		ASSERT_EQUAL(command_x.grid_raise(pl, coord3d(5, 4, 0)), "Tile not empty.")
	}

	// and lower
	{
		ASSERT_EQUAL(command_x.grid_lower(pl, coord3d(4, 2, 0)), "Tile not empty.")
		ASSERT_EQUAL(command_x.grid_lower(pl, coord3d(4, 3, 0)), "Tile not empty.")
		ASSERT_EQUAL(command_x.grid_lower(pl, coord3d(4, 4, 0)), "Tile not empty.")
		ASSERT_EQUAL(command_x.grid_lower(pl, coord3d(4, 5, 0)), "Tile not empty.")
		ASSERT_EQUAL(command_x.grid_lower(pl, coord3d(5, 2, 0)), "Tile not empty.")
		ASSERT_EQUAL(command_x.grid_lower(pl, coord3d(5, 3, 0)), "Tile not empty.")
		ASSERT_EQUAL(command_x.grid_lower(pl, coord3d(5, 4, 0)), "Tile not empty.")
	}

	// set slope up
	{
		// 2× edge slopes are not way-buildable (`slope_t::is_way` rejects
		// double-corner slopes), so direct setslope to 2*south under a way
		// is gated even though the engine's all_up progression to 2× is
		// already gated separately by `has_double_slopes()`.
		ASSERT_EQUAL(setslope(pl, coord3d(4, 2, 0),   slope.south_narrow), null)
		ASSERT_EQUAL(setslope(pl, coord3d(4, 2, 0),   slope.south_narrow), "")
		ASSERT_EQUAL(setslope(pl, coord3d(4, 2, 0), 2*slope.south_narrow), "Tile not empty.")
		ASSERT_EQUAL(setslope(pl, coord3d(4, 2, 0), slope.all_down_slope), null)

		ASSERT_EQUAL(setslope(pl, coord3d(4, 4, 0),   slope.north_narrow), null)
		ASSERT_EQUAL(setslope(pl, coord3d(4, 4, 0),   slope.north_narrow), "")
		ASSERT_EQUAL(setslope(pl, coord3d(4, 4, 0), 2*slope.north_narrow), "Tile not empty.")
		ASSERT_EQUAL(setslope(pl, coord3d(4, 4, 0), slope.all_down_slope), null)

		// First all_up_slope on a flat way tile makes the way the slope
		// hinge — single-height edge slope.  A second all_up_slope would
		// progress to 2× that edge, but 2× edges are no longer
		// way-buildable (has_double_slopes() == false), so the
		// terraformer rejects.  One all_down_slope undoes the lift.
		ASSERT_EQUAL(setslope(pl, coord3d(4, 2, 0), slope.all_up_slope), null)
		ASSERT_EQUAL(setslope(pl, coord3d(4, 2, 0), slope.all_up_slope), "Tile not empty.")
		ASSERT_EQUAL(setslope(pl, coord3d(4, 2, 0), slope.all_down_slope), null)

		ASSERT_EQUAL(setslope(pl, coord3d(4, 4, 0), slope.all_up_slope), null)
		ASSERT_EQUAL(setslope(pl, coord3d(4, 4, 0), slope.all_up_slope), "Tile not empty.")
		ASSERT_EQUAL(setslope(pl, coord3d(4, 4, 0), slope.all_down_slope), null)
	}

	// set slope down
	{
		ASSERT_EQUAL(setslope(pl, coord3d(4, 2,  0), slope.all_down_slope), null)
		ASSERT_EQUAL(setslope(pl, coord3d(4, 2,  0), slope.all_down_slope), "")
		ASSERT_EQUAL(setslope(pl, coord3d(4, 2, -1), slope.all_up_slope), null)

		ASSERT_EQUAL(setslope(pl, coord3d(4, 4,  0), slope.all_down_slope), null)
		ASSERT_EQUAL(setslope(pl, coord3d(4, 4,  0), slope.all_down_slope), "")
		ASSERT_EQUAL(setslope(pl, coord3d(4, 4, -1), slope.all_up_slope), null)
	}

	// non-dead-end, should fail for all slopes
	{
		for (local sl = slope.flat+1; sl <= slope.all_down_slope; ++sl) {
			// slope.all_up_slope - 1 sits between the concrete slope
			// range and the terraform sentinels; the engine rejects it
			// out-of-range before tile validation runs.
			if (sl != slope.raised && sl != slope.all_up_slope - 1) {
				ASSERT_EQUAL(setslope(pl, coord3d(4, 3, 0), sl), "Tile not empty.")
			}
		}
	}

	ASSERT_EQUAL(command_x(tool_remove_way).work(pl, coord3d(4, 2, 0), coord3d(4, 4, 0), "" + wt_road), null)
	RESET_ALL_PLAYER_FUNDS()
}


/// Helper function: Raise / lower land along the edge of a rectangular region
function terraform_volcano(pl, pos, size, h)
{
	local raise = h > 0
	local tool = raise ? command_x.grid_raise : command_x.grid_lower

	for (local dz = 0; raise ? dz<h : dz>h; dz += raise?1:-1) {
		for (local i = 0; i<size; ++i) {
			tool(pl, pos + coord3d(i,      0,      dz))
			tool(pl, pos + coord3d(size,   i,      dz))
			tool(pl, pos + coord3d(size-i, size,   dz))
			tool(pl, pos + coord3d(0,      size-i, dz))
		}
	}
}

// test_terraform_raise_lower_water_level: HEX-PORT PENDING.
function test_terraform_raise_lower_water_level()
{
	local pl = player_x(0)
	local public_pl = player_x(1)

	// invalid default_param
	{
		local error_caught = false
		try {
			ASSERT_EQUAL(command_x(tool_change_water_height).work(pl, coord3d(0, 0, 0)), "")
		}
		catch (e) {
			error_caught = true
			ASSERT_EQUAL(e, "Error during initializing tool")
		}
		ASSERT_TRUE(error_caught)

		error_caught = false
		try {
			ASSERT_EQUAL(command_x(tool_change_water_height).work(pl, coord3d(0, 0, 0), ""), "")
		}
		catch (e) {
			error_caught = true
			ASSERT_EQUAL(e, "Error during initializing tool")
		}
		ASSERT_TRUE(error_caught)

		error_caught = false
		try {
			ASSERT_EQUAL(command_x(tool_change_water_height).work(pl, coord3d(0, 0, 0), "foo"), "")
		}
		catch (e) {
			error_caught = true
			ASSERT_EQUAL(e, "Error during initializing tool")
		}
		ASSERT_TRUE(error_caught)

		error_caught = false
		try {
			ASSERT_EQUAL(command_x(tool_change_water_height).work(pl, coord3d(0, 0, 0), ".5"), "")
		}
		catch (e) {
			error_caught = true
			ASSERT_EQUAL(e, "Error during initializing tool")
		}
		ASSERT_TRUE(error_caught)
	}

	// invalid pos
	{
		ASSERT_EQUAL(command_x(tool_change_water_height).work(pl, coord3d(-1, -1, -1), "0"), "Cannot alter water")
	}

	// near map border
	{
		ASSERT_EQUAL(command_x(tool_change_water_height).work(pl, coord3d(5, 5, 0), "0"), "Cannot alter water")
		ASSERT_EQUAL(command_x(tool_change_water_height).work(pl, coord3d(5, 5, 0), "0"), "Cannot alter water")
	}

	terraform_volcano(public_pl, coord3d(5, 5, 0), 3, 1)

	// lower water level on ground, should fail
	{
		local chg_water = command_x(tool_change_water_height)
		ASSERT_EQUAL(chg_water.work(pl, coord3d(6, 6, 0), "0"), "Cannot alter water")
		chg_water.set_flags(2)

		ASSERT_EQUAL(chg_water.work(pl, coord3d(6, 6, 0), "0"), "Cannot alter water")
		chg_water.set_flags(2)
		ASSERT_EQUAL(chg_water.work(pl, coord3d(6, 6, 0), "0"), "Cannot alter water")
	}

	// raise water level on ground, should make shallow water first (i.e. tile with cl_water)
	{
		local old_cash = pl.get_current_cash()
		local chg_water = command_x(tool_change_water_height)

		ASSERT_EQUAL(chg_water.work(pl, coord3d(6, 6, 0), "1"), null)
		ASSERT_EQUAL(square_x(6, 6).get_climate(), cl_water)
		ASSERT_EQUAL(pl.get_current_cash(), old_cash)

		ASSERT_EQUAL(chg_water.work(pl, coord3d(6, 6, 0), "1"), "Cannot alter water")
		ASSERT_EQUAL(pl.get_current_cash(), old_cash)

		ASSERT_EQUAL(chg_water.work(pl, coord3d(6, 6, 0), "0"), null)
		ASSERT_EQUAL(square_x(6, 6).get_climate(), cl_mediterran)
	}

	// make double height volcano
	terraform_volcano(public_pl, coord3d(5, 5, 0), 3, 1)

	{
		local chg_water = command_x(tool_change_water_height)
		ASSERT_EQUAL(chg_water.work(pl, coord3d(6, 6, 0), "1"), null)
		ASSERT_EQUAL(chg_water.work(pl, coord3d(6, 6, 0), "1"), null)
		ASSERT_EQUAL(tile_x(5, 6, 1).get_slope(), slope.east)

		ASSERT_EQUAL(square_x(6, 6).get_climate(), cl_water)
		ASSERT_TRUE(square_x(6, 6).get_tile_at_height(1) != null)

		ASSERT_EQUAL(chg_water.work(pl, coord3d(6, 6, 1), "0"), null)
		ASSERT_EQUAL(tile_x(5, 6, 0).get_slope(), 2*slope.east) // make sure single height slopes are changed to double again

		ASSERT_EQUAL(chg_water.work(pl, coord3d(6, 6, 0), "0"), null)
	}

	terraform_volcano(public_pl, coord3d(5, 5, 0), 3, -2)

	// make 1-high barrier
	terraform_volcano(public_pl, coord3d(2, 2, 0), 7, 2)
	terraform_volcano(public_pl, coord3d(5, 5, 0), 1, 1)
	terraform_volcano(public_pl, coord3d(3, 5, 0), 1, 1)
	terraform_volcano(public_pl, coord3d(7, 5, 0), 1, 1)

	//
	{
		local chg_water = command_x(tool_change_water_height)
		chg_water.set_flags(2) // ctrl

		ASSERT_EQUAL(chg_water.work(pl, coord3d(5, 3, 0), "1"), null)
		ASSERT_EQUAL(square_x(4, 3).get_climate(), cl_mediterran)
		ASSERT_EQUAL(square_x(5, 3).get_climate(), cl_water)
		ASSERT_EQUAL(square_x(6, 3).get_climate(), cl_mediterran)

		// FIXME this gives different water on the center tile vs other tiles?
		ASSERT_EQUAL(chg_water.work(pl, coord3d(5, 3, 0), "1"), null)
		ASSERT_EQUAL(square_x(3, 4).get_climate(), cl_water)
		ASSERT_EQUAL(square_x(3, 5).get_climate(), cl_mediterran)

		// cannot drain with ctrl active
		ASSERT_EQUAL(chg_water.work(pl, coord3d(5, 4, 1), "0"), "Cannot alter water")
		chg_water.set_flags(0)
		ASSERT_EQUAL(chg_water.work(pl, coord3d(5, 4, 1), "0"), null)

		ASSERT_EQUAL(chg_water.work(pl, coord3d(5, 3, 0), "1"), null)
		ASSERT_EQUAL(square_x(3, 5).get_climate(), cl_water)
		ASSERT_EQUAL(square_x(7, 5).get_climate(), cl_water)
		ASSERT_EQUAL(square_x(3, 7).get_climate(), cl_water)
		ASSERT_EQUAL(square_x(7, 7).get_climate(), cl_water)

		// Using tool on very shallow water should give 1-tile islands
		chg_water.set_flags(2)
		ASSERT_EQUAL(chg_water.work(pl, coord3d(5, 5, 1), "0"), null)
		ASSERT_EQUAL(square_x(5, 5).get_climate(), cl_mediterran)
		ASSERT_EQUAL(square_x(4, 5).get_climate(), cl_mediterran)
		ASSERT_EQUAL(square_x(6, 5).get_climate(), cl_mediterran)
		ASSERT_EQUAL(square_x(5, 4).get_climate(), cl_water)
		ASSERT_EQUAL(square_x(5, 6).get_climate(), cl_water)

		ASSERT_EQUAL(chg_water.work(pl, coord3d(5, 5, 1), "1"), null)

		// draining without ctrl should leave 2 small basins
		chg_water.set_flags(0)
		ASSERT_EQUAL(chg_water.work(pl, coord3d(5, 5, 1), "0"), null)
		ASSERT_EQUAL(square_x(5, 3).get_climate(), cl_water)
		ASSERT_EQUAL(square_x(5, 4).get_climate(), cl_mediterran)
		ASSERT_EQUAL(square_x(5, 6).get_climate(), cl_mediterran)
		ASSERT_EQUAL(square_x(5, 7).get_climate(), cl_water)

		ASSERT_EQUAL(chg_water.work(pl, coord3d(5, 7, 0), "0"), null)
		ASSERT_EQUAL(chg_water.work(pl, coord3d(5, 3, 0), "0"), null)
	}

	terraform_volcano(public_pl, coord3d(5, 5, 1), 1, 1)
	terraform_volcano(public_pl, coord3d(3, 5, 1), 1, 1)
	terraform_volcano(public_pl, coord3d(7, 5, 1), 1, 1)


	// water also spills across tile edges and diagonals if low enough
	{
		local chg_water = command_x(tool_change_water_height)

		command_x.grid_lower(pl, coord3d(5, 5, 2))
		command_x.grid_lower(pl, coord3d(6, 6, 2))

		ASSERT_EQUAL(chg_water.work(pl, coord3d(5, 7, 0), "1"), null)
		ASSERT_EQUAL(chg_water.work(pl, coord3d(5, 7, 1), "1"), null)

		ASSERT_EQUAL(square_x(6, 3).get_climate(), cl_water)

		// note: This also drains the other basin 1 level
		ASSERT_EQUAL(chg_water.work(pl, coord3d(5, 7, 1), "0"), null)
		ASSERT_EQUAL(square_x(5, 3).get_tile_at_height(1), null)
		ASSERT_EQUAL(chg_water.work(pl, coord3d(5, 7, 0), "0"), null)
		ASSERT_EQUAL(chg_water.work(pl, coord3d(5, 3, 0), "0"), null)

		command_x.grid_raise(pl, coord3d(5, 5, 1))
		command_x.grid_raise(pl, coord3d(6, 6, 1))
	}

	// clean up

	terraform_volcano(public_pl, coord3d(2, 2, 2), 7, -2)
	terraform_volcano(public_pl, coord3d(5, 5, 2), 1, -2)
	terraform_volcano(public_pl, coord3d(3, 5, 2), 1, -2)
	terraform_volcano(public_pl, coord3d(7, 5, 2), 1, -2)

	RESET_ALL_PLAYER_FUNDS()
}
