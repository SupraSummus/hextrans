/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 */

#include "synth_overlay.h"

#include "ground_desc.h"
#include "image.h"
#include "synth_geometry.h"
#include "synth_ground_raster.h"
#include "synth_plane_partition.h"
#include "../display/simgraph.h"
#include "../display/hex_proj.h"
#include "../simconst.h"
#include "../simdebug.h"
#include "../dataobj/environment.h"
#include "../dataobj/koord.h"

#include <cmath>
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
// Mirrors `GROUND_NO_PIXEL` in synth_ground_raster.h; both buffers use
// the same sentinel and the same RLE encoder consumes them.
static const PIXVAL NO_PIXEL = GROUND_NO_PIXEL;

// One image_t per slope per half — index 0 = front half, 1 = back half
// (matches the @p background bool exposed in the public API).  Eager
// build in init(), get_marker() is then a flat table read; this also
// dodges thread-safety questions in the multi-threaded display path.
// Memory budget: hex outlines RLE-compress to ~1-2 KB each, so the
// full set fits comfortably under 3 MB.
static image_t* marker[2][slope_t::max_slopes];

// Per-climate-slot, per-slope filled hex ground tiles.  Eager build
// alongside marker; lookup is a flat array read.  Memory budget:
// each filled hex RLE-compresses to ~3-4 KB at pak64 raster width;
// only the ~340 valid slopes (per-edge diff ≤ 1) out of 4096 are
// generated, so the full 8 * 340 set is ~8 MB.
static image_t* ground[ground_climate_slots][slope_t::max_slopes];

// Per-slope alpha-mask hex tiles, RLE-shape-identical to ground[].
// Paired with the synth ground source by the climate / beach /
// snowline overlay blits — see get_alpha in the header.
static image_t* alpha[slope_t::max_slopes];

// Per-slope grid-line border tiles — single-image full hex outline,
// drawn over the tile when `grund_t::show_grid` is on.  Mirrors the
// marker geometry but combines both halves into one image since the
// grid overlay is a single draw call (`grund_t::display_boden`) and
// is not bracketed around tile content.
static image_t* border[slope_t::max_slopes];

// Cliff-face sprites for back walls (NW and N edges).  Indexed by
// [artificial][wall][image_index], following the encoding produced by
// `get_back_image_from_diff` in `grund.cc` (see header).  The fundament
// (artificial=true) and natural-cliff (artificial=false) variants share
// geometry and differ only in palette, mirroring the
// `ground_desc_t::fundament` vs `ground_desc_t::slopes` split on the
// pakset path.  Slope is not part of the key — the cliff face only
// uses h1, h2 from the index, not this tile's overall slope.
static image_t* back_wall[2][back_wall_count][back_wall_image_count];

static bool initialised = false;


// Climate base colours (`CLIMATE_RGB555`) live in synth_ground_raster.h
// alongside the rasteriser that consumes them, so external tools that
// dump the engine's flat-tile output don't need to link this TU.


static void free_all()
{
	for(  int half = 0;  half < 2;  half++  ) {
		for(  int s = 0;  s < slope_t::max_slopes;  s++  ) {
			delete marker[half][s];
			marker[half][s] = NULL;
		}
	}
	for(  int c = 0;  c < ground_climate_slots;  c++  ) {
		for(  int s = 0;  s < slope_t::max_slopes;  s++  ) {
			delete ground[c][s];
			ground[c][s] = NULL;
		}
	}
	for(  int s = 0;  s < slope_t::max_slopes;  s++  ) {
		delete alpha[s];
		alpha[s] = NULL;
	}
	for(  int s = 0;  s < slope_t::max_slopes;  s++  ) {
		delete border[s];
		border[s] = NULL;
	}
	for(  int a = 0;  a < 2;  a++  ) {
		for(  int w = 0;  w < back_wall_count;  w++  ) {
			for(  int i = 0;  i < back_wall_image_count;  i++  ) {
				delete back_wall[a][w][i];
				back_wall[a][w][i] = NULL;
			}
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


// Rasterise a sequence of hex-vertex line segments into an image.
// The flat-top hex footprint is a `4u × 2u` lattice cell (E and W
// vertices on the horizontal extremes at mid-y, NE/SE/NW/SW at
// quarter-width on the top and bottom rows), with each vertex lifted
// by `corner_h * hex_height_raster_scale_y(TILE_HEIGHT_STEP, 4u)` for
// slope-aware outlines.  The footprint dimensions are dictated by the
// hex iso lattice in `hex_proj.h` (column step `(3u, u)`, row step
// `(0, 2u)`).  The image bbox adds headroom above that footprint for
// the maximum 2-corner lift; without it, raised N-side vertices clip at
// local y=0 even though neighbouring endpoints mathematically meet.
// See `tools/hex_proj_test/hex_proj_test.cc ::
// test_synth_slope_bbox_contains_lifted_vertices`.
//
// @p path is a list of corner indices to visit in order; @p closed
// adds the wraparound edge from the last vertex back to the first.
// `path = {E, SE, SW, W}, closed=false` is the front half of the
// marker outline (3 south-side edges); `closed=true` over all 6
// corners is the full grid border.
//
static image_t* rasterise_outline(sint32 u, slope_t::type slope,
                                  const hex_corner_t::type* path, int n,
                                  bool closed)
{
	const synth_hex_geometry_t geom = synth_hex_geometry(u, TILE_HEIGHT_STEP);
	const sint32 w = geom.w;
	const sint32 h = geom.h;

	// Vertex screen coords in image-local pixel space, ordered to
	// match hex_corner_t::type so @p path indexes in directly.
	// Y grows down; corner height lifts UP, so subtract.
	struct vertex_t { sint32 x, y; };
	const vertex_t v[hex_corner_t::count] = {
		{ geom.vx[hex_corner_t::E ], geom.vy(slope, hex_corner_t::E ) }, // E
		{ geom.vx[hex_corner_t::SE], geom.vy(slope, hex_corner_t::SE) }, // SE
		{ geom.vx[hex_corner_t::SW], geom.vy(slope, hex_corner_t::SW) }, // SW
		{ geom.vx[hex_corner_t::W ], geom.vy(slope, hex_corner_t::W ) }, // W
		{ geom.vx[hex_corner_t::NW], geom.vy(slope, hex_corner_t::NW) }, // NW
		{ geom.vx[hex_corner_t::NE], geom.vy(slope, hex_corner_t::NE) }, // NE
	};

	PIXVAL* buf = new PIXVAL[w * h];
	memset(buf, 0, w * h * sizeof(PIXVAL));

	const int n_edges = closed ? n : n - 1;
	for(  int i = 0;  i < n_edges;  i++  ) {
		const vertex_t& a = v[path[i]];
		const vertex_t& b = v[path[(i + 1) % n]];
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
	img->x = 0;
	img->y = geom.image_y();
	img->zoomable = 1;

	return img;
}


// Marker outline half — three S-side edges (front) or three N-side
// edges (back); E and W endpoints are shared between the halves so
// the two halves bracket tile content (vehicles, buildings) cleanly.
//
// Hex slopes share corners across 3 tiles — that's a terrain-storage
// problem (per AGENTS.md "per-vertex height storage") not a marker
// problem; the marker only ever shows one tile's view of its own
// corners and is consistent by construction.
static image_t* build_marker(sint32 u, slope_t::type slope, bool background)
{
	static const hex_corner_t::type front_path[4] = {
		hex_corner_t::E, hex_corner_t::SE, hex_corner_t::SW, hex_corner_t::W
	};
	static const hex_corner_t::type back_path[4] = {
		hex_corner_t::E, hex_corner_t::NE, hex_corner_t::NW, hex_corner_t::W
	};
	const hex_corner_t::type* path = background ? back_path : front_path;
	return rasterise_outline(u, slope, path, 4, /*closed=*/false);
}


// Grid border — full closed 6-edge hex outline, drawn once over the
// tile when `grund_t::show_grid` is on.  Unlike the marker, the grid
// is not split into front/back halves: it's a single draw call atop
// tile content (`grund_t::display_boden`), so all 6 edges live in
// one image.
//
// Adjacent tiles draw their own borders on the same shared edge.
// Per-tile slope storage means two neighbours can disagree on the
// height of a shared vertex once terraforming touches them, and the
// two grid lines will visibly mismatch at that vertex.  Retired by
// per-vertex height storage (AGENTS.md "Critical findings driving
// priority"); same caveat as the synth ground tile.
static image_t* build_border(sint32 u, slope_t::type slope)
{
	static const hex_corner_t::type full_path[hex_corner_t::count] = {
		hex_corner_t::E,  hex_corner_t::SE, hex_corner_t::SW,
		hex_corner_t::W,  hex_corner_t::NW, hex_corner_t::NE,
	};
	return rasterise_outline(u, slope, full_path, hex_corner_t::count, /*closed=*/true);
}


// `synth_fill_polygon`, `shade_pixval`, `decode_corner_heights` and the
// pure ground-tile rasterisation step (`rasterise_ground`) live in
// `synth_ground_raster.h` so a standalone capture tool can dump the
// engine's authoritative output without linking image_t / simgraph.
// `build_ground` here wraps the rasteriser with RLE encoding and
// image_t allocation; the back-wall builder below reuses
// `synth_fill_polygon` and `shade_pixval` from the same header.


// Build the filled hex ground tile for one (slope, climate_idx).
// Geometry mirrors `build_outline`: a `4u × 2u` lattice footprint (E
// and W vertices on the horizontal extremes at mid-y; NE/SE/NW/SW at
// quarter-width on the top and bottom rows), plus bbox headroom for
// lifted vertices.
//
// The tile is partitioned into the minimum number of coplanar corner
// regions (PoC in `hex-plane-partition.html`, ported here).  Each
// region is then shaded from one Lambertian normal so genuinely planar
// quads/pentagons do not pick up centre-fan seam artefacts.
static image_t* build_ground(sint32 u, slope_t::type slope, uint8 climate_idx,
                             const plane_partition::hex_partition_t &partition)
{
	const synth_hex_geometry_t geom = synth_hex_geometry(u, TILE_HEIGHT_STEP);
	const sint32 w = geom.w;
	const sint32 h = geom.h;

	PIXVAL* buf = new PIXVAL[w * h];
	memset(buf, 0, w * h * sizeof(PIXVAL));

	rasterise_ground(buf, geom, slope, climate_idx, partition);

	// Encode into RLE.  Worst case as in build_outline: 2 PIXVALs per
	// pixel + per-row header + tail.
	const size_t cap = (size_t)w * h * 2 + (size_t)h * 4 + 4;
	PIXVAL* tmp = new PIXVAL[cap];
	const size_t rle_len = encode_rle(buf, w, h, tmp);
	delete [] buf;

	image_t* img = new image_t(rle_len);
	memcpy(img->data, tmp, rle_len * sizeof(PIXVAL));
	delete [] tmp;

	img->w = (scr_coord_val)w;
	img->h = (scr_coord_val)h;
	img->x = 0;
	img->y = geom.image_y();
	img->zoomable = 1;

	return img;
}


// Build the alpha-mask hex tile by copying the ground tile's RLE
// verbatim and overwriting every coloured-pixel slot with a full
// opaque PIXVAL.  Copying the RLE byte-for-byte (rather than
// re-rasterising the geometry) is what guarantees byte-identical
// RLE shape to the ground tile — `display_img_alpha_wc` walks the
// source (= ground) and alpha pointers in lockstep using the
// source's RLE, and any divergence in run boundaries between the
// two tiles would walk the alpha pointer off the end of its
// allocation.
static image_t* build_alpha(const image_t* ground)
{
	image_t* img = ground->copy_rotate(0);

	const PIXVAL full_alpha = 0x7FFF;
	PIXVAL* p = img->data;
	for(  int y = 0;  y < img->h;  y++  ) {
		p++; // start_x
		do {
			sint16 runlen = *p++;
			for(  int i = 0;  i < runlen;  i++  ) {
				*p++ = full_alpha;
			}
		} while(  *p++ != 0  );
	}

	return img;
}


// Cliff-face palette.  Natural cliffs read as exposed rock / dirt;
// fundament reads as a man-made platform.  Picked to be visibly
// different from the climate-base ground tones in CLIMATE_RGB555 so
// the cliff face stands out from the surrounding terrain.
#define RGB555(r, g, b) (PIXVAL)(((r) << 10) | ((g) << 5) | (b))
static const PIXVAL CLIFF_NATURAL = RGB555(13, 11,  7); // dirt brown
static const PIXVAL CLIFF_FUNDAMENT = RGB555(20, 20, 20); // light grey
#undef RGB555

// Per-wall shading multiplier (256 = 1.0x).  Wall 0 (NW edge) faces
// screen-up-left, wall 1 (N edge) faces screen-up, wall 2 (NE edge)
// faces screen-up-right.  These values are hand-picked rather than
// derived from the terrain Lambert helper: vertical wall normals fall
// well below the flat-ground reference cosine and the helper clamps
// every wall to the same minimum brightness, losing the wall-to-wall
// contrast.
static const sint32 WALL_SHADE[back_wall_count] = { 192, 224, 256 };


// Build one cliff-face sprite.  Wall 0 attaches along this hex's NW
// edge (W → NW corners in screen space); wall 1 attaches along the N
// edge (NW → NE); wall 2 attaches along the NE edge (NE → E).  The
// lower edge sits at the unlifted edge position; the upper edge is
// lifted by `h1 * geom.lift` at corner_a and `h2 * geom.lift` at
// corner_b.  The renderer composes this with a `back_height` shift
// via `yoff` at draw time, so the sprite itself only encodes the
// cliff face above that baseline.
//
// `index` follows the encoding `get_back_image_from_diff` in
// `grund.cc` produces: index 0 = no cliff, 1..8 = `(h1, h2)` for
// `h1, h2 ∈ {0, 1, 2}` with `index = h1 + 3*h2`, and 9..10 = the
// middle slopes of double-height stacks (one corner below baseline,
// one above).  Today 9 / 10 are drawn as the corresponding
// single-step half-cliffs (h1=0,h2=1 / h1=1,h2=0); see TODO.md for
// the missing notch shape.
static image_t* build_back_wall(sint32 u, uint8 wall, uint8 index, bool artificial)
{
	const synth_hex_geometry_t geom = synth_hex_geometry(u, TILE_HEIGHT_STEP);
	const sint32 w = geom.w;
	const sint32 h = geom.h;

	const sint8 h1 = (index == 10) ? 1 : (index == 9 ? 0 : (sint8)(index % 3));
	const sint8 h2 = (index == 10) ? 0 : (index == 9 ? 1 : (sint8)(index / 3));

	// Wall lower-edge endpoints in sprite-local screen space, flat
	// (no per-corner ground lift — the cliff sprite carries h1/h2
	// lift, not the underlying ground slope).
	static const hex_corner_t::type endpoints[back_wall_count][2] = {
		{ hex_corner_t::W,  hex_corner_t::NW }, // wall 0: NW edge
		{ hex_corner_t::NW, hex_corner_t::NE }, // wall 1: N  edge
		{ hex_corner_t::NE, hex_corner_t::E  }, // wall 2: NE edge
	};
	const sint32 ax = geom.vx     [endpoints[wall][0]];
	const sint32 ay = geom.vy_base[endpoints[wall][0]];
	const sint32 bx = geom.vx     [endpoints[wall][1]];
	const sint32 by = geom.vy_base[endpoints[wall][1]];

	const sint32 lift1 = (sint32)h1 * geom.lift;
	const sint32 lift2 = (sint32)h2 * geom.lift;

	PIXVAL* buf = new PIXVAL[w * h];
	memset(buf, 0, w * h * sizeof(PIXVAL));

	if(  lift1 > 0 || lift2 > 0  ) {
		const PIXVAL base = artificial ? CLIFF_FUNDAMENT : CLIFF_NATURAL;
		const PIXVAL face_color = shade_pixval(base, WALL_SHADE[wall]);

		// Cliff polygon: lower edge (a → b), upper edge lifted
		// (b → b - lift2, a - lift1 → a).  Even-odd parity, so
		// winding doesn't matter.
		const sint32 xs[4] = { ax, bx, bx,         ax         };
		const sint32 ys[4] = { ay, by, by - lift2, ay - lift1 };
		synth_fill_polygon(buf, w, h, xs, ys, 4, face_color);

		// `synth_fill_polygon` skips horizontal edges by design (parity
		// correctness), so close them explicitly — same pattern as
		// `build_ground`.  Wall 1's lower edge is horizontal (y=top_y);
		// the upper edge is horizontal whenever lift1 == lift2.
		for(  uint8 i = 0;  i < 4;  i++  ) {
			const uint8 j = (uint8)((i + 1) % 4);
			if(  ys[i] != ys[j]  ) {
				continue;
			}
			const sint32 y = ys[i];
			if(  y < 0 || y >= h  ) {
				continue;
			}
			sint32 x0 = xs[i] < xs[j] ? xs[i] : xs[j];
			sint32 x1 = xs[i] < xs[j] ? xs[j] : xs[i];
			if(  x0 < 0   ) { x0 = 0; }
			if(  x1 >= w  ) { x1 = w - 1; }
			for(  sint32 x = x0;  x <= x1;  x++  ) {
				buf[y * w + x] = face_color;
			}
		}
	}

	const size_t cap = (size_t)w * h * 2 + (size_t)h * 4 + 4;
	PIXVAL* tmp = new PIXVAL[cap];
	const size_t rle_len = encode_rle(buf, w, h, tmp);
	delete [] buf;

	image_t* img = new image_t(rle_len);
	memcpy(img->data, tmp, rle_len * sizeof(PIXVAL));
	delete [] tmp;

	img->w = (scr_coord_val)w;
	img->h = (scr_coord_val)h;
	img->x = 0;
	img->y = geom.image_y();
	img->zoomable = 1;

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

	// The lattice unit `u = W/4` is the natural parameter for every
	// piece of hex iso geometry (column step `(3u, u)`, row step
	// `(0, 2u)`, inscribed-hex bbox `4u × 2u`).  Pinning u as the
	// builder input makes the multiples explicit at the call sites
	// and keeps any future scale change (e.g. art-larger-than-bbox
	// trim, see TODO.md) on a clean integer grid.  Inheriting from
	// a pakset template's actual w/h is not safe — pak64's Marker.pak
	// ships 64×64, which gave a 64-tall hex on a 32-tall lattice
	// slot and gapped adjacent tiles by `u` px.  See
	// `tools/hex_proj_test/hex_proj_test.cc :: test_inscribed_hex_tiles_lattice`.
	const sint32 W = (sint32)gfx->get_base_tile_raster_width();
	const sint32 u = W / 4;
	if(  u < 1  ) {
		dbg->warning("synth_overlay::init", "tile raster width %d too small; synth disabled", W);
		return;
	}

	int generated = 0;
	for(  int s = 0;  s < slope_t::max_slopes;  s++  ) {
		uint8 ch[hex_corner_t::count];
		decode_corner_heights((slope_t::type)s, ch);

		// Skip slopes that violate the per-edge ≤ 1 height constraint —
		// they cannot appear on valid terrain, the partition solver may
		// fail on them, and generating sprites for them wastes startup
		// time and memory.  Callers check for NULL via IMG_EMPTY.
		bool valid = true;
		for(  int i = 0;  i < hex_corner_t::count;  i++  ) {
			const int j = (i + 1) % hex_corner_t::count;
			if(  ch[i] > ch[j] + 1 || ch[j] > ch[i] + 1  ) {
				valid = false;
				break;
			}
		}
		if(  !valid  ) {
			continue;
		}
		generated++;

		plane_partition::hex_partition_t partition;
		partition.region_count = 0;
		if(  !plane_partition::find_min_partition(ch, partition)  ) {
			dbg->fatal("synth_overlay::init", "no planar partition for slope %d", s);
		}

		for(  int half = 0;  half < 2;  half++  ) {
			image_t* img = build_marker(u, (slope_t::type)s, half == 1);
			img->register_image();
			marker[half][s] = img;
		}
		for(  int c = 0;  c < ground_climate_slots;  c++  ) {
			image_t* img = build_ground(u, (slope_t::type)s, (uint8)c, partition);
			img->register_image();
			ground[c][s] = img;
		}
		// Alpha shape mirrors the climate-0 ground tile — all 8
		// climate ground tiles for a given slope share the same
		// geometry (only colours differ), so any climate works as
		// the donor; pick 0 by convention.
		image_t* a = build_alpha(ground[0][s]);
		a->register_image();
		alpha[s] = a;

		image_t* bd = build_border(u, (slope_t::type)s);
		bd->register_image();
		border[s] = bd;
	}

	// Cliff-face sprites are not slope-keyed (they take h1/h2 from
	// the encoded back_image index, not from this tile's overall
	// slope), so build them once per (artificial × wall × index)
	// outside the slope loop.  Index 0 = "no cliff"; we leave it as
	// NULL and rely on the caller's IMG_EMPTY check.
	for(  uint8 art = 0;  art < 2;  art++  ) {
		for(  uint8 wall = 0;  wall < back_wall_count;  wall++  ) {
			for(  uint8 idx = 1;  idx < back_wall_image_count;  idx++  ) {
				image_t* img = build_back_wall(u, wall, idx, art == 1);
				img->register_image();
				back_wall[art][wall][idx] = img;
			}
		}
	}

	initialised = true;
	const auto bbox = synth_hex_geometry(u, TILE_HEIGHT_STEP);
	DBG_DEBUG("synth_overlay::init",
	          "synthesised sprites for %d/%d slopes (u=%d, bbox %dx%d)",
	          generated, slope_t::max_slopes, u, bbox.w, bbox.h);
}


image_id get_marker(slope_t::type slope, bool background)
{
	if(  !initialised  ||  slope < 0  ||  slope >= slope_t::max_slopes  ) {
		return IMG_EMPTY;
	}
	const image_t* img = marker[background ? 1 : 0][slope];
	return img != NULL ? img->get_id() : IMG_EMPTY;
}


image_id get_ground(slope_t::type slope, uint8 climate_idx)
{
	if(  !initialised
	  ||  slope < 0
	  ||  slope >= slope_t::max_slopes
	  ||  climate_idx >= ground_climate_slots  ) {
		return IMG_EMPTY;
	}
	const image_t* img = ground[climate_idx][slope];
	return img != NULL ? img->get_id() : IMG_EMPTY;
}


image_id get_alpha(slope_t::type slope)
{
	if(  !initialised  ||  slope < 0  ||  slope >= slope_t::max_slopes  ) {
		return IMG_EMPTY;
	}
	const image_t* img = alpha[slope];
	return img != NULL ? img->get_id() : IMG_EMPTY;
}


image_id get_border(slope_t::type slope)
{
	if(  !initialised  ||  slope < 0  ||  slope >= slope_t::max_slopes  ) {
		return IMG_EMPTY;
	}
	const image_t* img = border[slope];
	return img != NULL ? img->get_id() : IMG_EMPTY;
}


image_id get_back_wall(uint8 wall, uint8 index, bool artificial)
{
	if(  !initialised
	  ||  wall >= back_wall_count
	  ||  index == 0
	  ||  index >= back_wall_image_count  ) {
		return IMG_EMPTY;
	}
	const image_t* img = back_wall[artificial ? 1 : 0][wall][index];
	return img != NULL ? img->get_id() : IMG_EMPTY;
}


} // namespace synth_overlay
