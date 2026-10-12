//
// This file is part of the Simutrans project under the Artistic License.
// (see LICENSE.txt)
//


//
// Tests for the site search of the computer players (player/ai.cc): where a
// station goes near a factory.  They drive ai_t::suche_platz through the test
// support in api_ai_test.cc, on a goods AI that is created and dropped inside
// each test and never gets to step.
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
