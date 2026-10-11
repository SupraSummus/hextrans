//
// This file is part of the Simutrans project under the Artistic License.
// (see LICENSE.txt)
//


//
// test for halts/stops
//


function test_halt_build_rail_single_tile()
{
	local pl = player_x(0)
	local setslope = command_x.set_slope
	local stationbuilder = command_x(tool_build_station)
	local wayremover = command_x(tool_remove_way)
	local road_desc = way_desc_x.get_available_ways(wt_road, st_flat)[0] // road because it has double slopes available
	local station_desc = building_desc_x.get_available_stations(building_desc_x.station, wt_road, {} )[0]
	local bridge_desc = bridge_desc_x.get_available_bridges(wt_road)[0]

	// preconditions
	ASSERT_TRUE(road_desc != null)
	ASSERT_TRUE(station_desc != null)
	ASSERT_TRUE(bridge_desc != null)

	ASSERT_EQUAL(pl.get_current_maintenance(), 0)

	local pos = coord3d(4, 2, 0)

	{
		foreach (sl in interesting_slopes()) {
			ASSERT_EQUAL(setslope(pl, pos, sl), sl != slope.flat ? null : "")
			ASSERT_EQUAL(stationbuilder.work(pl, pos, station_desc.get_name()), "No suitable way on the ground!")

			ASSERT_EQUAL(pl.get_current_maintenance(), 0)
			ASSERT_EQUAL(tile_x(pos.x, pos.y, pos.z).find_object(mo_building), null)
		}
	}

	ASSERT_EQUAL(setslope(pl, pos, slope.flat), null)

	// cannot build on non-flat tile
	{
		foreach (sl in interesting_slopes()) {
			if (sl == slope.flat) continue
			ASSERT_EQUAL(setslope(pl, pos, sl), null)

			// `slope.to_dir` returns a direction for any sloped shape
			// (including single-corner raises and the legacy east/west
			// diagonals); `slope.is_way` adds the way-buildability gate.
			local d = slope.to_dir(sl)
			if (slope.is_way(sl) && d != dir.none) {
				RESET_ALL_PLAYER_FUNDS()

				local c = dir.to_coord(dir.backward(d))
				local adjacent = pos + coord3d(c.x, c.y, 0)
				ASSERT_EQUAL(command_x.build_way(pl, adjacent, pos, road_desc, true), null)
				local old_maintenance = pl.get_current_maintenance()

				ASSERT_EQUAL(stationbuilder.work(pl, pos, station_desc.get_name()), "No suitable way on the ground!")
				ASSERT_EQUAL(pl.get_current_maintenance(), old_maintenance)
				ASSERT_EQUAL(tile_x(pos.x, pos.y, pos.z).find_object(mo_building), null)

				ASSERT_EQUAL(wayremover.work(pl, pos, adjacent, "" + wt_road), null)
			}
		}
	}

	ASSERT_EQUAL(setslope(pl, pos, slope.flat), null)

	// build on bridge
	{
		ASSERT_EQUAL(setslope(pl, coord3d(3, 2, 0), slope.south_narrow), null)
		ASSERT_EQUAL(setslope(pl, coord3d(3, 4, 0), slope.north_narrow), null)
		ASSERT_EQUAL(command_x.build_bridge_at(pl, coord3d(3, 2, 0), bridge_desc), null)

		local old_maintenance = pl.get_current_maintenance()

		ASSERT_EQUAL(stationbuilder.work(pl, coord3d(3, 3, 1), station_desc.get_name()), null)
		ASSERT_EQUAL(command_x(tool_remover).work(pl, coord3d(3, 3, 1)), null)
		ASSERT_EQUAL(pl.get_current_maintenance(), old_maintenance)

		// note z = 0 instead of 1
		ASSERT_EQUAL(stationbuilder.work(pl, coord3d(3, 2, 0), station_desc.get_name()), null)
		ASSERT_EQUAL(command_x(tool_remover).work(pl, coord3d(3, 2, 0)), null)
		ASSERT_EQUAL(pl.get_current_maintenance(), old_maintenance)

		// note z = 0 instead of 1
		ASSERT_EQUAL(stationbuilder.work(pl, coord3d(3, 4, 0), station_desc.get_name()), null)
		ASSERT_EQUAL(command_x(tool_remover).work(pl, coord3d(3, 4, 0)), null)
		ASSERT_EQUAL(pl.get_current_maintenance(), old_maintenance)

		ASSERT_EQUAL(command_x(tool_remover).work(pl, coord3d(3, 4, 0)), null)
		ASSERT_EQUAL(setslope(pl, coord3d(3, 2, 0), slope.flat), null)
		ASSERT_EQUAL(setslope(pl, coord3d(3, 4, 0), slope.flat), null)

		ASSERT_EQUAL(pl.get_current_maintenance(), 0)
	}

	// clean up
	RESET_ALL_PLAYER_FUNDS()
}


function test_halt_build_harbour()
{
	local pl = player_x(0)
	local stationbuilder = command_x(tool_build_station)
	local station_desc = building_desc_x.get_available_stations(building_desc_x.harbour, wt_water, good_desc_x.passenger)[0] // FIXME: null instead of pax fails
	local setclimate = command_x(tool_set_climate)
	local setslope = command_x.set_slope

	// build harbour on flat land: should fail
	{
		ASSERT_EQUAL(stationbuilder.work(pl, coord3d(4, 3, 0), station_desc.get_name()), "Dock must be built on single slope!")
		ASSERT_EQUAL(tile_x(4, 3, 0).find_object(mo_building), null)
	}

	// build harbour on sloped land (not adjacent to water): should fail
	{
		ASSERT_EQUAL(setslope(pl, coord3d(4, 2, 0), slope.south_narrow), null)

		ASSERT_EQUAL(stationbuilder.work(pl, coord3d(4, 2, 0), station_desc.get_name()), "No suitable ground!")
		ASSERT_EQUAL(tile_x(4, 2, 0).find_object(mo_building), null)
	}

	// build harbour on sloped land adjacent to water: should succeed
	{
		ASSERT_EQUAL(setclimate.work(pl, coord3d(4, 3, 0), coord3d(4, 3, 0), "" + cl_water), null)
		ASSERT_EQUAL(stationbuilder.work(pl, coord3d(4, 2, 0), station_desc.get_name()), null)

		ASSERT_EQUAL(command_x(tool_remover).work(pl, coord3d(4, 3, 0)), null)

		ASSERT_EQUAL(pl.get_current_maintenance(), 0)
		ASSERT_EQUAL(tile_x(4, 2, 0).find_object(mo_building), null)

		ASSERT_EQUAL(setclimate.work(pl, coord3d(4, 3, 0), coord3d(4, 3, 0), "" + cl_mediterran), null)
	}

	// clean up
	ASSERT_EQUAL(setslope(pl, coord3d(4, 2, 0), slope.all_down_slope), null)
	RESET_ALL_PLAYER_FUNDS()
}


// The six directions a dock can face: [direction to the water, shore slope
// with that edge low, stored layout of a dock with 4 layouts].  The 4-layout
// images know S, SE, N and NW; building_desc_t::adjust_layout folds NE and SW
// onto S and SE, so those two only show up in the footprint.
function dock_faces()
{
	return [
		[dir.south,     slope.south_narrow,     0],
		[dir.southeast, slope.southeast_narrow, 1],
		[dir.north,     slope.north_narrow,     2],
		[dir.northwest, slope.northwest_narrow, 3],
		[dir.northeast, slope.northeast_narrow, 0],
		[dir.southwest, slope.southwest_narrow, 1],
	]
}


// Turn the `n` tiles in front of the shore tile `k`, along `face`, into
// water (`climate` = cl_water) or back into land (cl_mediterran).
function set_dock_water(pl, k, face, n, climate)
{
	local v = dir.to_coord(face)
	for (local i = 1; i <= n; i++) {
		local t = coord3d(k.x + v.x * i, k.y + v.y * i, 0)
		ASSERT_EQUAL(command_x(tool_set_climate).work(pl, t, t, "" + climate), null)
	}
}


// Number of buildings within 4 tiles of `k`.
function count_buildings_around(k)
{
	local n = 0
	for (local x = k.x - 4; x <= k.x + 4; x++) {
		for (local y = k.y - 4; y <= k.y + 4; y++) {
			if (square_x(x, y).get_ground_tile().find_object(mo_building) != null) n++
		}
	}
	return n
}


// The dock of `len` + 1 tiles that starts at the shore tile `k` and runs along
// `face` is the only building around `k`, and every tile of it has `layout`.
function ASSERT_DOCK_LINE(k, face, len, layout)
{
	local v = dir.to_coord(face)
	for (local i = 0; i <= len; i++) {
		local b = square_x(k.x + v.x * i, k.y + v.y * i).get_ground_tile().find_object(mo_building)
		ASSERT_TRUE(b != null)
		ASSERT_EQUAL(b.get_layout(), layout)
	}
	ASSERT_EQUAL(count_buildings_around(k), len + 1)
}


// A harbour on a shore slope of each hex direction builds along the line of
// water the slope faces.  pak64's rampShipStop (1 tile, 4 layouts),
// FreightShipStop (1 tile, 16 layouts), ShipStop (2 tiles) and LargeShipStop
// (4 tiles) cover the footprint going back to the shore tile for N and NW and
// the one-tile dock on the NE-SW axis; a longer dock cannot run NE-SW
// because the footprint has no third axis, and is refused.
function test_halt_build_harbour_all_directions()
{
	local pl = player_x(0)
	local k = coord3d(8, 8, 0)

	foreach (name in ["rampShipStop", "FreightShipStop", "ShipStop", "LargeShipStop"]) {
		local len = building_desc_x(name).get_size(0).y - 1

		foreach (f in dock_faces()) {
			local face = f[0]
			local layout = f[2]
			if (name == "FreightShipStop") {
				// 16 layouts: the same orientations with the "end" bits set
				layout = layout < 2 ? 15 - layout : 9 - layout
			}
			local across = (face == dir.northeast || face == dir.southwest)

			set_dock_water(pl, k, face, max(len, 1), cl_water)
			ASSERT_EQUAL(command_x.set_slope(pl, k, f[1]), null)

			if (len > 0 && across) {
				ASSERT_EQUAL(command_x(tool_build_station).work(pl, k, name), "A dock longer than one tile cannot run NE-SW yet.")
				ASSERT_EQUAL(count_buildings_around(k), 0)
			}
			else {
				ASSERT_EQUAL(command_x(tool_build_station).work(pl, k, name), null)
				ASSERT_DOCK_LINE(k, face, len, layout)

				ASSERT_EQUAL(command_x(tool_remover).work(pl, k), null)
				ASSERT_EQUAL(count_buildings_around(k), 0)
			}

			ASSERT_EQUAL(command_x.set_slope(pl, k, slope.flat), null)
			set_dock_water(pl, k, face, max(len, 1), cl_mediterran)
			RESET_ALL_PLAYER_FUNDS()
		}
	}
}


function test_halt_build_flat_dock_near_water()
{
	ASSERT_EQUAL(command_x(tool_set_climate).work(player_x(0), coord3d(4, 2, 0), coord3d(4, 2, 0), "" + cl_water), null)

	{
		ASSERT_EQUAL(command_x(tool_build_station).work(player_x(0), coord3d(4, 3, 0), "LakeShipStop"), null)
	}

	// clean up
	ASSERT_EQUAL(command_x(tool_set_climate).work(player_x(0), coord3d(4, 2, 0), coord3d(4, 2, 0), "" + cl_mediterran), null)
	ASSERT_EQUAL(command_x(tool_remover).work(player_x(0), coord3d(4, 3, 0)), null)
	RESET_ALL_PLAYER_FUNDS()
}


function test_halt_build_flat_dock_near_water_multiple_rotations()
{
	ASSERT_EQUAL(command_x(tool_set_climate).work(player_x(0), coord3d(4, 2, 0), coord3d(4, 2, 0), "" + cl_water), null)
	ASSERT_EQUAL(command_x(tool_set_climate).work(player_x(0), coord3d(3, 3, 0), coord3d(3, 3, 0), "" + cl_water), null)

	{
		ASSERT_EQUAL(command_x(tool_build_station).work(player_x(0), coord3d(4, 3, 0), "LakeShipStop"), "More than one possibility to build this dock found.")
	}

	// clean up
	ASSERT_EQUAL(command_x(tool_set_climate).work(player_x(0), coord3d(4, 2, 0), coord3d(4, 2, 0), "" + cl_mediterran), null)
	ASSERT_EQUAL(command_x(tool_set_climate).work(player_x(0), coord3d(3, 3, 0), coord3d(3, 3, 0), "" + cl_mediterran), null)
	RESET_ALL_PLAYER_FUNDS()
}


function test_halt_build_flat_dock_near_water_fixed_rotation_valid()
{
	ASSERT_EQUAL(command_x(tool_set_climate).work(player_x(0), coord3d(4, 2, 0), coord3d(4, 2, 0), "" + cl_water), null)

	{
		ASSERT_EQUAL(command_x(tool_build_station).work(player_x(0), coord3d(4, 3, 0), "LakeShipStop,2"), null)
	}

	// clean up
	ASSERT_EQUAL(command_x(tool_set_climate).work(player_x(0), coord3d(4, 2, 0), coord3d(4, 2, 0), "" + cl_mediterran), null)
	ASSERT_EQUAL(command_x(tool_remover).work(player_x(0), coord3d(4, 3, 0)), null)
	RESET_ALL_PLAYER_FUNDS()
}


function test_halt_build_flat_dock_near_water_fixed_rotation_invalid()
{
	ASSERT_EQUAL(command_x(tool_set_climate).work(player_x(0), coord3d(4, 2, 0), coord3d(4, 2, 0), "" + cl_water), null)

	{
		ASSERT_EQUAL(command_x(tool_build_station).work(player_x(0), coord3d(4, 3, 0), "LakeShipStop,1"), "No suitable ground!")
	}

	// clean up
	ASSERT_EQUAL(command_x(tool_set_climate).work(player_x(0), coord3d(4, 2, 0), coord3d(4, 2, 0), "" + cl_mediterran), null)
	RESET_ALL_PLAYER_FUNDS()
}


function test_halt_build_flat_dock_outside_map()
{
	{
		ASSERT_EQUAL(command_x(tool_build_station).work(player_x(0), coord3d(-1, -1, 0), "LakeShipStop"), "")
	}

	// clean up
	RESET_ALL_PLAYER_FUNDS()
}


function test_halt_build_flat_dock_near_map_border_auto_rotation()
{
	ASSERT_EQUAL(command_x(tool_set_climate).work(player_x(0), coord3d(4, 1, 0), coord3d(4, 1, 0), "" + cl_water), null)

	{
		ASSERT_EQUAL(command_x(tool_build_station).work(player_x(0), coord3d(4, 0, 0), "LakeShipStop"), null)
	}

	// clean up
	ASSERT_EQUAL(command_x(tool_set_climate).work(player_x(0), coord3d(4, 1, 0), coord3d(4, 0, 0), "" + cl_mediterran), null)
	ASSERT_EQUAL(command_x(tool_remover).work(player_x(0), coord3d(4, 0, 0)), null)
	RESET_ALL_PLAYER_FUNDS()
}


function test_halt_build_flat_dock_near_map_border_fixed_rotation()
{
	{
		ASSERT_EQUAL(command_x(tool_build_station).work(player_x(0), coord3d(4, 0, 0), "LakeShipStop,2"), "No suitable ground!")
	}

	// clean up
	RESET_ALL_PLAYER_FUNDS()
}


function test_halt_build_flat_dock_on_bridge()
{
	ASSERT_EQUAL(command_x.set_slope(player_x(0), coord3d(4, 2, 0), slope.south_narrow), null)
	ASSERT_EQUAL(command_x.set_slope(player_x(0), coord3d(4, 4, 0), slope.north_narrow), null)
	ASSERT_EQUAL(command_x(tool_build_bridge).work(player_x(0), coord3d(4, 2, 0), coord3d(4, 4, 0), "Schiffhebewerk"), null)

	{
		ASSERT_EQUAL(command_x(tool_build_station).work(player_x(0), coord3d(4, 3, 1), "LakeShipStop"), "")
	}

	// clean up
	ASSERT_EQUAL(command_x(tool_remover).work(player_x(0), coord3d(4, 2, 0)), null)
	ASSERT_EQUAL(command_x.set_slope(player_x(0), coord3d(4, 2, 0), slope.flat), null)
	ASSERT_EQUAL(command_x.set_slope(player_x(0), coord3d(4, 4, 0), slope.flat), null)
	RESET_ALL_PLAYER_FUNDS()
}


function test_halt_build_flat_dock_on_slope()
{
	ASSERT_EQUAL(command_x.set_slope(player_x(0), coord3d(4, 2, 0), slope.north_narrow), null)

	{
		ASSERT_EQUAL(command_x(tool_build_station).work(player_x(0), coord3d(4, 2, 0), "LakeShipStop"), "No suitable ground!")
	}

	// clean up
	ASSERT_EQUAL(command_x.set_slope(player_x(0), coord3d(4, 2, 0), slope.flat), null)
	RESET_ALL_PLAYER_FUNDS()
}


function test_halt_build_flat_dock_near_cliff()
{
	ASSERT_EQUAL(command_x(tool_set_climate).work(player_x(0), coord3d(4, 2, 0), coord3d(4, 2, 0), "" + cl_water), null)
	ASSERT_EQUAL(command_x.set_slope(player_x(0), coord3d(4, 3, 0), slope.all_up_slope), null)

	{
		ASSERT_EQUAL(command_x(tool_build_station).work(player_x(0), coord3d(4, 3, 1), "LakeShipStop"), "No suitable ground!")
	}

	// clean up
	ASSERT_EQUAL(command_x(tool_set_climate).work(player_x(0), coord3d(4, 2, 0), coord3d(4, 2, 0), "" + cl_mediterran), null)
	ASSERT_EQUAL(command_x.set_slope(player_x(0), coord3d(4, 3, 1), slope.all_down_slope), null)
	RESET_ALL_PLAYER_FUNDS()
}


function test_halt_build_flat_dock_in_water()
{
	ASSERT_EQUAL(command_x(tool_set_climate).work(player_x(0), coord3d(4, 2, 0), coord3d(4, 2, 0), "" + cl_water), null)

	{
		ASSERT_EQUAL(command_x(tool_build_station).work(player_x(0), coord3d(4, 2, 0), "LakeShipStop"), "No suitable ground!")
	}

	// clean up
	ASSERT_EQUAL(command_x(tool_set_climate).work(player_x(0), coord3d(4, 2, 0), coord3d(4, 2, 0), "" + cl_mediterran), null)
	RESET_ALL_PLAYER_FUNDS()
}


function test_halt_build_flat_dock_occupied()
{
	ASSERT_EQUAL(command_x(tool_add_city).work(player_x(1), coord3d(8, 8, 0), "0"), null)
	ASSERT_EQUAL(command_x(tool_build_house).work(player_x(1), coord3d(4, 2, 0), "11RES_01_23"), null)

	{
		ASSERT_EQUAL(command_x(tool_build_station).work(player_x(0), coord3d(4, 2, 0), "LakeShipStop"), "No suitable ground!")
	}

	// clean up
	ASSERT_EQUAL(command_x(tool_remover).work(player_x(1), coord3d(4, 2, 0)), null)
	ASSERT_EQUAL(command_x(tool_remover).work(player_x(1), coord3d(8, 8, 0)), null); // remove city
	clear_seed_roads(player_x(1), 8, 8); // founding road seeds on a random hex face
	RESET_ALL_PLAYER_FUNDS();
}


// A flat dock on a beach facing each hex direction: LakeShipStop is one tile,
// TEST_FLAT_DOCK_1x3 (tests/test-pak) three.  The long one builds along the
// line of water for S, SE, N and NW, N and NW with the building starting at
// the far end, and is refused for NE and SW.
function test_halt_build_flat_dock_all_directions()
{
	local pl = player_x(0)
	local k = coord3d(8, 8, 0)

	foreach (name in ["LakeShipStop", "TEST_FLAT_DOCK_1x3"]) {
		local len = building_desc_x(name).get_size(0).y - 1

		foreach (f in dock_faces()) {
			local face = f[0]
			local across = (face == dir.northeast || face == dir.southwest)

			set_dock_water(pl, k, face, max(len, 1), cl_water)

			if (len > 0 && across) {
				ASSERT_EQUAL(command_x(tool_build_station).work(pl, k, name), "A dock longer than one tile cannot run NE-SW yet.")
				ASSERT_EQUAL(count_buildings_around(k), 0)
			}
			else {
				ASSERT_EQUAL(command_x(tool_build_station).work(pl, k, name), null)
				ASSERT_DOCK_LINE(k, face, len, f[2])

				ASSERT_EQUAL(command_x(tool_remover).work(pl, k), null)
				ASSERT_EQUAL(count_buildings_around(k), 0)
			}

			set_dock_water(pl, k, face, max(len, 1), cl_mediterran)
			RESET_ALL_PLAYER_FUNDS()
		}
	}
}


// Water on a NE-SW side does not make a flat dock ambiguous when it cannot be
// built there: with water NE and S, a 3-tile dock goes S.
function test_halt_build_flat_dock_skips_third_axis()
{
	local pl = player_x(0)
	local k = coord3d(8, 8, 0)

	set_dock_water(pl, k, dir.northeast, 2, cl_water)
	set_dock_water(pl, k, dir.south, 2, cl_water)

	{
		ASSERT_EQUAL(command_x(tool_build_station).work(pl, k, "TEST_FLAT_DOCK_1x3"), null)
		ASSERT_DOCK_LINE(k, dir.south, 2, 0)
	}

	// clean up
	ASSERT_EQUAL(command_x(tool_remover).work(pl, k), null)
	set_dock_water(pl, k, dir.northeast, 2, cl_mediterran)
	set_dock_water(pl, k, dir.south, 2, cl_mediterran)
	RESET_ALL_PLAYER_FUNDS()
}


function test_halt_build_air()
{
	local pl = player_x(0)
	local runway = way_desc_x.get_available_ways(wt_air, st_runway)[0]
	local taxiway = way_desc_x.get_available_ways(wt_air, st_flat)[0]
	local airhalt = building_desc_x("AirStop")

	ASSERT_EQUAL(command_x.build_way(pl, coord3d(5, 5, 0), coord3d(5, 7, 0), runway, true), null)

	// air halts must be on taxiways (contrary to the untranslated error message)
	{
		ASSERT_EQUAL(command_x.build_station(pl, coord3d(5, 5, 0), airhalt), "Flugzeughalt muss auf\nRunway liegen!\n")
		ASSERT_EQUAL(command_x.build_station(pl, coord3d(5, 6, 0), airhalt), "Flugzeughalt muss auf\nRunway liegen!\n")
		ASSERT_EQUAL(command_x.build_station(pl, coord3d(5, 7, 0), airhalt), "Flugzeughalt muss auf\nRunway liegen!\n")
	}

	ASSERT_EQUAL(command_x.build_way(pl, coord3d(5, 6, 0), coord3d(3, 6, 0), taxiway, true), null)

	// not in the middle of a taxiway
	{
		ASSERT_EQUAL(command_x.build_station(pl, coord3d(4, 6, 0), airhalt), "No terminal station here!")
	}

	// end of taxiway -> success
	{
		ASSERT_EQUAL(command_x.build_station(pl, coord3d(3, 6, 0), airhalt), null)

		ASSERT_EQUAL(command_x(tool_remover).work(pl, coord3d(3, 6, 0)), null)
	}

	// clean up
	ASSERT_EQUAL(command_x(tool_remove_way).work(pl, coord3d(3, 6, 0), coord3d(5, 6, 0), "" + wt_air), null)
	ASSERT_EQUAL(command_x(tool_remove_way).work(pl, coord3d(5, 5, 0), coord3d(5, 7, 0), "" + wt_air), null)
	RESET_ALL_PLAYER_FUNDS()
}


function test_halt_build_multi_tile()
{
	local pl = player_x(0)
	local road = way_desc_x.get_available_ways(wt_road, st_flat)[0]
	local station_desc = building_desc_x.get_available_stations(building_desc_x.station, wt_road, {})[0]
	local bridge_desc = bridge_desc_x.get_available_bridges(wt_road)[0]

	// 2 adjacent tiles
	{
		ASSERT_EQUAL(command_x.build_way(pl, coord3d(2, 1, 0), coord3d(2, 3, 0), road, true), null)
		ASSERT_EQUAL(command_x.build_way(pl, coord3d(3, 1, 0), coord3d(3, 3, 0), road, true), null)

		ASSERT_EQUAL(command_x(tool_build_station).work(pl, coord3d(2, 2, 0), station_desc.get_name()), null)
		ASSERT_EQUAL(command_x(tool_build_station).work(pl, coord3d(3, 2, 0), station_desc.get_name()), null)

		local halt = halt_x.get_halt(coord3d(2, 2, 0), pl)
		ASSERT_TRUE(halt != null)
		ASSERT_EQUAL(tile_x(2, 2, 0).get_halt().get_name(), tile_x(3, 2, 0).get_halt().get_name()) // check that this is really the same halt
		ASSERT_EQUAL(halt.get_owner().get_name(), pl.get_name())
		ASSERT_EQUAL(halt.is_connected(halt, good_desc_x.passenger), 1)
		ASSERT_TRUE(halt.accepts_good(good_desc_x.passenger))
		ASSERT_FALSE(halt.accepts_good(good_desc_x.mail))

		ASSERT_EQUAL(halt.get_arrived()[0], 0)
		ASSERT_EQUAL(halt.get_departed()[0], 0)
		ASSERT_EQUAL(halt.get_waiting()[0], 0)
		ASSERT_EQUAL(halt.get_happy()[0], 0)
		ASSERT_EQUAL(halt.get_unhappy()[0], 0)
		ASSERT_EQUAL(halt.get_noroute()[0], 0)
		ASSERT_EQUAL(halt.get_convoys()[0], 0)
		ASSERT_EQUAL(halt.get_walked()[0], 0)
		ASSERT_EQUAL(halt.get_convoy_list().get_count(), 0)
		ASSERT_EQUAL(halt.get_line_list().get_count(), 0)
		ASSERT_EQUAL(halt.get_factory_list().len(), 0)

		local tile_list = halt.get_tile_list()
		local expected_tiles = [ tile_x(2, 2, 0), tile_x(3, 2, 0) ]

		foreach (t in tile_list) {
			ASSERT_TRUE(t.find_object(mo_building) != null)

			// expected_tiles.find(t) != null won't work
			ASSERT_EQUAL(expected_tiles.filter(@(idx, val) t.x == val.x && t.y == val.y && t.z == val.z).len(), 1)
		}

		ASSERT_EQUAL(halt.get_freight_to_dest(good_desc_x.passenger, coord(2, 2)), 0)
		ASSERT_EQUAL(halt.get_freight_to_halt(good_desc_x.passenger, halt), 0)

		// this also depends on the separate_halt_capacities setting
		ASSERT_EQUAL(halt.get_capacity(good_desc_x.passenger), 64)
		ASSERT_EQUAL(halt.get_capacity(good_desc_x.mail), 0)
		ASSERT_EQUAL(halt.get_capacity(good_desc_x("Kohle")), 0)
		ASSERT_EQUAL(halt.get_capacity(good_desc_x("nonexistent")), 0)

		ASSERT_EQUAL(halt.get_connections(good_desc_x.passenger).len(), 0)
		ASSERT_EQUAL(halt.get_connections(good_desc_x.mail).len(), 0)

		ASSERT_EQUAL(command_x(tool_remove_way).work(pl, coord3d(2, 1, 0), coord3d(2, 3, 0), "" + wt_road), null)
		ASSERT_EQUAL(command_x(tool_remove_way).work(pl, coord3d(3, 1, 0), coord3d(3, 3, 0), "" + wt_road), null)
		RESET_ALL_PLAYER_FUNDS()
	}

	// 2 tiles on top of each other
	{
		ASSERT_EQUAL(command_x.build_way(pl, coord3d(2, 4, 0), coord3d(4, 4, 0), road, true), null)
		ASSERT_EQUAL(command_x(tool_build_bridge).work(pl, coord3d(3, 3, 0), coord3d(3, 5, 0), bridge_desc.get_name()), null)

		ASSERT_EQUAL(command_x(tool_build_station).work(pl, coord3d(3, 4, 0), station_desc.get_name()), null)
		ASSERT_EQUAL(command_x(tool_build_station).work(pl, coord3d(3, 4, 1), station_desc.get_name()), null)

		local lower_halt = halt_x.get_halt(coord3d(3, 4, 0), pl)
		local upper_halt = halt_x.get_halt(coord3d(3, 4, 1), pl)

		ASSERT_EQUAL(lower_halt.get_name(), upper_halt.get_name())
		ASSERT_EQUAL(lower_halt.get_tile_list().len(), 2)

		ASSERT_EQUAL(command_x(tool_remove_way).work(pl, coord3d(2, 4, 0), coord3d(4, 4, 0), "" + wt_road), null)
		ASSERT_EQUAL(command_x(tool_remove_way).work(pl, coord3d(3, 2, 0), coord3d(3, 6, 0), "" + wt_road), null)
	}

	RESET_ALL_PLAYER_FUNDS()

	local old_cash = pl.get_cash()[0]
	ASSERT_EQUAL(old_cash, 200*1000)

	// 16x16 station
	{
		for (local x = 0; x < 16; ++x) {
			ASSERT_EQUAL(command_x.build_way(pl, coord3d(x, 0, 0), coord3d(x, 15, 0), road, true), null)
			for (local y = 0; y < 16; ++y) {
				ASSERT_EQUAL(command_x(tool_build_station).work(pl, coord3d(x, y, 0), station_desc.get_name()), null)
			}
		}

		ASSERT_EQUAL(pl.get_current_maintenance(), 16*16*(road.get_maintenance() + station_desc.get_maintenance()))
		ASSERT_EQUAL(pl.get_cash()[0]*100, old_cash*100 - 16*16*(road.get_cost() + station_desc.get_cost()))

		local halt = halt_x.get_halt(coord3d(0, 0, 0), pl)
		ASSERT_TRUE(halt != null)
		ASSERT_TRUE(halt.accepts_good(good_desc_x.passenger))
		ASSERT_FALSE(halt.accepts_good(good_desc_x.mail))
		ASSERT_EQUAL(halt.is_connected(halt, good_desc_x.passenger), 1)
		ASSERT_EQUAL(halt.is_connected(halt, good_desc_x.mail), 1)

		for (local x = 0; x < 16; ++x) {
			ASSERT_EQUAL(command_x(tool_remove_way).work(pl, coord3d(x, 0, 0), coord3d(x, 15, 0), "" + wt_road), null)
		}

		ASSERT_EQUAL(pl.get_current_maintenance(), 0)
	}

	RESET_ALL_PLAYER_FUNDS()
}


function test_halt_build_terminal_stop_all_directions()
{
	local pl = player_x(0)
	local road_desc = way_desc_x.get_available_ways(wt_road, st_flat)[0]
	local centre = coord3d(4, 4, 0)

	// CarStop is pak64's 4-layout terminal stop; the road's far tile
	// is a dead end facing back along d
	foreach (d in dir.nsew) {
		local tip = centre + dir.to_coord(d)
		ASSERT_EQUAL(command_x.build_way(pl, centre, tip, road_desc, true), null)
		ASSERT_EQUAL(command_x(tool_build_station).work(pl, tip, "CarStop"), null)
		ASSERT_EQUAL(command_x(tool_remove_way).work(pl, centre, tip, "" + wt_road), null)
	}

	ASSERT_EQUAL(pl.get_current_maintenance(), 0)
	RESET_ALL_PLAYER_FUNDS()
}


// building_desc_t::adjust_layout folds layouts 4 / 5 (NE / SW) onto 0 / 1
// (S / SE) for a descriptor with 4 or fewer layouts, so the stored layout
// of a NE or SW terminal stop reads as an S or SE one.  The road builder
// must take the direction from the way instead: the road along the stop's
// own axis stays buildable, a road from any other side is refused.
function test_halt_terminal_stop_third_axis()
{
	local pl = player_x(0)
	local roads = get_slow_and_fast_road()
	local stop = coord3d(8, 8, 0)

	foreach (face in [dir.northeast, dir.southwest]) {
		// dead end whose only exit is `face`
		local out = stop + dir.to_coord(face)
		ASSERT_EQUAL(command_x.build_way(pl, stop, out, roads[0], true), null)
		ASSERT_EQUAL(command_x(tool_build_station).work(pl, stop, "CarStop"), null)
		ASSERT_EQUAL(way_dirs_at(stop, wt_road), face)

		// upgrade the road into the stop along its axis
		ASSERT_EQUAL(command_x.build_way(pl, stop, out, roads[1], true), null)
		ASSERT_EQUAL(tile_x(stop.x, stop.y, stop.z).get_way(wt_road).get_desc().get_name(), roads[1].get_name())
		ASSERT_EQUAL(way_dirs_at(stop, wt_road), face)
		ASSERT_EQUAL(way_dirs_at(out, wt_road), dir.backward(face))

		// no road from the other five sides, the stop stays a dead end
		foreach (d in dir.nsew) {
			if (d == face) continue
			local side = stop + dir.to_coord(d)
			ASSERT_EQUAL(command_x.build_way(pl, side, stop, roads[0], true), "A building blocks the construction")
			ASSERT_EQUAL(way_dirs_at(stop, wt_road), face)
			ASSERT_EQUAL(way_dirs_at(side, wt_road), 0)
		}

		ASSERT_EQUAL(command_x(tool_remove_way).work(pl, stop, out, "" + wt_road), null)
	}

	ASSERT_EQUAL(pl.get_current_maintenance(), 0)
	RESET_ALL_PLAYER_FUNDS()
}


// BusStop is pak64's 2-layout through stop.  On a NE-SW road it has to
// stay passable along the road and closed to the four sides.
function test_halt_through_stop_third_axis()
{
	local pl = player_x(0)
	local roads = get_slow_and_fast_road()
	local mid = coord3d(8, 8, 0)
	local sw = mid + dir.to_coord(dir.southwest)
	local ne = mid + dir.to_coord(dir.northeast)

	ASSERT_EQUAL(command_x.build_way(pl, sw, ne, roads[0], true), null)
	ASSERT_EQUAL(command_x(tool_build_station).work(pl, mid, "BusStop"), null)
	ASSERT_EQUAL(way_dirs_at(mid, wt_road), dir.northeast_southwest)

	// extend the road through the platform at both ends
	local far_sw = sw + dir.to_coord(dir.southwest)
	local far_ne = ne + dir.to_coord(dir.northeast)
	ASSERT_EQUAL(command_x.build_way(pl, far_sw, far_ne, roads[1], true), null)
	ASSERT_EQUAL(way_dirs_at(far_sw, wt_road), dir.northeast)
	ASSERT_EQUAL(way_dirs_at(sw, wt_road), dir.northeast_southwest)
	ASSERT_EQUAL(way_dirs_at(mid, wt_road), dir.northeast_southwest)
	ASSERT_EQUAL(way_dirs_at(ne, wt_road), dir.northeast_southwest)
	ASSERT_EQUAL(way_dirs_at(far_ne, wt_road), dir.southwest)
	ASSERT_EQUAL(tile_x(mid.x, mid.y, mid.z).get_way(wt_road).get_desc().get_name(), roads[1].get_name())

	// no road from the sides
	foreach (d in [dir.southeast, dir.south, dir.northwest, dir.north]) {
		local side = mid + dir.to_coord(d)
		ASSERT_EQUAL(command_x.build_way(pl, side, mid, roads[0], true), "A building blocks the construction")
		ASSERT_EQUAL(way_dirs_at(mid, wt_road), dir.northeast_southwest)
		ASSERT_EQUAL(way_dirs_at(side, wt_road), 0)
	}

	ASSERT_EQUAL(command_x(tool_remove_way).work(pl, far_sw, far_ne, "" + wt_road), null)
	ASSERT_EQUAL(pl.get_current_maintenance(), 0)
	RESET_ALL_PLAYER_FUNDS()
}


// A stop whose road is gone has no way to read its direction from and
// falls back to the stored layout.  That is exact for the directions
// adjust_layout does not fold (S, SE, N, NW for a terminal stop, N-S and
// SE-NW for a through one).  A NE or SW terminal stop folds onto S or SE
// and loses its direction together with its road.
function test_halt_stop_reconnect_after_road_removed()
{
	local pl = player_x(0)
	local road = get_slow_and_fast_road()[0]
	local stop = coord3d(8, 8, 0)

	// terminal: only the stop's own side takes a road again
	foreach (face in [dir.south, dir.southeast, dir.north, dir.northwest]) {
		local out = stop + dir.to_coord(face)
		ASSERT_EQUAL(command_x.build_way(pl, stop, out, road, true), null)
		ASSERT_EQUAL(command_x(tool_build_station).work(pl, stop, "CarStop"), null)
		ASSERT_EQUAL(command_x(tool_remove_way).work(pl, out, out, "" + wt_road), null)
		ASSERT_EQUAL(way_dirs_at(stop, wt_road), 0)

		foreach (d in dir.nsew) {
			if (d == face) continue
			local side = stop + dir.to_coord(d)
			ASSERT_EQUAL(command_x.build_way(pl, side, stop, road, true), "A building blocks the construction")
		}
		ASSERT_EQUAL(command_x.build_way(pl, out, stop, road, true), null)
		ASSERT_EQUAL(way_dirs_at(stop, wt_road), face)

		ASSERT_EQUAL(command_x(tool_remove_way).work(pl, stop, out, "" + wt_road), null)
	}

	// through: only the stop's own axis takes a road again
	foreach (axis in [[dir.south, dir.north], [dir.southeast, dir.northwest]]) {
		local far = stop + dir.to_coord(axis[0])
		local near = stop + dir.to_coord(axis[1])
		ASSERT_EQUAL(command_x.build_way(pl, far, near, road, true), null)
		ASSERT_EQUAL(command_x(tool_build_station).work(pl, stop, "BusStop"), null)
		ASSERT_EQUAL(command_x(tool_remove_way).work(pl, far, far, "" + wt_road), null)
		ASSERT_EQUAL(command_x(tool_remove_way).work(pl, near, near, "" + wt_road), null)
		ASSERT_EQUAL(way_dirs_at(stop, wt_road), 0)

		foreach (d in dir.nsew) {
			if (d == axis[0] || d == axis[1]) continue
			local side = stop + dir.to_coord(d)
			ASSERT_EQUAL(command_x.build_way(pl, side, stop, road, true), "A building blocks the construction")
		}
		ASSERT_EQUAL(command_x.build_way(pl, far, near, road, true), null)
		ASSERT_EQUAL(way_dirs_at(stop, wt_road), axis[0] | axis[1])

		ASSERT_EQUAL(command_x(tool_remove_way).work(pl, far, near, "" + wt_road), null)
	}

	ASSERT_EQUAL(pl.get_current_maintenance(), 0)
	RESET_ALL_PLAYER_FUNDS()
}


// Tile layouts of three adjacent stops with 16 layouts (pak64's GCGTrainStop) on a
// rail line along `ends` = [lower end, upper end]: first the stops [lower,
// middle, upper] after the middle one was built first, then [lower, upper]
// once the middle one is removed.  A 16-layout stop's layout has an end bit
// for every side no other stop continues the platform on, so the layouts tell
// whether the stops found each other along the line.
function platform_layouts(ends)
{
	local pl = player_x(0)
	local rail = way_desc_x.get_available_ways(wt_rail, st_flat)[0]
	local step = dir.to_coord(ends[1])
	local tiles = []
	for (local i = -2; i <= 2; i++) {
		tiles.append(coord3d(8 + step.x * i, 8 + step.y * i, 0))
	}
	local layout_at = @(t) tile_x(t.x, t.y, t.z).find_object(mo_building).get_layout()

	ASSERT_EQUAL(command_x.build_way(pl, tiles[0], tiles[4], rail, true), null)
	foreach (i in [2, 1, 3]) {
		ASSERT_EQUAL(command_x(tool_build_station).work(pl, tiles[i], "GCGTrainStop"), null)
	}
	local built = [layout_at(tiles[1]), layout_at(tiles[2]), layout_at(tiles[3])]

	ASSERT_EQUAL(command_x(tool_remover).work(pl, tiles[2]), null)
	local cut = [layout_at(tiles[1]), layout_at(tiles[3])]

	ASSERT_EQUAL(command_x(tool_remove_way).work(pl, tiles[0], tiles[4], "" + wt_rail), null)
	ASSERT_EQUAL(pl.get_current_maintenance(), 0)
	RESET_ALL_PLAYER_FUNDS()

	return { built = built, cut = cut }
}


// The stored layout of a through stop has a bit for the N-S / SE-NW axis
// only, so a platform on the NE-SW axis has to find its neighbours along
// the way instead: building the middle stop first and the ends after, then
// removing the middle one, it has to end up with the layouts of the same
// platform on the SE-NW axis.
function test_halt_platform_end_layouts_third_axis()
{
	local reference = platform_layouts([dir.southeast, dir.northwest])
	local third = platform_layouts([dir.southwest, dir.northeast])

	// the reference is a real platform: the ends differ from the middle,
	// and from what they show once the middle is gone
	ASSERT_TRUE(reference.built[0] != reference.built[1])
	ASSERT_TRUE(reference.built[2] != reference.built[1])
	ASSERT_TRUE(reference.cut[0] != reference.built[0])
	ASSERT_TRUE(reference.cut[1] != reference.built[2])

	for (local i = 0; i < 3; i++) {
		ASSERT_EQUAL(third.built[i], reference.built[i])
	}
	for (local i = 0; i < 2; i++) {
		ASSERT_EQUAL(third.cut[i], reference.cut[i])
	}
}


function test_halt_build_multi_mode()
{
	local pl = player_x(0)
	local road_desc = way_desc_x.get_available_ways(wt_road, st_flat)[0]
	local rail_desc = way_desc_x.get_available_ways(wt_rail, st_flat)[0]
	local pax_stop    = building_desc_x.get_available_stations(building_desc_x.station, wt_road, good_desc_x.passenger)[0]
	local pax_station = building_desc_x.get_available_stations(building_desc_x.station, wt_rail, good_desc_x.passenger)[0]

	{
		ASSERT_EQUAL(command_x.build_way(pl, coord3d(4, 3, 0), coord3d(4, 4, 0), road_desc, true), null)
		ASSERT_EQUAL(command_x.build_way(pl, coord3d(5, 3, 0), coord3d(5, 4, 0), rail_desc, true), null)

		ASSERT_EQUAL(command_x(tool_build_station).work(pl, coord3d(4, 4, 0), pax_stop.get_name()), null)
		ASSERT_EQUAL(command_x(tool_build_station).work(pl, coord3d(5, 4, 0), pax_station.get_name()), null)

		ASSERT_EQUAL(halt_x.get_halt(coord3d(4, 4, 0), pl).get_name(), halt_x.get_halt(coord3d(5, 4, 0), pl).get_name())

		local halt = halt_x.get_halt(coord3d(4, 4, 0), pl)
		ASSERT_EQUAL(halt.get_capacity(good_desc_x.passenger), 64)
		ASSERT_TRUE(halt.accepts_good(good_desc_x.passenger))

		ASSERT_EQUAL(command_x(tool_remove_way).work(pl, coord3d(4, 4, 0), coord3d(4, 3, 0), "" + wt_road), null)
		ASSERT_EQUAL(command_x(tool_remove_way).work(pl, coord3d(5, 4, 0), coord3d(5, 3, 0), "" + wt_rail), null)
	}

	RESET_ALL_PLAYER_FUNDS()
}


function test_halt_build_multi_player()
{
	local pl = player_x(0)
	local public_pl = player_x(1)

	// create another player
	world.create_player(2,1)
	local pl2 = player_x(2)
	ASSERT_EQUAL(pl2.is_valid(), true)
	ASSERT_EQUAL(pl2.is_active(), true)

	local road_desc = way_desc_x.get_available_ways(wt_road, st_flat)[0]
	local pax_halt  = building_desc_x.get_available_stations(building_desc_x.station, wt_road, good_desc_x.passenger)[0]

	{
		ASSERT_EQUAL(command_x.build_way(public_pl, coord3d(4, 4, 0), coord3d(3, 4, 0), road_desc, true), null)
		ASSERT_EQUAL(command_x(tool_build_station).work(pl,        coord3d(4, 4, 0), pax_halt.get_name()), null)
		ASSERT_EQUAL(command_x(tool_build_station).work(pl2, coord3d(3, 4, 0), pax_halt.get_name()), null)

		ASSERT_TRUE(halt_x.get_halt(coord3d(4, 4, 0), pl) != null)
		ASSERT_TRUE(halt_x.get_halt(coord3d(3, 4, 0), pl) == null)

		local my_halt = halt_x.get_halt(coord3d(4, 4, 0), pl)
		local my2_halt = halt_x.get_halt(coord3d(3, 4, 0), pl2)
		ASSERT_TRUE(my_halt != null)
		ASSERT_TRUE(my2_halt != null)

		ASSERT_TRUE(my_halt.get_name() != my2_halt.get_name())
		ASSERT_EQUAL(my_halt.get_tile_list().len(), 1)
		ASSERT_EQUAL(my2_halt.get_tile_list().len(), 1)

		ASSERT_EQUAL(command_x(tool_remove_way).work(public_pl, coord3d(4, 4, 0), coord3d(3, 4, 0), "" + wt_road), null)
	}

	// now joint by tool
	{
		ASSERT_EQUAL(command_x.build_way(public_pl, coord3d(4, 4, 0), coord3d(3, 4, 0), road_desc, true), null)
		ASSERT_EQUAL(command_x(tool_build_station).work(pl,        coord3d(4, 4, 0), pax_halt.get_name()), null)
		local my_halt = halt_x.get_halt(coord3d(4, 4, 0), pl)
		ASSERT_TRUE(my_halt != null)
		ASSERT_FALSE(my_halt.can_use_halt(pl2))
		
		// now building second halt => should autojoin
		ASSERT_EQUAL(command_x(tool_build_station).work(pl2, coord3d(3, 4, 0), pax_halt.get_name()), null)
		local my_halt2 = halt_x.get_halt(coord3d(3, 4, 0), pl2)
		ASSERT_TRUE(my_halt2 != null)
		ASSERT_FALSE(my_halt2.can_use_halt(pl))

		ASSERT_TRUE(my_halt.get_name() != my_halt2.get_name())
		ASSERT_EQUAL(my_halt.get_tile_list().len(), 1)
		ASSERT_EQUAL(my_halt2.get_tile_list().len(), 1)
		
		my_halt.set_permissions(my_halt,4+1);
		ASSERT_EQUAL(my_halt.get_permissions(), 5)
		ASSERT_TRUE(my_halt.can_use_halt(pl2))

		local mergehalt  = command_x(tool_merge_stop)
		ASSERT_EQUAL(mergehalt.work(pl2, coord3d(4, 4, 0), coord3d(3, 4, 0), ""), null)

		// since the halthandle may have become invalid after merge
		my_halt = halt_x.get_halt(coord3d(4, 4, 0), pl)
		ASSERT_TRUE(my_halt != null)
		my_halt2 = halt_x.get_halt(coord3d(3, 4, 0), pl2)
		ASSERT_TRUE(my_halt2 != null)

		ASSERT_TRUE(my_halt.get_name() == my_halt2.get_name())
		ASSERT_EQUAL(my_halt.get_tile_list().len(), 2)
		ASSERT_EQUAL(my_halt2.get_tile_list().len(), 2)

		ASSERT_EQUAL(command_x(tool_remove_way).work(public_pl, coord3d(4, 4, 0), coord3d(3, 4, 0), "" + wt_road), null)
	}

	// now autojoing
	{
		ASSERT_EQUAL(command_x.build_way(public_pl, coord3d(4, 4, 0), coord3d(3, 4, 0), road_desc, true), null)
		ASSERT_EQUAL(command_x(tool_build_station).work(pl,        coord3d(4, 4, 0), pax_halt.get_name()), null)
		local my_halt = halt_x.get_halt(coord3d(4, 4, 0), pl)
		ASSERT_TRUE(my_halt != null)
		my_halt.set_permissions(my_halt,4+1);
		ASSERT_EQUAL(my_halt.get_permissions(), 5)
		ASSERT_TRUE(my_halt.can_use_halt(pl2))

		// now building second halt => should autojoin
		ASSERT_EQUAL(command_x(tool_build_station).work(pl2, coord3d(3, 4, 0), pax_halt.get_name()), null)
		local pub_halt = halt_x.get_halt(coord3d(3, 4, 0), pl)
		ASSERT_TRUE(pub_halt != null)

		ASSERT_TRUE(my_halt.get_name() == pub_halt.get_name())
		ASSERT_EQUAL(my_halt.get_tile_list().len(), 2)
		ASSERT_EQUAL(pub_halt.get_tile_list().len(), 2)

		world.remove_player(pl2)
		ASSERT_EQUAL(my_halt.get_tile_list().len(), 1)

		ASSERT_EQUAL(command_x(tool_remove_way).work(public_pl, coord3d(4, 4, 0), coord3d(3, 4, 0), "" + wt_road), null)
	}

	RESET_ALL_PLAYER_FUNDS()
}


function test_halt_build_separate()
{
	local pl = player_x(0)
	local road_desc = way_desc_x.get_available_ways(wt_road, st_flat)[0]
	local pax_halt  = building_desc_x.get_available_stations(building_desc_x.station, wt_road, good_desc_x.passenger)[0]

	{
		ASSERT_EQUAL(command_x.build_way(pl, coord3d(5, 4, 0), coord3d(3, 4, 0), road_desc, true), null)
		ASSERT_EQUAL(command_x(tool_build_station).work(pl, coord3d(5, 4, 0), pax_halt.get_name()), null)
		ASSERT_EQUAL(command_x(tool_build_station).work(pl, coord3d(3, 4, 0), pax_halt.get_name()), null)
		ASSERT_EQUAL(command_x(tool_build_station).work(pl, coord3d(4, 4, 0), pax_halt.get_name()), null)

		local halt3 = halt_x.get_halt(coord3d(3, 4, 0), pl)
		local halt4 = halt_x.get_halt(coord3d(4, 4, 0), pl)
		local halt5 = halt_x.get_halt(coord3d(5, 4, 0), pl)

		ASSERT_TRUE(halt3 != null)
		ASSERT_TRUE(halt4 != null)
		ASSERT_TRUE(halt5 != null)

		ASSERT_TRUE(halt3.get_name() != halt5.get_name())
		ASSERT_TRUE(halt4.get_name() == halt5.get_name())
		ASSERT_TRUE(halt4.get_name() != halt3.get_name())

		ASSERT_EQUAL(command_x(tool_remove_way).work(pl, coord3d(5, 4, 0), coord3d(3, 4, 0), "" + wt_road), null)
	}

	RESET_ALL_PLAYER_FUNDS()
}


function test_halt_build_near_factory()
{
	local pl = player_x(0)
	local public_pl = player_x(1)
	local road_desc = way_desc_x.get_available_ways(wt_road, st_flat)[0]
	local pax_halt     = building_desc_x.get_available_stations(building_desc_x.station, wt_road, good_desc_x.passenger)[0]
	local freight_halt = building_desc_x.get_available_stations(building_desc_x.station, wt_road, good_desc_x("Kohle"))[0]
	local remover = command_x(tool_remover)

	// build coal mine + coal power plant, then link them
	ASSERT_EQUAL(build_factory(pl, coord3d(0, 0, 0), 1, 1, 1024, "Kohlegrube"), null)

	{
		// also depends on station catchment area size
		ASSERT_EQUAL(command_x.build_way(pl, coord3d(4, 4, 0), coord3d(4, 3, 0), road_desc, true), null)
		ASSERT_EQUAL(command_x(tool_build_station).work(pl, coord3d(4, 4, 0), pax_halt.get_name()), null)

		local halt = halt_x.get_halt(coord3d(4, 4, 0), pl)
		ASSERT_TRUE(halt != null)
		ASSERT_EQUAL(halt.get_factory_list().len(), 1)
		ASSERT_EQUAL(halt.get_factory_list()[0].get_name(), "Coal mine")

		ASSERT_EQUAL(command_x(tool_remove_way).work(pl, coord3d(4, 4, 0), coord3d(4, 3, 0), "" + wt_road), null)
	}

	{
		// catchment is the hex disc, not the axial square: (5, 5) is only
		// 3 axial steps from the mine's corner tile (2, 2), but 6 hex
		// steps (third-axis offset), so with coverage 4 it must not link
		ASSERT_EQUAL(command_x.build_way(pl, coord3d(5, 5, 0), coord3d(5, 4, 0), road_desc, true), null)
		ASSERT_EQUAL(command_x(tool_build_station).work(pl, coord3d(5, 5, 0), pax_halt.get_name()), null)

		local halt = halt_x.get_halt(coord3d(5, 5, 0), pl)
		ASSERT_TRUE(halt != null)
		ASSERT_EQUAL(halt.get_factory_list().len(), 0)

		ASSERT_EQUAL(command_x(tool_remove_way).work(pl, coord3d(5, 5, 0), coord3d(5, 4, 0), "" + wt_road), null)
	}

	ASSERT_EQUAL(remover.work(public_pl, coord3d(0, 0, 0)), null)
	RESET_ALL_PLAYER_FUNDS()
}


function test_halt_build_near_factories()
{
	local pl = player_x(0)
	local public_pl = player_x(1)
	local road_desc = way_desc_x.get_available_ways(wt_road, st_flat)[0]
	local pax_halt     = building_desc_x.get_available_stations(building_desc_x.station, wt_road, good_desc_x.passenger)[0]
	local freight_halt = building_desc_x.get_available_stations(building_desc_x.station, wt_road, good_desc_x("Kohle"))[0]
	local remover = command_x(tool_remover)

	// build coal mine + coal power plant, then link them
	ASSERT_EQUAL(build_factory(pl, coord3d(0, 0, 0), 1, 1, 1024, "Kohlegrube"), null)
	ASSERT_EQUAL(build_factory(pl, coord3d(6, 6, 0), 1, 1, 1024, "Kohlekraftwerk"), null)
	ASSERT_EQUAL(command_x(tool_link_factory).work(pl, coord3d(0, 0, 0), coord3d(6, 6, 0), ""), null)

	{
		// also depends on station catchment area size
		ASSERT_EQUAL(command_x.build_way(pl, coord3d(4, 4, 0), coord3d(4, 3, 0), road_desc, true), null)
		ASSERT_EQUAL(command_x(tool_build_station).work(pl, coord3d(4, 4, 0), pax_halt.get_name()), null)

		local halt = halt_x.get_halt(coord3d(4, 4, 0), pl)
		ASSERT_TRUE(halt != null)
		ASSERT_EQUAL(halt.get_factory_list().len(), 2)
		ASSERT_EQUAL(halt.get_factory_list()[0].get_name(), "Coal power station")
		ASSERT_EQUAL(halt.get_factory_list()[1].get_name(), "Coal mine")

		ASSERT_EQUAL(command_x(tool_remove_way).work(pl, coord3d(4, 4, 0), coord3d(4, 3, 0), "" + wt_road), null)
	}

	// this only works with catchment area size of 4
	{
		ASSERT_EQUAL(command_x.build_way(pl, coord3d(0, 7, 0), coord3d(1, 7, 0), road_desc, true), null)
		ASSERT_EQUAL(command_x(tool_build_station).work(pl, coord3d(1, 7, 0), pax_halt.get_name()), null)

		local halt = halt_x.get_halt(coord3d(1, 7, 0), pl)
		ASSERT_TRUE(halt != null)
		ASSERT_EQUAL(halt.get_factory_list().len(), 0)

		ASSERT_EQUAL(command_x(tool_remove_way).work(pl, coord3d(0, 7, 0), coord3d(1, 7, 0), "" + wt_road), null)
	}


	ASSERT_EQUAL(remover.work(public_pl, coord3d(0, 0, 0)), null)
	ASSERT_EQUAL(remover.work(public_pl, coord3d(6, 6, 0)), null)
	RESET_ALL_PLAYER_FUNDS()
}


// The per-square halt lists — the passenger/mail catchment — hold the hex
// disc koord_distance <= coverage (4 here), not the axial square: (2, 2)
// is already on the rim (a 60°-pair sum), (-4, 4) is too (120°-pair sum),
// and (3, 3) is outside at 6 steps despite |dx|, |dy| <= 4.
function test_halt_coverage_is_hex_disc()
{
	local pl = player_x(0)
	local road_desc = way_desc_x.get_available_ways(wt_road, st_flat)[0]
	local pax_halt = building_desc_x.get_available_stations(building_desc_x.station, wt_road, good_desc_x.passenger)[0]

	ASSERT_EQUAL(command_x.build_way(pl, coord3d(7, 7, 0), coord3d(7, 6, 0), road_desc, true), null)
	ASSERT_EQUAL(command_x(tool_build_station).work(pl, coord3d(7, 7, 0), pax_halt.get_name()), null)

	foreach (rim in [[4, 0], [0, 4], [-4, 4], [2, 2], [-2, -2]]) {
		ASSERT_EQUAL(square_x(7 + rim[0], 7 + rim[1]).get_halt_list().len(), 1)
	}
	foreach (outside in [[3, 3], [-3, -3], [5, 0], [-1, 5]]) {
		ASSERT_EQUAL(square_x(7 + outside[0], 7 + outside[1]).get_halt_list().len(), 0)
	}

	ASSERT_EQUAL(command_x(tool_remove_way).work(pl, coord3d(7, 7, 0), coord3d(7, 6, 0), "" + wt_road), null)
	RESET_ALL_PLAYER_FUNDS()
}


// Counterpoint to `test_depot_build_on_tunnel_entrance`: station
// building has an explicit `ist_tunnel() && ist_karten_boden()` reject
// that the depot tool lacks, so the same 6-mouth scaffold fails on
// every mouth.
function test_halt_build_on_tunnel_entrance()
{
	local pl = player_x(0)
	local rail_tunnel = tunnel_desc_x.get_available_tunnels(wt_rail)[0]
	local station_desc = building_desc_x.get_available_stations(building_desc_x.station, wt_rail, good_desc_x.passenger)[0]
	local mouths = raise_hex_hill_with_six_mouths(pl, 4, 2, 0, rail_tunnel)

	foreach (p in mouths) {
		ASSERT_EQUAL(command_x.build_station(pl, p, station_desc), "No suitable way on the ground!")
	}

	local remover = command_x(tool_remover)
	remover.set_flags(2)
	ASSERT_EQUAL(remover.work(pl, mouths[0]), null)
	remover.set_flags(0)
	foreach (p in mouths) {
		ASSERT_TRUE(tile_x(p.x, p.y, 0).find_object(mo_tunnel) == null)
	}

	lower_hex_tile(pl, 4, 2, 0)
	RESET_ALL_PLAYER_FUNDS()
}


function test_halt_build_on_bridge_end()
{
	local pl = player_x(0)
	local rail_bridge = bridge_desc_x.get_available_bridges(wt_rail)[0]
	local setslope = command_x.set_slope
	local station_desc = building_desc_x.get_available_stations(building_desc_x.station, wt_rail, good_desc_x.passenger)[0]

	// north-south direction
	{
		ASSERT_EQUAL(setslope(pl, coord3d(4, 2, 0), slope.south_narrow), null)
		ASSERT_EQUAL(setslope(pl, coord3d(4, 4, 0), slope.north_narrow), null)

		ASSERT_EQUAL(command_x.build_bridge_at(pl, coord3d(4, 2, 0), rail_bridge), null)
		ASSERT_EQUAL(command_x.build_station(pl, coord3d(4, 2, 0), station_desc), null)
		ASSERT_EQUAL(command_x.build_station(pl, coord3d(4, 4, 0), station_desc), null)

		// tool_remover removes halt first, then bridge, then depot (depot is automatically removed when destroying bridge)
		// So here we have to remove things on the tile twice (contrary to test_depot_build_on_bridge_end)
		ASSERT_EQUAL(command_x(tool_remover).work(pl, coord3d(4, 2, 0)), null)
		ASSERT_EQUAL(command_x(tool_remover).work(pl, coord3d(4, 2, 0)), null)

		ASSERT_EQUAL(setslope(pl, coord3d(4, 2, 0), slope.flat), null)
		ASSERT_EQUAL(setslope(pl, coord3d(4, 4, 0), slope.flat), null)
	}

	// NW-SE direction (formerly "east-west" under square geometry; the
	// (3,3)→(5,3) vector is 2 × SE-neighbour step, so the bridge sits
	// on the hex NW-SE axis with low edges facing each other)
	{
		ASSERT_EQUAL(setslope(pl, coord3d(3, 3, 0), slope.southeast_narrow), null)
		ASSERT_EQUAL(setslope(pl, coord3d(5, 3, 0), slope.northwest_narrow), null)

		ASSERT_EQUAL(command_x.build_bridge_at(pl, coord3d(3, 3, 0), rail_bridge), null)
		ASSERT_EQUAL(command_x.build_depot(pl, coord3d(3, 3, 0), get_depot_by_wt(wt_rail)), null)
		ASSERT_EQUAL(command_x.build_depot(pl, coord3d(5, 3, 0), get_depot_by_wt(wt_rail)), null)
		ASSERT_EQUAL(command_x(tool_remover).work(pl, coord3d(3, 3, 0)), null)

		ASSERT_EQUAL(setslope(pl, coord3d(3, 3, 0), slope.flat), null)
		ASSERT_EQUAL(setslope(pl, coord3d(5, 3, 0), slope.flat), null)
	}

	// clean up
	RESET_ALL_PLAYER_FUNDS()
}


function test_halt_build_on_depot()
{
	local pl = player_x(0)
	local rail_desc = way_desc_x.get_available_ways(wt_rail, st_flat)[0]
	local station_desc = building_desc_x.get_available_stations(building_desc_x.station, wt_rail, good_desc_x.passenger)[0]
	local depot = get_depot_by_wt(wt_rail)

	ASSERT_EQUAL(command_x.build_way(pl, coord3d(4, 2, 0), coord3d(4, 4, 0), rail_desc, true), null)
	ASSERT_EQUAL(command_x.build_depot(pl, coord3d(4, 2, 0), depot), null)

	{
		ASSERT_EQUAL(command_x.build_station(pl, coord3d(4, 2, 0), station_desc), "No suitable ground!")
	}

	// clean up
	ASSERT_EQUAL(command_x(tool_remover).work(pl, coord3d(4, 2, 0)), null)
	ASSERT_EQUAL(command_x(tool_remove_way).work(pl, coord3d(4, 2, 0), coord3d(4, 4, 0), "" + wt_rail), null)
	RESET_ALL_PLAYER_FUNDS()
}


function test_halt_build_station_invalid_param()
{
	// null default_param
	{
		local error_caught = false
		try {
			ASSERT_EQUAL(command_x(tool_build_station).work(player_x(0), coord3d(0, 0, 0), null), "")
		}
		catch (e) {
			error_caught = true
			ASSERT_EQUAL(e, "Error during initializing tool")
		}
		ASSERT_TRUE(error_caught)
	}

	// empty default_param
	{
		local error_caught = false
		try {
			ASSERT_EQUAL(command_x(tool_build_station).work(player_x(0), coord3d(0, 0, 0), ""), "")
		}
		catch (e) {
			error_caught = true
			ASSERT_EQUAL(e, "Error during initializing tool")
		}
		ASSERT_TRUE(error_caught)
	}

	// rotation information not a number
	{
		local error_caught = false
		try {
			ASSERT_EQUAL(command_x(tool_build_station).work(player_x(0), coord3d(0, 0, 0), "foo,bar"), "")
		}
		catch (e) {
			error_caught = true
			ASSERT_EQUAL(e, "Error during initializing tool")
		}
		ASSERT_TRUE(error_caught)
	}

	// rotation information number out of range 1
	{
		local error_caught = false
		try {
			ASSERT_EQUAL(command_x(tool_build_station).work(player_x(0), coord3d(0, 0, 0), "foo,16"), "")
		}
		catch (e) {
			error_caught = true
			ASSERT_EQUAL(e, "Error during initializing tool")
		}
		ASSERT_TRUE(error_caught)
	}

	// rotation information number out of range 2
	{
		local error_caught = false
		try {
			ASSERT_EQUAL(command_x(tool_build_station).work(player_x(0), coord3d(0, 0, 0), "foo,-2"), "")
		}
		catch (e) {
			error_caught = true
			ASSERT_EQUAL(e, "Error during initializing tool")
		}
		ASSERT_TRUE(error_caught)
	}

	// rotation information number out of range 3 (out of sint8 range)
	{
		local error_caught = false
		try {
			ASSERT_EQUAL(command_x(tool_build_station).work(player_x(0), coord3d(0, 0, 0), "foo,1000"), "")
		}
		catch (e) {
			error_caught = true
			ASSERT_EQUAL(e, "Error during initializing tool")
		}
		ASSERT_TRUE(error_caught)
	}

	// clean up
	RESET_ALL_PLAYER_FUNDS()
}


function test_halt_build_station_extension()
{
	local pl = player_x(0)
	local setslope = command_x.set_slope
	local stationbuilder = command_x(tool_build_station)
	local wayremover = command_x(tool_remove_way)
	local road_desc = way_desc_x.get_available_ways(wt_road, st_flat)[0] // road because it has double slopes available
	local station_desc = building_desc_x.get_available_stations(building_desc_x.station, wt_road, {})[0]
	local stext_desc = building_desc_x.get_available_stations(building_desc_x.station_extension, wt_rail, good_desc_x.passenger)[0]
	local bridge_desc = bridge_desc_x.get_available_bridges(wt_road)[0]

	ASSERT_TRUE(station_desc != null)
	ASSERT_TRUE(stext_desc != null)
	ASSERT_TRUE(bridge_desc != null)
	ASSERT_TRUE(road_desc != null)

	// build station extension without station: should fail
	{
		ASSERT_EQUAL(stationbuilder.work(pl, coord3d(4, 3, 0), stext_desc.get_name()), "Post muss neben\nHaltestelle\nliegen!\n")

		ASSERT_EQUAL(pl.get_current_maintenance(), 0)
		ASSERT_EQUAL(tile_x(4, 3, 0).find_object(mo_building), null)
	}

	ASSERT_EQUAL(command_x.build_way(pl, coord3d(4, 2, 0), coord3d(4, 4, 0), road_desc, true), null)
	ASSERT_EQUAL(stationbuilder.work(pl, coord3d(4, 3, 0), station_desc.get_name()), null)
	local old_maintenance = pl.get_current_maintenance()

	// build station extension next to station: should succeed
	{
		ASSERT_EQUAL(stationbuilder.work(pl, coord3d(5, 3, 0), stext_desc.get_name()), null)
		ASSERT_EQUAL(command_x(tool_remover).work(pl, coord3d(5, 3, 0)), null)
		ASSERT_EQUAL(pl.get_current_maintenance(), old_maintenance)

		// and diagonal
		ASSERT_EQUAL(stationbuilder.work(pl, coord3d(5, 2, 0), stext_desc.get_name()), null)
		ASSERT_EQUAL(command_x(tool_remover).work(pl, coord3d(5, 2, 0)), null)
		ASSERT_EQUAL(pl.get_current_maintenance(), old_maintenance)
	}

	// build station extension on raised tile: should succeed
	{
		ASSERT_EQUAL(setslope(pl, coord3d(5, 3, 0), slope.all_up_slope), null)
		ASSERT_EQUAL(setslope(pl, coord3d(5, 3, 1), slope.all_up_slope), null)
		ASSERT_EQUAL(setslope(pl, coord3d(5, 3, 2), slope.all_up_slope), null)

		ASSERT_EQUAL(stationbuilder.work(pl, coord3d(5, 3, 3), stext_desc.get_name()), null)
		ASSERT_EQUAL(command_x(tool_remover).work(pl, coord3d(5, 3, 3)), null)
		ASSERT_EQUAL(pl.get_current_maintenance(), old_maintenance)

		// up slope is automatically removed
		ASSERT_EQUAL(square_x(5, 3).get_tile_at_height(1), null)
		ASSERT_EQUAL(square_x(5, 3).get_tile_at_height(2), null)
	}

	{
		// diagonal too
		ASSERT_EQUAL(setslope(pl, coord3d(5, 2, 0), slope.all_up_slope), null)
		ASSERT_EQUAL(setslope(pl, coord3d(5, 2, 1), slope.all_up_slope), null)

		ASSERT_EQUAL(stationbuilder.work(pl, coord3d(5, 2, 2), stext_desc.get_name()), null)
		ASSERT_EQUAL(command_x(tool_remover).work(pl, coord3d(5, 2, 2)), null)
		ASSERT_EQUAL(pl.get_current_maintenance(), old_maintenance)

		// up slope is automatically removed
		ASSERT_EQUAL(square_x(5, 3).get_tile_at_height(1), null)
		ASSERT_EQUAL(square_x(5, 3).get_tile_at_height(2), null)
	}

	ASSERT_EQUAL(wayremover.work(pl, coord3d(4, 2, 0), coord3d(4, 4, 0), "" + wt_road), null)
	ASSERT_EQUAL(pl.get_current_maintenance(), 0)

	// clean up
	RESET_ALL_PLAYER_FUNDS()
}


function test_halt_upgrade_downgrade()
{
	local pl = player_x(0)
	local stationbuilder = command_x(tool_build_station)
	local wayremover = command_x(tool_remove_way)
	local road_desc = way_desc_x.get_available_ways(wt_road, st_flat)[0]
	local pax_halts  = building_desc_x.get_available_stations(building_desc_x.station, wt_road, good_desc_x.passenger)
	pax_halts.sort(@(a, b) a.get_capacity() <=> b.get_capacity())

	ASSERT_TRUE(pax_halts.len() >= 2)
	local small_halt = pax_halts[0]
	local big_halt = pax_halts[1]
	local pos = coord3d(3, 3, 0)

	ASSERT_EQUAL(command_x.build_way(pl, coord3d(3, 2, 0), coord3d(3, 4, 0), road_desc, true), null)

	// upgrade station
	{
		ASSERT_EQUAL(stationbuilder.work(pl, pos, small_halt.get_name()), null)
		local halt = halt_x.get_halt(pos, pl)
		ASSERT_TRUE(halt != null)
		local old_capacity = halt.get_capacity(good_desc_x.passenger)
		local old_maint = pl.get_current_maintenance()
		ASSERT_EQUAL(stationbuilder.work(pl, pos, big_halt.get_name()), null)

		ASSERT_TRUE(halt.is_valid())
		ASSERT_TRUE(halt.accepts_good(good_desc_x.passenger))
		ASSERT_TRUE(halt.get_capacity(good_desc_x.passenger) > old_capacity)
		ASSERT_EQUAL(halt.get_tile_list().len(), 1)
		ASSERT_TRUE(pl.get_current_maintenance() > old_maint)
		ASSERT_EQUAL(pl.get_current_maintenance() - old_maint, big_halt.get_maintenance() - small_halt.get_maintenance())
	}

	// upgrade station to same level
	{
		local halt = halt_x.get_halt(pos, pl)
		local old_capacity = halt.get_capacity(good_desc_x.passenger)
		local old_maint = pl.get_current_maintenance()
		local old_cash = pl.get_current_cash()
		ASSERT_EQUAL(stationbuilder.work(pl, coord3d(3, 3, 0), big_halt.get_name()), null)

		ASSERT_TRUE(halt.is_valid())
		ASSERT_TRUE(halt.accepts_good(good_desc_x.passenger))
		ASSERT_EQUAL(halt.get_capacity(good_desc_x.passenger), old_capacity)
		ASSERT_EQUAL(halt.get_tile_list().len(), 1)
		ASSERT_EQUAL(pl.get_current_maintenance(), old_maint)
	}

	// downgrade without ctrl, should fail
	{
		local halt = halt_x.get_halt(pos, pl)
		local old_capacity = halt.get_capacity(good_desc_x.passenger)
		local old_maint = pl.get_current_maintenance()
		local old_cash = pl.get_current_cash()
		ASSERT_EQUAL(stationbuilder.work(pl, coord3d(3, 3, 0), small_halt.get_name()), "Upgrade must have\na higher level")

		ASSERT_TRUE(halt.is_valid())
		ASSERT_TRUE(halt.accepts_good(good_desc_x.passenger))
		ASSERT_EQUAL(halt.get_capacity(good_desc_x.passenger), old_capacity)
		ASSERT_EQUAL(halt.get_tile_list().len(), 1)
		ASSERT_EQUAL(pl.get_current_maintenance(), old_maint)
	}

	// downgrade with ctrl
	{
		local halt = halt_x.get_halt(pos, pl)
		local old_capacity = halt.get_capacity(good_desc_x.passenger)
		local old_maint = pl.get_current_maintenance()
		local old_cash = pl.get_current_cash()

		stationbuilder.set_flags(2) // ctrl
		ASSERT_EQUAL(stationbuilder.work(pl, coord3d(3, 3, 0), small_halt.get_name()), null)

		ASSERT_TRUE(halt.is_valid())
		ASSERT_TRUE(halt.accepts_good(good_desc_x.passenger))
		ASSERT_TRUE(halt.get_capacity(good_desc_x.passenger) < old_capacity)
		ASSERT_EQUAL(halt.get_tile_list().len(), 1)
		ASSERT_TRUE(pl.get_current_maintenance() < old_maint)
		ASSERT_EQUAL(pl.get_current_maintenance() - old_maint, small_halt.get_maintenance() - big_halt.get_maintenance())
	}

	// clean up
	ASSERT_EQUAL(wayremover.work(pl, coord3d(3, 2, 0), coord3d(3, 4, 0), "" + wt_road), null)
	RESET_ALL_PLAYER_FUNDS()
}


function test_halt_make_public_single()
{
	local pl = player_x(0)
	local public_pl = player_x(1)
	local stationbuilder = command_x(tool_build_station)
	local wayremover = command_x(tool_remove_way)
	local pax_halt   = building_desc_x.get_available_stations(building_desc_x.station, wt_road, good_desc_x.passenger)[0]
	local road_desc  = way_desc_x.get_available_ways(wt_road, st_flat)[0]
	local makepublic = command_x(tool_make_stop_public)

	ASSERT_EQUAL(command_x.build_way(pl, coord3d(4, 2, 0), coord3d(4, 4, 0), road_desc, true), null)
	ASSERT_EQUAL(command_x.build_station(pl, coord3d(4, 3, 0), pax_halt), null)

	// invalid pos
	{
		local old_cash = pl.get_current_cash()
		local old_maint = pl.get_current_maintenance()

		ASSERT_EQUAL(makepublic.work(pl, coord3d(-1, -1, -1)), "No suitable ground!")

		ASSERT_EQUAL(pl.get_current_cash(), old_cash)
		ASSERT_EQUAL(pl.get_current_maintenance(), old_maint)
	}

	// no stop under cursor
	{
		local old_cash = pl.get_current_cash()
		local old_maint = pl.get_current_maintenance()

		ASSERT_EQUAL(makepublic.work(pl, coord3d(3, 2, 0)), null)

		ASSERT_EQUAL(pl.get_current_cash(), old_cash)
		ASSERT_EQUAL(pl.get_current_maintenance(), old_maint)
	}

	// valid stop - Making the stop public also makes the road under it public
	{
		local old_cash = pl.get_current_cash()
		local old_maint = pl.get_current_maintenance()

		ASSERT_EQUAL(makepublic.work(pl, coord3d(4, 3, 0)), null)
		ASSERT_TRUE(halt_x.get_halt(coord3d(4, 3, 0), public_pl) != null)
		ASSERT_EQUAL(halt_x.get_halt(coord3d(4, 3, 0), public_pl).get_owner().get_name(), public_pl.get_name())

		ASSERT_EQUAL(pl.get_current_maintenance(), old_maint - 1*pax_halt.get_maintenance() - 1*road_desc.get_maintenance())
		ASSERT_EQUAL(pl.get_current_cash()*100, old_cash*100 - 60 * (1*pax_halt.get_maintenance() + 1*road_desc.get_maintenance()) )
		ASSERT_EQUAL(public_pl.get_current_maintenance(), 1*pax_halt.get_maintenance() + 1*road_desc.get_maintenance())
	}

	// already public - should have no effect
	{
		local old_cash  = pl.get_current_cash()
		local old_maint = pl.get_current_maintenance()

		ASSERT_EQUAL(makepublic.work(pl, coord3d(4, 3, 0)), "Das Feld gehoert\neinem anderen Spieler\n")
		ASSERT_TRUE(halt_x.get_halt(coord3d(4, 3, 0), public_pl) != null)
		ASSERT_EQUAL(halt_x.get_halt(coord3d(4, 3, 0), public_pl).get_owner().get_name(), public_pl.get_name())

		ASSERT_EQUAL(pl.get_current_maintenance(), old_maint)
		ASSERT_EQUAL(pl.get_current_cash(), old_cash)
	}

	// same for public player
	{
		local old_cash  = public_pl.get_current_cash()
		local old_maint = public_pl.get_current_maintenance()

		ASSERT_EQUAL(makepublic.work(public_pl, coord3d(4, 3, 0)), "Das Feld gehoert\neinem anderen Spieler\n")
		ASSERT_TRUE(halt_x.get_halt(coord3d(4, 3, 0), public_pl) != null)

		ASSERT_EQUAL(public_pl.get_current_maintenance(), old_maint)
		ASSERT_EQUAL(public_pl.get_current_cash(), old_cash)
	}

	ASSERT_EQUAL(command_x(tool_remover).work(public_pl, coord3d(4, 3, 0)), null)
	ASSERT_EQUAL(command_x.build_station(pl, coord3d(4, 3, 0), pax_halt), null)

	// If public player uses this tool, he pays for it
	{
		local old_cash  = public_pl.get_current_cash()
		local old_maint = public_pl.get_current_maintenance()

		ASSERT_EQUAL(makepublic.work(pl, coord3d(4, 3, 0)), null)

		ASSERT_TRUE(halt_x.get_halt(coord3d(4, 3, 0), public_pl) != null)
		ASSERT_EQUAL(halt_x.get_halt(coord3d(4, 3, 0), public_pl).get_owner().get_name(), public_pl.get_name())

		// only halt maintenance because the road is already public
		ASSERT_EQUAL(public_pl.get_current_cash()*100, 100*old_cash + 60 * pax_halt.get_maintenance())
		ASSERT_EQUAL(public_pl.get_current_maintenance(), old_maint + pax_halt.get_maintenance())
	}

	ASSERT_EQUAL(wayremover.work(public_pl, coord3d(4, 2, 0), coord3d(4, 4, 0), "" + wt_road), null)
	ASSERT_EQUAL(command_x.build_way(pl, coord3d(4, 2, 0), coord3d(4, 4, 0), road_desc, true), null)
	ASSERT_EQUAL(command_x.build_station(pl, coord3d(4, 3, 0), pax_halt), null)

	// Public player can make halts of other players public even if road underneath is not public
	{
		local old_cash  = public_pl.get_current_cash()
		local old_maint = public_pl.get_current_maintenance()

		ASSERT_EQUAL(makepublic.work(pl, coord3d(4, 3, 0)), null)

		ASSERT_TRUE(halt_x.get_halt(coord3d(4, 3, 0), public_pl) != null)
		ASSERT_EQUAL(halt_x.get_halt(coord3d(4, 3, 0), public_pl).get_owner().get_name(), public_pl.get_name())

		// halt + road maintenance
		ASSERT_EQUAL(public_pl.get_current_cash()*100, 100*old_cash + 60 * pax_halt.get_maintenance())
		ASSERT_EQUAL(public_pl.get_current_maintenance(), old_maint + pax_halt.get_maintenance() + road_desc.get_maintenance())
	}

	ASSERT_EQUAL(wayremover.work(public_pl, coord3d(4, 2, 0), coord3d(4, 4, 0), "" + wt_road), null)
	RESET_ALL_PLAYER_FUNDS()
}


// Regression test for the shared-halt ownership bug (forum topic 23970):
// making a stop public must only convert the initiating player's tiles, never the
// tiles of a co-owner of the same (public) halt.
function test_halt_make_public_shared()
{
	local pl        = player_x(0)
	local public_pl = player_x(1)
	local pax_halt  = building_desc_x.get_available_stations(building_desc_x.station, wt_road, good_desc_x.passenger)[0]
	local road_desc = way_desc_x.get_available_ways(wt_road, st_flat)[0]
	local makepublic = command_x(tool_make_stop_public)
	local wayremover = command_x(tool_remove_way)

	// second company that will co-own the halt
	world.create_player(2, 1)
	local pl2 = player_x(2)
	ASSERT_TRUE(pl2.is_valid())
	SET_PLAYER_FUNDS(pl2, 200000 * 100)

	// a public road, so that stations of different players may be built along it
	ASSERT_EQUAL(command_x.build_way(public_pl, coord3d(4, 1, 0), coord3d(4, 5, 0), road_desc, true), null)

	// pl0 builds a station and turns the halt public, so other players may join it
	ASSERT_EQUAL(command_x.build_station(pl, coord3d(4, 3, 0), pax_halt), null)
	ASSERT_EQUAL(makepublic.work(pl, coord3d(4, 3, 0)), null)

	// pl0 and pl2 each add a private station tile that joins the now public halt
	ASSERT_EQUAL(command_x.build_station(pl,  coord3d(4, 2, 0), pax_halt), null)
	ASSERT_EQUAL(command_x.build_station(pl2, coord3d(4, 4, 0), pax_halt), null)

	// verify the setup: one shared halt with three different tile owners
	ASSERT_TRUE(halt_x.get_halt(coord3d(4, 3, 0), public_pl) != null)
	ASSERT_EQUAL(building_x(4, 3, 0).get_owner().nr, public_pl.nr)
	ASSERT_EQUAL(building_x(4, 2, 0).get_owner().nr, pl.nr)
	ASSERT_EQUAL(building_x(4, 4, 0).get_owner().nr, pl2.nr)

	// pl0 makes the shared halt public again via its own tile
	{
		local pl2_maint = pl2.get_current_maintenance()

		ASSERT_EQUAL(makepublic.work(pl, coord3d(4, 2, 0)), null)

		// pl0's own tile is now public ...
		ASSERT_EQUAL(building_x(4, 2, 0).get_owner().nr, public_pl.nr)
		// ... but pl2's tile must be left untouched (this is the bug being fixed)
		ASSERT_EQUAL(building_x(4, 4, 0).get_owner().nr, pl2.nr)
		ASSERT_EQUAL(pl2.get_current_maintenance(), pl2_maint)
	}

	// cleanup
	world.remove_player(pl2)
	ASSERT_EQUAL(command_x(tool_remover).work(public_pl, coord3d(4, 2, 0)), null)
	ASSERT_EQUAL(command_x(tool_remover).work(public_pl, coord3d(4, 3, 0)), null)
	ASSERT_EQUAL(wayremover.work(public_pl, coord3d(4, 1, 0), coord3d(4, 5, 0), "" + wt_road), null)
	RESET_ALL_PLAYER_FUNDS()
}


function test_halt_make_public_multi_tile()
{
	local pl = player_x(0)
	local public_pl = player_x(1)
	local stationbuilder = command_x(tool_build_station)
	local wayremover = command_x(tool_remove_way)
	local pax_halt   = building_desc_x.get_available_stations(building_desc_x.station, wt_road, good_desc_x.passenger)[0]
	local road_desc  = way_desc_x.get_available_ways(wt_road, st_flat)[0]
	local makepublic = command_x(tool_make_stop_public)

	ASSERT_EQUAL(command_x.build_way(pl, coord3d(4, 2, 0), coord3d(4, 5, 0), road_desc, true), null)
	ASSERT_EQUAL(command_x.build_station(pl, coord3d(4, 3, 0), pax_halt), null)
	ASSERT_EQUAL(command_x.build_station(pl, coord3d(4, 4, 0), pax_halt), null)

	// valid stop - Making the stop public also makes the road under it public
	{
		local old_cash = pl.get_current_cash()
		local old_maint = pl.get_current_maintenance()

		ASSERT_EQUAL(makepublic.work(pl, coord3d(4, 3, 0)), null)
		ASSERT_TRUE(halt_x.get_halt(coord3d(4, 3, 0), public_pl) != null)
		ASSERT_EQUAL(halt_x.get_halt(coord3d(4, 3, 0), public_pl).get_owner().get_name(), public_pl.get_name())

		ASSERT_EQUAL(pl.get_current_maintenance(), old_maint - 2*pax_halt.get_maintenance() - 2*road_desc.get_maintenance())
		ASSERT_EQUAL(pl.get_current_cash()*100, old_cash*100 - 60 * (2*pax_halt.get_maintenance() + 2*road_desc.get_maintenance()) )
		ASSERT_EQUAL(public_pl.get_current_maintenance(), 2*pax_halt.get_maintenance() + 2*road_desc.get_maintenance())
	}

	// already public - should have no effect
	{
		local old_cash  = pl.get_current_cash()
		local old_maint = pl.get_current_maintenance()

		ASSERT_EQUAL(makepublic.work(pl, coord3d(4, 3, 0)), "Das Feld gehoert\neinem anderen Spieler\n")
		ASSERT_TRUE(halt_x.get_halt(coord3d(4, 3, 0), public_pl) != null)
		ASSERT_EQUAL(halt_x.get_halt(coord3d(4, 3, 0), public_pl).get_owner().get_name(), public_pl.get_name())

		ASSERT_EQUAL(pl.get_current_maintenance(), old_maint)
		ASSERT_EQUAL(pl.get_current_cash(), old_cash)
	}

	// same for public player
	{
		local old_cash  = public_pl.get_current_cash()
		local old_maint = public_pl.get_current_maintenance()

		ASSERT_EQUAL(makepublic.work(public_pl, coord3d(4, 3, 0)), "Das Feld gehoert\neinem anderen Spieler\n")
		ASSERT_TRUE(halt_x.get_halt(coord3d(4, 3, 0), public_pl) != null)

		ASSERT_EQUAL(public_pl.get_current_maintenance(), old_maint)
		ASSERT_EQUAL(public_pl.get_current_cash(), old_cash)
	}

	ASSERT_EQUAL(wayremover.work(public_pl, coord3d(4, 2, 0), coord3d(4, 5, 0), "" + wt_road), null)
	ASSERT_EQUAL(command_x.build_way(pl, coord3d(4, 2, 0), coord3d(4, 5, 0), road_desc, true), null)
	ASSERT_EQUAL(command_x.build_station(pl, coord3d(4, 3, 0), pax_halt), null)
	ASSERT_EQUAL(command_x.build_station(pl, coord3d(4, 4, 0), pax_halt), null)

	// If public player uses this tool, he pays for it
	{
		local old_cash  = public_pl.get_current_cash()
		local old_maint = public_pl.get_current_maintenance()

		ASSERT_EQUAL(makepublic.work(pl, coord3d(4, 3, 0)), null)

		ASSERT_TRUE(halt_x.get_halt(coord3d(4, 3, 0), public_pl) != null)
		ASSERT_EQUAL(halt_x.get_halt(coord3d(4, 3, 0), public_pl).get_owner().get_name(), public_pl.get_name())

		ASSERT_EQUAL(public_pl.get_current_cash()*100, 100*old_cash + 60 * 2*pax_halt.get_maintenance())
		ASSERT_EQUAL(public_pl.get_current_maintenance(), old_maint + (2*pax_halt.get_maintenance() + 2*road_desc.get_maintenance()))
	}

	ASSERT_EQUAL(wayremover.work(public_pl, coord3d(4, 2, 0), coord3d(4, 5, 0), "" + wt_road), null)
	RESET_ALL_PLAYER_FUNDS()
}


// make_public on an underground halt transfers exactly the halt
// + the one tunnel tile under it; the surface mouths on either side
// stay pl-owned.
function test_halt_make_public_underground()
{
	local pl = player_x(0)
	local public_pl = player_x(1)
	local tunnel_desc = tunnel_desc_x.get_available_tunnels(wt_road)[0]
	local pax_halt    = building_desc_x("LongBusStop")
	local makepublic  = command_x(tool_make_stop_public)
	local stationbuilder = command_x(tool_build_station)

	raise_hex_tile(public_pl, 4, 3, 0)

	ASSERT_EQUAL(command_x(tool_build_tunnel).work(pl, coord3d(4, 2, 0), tunnel_desc.get_name()), null)
	ASSERT_EQUAL(stationbuilder.work(pl, coord3d(4, 3, 0), pax_halt.get_name()), null)

	// valid underground stop
	{
		local old_cash = pl.get_current_cash()
		local old_maint = pl.get_current_maintenance()

		ASSERT_EQUAL(makepublic.work(pl, coord3d(4, 3, 0)), null)
		ASSERT_TRUE(halt_x.get_halt(coord3d(4, 3, 0), public_pl) != null)
		ASSERT_EQUAL(halt_x.get_halt(coord3d(4, 3, 0), public_pl).get_owner().get_name(), public_pl.get_name())

		ASSERT_EQUAL(pl.get_current_maintenance(), old_maint - 1*pax_halt.get_maintenance() - 1*tunnel_desc.get_maintenance())
		ASSERT_EQUAL(pl.get_current_cash()*100, old_cash*100 - 60 * (1*pax_halt.get_maintenance() + 1*tunnel_desc.get_maintenance()) )
		ASSERT_EQUAL(public_pl.get_current_maintenance(), 1*pax_halt.get_maintenance() + 1*tunnel_desc.get_maintenance())
	}

	ASSERT_EQUAL(command_x(tool_remove_way).work(public_pl, coord3d(4, 2, 0), coord3d(4, 4, 0), "" + wt_road), null)
	lower_hex_tile(public_pl, 4, 3, 0)
	RESET_ALL_PLAYER_FUNDS()
}


function test_halt_move_stop_invalid_param()
{
	// out of map limits
	{
		local error_caught = false
		try {
			ASSERT_EQUAL(command_x(tool_stop_mover).work(player_x(0), coord3d(-1, -1, 0), coord3d(-1, -1, 0)), "")
		}
		catch (e) {
			error_caught = true
			ASSERT_EQUAL(e, "Tool has no effects")
		}
		ASSERT_TRUE(error_caught)
	}

	// no way
	{
		local error_caught = false
		try {
			ASSERT_EQUAL(command_x(tool_stop_mover).work(player_x(0), coord3d(1, 1, 0), coord3d(3, 1, 0)), "")
		}
		catch (e) {
			error_caught = true
			ASSERT_EQUAL(e, "Tool has no effects")
		}
		ASSERT_TRUE(error_caught)
	}

	// clean up
	RESET_ALL_PLAYER_FUNDS()
}


function test_halt_build_explicit_layout_way_orientation()
{
	local pl = player_x(0)
	local stationbuilder = command_x(tool_build_station)
	local wayremover = command_x(tool_remove_way)
	local remover = command_x(tool_remover)
	local road_desc = way_desc_x.get_available_ways(wt_road, st_flat)[0]
	local rail_desc = way_desc_x.get_available_ways(wt_rail, st_flat)[0]
	// a tram track is a track_wt way whose system type is tram
	local tram_desc = way_desc_x.get_available_ways(wt_rail, st_tram)[0]
	local station_desc = building_desc_x("SandStationMHz") // through station, 2 layouts

	// preconditions
	ASSERT_TRUE(road_desc != null)
	ASSERT_TRUE(rail_desc != null)
	ASSERT_TRUE(tram_desc != null)
	ASSERT_TRUE(station_desc != null)

	// a level crossing: road and rail together do not run through the tile, so
	// no through station belongs here -- whatever layout the caller asks for
	{
		ASSERT_EQUAL(command_x.build_way(pl, coord3d(4, 2, 0), coord3d(4, 6, 0), road_desc, true), null)
		ASSERT_EQUAL(command_x.build_way(pl, coord3d(2, 4, 0), coord3d(6, 4, 0), rail_desc, true), null)
		ASSERT_TRUE(tile_x(4, 4, 0).has_two_ways())

		local old_cash = pl.get_current_cash()
		ASSERT_EQUAL(stationbuilder.work(pl, coord3d(4, 4, 0), station_desc.get_name()), "No through station here!")
		ASSERT_EQUAL(command_x.build_station(pl, coord3d(4, 4, 0), station_desc, 0), "No through station here!")
		ASSERT_EQUAL(command_x.build_station(pl, coord3d(4, 4, 0), station_desc, 1), "No through station here!")
		ASSERT_EQUAL(pl.get_current_cash(), old_cash)
		ASSERT_EQUAL(tile_x(4, 4, 0).find_object(mo_building), null)

		ASSERT_EQUAL(wayremover.work(pl, coord3d(2, 4, 0), coord3d(6, 4, 0), "" + wt_rail), null)
		ASSERT_EQUAL(wayremover.work(pl, coord3d(4, 2, 0), coord3d(4, 6, 0), "" + wt_road), null)
	}

	// a single way that bends: the same rule, with no road anywhere near it
	{
		ASSERT_EQUAL(command_x.build_way(pl, coord3d(4, 2, 0), coord3d(4, 4, 0), rail_desc, true), null)
		ASSERT_EQUAL(command_x.build_way(pl, coord3d(4, 4, 0), coord3d(6, 4, 0), rail_desc, true), null)
		ASSERT_FALSE(tile_x(4, 4, 0).has_two_ways())

		local old_cash = pl.get_current_cash()
		ASSERT_EQUAL(stationbuilder.work(pl, coord3d(4, 4, 0), station_desc.get_name()), "No through station here!")
		ASSERT_EQUAL(command_x.build_station(pl, coord3d(4, 4, 0), station_desc, 0), "No through station here!")
		ASSERT_EQUAL(pl.get_current_cash(), old_cash)
		ASSERT_EQUAL(tile_x(4, 4, 0).find_object(mo_building), null)

		ASSERT_EQUAL(wayremover.work(pl, coord3d(4, 2, 0), coord3d(6, 4, 0), "" + wt_rail), null)
	}

	// two ways whose orientations agree -- a tram embedded in a road -- are a
	// legitimate station tile, and must stay one for both call shapes
	{
		ASSERT_EQUAL(command_x.build_way(pl, coord3d(2, 4, 0), coord3d(6, 4, 0), road_desc, true), null)
		ASSERT_EQUAL(command_x.build_way(pl, coord3d(2, 4, 0), coord3d(6, 4, 0), tram_desc, true), null)
		ASSERT_TRUE(tile_x(4, 4, 0).has_two_ways())

		ASSERT_EQUAL(stationbuilder.work(pl, coord3d(4, 4, 0), station_desc.get_name()), null)
		ASSERT_EQUAL(remover.work(pl, coord3d(4, 4, 0)), null)
		ASSERT_EQUAL(command_x.build_station(pl, coord3d(4, 4, 0), station_desc, 0), null)
		ASSERT_EQUAL(remover.work(pl, coord3d(4, 4, 0)), null)

		ASSERT_EQUAL(wayremover.work(pl, coord3d(2, 4, 0), coord3d(6, 4, 0), "" + wt_rail), null)
		ASSERT_EQUAL(wayremover.work(pl, coord3d(2, 4, 0), coord3d(6, 4, 0), "" + wt_road), null)
	}

	// a dead end is straight, and an explicit layout may not change that either
	{
		ASSERT_EQUAL(command_x.build_way(pl, coord3d(4, 2, 0), coord3d(4, 4, 0), rail_desc, true), null)

		ASSERT_EQUAL(stationbuilder.work(pl, coord3d(4, 2, 0), station_desc.get_name()), null)
		ASSERT_EQUAL(remover.work(pl, coord3d(4, 2, 0)), null)
		ASSERT_EQUAL(command_x.build_station(pl, coord3d(4, 2, 0), station_desc, 0), null)
		ASSERT_EQUAL(remover.work(pl, coord3d(4, 2, 0)), null)

		ASSERT_EQUAL(wayremover.work(pl, coord3d(4, 2, 0), coord3d(4, 4, 0), "" + wt_rail), null)
	}

	// clean up
	RESET_ALL_PLAYER_FUNDS()
}
