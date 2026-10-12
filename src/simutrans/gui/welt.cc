/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 */

#include "welt.h"
#include "minimap.h"

#include "../simdebug.h"
#include "../world/simworld.h"
#include "simwin.h"
#include "../display/simimg.h"
#include "../simmesg.h"
#include "../simskin.h"
#include "../simversion.h"

#include "../builder/hausbauer.h"
#include "../builder/wegbauer.h"

#include "../descriptor/building_desc.h"

#include "../dataobj/height_map_loader.h"
#include "../dataobj/settings.h"
#include "../dataobj/loadsave.h"
#include "../dataobj/environment.h"
#include "../dataobj/translator.h"
#include "../dataobj/tabfile.h"

// just for their structure size ...
#include "../obj/way/schiene.h"
#include "../obj/baum.h"
#include "../world/simcity.h"
#include "../vehicle/vehicle.h"
#include "../player/simplay.h"
#include "../simconvoi.h"

#include "../simcolor.h"

#include "../display/simgraph.h"

#include "../sys/simsys.h"

#include "../tool/simmenu.h"

#include "../utils/simstring.h"
#include "../utils/simrandom.h"

#include "components/gui_divider.h"


#include "banner.h"
#include "sprachen.h"
#include "climates.h"
#include "settings_frame.h"
#include "load_relief_frame.h"
#include "messagebox.h"


// Local adjustment
#define L_PREVIEW_SIZE_MIN (16)

// the new-world sides are at least this long, as the square map was
#define L_SIZE_MIN (8)

// W' = W + D - 1 and H' = H + D - 1 are sint16 extents
#define L_EXTENT_MAX (32766)


/// The scale of the city, factory, attraction and river densities: the side of the square map with the tiles of the hexagon.
static double map_side(const settings_t *sets)
{
	return sets->get_map_shape().get_side_equivalent();
}


static sint32 size_step(sint32 v)
{
	return v >= 64 ? (v >= 512 ? 128 : 64) : 8;
}


/**
 * Fills the preview from the relief image frame of documentation/map-shape.md:
 * preview pixel (x, y) shows the image pixel (X, Y) at the centre of its cell,
 * which is the tile (X, shape.get_tile_row(X, Y)).  Pixels outside the
 * hexagon are blank.  @p colour_of(X, Y, r) gives the colour of an on-map pixel.
 */
template<class F>
static void fill_preview(array2d_tpl<PIXVAL> &map, const scr_size &map_size, const map_shape_t &shape, F colour_of)
{
	const PIXVAL blank = gfx->palette_lookup(COL_BLACK);
	const sint32 cols = shape.get_image_cols();
	const sint32 rows = shape.get_image_rows();
	for(  int y=0;  y<map_size.h;  y++  ) {
		const sint32 Y = ((2*y+1)*rows) / (2*map_size.h);
		for(  int x=0;  x<map_size.w;  x++  ) {
			const sint32 X = ((2*x+1)*cols) / (2*map_size.w);
			const sint32 r = shape.get_tile_row(X, Y);
			map.at(x,y) = shape.contains(X, r) ? colour_of(X, Y, r) : blank;
		}
	}
}


welt_gui_t::welt_gui_t() :
	gui_frame_t( translator::translate("Neue Welt" ) ),
	map(0,0)
{
	{
		// reread from simucon.tab(s) the settings and apply them
		tabfile_t simuconf;
		sets = new settings_t();
		*sets = env_t::default_settings;
		dr_chdir(env_t::base_dir);
		if (simuconf.open("config/simuconf.tab")) {
			sets->parse_simuconf(simuconf);
			sets->parse_colours(simuconf);
		}
		stadt_t::cityrules_init();
		dr_chdir(env_t::pak_dir.c_str());
		if (simuconf.open("config/simuconf.tab")) {
			sets->parse_simuconf(simuconf);
			sets->parse_colours(simuconf);
		}
		dr_chdir(env_t::user_dir);
		if (simuconf.open("simuconf.tab")) {
			sets->parse_simuconf(simuconf);
			sets->parse_colours(simuconf);
		}
		simuconf.close();
	}
	sets->beginner_mode = env_t::default_settings.get_beginner_mode();

	city_density       = ( sets->get_city_count()      ) ? map_side(sets) / sets->get_city_count()      : 0.0;
	industry_density   = ( sets->get_factory_count()       ) ? map_side(sets) / sets->get_factory_count()       : 0.0;
	attraction_density = ( sets->get_tourist_attractions() ) ? map_side(sets) / sets->get_tourist_attractions() : 0.0;
	river_density      = ( sets->get_river_number()        ) ? map_side(sets) / sets->get_river_number()        : 0.0;

	// playable era: roads bound the range, townhalls intersect it when present
	uint16 game_start = (way_builder_t::get_earliest_way(road_wt)->get_intro_year_month()+11)/12;
	uint16 game_ends  = (way_builder_t::get_latest_way(road_wt)->get_retire_year_month()+11)/12;

	vector_tpl<const building_desc_t*> const& townhalls = *hausbauer_t::get_list(building_desc_t::townhall);
	if(  !townhalls.empty()  ) {
		uint16 townhall_start = (townhalls[0]->get_intro_year_month()+11)/12;
		uint16 townhall_ends  = (townhalls[0]->get_retire_year_month()+11)/12;
		for(building_desc_t const* const desc : townhalls) {
			uint16 intro_year = (desc->get_intro_year_month()+11)/12;
			if(  intro_year<townhall_start  ) {
				townhall_start = intro_year;
			}
			uint16 retire_year = (desc->get_retire_year_month()+11)/12;
			if(  retire_year>townhall_ends  ) {
				townhall_ends = retire_year;
			}
		}
		game_start = max( game_start, townhall_start );
		game_ends  = min( game_ends,  townhall_ends  );
	}

	loaded_heightfield = load_heightfield = false;
	relief_w = relief_h = 0;
	sets->heightfield = "";

	//******************************************************************
	// Component creation
	set_table_layout(1,0);
	// top part: preview, maps size
	add_table(3,1);
	{
		// input fields
		add_table(2,0);
		{
			new_component<gui_label_t>("2WORLD_CHOOSE");
			inp_map_number.init( abs(sets->get_map_number()), 0, 0x7FFFFFFF, 1, true );
			inp_map_number.add_listener( this );
			add_component( &inp_map_number );

			// Map size label
			size_label.init();
			size_label.buf().printf(translator::translate("Size (%d MB):"), 99999);
			size_label.update();
			size_label.set_min_width(size_label.get_min_size().w); // Make sure to not make the component size too small when the window is opened with a small map size that is increased afterwards
			add_component( &size_label );

			add_table(5,0);
			{
				// the sides W, H and D of the hexagon, see documentation/map-shape.md
				inp_x_size.init( sets->get_size_x(), L_SIZE_MIN, L_EXTENT_MAX, size_step(sets->get_size_x()), false );
				inp_x_size.add_listener(this);
				add_component( &inp_x_size );

				new_component<gui_label_t>("x");

				inp_y_size.init( sets->get_size_y(), L_SIZE_MIN, L_EXTENT_MAX, size_step(sets->get_size_y()), false );
				inp_y_size.add_listener(this);
				add_component( &inp_y_size );

				new_component<gui_label_t>("x");

				inp_d_size.init( sets->get_size_d(), 1, L_EXTENT_MAX, size_step(sets->get_size_d()), false );
				inp_d_size.add_listener(this);
				add_component( &inp_d_size );
			}
			end_table();

			// while ticked, editing a side sets all three: a regular hexagon
			new_component<gui_empty_t>();
			equal_sides.init(button_t::square_state, "Equal sides");
			equal_sides.set_tooltip("All three sides the same: a regular hexagon");
			equal_sides.pressed = sets->get_size_x()==sets->get_size_y()  &&  sets->get_size_y()==sets->get_size_d();
			equal_sides.add_listener( this );
			add_component( &equal_sides );
		}
		end_table();

		new_component<gui_fill_t>();
		// Map preview (will be initialized in update_preview
		add_component( &map_preview );
	}
	end_table();

	// two buttons
	add_table(2,0)->set_force_equal_columns(true);
	{
		// Random map button
		random_map.init(button_t::roundbox | button_t::flexible, "Random map");
		random_map.set_tooltip("chooses a random map");
		random_map.add_listener( this );
		add_component( &random_map );

		// Load height map button
		load_map.init(button_t::roundbox | button_t::flexible, "Lade Relief");
		load_map.set_tooltip("load height data from file");
		load_map.add_listener( this );
		add_component( &load_map );
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

		// Intercity road length
		new_component<gui_label_t>("Intercity road len:");
		inp_intercity_road_len.add_listener(this);
		inp_intercity_road_len.set_limits(0,9999);
		inp_intercity_road_len.set_value( env_t::intercity_road_length );
		inp_intercity_road_len.set_increment_mode( env_t::intercity_road_length>=1000 ? 100 : 20 );
		add_component( &inp_intercity_road_len );

		// Factories
		new_component<gui_label_t>("No. of Factories");
		inp_other_industries.add_listener(this);
		inp_other_industries.set_limits(0,999);
		inp_other_industries.set_value(abs(sets->get_factory_count()) );
		add_component( &inp_other_industries );

		// Tourist attr.
		new_component<gui_label_t>("Tourist attractions");
		inp_tourist_attractions.add_listener(this);
		inp_tourist_attractions.set_limits(0,999);
		inp_tourist_attractions.set_value(abs(sets->get_tourist_attractions()) );
		add_component( &inp_tourist_attractions );

		// Use timeline checkbox
		use_intro_dates.init(button_t::square_state, "Use timeline start year");
		use_intro_dates.pressed = sets->get_use_timeline()&1;
		use_intro_dates.add_listener( this );
		add_component( &use_intro_dates );

		// Timeline year edit
		inp_intro_date.add_listener(this);
		inp_intro_date.set_limits(game_start,game_ends);
		inp_intro_date.set_increment_mode(10);
		inp_intro_date.set_value(abs(sets->get_starting_year()) );
		add_component( &inp_intro_date );

		// Use beginner mode checkbox

		use_beginner_mode.init(button_t::square_state, "Use beginner mode");
		use_beginner_mode.set_tooltip("Higher transport fees, crossconnect all factories");
		use_beginner_mode.pressed = sets->get_beginner_mode();
		use_beginner_mode.add_listener( this );
		add_component( &use_beginner_mode );

		add_table(3, 0);
		{
			new_component<gui_label_t>(translator::translate("Transport fees: +"));
			beginner_price_factor.init((env_t::default_settings.beginner_price_factor - 1000) / 10, -100, 25000, /*step_size=*/10, /*wrap=*/false);
			beginner_price_factor.enable(use_beginner_mode.pressed);
			beginner_price_factor.add_listener(this);
			add_component(&beginner_price_factor);
			new_component<gui_label_t>("%");
		}
		end_table();
	}
	end_table();

	new_component<gui_divider_t>();

	add_table(2,1)->set_force_equal_columns(true);
	{
		// Map settings button
		open_setting_gui.init(button_t::roundbox | button_t::flexible, "Setting");
		open_setting_gui.pressed = win_get_magic( magic_settings_frame_t );
		open_setting_gui.add_listener( this );
		add_component( &open_setting_gui );

		// Landscape settings button
		open_climate_gui.init(button_t::roundbox | button_t::flexible,"Climate Control");
		open_climate_gui.pressed = win_get_magic( magic_climate );
		open_climate_gui.add_listener( this );
		add_component( &open_climate_gui );
	}
	end_table();

	new_component<gui_divider_t>();

	add_table(2,0)->set_force_equal_columns(true);
	{
		// start game
		start_game.init(button_t::roundbox | button_t::flexible, "Starte Spiel");
		start_game.add_listener( this );
		add_component( &start_game );

		// return to menu
		return_menu.init(button_t::roundbox | button_t::flexible,"Return to menu");
		return_menu.add_listener( this );
		add_component( &return_menu );
	}
	end_table();

	update_size_limits();
	update_preview();
	update_memory(&size_label, sets);

	reset_min_windowsize();
	set_windowsize(get_min_windowsize());
	set_resizemode(gui_frame_t::diagonal_resize);
}


/**
 * Calculates preview from height map
 * @param filename name of heightfield file
 */
bool welt_gui_t::update_from_heightfield(const char *filename)
{
	DBG_MESSAGE("welt_gui_t::update_from_heightfield()", "%s", filename);

	const sint8 min_h = env_t::default_settings.get_minimumheight();
	const sint8 max_h = env_t::default_settings.get_maximumheight();

	height_map_loader_t hml(min_h, max_h, env_t::height_conv_mode);
	sint16 w, h;
	sint8 *h_field=NULL;

	if(hml.get_height_data_from_file(filename, (sint8)sets->get_groundwater(), h_field, w, h, false )) {
		// the image fixes the sides: its columns give W' = W+D-1, its rows H
		sint32 d = sets->get_size_d();
		map_shape_t shape = map_shape_t::from_image_size(w, h, d);
		if(  shape.h < 1  ) {
			// too flat for its width: no D helps
			free(h_field);
			create_win( new news_img("\nThe relief is too wide for its height.\n"), w_info, magic_none );
			return false;
		}
		if(  shape.w < 1  ) {
			// this D does not fit the relief: take the regular hexagon's
			d = (w+1)/2;
			shape = map_shape_t::from_image_size(w, h, d);
		}
		loaded_heightfield = true;
		relief_w = w;
		relief_h = h;
		sets->set_size(shape.w, shape.h);
		sets->set_size_d(d);
		update_densities();
		update_size_limits();
		update_size_fields();

		resize_preview();

		fill_preview( map, map_size, shape, [&](sint32 x, sint32 y, sint32) {
			return minimap_t::calc_height_color( h_field[x+y*w], sets->get_groundwater()-1 );
		} );
		map_preview.set_map_data(&map);
		free(h_field);
		return true;
	}

	return false;
}


// sets the new values for the number input filed for the densities
void welt_gui_t::update_densities()
{
	const double side = map_side(sets);
	if(  city_density!=0.0  ) {
		inp_number_of_towns.set_value( max( 1, (sint32)(0.5+side/city_density) ) );
		sets->set_city_count( inp_number_of_towns.get_value() );
	}
	if(  industry_density!=0.0  ) {
		inp_other_industries.set_value( max( 1, (sint32)(0.5+side/industry_density) ) );
		sets->set_factory_count( inp_other_industries.get_value() );
	}
	if(  attraction_density!=0.0  ) {
		inp_tourist_attractions.set_value( max( 1, (sint32)(0.5+side/attraction_density) ) );
		sets->set_tourist_attractions( inp_tourist_attractions.get_value() );
	}
	if(  river_density!=0.0  ) {
		sets->river_number = max( 1, (sint32)(0.5+side/river_density) );
		if(  climate_gui_t *climate_gui = (climate_gui_t *)win_get_magic( magic_climate )  ) {
			climate_gui->update_river_number( sets->get_river_number() );
		}
	}
}


void welt_gui_t::update_size_limits()
{
	const sint32 w = sets->get_size_x();
	const sint32 h = sets->get_size_y();
	const sint32 d = sets->get_size_d();

	if(  loaded_heightfield  ) {
		// the relief fixes W and H, D may rise to the one that leaves W = 1
		inp_x_size.set_limits( w, w );
		inp_y_size.set_limits( h, h );
		inp_d_size.set_limits( 1, relief_w );
	}
	else if(  equal_sides.pressed  ) {
		// the regular hexagon has the extent 2n-1
		inp_x_size.set_limits( L_SIZE_MIN, (L_EXTENT_MAX+1)/2 );
		inp_y_size.set_limits( L_SIZE_MIN, (L_EXTENT_MAX+1)/2 );
		inp_d_size.set_limits( L_SIZE_MIN, (L_EXTENT_MAX+1)/2 );
	}
	else {
		inp_x_size.set_limits( L_SIZE_MIN, L_EXTENT_MAX+1-d );
		inp_y_size.set_limits( L_SIZE_MIN, L_EXTENT_MAX+1-d );
		inp_d_size.set_limits( 1, L_EXTENT_MAX+1-max(w,h) );
	}
	// a relief is no regular hexagon
	equal_sides.enable( !loaded_heightfield );

	inp_x_size.set_increment_mode( size_step(w) );
	inp_y_size.set_increment_mode( size_step(h) );
	inp_d_size.set_increment_mode( size_step(d) );
}


void welt_gui_t::update_size_fields()
{
	inp_x_size.set_value( sets->get_size_x() );
	inp_y_size.set_value( sets->get_size_y() );
	inp_d_size.set_value( sets->get_size_d() );
	// the limits may have moved a side
	sets->set_size( inp_x_size.get_value(), inp_y_size.get_value() );
	sets->set_size_d( inp_d_size.get_value() );
}


void welt_gui_t::change_size(gui_numberinput_t *changed)
{
	// within the limits of the field
	const sint32 value = changed->get_value();

	if(  loaded_heightfield  ) {
		// W and H follow from the relief; D may change as long as W stays at least 1
		if(  changed == &inp_d_size  &&  map_shape_t::from_image_size(relief_w, relief_h, value).is_valid()  ) {
			sets->set_size_d( value );
			update_from_heightfield( sets->heightfield.c_str() );
		}
		// shows the sides again, also those of a refused edit
		update_size_fields();
		return;
	}

	if(  equal_sides.pressed  ) {
		sets->set_size( value, value );
		sets->set_size_d( value );
	}
	else if(  changed == &inp_x_size  ) {
		sets->set_size_x( value );
	}
	else if(  changed == &inp_y_size  ) {
		sets->set_size_y( value );
	}
	else {
		sets->set_size_d( value );
	}
	update_size_limits();
	update_size_fields();
	update_densities();
}


void welt_gui_t::update_memory(gui_label_buf_t *label, const settings_t* sets)
{
	// Calculate map memory: the planquadrat of each cell of the bounding rhombus,
	// the ground and trees of each tile of the hexagon
	const map_shape_t shape = sets->get_map_shape();
	const uint sx = shape.get_size_q();
	const uint sy = shape.get_size_r();
	const uint64 memory = (
		(uint64)sizeof(karte_t) +
		sizeof(player_t) * 8 +
		sizeof(convoi_t) * 1000 +
		(sizeof(schiene_t) + sizeof(vehicle_t)) * 10 * (sx + sy) +
		sizeof(stadt_t) * sets->get_city_count() +
		(
			sizeof(planquadrat_t) +
			sizeof(void*)*2
		) * (uint64)sx * (uint64)sy +
		(
			sizeof(grund_t) +
			sizeof(baum_t)*(sets->get_tree_distribution()!=settings_t::TREE_DIST_NONE) /* only one since a lot will be water */
		) * (uint64)shape.get_tile_count()
	) / (1024ll * 1024ll);

	label->buf().printf(translator::translate("Size (%d MB):"), memory);
	label->update();
}


/**
 * Calculate the new Map-Preview. Initialize the new RNG!
 */
void welt_gui_t::update_preview(bool load_heightfield)
{
	if(  loaded_heightfield  ||  load_heightfield) {
		loaded_heightfield = true;
		if(  update_from_heightfield(sets->heightfield.c_str())  ) {
			map_preview.set_map_data(&map);
			return;
		}
		// not a usable relief: back to the generated map
		loaded_heightfield = false;
		sets->heightfield = "";
		update_size_limits();
		update_size_fields();
	}

	resize_preview();

	setsimrand( 0xFFFFFFFF, sets->get_map_number() );

	// the size perlin_hoehe_loop passes, so a new world turned by 180°
	// (settings from a loaded turned game) previews what it generates
	fill_preview( map, map_size, sets->get_map_shape(), [&](sint32 x, sint32, sint32 r) {
		return minimap_t::calc_height_color(karte_t::perlin_hoehe( sets, koord(x,r), hex_corner_t::E, koord(0,0) ), sets->get_groundwater());
	} );
	sets->heightfield = "";
	map_preview.set_map_data(&map);
}


void welt_gui_t::resize_preview()
{
	// the preview is the relief image of the hexagon
	const map_shape_t shape = sets->get_map_shape();
	const float world_aspect = (float)shape.get_image_cols() / (float)shape.get_image_rows();

	if(  world_aspect > 1.0  ) {
		map_size.w = MAP_PREVIEW_SIZE_X-2;
		map_size.h = (sint16) max( (int)((float)map_size.w / world_aspect), L_PREVIEW_SIZE_MIN-2);
	}
	else {
		map_size.h = MAP_PREVIEW_SIZE_Y-2;
		map_size.w = (sint16) max( (int)((float)map_size.h * world_aspect), L_PREVIEW_SIZE_MIN-2);
	}
	map.resize( map_size.w, map_size.h );
}


/**
 * This method is called if an action is triggered
 */
bool welt_gui_t::action_triggered( gui_action_creator_t *comp,value_t v)
{
	if (!sets) {
		// new map has been already created => ignored late events
		return true;
	}

	// check for changed map (update preview for any event)
	int knr = inp_map_number.get_value(); //

	if(comp==&inp_map_number) {
		sets->heightfield = "";
		loaded_heightfield = false;
	}
	else if(comp==&inp_x_size  ||  comp==&inp_y_size  ||  comp==&inp_d_size) {
		// with a relief loaded only D can change
		change_size( static_cast<gui_numberinput_t *>(comp) );
	}
	else if(comp==&equal_sides) {
		if(  !loaded_heightfield  ) {
			equal_sides.pressed = !equal_sides.pressed;
			update_size_limits();
			if(  equal_sides.pressed  ) {
				// all three sides become W
				change_size( &inp_x_size );
			}
		}
	}
	else if (comp == &inp_number_of_towns) {
		sets->set_city_count(v.i);
		city_density = sets->get_city_count() ? map_side(sets) / sets->get_city_count() : 0.0;
	}
	else if (comp == &inp_town_size) {
		sets->set_mean_citizen_count(v.i);
	}
	else if (comp == &inp_intercity_road_len) {
		env_t::intercity_road_length = v.i;
		inp_intercity_road_len.set_increment_mode(v.i >= 1000 ? 100 : 20);
	}
	else if (comp == &inp_other_industries) {
		sets->set_factory_count(v.i);
		industry_density = sets->get_factory_count() ? map_side(sets) / sets->get_factory_count() : 0.0;
	}
	else if (comp == &inp_tourist_attractions) {
		sets->set_tourist_attractions(v.i);
		attraction_density = sets->get_tourist_attractions() ? map_side(sets) / sets->get_tourist_attractions() : 0.0;
	}
	else if (comp == &inp_intro_date) {
		sets->set_starting_year((sint16)(v.i));
	}
	else if (comp == &random_map) {
		knr = sim_async_rand(9999);
		inp_map_number.set_value(knr);
		sets->heightfield = "";
		loaded_heightfield = false;
	}
	else if (comp == &load_map) {
		// load relief
		loaded_heightfield = false;
		sets->heightfield = "";
		load_relief_frame_t* lrf = new load_relief_frame_t(sets);
		create_win(lrf, w_info, magic_load_t);

		const scr_coord new_pos{ (gfx->get_screen_size().w - lrf->get_windowsize().w - 10), env_t::iconsize.h };
		win_set_pos(lrf, new_pos);
		knr = sets->get_map_number(); // otherwise using cancel would not show the normal generated map again
	}
	else if (comp == &use_intro_dates) {
		// 0,1 should force setting to new game as well. don't allow to change
		// 2,3 allow to change
		if (sets->get_use_timeline() & 2) {
			// don't change bit1. bit1 affects loading saved game
			sets->set_use_timeline(sets->get_use_timeline() ^ 1);
			use_intro_dates.pressed = sets->get_use_timeline() & 1;
		}
	}
	else if (comp == &use_beginner_mode) {
		sets->beginner_mode = sets->get_beginner_mode() ^ 1;
		use_beginner_mode.pressed = sets->get_beginner_mode();

		beginner_price_factor.enable(use_beginner_mode.pressed);
	}
	else if (comp == &beginner_price_factor) {
		sets->beginner_price_factor = 1000 + 10 * beginner_price_factor.get_value();
	}
	else if (comp == &open_setting_gui) {
		gui_frame_t* sg = win_get_magic(magic_settings_frame_t);
		if (sg) {
			destroy_win(sg);
			open_setting_gui.pressed = false;
		}
		else {
			create_win({ 10, 40 }, new settings_frame_t(sets), w_info, magic_settings_frame_t);
			open_setting_gui.pressed = true;
		}
	}
	else if (comp == &open_climate_gui) {
		gui_frame_t* climate_gui = win_get_magic(magic_climate);
		if (climate_gui) {
			destroy_win(climate_gui);
			open_climate_gui.pressed = false;
		}
		else {
			climate_gui_t* cg = new climate_gui_t(sets);
			const scr_coord_val xoff = min(win_get_pos(this).x + this->size.w, gfx->get_screen_size().w - cg->get_windowsize().w);
			const scr_coord_val yoff = win_get_pos(this).y;
			create_win({ xoff, yoff }, cg, w_info, magic_climate);
			open_climate_gui.pressed = true;
		}
	}
	else if (comp == &start_game) {
		destroy_all_win(true);
		if (!loaded_heightfield) {
			env_t::default_settings.heightfield = "";
		}
		env_t::default_settings = *sets;
		welt->get_message()->clear();
		delete sets;
		sets = NULL;
		// save setting ...
		loadsave_t file;
		chdir(env_t::user_dir);
		if (file.wr_open("default.sve", loadsave_t::binary, 0, "settings only", SAVEGAME_VER_NR) == loadsave_t::FILE_STATUS_OK) {
			// save default setting
			env_t::default_settings.rdwr(&file);
			file.close();
		}
		tool_t::simple_tool[TOOL_WORK_WORLD]->set_default_param("n");
		welt->set_tool(tool_t::simple_tool[TOOL_WORK_WORLD], NULL);
	}
	else if(comp==&return_menu) {
		destroy_all_win(true);
		banner_t::show_banner();
	}

	if(  sets  &&  (comp==&inp_map_number  ||  comp==&random_map  ||  comp==&load_map)  ) {
		// these dropped a loaded relief, and with it the fixed W and H
		update_size_limits();
		update_size_fields();
	}

	if(knr>=0  &&  sets) {
		sets->map_number = knr;
		if(!loaded_heightfield) {
			update_preview();
		}
	}
	return true;
}


void welt_gui_t::draw(scr_coord pos, scr_size size)
{
	// Coordinates are relative to parent (TITLEHEIGHT **NOT** subtracted)
	cbuffer_t buf;

	// Update child controls before redraw
	if(!loaded_heightfield  && sets->heightfield.size()!=0) {
		if(update_from_heightfield(sets->heightfield.c_str())) {
			loaded_heightfield = true;
		}
		else {
			loaded_heightfield = false;
			sets->heightfield = "";
			update_size_limits();
			update_size_fields();
		}
	}

	// simulate toggle buttons
	open_setting_gui.pressed = win_get_magic( magic_settings_frame_t );
	open_climate_gui.pressed = false;

	if(  win_get_magic( magic_climate )  ) {
		open_climate_gui.pressed = true;
		// check if number was directly changed
		sint16 new_river_number = max( 1, (sint32)(0.5+map_side(sets)/river_density) );
		if(  sets->get_river_number() != new_river_number  ) {
			river_density = sets->get_river_number() ? map_side(sets) / sets->get_river_number() : 0.0;
		}
	}

	use_intro_dates.pressed = sets->get_use_timeline()&1;
	use_beginner_mode.pressed = sets->get_beginner_mode();

	update_memory(&size_label, sets);
	// draw child controls
	gui_frame_t::draw(pos, size);
}
