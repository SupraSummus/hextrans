//
// This file is part of the Simutrans project under the Artistic License.
// (see LICENSE.txt)
//


function create_simple_schedule(waytype, stop_positions)
{
	return schedule_x(waytype, stop_positions.map(@(pos) schedule_entry_x(pos, 0, 0)))
}

//
// Tests for transporting passengers, mail and goods
//

function test_transport_generate_pax_invalid_pos()
{
	ASSERT_EQUAL(world.generate_goods(coord(-1, -1), coord(-1, -1), good_desc_x.passenger, 42), 0)
	ASSERT_EQUAL(world.generate_goods(coord( 0,  0), coord(-1, -1), good_desc_x.passenger, 42), 0)
	ASSERT_EQUAL(world.generate_goods(coord(-1, -1), coord( 0,  0), good_desc_x.passenger, 42), 0)
}


function test_transport_generate_pax_walked()
{
	local pl = player_x(0)
	local road_desc = way_desc_x.get_available_ways(wt_road, st_flat)[0]

	ASSERT_EQUAL(command_x.build_way(pl, coord3d(4, 2, 0), coord3d(4, 3, 0), road_desc, true), null)
	ASSERT_EQUAL(command_x(tool_build_station).work(pl, coord3d(4, 2, 0), "BusStop"), null)

	{
		ASSERT_EQUAL(world.generate_goods(coord(3, 2), coord(5, 2), good_desc_x.passenger, 42), 2) // 2 == walked

		ASSERT_EQUAL(halt_x.get_halt(coord3d(4, 2, 0), player_x(0)).get_walked()[0], 42)
	}

	// clean up
	ASSERT_EQUAL(command_x(tool_remover).work(player_x(0), coord3d(4, 2, 0)), null)
	ASSERT_EQUAL(command_x(tool_remove_way).work(player_x(0), coord3d(4, 2, 0), coord3d(4, 3, 0), "" + wt_road), null)
	RESET_ALL_PLAYER_FUNDS()
}


function test_transport_generate_pax_no_route()
{
	local pl = player_x(0)
	local road_desc = way_desc_x.get_available_ways(wt_road, st_flat)[0]

	ASSERT_EQUAL(command_x.build_way(pl, coord3d(4, 2, 0), coord3d(4, 7, 0), road_desc, true), null)
	ASSERT_EQUAL(command_x(tool_build_station).work(pl, coord3d(4, 2, 0), "BusStop"), null)
	ASSERT_EQUAL(command_x(tool_build_station).work(pl, coord3d(4, 7, 0), "BusStop"), null)

	{
		ASSERT_EQUAL(world.generate_goods(coord(3, 2), coord(3, 7), good_desc_x.passenger, 42), 0)
	}

	// clean up
	ASSERT_EQUAL(command_x(tool_remover).work(player_x(0), coord3d(4, 2, 0)), null)
	ASSERT_EQUAL(command_x(tool_remover).work(player_x(0), coord3d(4, 7, 0)), null)
	ASSERT_EQUAL(command_x(tool_remove_way).work(player_x(0), coord3d(4, 2, 0), coord3d(4, 7, 0), "" + wt_road), null)
	RESET_ALL_PLAYER_FUNDS()
}


function test_transport_pax_valid_route()
{
	local pl = player_x(0)
	local road_desc = way_desc_x.get_available_ways(wt_road, st_flat)[0]

	ASSERT_EQUAL(command_x.build_way(pl, coord3d(4, 2, 0), coord3d(4, 8, 0), road_desc, true), null)
	ASSERT_EQUAL(command_x(tool_build_station).work(pl, coord3d(4, 2, 0), "BusStop"), null)
	ASSERT_EQUAL(command_x(tool_build_station).work(pl, coord3d(4, 7, 0), "BusStop"), null)
	ASSERT_EQUAL(command_x.build_depot(pl, coord3d(4, 8, 0), building_desc_x("CarDepot")), null)

	local depot     = depot_x(4, 8, 0)
	local from_halt = halt_x.get_halt(coord3d(4, 7, 0), pl)
	local to_halt   = halt_x.get_halt(coord3d(4, 2, 0), pl)

	// create vehicle ...
	depot.append_vehicle(pl, convoy_x(0), vehicle_desc_x("Buessig"))
	local cnv = depot.get_convoy_list()[0]
	// ... with simple 2-entry schedule ...
	cnv.change_schedule(pl, create_simple_schedule(wt_road, [ coord3d(4, 7, 0), coord3d(4, 2, 0) ]))
	// and start the convoy
	depot.start_all_convoys(pl)

	// make sure that the graph linking all halts is updated
	// make sure that the graph linking and all halts is updated
	while(!halt_x.is_rerouting_finished()) {
		sleep()
	}

	{
		ASSERT_EQUAL(world.generate_goods(coord(3, 7), coord(3, 2), good_desc_x.passenger, 30), 1) // 1 == OK

		ASSERT_EQUAL(from_halt.waiting[0], 30)
		ASSERT_EQUAL(from_halt.get_freight_to_halt(good_desc_x.passenger, to_halt), 30)
		ASSERT_EQUAL(from_halt.get_freight_to_dest(good_desc_x.passenger, coord3d(3, 2, 0)), 30)
		ASSERT_EQUAL(from_halt.get_freight_to_dest(good_desc_x.passenger, coord3d(4, 2, 0)), 0)
		ASSERT_EQUAL(from_halt.happy[0], 30)
		ASSERT_EQUAL(from_halt.unhappy[0], 0)
		ASSERT_EQUAL(from_halt.noroute[0], 0)
		ASSERT_EQUAL(from_halt.walked[0], 0)
		ASSERT_EQUAL(from_halt.departed[0], 0)
		ASSERT_EQUAL(from_halt.arrived[0], 0)
		ASSERT_EQUAL(from_halt.convoys[0], 0)
	}

	while (from_halt.convoys[0] == 0) {
		sleep()
	}

	// Loading is not instant, so we need to make sure loading has finished
	while (from_halt.waiting[0] > 0) {
		sleep()
	}

	{
		ASSERT_EQUAL(from_halt.convoys[0], 1)
		ASSERT_EQUAL(from_halt.waiting[0], 0)
		ASSERT_EQUAL(from_halt.get_freight_to_halt(good_desc_x.passenger, to_halt), 0)
		ASSERT_EQUAL(from_halt.get_freight_to_dest(good_desc_x.passenger, coord3d(3, 2, 0)), 0)
		ASSERT_EQUAL(from_halt.get_freight_to_dest(good_desc_x.passenger, coord3d(4, 2, 0)), 0)
		ASSERT_EQUAL(from_halt.happy[0], 30)
		ASSERT_EQUAL(from_halt.unhappy[0], 0)
		ASSERT_EQUAL(from_halt.noroute[0], 0)
		ASSERT_EQUAL(from_halt.walked[0], 0)
		ASSERT_EQUAL(from_halt.departed[0], 30)
		ASSERT_EQUAL(from_halt.arrived[0], 0)
	}

	while (to_halt.convoys[0] == 0) {
		sleep()
	}

	// make sure unloading has finished
	while (to_halt.arrived[0] < 30) {
		sleep()
	}

	{
		ASSERT_EQUAL(to_halt.waiting[0], 0)
		ASSERT_EQUAL(to_halt.happy[0], 0)
		ASSERT_EQUAL(to_halt.unhappy[0], 0)
		ASSERT_EQUAL(to_halt.noroute[0], 0)
		ASSERT_EQUAL(to_halt.walked[0], 0)
		ASSERT_EQUAL(to_halt.departed[0], 0)
		ASSERT_EQUAL(to_halt.arrived[0], 30)
	}

	// clean up
	cnv.destroy(pl)
	sleep()
	sleep() // make sure the convoy is destroyed
	ASSERT_EQUAL(command_x(tool_remover).work(player_x(0), coord3d(4, 2, 0)), null)
	ASSERT_EQUAL(command_x(tool_remover).work(player_x(0), coord3d(4, 7, 0)), null)
	ASSERT_EQUAL(command_x(tool_remover).work(player_x(0), coord3d(4, 8, 0)), null)
	ASSERT_EQUAL(command_x(tool_remove_way).work(player_x(0), coord3d(4, 2, 0), coord3d(4, 8, 0), "" + wt_road), null)
	RESET_ALL_PLAYER_FUNDS()
}


function test_transport_mail_valid_route()
{
	local pl = player_x(0)
	local road_desc = way_desc_x.get_available_ways(wt_road, st_flat)[0]

	ASSERT_EQUAL(command_x.build_way(pl, coord3d(4, 2, 0), coord3d(4, 8, 0), road_desc, true), null)
	ASSERT_EQUAL(command_x(tool_build_station).work(pl, coord3d(4, 2, 0), "PostStop"), null)
	ASSERT_EQUAL(command_x(tool_build_station).work(pl, coord3d(4, 7, 0), "PostStop"), null)
	ASSERT_EQUAL(command_x.build_depot(pl, coord3d(4, 8, 0), building_desc_x("CarDepot")), null)

	local depot = depot_x(4, 8, 0)
	local from_halt = halt_x.get_halt(coord3d(4, 7, 0), pl)
	local to_halt   = halt_x.get_halt(coord3d(4, 2, 0), pl)

	// create vehicle ...
	depot.append_vehicle(pl, convoy_x(0), vehicle_desc_x("Posttransporter"))
	local cnv = depot.get_convoy_list()[0]
	// ... with simple 2-entry schedule ...
	cnv.change_schedule(pl, create_simple_schedule(wt_road, [ coord3d(4, 7, 0), coord3d(4, 2, 0) ]))
	// and start the convoy
	depot.start_all_convoys(pl)

	// make sure that the graph linking all halts is updated
	// make sure that the graph linking and all halts is updated
	while(!halt_x.is_rerouting_finished()) {
		sleep()
	}

	{
		ASSERT_EQUAL(world.generate_goods(coord(3, 7), coord(3, 2), good_desc_x.mail, 30), 1) // 1 == OK
	}

	sleep()
	sleep()

	{
		ASSERT_EQUAL(from_halt.waiting[0], 30)
		ASSERT_EQUAL(from_halt.get_freight_to_halt(good_desc_x.mail, to_halt), 30)
		ASSERT_EQUAL(from_halt.get_freight_to_dest(good_desc_x.mail, coord3d(3, 2, 0)), 30)
		ASSERT_EQUAL(from_halt.get_freight_to_dest(good_desc_x.mail, coord3d(4, 2, 0)), 0)
		ASSERT_EQUAL(from_halt.happy[0], 0)
		ASSERT_EQUAL(from_halt.unhappy[0], 0)
		ASSERT_EQUAL(from_halt.noroute[0], 0)
		ASSERT_EQUAL(from_halt.walked[0], 0)
		ASSERT_EQUAL(from_halt.departed[0], 0)
		ASSERT_EQUAL(from_halt.arrived[0], 0)
		ASSERT_EQUAL(from_halt.convoys[0], 0)
	}

	while (from_halt.convoys[0] == 0) {
		sleep()
	}

	// Loading is not instant, so we need to make sure loading has finished
	while (from_halt.waiting[0] > 0) {
		sleep()
	}

	{
		ASSERT_EQUAL(from_halt.convoys[0], 1)
		ASSERT_EQUAL(from_halt.waiting[0], 0)
		ASSERT_EQUAL(from_halt.get_freight_to_halt(good_desc_x.mail, to_halt), 0)
		ASSERT_EQUAL(from_halt.get_freight_to_dest(good_desc_x.mail, coord3d(3, 2, 0)), 0)
		ASSERT_EQUAL(from_halt.get_freight_to_dest(good_desc_x.mail, coord3d(4, 2, 0)), 0)
		ASSERT_EQUAL(from_halt.happy[0], 0)
		ASSERT_EQUAL(from_halt.unhappy[0], 0)
		ASSERT_EQUAL(from_halt.noroute[0], 0)
		ASSERT_EQUAL(from_halt.walked[0], 0)
		ASSERT_EQUAL(from_halt.departed[0], 30)
		ASSERT_EQUAL(from_halt.arrived[0], 0)
	}

	while (to_halt.convoys[0] == 0) {
		sleep()
	}

	// make sure unloading has finished
	while (to_halt.arrived[0] < 30) {
		sleep()
	}

	{
		ASSERT_EQUAL(to_halt.waiting[0], 0)
		ASSERT_EQUAL(to_halt.happy[0], 0)
		ASSERT_EQUAL(to_halt.unhappy[0], 0)
		ASSERT_EQUAL(to_halt.noroute[0], 0)
		ASSERT_EQUAL(to_halt.walked[0], 0)
		ASSERT_EQUAL(to_halt.departed[0], 0)
		ASSERT_EQUAL(to_halt.arrived[0], 30)
	}

	// clean up
	cnv.destroy(pl)
	sleep()
	sleep() // make sure the convoy is destroyed
	ASSERT_EQUAL(command_x(tool_remover).work(player_x(0), coord3d(4, 2, 0)), null)
	ASSERT_EQUAL(command_x(tool_remover).work(player_x(0), coord3d(4, 7, 0)), null)
	ASSERT_EQUAL(command_x(tool_remover).work(player_x(0), coord3d(4, 8, 0)), null)
	ASSERT_EQUAL(command_x(tool_remove_way).work(player_x(0), coord3d(4, 2, 0), coord3d(4, 8, 0), "" + wt_road), null)
	RESET_ALL_PLAYER_FUNDS()
}


function test_transport_freight_valid_route()
{
	local pl = player_x(0)
	local road_desc = way_desc_x.get_available_ways(wt_road, st_flat)[0]

	ASSERT_EQUAL(command_x.build_way(pl, coord3d(4, 2, 0), coord3d(4, 8, 0), road_desc, true), null)
	ASSERT_EQUAL(command_x.build_way(pl, coord3d(4, 7, 0), coord3d(5, 7, 0), road_desc, true), null)
	ASSERT_EQUAL(command_x(tool_build_station).work(pl, coord3d(4, 2, 0), "CarStop"), null)
	ASSERT_EQUAL(command_x(tool_build_station).work(pl, coord3d(5, 7, 0), "CarStop"), null)
	ASSERT_EQUAL(command_x.build_depot(pl, coord3d(4, 8, 0), building_desc_x("CarDepot")), null)

	local depot = depot_x(4, 8, 0)
	local from_halt = halt_x.get_halt(coord3d(5, 7, 0), pl)
	local to_halt   = halt_x.get_halt(coord3d(4, 2, 0), pl)

	// create vehicle ...
	depot.append_vehicle(pl, convoy_x(0), vehicle_desc_x("Kohletransporter"))
	local cnv = depot.get_convoy_list()[0]
	// ... with simple 2-entry schedule ...
	cnv.change_schedule(pl, create_simple_schedule(wt_road, [ coord3d(5, 7, 0), coord3d(4, 2, 0) ]))
	// and start the convoy
	depot.start_all_convoys(pl)

	// make sure that the graph linking all halts is updated
	// make sure that the graph linking and all halts is updated
	while(!halt_x.is_rerouting_finished()) {
		sleep()
	}

	{
		ASSERT_EQUAL(world.generate_goods(coord(3, 7), coord(3, 2), good_desc_x("Kohle"), 18), 1) // 1 == OK
	}

	sleep()
	sleep()

	{
		ASSERT_EQUAL(from_halt.waiting[0], 18)
		ASSERT_EQUAL(from_halt.get_freight_to_halt(good_desc_x("Kohle"), to_halt), 18)
		ASSERT_EQUAL(from_halt.get_freight_to_dest(good_desc_x("Kohle"), coord3d(3, 2, 0)), 18)
		ASSERT_EQUAL(from_halt.get_freight_to_dest(good_desc_x("Kohle"), coord3d(4, 2, 0)), 0)
		ASSERT_EQUAL(from_halt.happy[0], 0)
		ASSERT_EQUAL(from_halt.unhappy[0], 0)
		ASSERT_EQUAL(from_halt.noroute[0], 0)
		ASSERT_EQUAL(from_halt.walked[0], 0)
		ASSERT_EQUAL(from_halt.departed[0], 0)
		ASSERT_EQUAL(from_halt.arrived[0], 0)
		ASSERT_EQUAL(from_halt.convoys[0], 0)
	}

	while (from_halt.convoys[0] == 0) {
		sleep()
	}

	// Loading is not instant, so we need to make sure loading has finished
	while (from_halt.waiting[0] > 0) {
		sleep()
	}

	{
		ASSERT_EQUAL(from_halt.convoys[0], 1)
		ASSERT_EQUAL(from_halt.waiting[0], 0)
		ASSERT_EQUAL(from_halt.get_freight_to_halt(good_desc_x("Kohle"), to_halt), 0)
		ASSERT_EQUAL(from_halt.get_freight_to_dest(good_desc_x("Kohle"), coord3d(3, 2, 0)), 0)
		ASSERT_EQUAL(from_halt.get_freight_to_dest(good_desc_x("Kohle"), coord3d(4, 2, 0)), 0)
		ASSERT_EQUAL(from_halt.happy[0], 0)
		ASSERT_EQUAL(from_halt.unhappy[0], 0)
		ASSERT_EQUAL(from_halt.noroute[0], 0)
		ASSERT_EQUAL(from_halt.walked[0], 0)
		ASSERT_EQUAL(from_halt.departed[0], 18)
		ASSERT_EQUAL(from_halt.arrived[0], 0)
	}

	while (to_halt.convoys[0] == 0) {
		sleep()
	}

	// make sure unloading has finished
	while (to_halt.arrived[0] < 18) {
		sleep()
	}

	{
		ASSERT_EQUAL(to_halt.waiting[0], 0)
		ASSERT_EQUAL(to_halt.happy[0], 0)
		ASSERT_EQUAL(to_halt.unhappy[0], 0)
		ASSERT_EQUAL(to_halt.noroute[0], 0)
		ASSERT_EQUAL(to_halt.walked[0], 0)
		ASSERT_EQUAL(to_halt.departed[0], 0)
		ASSERT_EQUAL(to_halt.arrived[0], 18)
	}

	// clean up
	cnv.destroy(pl)
	sleep()
	sleep() // make sure the convoy is destroyed
	ASSERT_EQUAL(command_x(tool_remover).work(player_x(0), coord3d(4, 2, 0)), null)
	ASSERT_EQUAL(command_x(tool_remover).work(player_x(0), coord3d(5, 7, 0)), null)
	ASSERT_EQUAL(command_x(tool_remover).work(player_x(0), coord3d(4, 8, 0)), null)
	ASSERT_EQUAL(command_x(tool_remove_way).work(player_x(0), coord3d(4, 2, 0), coord3d(4, 8, 0), "" + wt_road), null)
	ASSERT_EQUAL(command_x(tool_remove_way).work(player_x(0), coord3d(5, 7, 0), coord3d(4, 7, 0), "" + wt_road), null)
	RESET_ALL_PLAYER_FUNDS()
}


function test_transport_buses_pass_on_bends()
{
	local pl = player_x(0)
	local road_desc = way_desc_x.get_available_ways(wt_road, st_flat)[0]

	// stop A at (3,2), then six tiles that each bend between SE and S,
	// whose chords from the tile before to the tile after are (1,1) or (-1,-1),
	// then stop B at (6,7)
	local corners = [ [3,1], [3,3], [4,3], [4,4], [5,4], [5,5], [6,5], [6,7] ]
	for (local i = 1; i < corners.len(); i++) {
		local a = corners[i-1], b = corners[i]
		ASSERT_EQUAL(command_x.build_way(pl, coord3d(a[0], a[1], 0), coord3d(b[0], b[1], 0), road_desc, true), null)
	}
	ASSERT_EQUAL(command_x(tool_build_station).work(pl, coord3d(3, 2, 0), "BusStop"), null)
	ASSERT_EQUAL(command_x(tool_build_station).work(pl, coord3d(6, 7, 0), "BusStop"), null)
	ASSERT_EQUAL(command_x.build_depot(pl, coord3d(3, 1, 0), building_desc_x("CarDepot")), null)

	// two buses between A and B, one setting off for each, so they meet head on
	local depot = depot_x(3, 1, 0)
	local stops = [ coord3d(3, 2, 0), coord3d(6, 7, 0) ]
	depot.append_vehicle(pl, convoy_x(0), vehicle_desc_x("Buessig"))
	depot.append_vehicle(pl, convoy_x(0), vehicle_desc_x("Buessig"))
	local convoys = depot.get_convoy_list()
	convoys[0].change_schedule(pl, create_simple_schedule(wt_road, [ stops[1], stops[0] ]))
	convoys[1].change_schedule(pl, create_simple_schedule(wt_road, [ stops[0], stops[1] ]))
	depot.start_all_convoys(pl)

	// a few round trips each, which they only make if they get past each other
	for (local i = 0; i < 20000; i++) {
		if (convoys[0].get_distance_traveled_total() >= 40  &&  convoys[1].get_distance_traveled_total() >= 40) {
			break
		}
		sleep()
	}
	ASSERT_TRUE(convoys[0].get_distance_traveled_total() >= 40)
	ASSERT_TRUE(convoys[1].get_distance_traveled_total() >= 40)

	// clean up
	foreach (cnv in convoys) {
		cnv.destroy(pl)
	}
	sleep()
	sleep() // make sure the convoys are destroyed
	ASSERT_EQUAL(command_x(tool_remover).work(pl, coord3d(3, 2, 0)), null)
	ASSERT_EQUAL(command_x(tool_remover).work(pl, coord3d(6, 7, 0)), null)
	ASSERT_EQUAL(command_x(tool_remover).work(pl, coord3d(3, 1, 0)), null)
	ASSERT_EQUAL(command_x(tool_remove_way).work(pl, coord3d(3, 1, 0), coord3d(6, 7, 0), "" + wt_road), null)
	ASSERT_EQUAL(pl.get_current_maintenance(), 0)
	RESET_ALL_PLAYER_FUNDS()
}
