//
// This file is part of the Simutrans project under the Artistic License.
// (see LICENSE.txt)
//


//
// Tests for the scripting `dir` class.
//
// Flat-top hex has 6 edge directions (SE, S, SW, NW, N, NE) and 3
// straight axes (N-S, NE-SW, NW-SE).  See AGENTS.md → "Direction
// naming convention" and src/simutrans/dataobj/ribi.h for the bit
// layout.
//


function test_dir_is_single()
{
	ASSERT_FALSE(dir.is_single(dir.none))

	// All 6 hex single-edge directions are is_single.
	ASSERT_TRUE(dir.is_single(dir.southeast))
	ASSERT_TRUE(dir.is_single(dir.south))
	ASSERT_TRUE(dir.is_single(dir.southwest))
	ASSERT_TRUE(dir.is_single(dir.northwest))
	ASSERT_TRUE(dir.is_single(dir.north))
	ASSERT_TRUE(dir.is_single(dir.northeast))

	// The 3 straight-axis pairs are 2-bit ribis, not single.
	ASSERT_FALSE(dir.is_single(dir.northsouth))
	ASSERT_FALSE(dir.is_single(dir.northeast_southwest))
	ASSERT_FALSE(dir.is_single(dir.northwest_southeast))

	// Arbitrary 2-bit bends and 3-bit combos also not single.
	ASSERT_FALSE(dir.is_single(dir.north | dir.northeast))
	ASSERT_FALSE(dir.is_single(dir.north | dir.south | dir.southeast))

	ASSERT_FALSE(dir.is_single(dir.all))
}


function test_dir_is_twoway()
{
	ASSERT_FALSE(dir.is_twoway(dir.none))

	// Single-edge ribis are not two-way.
	foreach (d in [dir.southeast, dir.south, dir.southwest,
	               dir.northwest, dir.north, dir.northeast]) {
		ASSERT_FALSE(dir.is_twoway(d))
	}

	// The 3 straight-axis pairs.
	ASSERT_TRUE(dir.is_twoway(dir.northsouth))
	ASSERT_TRUE(dir.is_twoway(dir.northeast_southwest))
	ASSERT_TRUE(dir.is_twoway(dir.northwest_southeast))

	// Adjacent-edge bends are also two-way.
	ASSERT_TRUE(dir.is_twoway(dir.north     | dir.northeast))
	ASSERT_TRUE(dir.is_twoway(dir.northeast | dir.southeast))
	ASSERT_TRUE(dir.is_twoway(dir.southeast | dir.south))
	ASSERT_TRUE(dir.is_twoway(dir.south     | dir.southwest))
	ASSERT_TRUE(dir.is_twoway(dir.southwest | dir.northwest))
	ASSERT_TRUE(dir.is_twoway(dir.northwest | dir.north))

	// Three-way and four+-way ribis are not two-way.
	ASSERT_FALSE(dir.is_twoway(dir.north | dir.south | dir.southeast))
	ASSERT_FALSE(dir.is_twoway(dir.all))
}


function test_dir_is_threeway()
{
	ASSERT_FALSE(dir.is_threeway(dir.none))

	foreach (d in [dir.southeast, dir.south, dir.southwest,
	               dir.northwest, dir.north, dir.northeast]) {
		ASSERT_FALSE(dir.is_threeway(d))
	}

	ASSERT_FALSE(dir.is_threeway(dir.northsouth))
	ASSERT_FALSE(dir.is_threeway(dir.northeast_southwest))
	ASSERT_FALSE(dir.is_threeway(dir.northwest_southeast))

	// is_threeway is "3 or more bits set" — it's really
	// "3+-way".  Sample the 3-, 4-, 5-, 6-bit cases.
	ASSERT_TRUE(dir.is_threeway(dir.north | dir.south | dir.southeast))
	ASSERT_TRUE(dir.is_threeway(dir.north | dir.south | dir.northeast | dir.southwest))
	ASSERT_TRUE(dir.is_threeway(dir.all & ~dir.north))
	ASSERT_TRUE(dir.is_threeway(dir.all))
}


function test_dir_is_curve()
{
	ASSERT_FALSE(dir.is_curve(dir.none))

	// Single edges are not bends.
	foreach (d in [dir.southeast, dir.south, dir.southwest,
	               dir.northwest, dir.north, dir.northeast]) {
		ASSERT_FALSE(dir.is_curve(d))
	}

	// Straight axes are not bends.
	ASSERT_FALSE(dir.is_curve(dir.northsouth))
	ASSERT_FALSE(dir.is_curve(dir.northeast_southwest))
	ASSERT_FALSE(dir.is_curve(dir.northwest_southeast))

	// All 2-bit non-opposite combos are bends.  Sample several,
	// covering the 6 adjacent-edge pairs and a few 120°-apart ones.
	ASSERT_TRUE(dir.is_curve(dir.north     | dir.northeast))
	ASSERT_TRUE(dir.is_curve(dir.northeast | dir.southeast))
	ASSERT_TRUE(dir.is_curve(dir.southeast | dir.south))
	ASSERT_TRUE(dir.is_curve(dir.south     | dir.southwest))
	ASSERT_TRUE(dir.is_curve(dir.southwest | dir.northwest))
	ASSERT_TRUE(dir.is_curve(dir.northwest | dir.north))
	ASSERT_TRUE(dir.is_curve(dir.north     | dir.southeast))
	ASSERT_TRUE(dir.is_curve(dir.north     | dir.southwest))

	// 3+-way is not a bend.
	ASSERT_FALSE(dir.is_curve(dir.north | dir.south | dir.southeast))
	ASSERT_FALSE(dir.is_curve(dir.all))
}


function test_dir_is_straight()
{
	ASSERT_FALSE(dir.is_straight(dir.none))

	// Every single edge is straight.
	foreach (d in [dir.southeast, dir.south, dir.southwest,
	               dir.northwest, dir.north, dir.northeast]) {
		ASSERT_TRUE(dir.is_straight(d))
	}

	// The 3 straight-axis pairs.
	ASSERT_TRUE(dir.is_straight(dir.northsouth))
	ASSERT_TRUE(dir.is_straight(dir.northeast_southwest))
	ASSERT_TRUE(dir.is_straight(dir.northwest_southeast))

	// Bends are not straight.
	ASSERT_FALSE(dir.is_straight(dir.north     | dir.northeast))
	ASSERT_FALSE(dir.is_straight(dir.north     | dir.southeast))
	ASSERT_FALSE(dir.is_straight(dir.southwest | dir.northwest))

	// Three-or-more-bit ribis mix more than one axis, so not straight.
	ASSERT_FALSE(dir.is_straight(dir.north | dir.south | dir.southeast))
	ASSERT_FALSE(dir.is_straight(dir.all))
}


function test_dir_double()
{
	// `dir.double(x)` returns the straight axis containing x when x is
	// a single edge, else 0.  See ribi_t::doubles.
	ASSERT_EQUAL(dir.double(dir.none), dir.none)

	ASSERT_EQUAL(dir.double(dir.north),     dir.northsouth)
	ASSERT_EQUAL(dir.double(dir.south),     dir.northsouth)
	ASSERT_EQUAL(dir.double(dir.northeast), dir.northeast_southwest)
	ASSERT_EQUAL(dir.double(dir.southwest), dir.northeast_southwest)
	ASSERT_EQUAL(dir.double(dir.northwest), dir.northwest_southeast)
	ASSERT_EQUAL(dir.double(dir.southeast), dir.northwest_southeast)

	// Multi-bit ribis return none.
	ASSERT_EQUAL(dir.double(dir.northsouth),          dir.none)
	ASSERT_EQUAL(dir.double(dir.northeast_southwest), dir.none)
	ASSERT_EQUAL(dir.double(dir.northwest_southeast), dir.none)
	ASSERT_EQUAL(dir.double(dir.north | dir.northeast), dir.none)
	ASSERT_EQUAL(dir.double(dir.all), dir.none)
}


function test_dir_backward()
{
	// backward flips each set bit to its 180°-opposite hex edge:
	// SE↔NW, S↔N, SW↔NE.  Unlike the old 4-bit ribi, backward(none)
	// = none and backward(all) = all (clean bit-rotate, no NOT trick).
	ASSERT_EQUAL(dir.backward(dir.none), dir.none)
	ASSERT_EQUAL(dir.backward(dir.all),  dir.all)

	ASSERT_EQUAL(dir.backward(dir.north),     dir.south)
	ASSERT_EQUAL(dir.backward(dir.south),     dir.north)
	ASSERT_EQUAL(dir.backward(dir.southeast), dir.northwest)
	ASSERT_EQUAL(dir.backward(dir.northwest), dir.southeast)
	ASSERT_EQUAL(dir.backward(dir.southwest), dir.northeast)
	ASSERT_EQUAL(dir.backward(dir.northeast), dir.southwest)

	// Axis pairs are self-inverse.
	ASSERT_EQUAL(dir.backward(dir.northsouth),          dir.northsouth)
	ASSERT_EQUAL(dir.backward(dir.northeast_southwest), dir.northeast_southwest)
	ASSERT_EQUAL(dir.backward(dir.northwest_southeast), dir.northwest_southeast)

	// Bends flip into the opposite-bend (both edges flipped).
	ASSERT_EQUAL(dir.backward(dir.north | dir.northeast), dir.south | dir.southwest)
	ASSERT_EQUAL(dir.backward(dir.north | dir.southeast), dir.south | dir.northwest)
}


function test_dir_to_slope()
{
	// dir.to_slope maps a single hex edge to the slope whose low
	// edge is that direction.  Under hex only the 2 legacy N/S hex
	// edges have slope aliases today — the other 4 (NE, SE, SW, NW)
	// return flat until the hex-only slope-edge constants land
	// (tracked under "slope-edge constants" in TODO.md).
	ASSERT_EQUAL(dir.to_slope(dir.none),  slope.flat)

	ASSERT_EQUAL(dir.to_slope(dir.north), slope.south)
	ASSERT_EQUAL(dir.to_slope(dir.south), slope.north)

	ASSERT_EQUAL(dir.to_slope(dir.southeast), slope.flat)
	ASSERT_EQUAL(dir.to_slope(dir.northwest), slope.flat)
	ASSERT_EQUAL(dir.to_slope(dir.northeast), slope.flat)
	ASSERT_EQUAL(dir.to_slope(dir.southwest), slope.flat)

	// Multi-bit ribis are not a slope direction.
	ASSERT_EQUAL(dir.to_slope(dir.northsouth),           slope.flat)
	ASSERT_EQUAL(dir.to_slope(dir.northeast_southwest),  slope.flat)
	ASSERT_EQUAL(dir.to_slope(dir.northwest_southeast),  slope.flat)
	ASSERT_EQUAL(dir.to_slope(dir.north | dir.northeast), slope.flat)
	ASSERT_EQUAL(dir.to_slope(dir.all), slope.flat)
}


function test_dir_to_coord()
{
	// dir.to_coord sums the neighbour displacement for each set bit.
	// Matches koord::koord(ribi_t::ribi) and koord::neighbours[].
	ASSERT_EQUAL(dir.to_coord(dir.none).tostring(), coord( 0,  0).tostring())

	// Single-edge directions: the 6 hex neighbours, in bit-position
	// order (SE, S, SW, NW, N, NE).
	ASSERT_EQUAL(dir.to_coord(dir.southeast).tostring(), coord( 1,  0).tostring())
	ASSERT_EQUAL(dir.to_coord(dir.south    ).tostring(), coord( 0,  1).tostring())
	ASSERT_EQUAL(dir.to_coord(dir.southwest).tostring(), coord(-1,  1).tostring())
	ASSERT_EQUAL(dir.to_coord(dir.northwest).tostring(), coord(-1,  0).tostring())
	ASSERT_EQUAL(dir.to_coord(dir.north    ).tostring(), coord( 0, -1).tostring())
	ASSERT_EQUAL(dir.to_coord(dir.northeast).tostring(), coord( 1, -1).tostring())

	// Straight axes sum to zero (opposite neighbours cancel).
	ASSERT_EQUAL(dir.to_coord(dir.northsouth         ).tostring(), coord(0, 0).tostring())
	ASSERT_EQUAL(dir.to_coord(dir.northeast_southwest).tostring(), coord(0, 0).tostring())
	ASSERT_EQUAL(dir.to_coord(dir.northwest_southeast).tostring(), coord(0, 0).tostring())
	ASSERT_EQUAL(dir.to_coord(dir.all).tostring(),                 coord(0, 0).tostring())

	// A bend sums the two adjacent neighbours.
	ASSERT_EQUAL(dir.to_coord(dir.north | dir.northeast).tostring(), coord( 1, -2).tostring())
	ASSERT_EQUAL(dir.to_coord(dir.south | dir.southwest).tostring(), coord(-1,  2).tostring())

	// Out-of-range dirs (bits above bit 5) raise.
	local error_raised = false
	try {
		dir.to_coord(64)
	}
	catch (e) {
		ASSERT_EQUAL(e, "Invalid dir 64 (valid values are 0..63)")
		error_raised = true
	}
	ASSERT_TRUE(error_raised)
}
