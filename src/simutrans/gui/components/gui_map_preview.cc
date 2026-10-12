/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 */

#include "gui_map_preview.h"
#include "../../world/simworld.h"

gui_map_preview_t::gui_map_preview_t() :
	gui_component_t()
{
	map_data = NULL;
	set_size (scr_size( MAP_PREVIEW_SIZE_X,MAP_PREVIEW_SIZE_Y ));
}


// the previews are at least this long
#define L_PREVIEW_SIZE_MIN (16)


scr_size gui_map_preview_t::get_preview_size(const map_shape_t &shape)
{
	const float world_aspect = (float)shape.get_image_cols() / (float)shape.get_image_rows();

	scr_size size;
	if(  world_aspect > 1.0  ) {
		size.w = MAP_PREVIEW_SIZE_X-2;
		size.h = (sint16) max( (int)((float)size.w / world_aspect), L_PREVIEW_SIZE_MIN-2);
	}
	else {
		size.h = MAP_PREVIEW_SIZE_Y-2;
		size.w = (sint16) max( (int)((float)size.h * world_aspect), L_PREVIEW_SIZE_MIN-2);
	}
	return size;
}


koord gui_map_preview_t::get_preview_tile(const map_shape_t &shape, const scr_size &size, int x, int y)
{
	const sint32 Y = ((2*y+1)*shape.get_image_rows()) / (2*size.h);
	const sint32 X = ((2*x+1)*shape.get_image_cols()) / (2*size.w);
	return koord((sint16)X, (sint16)shape.get_tile_row(X, Y));
}
