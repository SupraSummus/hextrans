//
// This file is part of the Simutrans project under the Artistic License.
// (see LICENSE.txt)
//


//
// Tests for map rotation (karte_t::rotate90).
//
// Scripts see the world as it was when they started: coordinates,
// directions, slopes and building layouts come back through
// script_api::coordinate_transform_t.  So a snapshot of the world taken
// through the script API must not change when the map turns.
//


// moving or transient things whose state is not part of the snapshot
local rotate_skip_types = [ mo_pointer, mo_cloud ]


// coord's _tostring prints the world position (see coord_to_string); the
// snapshot wants the script's
function rotate_str(c)
{
	return "z" in c ? c.x + "," + c.y + "," + c.z : c.x + "," + c.y
}


function rotate_snapshot_tile(t)
{
	local s = "z=" + t.z + " slope=" + t.get_slope()
	if (t.is_bridge()) s += " bridge"
	if (t.is_tunnel()) s += " tunnel"
	if (t.is_water()) s += " water"
	local text = t.get_text()
	if (text != null) s += " text=" + text
	foreach (wt in [wt_road, wt_rail, wt_water, wt_monorail, wt_maglev, wt_tram, wt_narrowgauge, wt_air]) {
		local w = t.get_way(wt)
		if (w != null) {
			s += " way" + wt + "=" + w.get_desc().get_name() + "/" + w.get_dirs() + "/" + w.get_dirs_masked()
		}
	}
	local objs = []
	foreach (o in t.get_objects()) {
		local typ = o.get_type()
		if (rotate_skip_types.find(typ) != null) {
			continue
		}
		local owner = o.get_owner()
		local os = "obj" + typ + "=" + o.get_name() + "@" + (owner ? owner.nr : -1)
		if (typ == mo_building) {
			os += "/layout" + o.get_layout()
		}
		objs.append(os)
	}
	objs.sort()
	foreach (os in objs) {
		s += " " + os
	}
	return s
}


// Everything a script can see of the map, one line per square.
function rotate_snapshot()
{
	local out = []
	local size = world.get_size()
	out.append("size=" + size.x + "," + size.y)
	for (local y = 0; y < size.y; y++) {
		for (local x = 0; x < size.x; x++) {
			local sq = square_x(x, y)
			local g = sq.get_ground_tile()
			local line = "(" + x + "," + y + ") climate=" + sq.get_climate()
			for (local z = g.z - 4; z <= g.z + 4; z++) {
				local t = sq.get_tile_at_height(z)
				if (t != null) {
					line += " [" + rotate_snapshot_tile(t) + "]"
				}
			}
			local h = sq.get_halt()
			if (h != null) {
				line += " halt=" + h.get_name()
			}
			out.append(line)
		}
	}
	foreach (city in city_list_x()) {
		local nw = city.get_pos_nw()
		local se = city.get_pos_se()
		out.append("city " + city.get_name() + " at " + rotate_str(city.get_pos())
			+ " area " + min(nw.x, se.x) + "," + min(nw.y, se.y) + "-" + max(nw.x, se.x) + "," + max(nw.y, se.y))
	}
	foreach (fab in factory_list_x()) {
		local tiles = fab.get_tile_list().map(@(t) rotate_str(t))
		tiles.sort()
		local line = "factory " + fab.get_name() + " at " + rotate_str(fab)
		foreach (t in tiles) {
			line += " " + t
		}
		out.append(line)
	}
	foreach (cnv in world.get_convoy_list()) {
		local line = "convoy " + cnv.get_name() + " at " + rotate_str(cnv.get_pos()) + " depot " + rotate_str(cnv.get_home_depot())
		foreach (e in cnv.get_schedule().entries) {
			line += " stop " + e.x + "," + e.y + "," + e.z
		}
		out.append(line)
	}
	return out
}


// removes every way of waytype wt on the ground, one tile at a time
function rotate_remove_ways(pl, wt)
{
	local size = world.get_size()
	for (local y = 0; y < size.y; y++) {
		for (local x = 0; x < size.x; x++) {
			local tile = square_x(x, y).get_ground_tile()
			if (tile.get_way(wt) != null) {
				local p = coord3d(x, y, tile.z)
				ASSERT_EQUAL(command_x(tool_remove_way).work(pl, p, p, "" + wt), null)
			}
		}
	}
}


// removes the city: the townhall takes its buildings along, then all roads
function rotate_remove_city(pl)
{
	foreach (city in city_list_x()) {
		local cp = city.get_pos()
		ASSERT_EQUAL(command_x(tool_remover).work(pl, coord3d(cp.x, cp.y, square_x(cp.x, cp.y).get_ground_tile().z)), null)
	}
	rotate_remove_ways(pl, wt_road)
}


function ASSERT_SNAPSHOT_EQUAL(act, exp)
{
	ASSERT_EQUAL(act.len(), exp.len())
	foreach (i, line in exp) {
		ASSERT_EQUAL(act[i], line)
	}
}


function test_world_rotate_roundtrip()
{
	local pl = player_x(0)
	local public_pl = player_x(1)
	local road = way_desc_x.get_available_ways(wt_road, st_flat)[0]
	local rail = way_desc_x.get_available_ways(wt_rail, st_flat)[0]
	local power = way_desc_x.get_available_ways(wt_power, st_flat)[0]
	local oneway = sign_desc_x.get_available_signs(wt_road).filter(@(idx, sign) sign.is_one_way())[0]
	local signal = sign_desc_x.get_available_signs(wt_rail).filter(@(idx, sign) sign.is_signal())[0]
	local bridge_desc = bridge_desc_x.get_available_bridges(wt_road)[0]
	local tunnel_desc = tunnel_desc_x.get_available_tunnels(wt_road)[0]

	// a road tunnel through a ridge, S axis: portals at (1,2) and (1,7)
	TUNNEL_RIDGE(pl, 1, 3, 6, true)
	ASSERT_EQUAL(command_x(tool_build_tunnel).work(pl, coord3d(1, 2, 0), tunnel_desc.get_name()), null)
	// a road bridge between two ramps, S axis
	ASSERT_EQUAL(command_x.set_slope(pl, coord3d(3, 2, 0), slope.south_narrow), null)
	ASSERT_EQUAL(command_x.set_slope(pl, coord3d(3, 5, 0), slope.north_narrow), null)
	ASSERT_EQUAL(command_x.build_bridge_at(pl, coord3d(3, 2, 0), bridge_desc), null)
	// a bus line on the S axis: two stops and a depot
	ASSERT_EQUAL(command_x.build_way(pl, coord3d(5, 2, 0), coord3d(5, 8, 0), road, true), null)
	ASSERT_EQUAL(command_x(tool_build_station).work(pl, coord3d(5, 2, 0), "BusStop"), null)
	ASSERT_EQUAL(command_x(tool_build_station).work(pl, coord3d(5, 7, 0), "BusStop"), null)
	ASSERT_EQUAL(command_x.build_depot(pl, coord3d(5, 8, 0), building_desc_x("CarDepot")), null)
	// roads on the SE and NE axes, a one-way sign on the first
	ASSERT_EQUAL(command_x.build_way(pl, coord3d(2, 10, 0), coord3d(7, 10, 0), road, true), null)
	ASSERT_EQUAL(command_x.build_sign_at(pl, coord3d(4, 10, 0), oneway), null)
	ASSERT_EQUAL(command_x.build_way(pl, coord3d(9, 8, 0), coord3d(13, 4, 0), road, true), null)
	// rails on the S and SE axes, a signal on the first
	ASSERT_EQUAL(command_x.build_way(pl, coord3d(14, 1, 0), coord3d(14, 8, 0), rail, true), null)
	ASSERT_EQUAL(command_x.build_sign_at(pl, coord3d(14, 4, 0), signal), null)
	ASSERT_EQUAL(command_x.build_way(pl, coord3d(2, 13, 0), coord3d(7, 13, 0), rail, true), null)
	// a powerline
	ASSERT_EQUAL(command_x.build_way(pl, coord3d(9, 10, 0), coord3d(9, 14, 0), power, true), null)
	// a raised tile with sloped neighbours
	raise_hex_tile(pl, 12, 8, 0)
	// a 3x3 factory in a turned layout, a city and a label
	ASSERT_EQUAL(build_factory(pl, coord3d(8, 2, 0), 0, 1, 1024, "Aufwindkraftwerk"), null)
	ASSERT_EQUAL(command_x(tool_add_city).work(public_pl, coord3d(12, 13, 0)), null)
	label_x.create(coord(0, 14), pl, "rotate")

	// a bus shuttling between the stops
	local depot = depot_x(5, 8, 0)
	depot.append_vehicle(pl, convoy_x(0), vehicle_desc_x("Buessig"))
	local cnv = depot.get_convoy_list()[0]
	cnv.change_schedule(pl, schedule_x(wt_road, [ schedule_entry_x(coord3d(5, 7, 0), 0, 0), schedule_entry_x(coord3d(5, 2, 0), 0, 0) ]))
	depot.start_all_convoys(pl)
	while (cnv.get_traveled_distance()[0] == 0  ||  cnv.is_in_depot()) {
		sleep()
	}

	// a half turn and back leave the snapshot as it is
	local before = rotate_snapshot()
	ASSERT_TRUE(debug.rotate_map())
	ASSERT_SNAPSHOT_EQUAL(rotate_snapshot(), before)
	ASSERT_TRUE(debug.rotate_map())
	ASSERT_SNAPSHOT_EQUAL(rotate_snapshot(), before)

	// the game keeps running in the turned world: the bus serves both stops
	ASSERT_TRUE(debug.rotate_map())
	local far_halt = halt_x.get_halt(coord3d(5, 2, 0), pl)
	local near_halt = halt_x.get_halt(coord3d(5, 7, 0), pl)
	local far_visits = far_halt.get_convoys()[0]
	local near_visits = near_halt.get_convoys()[0]
	while (far_halt.get_convoys()[0] == far_visits  ||  near_halt.get_convoys()[0] == near_visits) {
		sleep()
	}
	local turned = rotate_snapshot()
	ASSERT_TRUE(debug.rotate_map())
	ASSERT_SNAPSHOT_EQUAL(rotate_snapshot(), turned)

	// clean up
	cnv.destroy(pl)
	while (cnv.is_valid()) {
		sleep()
	}
	ASSERT_EQUAL(command_x(tool_remover).work(pl, coord3d(5, 8, 0)), null)
	ASSERT_EQUAL(command_x(tool_remover).work(pl, coord3d(5, 2, 0)), null)
	ASSERT_EQUAL(command_x(tool_remover).work(pl, coord3d(5, 7, 0)), null)
	ASSERT_EQUAL(command_x(tool_remove_way).work(pl, coord3d(9, 10, 0), coord3d(9, 14, 0), "" + wt_power), null)
	ASSERT_EQUAL(command_x(tool_remover).work(pl, coord3d(3, 2, 0)), null)
	ASSERT_EQUAL(command_x.set_slope(pl, coord3d(3, 2, 0), slope.flat), null)
	ASSERT_EQUAL(command_x.set_slope(pl, coord3d(3, 5, 0), slope.flat), null)
	ASSERT_EQUAL(command_x(tool_remover).work(pl, coord3d(1, 2, 0)), null)
	TUNNEL_RIDGE(pl, 1, 3, 6, false)
	lower_hex_tile(pl, 12, 8, 0)
	ASSERT_EQUAL(command_x(tool_remover).work(public_pl, coord3d(8, 2, 0)), null)
	ASSERT_EQUAL(command_x(tool_remover).work(pl, coord3d(0, 14, 0)), null)
	rotate_remove_city(public_pl)
	rotate_remove_ways(public_pl, wt_rail)
	ASSERT_EQUAL(pl.get_current_maintenance(), 0)
	RESET_ALL_PLAYER_FUNDS()
}
