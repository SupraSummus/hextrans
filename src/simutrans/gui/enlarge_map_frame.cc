/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 */

#include <string.h>

#include "enlarge_map_frame.h"
#include "minimap.h"
#include "welt.h"
#include "components/gui_divider.h"

#include "../simdebug.h"
#include "../world/simworld.h"
#include "simwin.h"
#include "../display/simimg.h"

#include "../dataobj/settings.h"
#include "../dataobj/translator.h"

#include "../simcolor.h"

#include "../display/simgraph.h"

#include "../utils/simrandom.h"


koord enlarge_map_frame_t::koord_from_rotation(settings_t const* const sets, sint16 const x, sint16 const y, sint16 const w, sint16 const h)
{
	koord offset( sets->get_origin_x(), sets->get_origin_y() );
	// the map turns by half turns only (karte_t::can_rotate90)
	switch( sets->get_rotation() ) {
		default:
		case 0: return offset+koord(x,y);
		case 3: return offset+koord(w-x,h-y);
	}
}


enlarge_map_frame_t::enlarge_map_frame_t() :
	gui_frame_t( translator::translate("enlarge map") ),
	sets(new settings_t(welt->get_settings())), // Make a copy.
	map(MAP_PREVIEW_SIZE_X-2, MAP_PREVIEW_SIZE_Y-2)
{
	// the copy holds the sides W, H, D; welt->get_size() is the bounding
	// rhombus, the same only for D = 1.  W and H grow, D stays.

	changed_number_of_towns = false;

	// Component creation
	set_table_layout(1,0);
	// top part: preview, maps size
	add_table(2,1);
	{
		// input fields
		add_table(2,3);
		{
			// map seed number label
			map_number_label.init();
			map_number_label.buf().printf("%s %d", translator::translate("2WORLD_CHOOSE"), welt->get_settings().get_map_number());
			map_number_label.update();
			add_component(&map_number_label);

			// Map X size edit: it does not shrink, and the extent W + D - 1 stays within 32766
			inp_x_size.init( sets->get_size_x(), sets->get_size_x(), 32766 - (sets->get_size_d() - 1), sets->get_size_x()>=512 ? 128 : 64, false );
			inp_x_size.add_listener(this);
			add_component( &inp_x_size );

			// Map size label
			size_label.init();
			size_label.buf().printf(translator::translate("Size (%d MB):"), 9999);
			size_label.update();
			add_component( &size_label );

			// Map size Y edit
			inp_y_size.init( sets->get_size_y(), sets->get_size_y(), 32766 - (sets->get_size_d() - 1), sets->get_size_y()>=512 ? 128 : 64, false );
			inp_y_size.add_listener(this);
			add_component( &inp_y_size );

			// the depth D of the hexagon cannot change: that moves every tile and
			// needs a 60° rotation step, see documentation/map-shape.md
			new_component<gui_empty_t>();
			depth_label.init();
			depth_label.buf().printf("%s %d", translator::translate("Depth D:"), sets->get_size_d());
			depth_label.update();
			add_component( &depth_label );
		}
		end_table();

		// Map preview (will be initialized in update_preview
		add_component( &map_preview );
	}
	end_table();

	// specify map parameters
	add_table(2,0);
	{
		// Number of towns
		new_component<gui_label_t>("5WORLD_CHOOSE");
		inp_number_of_towns.add_listener(this);
		inp_number_of_towns.init(abs(sets->get_city_count()), 0, 999);
		add_component( &inp_number_of_towns );

		// Town size
		new_component<gui_label_t>("Median Citizen per town");
		inp_town_size.add_listener(this);
		inp_town_size.set_limits(0,999999);
		inp_town_size.set_increment_mode(50);
		inp_town_size.set_value( sets->get_mean_citizen_count() );
		add_component( &inp_town_size );
	}
	end_table();

	new_component<gui_divider_t>();

	// start game
	start_button.init( button_t::roundbox | button_t::flexible, "enlarge map");
	start_button.add_listener( this );
	add_component( &start_button );

	welt_gui_t::update_memory(&size_label, sets);
	update_preview();

	reset_min_windowsize();
	set_windowsize(get_min_windowsize());
}


enlarge_map_frame_t::~enlarge_map_frame_t()
{
	delete sets;
}


/**
 * This method is called if an action is triggered
 */
bool enlarge_map_frame_t::action_triggered( gui_action_creator_t *comp,value_t v)
{
	if(comp==&inp_x_size) {
		sets->set_size_x( v.i );
		inp_x_size.set_increment_mode( v.i>=64 ? (v.i>=512 ? 128 : 64) : 8 );
		update_preview();
	}
	else if(comp==&inp_y_size) {
		sets->set_size_y( v.i );
		inp_y_size.set_increment_mode( v.i>=64 ? (v.i>=512 ? 128 : 64) : 8 );
		update_preview();
	}
	else if(comp==&inp_number_of_towns) {
		sets->set_city_count( v.i );
	}
	else if(comp==&inp_town_size) {
		sets->set_mean_citizen_count( v.i );
	}
	else if(comp==&start_button) {
		destroy_all_win( true );
		welt->enlarge_map(sets, NULL);
	}
	else {
		return false;
	}
	return true;
}


void enlarge_map_frame_t::draw(scr_coord pos, scr_size size)
{
	while (welt->get_settings().get_rotation() != sets->get_rotation()) {
		// map was rotated while we are active ... => rotate too!
		sets->rotate90();
		update_preview();
	}

	gui_frame_t::draw(pos, size);
}


/**
 * Calculate the new Map-Preview. Initialize the new RNG!
 *
 * The preview is the relief image frame of the new hexagon, as in the new-world
 * dialog (documentation/map-shape.md): the old map as it is, with a white outline
 * along its far sides, and the noise of the new part.
 */
void enlarge_map_frame_t::update_preview()
{
	// reset noise seed
	setsimrand(0xFFFFFFFF, welt->get_settings().get_map_number());

	// "welt" still knows the old shape. The new one is saved in "sets".
	const map_shape_t old_shape = welt->get_map_shape();
	const map_shape_t new_shape = sets->get_map_shape();
	const koord old_size = welt->get_size();

	const scr_size map_size = gui_map_preview_t::get_preview_size(new_shape);
	map.resize( map_size.w, map_size.h );

	const PIXVAL blank = gfx->palette_lookup(COL_BLACK);
	const PIXVAL outline = gfx->palette_lookup(COL_WHITE);
	// does pixel (x, y) show an added tile?
	const auto is_new = [&](int x, int y) {
		const koord tile = gui_map_preview_t::get_preview_tile(new_shape, map_size, x, y);
		return new_shape.contains(tile)  &&  !old_shape.contains(tile);
	};

	for(  int y=0;  y<map_size.h;  y++  ) {
		for(  int x=0;  x<map_size.w;  x++  ) {
			const koord tile = gui_map_preview_t::get_preview_tile(new_shape, map_size, x, y);
			if(  !new_shape.contains(tile)  ) {
				map.at(x,y) = blank;
			}
			else if(  old_shape.contains(tile)  ) {
				if(  is_new(x+1, y)  ||  is_new(x, y+1)  ||  is_new(x-1, y)  ||  is_new(x, y-1)  ) {
					// border
					map.at(x,y) = outline;
				}
				else {
					const sint16 height = welt->min_hgt( tile );
					map.at(x,y) = minimap_t::calc_height_color(height, sets->get_groundwater());
				}
			}
			else {
				// new part
				const sint16 height = karte_t::perlin_hoehe(sets, tile, hex_corner_t::E, old_size );
				map.at(x,y) = minimap_t::calc_height_color(height, sets->get_groundwater());
			}
		}
	}
	map_preview.set_map_data(&map);

	sets->heightfield = "";

	if(!changed_number_of_towns){// Interpolate number of towns.
		const sint64 new_area = new_shape.get_tile_count();
		const sint64 old_area = old_shape.get_tile_count();
		sint32 const towns = welt->get_settings().get_city_count();
		sets->set_city_count( (sint32)(towns * new_area / old_area - towns) );
		inp_number_of_towns.set_value(abs(sets->get_city_count()) );
	}

	// guess the new memory needed
	welt_gui_t::update_memory(&size_label, sets);
}
