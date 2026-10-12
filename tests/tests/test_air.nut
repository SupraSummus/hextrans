//
// This file is part of the Simutrans project under the Artistic License.
// (see LICENSE.txt)
//


//
// Tests for aircraft: take-off, flight and landing on runways of all three hex axes.
//
// Every flight starts in the hangar of a small airport with the schedule
// [waypoint, stop]: the plane has to take off, fly to the waypoint (a free tile in
// the air), fly back, land and taxi to the air stop.  An airport is a runway on one
// hex axis (runway ends must be 5 tiles inside the map border), a taxiway crossing
// the middle of the runway, the stop at one end of the taxiway and the hangar at
// the other.  The layouts and waypoints assume the default wind from the
// north-west: planes then take off to the N, NW and SW, and the hold pattern in
// front of the landing end still fits on the 16x16 map.
//
// The plane is followed once per script step and has to be seen on every tile it
// flies over, so these tests rely on the fixed time steps of fast-forward.
//

air_test_airports <- [
	{ name = "N-S",   rw_a = coord3d(5, 5, 0), rw_b = coord3d(5, 8, 0), stop = coord3d(2, 6, 0), depot = coord3d(8, 6, 0) },
	{ name = "SE-NW", rw_a = coord3d(5, 7, 0), rw_b = coord3d(8, 7, 0), stop = coord3d(6, 4, 0), depot = coord3d(6, 10, 0) },
	{ name = "NE-SW", rw_a = coord3d(8, 5, 0), rw_b = coord3d(5, 8, 0), stop = coord3d(4, 6, 0), depot = coord3d(10, 6, 0) },
]

// The 6 hex neighbour steps clockwise from SE: the order of koord::neighbours and of the ribi bits.
air_neighbours <- [ [1, 0], [0, 1], [-1, 1], [-1, 0], [0, -1], [1, -1] ]
air_neighbour_names <- [ "SE", "S", "SW", "NW", "N", "NE" ]

// Waypoint the plane flies to unless a test says otherwise: far to the north-east of all airports.
air_default_waypoint <- coord3d(14, 1, 0)

// Finished flights by airport and waypoint, so that the tests can share them.
air_flights <- {}


function air_hexdist(ax, ay, bx, by)
{
	local dx = ax - bx
	local dy = ay - by
	return (abs(dx) + abs(dy) + abs(dx + dy)) / 2
}


// Offset on the (flat-top) map plane of an axial displacement, in hex circumradii.
function air_phys(dq, dr)
{
	return [1.5 * dq, sqrt(3.0) * (dr + 0.5 * dq)]
}


function air_build(ap)
{
	local pl = player_x(0)
	local runway = way_desc_x.get_available_ways(wt_air, st_runway)[0]
	local taxiway = way_desc_x.get_available_ways(wt_air, st_flat)[0]

	ASSERT_EQUAL(command_x.build_way(pl, ap.rw_a, ap.rw_b, runway, true), null)
	ASSERT_EQUAL(command_x.build_way(pl, ap.stop, ap.depot, taxiway, true), null)
	ASSERT_EQUAL(command_x.build_station(pl, ap.stop, building_desc_x("AirStop")), null)
	ASSERT_EQUAL(command_x.build_depot(pl, ap.depot, get_depot_by_wt(wt_air)), null)
}


function air_remove(ap)
{
	local pl = player_x(0)

	ASSERT_EQUAL(command_x(tool_remover).work(pl, ap.stop), null)
	ASSERT_EQUAL(command_x(tool_remover).work(pl, ap.depot), null)
	ASSERT_EQUAL(command_x(tool_remove_way).work(pl, ap.stop, ap.depot, "" + wt_air), null)
	ASSERT_EQUAL(command_x(tool_remove_way).work(pl, ap.rw_a, ap.rw_b, "" + wt_air), null)
	RESET_ALL_PLAYER_FUNDS()
}


// Fly a plane from the hangar of `ap` to the waypoint `wp` and back to its stop, on a
// freshly built airport that is removed again.  The plane is followed once per script
// step; the result lists every tile it was seen on, as [q, r, tiles traveled so far].
// With `full` false the flight is abandoned shortly after passing the waypoint; full
// flights are kept in air_flights for the other tests.
// Returns { arrived = plane reached the stop again, trace = [[q, r, hops], ...] }.
function air_fly(ap, wp, full)
{
	local key = ap.name + " " + wp.x + "," + wp.y
	if (key in air_flights) {
		return air_flights[key]
	}

	local pl = player_x(0)
	air_build(ap)

	local depot = depot_x(ap.depot.x, ap.depot.y, ap.depot.z)
	depot.append_vehicle(pl, convoy_x(0), vehicle_desc_x("DC-3"))
	local cnv = depot.get_convoy_list()[0]
	cnv.change_schedule(pl, create_simple_schedule(wt_air, [ wp, ap.stop ]))
	depot.start_all_convoys(pl)

	local trace = []
	local arrived = false
	local near_step = -1
	local last_x = -1
	local last_y = -1

	for (local step = 0; step < 4000; step++) {
		sleep()

		local pos = cnv.get_pos()
		if (pos.x >= 0  &&  (pos.x != last_x  ||  pos.y != last_y)) {
			trace.append([pos.x, pos.y, cnv.get_distance_traveled_total()])
			last_x = pos.x
			last_y = pos.y
		}

		if (near_step < 0  &&  air_hexdist(pos.x, pos.y, wp.x, wp.y) <= 1) {
			near_step = step
		}
		if (!full  &&  near_step >= 0  &&  step >= near_step + 12) {
			break
		}
		if (pos.x == ap.stop.x  &&  pos.y == ap.stop.y) {
			arrived = true
			break
		}
	}

	// clean up
	cnv.destroy(pl)
	sleep()
	sleep() // make sure the convoy is destroyed
	air_remove(ap)

	// the checks need to see every tile the plane flies over, which the fixed time steps
	// of fast-forward give; a coarser clock lets the plane skip tiles between two steps
	for (local i = 0; i + 1 < trace.len(); i++) {
		if (air_hexdist(trace[i][0], trace[i][1], trace[i + 1][0], trace[i + 1][1]) != 1) {
			throw ap.name + " runway: plane seen on non-adjacent tiles " + air_trace_str(trace, i, 2) + " (script steps too coarse, tests are meant to run with -fast)"
		}
	}

	local flight = { arrived = arrived, trace = trace }
	if (full) {
		air_flights[key] <- flight
	}
	return flight
}


// Trace entries, as text for error messages.
function air_trace_str(trace, from, count)
{
	local s = ""
	for (local i = from; i < from + count  &&  i < trace.len(); i++) {
		s += "(" + trace[i][0] + "," + trace[i][1] + ")"
	}
	return s
}


// The take-off run of a flight: the plane leaves its runway in the direction of the
// runway axis and flies 3 tiles beyond the end.  Returns { heading = index into
// air_neighbours, end = the runway end it leaves over, last = index in the trace of
// the last tile of that straight run }.
function air_takeoff_run(flight, ap)
{
	local trace = flight.trace
	local len = air_hexdist(ap.rw_a.x, ap.rw_a.y, ap.rw_b.x, ap.rw_b.y)
	local ux = (ap.rw_b.x - ap.rw_a.x) / len
	local uy = (ap.rw_b.y - ap.rw_a.y) / len

	for (local i = 0; i < trace.len(); i++) {
		local end = null
		local dx = ux
		local dy = uy
		if (trace[i][0] == ap.rw_a.x - ux  &&  trace[i][1] == ap.rw_a.y - uy) {
			end = ap.rw_a
			dx = -ux
			dy = -uy
		}
		else if (trace[i][0] == ap.rw_b.x + ux  &&  trace[i][1] == ap.rw_b.y + uy) {
			end = ap.rw_b
		}
		else {
			continue
		}

		// first tile beyond the runway end: the run goes on for 2 more tiles
		for (local k = 1; k <= 2; k++) {
			local t = i + k
			if (t >= trace.len()  ||  trace[t][0] != end.x + (k + 1) * dx  ||  trace[t][1] != end.y + (k + 1) * dy) {
				throw ap.name + " runway: take-off run is not 3 tiles beyond the runway end: " + air_trace_str(trace, i, 3)
			}
		}
		for (local h = 0; h < 6; h++) {
			if (air_neighbours[h][0] == dx  &&  air_neighbours[h][1] == dy) {
				return { heading = h, end = end, last = i + 2 }
			}
		}
	}
	throw ap.name + " runway: plane never took off"
}


// Where the plane, having flown on in direction `heading` (index into air_neighbours)
// to the tile `gr`, has to turn on its way to `wp`: -1 if it can fly on, else the
// neighbour index of the sidestep.  Worked out on the map plane: flying on is only
// worth it if it brings the plane closer to the waypoint; otherwise the plane
// sidesteps one hex edge to the side of its heading that the waypoint is on, so that
// it does not have to turn back at once.
function air_expected_sidestep(heading, gr, wp)
{
	local h = air_phys(air_neighbours[heading][0], air_neighbours[heading][1])
	local t = air_phys(wp.x - gr[0], wp.y - gr[1])

	// a step has length sqrt(3): stepping ahead gets closer iff the waypoint is
	// more than half a step in front of the plane
	if (h[0] * t[0] + h[1] * t[1] > 1.5 + 1e-9) {
		return -1
	}
	// cross product > 0: waypoint is clockwise of the heading, i.e. to the right
	local cross = h[0] * t[1] - h[1] * t[0]
	return cross > 1e-9 ? (heading + 1) % 6 : (heading + 5) % 6
}


function air_check_sidestep(flight, ap, wp)
{
	local trace = flight.trace
	local run = air_takeoff_run(flight, ap)
	local heading = run.heading
	local i = run.last
	local gr = [trace[i][0], trace[i][1]]
	local what = ap.name + " runway, heading " + air_neighbour_names[heading] + ", waypoint (" + wp.x + "," + wp.y + "): "

	local side = air_expected_sidestep(heading, gr, wp)
	if (side < 0) {
		// the waypoint is ahead (the tests put it right on the heading): no sidestep, so
		// no double step to either side of the heading
		if (i + 2 >= trace.len()) {
			throw what + "flight ends after the take-off run"
		}
		foreach (turn in [ (heading + 1) % 6, (heading + 5) % 6 ]) {
			local tx = air_neighbours[turn][0]
			local ty = air_neighbours[turn][1]
			if (trace[i + 1][0] == gr[0] + tx  &&  trace[i + 1][1] == gr[1] + ty  &&  trace[i + 2][0] == gr[0] + 2 * tx  &&  trace[i + 2][1] == gr[1] + 2 * ty) {
				throw what + "plane should fly straight on from (" + gr[0] + "," + gr[1] + ") but went " + air_trace_str(trace, i, 3)
			}
		}
		return
	}

	local sx = air_neighbours[side][0]
	local sy = air_neighbours[side][1]
	if (i + 2 >= trace.len()  ||  trace[i + 1][0] != gr[0] + sx  ||  trace[i + 1][1] != gr[1] + sy  ||
	    trace[i + 2][0] != gr[0] + 2 * sx  ||  trace[i + 2][1] != gr[1] + 2 * sy) {
		throw what + "plane should sidestep " + air_neighbour_names[side] + " twice from (" + gr[0] + "," + gr[1] + ") but went " + air_trace_str(trace, i, 4)
	}
}


// On the final approach to the waypoint every tile the plane flies has to bring it one
// hex step closer: the straight leg is a shortest path.  Starting from the first tile
// within 3 tiles, up to the waypoint or whatever the plane does after passing it.
function air_check_straight_leg(flight, ap, wp)
{
	local trace = flight.trace
	local i = 0
	while (i < trace.len()  &&  air_hexdist(trace[i][0], trace[i][1], wp.x, wp.y) > 3) {
		i++
	}
	if (i >= trace.len()) {
		throw ap.name + " runway: plane never came within 3 tiles of the waypoint (" + wp.x + "," + wp.y + ")"
	}

	local start = i
	for (; i + 1 < trace.len(); i++) {
		local d0 = air_hexdist(trace[i][0], trace[i][1], wp.x, wp.y)
		local d1 = air_hexdist(trace[i + 1][0], trace[i + 1][1], wp.x, wp.y)
		if (d1 > d0) {
			break // passed the waypoint
		}
		local hops = trace[i + 1][2] - trace[i][2]
		if (d1 != d0 - hops) {
			throw ap.name + " runway: flew " + hops + " tile(s) but came only " + (d0 - d1) + " hex step(s) closer to the waypoint, on " + air_trace_str(trace, start, i - start + 2)
		}
	}
}


// The plane lands heading the way it took off: it comes in along the axis of the runway
// to the end it took off from, touches down there and rolls on along the runway.  The
// last visit of that end is the landing.  (The turning points at the other runway end
// and at the start of the hold pattern are passed without the plane being seen there.)
function air_check_landing_roll(flight, ap)
{
	local trace = flight.trace
	local run = air_takeoff_run(flight, ap)
	local step = air_neighbours[run.heading]
	local len = air_hexdist(ap.rw_a.x, ap.rw_a.y, ap.rw_b.x, ap.rw_b.y)
	local ex = run.end.x - len * step[0]
	local ey = run.end.y - len * step[1]

	local k = -1
	for (local i = 0; i < trace.len(); i++) {
		if (trace[i][0] == ex  &&  trace[i][1] == ey) {
			k = i
		}
	}

	// 2 tiles of approach and 2 tiles of roll, all on the runway axis
	for (local j = -2; j <= 2; j++) {
		local t = k + j
		if (k < 0  ||  t < 0  ||  t >= trace.len()  ||  trace[t][0] != ex + j * step[0]  ||  trace[t][1] != ey + j * step[1]) {
			throw ap.name + " runway: landing does not follow the runway axis, touchdown end (" + ex + "," + ey + ") seen as " + air_trace_str(trace, max(0, k - 2), 5)
		}
	}
}


function test_air_takeoff_and_landing_all_runway_axes()
{
	foreach (ap in air_test_airports) {
		local flight = air_fly(ap, air_default_waypoint, true)

		if (flight.trace.len() < 2) {
			throw ap.name + " runway: the plane could not leave the hangar"
		}
		if (!flight.arrived) {
			throw ap.name + " runway: the plane did not come back to the stop, last seen on " + air_trace_str(flight.trace, flight.trace.len() - 3, 3)
		}
		air_check_landing_roll(flight, ap)
	}
}


function test_air_straight_leg_is_shortest_path()
{
	foreach (ap in air_test_airports) {
		air_check_straight_leg(air_fly(ap, air_default_waypoint, true), ap, air_default_waypoint)
	}
}


function test_air_takeoff_sidestep_towards_target()
{
	// per runway axis (planes take off to the N, NW and SW): waypoints behind the plane to its
	// right and to its left, and one straight ahead of its take-off run
	local waypoints = [
		[ air_test_airports[0], [ coord3d(14, 1, 0), coord3d(1, 14, 0), coord3d(5, 0, 0) ] ],
		[ air_test_airports[1], [ coord3d(14, 1, 0), coord3d(11, 12, 0), coord3d(0, 7, 0) ] ],
		[ air_test_airports[2], [ coord3d(14, 1, 0), coord3d(0, 2, 0), coord3d(0, 13, 0) ] ],
	]

	foreach (entry in waypoints) {
		local ap = entry[0]
		foreach (wp in entry[1]) {
			air_check_sidestep(air_fly(ap, wp, wp.x == air_default_waypoint.x  &&  wp.y == air_default_waypoint.y), ap, wp)
		}
	}
}
