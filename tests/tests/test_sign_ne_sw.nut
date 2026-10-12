//
// This file is part of the Simutrans project under the Artistic License.
// (see LICENSE.txt)
//


//
// Signs, signals and traffic lights on the NE-SW axis
//
// A sign has art for four directions (N, S, NW and SE) in the pakset, and the
// engine shows NE and SW with the art of N and S, unless the sign has NE and
// SW art of its own. pak64 has none, so most of these tests compare what a
// sign on the NE-SW axis shows with what the same sign shows on the N-S axis:
// whether an image is there and in which of the two layers, the one behind
// the vehicles and the one before them. The signs of
// tests/test-pak/test_sign_ne_sw.dat get their art from the NE and SW lists
// only, and are told apart by having an image or none.
//
// The images are looked at through get_image and get_front_image, which is
// -1 for no image. A build without graphics gives every image that has data
// the same id, so it can tell an image from none, and that is all these tests
// ask of it: none of them is skipped there.
//
// The direction of a signal is the one it faces: the way blocks that
// direction. Building a signal again cycles it through both ways open, SW
// blocked, NE blocked and both ways open again.
//


// "back/front": the image ids of the sign or signal on the tile, -1 for no
// image. Only for comparing the images of two tiles.
function sign_images_at(pos)
{
	local t = tile_x(pos.x, pos.y, pos.z)
	local s = t.find_object(mo_signal)
	if (s == null) {
		s = t.find_object(mo_roadsign)
	}
	ASSERT_TRUE(s != null)
	return "" + s.get_image() + "/" + s.get_front_image()
}


// Whether the sign or signal on the tile has an image behind the vehicles
// (back) and one before them (front).
function ASSERT_SIGN_DRAWN(pos, back, front)
{
	local t = tile_x(pos.x, pos.y, pos.z)
	local s = t.find_object(mo_signal)
	if (s == null) {
		s = t.find_object(mo_roadsign)
	}
	ASSERT_TRUE(s != null)
	ASSERT_EQUAL(s.get_image() != -1, back)
	ASSERT_EQUAL(s.get_front_image() != -1, front)
}


function test_sign_signal_ne_sw()
{
	local pl = player_x(0)
	local wayremover = command_x(tool_remove_way)
	local rail = way_desc_x.get_available_ways(wt_rail, st_flat)[0]
	local signal = sign_desc_x.get_available_signs(wt_rail).filter(@(idx, sign) sign.is_signal())[0]

	// NE-SW runs +x, -y; the reference signal stands on the N-S axis
	local ne_sw = coord3d(4, 4, 0)
	local n_s   = coord3d(9, 4, 0)

	ASSERT_EQUAL(command_x.build_way(pl, coord3d(6, 2, 0), coord3d(2, 6, 0), rail, true), null)
	ASSERT_EQUAL(command_x.build_way(pl, coord3d(9, 2, 0), coord3d(9, 6, 0), rail, true), null)

	// NE end SW=4, middle NE|SW=36, SW end NE=32
	ASSERT_WAY_PATTERN(wt_rail, coord3d(0, 0, 0),
		[
			[0, 0, 0,  0,  0, 0, 0, 0],
			[0, 0, 0,  0,  0, 0, 0, 0],
			[0, 0, 0,  0,  0, 0, 4, 0],
			[0, 0, 0,  0,  0, 36, 0, 0],
			[0, 0, 0,  0, 36, 0, 0, 0],
			[0, 0, 0, 36,  0, 0, 0, 0],
			[0, 0, 32, 0,  0, 0, 0, 0],
			[0, 0, 0,  0,  0, 0, 0, 0],
		])

	local blocked = [dir.northeast_southwest, dir.southwest, dir.northeast, dir.northeast_southwest]
	local open    = [dir.northeast_southwest, dir.northeast, dir.southwest, dir.northeast_southwest]

	local reference = []
	for (local i = 0; i < 4; i++) {
		ASSERT_EQUAL(command_x.build_sign_at(pl, n_s, signal), null)
		reference.append(sign_images_at(n_s))
	}
	// the N-S signal shows something, and a different thing for each of its two
	// directions, or the comparison below would prove nothing
	ASSERT_TRUE(reference[0] != "-1/-1")
	ASSERT_TRUE(reference[1] != reference[2])

	local way = tile_x(ne_sw.x, ne_sw.y, ne_sw.z).find_object(mo_way)
	for (local i = 0; i < 4; i++) {
		ASSERT_EQUAL(command_x.build_sign_at(pl, ne_sw, signal), null)

		local s = tile_x(ne_sw.x, ne_sw.y, ne_sw.z).find_object(mo_signal)
		ASSERT_TRUE(s != null)
		ASSERT_TRUE(s.is_valid())
		ASSERT_EQUAL(s.get_dirs(), blocked[i])
		ASSERT_EQUAL(way.get_dirs(), dir.northeast_southwest)
		ASSERT_EQUAL(way.get_dirs_masked(), open[i])

		// NE is drawn as N and SW as S: the images are those of the N-S signal
		ASSERT_EQUAL(sign_images_at(ne_sw), reference[i])
	}

	ASSERT_EQUAL(command_x(tool_remover).work(pl, ne_sw), null)
	ASSERT_EQUAL(command_x(tool_remover).work(pl, n_s), null)
	ASSERT_EQUAL(wayremover.work(pl, coord3d(6, 2, 0), coord3d(2, 6, 0), "" + wt_rail), null)
	ASSERT_EQUAL(wayremover.work(pl, coord3d(9, 2, 0), coord3d(9, 6, 0), "" + wt_rail), null)

	RESET_ALL_PLAYER_FUNDS()
}


// A signal with art in the NE and SW lists only: the NE image of the red state
// is the one image there is, and it has to be found where the NE-SW signal
// looks for it and nowhere else.
function test_sign_signal_ne_sw_art()
{
	local pl = player_x(0)
	local wayremover = command_x(tool_remove_way)
	local rail = way_desc_x.get_available_ways(wt_rail, st_flat)[0]
	local signal = sign_desc_x("test_signal_ne_sw")

	ASSERT_TRUE(signal.is_signal())

	local ne_sw   = coord3d(4, 4, 0)
	local n_s     = coord3d(9, 4, 0)
	local nw_se   = coord3d(4, 10, 0)

	ASSERT_EQUAL(command_x.build_way(pl, coord3d(6, 2, 0), coord3d(2, 6, 0), rail, true), null)
	ASSERT_EQUAL(command_x.build_way(pl, coord3d(9, 2, 0), coord3d(9, 6, 0), rail, true), null)
	ASSERT_EQUAL(command_x.build_way(pl, coord3d(2, 10, 0), coord3d(6, 10, 0), rail, true), null)

	// the signal draws the directions it does not block: both with both ways
	// open, NE only when SW is blocked, SW only when NE is blocked. NE stands on
	// the right of its way and is drawn before the vehicles, SW behind them
	local ne_art = [true, true, false, true]
	for (local i = 0; i < 4; i++) {
		ASSERT_EQUAL(command_x.build_sign_at(pl, ne_sw, signal), null)
		ASSERT_SIGN_DRAWN(ne_sw, false, ne_art[i])
	}

	// no art for the other axes, and the NE image is not theirs
	for (local i = 0; i < 4; i++) {
		ASSERT_EQUAL(command_x.build_sign_at(pl, n_s, signal), null)
		ASSERT_SIGN_DRAWN(n_s, false, false)
		ASSERT_EQUAL(command_x.build_sign_at(pl, nw_se, signal), null)
		ASSERT_SIGN_DRAWN(nw_se, false, false)
	}

	ASSERT_EQUAL(command_x(tool_remover).work(pl, ne_sw), null)
	ASSERT_EQUAL(command_x(tool_remover).work(pl, n_s), null)
	ASSERT_EQUAL(command_x(tool_remover).work(pl, nw_se), null)
	ASSERT_EQUAL(wayremover.work(pl, coord3d(6, 2, 0), coord3d(2, 6, 0), "" + wt_rail), null)
	ASSERT_EQUAL(wayremover.work(pl, coord3d(9, 2, 0), coord3d(9, 6, 0), "" + wt_rail), null)
	ASSERT_EQUAL(wayremover.work(pl, coord3d(2, 10, 0), coord3d(6, 10, 0), "" + wt_rail), null)

	RESET_ALL_PLAYER_FUNDS()
}


function test_sign_oneway_ne_sw()
{
	local pl = player_x(0)
	local wayremover = command_x(tool_remove_way)
	local road = way_desc_x("dirt_road")
	local sign = sign_desc_x.get_available_signs(wt_road).filter(@(idx, sign) sign.is_one_way())[0]

	local ne_sw = coord3d(4, 4, 0)
	local n_s   = coord3d(9, 4, 0)

	ASSERT_EQUAL(command_x.build_way(pl, coord3d(6, 2, 0), coord3d(2, 6, 0), road, true), null)
	ASSERT_EQUAL(command_x.build_way(pl, coord3d(9, 2, 0), coord3d(9, 6, 0), road, true), null)

	// the first sign faces the first direction of the way in the order SE, S, SW,
	// NW, N, NE, which is SW here and S on the N-S axis; clicking again turns it
	local blocked = [dir.southwest, dir.northeast, dir.southwest]
	local open    = [dir.northeast, dir.southwest, dir.northeast]

	local reference = []
	for (local i = 0; i < 3; i++) {
		ASSERT_EQUAL(command_x.build_sign_at(pl, n_s, sign), null)
		reference.append(sign_images_at(n_s))
	}
	ASSERT_TRUE(reference[0] != "-1/-1")
	ASSERT_TRUE(reference[0] != reference[1])

	local way = tile_x(ne_sw.x, ne_sw.y, ne_sw.z).find_object(mo_way)
	for (local i = 0; i < 3; i++) {
		ASSERT_EQUAL(command_x.build_sign_at(pl, ne_sw, sign), null)

		local s = tile_x(ne_sw.x, ne_sw.y, ne_sw.z).find_object(mo_roadsign)
		ASSERT_TRUE(s != null)
		ASSERT_TRUE(s.can_pass(pl))
		ASSERT_EQUAL(s.get_dirs(), blocked[i])
		ASSERT_EQUAL(way.get_dirs(), dir.northeast_southwest)
		ASSERT_EQUAL(way.get_dirs_masked(), open[i])

		ASSERT_EQUAL(sign_images_at(ne_sw), reference[i])
	}

	ASSERT_EQUAL(command_x(tool_remover).work(pl, ne_sw), null)
	ASSERT_EQUAL(command_x(tool_remover).work(pl, n_s), null)
	ASSERT_EQUAL(wayremover.work(pl, coord3d(6, 2, 0), coord3d(2, 6, 0), "" + wt_road), null)
	ASSERT_EQUAL(wayremover.work(pl, coord3d(9, 2, 0), coord3d(9, 6, 0), "" + wt_road), null)

	RESET_ALL_PLAYER_FUNDS()
}


// A sign with a front image list: facing one direction it shows the back
// image from the NE and SW list, and the front image from the NE and SW front
// list. Only the back image of NE and the front image of SW exist.
function test_sign_oneway_ne_sw_art()
{
	local pl = player_x(0)
	local wayremover = command_x(tool_remove_way)
	local road = way_desc_x("dirt_road")
	local sign = sign_desc_x("test_oneway_ne_sw")

	ASSERT_TRUE(sign.is_one_way())

	local ne_sw = coord3d(4, 4, 0)
	local n_s   = coord3d(9, 4, 0)

	ASSERT_EQUAL(command_x.build_way(pl, coord3d(6, 2, 0), coord3d(2, 6, 0), road, true), null)
	ASSERT_EQUAL(command_x.build_way(pl, coord3d(9, 2, 0), coord3d(9, 6, 0), road, true), null)

	// facing SW (the first click), then NE
	ASSERT_EQUAL(command_x.build_sign_at(pl, ne_sw, sign), null)
	ASSERT_EQUAL(tile_x(ne_sw.x, ne_sw.y, ne_sw.z).find_object(mo_roadsign).get_dirs(), dir.southwest)
	ASSERT_SIGN_DRAWN(ne_sw, false, true)

	ASSERT_EQUAL(command_x.build_sign_at(pl, ne_sw, sign), null)
	ASSERT_EQUAL(tile_x(ne_sw.x, ne_sw.y, ne_sw.z).find_object(mo_roadsign).get_dirs(), dir.northeast)
	ASSERT_SIGN_DRAWN(ne_sw, true, false)

	// the lists of the four old directions have no image
	for (local i = 0; i < 2; i++) {
		ASSERT_EQUAL(command_x.build_sign_at(pl, n_s, sign), null)
		ASSERT_SIGN_DRAWN(n_s, false, false)
	}

	ASSERT_EQUAL(command_x(tool_remover).work(pl, ne_sw), null)
	ASSERT_EQUAL(command_x(tool_remover).work(pl, n_s), null)
	ASSERT_EQUAL(wayremover.work(pl, coord3d(6, 2, 0), coord3d(2, 6, 0), "" + wt_road), null)
	ASSERT_EQUAL(wayremover.work(pl, coord3d(9, 2, 0), coord3d(9, 6, 0), "" + wt_road), null)

	RESET_ALL_PLAYER_FUNDS()
}


function test_sign_private_way_ne_sw()
{
	local pl = player_x(0)
	local public_pl = player_x(1)
	local wayremover = command_x(tool_remove_way)
	local road = way_desc_x("dirt_road")

	// N-S, NW-SE and NE-SW road, a private sign on each
	local pos = [coord3d(4, 4, 0), coord3d(4, 10, 0), coord3d(10, 6, 0)]
	local roads = [
		[coord3d(4, 2, 0), coord3d(4, 6, 0)],
		[coord3d(2, 10, 0), coord3d(6, 10, 0)],
		[coord3d(12, 4, 0), coord3d(8, 8, 0)],
	]
	local axis = [dir.northsouth, dir.northwest_southeast, dir.northeast_southwest]

	foreach (r in roads) {
		ASSERT_EQUAL(command_x.build_way(pl, r[0], r[1], road, true), null)
	}

	// pak64: the sign keeps the road to the owner on any axis, and NE-SW
	// shows the gate of N-S
	{
		local sign = sign_desc_x.get_available_signs(wt_road).filter(@(idx, sign) sign.is_private_way())[0]
		foreach (i, p in pos) {
			ASSERT_EQUAL(command_x.build_sign_at(pl, p, sign), null)
			local s = tile_x(p.x, p.y, p.z).find_object(mo_roadsign)
			ASSERT_TRUE(s != null)
			ASSERT_TRUE(s.can_pass(pl))
			ASSERT_FALSE(s.can_pass(public_pl))
			ASSERT_EQUAL(s.get_dirs(), axis[i])
			ASSERT_EQUAL(tile_x(p.x, p.y, p.z).find_object(mo_way).get_dirs_masked(), axis[i])
			ASSERT_SIGN_DRAWN(p, true, false)
		}
		ASSERT_EQUAL(sign_images_at(pos[2]), sign_images_at(pos[0]))

		foreach (p in pos) {
			ASSERT_EQUAL(command_x(tool_remover).work(pl, p), null)
		}
	}

	// the gates of test_private_ne_sw: the NW-SE gate and the NE-SW gate are
	// open for their owner, the N-S gate is not
	{
		local sign = sign_desc_x("test_private_ne_sw")
		ASSERT_TRUE(sign.is_private_way())

		local drawn = [false, true, true]
		foreach (i, p in pos) {
			ASSERT_EQUAL(command_x.build_sign_at(pl, p, sign), null)
			ASSERT_SIGN_DRAWN(p, drawn[i], false)
			ASSERT_FALSE(tile_x(p.x, p.y, p.z).find_object(mo_roadsign).can_pass(public_pl))
		}

		foreach (p in pos) {
			ASSERT_EQUAL(command_x(tool_remover).work(pl, p), null)
		}
	}

	foreach (r in roads) {
		ASSERT_EQUAL(wayremover.work(pl, r[0], r[1], "" + wt_road), null)
	}

	RESET_ALL_PLAYER_FUNDS()
}


// A traffic light switches by game time: a whole round of its axes takes
// 700 to 1100 steps. Advances the simulation until each light has shown the
// number of states in wanted, or gives up. The result is, for each light, the
// directions and the images the light had when each of its states came up.
// The state a light is first seen in is left out: a new light stands in state 0
// with every direction of its tile until the first state change.
function observe_trafficlights(lights, wanted)
{
	local seen = lights.map(@(l) {})
	local last = lights.map(@(l) -1)

	for (local step = 0; step < 3000; step++) {
		sleep()

		local done = true
		foreach (i, l in lights) {
			local st = l.get_state()
			if (st != last[i]) {
				if (last[i] != -1) {
					seen[i][st] <- [l.get_dirs(), l.get_image() + "/" + l.get_front_image()]
				}
				last[i] = st
			}
			done = done  &&  seen[i].len() == wanted[i]
		}
		if (done) {
			break
		}
	}
	return seen
}


function test_sign_trafficlight_ne_sw()
{
	local pl = player_x(0)
	local wayremover = command_x(tool_remove_way)
	local road = way_desc_x.get_available_ways(wt_road, st_flat)[0]
	local trafficlight = sign_desc_x.get_available_signs(wt_road).filter(@(idx, sign) sign.is_traffic_light())[0]

	// the axes in the order of the states: green 0, 1 and 2, yellow 3, 4 and 5
	local axes = [dir.northsouth, dir.northwest_southeast, dir.northeast_southwest]

	// a six-way junction with all three axes at (4,4), and two Y junctions: the
	// NE-SW road with an arm to the SE at (4,12), which has no N-S axis, and the
	// N-S road with an arm to the SE at (11,12), which has no NE-SW axis
	local lights_at = [coord3d(4, 4, 0), coord3d(4, 12, 0), coord3d(11, 12, 0)]
	local roads = [
		[coord3d(4, 1, 0),  coord3d(4, 7, 0)],
		[coord3d(1, 4, 0),  coord3d(7, 4, 0)],
		[coord3d(7, 1, 0),  coord3d(1, 7, 0)],
		[coord3d(7, 9, 0),  coord3d(1, 15, 0)],
		[coord3d(4, 12, 0), coord3d(7, 12, 0)],
		[coord3d(11, 9, 0), coord3d(11, 15, 0)],
		[coord3d(11, 12, 0), coord3d(14, 12, 0)],
	]
	foreach (r in roads) {
		ASSERT_EQUAL(command_x.build_way(pl, r[0], r[1], road, true), null)
	}

	ASSERT_EQUAL(tile_x(4, 4, 0).get_way_dirs(wt_road), dir.all)
	ASSERT_EQUAL(tile_x(4, 12, 0).get_way_dirs(wt_road), dir.northeast | dir.southwest | dir.southeast)
	ASSERT_EQUAL(tile_x(11, 12, 0).get_way_dirs(wt_road), dir.north | dir.south | dir.southeast)

	foreach (p in lights_at) {
		ASSERT_EQUAL(command_x.build_sign_at(pl, p, trafficlight), null)
	}
	local lights = lights_at.map(@(p) tile_x(p.x, p.y, p.z).find_object(mo_roadsign))
	foreach (l in lights) {
		ASSERT_TRUE(l != null)
		ASSERT_TRUE(l.get_desc().is_traffic_light())
	}

	local seen = observe_trafficlights(lights, [6, 4, 4])

	// every axis that has a road gets its turn: green with the axis as the
	// direction, then yellow with none; the lights always show something
	for (local i = 0; i < 3; i++) {
		foreach (st, v in seen[i]) {
			ASSERT_EQUAL(v[0], st < 3 ? axes[st % 3] : dir.none)
			ASSERT_TRUE(v[1] != "-1/-1")
		}
	}
	for (local st = 0; st < 6; st++) {
		ASSERT_TRUE(st in seen[0])
		ASSERT_EQUAL(st in seen[1], st % 3 != 0) // no N-S road
		ASSERT_EQUAL(st in seen[2], st % 3 != 2) // no NE-SW road
	}

	// pak64 has no traffic light art for NE and SW: the arms are drawn as the
	// N and S arms and the NE-SW states with the N-S images. The Y junction
	// of NE, SW and SE then looks like that of N, S and SE: its NE-SW states
	// as the N-S states of the other
	for (local st = 0; st < 6; st++) {
		local other = st % 3 == 2 ? st - 2 : st
		if (st in seen[1]) {
			ASSERT_TRUE(other in seen[2])
			ASSERT_EQUAL(seen[1][st][1], seen[2][other][1])
		}
	}

	foreach (p in lights_at) {
		ASSERT_EQUAL(command_x(tool_remover).work(pl, p), null)
	}
	foreach (r in roads) {
		ASSERT_EQUAL(wayremover.work(pl, r[0], r[1], "" + wt_road), null)
	}

	RESET_ALL_PLAYER_FUNDS()
}


// A signal on a tunnel mouth leaves out the image of the direction that
// leads into the tunnel, which would stand inside the hill. The mouth
// facing NE has the tunnel to the SW, and the mouth facing SW has it to the
// NE: each of them shows the direction it faces.
function test_sign_signal_tunnel_mouth_ne_sw()
{
	local pl = player_x(0)
	local wayremover = command_x(tool_remove_way)
	local rail = way_desc_x.get_available_ways(wt_rail, st_flat)[0]
	local tunnel = tunnel_desc_x.get_available_tunnels(wt_rail)[0]
	local signal = sign_desc_x.get_available_signs(wt_rail).filter(@(idx, sign) sign.is_signal())[0]

	// the hill at (5,5), its mouths at (6,4) facing NE and (4,6) facing SW, the
	// rail outside them, and (7,3) as the same signal on open ground
	local ne_mouth = coord3d(6, 4, 0)
	local sw_mouth = coord3d(4, 6, 0)
	local open     = coord3d(7, 3, 0)

	raise_hex_tile(pl, 5, 5, 0)
	ASSERT_EQUAL(command_x(tool_build_tunnel).work(pl, ne_mouth, tunnel.get_name()), null)
	ASSERT_TRUE(tile_x(ne_mouth.x, ne_mouth.y, ne_mouth.z).find_object(mo_tunnel) != null)
	ASSERT_TRUE(tile_x(sw_mouth.x, sw_mouth.y, sw_mouth.z).find_object(mo_tunnel) != null)
	ASSERT_EQUAL(command_x.build_way(pl, coord3d(8, 2, 0), ne_mouth, rail, true), null)
	ASSERT_EQUAL(command_x.build_way(pl, sw_mouth, coord3d(2, 8, 0), rail, true), null)

	foreach (p in [ne_mouth, sw_mouth, open]) {
		ASSERT_EQUAL(tile_x(p.x, p.y, p.z).get_way_dirs(wt_rail), dir.northeast_southwest)
	}

	// the clicks are both ways open, SW blocked, NE blocked
	local on_open = []
	local at_ne = []
	local at_sw = []
	foreach (a in [[open, on_open], [ne_mouth, at_ne], [sw_mouth, at_sw]]) {
		for (local i = 0; i < 3; i++) {
			ASSERT_EQUAL(command_x.build_sign_at(pl, a[0], signal), null)
			a[1].append(sign_images_at(a[0]))
		}
	}

	// on open ground NE shows as N before the vehicles and SW as S behind them
	ASSERT_TRUE(on_open[0] != "-1/-1")
	ASSERT_TRUE(on_open[1] != on_open[2])

	// the NE mouth shows NE, the SW mouth SW, and with that direction blocked
	// the signal shows nothing
	ASSERT_EQUAL(at_ne[0], on_open[1])
	ASSERT_EQUAL(at_ne[1], on_open[1])
	ASSERT_EQUAL(at_ne[2], "-1/-1")

	ASSERT_EQUAL(at_sw[0], on_open[2])
	ASSERT_EQUAL(at_sw[1], "-1/-1")
	ASSERT_EQUAL(at_sw[2], on_open[2])

	foreach (p in [ne_mouth, sw_mouth, open]) {
		ASSERT_EQUAL(command_x(tool_remover).work(pl, p), null)
	}
	// the rail up to the mouths, then the tunnel
	ASSERT_EQUAL(wayremover.work(pl, coord3d(8, 2, 0), ne_mouth, "" + wt_rail), null)
	ASSERT_EQUAL(wayremover.work(pl, sw_mouth, coord3d(2, 8, 0), "" + wt_rail), null)
	ASSERT_EQUAL(command_x(tool_remover).work(pl, ne_mouth), null)
	lower_hex_tile(pl, 5, 5, 0)

	RESET_ALL_PLAYER_FUNDS()
}
