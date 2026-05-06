/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 */

#include <stdio.h>

#include "../simdebug.h"
#include "../world/simworld.h"
#include "../display/simgraph.h"
#include "../simconst.h"
#include "spezial_obj_tpl.h"
#include "ground_desc.h"
#include "../dataobj/environment.h"

// Number of possible slope values under the 6-corner base-4 encoding.
// 4^6 = 4096.  Was 81 under square 4-corner base-3.
const int totalslopes = slope_t::max_slopes;


/****************************************************************************************************
* some functions for manipulations/blending images
* maybe they should be put in their own module, even though they are only used here ...
*/

#if COLOUR_DEPTH != 0
#define red_comp(pix)    (((pix)>>10)&0x001f)
#define green_comp(pix)   (((pix)>>5)&0x001f)
#define blue_comp(pix)         ((pix)&0x001f)
#endif


/* combines a texture and a lightmap
 * just weights all pixels by the lightmap
 * @param binary if true, then a binary decision is made: if lightmap is grey then take original pixel, if not set to black
 */
static image_t* create_textured_tile(const image_t* image_lightmap, const image_t* image_texture, bool binary = false)
{
	if(  image_lightmap == NULL  ) {
		image_t *image_dest = image_t::create_single_pixel();
		image_dest->register_image();
		return image_dest;
	}

	image_t *image_dest = image_lightmap->copy_rotate(0);
#if COLOUR_DEPTH != 0
	PIXVAL* dest = image_dest->get_data();

	PIXVAL const* const texture = image_texture->get_data();
	sint16        const x_y     = image_texture->get_pic()->w;
	// now mix the images
	for (int j = 0; j < image_dest->get_pic()->h; j++) {
		sint32 x = *dest++;
		assert(x >= 0);
		const sint32 offset = (image_dest->get_pic()->y + j - image_texture->get_pic()->y) * (x_y + 3) + 2; // position of the pixel in a rectangular map
		do
		{
			sint16 runlen = *dest++;
			assert(runlen >= 0);
			for(int i=0; i<runlen; i++) {
				assert(offset+x>0);
				uint16 mix = texture[offset+x];

				if (!binary) {
					uint16 grey = *dest;
					uint16 rc = (red_comp(grey)*red_comp(mix))/16;
					if(rc>=32) {
						rc = 31;
					}
					uint16 gc = (green_comp(grey)*green_comp(mix))/16;
					if(gc>=32) {
						gc = 31;
					}
					uint16 bc = (blue_comp(grey)*blue_comp(mix))/16;
					if(bc>=32) {
						bc = 31;
					}
					*dest++ = (rc<<10) | (gc<<5) | bc;
				}
				else {
					if (*dest) { *dest = mix;}
					dest++;
				}
				x ++;
			}
			x += *dest;
		} while(  (*dest++)!=0 );
	}
	assert(dest - image_dest->get_data() == (ptrdiff_t)image_dest->get_pic()->len);
#else
	(void)image_texture;
	(void)binary;
#endif
	image_dest->register_image();
	return image_dest;
}


// copy ref texture, copy pixels from image into new texture
static image_t* create_texture_from_tile(const image_t* image, const image_t* ref)
{
	if(  image == NULL  ||  image->get_pic()->w < 2  ) {
		image_t *image_dest = image_t::create_single_pixel();
		return image_dest;
	}
	// assumes ref is texture image with no clear runs, full rows.
	image_t *image_dest = image_t::copy_image(*ref);
	PIXVAL *const sp2 = image_dest->get_data();

	assert(ref->w == ref->y + ref->h  &&  ref->x == 0);

	const sint32 ref_w = ref->w;
	const sint32 height= image->get_pic()->h;

	// decode image and put it into dest
	const PIXVAL* sp = image->get_data();

	for(int y = 0;  y < height;  y++  ) {

		int x = image->x;
		uint16 runlen = *sp++;

		do {
			// we start with a clear run
			x += runlen;

			// now get colored pixels
			runlen = (*sp++);

			for(uint16 i=0; i<runlen; i++) {
				PIXVAL p = *sp++;

				// macro to copy pixels into rle-encoded image, with range check
#				define copypixel(xx, yy) \
				if (ref->y <= (yy)  &&  (yy) < ref->h  &&  0 <= (xx)  &&  (xx) < ref_w) { \
					size_t const index = (ref_w + 3) * (yy - ref->y) + xx + 2; \
					assert(index < image_dest->len); \
					sp2[index] = p; \
				}
				/* Put multiple copies into dest image
				 *
				 * image is assumed to be tile shaped,
				 * and is copied four times to cover tiles of neighboring tiles.
				 *
				 * copy +   copy
				 * | /     \  |
				 * +  image   +
				 * | \     /  |
				 * copy +   copy
				 *
				 * ref image is assumed to be rectangular,
				 * it is used to fill holes due to missing pixels in image.
				 */
				copypixel(x, y + image->y);
				copypixel(x + ref_w/2, y + image->y + ref_w/4);
				copypixel(x - ref_w/2, y + image->y + ref_w/4);
				copypixel(x + ref_w/2, y + image->y - ref_w/4);
				copypixel(x - ref_w/2, y + image->y - ref_w/4);

				x++;
			}
		} while(  (runlen = *sp++)  );
	}
	// image_dest not registered
	return image_dest;
#undef copypixel
}

/****************************************************************************************************
* the real textures are registered/calculated below
*/

karte_t *ground_desc_t::world = NULL;


// 4-corner-canonical sprite-imgnr lookup: for each of the 14
// distinct single-height 4-corner slopes plus flat, return the
// pakset's 0..14 sprite index.  Anything else returns 0xFF — used
// only by the sprite-generation loop below to filter out
// non-canonicals.  Runtime callers go through
// `project_to_square_sprite`, which projects every 6-corner slope
// onto one of these canonicals first.
//
// `slope_t::all_up_two` is the bridgehead / flat-top sentinel; it
// projects to "all 4 square corners up", which has no dedicated
// sprite — we map it to the flat sprite (0) so it renders as a flat
// tile at the raised altitude.  Old square code skipped all_up_two
// entirely (sprite loop bound 79 not 80), which incidentally meant
// climate-sprite lookup read past the end for that slope.
static uint8 slopetable(slope_t::type slope)
{
	switch (slope) {
		case slope_t::flat:                                                   return 0;
		case slope_t::all_up_two:                                             return 0;
		case slope_t::raised_SW:                                              return 1;
		case slope_t::raised_SE:                                              return 2;
		case slope_t::raised_SE + slope_t::raised_SW:                         return 3;  // slope_t::north
		case slope_t::raised_NE:                                              return 4;
		case slope_t::raised_NE + slope_t::raised_SW:                         return 5;  // NE+SW
		case slope_t::raised_NE + slope_t::raised_SE:                         return 6;  // slope_t::west
		case slope_t::raised_NE + slope_t::raised_SE + slope_t::raised_SW:    return 7;
		case slope_t::raised_NW:                                              return 8;
		case slope_t::raised_NW + slope_t::raised_SW:                         return 9;  // slope_t::east
		case slope_t::raised_NW + slope_t::raised_SE:                         return 10; // NW+SE
		case slope_t::raised_NW + slope_t::raised_SE + slope_t::raised_SW:    return 11;
		case slope_t::raised_NW + slope_t::raised_NE:                         return 12; // slope_t::south
		case slope_t::raised_NW + slope_t::raised_NE + slope_t::raised_SW:    return 13;
		case slope_t::raised_NW + slope_t::raised_NE + slope_t::raised_SE:    return 14;
		default: return 0xFF;
	}
}


// Project any 6-corner slope onto one of the pakset's 15 single-height
// sprite indices.  `slope_t::project_to_square` does the lossy 6→4
// math; the table maps the 4-corner result onto a sprite.  All 4
// square corners raised has no dedicated sprite (reached by
// all_up_one, all_up_two, and any hex slope that saturates the
// projection) — render those as flat at the raised altitude.
uint8 ground_desc_t::project_to_square_sprite(slope_t::type slope)
{
	const uint8 imgnr = slopetable(slope_t::project_to_square(slope));
	return imgnr == 0xFF ? 0 : imgnr;
}


// since we only use valid slope (to gain some more image slots) we use this lookup table
// 255 slopes are invalid
/* for double slope it should look like this, and for single slope like above
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
	16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
	32, 33, 34, 35, 36, 37, 38, 39, 255, 255, 40, 255, 255, 41, 42, 43,
	44, 255, 255, 45, 255, 255, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55,
	56, 57, 58, 255, 255, 59, 255, 255, 60, 61, 62, 63, 255, 255, 64, 255, 255
*/
uint16 doubleslope_to_imgnr[totalslopes];


// how many animation stages we got for waves
uint16 ground_desc_t::water_animation_stages = 1;
sint16 ground_desc_t::water_depth_levels = 0;

// are double_grounds available in this pakset ?
bool ground_desc_t::double_grounds = true;

static const ground_desc_t* boden_texture            = NULL;
static const ground_desc_t* hex_light_map            = NULL;
static const ground_desc_t* transition_water_texture = NULL;
static const ground_desc_t* transition_slope_texture = NULL;
const ground_desc_t *ground_desc_t::shore = NULL;
const ground_desc_t *ground_desc_t::fundament = NULL;
const ground_desc_t *ground_desc_t::slopes = NULL;
const ground_desc_t *ground_desc_t::fences = NULL;
const ground_desc_t *ground_desc_t::marker = NULL;
const ground_desc_t *ground_desc_t::borders = NULL;
const ground_desc_t *ground_desc_t::sea = NULL;
const ground_desc_t *ground_desc_t::outside = NULL;

static special_obj_tpl<ground_desc_t> const grounds[] = {
	{ &ground_desc_t::shore,     "Shore"          },
	{ &boden_texture,            "ClimateTexture" },
	{ &hex_light_map,            "LightTexture"   },
	{ &transition_water_texture, "ShoreTrans"     },
	{ &transition_slope_texture, "SlopeTrans"     },
	{ &ground_desc_t::fundament, "Basement"       },
	{ &ground_desc_t::slopes,    "Slopes"         },
	{ &ground_desc_t::fences,    "Fence"          },
	{ &ground_desc_t::marker,    "Marker"         },
	{ &ground_desc_t::borders,   "Borders"        },
	{ &ground_desc_t::sea,       "Water"          },
	{ &ground_desc_t::outside,   "Outside"        },
	{ NULL, NULL }
};

// the water and seven climates
static const char* const climate_names[MAX_CLIMATES] =
{
	"sea", "desert", "tropic", "mediterran", "temperate", "tundra", "rocky", "arctic"
};

// from this number on there will be all ground images
// i.e. 15 times slopes + 7
image_id ground_desc_t::image_offset = IMG_EMPTY;
static const uint8 number_of_climates = 7;
static slist_tpl<image_t *> ground_image_list;
static image_id climate_image[32], water_image;
// SlopeTrans alpha images now live in the pakset descriptor itself
// — the bake under `landscape/grounds/texture-slope/` ships one
// `Image[<slope_t>][<corner_mask>]` cell per `(slope, mask)` against
// `LightTexture`'s silhouette, so `get_alpha_tile(slope, corners)` is
// a direct `transition_slope_texture->get_image(...)` lookup with no
// runtime cache.


/*
 *      called every time an object is read
 *      the object will be assigned according to its name
 */
bool ground_desc_t::register_desc(const ground_desc_t *desc)
{
	if(strcmp("Outside", desc->get_name())==0) {
		image_t const* const image = desc->get_child<image_array_t>(2)->get_image(0,0);
		dbg->message("ground_desc_t::register_desc()", "setting raster width to %i", image->get_pic()->w);
		gfx->set_base_raster_width(image->get_pic()->w);
	}
	// find out water animation stages
	if(strcmp("Water", desc->get_name())==0) {
		water_animation_stages = 0;
		while(  desc->get_image(0, water_animation_stages)!=IMG_EMPTY  ) {
			DBG_MESSAGE( "water", "image(0,%i)=%u", water_animation_stages, desc->get_image(0, water_animation_stages) );
			water_animation_stages ++;
		}
		// then ignore all ms settings
		if(water_animation_stages==1) {
			env_t::water_animation = 0;
		}
		water_depth_levels = desc->get_child<image_array_t>(2)->get_count()-2;
		if(water_depth_levels<=0) {
			water_depth_levels = 0;
		}
	}
	return ::register_desc(grounds, desc);
}


/*
 * called after loading; usually checks for completeness
 * however, we have to calculate all textures
 * and put them into images
 */
bool ground_desc_t::successfully_loaded()
{
	DBG_MESSAGE("ground_desc_t::successfully_loaded()","boden");
	return ::successfully_loaded(grounds+1);
}


/* returns the untranslated name of the matching climate
 */
char const* ground_desc_t::get_climate_name_from_bit(climate n)
{
	return n<MAX_CLIMATES ? climate_names[n] : NULL;
}


void ground_desc_t::init_ground_textures(karte_t *world)
{
	ground_desc_t::world = world;

	DBG_DEBUG("ground_desc_t::init_ground_textures()", "Calculating ground textures ...");

	// free old ones
	if(image_offset!=IMG_EMPTY) {
		gfx->free_all_images_above(image_offset);
	}
#if COLOUR_DEPTH != 0
	while (!ground_image_list.empty()) {
		delete ground_image_list.remove_first();
	}
#endif

	// not the wrong tile size?
	assert(boden_texture->get_image_ptr(0)->get_pic()->w == ground_desc_t::outside->get_image_ptr(0)->get_pic()->w);
	const ground_desc_t* const ground_light_map = hex_light_map;

	// SlopeTrans now ships one alpha cell per (slope, corner_mask)
	// against `LightTexture`'s silhouette — the per-slope rotation
	// table + diamond-unwarp `create_alpha_tile` projection that lived
	// here under the square pakset are gone; lookups in
	// `get_alpha_tile` just call `transition_slope_texture->get_image`
	// directly.  The init pass below only needs to populate
	// `doubleslope_to_imgnr` (slope-canonicalisation index for the
	// generated water / climate / snow tiles) — the SlopeTrans
	// silhouette invariant is checked by the tripwire just below the
	// shore-side one further down.
	image_t *final_tile = NULL;

	doubleslope_to_imgnr[0] = 0;
	for(  int slope = 1, slopeimgnr=1;  slope < totalslopes;  slope++  ) {
		doubleslope_to_imgnr[slope] = 255;

		if(  slope != slope_t::all_up_two  &&  slope_t::is_all_up(slope)  ) {
			// no need to initialize unneeded slopes — all_up_two is
			// kept as a special case (bridgehead / flat-top).
			continue;
		}

		if(  ground_light_map->get_image_ptr((uint16)slope) == NULL  ) {
			// Sparse hex paksets intentionally leave absent slopes as
			// empty slots.  Missing display-time slopes trip in the
			// lookup helpers below rather than falling back to square
			// lightmaps.
			continue;
		}

		doubleslope_to_imgnr[slope] = slopeimgnr++;
	}

	// from here on the images are generated by us => deletion also by us then
	image_offset = gfx->get_image_count();
	DBG_MESSAGE("ground_desc_t::init_ground_textures()","image_offset=%d", image_offset );

	// water images for water and overlay
	water_image = image_offset;

	image_t **water_stage_texture = new image_t*[water_animation_stages];
	for(uint16 stage = 0; stage < water_animation_stages; stage++) {
		water_stage_texture[stage] = create_texture_from_tile(sea->get_image_ptr(0 /*depth*/, stage), boden_texture->get_image_ptr(water_climate));
	}
	for(  int dslope = 0;  dslope < totalslopes - 1;  dslope++  ) {
		for(uint16 stage = 0; stage < water_animation_stages; stage++) {
			if(  doubleslope_to_imgnr[dslope] != 255  ) {
				final_tile = create_textured_tile( ground_light_map->get_image_ptr( dslope ), water_stage_texture[stage], true);
				ground_image_list.append( final_tile );
			}
		}
	}
	for(uint16 stage = 0; stage < water_animation_stages; stage++) {
		delete water_stage_texture[stage];
	}
	delete [] water_stage_texture;

	// now the other transitions
	for(  int i=0;  i < number_of_climates;  i++  ) {
		// normal tile (no transition, not snow)
		climate_image[i] = gfx->get_image_count();
		for(  int dslope = 0;  dslope < totalslopes - 1;  dslope++  ) {
			if(  doubleslope_to_imgnr[dslope] != 255  ) {
				final_tile = create_textured_tile( ground_light_map->get_image_ptr( dslope ), boden_texture->get_image_ptr( i+1 ) );
				ground_image_list.append( final_tile );
			}
		}
	}
	// finally full snow
	climate_image[number_of_climates] = final_tile->get_id() + 1;
	for(  int dslope = 0;  dslope < totalslopes - 1;  dslope++  ) {
		if(  doubleslope_to_imgnr[dslope] != 255  ) {
			final_tile = create_textured_tile( ground_light_map->get_image_ptr( dslope ), boden_texture->get_image_ptr( arctic_climate ) );
			ground_image_list.append( final_tile );
		}
	}

	// Shore alpha (`get_beach_tile`) and slope alpha
	// (`get_alpha_tile`) both read pakset descriptors directly:
	// `texture-shore/render.py` and `texture-slope/render.py` share
	// `LightTexture`'s silhouette via `hex_synth.silhouette_mask`, so
	// each populated `(slope, mask)` cell already has an RLE
	// footprint identical to the matching `LightTexture[slope]` cell
	// and `draw_alpha` walks both streams in lockstep without a
	// runtime normalisation cache.  Tripwire both invariants once at
	// startup so future pakset drift fails loudly instead of
	// reintroducing the alpha-renderer overread that motivated the
	// retired cache.
	for(  int dslope = 0;  dslope < totalslopes - 1;  dslope++  ) {
		const image_t* const lightmap = ground_light_map->get_image_ptr( dslope );
		if(  lightmap == NULL  ) {
			continue;
		}
		for(  int corner_mask = 1;  corner_mask < (1 << hex_corner_t::count);  corner_mask++  ) {
			const image_t* const shore_alpha = transition_water_texture->get_image_ptr((uint16)dslope, (uint16)corner_mask);
			if(  shore_alpha != NULL  ) {
				if(  shore_alpha->w != lightmap->w  ||  shore_alpha->h != lightmap->h
				  ||  shore_alpha->x != lightmap->x  ||  shore_alpha->y != lightmap->y
				  ||  shore_alpha->len != lightmap->len  ) {
					dbg->fatal("ground_desc_t::init_ground_textures",
						"ShoreTrans[%d][%d] shape (x=%d y=%d w=%d h=%d len=%lu) "
						"differs from LightTexture[%d] (x=%d y=%d w=%d h=%d len=%lu); "
						"pakset shore baker must share LightTexture's silhouette.",
						dslope, corner_mask,
						shore_alpha->x, shore_alpha->y, shore_alpha->w, shore_alpha->h,
						(unsigned long)shore_alpha->len,
						dslope,
						lightmap->x, lightmap->y, lightmap->w, lightmap->h,
						(unsigned long)lightmap->len);
				}
			}
			const image_t* const slope_alpha = transition_slope_texture->get_image_ptr((uint16)dslope, (uint16)corner_mask);
			if(  slope_alpha != NULL  ) {
				if(  slope_alpha->w != lightmap->w  ||  slope_alpha->h != lightmap->h
				  ||  slope_alpha->x != lightmap->x  ||  slope_alpha->y != lightmap->y
				  ||  slope_alpha->len != lightmap->len  ) {
					dbg->fatal("ground_desc_t::init_ground_textures",
						"SlopeTrans[%d][%d] shape (x=%d y=%d w=%d h=%d len=%lu) "
						"differs from LightTexture[%d] (x=%d y=%d w=%d h=%d len=%lu); "
						"pakset slope baker must share LightTexture's silhouette.",
						dslope, corner_mask,
						slope_alpha->x, slope_alpha->y, slope_alpha->w, slope_alpha->h,
						(unsigned long)slope_alpha->len,
						dslope,
						lightmap->x, lightmap->y, lightmap->w, lightmap->h,
						(unsigned long)lightmap->len);
				}
			}
		}
	}

	for(  int slope = 1;  slope < totalslopes;  slope++  ) {
		if(  doubleslope_to_imgnr[slope] != 255  ) {
			continue;
		}

		slope_t::type hex_slope = (slope_t::type)slope;
		if(  slope <= LEGACY_SLOPE4_MAX  ) {
			hex_slope = slope_from_legacy_slope4_table((sint16)slope);
		}
		hex_slope = slope_t::lower_min_corner(hex_slope);
		if(  hex_slope >= 0
		  &&  hex_slope < totalslopes
		  &&  hex_slope != slope
		  &&  doubleslope_to_imgnr[hex_slope] != 255  ) {
			doubleslope_to_imgnr[slope] = doubleslope_to_imgnr[hex_slope];
		}
	}

	//dbg->message("ground_desc_t::calc_water_level()", "Last image nr %u", final_tile->get_pic()->imageid);
	DBG_DEBUG("ground_desc_t::init_ground_textures()", "Init ground textures successful");
}


/* returns a ground image for all ground tiles
 * current is the current climate
 * transition is true, if the next level is another climate
 * snow is true, if above slowline
 *
 * Since not all of the climates are used in their numerical order, we use a
 * private (static table "height_to_texture_climate" for lookup)
 */
// Ground lookup.  LightTexture owns ground imagery; missing slots
// resolve to IMG_EMPTY so paksets that don't ship a hex-aware
// LightTexture (e.g. stock pak64) still boot and render the slopes
// they do cover.
static image_id pick_ground_image(slope_t::type slope, sint16 climate_nr)
{
	if(  slope < 0  ||  slope >= totalslopes  ||  doubleslope_to_imgnr[slope] == 255  ) {
		return IMG_EMPTY;
	}
	return climate_image[climate_nr] + doubleslope_to_imgnr[slope];
}


static void require_normalized_ground_slope(slope_t::type slope, const char *caller)
{
	const uint8 h = slope_t::min_corner_height(slope);
	if(  h != 0  ) {
		dbg->fatal(caller, "non-normalized ground slope %d has common corner height %u", slope, h);
	}
}


image_id ground_desc_t::get_ground_tile(grund_t *gr)
{
	slope_t::type slope = gr->get_grund_hang();
	require_normalized_ground_slope(slope, "ground_desc_t::get_ground_tile");
	sint16 height = gr->get_hoehe();
	koord k = gr->get_pos().get_2d();
	const sint16 tile_h = height - world->get_water_hgt(k);
	if(  tile_h < 0  ||  (tile_h == 0  &&  slope == slope_t::flat)  ) {
		// deep water
		image_array_t const* const list = sea->get_child<image_array_t>(2);
		int nr = min( -tile_h, list->get_count() - 2 );
		return list->get_image( nr, 0 )->get_id();
	}
	const bool snow = height >= world->get_snowline();
	const sint16 climate_nr = snow ? number_of_climates : (world->get_climate(k) > 1 ? world->get_climate(k) - 1 : 0);
	return pick_ground_image(slope, climate_nr);
}


image_id ground_desc_t::get_water_tile(slope_t::type slope, int stage)
{
	require_normalized_ground_slope(slope, "ground_desc_t::get_water_tile");
	if(  slope < 0  ||  slope >= totalslopes  ||  doubleslope_to_imgnr[slope] == 255  ) {
		return IMG_EMPTY;
	}
	return water_image + stage + water_animation_stages*doubleslope_to_imgnr[slope];
}


image_id ground_desc_t::get_climate_tile(climate cl, slope_t::type slope)
{
	require_normalized_ground_slope(slope, "ground_desc_t::get_climate_tile");
	return pick_ground_image(slope, cl <= 0 ? 0 : cl - 1);
}


image_id ground_desc_t::get_snow_tile(slope_t::type slope)
{
	require_normalized_ground_slope(slope, "ground_desc_t::get_snow_tile");
	return pick_ground_image(slope, number_of_climates);
}


image_id ground_desc_t::get_beach_tile(slope_t::type slope, uint8 corners)
{
	require_normalized_ground_slope(slope, "ground_desc_t::get_beach_tile");
	// Pakset-owned via `landscape/grounds/texture-shore/`: one cell per
	// realisable `(slope, water_mask)`, baked against the LightTexture
	// silhouette so the alpha walk is in lockstep with the source water
	// tile.  `corners` is the 6-bit `water_corners` mask (E=1, SE=2,
	// SW=4, W=8, NW=16, NE=32); cells the bake doesn't ship resolve to
	// `IMG_EMPTY` and `draw_alpha` skips the overlay.  Init-time
	// tripwire in `init_ground_textures` guards the silhouette
	// invariant against future pakset drift.
	return transition_water_texture->get_image((uint16)slope, (uint16)corners);
}


image_id ground_desc_t::get_alpha_tile(slope_t::type slope, uint8 corners)
{
	require_normalized_ground_slope(slope, "ground_desc_t::get_alpha_tile");
	// Pakset-owned via `landscape/grounds/texture-slope/`: one cell
	// per `(slope, corner_mask)`, baked against `LightTexture`'s
	// silhouette so the alpha walk is in lockstep with the source
	// climate / snow tile.  `corners` is the 6-bit hex corner mask
	// (E=1, SE=2, SW=4, W=8, NW=16, NE=32) `grund.cc::display`
	// builds from `climate_corners`; cells the bake doesn't ship
	// resolve to `IMG_EMPTY` and `draw_alpha` skips the overlay.
	// Init-time tripwire in `init_ground_textures` guards the
	// silhouette invariant against future pakset drift.
	return transition_slope_texture->get_image((uint16)slope, (uint16)corners);
}


image_id ground_desc_t::get_alpha_tile(slope_t::type slope)
{
	require_normalized_ground_slope(slope, "ground_desc_t::get_alpha_tile");
	// Snowline transition: the cell is keyed by the slope's high
	// corners (`corner_height > 0`, since slopes are normalised to
	// `min(ch) == 0`).  Routes through the (slope, mask) form so
	// SlopeTrans serves both alpha-flag readers from one bake.
	const uint8 mask =
		  ((corner_e (slope) > 0) ? (1 << hex_corner_t::E ) : 0)
		| ((corner_se(slope) > 0) ? (1 << hex_corner_t::SE) : 0)
		| ((corner_sw(slope) > 0) ? (1 << hex_corner_t::SW) : 0)
		| ((corner_w (slope) > 0) ? (1 << hex_corner_t::W ) : 0)
		| ((corner_nw(slope) > 0) ? (1 << hex_corner_t::NW) : 0)
		| ((corner_ne(slope) > 0) ? (1 << hex_corner_t::NE) : 0);
	return transition_slope_texture->get_image((uint16)slope, (uint16)mask);
}
