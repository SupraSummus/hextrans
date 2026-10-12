//
// This file is part of the Simutrans project under the Artistic License.
// (see LICENSE.txt)
//


//
// Tests for vehicle movement along the three hex axes
//

// Headings of the straight roads: a road is built from the depot at the first point
// of `path` to the stop at the last, so that is the direction of travel.  All roads
// are 11 steps long and lie well inside the 16x16 map.
vehicle_roads <- [
	{ name = "S",  path = [coord3d(4, 2, 0),  coord3d(4, 13, 0)] },
	{ name = "N",  path = [coord3d(4, 13, 0), coord3d(4, 2, 0)] },
	{ name = "SE", path = [coord3d(2, 4, 0),  coord3d(13, 4, 0)] },
	{ name = "NW", path = [coord3d(13, 4, 0), coord3d(2, 4, 0)] },
	{ name = "SW", path = [coord3d(13, 2, 0), coord3d(2, 13, 0)] },
	{ name = "NE", path = [coord3d(2, 13, 0), coord3d(13, 2, 0)] },
]

// The 6 hex edges in clockwise order, as axial steps.
vehicle_edges <- [
	{ name = "SE", x =  1, y =  0 },
	{ name = "S",  x =  0, y =  1 },
	{ name = "SW", x = -1, y =  1 },
	{ name = "NW", x = -1, y =  0 },
	{ name = "N",  x =  0, y = -1 },
	{ name = "NE", x =  1, y = -1 },
]

// Bend tile of the bent roads, and the length of their legs.
vehicle_bend <- coord3d(8, 8, 0)
const vehicle_leg = 5


function vehicle_hex_dist(a, b)
{
	local dx = b.x - a.x
	local dy = b.y - a.y
	return (abs(dx) + abs(dy) + abs(dx + dy)) / 2
}


// A road that comes in along edge `i` to the bend tile and leaves along the
// edge `turn` steps clockwise from it.
function vehicle_bent_road(i, turn)
{
	local e1 = vehicle_edges[i]
	local e2 = vehicle_edges[(i + turn) % 6]
	local b = vehicle_bend
	return {
		name = e1.name + "-" + e2.name,
		path = [
			coord3d(b.x - vehicle_leg * e1.x, b.y - vehicle_leg * e1.y, 0),
			b,
			coord3d(b.x + vehicle_leg * e2.x, b.y + vehicle_leg * e2.y, 0)
		]
	}
}


// Drive a bus from the depot at one end of the road to the stop at the other end and
// return how many script steps it needed to cross each tile in between, as an array
// indexed by tile on the road (0 is the depot, which has no entry of its own).
// The first tile is left out: how long the bus needs to leave the depot varies.
// A second stop next to the depot gives the bus a schedule of two entries; it drives
// through it on the way to the first one.
function vehicle_cross_times(road)
{
	local pl = player_x(0)
	local road_desc = way_desc_x.get_available_ways(wt_road, st_flat)[0]
	local path = road.path
	local from = path[0]
	local to = path[path.len() - 1]
	local len = 0
	for (local i = 1; i < path.len(); i++) {
		len += vehicle_hex_dist(path[i - 1], path[i])
	}
	local first = vehicle_hex_dist(path[0], path[1])
	local near = coord3d(from.x + (path[1].x - from.x) / first, from.y + (path[1].y - from.y) / first, 0)

	for (local i = 1; i < path.len(); i++) {
		ASSERT_EQUAL(command_x.build_way(pl, path[i - 1], path[i], road_desc, true), null)
	}
	ASSERT_EQUAL(command_x(tool_build_station).work(pl, to, "BusStop"), null)
	ASSERT_EQUAL(command_x(tool_build_station).work(pl, near, "BusStop"), null)
	ASSERT_EQUAL(command_x.build_depot(pl, from, building_desc_x("CarDepot")), null)

	local depot = depot_x(from.x, from.y, from.z)
	depot.append_vehicle(pl, convoy_x(0), vehicle_desc_x("Buessig"))
	local cnv = depot.get_convoy_list()[0]
	cnv.change_schedule(pl, create_simple_schedule(wt_road, [ to, near ]))
	depot.start_all_convoys(pl)

	// script step at which the bus was first seen on each tile
	local seen = [0]
	local last_x = from.x
	local last_y = from.y
	for (local step = 1; step < 3000; step++) {
		sleep()

		local pos = cnv.get_pos()
		if (pos.x != last_x  ||  pos.y != last_y) {
			local d = abs(pos.x - last_x) + abs(pos.y - last_y) + abs(pos.x - last_x + pos.y - last_y)
			if (d != 2) {
				throw road.name + " road: bus seen on non-adjacent tiles (" + last_x + "," + last_y + ") and (" + pos.x + "," + pos.y + ")"
			}
			seen.append(step)
			last_x = pos.x
			last_y = pos.y
		}
		if (pos.x == to.x  &&  pos.y == to.y) {
			break
		}
	}

	// clean up
	cnv.destroy(pl)
	sleep()
	sleep() // make sure the convoy is destroyed
	ASSERT_EQUAL(command_x(tool_remover).work(pl, to), null)
	ASSERT_EQUAL(command_x(tool_remover).work(pl, near), null)
	ASSERT_EQUAL(command_x(tool_remover).work(pl, from), null)
	for (local i = 1; i < path.len(); i++) {
		ASSERT_EQUAL(command_x(tool_remove_way).work(pl, path[i - 1], path[i], "" + wt_road), null)
	}
	RESET_ALL_PLAYER_FUNDS()

	if (seen.len() != len + 1) {
		throw road.name + " road: bus reached " + (seen.len() - 1) + " of " + len + " tiles"
	}

	local times = []
	for (local i = 1; i + 1 < seen.len(); i++) {
		times.append(seen[i + 1] - seen[i])
	}
	return times
}


function vehicle_times_str(times)
{
	local s = ""
	foreach (t in times) {
		s += (s == "" ? "" : " ") + t
	}
	return s
}


// All 6 neighbours of a tile are the same distance away, so a bus needs as long to
// cross a tile whichever of the 6 edges it leaves by: a road along the NE-SW axis
// takes as long as one along N-S or NW-SE.  The times are compared tile by tile; at
// the same speed they come out identical, the margin only covers a different
// phase of the sync steps.
function test_vehicle_tile_length_all_hex_directions()
{
	local ref = vehicle_cross_times(vehicle_roads[0])

	for (local r = 1; r < vehicle_roads.len(); r++) {
		local road = vehicle_roads[r]
		local times = vehicle_cross_times(road)

		for (local i = 0; i < ref.len(); i++) {
			if (abs(times[i] - ref[i]) > 3) {
				throw road.name + " road: script steps per tile are [" + vehicle_times_str(times) + "] but [" + vehicle_times_str(ref) + "] on the " + vehicle_roads[0].name + " road"
			}
		}
	}
}


// A bus crosses a tile on the chord from the middle of the edge it enters by to the
// middle of the edge it leaves by.  Turning by 60 degrees the two edges are 120
// degrees apart and the chord is cos 30 = 0.87 of a straight crossing; turning by
// 120 degrees they are 60 degrees apart and it is half of one.  Each bent road is
// compared with the straight S road tile by tile: up to the bend tile the bus
// drives exactly as there.
function test_vehicle_tile_length_bends()
{
	local ref = vehicle_cross_times(vehicle_roads[0])
	// the bend is tile `vehicle_leg` from the depot, times[] starts at tile 1
	local k = vehicle_leg - 1

	foreach (turn in [1, 2]) {
		for (local i = 0; i < 6; i++) {
			local road = vehicle_bent_road(i, turn)
			local times = vehicle_cross_times(road)
			local expect = turn == 1 ? (ref[k] * 222 + 128) / 256 : (ref[k] + 1) / 2

			if (abs(times[k] - expect) > 3  ||  abs(times[k - 1] - ref[k - 1]) > 3) {
				throw road.name + " road: script steps per tile are [" + vehicle_times_str(times) + "], expected " + expect + " on the bend tile " + (k + 1) + " and [" + vehicle_times_str(ref) + "] on the " + vehicle_roads[0].name + " road"
			}
		}
	}
}


// Two buses drive towards each other over a 60 degree bend, each from its depot at
// one end to a stop next to the other depot.  They meet on the bend tile and must
// pass each other there, on their own lanes, like on a straight road.
function test_vehicle_buses_meet_on_bend()
{
	local pl = player_x(0)
	local road_desc = way_desc_x.get_available_ways(wt_road, st_flat)[0]

	for (local i = 0; i < 6; i++) {
		local road = vehicle_bent_road(i, 1)
		local a = road.path[0]
		local b = road.path[1]
		local c = road.path[2]
		local e1 = vehicle_edges[i]
		local e2 = vehicle_edges[(i + 1) % 6]
		local stop_a = coord3d(a.x + e1.x, a.y + e1.y, 0)
		local stop_c = coord3d(c.x - e2.x, c.y - e2.y, 0)

		ASSERT_EQUAL(command_x.build_way(pl, a, b, road_desc, true), null)
		ASSERT_EQUAL(command_x.build_way(pl, b, c, road_desc, true), null)
		ASSERT_EQUAL(command_x(tool_build_station).work(pl, stop_a, "BusStop"), null)
		ASSERT_EQUAL(command_x(tool_build_station).work(pl, stop_c, "BusStop"), null)
		ASSERT_EQUAL(command_x.build_depot(pl, a, building_desc_x("CarDepot")), null)
		ASSERT_EQUAL(command_x.build_depot(pl, c, building_desc_x("CarDepot")), null)

		local depot_a = depot_x(a.x, a.y, a.z)
		local depot_c = depot_x(c.x, c.y, c.z)
		depot_a.append_vehicle(pl, convoy_x(0), vehicle_desc_x("Buessig"))
		depot_c.append_vehicle(pl, convoy_x(0), vehicle_desc_x("Buessig"))
		local cnv_a = depot_a.get_convoy_list()[0]
		local cnv_c = depot_c.get_convoy_list()[0]
		cnv_a.change_schedule(pl, create_simple_schedule(wt_road, [ stop_c, stop_a ]))
		cnv_c.change_schedule(pl, create_simple_schedule(wt_road, [ stop_a, stop_c ]))
		depot_a.start_all_convoys(pl)
		depot_c.start_all_convoys(pl)

		local arrived_a = false
		local arrived_c = false
		for (local step = 1; step < 3000  &&  !(arrived_a  &&  arrived_c); step++) {
			sleep()
			local pa = cnv_a.get_pos()
			local pc = cnv_c.get_pos()
			arrived_a = arrived_a  ||  (pa.x == stop_c.x  &&  pa.y == stop_c.y)
			arrived_c = arrived_c  ||  (pc.x == stop_a.x  &&  pc.y == stop_a.y)
		}
		local pa = cnv_a.get_pos()
		local pc = cnv_c.get_pos()

		// clean up
		cnv_a.destroy(pl)
		cnv_c.destroy(pl)
		sleep()
		sleep() // make sure the convoys are destroyed
		ASSERT_EQUAL(command_x(tool_remover).work(pl, stop_a), null)
		ASSERT_EQUAL(command_x(tool_remover).work(pl, stop_c), null)
		ASSERT_EQUAL(command_x(tool_remover).work(pl, a), null)
		ASSERT_EQUAL(command_x(tool_remover).work(pl, c), null)
		ASSERT_EQUAL(command_x(tool_remove_way).work(pl, a, b, "" + wt_road), null)
		ASSERT_EQUAL(command_x(tool_remove_way).work(pl, b, c, "" + wt_road), null)
		RESET_ALL_PLAYER_FUNDS()

		if (!arrived_a  ||  !arrived_c) {
			throw road.name + " road: the buses got stuck at (" + pa.x + "," + pa.y + ") and (" + pc.x + "," + pc.y + ")"
		}
	}
}
