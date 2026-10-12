//
// This file is part of the Simutrans project under the Artistic License.
// (see LICENSE.txt)
//


//
// Tests for the "generate script" tool: export an area, remove what is in
// it and replay the script with the build toolkit (script/hm_toolkit_v3.nut).
//


// Replays a script from world.generate_build_script with its origin at
// `origin` and returns the error message of the toolkit (null if all went
// well).  The script includes the toolkit, init() collects its commands and
// work() executes them.
function replay_generated_script(pl, script, origin)
{
	// loading the toolkit is expensive: start with a full instruction budget
	sleep()
	compilestring(script).call(getroottable())
	ASSERT_TRUE(init(pl))
	return work(pl, origin)
}


// Descriptor name and unmasked directions of the way of type `wt` on `pos`,
// as one string; "-" if there is no way.
function way_signature(pos, wt)
{
	local way = tile_x(pos.x, pos.y, pos.z).get_way(wt)
	return way == null ? "-" : way.get_desc().get_name() + ":" + way_dirs_at(pos, wt)
}


function test_script_generator_northeast_southwest_road()
{
	local pl = player_x(0)
	local road = way_desc_x("dirt_road")

	// A road along the NE-SW axis that ends in a 60 degree bend towards S.
	local sw = coord3d(5, 11, 0)
	local ne = coord3d(9, 7, 0)
	local south_end = coord3d(9, 10, 0)
	local tiles = []
	for (local i = 0; i < 5; i++) {
		tiles.append(coord3d(5 + i, 11 - i, 0))
	}
	tiles.append(coord3d(9, 8, 0))
	tiles.append(coord3d(9, 9, 0))
	tiles.append(south_end)

	ASSERT_EQUAL(command_x.build_way(pl, sw, ne, road, true), null)
	ASSERT_EQUAL(command_x.build_way(pl, ne, south_end, road, true), null)
	ASSERT_EQUAL(way_dirs_at(sw, wt_road), dir.northeast)
	ASSERT_EQUAL(way_dirs_at(coord3d(7, 9, 0), wt_road), dir.northeast_southwest)
	ASSERT_EQUAL(way_dirs_at(ne, wt_road), dir.southwest | dir.south)
	ASSERT_EQUAL(way_dirs_at(south_end, wt_road), dir.north)

	local before = []
	foreach (pos in tiles) {
		before.append(way_signature(pos, wt_road))
	}

	local corner1 = coord3d(4, 6, 0)
	local corner2 = coord3d(10, 12, 0)
	local script = world.generate_build_script(pl, corner1, corner2)
	ASSERT_TRUE(script != null)
	// both straight pieces are written as one command each, the NE-SW one included
	ASSERT_TRUE(script.find("hm_way_tl(\"dirt_road\",[1,5,0],[5,1,0],") != null)
	ASSERT_TRUE(script.find("hm_way_tl(\"dirt_road\",[5,4,0],[5,1,0],") != null)

	ASSERT_EQUAL(command_x(tool_remove_way).work(pl, sw, ne, "" + wt_road), null)
	ASSERT_EQUAL(command_x(tool_remove_way).work(pl, ne, south_end, "" + wt_road), null)
	foreach (pos in tiles) {
		ASSERT_EQUAL(way_signature(pos, wt_road), "-")
	}

	ASSERT_EQUAL(replay_generated_script(pl, script, corner1), null)
	foreach (i, pos in tiles) {
		ASSERT_EQUAL(way_signature(pos, wt_road), before[i])
	}

	ASSERT_EQUAL(command_x(tool_remove_way).work(pl, sw, ne, "" + wt_road), null)
	ASSERT_EQUAL(command_x(tool_remove_way).work(pl, ne, south_end, "" + wt_road), null)
	RESET_ALL_PLAYER_FUNDS()
}


// Unmasked and masked directions of the way of type `wt` on `pos` and the
// sign standing there, as one string.
function sign_signature(pos, wt)
{
	local tile = tile_x(pos.x, pos.y, pos.z)
	local sign = tile.find_object(mo_signal)
	if (sign == null) {
		sign = tile.find_object(mo_roadsign)
	}
	ASSERT_TRUE(sign != null)
	return sign.get_desc().get_name() + ":" + tile.get_way_dirs(wt) + ":" + tile.get_way_dirs_masked(wt)
}


// Removes the sign on `mid` and then the ways with legs towards `leg_a` and
// `leg_b`.  The sign has to go first: a sign that blocks one direction
// leaves no route for the way remover.
function remove_ways_with_sign(pl, wt, leg_a, mid, leg_b)
{
	ASSERT_EQUAL(command_x(tool_remover).work(pl, mid), null)
	ASSERT_EQUAL(command_x(tool_remove_way).work(pl, leg_a, mid, "" + wt), null)
	ASSERT_EQUAL(command_x(tool_remove_way).work(pl, mid, leg_b, "" + wt), null)
	ASSERT_EQUAL(way_dirs_at(mid, wt), 0)
}


// Puts ways with legs towards `dir_a` and `dir_b` on `mid`, clicks `sign`
// onto `mid` until it has been through all of its `states`, and after every
// click lets the script generator export the area, replaces the ways by the
// replayed script and checks that the sign was rebuilt the way it stood.
function check_sign_roundtrip(pl, way_desc, sign, mid, dir_a, dir_b, states)
{
	local wt = way_desc.get_waytype()
	local leg_a = mid + dir.to_coord(dir_a)
	local leg_b = mid + dir.to_coord(dir_b)
	local corner1 = mid - coord3d(2, 2, 0)
	local corner2 = mid + coord3d(2, 2, 0)
	local seen = {}

	ASSERT_EQUAL(command_x.build_way(pl, leg_a, mid, way_desc, true), null)
	ASSERT_EQUAL(command_x.build_way(pl, mid, leg_b, way_desc, true), null)

	for (local click = 1; click <= states; click++) {
		ASSERT_EQUAL(command_x.build_sign_at(pl, mid, sign), null)
		local expected = sign_signature(mid, wt)
		// every click has to show up as a state of its own, otherwise
		// the replay could not tell them apart
		ASSERT_FALSE(expected in seen)
		seen[expected] <- click

		local script = world.generate_build_script(pl, corner1, corner2)
		ASSERT_TRUE(script != null)

		remove_ways_with_sign(pl, wt, leg_a, mid, leg_b)

		ASSERT_EQUAL(replay_generated_script(pl, script, corner1), null)
		ASSERT_EQUAL(sign_signature(mid, wt), expected)
	}

	remove_ways_with_sign(pl, wt, leg_a, mid, leg_b)
}


// All pairs of different directions: the three straight axes and the 60
// and 120 degree bends.
function all_leg_pairs()
{
	local pairs = []
	for (local a = 0; a < 6; a++) {
		for (local b = a + 1; b < 6; b++) {
			pairs.append([dir.nsew[a], dir.nsew[b]])
		}
	}
	return pairs
}


function test_script_generator_oneway_sign_direction()
{
	local pl = player_x(0)
	local road = way_desc_x("dirt_road")
	local sign = sign_desc_x.get_available_signs(wt_road).filter(@(idx, sign) sign.is_one_way())[0]
	ASSERT_TRUE(sign != null)

	foreach (legs in all_leg_pairs()) {
		check_sign_roundtrip(pl, road, sign, coord3d(8, 8, 0), legs[0], legs[1], 2)
	}
	RESET_ALL_PLAYER_FUNDS()
}


function test_script_generator_signal_direction()
{
	local pl = player_x(0)
	local rail = way_desc_x.get_available_ways(wt_rail, st_flat)[0]
	local signal = sign_desc_x.get_available_signs(wt_rail).filter(@(idx, sign) sign.is_signal())[0]
	ASSERT_TRUE(signal != null)

	foreach (legs in all_leg_pairs()) {
		check_sign_roundtrip(pl, rail, signal, coord3d(8, 8, 0), legs[0], legs[1], 3)
	}
	RESET_ALL_PLAYER_FUNDS()
}
