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


// 2-tile S-axis hill at (1,1)+(1,2); rail tunnel, so the second
// consecutive all_down hits the no-double-slopes reject.
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


// 2-tile S-axis hill at (1,1)+(1,2).  Picks `test_tunnel_double`
// (tests/test-pak/) for the 2-step underground slope dive; the
// gate at simtool.cc:1324 rejects the second consecutive
// `setslope all_down` unless the way underneath opts into
// has_double_slopes=1, which pak64 road tunnels don't.
function test_way_tunnel_build_above_tunnel_slope()
{
	// Building at z=0 above the top of a 2-step underground slope
	// (z=-2 → z=-1) should be allowed.  Upstream r11373 regressed
	// this; the test guards the fix.  If way_height_clearance is
	// >= 2, the test DOES NOT APPLY.
	local pl = player_x(0)
	local tunnel = null
	foreach (t in tunnel_desc_x.get_available_tunnels(wt_road)) {
		if (t.get_name() == "test_tunnel_double") tunnel = t
	}
	ASSERT_TRUE(tunnel != null) // tests/test-pak ships test_tunnel_double
	local digger = command_x(tool_build_tunnel)
	local remover = command_x(tool_remover)
	local setslope = command_x.set_slope

	raise_hex_tile_pair_S(pl, 1, 1, 0)

	{
		digger.set_flags(2)
		ASSERT_EQUAL(digger.work(pl, tile_x(1, 0, 0), tunnel.get_name()), null)
		ASSERT_EQUAL(digger.work(pl, tile_x(1, 3, 0), tunnel.get_name()), null)
		digger.set_flags(0)

		ASSERT_EQUAL(digger.work(pl, tile_x(1, 0, 0), tile_x(1, 1, 0), tunnel.get_name()), null)
		ASSERT_EQUAL(setslope(pl, coord3d(1, 1, 0), slope.all_down_slope), null)
		ASSERT_EQUAL(setslope(pl, coord3d(1, 1, -1), slope.all_down_slope), null)
		// guard the has_double_slopes gate beyond "setslope returned null":
		// the way must have actually sunk two levels.
		ASSERT_TRUE(tile_x(1, 1,  0).find_object(mo_tunnel) == null)
		ASSERT_TRUE(tile_x(1, 1, -1).find_object(mo_tunnel) == null)
		ASSERT_TRUE(tile_x(1, 1, -2).find_object(mo_tunnel) != null)

		ASSERT_EQUAL(digger.work(pl, tile_x(1, 1, -2), tile_x(1, 2, -2), tunnel.get_name()), null)
		ASSERT_EQUAL(setslope(pl, coord3d(1, 2, -2), slope.all_up_slope), null)

		// The real test: build at z=0 above the slope from z=-2 to z=-1.
		ASSERT_EQUAL(digger.work(pl, tile_x(1, 3, 0), tile_x(1, 2, 0), tunnel.get_name()), null)
		// guard against silent no-op: a tunnel must actually be present.
		ASSERT_TRUE(tile_x(1, 2, 0).find_object(mo_tunnel) != null)
		ASSERT_EQUAL(remover.work(pl, coord3d(1, 2, 0)), null)
	}

	// Clean up
	ASSERT_EQUAL(remover.work(pl, coord3d(1, 1, -2)), null)
	ASSERT_EQUAL(remover.work(pl, coord3d(1, 2, -2)), null)
	ASSERT_EQUAL(remover.work(pl, coord3d(1, 0,  0)), null)
	ASSERT_EQUAL(remover.work(pl, coord3d(1, 3,  0)), null)

	lower_hex_tile_pair_S(pl, 1, 1, 0)

	RESET_ALL_PLAYER_FUNDS()
}


// Same scaffold as `_above_tunnel_slope`: 2-tile S-axis hill,
// 2-step underground slope dive, `test_tunnel_double` from
// tests/test-pak/.
function test_way_tunnel_build_across_tunnel_slope()
{
	local pl = player_x(0)
	local tunnel = null
	foreach (t in tunnel_desc_x.get_available_tunnels(wt_road)) {
		if (t.get_name() == "test_tunnel_double") tunnel = t
	}
	ASSERT_TRUE(tunnel != null) // tests/test-pak ships test_tunnel_double
	local digger = command_x(tool_build_tunnel)
	local remover = command_x(tool_remover)
	local setslope = command_x.set_slope

	raise_hex_tile_pair_S(pl, 1, 1, 0)

	{
		// build lone tunnel mouths
		digger.set_flags(2)
		ASSERT_EQUAL(digger.work(pl, tile_x(1, 0, 0), tunnel.get_name()), null)
		ASSERT_EQUAL(digger.work(pl, tile_x(1, 3, 0), tunnel.get_name()), null)
		digger.set_flags(0)

		// make double slope
		ASSERT_EQUAL(digger.work(pl, tile_x(1, 0, 0), tile_x(1, 1, 0), tunnel.get_name()), null)
		ASSERT_EQUAL(setslope(pl, coord3d(1, 1, 0), slope.all_down_slope), null)
		ASSERT_EQUAL(setslope(pl, coord3d(1, 1, -1), slope.all_down_slope), null)
		ASSERT_TRUE(tile_x(1, 1, -2).find_object(mo_tunnel) != null) // gate worked

		local net_wealth = pl.get_current_net_wealth()

		// tunnel builder only builds straight tunnels with no elevation
		// change, so digging across the now-staircased underground rejects.
		ASSERT_EQUAL(digger.work(pl, tile_x(1, 0, 0), tile_x(1, 3, 0), tunnel.get_name()), null)

		// nothing built: net_wealth catches the cost side, find_object
		// catches a silent-no-cost stub on the would-be path.
		ASSERT_EQUAL( net_wealth, pl.get_current_net_wealth() )
		ASSERT_TRUE(tile_x(1, 1, 0).find_object(mo_tunnel) == null)
		ASSERT_TRUE(tile_x(1, 2, 0).find_object(mo_tunnel) == null)

		// remove lone tunnel mouth
		ASSERT_EQUAL(remover.work(pl, coord3d(1, 3,  0)), null)

		// single-arg form: find_end_pos returns an invalid coordinate
		// before the builder is invoked, so the error path differs.
		local err = digger.work(pl, tile_x(1, 3, 0), tunnel.get_name())
		ASSERT_EQUAL(err, "Tunnel must start on single way!")
	}

	// clean up
	ASSERT_EQUAL(remover.work(pl, coord3d(1, 1, -2)), null)
	ASSERT_EQUAL(remover.work(pl, coord3d(1, 0,  0)), null)

	lower_hex_tile_pair_S(pl, 1, 1, 0)

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


// A `southeast_double` (NW-axis 2-step ramp) is a valid hex tunnel
// mouth on the same footing as the narrow / wide variants.  Ctrl-dig
// builds an isolated single-tile mouth so cleanup is local.
function test_way_tunnel_build_nw_double_axis_slope()
{
	local digger = command_x(tool_build_tunnel)
	local remover = command_x(tool_remover)
	local setslope = command_x.set_slope
	local default_tunnel = tunnel_desc_x.get_available_tunnels(wt_rail)[0]
	local pl = player_x(0)

	ASSERT_TRUE(default_tunnel != null)

	ASSERT_EQUAL(setslope(pl, coord3d(4, 3, 0), slope.southeast_double), null)
	ASSERT_EQUAL(tile_x(4, 3, 0).get_slope(), slope.southeast_double)

	digger.set_flags(2)
	ASSERT_EQUAL(digger.work(pl, tile_x(4, 3, 0), default_tunnel.get_name()), null)
	ASSERT_TRUE(tile_x(4, 3, 0).find_object(mo_tunnel) != null)
	digger.set_flags(0)

	ASSERT_EQUAL(remover.work(pl, coord3d(4, 3, 0)), null)
	ASSERT_TRUE(tile_x(4, 3, 0).find_object(mo_tunnel) == null)

	ASSERT_EQUAL(setslope(pl, coord3d(4, 3, 0), slope.flat), null)

	RESET_ALL_PLAYER_FUNDS()
}


// A tunnel built between two planar-double mouths facing each other
// must not terraform any tile — both mouths are valid on their own
// (`test_tunnel_double` opts in to `has_double_slopes`).
function test_way_tunnel_build_does_not_terraform_double_mouth()
{
	local digger = command_x(tool_build_tunnel)
	local remover = command_x(tool_remover)
	local setslope = command_x.set_slope
	local tunnel = null
	foreach (t in tunnel_desc_x.get_available_tunnels(wt_road)) {
		if (t.get_name() == "test_tunnel_double") tunnel = t
	}
	ASSERT_TRUE(tunnel != null)
	local pl = player_x(0)

	// two adjacent S-axis tiles, each a planar 2-step ramp facing the
	// other.  Shared edge at the centre is z=2.  setslope cascades to
	// surrounding tiles only on the side corners (E, W at z=1), nowhere
	// near the rest of the map.
	ASSERT_EQUAL(setslope(pl, coord3d(3, 3, 0), slope.north_double), null)
	ASSERT_EQUAL(setslope(pl, coord3d(3, 4, 0), slope.south_double), null)
	ASSERT_EQUAL(tile_x(3, 3, 0).get_slope(), slope.north_double)
	ASSERT_EQUAL(tile_x(3, 4, 0).get_slope(), slope.south_double)

	// snapshot the work area and the 6 hex neighbours of each mouth
	local probes = [
		[3, 3], [3, 4],                  // mouths
		[2, 3], [4, 3], [2, 4], [4, 4],  // side neighbours (NW/SE/SW/NE)
		[3, 2], [3, 5],                  // along-axis neighbours
	]
	local before = {}
	foreach (p in probes) {
		before[p[0] + "," + p[1]] <- tile_x(p[0], p[1], 0).get_slope()
	}

	// dig a 2-tile tunnel (mouth on each ramp)
	ASSERT_EQUAL(digger.work(pl, tile_x(3, 3, 0), tunnel.get_name()), null)
	ASSERT_TRUE(tile_x(3, 3, 0).find_object(mo_tunnel) != null)
	ASSERT_TRUE(tile_x(3, 4, 0).find_object(mo_tunnel) != null)

	// remove the network
	remover.set_flags(2)
	ASSERT_EQUAL(remover.work(pl, coord3d(3, 3, 0)), null)
	remover.set_flags(0)

	// every probed slope must survive the build+remove unchanged
	foreach (p in probes) {
		local key = p[0] + "," + p[1]
		local now = tile_x(p[0], p[1], 0).get_slope()
		if (now != before[key]) {
			throw "tile (" + p[0] + "," + p[1] + ",0) slope changed: " + before[key] + " -> " + now
		}
	}

	// teardown: setslope back to flat
	ASSERT_EQUAL(setslope(pl, coord3d(3, 3, 0), slope.flat), null)
	ASSERT_EQUAL(setslope(pl, coord3d(3, 4, 0), slope.flat), null)
	RESET_ALL_PLAYER_FUNDS()
}
