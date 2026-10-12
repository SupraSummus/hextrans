/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 */

#include "gui_building.h"
#include "gui_image.h"
#include "../../descriptor/building_desc.h"
#include "../../display/hex_proj.h"
#include "../../tpl/vector_tpl.h"
#include "../../world/simworld.h"


gui_building_t::gui_building_t(const building_desc_t* d, int r)
{
	init(d, r);
}

void gui_building_t::init(const building_desc_t* d, int r)
{
	tl = scr_coord(0,0);
	br = scr_coord(0,0);
	desc = d;
	layout = r;
	clear_ptr_vector(images);

	if (desc == 0) {
		return;
	}

	// The tiles lie where the map puts them: tile (i, j) is the axial offset
	// (i, j) of the footprint, drawn back to front in rows of the hex lattice
	// (display/hex_proj.h).
	const scr_coord_val raster = gfx->get_base_tile_raster_width();
	const sint16 size_x = desc->get_x(layout);
	const sint16 size_y = desc->get_y(layout);
	hex_for_each_in_rows(0, (size_x - 1) + 2 * (size_y - 1), 0, 3 * (size_x - 1), [&](koord const k) {
		// the rows of the footprint's parallelogram only
		if (k.y < 0  ||  k.y >= size_y) {
			return;
		}
		image_id id = desc->get_tile(layout,k.x,k.y)->get_background(0,0,0);
		if (id == IMG_EMPTY) {
			id = desc->get_tile(layout,k.x,k.y)->get_foreground(0,0);
		}
		if (id != IMG_EMPTY) {
			gui_image_t *img = new gui_image_t(id, world()->get_active_player_nr());
			img->set_pos(scr_coord( hex_screen_dx(k.x, raster), hex_screen_dy(k.x, k.y, raster) ));
			images.append(img);
			// compute bounding box
			scr_rect r = gfx->get_base_image_offset(id);
			tl.clip_rightbottom(img->get_pos() + scr_coord(r.x, r.y));
			br.clip_lefttop(img->get_pos() + scr_coord(r.x + r.w, r.y+r.h));
		}
	});
}


gui_building_t::~gui_building_t()
{
	clear_ptr_vector(images);
}


scr_size gui_building_t::get_min_size() const
{
	scr_coord diff = br - tl;
	return scr_size(diff.x, diff.y);
}


void gui_building_t::draw(scr_coord offset)
{
	scr_coord pos = get_pos() + offset - tl;
	for(gui_image_t* img : images) {
		img->set_player_nr(world()->get_active_player_nr());
		img->draw(pos);
	}
}


bool gui_building_t::infowin_event(const event_t *ev) {
	if (IS_LEFTRELEASE(ev)) {
		call_listeners( (value_t)layout);
	}
	return false;
}
