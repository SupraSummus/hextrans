//
// This file is part of the Simutrans project under the Artistic License.
// (see LICENSE.txt)
//


//
// Tests for the site search of the computer players (player/ai.cc): where a
// station goes near a factory, and where a harbour and the rail station
// behind it go on a shore.  They drive ai_t::suche_platz and
// ai_t::find_harbour through the test support in api_ai_test.cc, on a goods
// AI that is created and dropped inside each test and never gets to step.
//
// The result of a search is the text "found,x,y,run.x,run.y": the first tile
// of the station, and the vector from it to its last tile, a hex direction
// times a length.
//

function ai_site_make_ai()
{
	local slot = -1
	for (local i = 2; i < 8; i++) {
		if (!player_x(i).is_valid()) {
			slot = i
			break
		}
	}
	ASSERT_TRUE(slot >= 2)
	ASSERT_TRUE(world.create_player(slot, 2)) // 2 == player_t::AI_GOODS
	return slot
}


function ai_site_drop_ai(slot)
{
	ASSERT_TRUE(world.remove_player(player_x(slot)))
}


function ai_site_text(found, x, y, rx, ry)
{
	return (found ? "1" : "0") + "," + x + "," + y + "," + rx + "," + ry
}


function ai_site_parse(text)
{
	local numbers = []
	local from = 0
	for (local i = 0; i <= text.len(); i++) {
		if (i == text.len() || text[i] == ',') {
			numbers.append(text.slice(from, i).tointeger())
			from = i + 1
		}
	}
	return numbers
}


// Hex steps from (x, y) to the nearest tile of the w by h footprint at (fx, fy).
function ai_site_dist_to_rect(x, y, fx, fy, w, h)
{
	local dx = x - min(max(x, fx), fx + w - 1)
	local dy = y - min(max(y, fy), fy + h - 1)
	return max(max(abs(dx), abs(dy)), abs(dx + dy))
}


// The hex directions in clockwise order, from SE.
ai_site_ring <- [[1, 0], [0, 1], [-1, 1], [-1, 0], [0, -1], [1, -1]]


// `d` turned `steps` times by 60 degrees clockwise.
function ai_site_rotate(d, steps)
{
	for (local i = 0; i < 6; i++) {
		if (ai_site_ring[i][0] == d.x && ai_site_ring[i][1] == d.y) {
			local r = ai_site_ring[(i + steps + 6) % 6]
			return coord(r[0], r[1])
		}
	}
	ASSERT_TRUE(false)
}


// The entry of dock_faces() for the direction `face` of the water.
function ai_site_face(face)
{
	foreach (f in dock_faces()) {
		if (f[0] == face) {
			return f
		}
	}
	ASSERT_TRUE(false)
}


// A shore tile `k` that faces `face`, with `n` tiles of water in front of it:
// the tile of a sea coast, which has the height of its own water.
function ai_site_build_coast(pl, k, f, n)
{
	set_dock_water(pl, k, f[0], n, cl_water)
	ASSERT_EQUAL(command_x.set_slope(pl, k, f[1]), null)
	aitest_set_water_hgt(k.x, k.y, k.z)
}


function ai_site_remove_coast(pl, k, f, n, ground_water)
{
	aitest_set_water_hgt(k.x, k.y, ground_water)
	ASSERT_EQUAL(command_x.set_slope(pl, k, slope.flat), null)
	set_dock_water(pl, k, f[0], n, cl_mediterran)
}


// A station for a factory has to stand where its catchment reaches the
// factory.  The tiles that do are the hex disc around the footprint; the
// axial square around it has corners twice as far out, along (+1,+1) and
// (-1,-1), and the search used to pick the corner closest to its target.
function test_ai_site_station_reaches_factory()
{
	local slot = ai_site_make_ai()
	local cov = settings.get_station_coverage()
	local fab = coord(8, 8)

	// the six directions, and the six between them
	local steps = [[1, 0], [0, 1], [-1, 1], [-1, 0], [0, -1], [1, -1], [1, 1], [-1, -1], [2, -1], [-2, 1], [1, -2], [-1, 2]]

	foreach (size in [1, 2]) {
		foreach (length in [0, 2]) {
			foreach (s in steps) {
				local target = coord(fab.x + s[0] * 5, fab.y + s[1] * 5)
				local found = ai_site_parse(aitest_suche_platz(slot, fab, coord(size, size), target, length))

				ASSERT_EQUAL(found[0], 1)
				ASSERT_TRUE(ai_site_dist_to_rect(found[1], found[2], fab.x, fab.y, size, size) <= cov)
			}
		}
	}

	ai_site_drop_ai(slot)
}


// On a shore slope of each of the six directions the harbour is the shore
// tile, and the rail station behind it runs inland along the slope.
function test_ai_site_harbour_all_faces()
{
	local slot = ai_site_make_ai()
	local pl = player_x(0)
	local k = coord3d(8, 8, 0)
	local ground_water = aitest_get_water_hgt(k.x, k.y)

	foreach (f in dock_faces()) {
		local v = dir.to_coord(f[0])
		ai_site_build_coast(pl, k, f, 3)

		local water = coord(k.x + v.x * 2, k.y + v.y * 2)
		local target = coord(k.x - v.x * 5, k.y - v.y * 5)
		foreach (length in [0, 2]) {
			ASSERT_EQUAL(aitest_find_harbour(slot, water, target, length), ai_site_text(true, k.x, k.y, -v.x * length, -v.y * length))
		}

		ai_site_remove_coast(pl, k, f, 3, ground_water)
		RESET_ALL_PLAYER_FUNDS()
	}

	ai_site_drop_ai(slot)
}


// The harbour is looked for within 5 steps of the shore point of the line to
// the destination, a hex disc.  The axial square of 11 by 11 around that point
// has corners 10 steps out, along (+1,+1) and (-1,-1), which the search used
// to take in.
function test_ai_site_harbour_search_window_is_a_disc()
{
	local slot = ai_site_make_ai()
	local pl = player_x(0)
	local face = ai_site_face(dir.south)
	local ground_water = aitest_get_water_hgt(0, 0)

	// With no water on the line, the shore point is its end.  The coast is 4
	// steps from it along (-1,-1), which is 8 steps, or along (-1,+1), which
	// is 4.
	foreach (c in [[coord3d(5, 5, 0), coord(9, 9), coord(13, 13), false], [coord3d(5, 9, 0), coord(9, 5), coord(13, 1), true]]) {
		local k = c[0]
		ai_site_build_coast(pl, k, face, 3)

		local found = aitest_find_harbour(slot, c[1], c[2], 0)
		ASSERT_EQUAL(found, c[3] ? ai_site_text(true, k.x, k.y, 0, 0) : ai_site_text(false, c[1].x, c[1].y, 0, 0))

		ai_site_remove_coast(pl, k, face, 3, ground_water)
		RESET_ALL_PLAYER_FUNDS()
	}

	ai_site_drop_ai(slot)
}


// The length of the station is the number of steps of its run, whichever
// direction the run takes: after a shore on the NE-SW axis the search went on
// with a station of no length, because that run adds up to 0 in x + y.
function test_ai_site_harbour_run_length_after_ne_sw_shore()
{
	local slot = ai_site_make_ai()
	local pl = player_x(0)
	local ground_water = aitest_get_water_hgt(0, 0)

	// the search finds the SW shore first and then the S shore, which is
	// closer to the destination
	local a = coord3d(7, 8, 0)
	local b = coord3d(8, 9, 0)
	local fa = ai_site_face(dir.southwest)
	local fb = ai_site_face(dir.south)
	ai_site_build_coast(pl, a, fa, 3)
	ai_site_build_coast(pl, b, fb, 3)

	ASSERT_EQUAL(aitest_find_harbour(slot, coord(8, 11), coord(12, 13), 2), ai_site_text(true, 8, 9, 0, -2))

	ai_site_remove_coast(pl, b, fb, 3, ground_water)
	ai_site_remove_coast(pl, a, fa, 3, ground_water)
	RESET_ALL_PLAYER_FUNDS()
	ai_site_drop_ai(slot)
}


// Where the station cannot run inland along the slope the search tries
// another direction.  It is one of the two at 60 degrees from the slope's, so
// away from the water; swapping x and y, as on the square grid, gave the
// direction of the water for the NE and SW shores.
function test_ai_site_harbour_second_direction()
{
	local slot = ai_site_make_ai()
	local pl = player_x(0)
	local k = coord3d(8, 8, 0)
	local ground_water = aitest_get_water_hgt(k.x, k.y)
	local road = way_desc_x.get_available_ways(wt_road, st_flat)[0].get_name()
	local length = 2

	foreach (f in dock_faces()) {
		local v = dir.to_coord(f[0])
		local uphill = coord(-v.x, -v.y)
		ai_site_build_coast(pl, k, f, 3)

		// a road across the far end of the run along the slope
		local blocker = coord3d(k.x + uphill.x * (length + 1), k.y + uphill.y * (length + 1), 0)
		local beyond = coord3d(k.x + uphill.x * (length + 2), k.y + uphill.y * (length + 2), 0)
		ASSERT_EQUAL(command_x(tool_build_way).work(pl, blocker, beyond, road), null)

		local water = coord(k.x + v.x * 2, k.y + v.y * 2)
		local target = coord(k.x + uphill.x * 5, k.y + uphill.y * 5)
		local cw = ai_site_rotate(uphill, 1)
		local ccw = ai_site_rotate(uphill, -1)
		local found = aitest_find_harbour(slot, water, target, length)
		ASSERT_TRUE(found == ai_site_text(true, k.x, k.y, cw.x * length, cw.y * length)
			|| found == ai_site_text(true, k.x, k.y, ccw.x * length, ccw.y * length))

		ASSERT_EQUAL(command_x(tool_remove_way).work(pl, blocker, beyond, "" + wt_road), null)
		ai_site_remove_coast(pl, k, f, 3, ground_water)
		RESET_ALL_PLAYER_FUNDS()
	}

	ai_site_drop_ai(slot)
}


// The dock the goods AI builds is at most as long as asked for.  The tool
// refuses a longer one on a shore of the NE-SW axis.
function test_ai_site_random_dock_length_limit()
{
	foreach (limit in [1, 2, 4]) {
		for (local i = 0; i < 40; i++) {
			local name = aitest_random_dock(limit)
			ASSERT_TRUE(name != "")
			ASSERT_TRUE(building_desc_x(name).get_size(0).y <= limit)
		}
	}
	ASSERT_TRUE(aitest_random_dock(0) != "")
}


// A station of its own that stands where the catchment reaches the factory is
// extended from any of the six tiles around it.  The search tried the four
// that are neighbours on the square grid, not the NE and the SW one.
function test_ai_site_extends_own_station_on_all_sides()
{
	local slot = ai_site_make_ai()
	local ai = player_x(slot)
	local road = way_desc_x.get_available_ways(wt_road, st_flat)[0].get_name()
	local cov = settings.get_station_coverage()

	// the stop on the NE rim of the catchment of the factory at (6,10); the
	// closest tile to the destination is its NE neighbour, outside the rim
	local fab = coord(6, 10)
	local stop = coord3d(fab.x + cov, fab.y - cov, 0)
	local behind = coord3d(stop.x, stop.y + 1, 0)
	ASSERT_EQUAL(command_x(tool_build_way).work(ai, stop, behind, road), null)
	ASSERT_EQUAL(command_x(tool_build_station).work(ai, stop, "BusStop"), null)

	local target = coord(fab.x + 10, fab.y - 10)
	ASSERT_EQUAL(aitest_suche_platz(slot, fab, coord(1, 1), target, 0), ai_site_text(true, stop.x + 1, stop.y - 1, 0, 0))

	ASSERT_EQUAL(command_x(tool_remover).work(ai, stop), null)
	ASSERT_EQUAL(command_x(tool_remove_way).work(ai, stop, behind, "" + wt_road), null)
	ai_site_drop_ai(slot)
	RESET_ALL_PLAYER_FUNDS()
}
