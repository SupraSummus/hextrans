//
// This file is part of the Simutrans project under the Artistic License.
// (see LICENSE.txt)
//


//
// Test helpers
//
function make_assertion_str(val)
{
	if (typeof val == "string") {
		return "\"" + val + "\""
	}
	else {
		return "" + val
	}
}


function ASSERT_EQUAL(act, exp)
{
	if (!(act == exp)) {
		local err = ttext("Assertion failed, '{act} == {exp}' was not true")
		err.act = make_assertion_str(act)
		err.exp = make_assertion_str(exp)
		throw err.tostring()
	}
}


function ASSERT_TRUE(a)
{
	if (!(a == true)) {
		local err = ttext("Assertion failed, '{a}' was not true")
		err.a = make_assertion_str(a)
		throw err.tostring()
	}
}


function ASSERT_FALSE(a)
{
	if (!(a == false)) {
		local err = ttext("Assertion failed, '{a}' was not false")
		err.a = make_assertion_str(a)
		throw err.tostring()
	}
}


// Set player funds (incl. non-cash assets) to some specific amount (in credit-cents)
function SET_PLAYER_FUNDS(pl, amount)
{
	pl.book_cash(amount - pl.get_current_net_wealth())
}


function RESET_ALL_PLAYER_FUNDS()
{
	local default_cash = 200000 * 100

	for (local i = 0; i < 8; ++i) {
		local pl = player_x(i)
		if (pl.is_valid()) {
			ASSERT_EQUAL(pl.get_current_maintenance(), 0)
			SET_PLAYER_FUNDS(pl, default_cash)
		}
	}
}


// Patterns are arrays of arrays of 6-bit ribi values (see ribi_t in
// src/simutrans/dataobj/ribi.h: SE=1, S=2, SW=4, NW=8, N=16, NE=32).
// A cell value of -1 means "don't care" — skip the assertion entirely.
// 0 means "no way here" and is asserted normally.
function ASSERT_WAY_PATTERN(waytype, lefttop, pattern)
{
	local z = lefttop.z

	for (local y = 0; y < pattern.len(); ++y) {
		local row = pattern[y]
		for (local x = 0; x < row.len(); ++x) {
			local expected_dir = row[x]
			if (expected_dir < 0) continue
			local tile = square_x(lefttop.x + x, lefttop.y + y).get_tile_at_height(z)
			if (tile == null) continue

			local actual_dir
			if (waytype != wt_power) {
				actual_dir = tile.get_way_dirs(waytype)
			}
			else {
				// powerlines connect to other powerlines automatically.
				actual_dir = 0
				if (tile.find_object(mo_powerline)) {
					for (local i = 0; i < 4; ++i) {
						local offset = dir.to_coord(1<<i)
						local nb = square_x(lefttop.x + x + offset.x, lefttop.y + y + offset.y)
						if (nb && nb.is_valid() && nb.get_tile_at_height(z) && nb.get_tile_at_height(z).find_object(mo_powerline)) {
							actual_dir = actual_dir | (1<<i)
						}
					}
				}
			}

			ASSERT_EQUAL(actual_dir, expected_dir)
		}
	}
}


function ASSERT_WAY_PATTERN_MASKED(waytype, lefttop, pattern)
{
	if (waytype == wt_power) {
		ASSERT_WAY_PATTERN(waytype, lefttop, pattern)
		return
	}

	local z = lefttop.z

	for (local y = 0; y < pattern.len(); ++y) {
		local row = pattern[y]
		for (local x = 0; x < row.len(); ++x) {
			local expected_dir = row[x]
			if (expected_dir < 0) continue
			local tile = square_x(lefttop.x + x, lefttop.y + y).get_tile_at_height(z)
			if (tile == null) continue

			ASSERT_EQUAL(tile.get_way_dirs_masked(waytype), expected_dir)
		}
	}
}


// Representative single-height slopes for tests that used to iterate
// `for sl in 0..slope.raised`.  Under the base-4 hex encoding most
// integers in 0..4095 don't decode to single-height slopes; this
// returns the 21 that do: flat, each single-corner raised, each
// 2-corner hex edge ("narrow") and 4-corner hex edge ("wide"), and
// the 2 legacy square diagonals (`slope.east`, `slope.west`).  Omits
// `slope.raised` (all_up_one): raising all corners uniformly shifts
// the whole tile and breaks the cleanup some callers do after the
// loop.  Of these, only the 12 hex edges (narrow + wide) and flat
// are way-buildable — see slope_t::is_way.
function interesting_slopes()
{
	return [
		slope.flat,
		// 6 single-corner raised, in hex_corner_t order (E, SE, SW, W,
		// NW, NE).  E and W have no script-side aliases — `slope.east`
		// and `slope.west` are taken by the legacy 2-corner diagonals
		// (see end of this list).
		1, slope.southeast, slope.southwest, 64, slope.northwest, slope.northeast,
		// 6 narrow hex edges (2-corner), cyclic: low edge NW, N, NE, SE, S, SW
		slope.nw_edge, slope.north, slope.ne_edge,
		slope.se_edge, slope.south, slope.sw_edge,
		// 6 wide hex edges (4-corner), same cyclic order
		slope.nw_wide, slope.north_wide, slope.ne_wide,
		slope.se_wide, slope.south_wide, slope.sw_wide,
		// 2 legacy square diagonals (no longer way-buildable)
		slope.east, slope.west,
	]
}


function HEX_SLOPE(e, se, sw, w, nw, ne)
{
	return e + 4*se + 16*sw + 64*w + 256*nw + 1024*ne
}


function get_depot_by_wt(waytype)
{
	local list = building_desc_x.get_building_list(building_desc_x.depot)

	foreach (building in list) {
		if (building.get_type() == building_desc_x.depot && building.get_waytype() == waytype) {
			return building
		}
	}

	return null
}
