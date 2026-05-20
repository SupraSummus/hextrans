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


// Raise all 6 vertices of tile (q, r) by 1 from base height z, so the
// tile becomes flat at z+1.  Three vertices are reachable through
// `grid_raise` (which targets the NW corner — i.e., vertex `(q-1, r).E`
// of the picked tile); the other three are SE-canonical and need
// `grid_raise_at_corner(..., 1)`.  Each raised vertex is shared with
// exactly one of the 6 surrounding hex tiles, which pick up clean
// 2-corner edge slopes for free — useful for tunnel/depot/halt tests
// that need a single elevated tile with sloped approaches.  The
// original square-era 4-grid_raise scaffold doesn't translate cleanly
// under per-vertex hex topology.
function raise_hex_tile(pl, q, r, z)
{
	ASSERT_EQUAL(command_x.grid_raise(pl, coord3d(q,   r,   z)), null)              // (q,r).NW
	ASSERT_EQUAL(command_x.grid_raise(pl, coord3d(q+1, r,   z)), null)              // (q,r).E
	ASSERT_EQUAL(command_x.grid_raise(pl, coord3d(q,   r+1, z)), null)              // (q,r).SW
	ASSERT_EQUAL(command_x.grid_raise_at_corner(pl, coord3d(q,   r,   z), 1), null) // (q,r).SE
	ASSERT_EQUAL(command_x.grid_raise_at_corner(pl, coord3d(q-1, r,   z), 1), null) // (q,r).W
	ASSERT_EQUAL(command_x.grid_raise_at_corner(pl, coord3d(q,   r-1, z), 1), null) // (q,r).NE
}


// Inverse of raise_hex_tile: tile (q, r) is flat at z+1 (with the
// surrounding 6 neighbours holding edge slopes from the raise), bring
// it back to flat at z and restore the neighbours to flat.
function lower_hex_tile(pl, q, r, z)
{
	ASSERT_EQUAL(command_x.grid_lower_at_corner(pl, coord3d(q,   r-1, z+1), 1), null) // (q,r).NE
	ASSERT_EQUAL(command_x.grid_lower_at_corner(pl, coord3d(q-1, r,   z+1), 1), null) // (q,r).W
	ASSERT_EQUAL(command_x.grid_lower_at_corner(pl, coord3d(q,   r,   z+1), 1), null) // (q,r).SE
	ASSERT_EQUAL(command_x.grid_lower(pl, coord3d(q,   r+1, z+1)), null)              // (q,r).SW
	ASSERT_EQUAL(command_x.grid_lower(pl, coord3d(q+1, r,   z+1)), null)              // (q,r).E
	ASSERT_EQUAL(command_x.grid_lower(pl, coord3d(q,   r,   z+1)), null)              // (q,r).NW
}


// Raise tiles (q, r) and (q, r+1) to flat at z+1 (S-axis-adjacent
// pair).  The two tiles share an edge whose 2 vertices are common, so
// the recipe is raise_hex_tile(q, r) plus the 4 vertices of (q, r+1)
// not shared with (q, r) — `(q,r+1)`'s NW = `(q,r)`'s SW and `(q,r+1)`'s
// NE = `(q,r)`'s SE.  Surrounding edge slopes mirror the 1-tile case
// extended along the pair: (q, r-1) → north_narrow, (q, r+2) →
// south_narrow, plus side slopes on (q±1, r), (q±1, r+1).
function raise_hex_tile_pair_S(pl, q, r, z)
{
	raise_hex_tile(pl, q, r, z)
	ASSERT_EQUAL(command_x.grid_raise(pl, coord3d(q+1, r+1, z)), null)              // (q,r+1).E
	ASSERT_EQUAL(command_x.grid_raise(pl, coord3d(q,   r+2, z)), null)              // (q,r+1).SW
	ASSERT_EQUAL(command_x.grid_raise_at_corner(pl, coord3d(q,   r+1, z), 1), null) // (q,r+1).SE
	ASSERT_EQUAL(command_x.grid_raise_at_corner(pl, coord3d(q-1, r+1, z), 1), null) // (q,r+1).W
}


function lower_hex_tile_pair_S(pl, q, r, z)
{
	ASSERT_EQUAL(command_x.grid_lower_at_corner(pl, coord3d(q-1, r+1, z+1), 1), null)
	ASSERT_EQUAL(command_x.grid_lower_at_corner(pl, coord3d(q,   r+1, z+1), 1), null)
	ASSERT_EQUAL(command_x.grid_lower(pl, coord3d(q,   r+2, z+1)), null)
	ASSERT_EQUAL(command_x.grid_lower(pl, coord3d(q+1, r+1, z+1)), null)
	lower_hex_tile(pl, q, r, z)
}


// Raise a 1-tile hex hill at (q, r, z) and surround it with 6 surface
// tunnel mouths converging underground — one per hex axis.  Recipe:
// one full S-axis drill creates the (q, r-1)/(q, r+1) mouth pair;
// the four remaining hex neighbours then drill (no ctrl) and join
// the buried network as branches via `find_end_pos`'s ist_tunnel
// path.  See AGENTS.md "Hex test-authoring primer" for the why.
// Returns the 6 mouth coords.
function raise_hex_hill_with_six_mouths(pl, q, r, z, tunnel_desc)
{
	raise_hex_tile(pl, q, r, z)
	local digger = command_x(tool_build_tunnel)
	local mouths = [coord3d(q,   r-1, z), coord3d(q,   r+1, z),
	                coord3d(q-1, r,   z), coord3d(q+1, r,   z),
	                coord3d(q-1, r+1, z), coord3d(q+1, r-1, z)]
	ASSERT_EQUAL(digger.work(pl, mouths[0], tunnel_desc.get_name()), null)
	for (local i = 2; i < mouths.len(); i++) {
		ASSERT_EQUAL(digger.work(pl, mouths[i], tunnel_desc.get_name()), null)
	}
	foreach (p in mouths) {
		ASSERT_TRUE(tile_x(p.x, p.y, z).find_object(mo_tunnel) != null)
	}
	return mouths
}


// Patterns are arrays of arrays of 6-bit ribi values (see ribi_t in
// src/simutrans/dataobj/ribi.h: SE=1, S=2, SW=4, NW=8, N=16, NE=32).
// A cell value of -1 means "don't care" — skip the assertion entirely.
// 0 means "no way here" and is asserted normally.
// Strings are rejected: legacy square-era patterns used a "..5...."
// digit-shorthand that Squirrel decodes as ASCII codes (so '5' read as
// 53), and 6-bit hex ribis don't fit a single digit anyway.
function ASSERT_WAY_PATTERN(waytype, lefttop, pattern)
{
	local z = lefttop.z

	for (local y = 0; y < pattern.len(); ++y) {
		local row = pattern[y]
		if (typeof row == "string") {
			throw "ASSERT_WAY_PATTERN row is a string; pass an array of 6-bit ribi ints"
		}
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
					for (local i = 0; i < 6; ++i) {
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
		if (typeof row == "string") {
			throw "ASSERT_WAY_PATTERN_MASKED row is a string; pass an array of 6-bit ribi ints"
		}
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
// loop.  Of these, only the 12 hex edges (narrow + wide), the 2
// legacy diagonals, and flat are way-buildable — see `slope_t::is_way`.
function interesting_slopes()
{
	return [
		slope.flat,
		// 6 single-corner raised, in hex_corner_t order (E, SE, SW, W, NW, NE).
		slope.raised_E, slope.raised_SE, slope.raised_SW, slope.raised_W, slope.raised_NW, slope.raised_NE,
		// 6 narrow hex edges (2-corner), cyclic: low edge NW, N, NE, SE, S, SW
		slope.northwest_narrow, slope.north_narrow, slope.northeast_narrow,
		slope.southeast_narrow, slope.south_narrow, slope.southwest_narrow,
		// 6 wide hex edges (4-corner), same cyclic order
		slope.northwest_wide, slope.north_wide, slope.northeast_wide,
		slope.southeast_wide, slope.south_wide, slope.southwest_wide,
		// 2 legacy square diagonals (raise two non-adjacent hex
		// corners with the third in the valley between them — no
		// clean hex gradient, but `slope_allows_ribi` admits stubs
		// against their side chords)
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
