/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 */

#include "synth_overlay.h"

#include "ground_desc.h"
#include "image.h"
#include "../display/simgraph.h"
#include "../simconst.h"
#include "../simdebug.h"
#include "../dataobj/environment.h"
#include "../dataobj/koord.h"

#include <cstring>


namespace synth_overlay {


bool prefer_over_pakset = true;


// Outline color in the runtime PIXVAL layout — RGB555 with no flag
// bit, bits 0..4 = B, 5..9 = G, 10..14 = R (matches the red_comp /
// green_comp / blue_comp macros in ground_desc.cc).  Values >= 0x8000
// are reserved for player-color slots and special palette entries
// (see simgraph16.cc register_image), so RGB pixels live in
// 0x0000..0x7FFF and are read directly without rgbmap remap.
//
// Bright yellow: R=31, G=31, B=0 → (31<<10) | (31<<5) = 0x7FE0.
static const PIXVAL OUTLINE_COLOR = 0x7FE0;

// Sentinel for "no pixel here" in the raw scratch buffer used during
// outline rasterisation.  The outline color is non-zero (0x7FE0), so
// 0 is never produced as an outline pixel and is safe to use.
static const PIXVAL NO_PIXEL = 0;

// One image_t per slope per half — index 0 = front half, 1 = back half
// (matches the @p background bool exposed in the public API).  Eager
// build in init(), get_marker() is then a flat table read; this also
// dodges thread-safety questions in the multi-threaded display path.
// Memory budget: hex outlines RLE-compress to ~1-2 KB each, so the
// full set fits comfortably under 3 MB.
static image_t* marker[2][slope_t::max_slopes];
static bool initialised = false;


static void free_all()
{
	for(  int half = 0;  half < 2;  half++  ) {
		for(  int s = 0;  s < slope_t::max_slopes;  s++  ) {
			delete marker[half][s];
			marker[half][s] = NULL;
		}
	}
}


// Bresenham line into a w*h scratch buffer; out-of-bounds pixels
// are clipped (silently dropped) so callers can pass vertices that
// extend above the image when corner heights are raised.
static void draw_line(PIXVAL* buf, sint32 w, sint32 h,
                      sint32 x0, sint32 y0, sint32 x1, sint32 y1,
                      PIXVAL color)
{
	const sint32 dx =  (x1 > x0 ? x1 - x0 : x0 - x1);
	const sint32 dy = -(y1 > y0 ? y1 - y0 : y0 - y1);
	const sint32 sx = x0 < x1 ? 1 : -1;
	const sint32 sy = y0 < y1 ? 1 : -1;
	sint32 err = dx + dy;
	while(  true  ) {
		if(  x0 >= 0  &&  x0 < w  &&  y0 >= 0  &&  y0 < h  ) {
			buf[y0 * w + x0] = color;
		}
		if(  x0 == x1  &&  y0 == y1  ) {
			break;
		}
		const sint32 e2 = 2 * err;
		if(  e2 >= dy  ) { err += dy; x0 += sx; }
		if(  e2 <= dx  ) { err += dx; y0 += sy; }
	}
}


// Encode a w*h scratch buffer into the simgraph16 RLE format.
// Each row is encoded as
//     <start_x> { <run_len> <run_pixels...> <skip> }* 0
// with the trailing 0 (skip) marking end-of-line; an empty row is
// the canonical [0][0][0].  The decoder lives in simgraph16.cc and
// drives RLE consumption from `len`, which the caller sets to the
// returned PIXVAL count.
static size_t encode_rle(const PIXVAL* buf, sint32 w, sint32 h, PIXVAL* out)
{
	PIXVAL* p = out;
	for(  sint32 y = 0;  y < h;  y++  ) {
		const PIXVAL* row = buf + y * w;
		sint32 x = 0;
		while(  x < w  &&  row[x] == NO_PIXEL  ) { x++; }
		if(  x >= w  ) {
			// empty row
			*p++ = 0;
			*p++ = 0;
			*p++ = 0;
			continue;
		}
		*p++ = (PIXVAL)x;
		while(  x < w  ) {
			const sint32 run_start = x;
			while(  x < w  &&  row[x] != NO_PIXEL  ) { x++; }
			const sint32 run_len = x - run_start;
			*p++ = (PIXVAL)run_len;
			for(  sint32 i = 0;  i < run_len;  i++  ) {
				*p++ = row[run_start + i];
			}
			const sint32 skip_start = x;
			while(  x < w  &&  row[x] == NO_PIXEL  ) { x++; }
			if(  x >= w  ) {
				*p++ = 0;  // end-of-line
				break;
			}
			*p++ = (PIXVAL)(x - skip_start);
		}
	}
	return p - out;
}


// Build the outline image for one slope, half (fg or bg).  The
// flat-top hex is inscribed in the template marker's bounding box
// (E and W vertices on the horizontal extremes at mid-y, NE/SE/NW/SW
// at quarter-width on the top and bottom rows), with each vertex
// lifted by `corner_h * tile_raster_scale_y(TILE_HEIGHT_STEP, w)` for
// slope-aware outlines.  Hex slopes share corners across 3 tiles —
// that's a terrain-storage problem (per AGENTS.md "per-vertex height
// storage") not a marker problem; the marker only ever shows one
// tile's view of its own corners and is consistent by construction.
//
// Lifted vertices may project above the template's top edge; the
// line drawer clips them.  Visually the outline tops can get cut off
// for double-height raised corners — not a concern for the cursor
// at single-height terrain, revisit if the cropping shows up.
static image_t* build_outline(const image_t* tmpl, slope_t::type slope, bool background)
{
	const sint32 w = tmpl->w;
	const sint32 h = tmpl->h;
	const sint32 mid_y = h / 2;
	const sint32 top_y = 0;
	const sint32 bot_y = h - 1;
	const sint16 lift = (sint16)tile_raster_scale_y(TILE_HEIGHT_STEP, w);

	// Vertex screen coords in image-local pixel space, ordered to
	// match hex_corner_t::type so the path tables below index in.
	// Y grows down; corner height lifts UP, so subtract.
	struct vertex_t { sint32 x, y; };
	const vertex_t v[hex_corner_t::count] = {
		{ w - 1,     mid_y - corner_e(slope)  * lift }, // E
		{ (w*3) / 4, bot_y - corner_se(slope) * lift }, // SE
		{  w    / 4, bot_y - corner_sw(slope) * lift }, // SW
		{ 0,         mid_y - corner_w(slope)  * lift }, // W
		{  w    / 4, top_y - corner_nw(slope) * lift }, // NW
		{ (w*3) / 4, top_y - corner_ne(slope) * lift }, // NE
	};

	// Vertex visit order around each half of the outline.  Front
	// half = three S-side edges; back half = three N-side edges.
	// E and W endpoints are shared between the halves.
	static const hex_corner_t::type front_path[4] = {
		hex_corner_t::E, hex_corner_t::SE, hex_corner_t::SW, hex_corner_t::W
	};
	static const hex_corner_t::type back_path[4] = {
		hex_corner_t::E, hex_corner_t::NE, hex_corner_t::NW, hex_corner_t::W
	};
	const hex_corner_t::type* path = background ? back_path : front_path;

	PIXVAL* buf = new PIXVAL[w * h];
	memset(buf, 0, w * h * sizeof(PIXVAL));

	for(  int i = 0;  i < 3;  i++  ) {
		const vertex_t& a = v[path[i]];
		const vertex_t& b = v[path[i + 1]];
		draw_line(buf, w, h, a.x, a.y, b.x, b.y, OUTLINE_COLOR);
	}

	// Encode into RLE.  Worst-case bound for alternating opaque/skip
	// is 3 PIXVALs per pixel + per-row header, comfortably below
	// w*h*2 + h*4.
	const size_t cap = (size_t)w * h * 2 + (size_t)h * 4 + 4;
	PIXVAL* tmp = new PIXVAL[cap];
	const size_t rle_len = encode_rle(buf, w, h, tmp);
	delete [] buf;

	image_t* img = new image_t(rle_len);
	memcpy(img->data, tmp, rle_len * sizeof(PIXVAL));
	delete [] tmp;

	img->w = (scr_coord_val)w;
	img->h = (scr_coord_val)h;
	img->x = tmpl->x;
	img->y = tmpl->y;
	img->zoomable = tmpl->zoomable;

	return img;
}


void init()
{
	if(  initialised  ) {
		// On world reset, gfx->free_all_images_above has already
		// cleared the gfx-side entries; we still own the image_t*
		// objects on the heap and free them here before rebuilding.
		free_all();
		initialised = false;
	}

	if(  ground_desc_t::marker == NULL  ) {
		dbg->warning("synth_overlay::init", "no Marker pakset; synth disabled, falling back to legacy projection");
		return;
	}
	const image_t* tmpl = ground_desc_t::marker->get_image_ptr(0);
	if(  tmpl == NULL  ||  tmpl->w <= 0  ||  tmpl->h <= 0  ) {
		dbg->warning("synth_overlay::init", "no usable Marker template (image 0); synth disabled");
		return;
	}

	for(  int s = 0;  s < slope_t::max_slopes;  s++  ) {
		for(  int half = 0;  half < 2;  half++  ) {
			image_t* img = build_outline(tmpl, (slope_t::type)s, half == 1);
			img->register_image();
			marker[half][s] = img;
		}
	}

	initialised = true;
	DBG_DEBUG("synth_overlay::init", "synthesised %d hex marker sprites (w=%d h=%d)",
	          slope_t::max_slopes * 2, tmpl->w, tmpl->h);
}


image_id get_marker(slope_t::type slope, bool background)
{
	if(  !initialised  ||  slope < 0  ||  slope >= slope_t::max_slopes  ) {
		return IMG_EMPTY;
	}
	const image_t* img = marker[background ? 1 : 0][slope];
	return img != NULL ? img->get_id() : IMG_EMPTY;
}


} // namespace synth_overlay
