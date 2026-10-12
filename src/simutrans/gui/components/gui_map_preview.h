/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 */

#ifndef GUI_COMPONENTS_GUI_MAP_PREVIEW_H
#define GUI_COMPONENTS_GUI_MAP_PREVIEW_H


#include "gui_component.h"
#include "../../simcolor.h"
#include "../../dataobj/map_shape.h"
#include "../../display/simgraph.h"
#include "../../tpl/array2d_tpl.h"

#define MAP_PREVIEW_SIZE_X ((scr_coord_val)(64))
#define MAP_PREVIEW_SIZE_Y ((scr_coord_val)(64))

/**
 * A map preview component
 *
 */
class gui_map_preview_t : public gui_component_t
{

	private:
		array2d_tpl<PIXVAL> *map_data;

	public:
		gui_map_preview_t();

		void set_map_data(array2d_tpl<PIXVAL> *map_data_par) {
			map_data = map_data_par;
		}

		/**
		 * The previews of the new-world and the enlarge-map dialog show the
		 * relief image frame of the map shape, the physical plane in offset
		 * coordinates (documentation/map-shape.md): the size of the preview
		 * of a map of shape @p shape has the aspect of its image.
		 */
		static scr_size get_preview_size(const map_shape_t &shape);

		/**
		 * The tile that pixel (@p x, @p y) of a preview of size @p size shows
		 * of a map of shape @p shape: the image pixel at the centre of the
		 * pixel's cell is the tile (X, shape.get_tile_row(X, Y)).  The pixel
		 * is outside the hexagon unless shape.contains() holds for the tile.
		 */
		static koord get_preview_tile(const map_shape_t &shape, const scr_size &size, int x, int y);

		/**
		 * Draws the component.
		 */
		void draw(scr_coord offset) OVERRIDE {
			gfx->draw_box3d_clipped(pos.x + offset.x, pos.y + offset.y, size.w, size.h, gfx->palette_lookup(MN_GREY0), gfx->palette_lookup(MN_GREY4));

			if(map_data) {
				gfx->draw_array(pos.x + offset.x + 1, pos.y + offset.y + 1, map_data->get_width(), map_data->get_height(), map_data->to_array());
			}
		}

		scr_size get_min_size() const OVERRIDE { return scr_size(MAP_PREVIEW_SIZE_X, MAP_PREVIEW_SIZE_Y); }

		scr_size get_max_size() const OVERRIDE { return scr_size(MAP_PREVIEW_SIZE_X, MAP_PREVIEW_SIZE_Y); }
};

#endif
