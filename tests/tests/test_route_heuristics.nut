//
// This file is part of the Simutrans project under the Artistic License.
// (see LICENSE.txt)
//


//
// Tests for the route searches: the distance estimate of the way builder's A*
// (way_builder_t::calc_distance) and the postprocessing of ship routes
// (route_t::postprocess_water_route).
//
// Terminology used below:
//   hex distance   fewest steps between two tiles over the six neighbours,
//                  (|dx| + |dy| + |dx + dy|) / 2 on axial coordinates
//   shortest       a route over distance + 1 tiles on flat open ground
//


// the hex distance (koord_distance) of two tiles
function route_hex_distance(x0, y0, x1, y1)
{
	local dx = x1 - x0
	local dy = y1 - y0
	local abs_dx = dx < 0 ? -dx : dx
	local abs_dy = dy < 0 ? -dy : dy
	local abs_sum = (dx + dy) < 0 ? -(dx + dy) : (dx + dy)
	return (abs_dx + abs_dy + abs_sum) / 2
}


// all road tiles on the map, as one list of [x, y]
function route_road_tiles()
{
	local tiles = []
	for (local x = 0; x < 16; ++x) {
		for (local y = 0; y < 16; ++y) {
			local tile = square_x(x, y).get_tile_at_height(0)
			if (tile != null  &&  tile.has_way(wt_road)) {
				tiles.append([x, y])
			}
		}
	}
	return tiles
}


// a route is not a straight line, so it is taken down tile by tile
function route_clear_roads(pl)
{
	foreach (tile in route_road_tiles()) {
		command_x(tool_remover).work(pl, coord3d(tile[0], tile[1], 0))
	}
	ASSERT_EQUAL(route_road_tiles().len(), 0)
}


// Builds a road with the route search and returns the number of tiles it took.
function route_build_road(pl, x0, y0, x1, y1)
{
	local road = way_desc_x.get_available_ways(wt_road, st_flat)[0]
	ASSERT_TRUE(road != null)
	ASSERT_EQUAL(route_road_tiles().len(), 0)
	ASSERT_EQUAL(command_x.build_way(pl, coord3d(x0, y0, 0), coord3d(x1, y1, 0), road, false), null)
	return route_road_tiles().len()
}


//
// Along each hex axis the only shortest route is the straight line; the
// search must find exactly that, whichever way the road is laid.  NE-SW is the
// axis whose distance the axial Manhattan estimate doubled.
//
function test_route_heuristics_way_axes()
{
	local pl = player_x(0)

	// [x0, y0, x1, y1, step in x, step in y]
	local axes = [
		[3, 2, 3, 12, 0, 1],    // S
		[2, 3, 12, 3, 1, 0],    // SE
		[12, 2, 2, 12, -1, 1],  // SW, from the NE end
	]

	foreach (axis in axes) {
		foreach (reversed in [false, true]) {
			local x0 = reversed ? axis[2] : axis[0]
			local y0 = reversed ? axis[3] : axis[1]
			local x1 = reversed ? axis[0] : axis[2]
			local y1 = reversed ? axis[1] : axis[3]

			local length = route_hex_distance(x0, y0, x1, y1) + 1
			ASSERT_EQUAL(route_build_road(pl, x0, y0, x1, y1), length)

			// and every one of them is on the line
			for (local i = 0; i < length; ++i) {
				ASSERT_TRUE(square_x(axis[0] + i * axis[4], axis[1] + i * axis[5]).get_tile_at_height(0).has_way(wt_road))
			}

			route_clear_roads(pl)
		}
	}
	RESET_ALL_PLAYER_FUNDS()
}


//
// Off the axes there are many shortest routes, and the search has to find one.
// These are pairs of tiles that the axial Manhattan estimate sent on a detour
// of one tile; the other tiles of the map are not all shortest, because the
// curve costs of the way builder sometimes prefer an extra tile to a turn.
//
function test_route_heuristics_way_open_ground_shortest()
{
	local pl = player_x(0)

	local pairs = [
		[10, 12, 2, 9],
		[15, 7, 9, 5],
		[13, 10, 1, 3],
		[4, 0, 11, 3],
		[15, 11, 12, 7],
		[1, 2, 11, 12],
		[12, 12, 8, 0],
		[0, 15, 10, 7],
		[0, 0, 14, 5],
	]

	foreach (p in pairs) {
		ASSERT_EQUAL(route_build_road(pl, p[0], p[1], p[2], p[3]), route_hex_distance(p[0], p[1], p[2], p[3]) + 1)
		route_clear_roads(pl)
	}
	RESET_ALL_PLAYER_FUNDS()
}


//
// The ship route search takes no turn on a straight run and a postprocessing
// pass takes out the extra turns this leaves. The pass replaces part of the
// route by a straight substitute, which has to end on the tile where the
// replaced part ended; one that ended elsewhere left a gap of tiles that are
// not neighbours, and the ship would have crossed it in one step.
//
function test_route_heuristics_water_route_is_connected()
{
	local pl = player_x(0)
	local climate = command_x(tool_set_climate)

	// a lake with one island
	ASSERT_EQUAL(climate.work(pl, coord3d(1, 1, 0), coord3d(14, 14, 0), "" + cl_water), null)
	ASSERT_EQUAL(climate.work(pl, coord3d(7, 7, 0), coord3d(7, 7, 0), "" + cl_mediterran), null)

	// pairs of tiles whose routes the pass used to break
	local pairs = [
		[5, 4, 7, 8],
		[7, 6, 5, 12],
		[9, 10, 6, 5],
		[3, 7, 12, 5],
		[9, 4, 4, 12],
		[10, 11, 7, 6],
		[5, 12, 7, 2],
	]

	foreach (p in pairs) {
		local from = coord3d(p[0], p[1], 0)
		local to = coord3d(p[2], p[3], 0)
		sroute_open(0, sroute_schedule(wt_water, [from, to]), 0)
		sroute_mark(0, true)
		sroute_step()

		ASSERT_TRUE(sroute_len() > 0)
		ASSERT_EQUAL(sroute_gaps(), 0)

		// from one tile to the next is one step, or none where two legs meet
		local prev = sroute_tile(0)
		for (local i = 1; i < sroute_len(); ++i) {
			local tile = sroute_tile(i)
			local steps = route_hex_distance(prev.x, prev.y, tile.x, tile.y)
			if (steps > 1) {
				print("ship route from " + from + " to " + to + " jumps " + steps + " tiles from " + prev + " to " + tile)
			}
			ASSERT_TRUE(steps <= 1)
			prev = tile
		}
		sroute_close(0)
	}

	ASSERT_EQUAL(climate.work(pl, coord3d(1, 1, 0), coord3d(14, 14, 0), "" + cl_mediterran), null)
	RESET_ALL_PLAYER_FUNDS()
}
