//
// This file is part of the Simutrans project under the Artistic License.
// (see LICENSE.txt)
//


//
// Tests for building and removal of tunnels
//


// Hill at (3,2) via raise_hex_tile (see test_helpers.nut for the
// 6-vertex recipe).  The 6 surrounding hex tiles end up with 2-corner
// edge slopes — (3,1) → north_narrow, (3,3) → south_narrow, (2,2) →
// northwest_narrow, (4,2) → southeast_narrow are the four the test
// uses; (4,1) and (2,3) get unused side slopes.
function test_way_tunnel_build_straight()
{
	local digger = command_x(tool_build_tunnel)
	local remover = command_x(tool_remover)
	local default_tunnel = tunnel_desc_x.get_available_tunnels(wt_road)[0]
	local pl = player_x(0)

	ASSERT_TRUE(default_tunnel != null)

	raise_hex_tile(pl, 3, 2, 0)

	// ctrl-dig builds single-tile mouths.  An unconnected mouth has
	// get_way_dirs == 0, indistinguishable from "tile has no way at
	// all" — assert on the tunnel object instead.
	{
		digger.set_flags(2)
		ASSERT_EQUAL(digger.work(pl, tile_x(3, 1, 0), default_tunnel.get_name()), null)
		ASSERT_TRUE(tile_x(3, 1, 0).find_object(mo_tunnel) != null)
		ASSERT_EQUAL(digger.work(pl, tile_x(3, 3, 0), default_tunnel.get_name()), null)
		ASSERT_TRUE(tile_x(3, 3, 0).find_object(mo_tunnel) != null)

		ASSERT_EQUAL(remover.work(pl, coord3d(3, 1, 0)), null)
		ASSERT_TRUE(tile_x(3, 1, 0).find_object(mo_tunnel) == null)
		ASSERT_EQUAL(remover.work(pl, coord3d(3, 3, 0)), null)
		ASSERT_TRUE(tile_x(3, 3, 0).find_object(mo_tunnel) == null)
		digger.set_flags(0)
	}

	// straight tunnel along S axis: dig at (3,1) auto-finds end (3,3).
	// Mouth at (3,1) faces S (ribi=2), middle at (3,2) is N|S=18, mouth
	// at (3,3) faces N (ribi=16).  Cells outside the tunnel hold 0:
	// any spurious way fails the assertion.
	{
		ASSERT_EQUAL(digger.work(pl, tile_x(3, 1, 0), default_tunnel.get_name()), null)

		ASSERT_WAY_PATTERN(wt_road, coord3d(0, 0, 0),
			[
				[0, 0, 0,  0, 0, 0, 0, 0],
				[0, 0, 0,  2, 0, 0, 0, 0],
				[0, 0, 0, 18, 0, 0, 0, 0],
				[0, 0, 0, 16, 0, 0, 0, 0],
				[0, 0, 0,  0, 0, 0, 0, 0],
				[0, 0, 0,  0, 0, 0, 0, 0],
				[0, 0, 0,  0, 0, 0, 0, 0],
				[0, 0, 0,  0, 0, 0, 0, 0],
			])

		// tunnel objects on both entrances; the through-tile (3,2,0) has
		// only the way under it, no tunnel object.
		local tunnel = tile_x(3, 1, 0).find_object(mo_tunnel)
		ASSERT_TRUE(tunnel != null)
		ASSERT_EQUAL(tunnel.get_desc().get_name(), default_tunnel.get_name())
		ASSERT_TRUE(tile_x(3, 3, 0).find_object(mo_tunnel) != null)
	}

	// branch: NW-axis approach at (2,2) joins the tunnel at (3,2).
	// (2,2) ribi = SE = 1; (3,2) gains the NW connection so ribi =
	// N|S|NW = 26.
	{
		ASSERT_EQUAL(digger.work(pl, tile_x(2, 2, 0), default_tunnel.get_name()), null)
		ASSERT_TRUE(tile_x(2, 2, 0).find_object(mo_tunnel) != null)

		ASSERT_WAY_PATTERN(wt_road, coord3d(0, 0, 0),
			[
				[0, 0, 0,  0, 0, 0, 0, 0],
				[0, 0, 0,  2, 0, 0, 0, 0],
				[0, 0, 1, 26, 0, 0, 0, 0],
				[0, 0, 0, 16, 0, 0, 0, 0],
				[0, 0, 0,  0, 0, 0, 0, 0],
				[0, 0, 0,  0, 0, 0, 0, 0],
				[0, 0, 0,  0, 0, 0, 0, 0],
				[0, 0, 0,  0, 0, 0, 0, 0],
			])

		// ctrl-dig at (4,2): isolated mouth alongside the branched tunnel
		digger.set_flags(2)
		ASSERT_EQUAL(digger.work(pl, tile_x(4, 2, 0), default_tunnel.get_name()), null)
		ASSERT_TRUE(tile_x(4, 2, 0).find_object(mo_tunnel) != null)
		digger.set_flags(0)

		// remove the lone (4,2) entrance: branched tunnel stays intact
		ASSERT_EQUAL(remover.work(pl, coord3d(4, 2, 0)), null)
		ASSERT_TRUE(tile_x(4, 2, 0).find_object(mo_tunnel) == null)
		ASSERT_WAY_PATTERN(wt_road, coord3d(0, 0, 0),
			[
				[0, 0, 0,  0, 0, 0, 0, 0],
				[0, 0, 0,  2, 0, 0, 0, 0],
				[0, 0, 1, 26, 0, 0, 0, 0],
				[0, 0, 0, 16, 0, 0, 0, 0],
				[0, 0, 0,  0, 0, 0, 0, 0],
				[0, 0, 0,  0, 0, 0, 0, 0],
				[0, 0, 0,  0, 0, 0, 0, 0],
				[0, 0, 0,  0, 0, 0, 0, 0],
			])

		// remove tunnel network with branches: requires ctrl
		local err = remover.work(pl, coord3d(3, 3, 0))
		ASSERT_EQUAL(err, "This tunnel branches. You can try Control+Click to remove.")

		remover.set_flags(2)
		ASSERT_EQUAL(remover.work(pl, coord3d(3, 3, 0)), null)
		ASSERT_TRUE(tile_x(3, 1, 0).find_object(mo_tunnel) == null)
		ASSERT_TRUE(tile_x(3, 3, 0).find_object(mo_tunnel) == null)
		ASSERT_TRUE(tile_x(2, 2, 0).find_object(mo_tunnel) == null)
		remover.set_flags(0)
	}

	lower_hex_tile(pl, 3, 2, 0)
	RESET_ALL_PLAYER_FUNDS()
}


// 2-tile S-axis hill at (1,1)+(1,2) so a tunnel ctrl-dragged from
// (1,0,0) into the hill has somewhere underground to live.  Tunnel is
// rail (does not support double slopes — the test exercises the
// rejection on the second consecutive all_down).  Patterns use sharp
// 0s so any spurious ribi anywhere on the 8x8 fails the assertion.
//
// HEX-PORT TODO: the original test had a final sub-block that raised
// terrain around (1,2)+(1,3) at z=1 to force a step-2 all_up at
// (1,1,0) and verify the rejection.  Under hex `has_double_slopes()`
// is hardcoded false (`way_desc.h:205`), so any all_up that would
// create a 2-step slope already fails for every way type — the
// invariant survives, but its specific scaffold needs the same
// treatment as `_above_tunnel_slope` / `_across_tunnel_slope`.
// Lands together with that stub's restoration.
function test_way_tunnel_build_up_down()
{
	local digger = command_x(tool_build_tunnel)
	local setslope = command_x.set_slope
	local remover = command_x(tool_remover)
	local default_tunnel = tunnel_desc_x.get_available_tunnels(wt_rail)[0]
	local pl = player_x(0)

	ASSERT_TRUE(default_tunnel != null)

	raise_hex_tile_pair_S(pl, 1, 1, 0)

	digger.set_flags(2) // ctrl
	ASSERT_EQUAL(digger.work(pl, coord3d(1, 0, 0), default_tunnel.get_name()), null)
	ASSERT_EQUAL(digger.work(pl, coord3d(1, 0, 0), coord3d(1, 1, 0), default_tunnel.get_name()), null)

	// (1,0,0): mouth ribi=S=2; (1,1,0): underground way ribi=N=16.
	local two_tile_z0 = [
		[0, 2, 0, 0, 0, 0, 0, 0],
		[0,16, 0, 0, 0, 0, 0, 0],
		[0, 0, 0, 0, 0, 0, 0, 0],
		[0, 0, 0, 0, 0, 0, 0, 0],
		[0, 0, 0, 0, 0, 0, 0, 0],
		[0, 0, 0, 0, 0, 0, 0, 0],
		[0, 0, 0, 0, 0, 0, 0, 0],
		[0, 0, 0, 0, 0, 0, 0, 0],
	]
	ASSERT_WAY_PATTERN(wt_rail, coord3d(0, 0, 0), two_tile_z0)

	// invalid slope param: underground tiles only accept all_up /
	// all_down.  Pattern unchanged → confirms the rejected call had
	// no side effect.
	{
		ASSERT_EQUAL(setslope(pl, coord3d(1, 1, 0), 42), "Only up and down movement in the underground!")
		ASSERT_WAY_PATTERN(wt_rail, coord3d(0, 0, 0), two_tile_z0)
	}

	// all_up rejected: surface tile (1,1,1) is in the way.
	{
		ASSERT_EQUAL(setslope(pl, coord3d(1, 1, 0), slope.all_up_slope), "Tile not empty.")
		ASSERT_WAY_PATTERN(wt_rail, coord3d(0, 0, 0), two_tile_z0)
	}

	// (1,1,-1) holds a single underground way ribi=N=16, used both
	// post-all_down and to verify the second (rejected) all_down
	// leaves the way in place.
	local one_tile_z_minus_1 = [
		[0, 0, 0, 0, 0, 0, 0, 0],
		[0,16, 0, 0, 0, 0, 0, 0],
		[0, 0, 0, 0, 0, 0, 0, 0],
		[0, 0, 0, 0, 0, 0, 0, 0],
		[0, 0, 0, 0, 0, 0, 0, 0],
		[0, 0, 0, 0, 0, 0, 0, 0],
		[0, 0, 0, 0, 0, 0, 0, 0],
		[0, 0, 0, 0, 0, 0, 0, 0],
	]

	// all_down: (1,1,0) underground way drops to (1,1,-1).  Mouth at
	// (1,0,0) keeps its S ribi (the entrance still goes underground,
	// just at z=-1 now).
	{
		local old_maint = pl.get_current_maintenance()
		ASSERT_EQUAL(setslope(pl, coord3d(1, 1, 0), slope.all_down_slope), null)

		ASSERT_WAY_PATTERN(wt_rail, coord3d(0, 0, 0),
			[
				[0, 2, 0, 0, 0, 0, 0, 0],
				[0, 0, 0, 0, 0, 0, 0, 0],
				[0, 0, 0, 0, 0, 0, 0, 0],
				[0, 0, 0, 0, 0, 0, 0, 0],
				[0, 0, 0, 0, 0, 0, 0, 0],
				[0, 0, 0, 0, 0, 0, 0, 0],
				[0, 0, 0, 0, 0, 0, 0, 0],
				[0, 0, 0, 0, 0, 0, 0, 0],
			])
		ASSERT_WAY_PATTERN(wt_rail, coord3d(0, 0, -1), one_tile_z_minus_1)

		ASSERT_EQUAL(pl.get_current_maintenance(), old_maint)
	}

	// rail rejects double slope: another all_down on the already-down
	// tile fails with "Tile not empty", way unchanged at (1,1,-1).
	{
		local old_maint = pl.get_current_maintenance()
		ASSERT_EQUAL(setslope(pl, coord3d(1, 1, -1), slope.all_down_slope), "Tile not empty.")
		ASSERT_WAY_PATTERN(wt_rail, coord3d(0, 0, -1), one_tile_z_minus_1)
		ASSERT_EQUAL(pl.get_current_maintenance(), old_maint)
	}

	// extend the underground tunnel one tile further
	ASSERT_EQUAL(digger.work(pl, coord3d(1, 1, -1), coord3d(1, 2, -1), default_tunnel.get_name()), null)

	// all_up at (1,2,-1): the underground end becomes a slope-up
	// transition.  (1,1,-1) gains a S connection (way ribi N|S=18);
	// (1,2,-1) keeps a N-only ribi reflecting the new slope-up.
	{
		local old_maint = pl.get_current_maintenance()
		ASSERT_EQUAL(setslope(pl, coord3d(1, 2, -1), slope.all_up_slope), null)

		ASSERT_WAY_PATTERN(wt_rail, coord3d(0, 0, -1),
			[
				[0, 0, 0, 0, 0, 0, 0, 0],
				[0,18, 0, 0, 0, 0, 0, 0],
				[0,16, 0, 0, 0, 0, 0, 0],
				[0, 0, 0, 0, 0, 0, 0, 0],
				[0, 0, 0, 0, 0, 0, 0, 0],
				[0, 0, 0, 0, 0, 0, 0, 0],
				[0, 0, 0, 0, 0, 0, 0, 0],
				[0, 0, 0, 0, 0, 0, 0, 0],
			])

		ASSERT_EQUAL(pl.get_current_maintenance(), old_maint)
	}

	// clean up: ctrl-remove walks the tunnel network and tears down
	// every piece anchored to the (1,0,0) entrance.
	remover.set_flags(2)
	ASSERT_EQUAL(remover.work(pl, coord3d(1, 0, 0)), null)
	remover.set_flags(0)
	ASSERT_TRUE(tile_x(1, 0, 0).find_object(mo_tunnel) == null)

	lower_hex_tile_pair_S(pl, 1, 1, 0)
	RESET_ALL_PLAYER_FUNDS()
}


// test_way_tunnel_build_above_tunnel_slope: HEX-PORT PENDING.
function test_way_tunnel_build_above_tunnel_slope()
{
	// Tests building on the tile above the top of a tunnel slope in pak64
	// Should be allowed but, as of r11373, isn't.
	// If way_height_clearance is >= 2, this test DOES NOT APPLY.
	// FIXME: ignore this test if way_height_clearance >= 2 ?
	local pl = player_x(0)
	local default_tunnel = tunnel_desc_x.get_available_tunnels(wt_road)[0]
	local digger = command_x(tool_build_tunnel)
	local remover = command_x(tool_remover)
	local setslope = command_x.set_slope

	// Prepare area
	ASSERT_EQUAL(command_x.grid_raise(pl, coord3d(1, 1, 1)), null)
	ASSERT_EQUAL(command_x.grid_raise(pl, coord3d(2, 1, 1)), null)
	ASSERT_EQUAL(command_x.grid_raise(pl, coord3d(1, 2, 1)), null)
	ASSERT_EQUAL(command_x.grid_raise(pl, coord3d(2, 2, 1)), null)
	ASSERT_EQUAL(command_x.grid_raise(pl, coord3d(1, 3, 1)), null)
	ASSERT_EQUAL(command_x.grid_raise(pl, coord3d(2, 3, 1)), null)

	{
		digger.set_flags(2)
		ASSERT_EQUAL(digger.work(pl, tile_x(1, 0, 0), default_tunnel.get_name()), null)
		ASSERT_EQUAL(digger.work(pl, tile_x(1, 3, 0), default_tunnel.get_name()), null)
		digger.set_flags(0)

		ASSERT_EQUAL(digger.work(pl, tile_x(1, 0, 0), tile_x(1, 1, 0), default_tunnel.get_name()), null)
		ASSERT_EQUAL(setslope(pl, coord3d(1, 1, 0), slope.all_down_slope), null)
		ASSERT_EQUAL(setslope(pl, coord3d(1, 1, -1), slope.all_down_slope), null)
		ASSERT_EQUAL(digger.work(pl, tile_x(1, 1, -2), tile_x(1, 2, -2), default_tunnel.get_name()), null)
		ASSERT_EQUAL(setslope(pl, coord3d(1, 2, -2), slope.all_up_slope), null)

		// Now for the real test: build at z=0 above simple slope from z=-2 to z=-1, should be allowed
		ASSERT_EQUAL(digger.work(pl, tile_x(1, 3, 0), tile_x(1, 2, 0), default_tunnel.get_name()), null)
		ASSERT_EQUAL(remover.work(pl, coord3d(1, 2, 0)), null)
	}

	// Clean up
	ASSERT_EQUAL(remover.work(pl, coord3d(1, 1, -2)), null)
	ASSERT_EQUAL(remover.work(pl, coord3d(1, 2, -2)), null)
	ASSERT_EQUAL(remover.work(pl, coord3d(1, 0,  0)), null)
	ASSERT_EQUAL(remover.work(pl, coord3d(1, 3,  0)), null)
	ASSERT_EQUAL(command_x.grid_lower(pl, coord3d(1, 1, 1)), null)
	ASSERT_EQUAL(command_x.grid_lower(pl, coord3d(2, 1, 1)), null)
	ASSERT_EQUAL(command_x.grid_lower(pl, coord3d(1, 2, 1)), null)
	ASSERT_EQUAL(command_x.grid_lower(pl, coord3d(2, 2, 1)), null)
	ASSERT_EQUAL(command_x.grid_lower(pl, coord3d(1, 3, 1)), null)
	ASSERT_EQUAL(command_x.grid_lower(pl, coord3d(2, 3, 1)), null)

	RESET_ALL_PLAYER_FUNDS()
}


// test_way_tunnel_build_across_tunnel_slope: HEX-PORT PENDING.
function test_way_tunnel_build_across_tunnel_slope()
{
	local pl = player_x(0)
	local default_tunnel = tunnel_desc_x.get_available_tunnels(wt_road)[0]
	local digger = command_x(tool_build_tunnel)
	local remover = command_x(tool_remover)
	local setslope = command_x.set_slope

	// Prepare area
	ASSERT_EQUAL(command_x.grid_raise(pl, coord3d(1, 1, 1)), null)
	ASSERT_EQUAL(command_x.grid_raise(pl, coord3d(2, 1, 1)), null)
	ASSERT_EQUAL(command_x.grid_raise(pl, coord3d(1, 2, 1)), null)
	ASSERT_EQUAL(command_x.grid_raise(pl, coord3d(2, 2, 1)), null)
	ASSERT_EQUAL(command_x.grid_raise(pl, coord3d(1, 3, 1)), null)
	ASSERT_EQUAL(command_x.grid_raise(pl, coord3d(2, 3, 1)), null)

	{
		// build lone tunnel mouths
		digger.set_flags(2)
		ASSERT_EQUAL(digger.work(pl, tile_x(1, 0, 0), default_tunnel.get_name()), null)
		ASSERT_EQUAL(digger.work(pl, tile_x(1, 3, 0), default_tunnel.get_name()), null)
		digger.set_flags(0)

		// make double slope
		ASSERT_EQUAL(digger.work(pl, tile_x(1, 0, 0), tile_x(1, 1, 0), default_tunnel.get_name()), null)
		ASSERT_EQUAL(setslope(pl, coord3d(1, 1, 0), slope.all_down_slope), null)
		ASSERT_EQUAL(setslope(pl, coord3d(1, 1, -1), slope.all_down_slope), null)

		local net_wealth = pl.get_current_net_wealth()

		// try to tunnel across
		ASSERT_EQUAL(digger.work(pl, tile_x(1, 0, 0), tile_x(1, 3, 0), default_tunnel.get_name()), null)

		// nothing should be buid here
		ASSERT_EQUAL( net_wealth, pl.get_current_net_wealth() )

		// remove lone tunnel mouth
		ASSERT_EQUAL(remover.work(pl, coord3d(1, 3,  0)), null)

		// try to build across it - should fail and not build anything since the tunnel builder
		// only builds straight tunnels with no elevation changes
		// however, the player cannot call it like this since before the find_end_pos will return an invalid coordinate
		local err = digger.work(pl, tile_x(1, 3, 0), default_tunnel.get_name())
		ASSERT_EQUAL(err, "Tunnel must start on single way!")
	}
	// clean up
	ASSERT_EQUAL(remover.work(pl, coord3d(1, 1, -2)), null)
	ASSERT_EQUAL(remover.work(pl, coord3d(1, 0,  0)), null)

	ASSERT_EQUAL(command_x.grid_lower(pl, coord3d(1, 1, 1)), null)
	ASSERT_EQUAL(command_x.grid_lower(pl, coord3d(2, 1, 1)), null)
	ASSERT_EQUAL(command_x.grid_lower(pl, coord3d(1, 2, 1)), null)
	ASSERT_EQUAL(command_x.grid_lower(pl, coord3d(2, 2, 1)), null)
	ASSERT_EQUAL(command_x.grid_lower(pl, coord3d(1, 3, 1)), null)
	ASSERT_EQUAL(command_x.grid_lower(pl, coord3d(2, 3, 1)), null)

	RESET_ALL_PLAYER_FUNDS()
}


// 1-tile hill at (4,3) via raise_hex_tile.  Tunnel digs (4,2)→(4,4)
// along S axis through the buried tile.  Make-public is exercised on
// the entrance (4,2,0) and on the underground middle (4,3,0); the
// far entrance (4,4,0) stays pl-owned to verify make-public is
// per-tile rather than network-wide.
function test_way_tunnel_make_public()
{
	local pl = player_x(0)
	local public_pl = player_x(1)
	local tunnel_desc = tunnel_desc_x.get_available_tunnels(wt_road)[0]
	local makepublic  = command_x(tool_make_stop_public)

	raise_hex_tile(public_pl, 4, 3, 0)

	ASSERT_EQUAL(command_x(tool_build_tunnel).work(pl, coord3d(4, 2, 0), tunnel_desc.get_name()), null)

	// pre-condition: all three tunnel tiles owned by pl
	ASSERT_EQUAL(way_x(4, 2, 0).get_owner().get_name(), pl.get_name())
	ASSERT_EQUAL(way_x(4, 3, 0).get_owner().get_name(), pl.get_name())
	ASSERT_EQUAL(way_x(4, 4, 0).get_owner().get_name(), pl.get_name())

	// make tunnel portal public.  Verify: (a) ownership transfer at
	// (4,2,0); (b) pl pays 60 months of maintenance up front and
	// loses the recurring maintenance; (c) public_pl gains the
	// recurring maintenance for free; (d) the other two tiles stay
	// pl-owned (per-tile, not network-wide).
	{
		local old_pl_cash = pl.get_current_cash()
		local old_pl_maint = pl.get_current_maintenance()
		local old_public_cash = public_pl.get_current_cash()
		local old_public_maint = public_pl.get_current_maintenance()

		ASSERT_EQUAL(makepublic.work(pl, coord3d(4, 2, 0)), null)

		ASSERT_EQUAL(way_x(4, 2, 0).get_owner().get_name(), public_pl.get_name())
		ASSERT_EQUAL(way_x(4, 3, 0).get_owner().get_name(), pl.get_name())
		ASSERT_EQUAL(way_x(4, 4, 0).get_owner().get_name(), pl.get_name())

		ASSERT_EQUAL(pl.get_current_cash()*100, old_pl_cash*100 - 60 * tunnel_desc.get_maintenance()) // 60 == cst_make_public_months
		ASSERT_EQUAL(pl.get_current_maintenance(), old_pl_maint - tunnel_desc.get_maintenance())
		ASSERT_EQUAL(public_pl.get_current_maintenance(), old_public_maint + tunnel_desc.get_maintenance())
		ASSERT_EQUAL(public_pl.get_current_cash(), old_public_cash)
	}

	// make tunnel middle public.  Same per-tile transfer; (4,4,0)
	// remains pl-owned.
	{
		local old_pl_cash = pl.get_current_cash()
		local old_pl_maint = pl.get_current_maintenance()
		local old_public_cash = public_pl.get_current_cash()
		local old_public_maint = public_pl.get_current_maintenance()

		ASSERT_EQUAL(makepublic.work(pl, coord3d(4, 3, 0)), null)

		ASSERT_EQUAL(way_x(4, 3, 0).get_owner().get_name(), public_pl.get_name())
		ASSERT_EQUAL(way_x(4, 4, 0).get_owner().get_name(), pl.get_name())

		ASSERT_EQUAL(pl.get_current_cash()*100, old_pl_cash*100 - 60 * tunnel_desc.get_maintenance())
		ASSERT_EQUAL(pl.get_current_maintenance(), old_pl_maint - tunnel_desc.get_maintenance())
		ASSERT_EQUAL(public_pl.get_current_maintenance(), old_public_maint + tunnel_desc.get_maintenance())
		ASSERT_EQUAL(public_pl.get_current_cash(), old_public_cash)
	}

	// clean up
	ASSERT_EQUAL(command_x(tool_remove_way).work(public_pl, coord3d(4, 2, 0), coord3d(4, 4, 0), "" + wt_road), null)
	lower_hex_tile(public_pl, 4, 3, 0)
	RESET_ALL_PLAYER_FUNDS()
}
