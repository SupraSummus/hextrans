//
// This file is part of the Simutrans project under the Artistic License.
// (see LICENSE.txt)
//


//
// Tests for vehicle movement along the three hex axes
//

// Headings of the straight roads: a road is built from the depot at `from` to the
// stop at `to`, so `from` -> `to` is the direction of travel.  All roads are 11 steps
// long and lie well inside the 16x16 map.
vehicle_roads <- [
	{ name = "S",  from = coord3d(4, 2, 0),  to = coord3d(4, 13, 0) },
	{ name = "N",  from = coord3d(4, 13, 0), to = coord3d(4, 2, 0) },
	{ name = "SE", from = coord3d(2, 4, 0),  to = coord3d(13, 4, 0) },
	{ name = "NW", from = coord3d(13, 4, 0), to = coord3d(2, 4, 0) },
	{ name = "SW", from = coord3d(13, 2, 0), to = coord3d(2, 13, 0) },
	{ name = "NE", from = coord3d(2, 13, 0), to = coord3d(13, 2, 0) },
]


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
	local from = road.from
	local to = road.to
	local len = (abs(to.x - from.x) + abs(to.y - from.y) + abs(to.x - from.x + to.y - from.y)) / 2
	local near = coord3d(from.x + (to.x - from.x) / len, from.y + (to.y - from.y) / len, 0)

	ASSERT_EQUAL(command_x.build_way(pl, from, to, road_desc, true), null)
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
	ASSERT_EQUAL(command_x(tool_remove_way).work(pl, from, to, "" + wt_road), null)
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
