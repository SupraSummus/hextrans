//
// This file is part of the Simutrans project under the Artistic License.
// (see LICENSE.txt)
//

//
// Tests for adding, removing and deleting cities
//


function test_city_add_invalid()
{
	ASSERT_EQUAL(command_x(tool_add_city).work(player_x(1), coord3d(-1, -1, 0)), "")
}


function test_city_add_cannot_afford()
{
	ASSERT_EQUAL(command_x(tool_add_city).work(player_x(0), coord3d(7, 8, 0)), "Insufficient funds!")

	// clean up
	RESET_ALL_PLAYER_FUNDS()
}


function test_city_add_by_public_player()
{
	{
		ASSERT_EQUAL(command_x(tool_add_city).work(player_x(1), coord3d(7, 8, 0)), null)
		assert_townhall_with_road(7, 8)
	}

	// clean up
	clear_townhall_and_roads(player_x(1), 7, 8)
	RESET_ALL_PLAYER_FUNDS()
}


function test_city_add_on_existing_townhall()
{
	ASSERT_EQUAL(command_x(tool_add_city).work(player_x(1), coord3d(7, 8, 0)), null)

	{
		ASSERT_EQUAL(command_x(tool_add_city).work(player_x(1), coord3d(7, 8, 0)), "Cities can only be built on flat empty ground!")
	}

	// clean up
	clear_townhall_and_roads(player_x(1), 7, 8)
	RESET_ALL_PLAYER_FUNDS()
}


function test_city_add_near_map_border()
{
	{
		ASSERT_EQUAL(command_x(tool_add_city).work(player_x(1), coord3d(0, 15, 0)), null)
		assert_townhall_with_road(0, 15)
	}

	// clean up
	clear_townhall_and_roads(player_x(1), 0, 15)
	RESET_ALL_PLAYER_FUNDS()
}


function test_city_change_size_invalid_params()
{
	local pl = player_x(0)

	// not a city
	{
		ASSERT_EQUAL(command_x(tool_change_city_size).work(pl, coord3d(1, 1, 0), "100"), "")
	}

	ASSERT_EQUAL(command_x(tool_add_city).work(player_x(1), coord3d(1, 1, 0)), null)

	// invalid pos
	{
		ASSERT_EQUAL(command_x(tool_change_city_size).work(pl, coord3d(-1,-1,-1), "100"), "")
	}

	// Null default_param
	{
		local error_caught = false
		try {
			ASSERT_EQUAL(command_x(tool_change_city_size).work(pl, coord3d(1, 1, 0), null), "")
		}
		catch (e) {
			error_caught = true
			ASSERT_EQUAL(e, "Error during initializing tool")
		}
		ASSERT_TRUE(error_caught)
	}

	// Empty default_param
	{
		local error_caught = false
		try {
			ASSERT_EQUAL(command_x(tool_change_city_size).work(pl, coord3d(1, 1, 0), ""), "")
		}
		catch (e) {
			error_caught = true
			ASSERT_EQUAL(e, "Error during initializing tool")
		}
		ASSERT_TRUE(error_caught)
	}

	// Wrong default_param
	{
		local error_caught = false
		try {
			ASSERT_EQUAL(command_x(tool_change_city_size).work(pl, coord3d(1, 1, 0), "bla"), "")
		}
		catch (e) {
			error_caught = true
			ASSERT_EQUAL(e, "Error during initializing tool")
		}
		ASSERT_TRUE(error_caught)
	}

	// clean up
	clear_townhall_and_roads(player_x(1), 1, 1)
	RESET_ALL_PLAYER_FUNDS()
}


// Exercises the stadt_t::~stadt_t() multi-tile-building path: build a
// city, grow it until check_bau_townhall picks a multi-tile townhall
// (pak64 lands on 04_CITY 2x2 around bev=20000), tear down every
// non-townhall building first to keep the destructor's iteration
// focused on the multi-tile townhall, then remove the townhall.
// Without the destructor fix in this commit, the cascade through
// gebaeude_t::cleanup -> stadt_t::remove_gebaeude_from_stadt asserts
// when it walks back to the already-popped head tile.
function test_city_remove_with_multitile_townhall()
{
	local pl = player_x(1)
	local townhall_pos = coord3d(8, 8, 0)

	ASSERT_EQUAL(command_x(tool_add_city).work(pl, townhall_pos, "0"), null)
	ASSERT_EQUAL(command_x(tool_change_city_size).work(player_x(0), townhall_pos, "20000"), null)

	// Tear down every non-townhall building.  Capture the townhall's
	// position on the way through.
	local size = world.get_size()
	local th_corner = null
	for (local x = 0; x < size.x; x++) {
		for (local y = 0; y < size.y; y++) {
			local b = tile_x(x, y, 0).find_object(mo_building)
			if (b == null) continue
			if (b.is_townhall()) {
				if (th_corner == null) th_corner = coord3d(x, y, 0)
				continue
			}
			command_x(tool_remover).work(pl, coord3d(x, y, 0))
		}
	}

	// Fail loudly if the pakset never upgraded the townhall to multi-
	// tile; otherwise the test would silently pass without exercising
	// the destructor path we care about.
	local th_size = tile_x(th_corner.x, th_corner.y, 0).find_object(mo_building).get_desc().get_size(0)
	ASSERT_TRUE(th_size.x > 1 || th_size.y > 1)

	ASSERT_EQUAL(command_x(tool_remover).work(pl, th_corner), null)

	// ~stadt_t() tears down everything in stadt::buildings (the
	// townhall, plus monuments/attractions added via
	// add_gebaeude_to_stadt) but not the auto-generated road network;
	// sweep what's left so RESET_ALL_PLAYER_FUNDS sees zero
	// maintenance.
	for (local x = 0; x < size.x; x++) {
		for (local y = 0; y < size.y; y++) {
			local tile = tile_x(x, y, 0)
			if (tile.get_way(wt_road) != null) {
				local p = coord3d(x, y, 0)
				command_x(tool_remove_way).work(pl, p, p, "" + wt_road)
			}
		}
	}

	RESET_ALL_PLAYER_FUNDS()
}


function test_city_change_size_to_minimum()
{
	ASSERT_EQUAL(command_x(tool_add_city).work(player_x(1), coord3d(1, 1, 0)), null)
	ASSERT_EQUAL(city_x(1, 1).get_citizens()[0], 126)

	{
		ASSERT_EQUAL(command_x(tool_change_city_size).work(player_x(0),  coord3d(1, 1, 0), "-100"), null)
		ASSERT_EQUAL(city_x(1, 1).get_citizens()[0], 126)
	}

	// clean up
	clear_townhall_and_roads(player_x(1), 1, 1)
	RESET_ALL_PLAYER_FUNDS()
}


function test_city_population_ledger()
{
	local pl = player_x(1)

	// recompute the city population ledger from the map: every tile of a
	// residential city building houses level*10 citizens, every tile of a
	// commercial/industrial one provides level*20 jobs.
	// stadt_t::build_city_house() maintains the same numbers incrementally.
	local check_ledger = function(city, where) {
		local sum_res = 0
		local sum_jobs = 0
		for (local x = 0; x < 16; x++) {
			for (local y = 0; y < 16; y++) {
				local b = square_x(x, y).get_ground_tile().find_object(mo_building)
				if (b == null) {
					continue
				}
				local d = b.get_desc()
				local t = d.get_type()
				if (t == building_desc_x.city_res) {
					sum_res += d.get_level()
				}
				else if (t == building_desc_x.city_com || t == building_desc_x.city_ind) {
					sum_jobs += d.get_level()
				}
			}
		}
		ASSERT_EQUAL("housing " + where + ": " + city.get_housing(), "housing " + where + ": " + (sum_res * 10))
		ASSERT_EQUAL("jobs " + where + ": " + city.get_jobs(), "jobs " + where + ": " + (sum_jobs * 20))
	}

	ASSERT_EQUAL(command_x(tool_add_city).work(pl, coord3d(8, 8, 0), "0"), null)
	local city = city_x(8, 8)
	check_ledger(city, "after founding")

	// plant the test-only 2x2 residential building; this also goes through
	// stadt_t::build_city_house and must keep the ledger consistent
	ASSERT_EQUAL(command_x(tool_build_house).work(pl, coord3d(4, 8, 0), "1ATEST_RES_2x2"), null)
	check_ledger(city, "after planting 2x2")

	// grow the city until renovation replaces a TEST_RES_2x2 by a smaller
	// building.  TEST_RES_2x2 is the only multi-tile city building
	// available, so a renovation of it necessarily goes through the
	// leftover-tile conversion in build_city_house.
	local count_2x2_tiles = function() {
		local n = 0
		for (local x = 0; x < 16; x++) {
			for (local y = 0; y < 16; y++) {
				local b = square_x(x, y).get_ground_tile().find_object(mo_building)
				if (b != null && b.get_desc().get_name() == "TEST_RES_2x2") {
					n++
				}
			}
		}
		return n
	}

	local shrunk = false
	local seen_max = count_2x2_tiles()
	for (local i = 0; i < 300 && !shrunk; i++) {
		city.change_size(100)
		local n = count_2x2_tiles()
		if (n < seen_max) {
			// one of the 2x2 buildings was replaced by something smaller
			shrunk = true
		}
		else if (n > seen_max) {
			seen_max = n
		}
	}
	ASSERT_TRUE(shrunk) // if this fails, growth never renovated a 2x2; test setup issue, not a ledger bug

	check_ledger(city, "after renovation")

	// clean up: removing the townhall removes the city and all its buildings
	local cp = city.get_pos()
	local th_tile = square_x(cp.x, cp.y).get_ground_tile()
	ASSERT_EQUAL(command_x(tool_remover).work(pl, coord3d(cp.x, cp.y, th_tile.z)), null)
	for (local x = 0; x < 16; x++) {
		for (local y = 0; y < 16; y++) {
			local tile = square_x(x, y).get_ground_tile()
			if (tile.get_way(wt_road) != null) {
				local p = coord3d(x, y, tile.z)
				command_x(tool_remove_way).work(pl, p, p, "" + wt_road)
			}
		}
	}
	RESET_ALL_PLAYER_FUNDS()
}


// A 1x1 townhall seeds its road on one neighbour tile.  With every
// neighbour but `seed` flooded the seed lands on `seed`, and on the
// townhall's NE or SW face that goes through stadt_t::build_road, which
// decides what the new city road joins.  `seed` touches a stop or depot on
// a dead end, built from a road that leaves along `road_to`.
//   kind      "depot", or the name of a stop
//   road_to   direction of the building's road
//   seed_dir  direction from the building to the seed tile
//   town_dir  direction from the seed tile to the townhall (NE or SW)
//   joins     whether the seed road is to connect to the building
function check_city_seed_next_to_building(kind, road_to, seed_dir, town_dir, joins)
{
	local pl = player_x(0)
	local public_pl = player_x(1)
	local road = get_slow_and_fast_road()[0]
	local site = coord3d(8, 8, 0)
	local out = site + dir.to_coord(road_to)
	local seed = site + dir.to_coord(seed_dir)
	local townhall = seed + dir.to_coord(town_dir)

	ASSERT_EQUAL(command_x.build_way(pl, site, out, road, true), null)
	if (kind == "depot") {
		ASSERT_EQUAL(command_x.build_depot(pl, site, get_depot_by_wt(wt_road)), null)
	}
	else {
		ASSERT_EQUAL(command_x(tool_build_station).work(pl, site, kind), null)
	}
	ASSERT_EQUAL(way_dirs_at(site, wt_road), road_to)

	local flooded = []
	foreach (d in dir.nsew) {
		local c = townhall + dir.to_coord(d)
		if (c.x == seed.x && c.y == seed.y) continue
		ASSERT_EQUAL(command_x(tool_set_climate).work(pl, c, c, "" + cl_water), null)
		flooded.append(c)
	}

	ASSERT_EQUAL(command_x(tool_add_city).work(public_pl, townhall, "0"), null)

	ASSERT_TRUE(tile_x(seed.x, seed.y, seed.z).get_way(wt_road) != null)
	if (joins) {
		ASSERT_EQUAL(way_dirs_at(site, wt_road), road_to | seed_dir)
		ASSERT_EQUAL(way_dirs_at(seed, wt_road), dir.backward(seed_dir))
	}
	else {
		ASSERT_EQUAL(way_dirs_at(site, wt_road), road_to)
		ASSERT_EQUAL(way_dirs_at(seed, wt_road), 0)
	}

	// clean up
	clear_townhall_and_roads(public_pl, townhall.x, townhall.y)
	foreach (c in flooded) {
		ASSERT_EQUAL(command_x(tool_set_climate).work(pl, c, c, "" + cl_mediterran), null)
	}
	ASSERT_EQUAL(command_x(tool_remover).work(pl, site), null)
	ASSERT_EQUAL(command_x(tool_remove_way).work(pl, site, out, "" + wt_road), null)
}


// building_desc_t::adjust_layout folds a NE terminal stop onto layout 0
// (S) and a SW one onto layout 1 (SE), so the stored layout names the
// wrong side.  The city road has to go by the way: it must not bend the
// terminal from the folded side.  A through stop on a NE-SW dead end takes
// the city road at its open end, and not across the platform.
function test_city_road_seed_next_to_stop()
{
	check_city_seed_next_to_building("CarStop", dir.northeast, dir.south,     dir.southwest, false)
	check_city_seed_next_to_building("CarStop", dir.southwest, dir.southeast, dir.northeast, false)
	check_city_seed_next_to_building("BusStop", dir.southwest, dir.northeast, dir.northeast, true)
	check_city_seed_next_to_building("BusStop", dir.southwest, dir.southeast, dir.northeast, false)

	RESET_ALL_PLAYER_FUNDS()
}


function test_city_road_seed_next_to_depot()
{
	check_city_seed_next_to_building("depot", dir.northeast, dir.south,     dir.southwest, false)
	check_city_seed_next_to_building("depot", dir.southwest, dir.southeast, dir.northeast, false)

	RESET_ALL_PLAYER_FUNDS()
}


// stadt_t::orient_city_building turns a one-tile city building to face a
// road on any of its six sides.  `faces` lists [side, layout] pairs: a
// road on `side` of the building must give it `layout`.
function check_city_building_faces_road(name, faces)
{
	local pl = player_x(0)
	local public_pl = player_x(1)
	local road = get_slow_and_fast_road()[0]
	local site = coord3d(10, 10, 0)

	// the building is only placed if there is a city to own it
	ASSERT_EQUAL(command_x(tool_add_city).work(public_pl, coord3d(3, 3, 0), "0"), null)

	foreach (face in faces) {
		local near = site + dir.to_coord(face[0])
		local far = near + dir.to_coord(face[0])
		local where = name + " with a road to " + face[0]
		ASSERT_EQUAL(command_x.build_way(pl, near, far, road, true), null)

		// the "A" in the parameter asks for the automatic orientation
		ASSERT_EQUAL(command_x(tool_build_house).work(public_pl, site, "1A" + name), null)
		local house = tile_x(site.x, site.y, site.z).find_object(mo_building)
		ASSERT_TRUE(house != null)
		ASSERT_EQUAL(where + ": layout " + house.get_layout(), where + ": layout " + face[1])

		ASSERT_EQUAL(command_x(tool_remover).work(public_pl, site), null)
		ASSERT_EQUAL(command_x(tool_remove_way).work(pl, near, far, "" + wt_road), null)
	}

	// clean up
	clear_townhall_and_roads(public_pl, 3, 3)
}


// Layouts 0..5 face S, SE, N, NW, NE and SW.  A building with four layouts
// has none for NE and SW, which building_desc_t::adjust_layout folds onto
// S and SE.
function test_city_building_faces_road()
{
	check_city_building_faces_road("TEST_RES_1x1_8L", [
		[dir.south, 0], [dir.southeast, 1], [dir.north, 2],
		[dir.northwest, 3], [dir.northeast, 4], [dir.southwest, 5]])
	check_city_building_faces_road("TEST_RES_1x1_4L", [
		[dir.south, 0], [dir.southeast, 1], [dir.north, 2],
		[dir.northwest, 3], [dir.northeast, 0], [dir.southwest, 1]])

	RESET_ALL_PLAYER_FUNDS()
}


// The tiles [x, y] with a road in the 3x3 axial ring around (q, r).
function city_ring_roads(q, r)
{
	local roads = []
	for (local x = q - 1; x <= q + 1; x++) {
		for (local y = r - 1; y <= r + 1; y++) {
			if (square_x(x, y).get_ground_tile().get_way(wt_road) != null) {
				roads.append([x, y])
			}
		}
	}
	return roads
}


// Remove the townhall at (q, r) and the roads on the ring around it.
function clear_city_on_raised_ground(pl, q, r)
{
	local th = square_x(q, r).get_ground_tile()
	ASSERT_EQUAL(command_x(tool_remover).work(pl, coord3d(q, r, th.z)), null)
	foreach (c in city_ring_roads(q, r)) {
		local p = coord3d(c[0], c[1], square_x(c[0], c[1]).get_ground_tile().z)
		ASSERT_EQUAL(command_x(tool_remove_way).work(pl, p, p, "" + wt_road), null)
	}
}


// stadt_t::build_road joins a new city road to a road at another height.
// Where the ground cannot be levelled it bends the tile instead, and a
// tile with all its corners but one at the road's height is raised whole
// rather than turned into a ramp.  The tile K = (8, 8) lies between the
// townhall T = (7, 9) on its SW side, a hill one level up, and a road on
// two tiles to the north, one level up as well.  Its SE corner is up too,
// so the E corner is the only low one, and the old check, which counted
// the NE, SE, NW and SW corners, saw four of five.  A label on the SE
// neighbour of K, which shares the E corner, keeps the ground from being
// levelled.
//
// A townhall founds its road on one of its six sides: a strip of three
// tiles on the sides but NE and SW, which have a single tile and go
// through build_road.  The strips would terrace the ground around K, so
// water at (6, 8) and (8, 10) stops all four.  Of the six random starting
// sides all but SW then end on K.
function test_city_road_fills_valley_tile()
{
	local pl = player_x(0)
	local public_pl = player_x(1)
	local road = get_slow_and_fast_road()[0]
	local water = [coord3d(6, 8, 0), coord3d(8, 10, 0)]

	raise_hex_tile(pl, 7, 9, 0)
	raise_hex_tile_pair_S(pl, 8, 6, 0)
	ASSERT_EQUAL(command_x.grid_raise_at_corner(pl, coord3d(8, 8, 0), 1), null)
	ASSERT_EQUAL(tile_x(8, 8, 0).get_slope(), HEX_SLOPE(0, 1, 1, 1, 1, 1))
	ASSERT_EQUAL(command_x.build_way(pl, coord3d(8, 6, 1), coord3d(8, 7, 1), road, true), null)
	ASSERT_EQUAL(label_x.create(coord3d(9, 8, 0), pl, "unlevelled"), null)
	foreach (c in water) {
		ASSERT_EQUAL(command_x(tool_set_climate).work(pl, c, c, "" + cl_water), null)
	}

	local seeded = false
	for (local i = 0; i < 20 && !seeded; i++) {
		ASSERT_EQUAL(command_x(tool_add_city).work(public_pl, coord3d(7, 9, 1), "0"), null)
		local roads = city_ring_roads(7, 9)
		seeded = roads.len() == 1 && roads[0][0] == 8 && roads[0][1] == 8
		if (!seeded) {
			clear_city_on_raised_ground(public_pl, 7, 9)
		}
	}
	ASSERT_TRUE(seeded)

	// the tile is level with the townhall and the road, and joins the road
	local ground = square_x(8, 8).get_ground_tile()
	ASSERT_EQUAL("tile height " + ground.z, "tile height 1")
	ASSERT_EQUAL(ground.get_slope(), slope.flat)
	ASSERT_EQUAL(way_dirs_at(coord3d(8, 8, 1), wt_road), dir.north)

	// clean up
	clear_city_on_raised_ground(public_pl, 7, 9)
	ASSERT_EQUAL(command_x(tool_remover).work(pl, coord3d(9, 8, 0)), null)
	ASSERT_EQUAL(command_x(tool_remove_way).work(pl, coord3d(8, 6, 1), coord3d(8, 7, 1), "" + wt_road), null)
	foreach (c in water) {
		ASSERT_EQUAL(command_x(tool_set_climate).work(pl, c, c, "" + cl_mediterran), null)
	}
	// raising K to the road left its slope at odds with the corners
	ASSERT_EQUAL(command_x(tool_restoreslope).work(pl, coord3d(8, 8, 1)), null)
	ASSERT_EQUAL(command_x.grid_lower_at_corner(pl, coord3d(8, 8, 0), 1), null)
	lower_hex_tile_pair_S(pl, 8, 6, 0)
	lower_hex_tile(pl, 7, 9, 0)
	ASSERT_EQUAL(tile_x(8, 8, 0).get_slope(), slope.flat)

	RESET_ALL_PLAYER_FUNDS()
}
