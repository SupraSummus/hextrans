/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 */

#include "surface.h"

#include "simworld.h"
#include "terraformer.h"

#include "../simdebug.h"
#include "../descriptor/ground_desc.h"
#include "../ground/grund.h"
#include "../player/simplay.h"
#include "../tpl/vector_tpl.h"

// Test the active-ASAN macro, not __has_include: libgcc-dev ships
// <sanitizer/common_interface_defs.h> on every Linux toolchain, so a
// header probe lets the call slip into non-ASAN builds and breaks
// linking.
#ifdef __SANITIZE_ADDRESS__
#  include <sanitizer/common_interface_defs.h>
#  define HEX_PORT_PRINT_STACK() __sanitizer_print_stack_trace()
#else
#  define HEX_PORT_PRINT_STACK() ((void)0)
#endif


#define array_koord(px,py) (px + py * get_size().x)


// HEX-PORT: the legacy grid-point `(x, y)` height accessors are fatal
// during the port.  Every caller must migrate to the `(koord tile,
// hex_corner_t::type c)` overloads in surface.h.  The `koord` forms
// forward to their `(x, y)` siblings so one stack frame gives us the
// full site identity.  HEX_PORT_PRINT_STACK runs first because
// dbg->fatal calls abort() which ASAN does not backtrace.
sint8 surface_t::lookup_hgt_nocheck(sint16 x, sint16 y) const
{
	HEX_PORT_PRINT_STACK();
	dbg->fatal("surface_t::lookup_hgt_nocheck(x,y)",
		"legacy grid-point reader at (%d,%d) — port to the (tile, hex_corner_t) overload", x, y);
}

sint8 surface_t::lookup_hgt_nocheck(koord k) const
{
	return lookup_hgt_nocheck(k.x, k.y);
}

sint8 surface_t::lookup_hgt(sint16 x, sint16 y) const
{
	HEX_PORT_PRINT_STACK();
	dbg->fatal("surface_t::lookup_hgt(x,y)",
		"legacy grid-point reader at (%d,%d) — port to the (tile, hex_corner_t) overload", x, y);
}

sint8 surface_t::lookup_hgt(koord k) const
{
	return lookup_hgt(k.x, k.y);
}

void surface_t::set_grid_hgt_nocheck(sint16 x, sint16 y, sint8 hgt)
{
	HEX_PORT_PRINT_STACK();
	dbg->fatal("surface_t::set_grid_hgt_nocheck(x,y)",
		"legacy grid-point writer at (%d,%d) hgt=%d — port to the (tile, hex_corner_t) overload", x, y, hgt);
}

void surface_t::set_grid_hgt_nocheck(koord k, sint8 hgt)
{
	set_grid_hgt_nocheck(k.x, k.y, hgt);
}


surface_t::surface_t() :
	climate_map(0, 0),
	humidity_map(0, 0)
{
}


surface_t::~surface_t()
{
}


koord surface_t::get_closest_coordinate(koord outside_pos) const
{
	outside_pos.clip_min(koord(0,0));
	outside_pos.clip_max(koord(get_size().x-1,get_size().y-1));

	return outside_pos;
}


sint8 surface_t::vertex_corner_height(hex_vertex_t v) const
{
	// Off-map (tile, corner) names still refer to a real vertex; use the
	// lex-min canonical name so corner indices line up with the tile we read.
	const hex_vertex_t c = canonical_vertex(v);
	koord t = c.tile;
	if(  !is_within_limits(t)  ) {
		t = get_closest_coordinate(t);
	}
	const planquadrat_t *pl = access(t);
	if(  !pl  ) {
		return get_groundwater();
	}
	const grund_t *gr = pl->get_kartenboden();
	const slope_t::type sl = gr->get_grund_hang();
	const sint8 base = gr->get_hoehe();
	switch(  c.corner  ) {
		case hex_corner_t::E:  return base + (sint8)corner_e(sl);
		case hex_corner_t::SE: return base + (sint8)corner_se(sl);
		case hex_corner_t::SW: return base + (sint8)corner_sw(sl);
		case hex_corner_t::W:  return base + (sint8)corner_w(sl);
		case hex_corner_t::NW: return base + (sint8)corner_nw(sl);
		case hex_corner_t::NE: return base + (sint8)corner_ne(sl);
		default: return base;
	}
}


climate surface_t::climate_at_clamped(koord k) const
{
	if(  !is_within_limits(k)  ) {
		k = get_closest_coordinate(k);
	}
	return get_climate(k);
}


bool surface_t::is_water(koord pos, koord dim) const
{
	koord k;
	for(  k.y = pos.y;  k.y < pos.y + dim.y;  k.y++  ) {
		for(  k.x = pos.x;  k.x < pos.x + dim.x;  k.x++  ) {
			if(  !is_within_grid_limits( k + koord(1, 1) )  ||  max_hgt(k) > get_water_hgt(k)  ) {
				return false;
			}
		}
	}

	return true;
}


bool surface_t::square_is_free(koord pos, sint16 w, sint16 h, int *last_y, climate_bits cl) const
{
	if(pos.x < 0  ||  pos.y < 0  ||  pos.x+w > get_size().x || pos.y+h > get_size().y) {
		return false;
	}

	grund_t *gr = lookup_kartenboden(pos);
	const sint16 platz_base_h = gr->get_hoehe(); // remember the base height of the first tile
	const sint16 platz_max_h = gr->get_hoehe() + slope_t::max_diff( gr->get_grund_hang() ); // remember the max height of the first tile

	koord k_check;
	for(k_check.x=pos.x; k_check.x<pos.x+w; k_check.x++) {
		for(k_check.y=pos.y+h-1; k_check.y>=pos.y; k_check.y--) {
			const grund_t *gr = lookup_kartenboden(k_check);

			// we can built, if: max height all the same, everything removable and no buildings there
			slope_t::type slope = gr->get_grund_hang();
			sint8 max_height = gr->get_hoehe() + slope_t::max_diff(slope);
			climate test_climate = get_climate(k_check);
			if(  cl & (1 << water_climate)  &&  test_climate != water_climate  ) {
				bool neighbour_water = false;
				for(size_t i=0; i<lengthof(koord::neighbours)  &&  !neighbour_water; i++) {
					if(  is_within_limits(k_check + koord::neighbours[i])  &&  get_climate( k_check + koord::neighbours[i] ) == water_climate  ) {
						neighbour_water = true;
					}
				}
				if(  neighbour_water  ) {
					test_climate = water_climate;
				}
			}
			if( (platz_max_h != max_height  &&  platz_base_h != gr->get_hoehe())  ||  !gr->ist_natur()  ||  gr->kann_alle_obj_entfernen(NULL) != NULL  ||
			     (cl & (1 << test_climate)) == 0  ||  ( slope && (lookup( gr->get_pos()+koord3d(0,0,1) ) ||
			     (slope_t::max_diff(slope)==2 && lookup( gr->get_pos()+koord3d(0,0,2) )) ))  ) {
				if(  last_y  ) {
					*last_y = k_check.y;
				}
				return false;
			}
		}
	}

	return true;
}


sint8 surface_t::min_hgt_nocheck(const koord k) const
{
	// Minimum over the 6 hex corners of tile k.  Reads via the
	// per-vertex accessor; canonicalisation + index lookup are
	// cheap enough here that keeping the old pointer-arithmetic
	// fast path isn't worth the square-shaped geometry it carries.
	const int hE  = lookup_hgt_nocheck(k, hex_corner_t::E);
	const int hSE = lookup_hgt_nocheck(k, hex_corner_t::SE);
	const int hSW = lookup_hgt_nocheck(k, hex_corner_t::SW);
	const int hW  = lookup_hgt_nocheck(k, hex_corner_t::W);
	const int hNW = lookup_hgt_nocheck(k, hex_corner_t::NW);
	const int hNE = lookup_hgt_nocheck(k, hex_corner_t::NE);
	return min( min( min(hE, hSE), min(hSW, hW) ), min(hNW, hNE) );
}


sint8 surface_t::max_hgt_nocheck(const koord k) const
{
	// Maximum over the 6 hex corners of tile k — see min_hgt_nocheck.
	const int hE  = lookup_hgt_nocheck(k, hex_corner_t::E);
	const int hSE = lookup_hgt_nocheck(k, hex_corner_t::SE);
	const int hSW = lookup_hgt_nocheck(k, hex_corner_t::SW);
	const int hW  = lookup_hgt_nocheck(k, hex_corner_t::W);
	const int hNW = lookup_hgt_nocheck(k, hex_corner_t::NW);
	const int hNE = lookup_hgt_nocheck(k, hex_corner_t::NE);
	return max( max( max(hE, hSE), max(hSW, hW) ), max(hNW, hNE) );
}


sint8 surface_t::min_hgt(const koord k) const
{
	// Same as min_hgt_nocheck but bounds-checks the tile.  6 hex
	// corners of tile k.
	if( !is_within_limits(k) ) {
		return groundwater;
	}
	return min_hgt_nocheck(k);
}


sint8 surface_t::max_hgt(const koord k) const
{
	if( !is_within_limits(k) ) {
		return groundwater;
	}
	const int hE  = lookup_hgt_nocheck(k, hex_corner_t::E);
	const int hSE = lookup_hgt_nocheck(k, hex_corner_t::SE);
	const int hSW = lookup_hgt_nocheck(k, hex_corner_t::SW);
	const int hW  = lookup_hgt_nocheck(k, hex_corner_t::W);
	const int hNW = lookup_hgt_nocheck(k, hex_corner_t::NW);
	const int hNE = lookup_hgt_nocheck(k, hex_corner_t::NE);
	return max( max( max(hE, hSE), max(hSW, hW) ), max(hNW, hNE) );
}


void surface_t::get_height_slope_from_grid(koord k, sint8 &hgt, slope_t::type &slope) const
{
	if(  (k.x | k.y | (cached_grid_size.x - k.x-1) | (cached_grid_size.y - k.y-1)) >= 0  ) {
		// Tile is inside map — read its 6 hex corners and derive
		// the slope; structurally the same shape as
		// calc_natural_slope, but this variant also hands back the
		// tile's min-corner height via the @p hgt out-param.
		const int hE  = lookup_hgt_nocheck(k, hex_corner_t::E);
		const int hSE = lookup_hgt_nocheck(k, hex_corner_t::SE);
		const int hSW = lookup_hgt_nocheck(k, hex_corner_t::SW);
		const int hW  = lookup_hgt_nocheck(k, hex_corner_t::W);
		const int hNW = lookup_hgt_nocheck(k, hex_corner_t::NW);
		const int hNE = lookup_hgt_nocheck(k, hex_corner_t::NE);

		hgt = (sint8)min( min( min(hE, hSE), min(hSW, hW) ),
		                  min(hNW, hNE) );

		slope  = slope_t::raised_E  * min(hE  - hgt, 3);
		slope |= slope_t::raised_SE * min(hSE - hgt, 3);
		slope |= slope_t::raised_SW * min(hSW - hgt, 3);
		slope |= slope_t::raised_W  * min(hW  - hgt, 3);
		slope |= slope_t::raised_NW * min(hNW - hgt, 3);
		slope |= slope_t::raised_NE * min(hNE - hgt, 3);
	}
}


void surface_t::get_natural_height_slope_from_grid(koord k, sint8 &hgt, slope_t::type &slope) const
{
	if(  (k.x | k.y | (cached_grid_size.x - k.x-1) | (cached_grid_size.y - k.y-1)) >= 0  ) {
		const int hE  = lookup_natural_hgt_nocheck(k, hex_corner_t::E);
		const int hSE = lookup_natural_hgt_nocheck(k, hex_corner_t::SE);
		const int hSW = lookup_natural_hgt_nocheck(k, hex_corner_t::SW);
		const int hW  = lookup_natural_hgt_nocheck(k, hex_corner_t::W);
		const int hNW = lookup_natural_hgt_nocheck(k, hex_corner_t::NW);
		const int hNE = lookup_natural_hgt_nocheck(k, hex_corner_t::NE);

		hgt = (sint8)min( min( min(hE, hSE), min(hSW, hW) ),
		                  min(hNW, hNE) );

		slope  = slope_t::raised_E  * min(hE  - hgt, 3);
		slope |= slope_t::raised_SE * min(hSE - hgt, 3);
		slope |= slope_t::raised_SW * min(hSW - hgt, 3);
		slope |= slope_t::raised_W  * min(hW  - hgt, 3);
		slope |= slope_t::raised_NW * min(hNW - hgt, 3);
		slope |= slope_t::raised_NE * min(hNE - hgt, 3);
	}
}


void surface_t::reset_natural_to_visible()
{
	if(  grid_hgts  &&  natural_grid_hgts  ) {
		const uint32 slots = vertex_slot_count(cached_grid_size.x, cached_grid_size.y);
		memcpy(natural_grid_hgts, grid_hgts, slots);
	}
}


bool surface_t::is_plan_height_changeable(sint16 x, sint16 y) const
{
	const planquadrat_t *plan = access(x,y);
	bool ok = true;

	if(plan != NULL) {
		grund_t *gr = plan->get_kartenboden();

		ok = (gr->ist_natur() || gr->is_water())  &&  !gr->hat_wege()  &&  !gr->is_halt();

		for(  int i=0; ok  &&  i<gr->obj_count(); i++  ) {
			const obj_t *obj = gr->obj_bei(i);
			assert(obj != NULL);
			ok =
				obj->get_typ() == obj_t::baum  ||
				obj->get_typ() == obj_t::zeiger  ||
				obj->get_typ() == obj_t::cloud  ||
				obj->get_typ() == obj_t::groundobj;
		}
	}

	return ok;
}


int surface_t::grid_raise(const player_t *player, koord k, hex_corner_t::type corner, const char*&err)
{
	int n = 0;

	if(is_within_grid_limits(k)) {

		const grund_t *gr = lookup_kartenboden_gridcoords(k);

		const sint16 x = gr->get_pos().x;
		const sint16 y = gr->get_pos().y;
		const sint8 hgt = gr->get_hoehe(corner);

		// All 6 corners track the same target height: the picked corner
		// rises by `f`, the others may stay anywhere `≥ hgt - o`.  The
		// terraformer's 6-edge propagation handles neighbour flow.
		sint8 h[hex_corner_t::count];
		if(  !gr->is_water()  ) {
			const sint8 f = ground_desc_t::double_grounds ?  2 : 1;
			const sint8 o = ground_desc_t::double_grounds ?  1 : 0;
			for (uint8 c = 0; c < hex_corner_t::count; c++) {
				h[c] = hgt - o + (c == (uint8)corner ? f : 0);
			}
		}
		else {
			for (uint8 c = 0; c < hex_corner_t::count; c++) h[c] = hgt;
		}

		terraformer_t digger(terraformer_t::raise, world());
		digger.add_node(x, y,
			h[hex_corner_t::E], h[hex_corner_t::SE], h[hex_corner_t::SW],
			h[hex_corner_t::W], h[hex_corner_t::NW], h[hex_corner_t::NE]);
		digger.generate_affected_tile_list();

		err = digger.can_raise_all(player);
		if (err) {
			return 0;
		}
		n = digger.apply();

		// force world full redraw, or background could be dirty.
		world()->set_dirty();

		if(  max_height < lookup_kartenboden_gridcoords(k)->get_hoehe()  ) {
			max_height = lookup_kartenboden_gridcoords(k)->get_hoehe();
		}
	}

	return (n+3)>>2;
}


int surface_t::grid_lower(const player_t *player, koord k, hex_corner_t::type corner, const char*&err)
{
	int n = 0;

	if(is_within_grid_limits(k)) {

		const grund_t *gr = lookup_kartenboden_gridcoords(k);

		const sint16 x = gr->get_pos().x;
		const sint16 y = gr->get_pos().y;
		const sint8 hgt = gr->get_hoehe(corner);

		// Mirror of grid_raise.
		const sint8 f = ground_desc_t::double_grounds ?  2 : 1;
		const sint8 o = ground_desc_t::double_grounds ?  1 : 0;
		sint8 h[hex_corner_t::count];
		for (uint8 c = 0; c < hex_corner_t::count; c++) {
			h[c] = hgt + o - (c == (uint8)corner ? f : 0);
		}

		terraformer_t digger(terraformer_t::lower, world());
		digger.add_node(x, y,
			h[hex_corner_t::E], h[hex_corner_t::SE], h[hex_corner_t::SW],
			h[hex_corner_t::W], h[hex_corner_t::NW], h[hex_corner_t::NE]);
		digger.generate_affected_tile_list();

		err = digger.can_lower_all(player);
		if (err) {
			return 0;
		}

		n = digger.apply();
		err = NULL;

		// force world full redraw, or background could be dirty.
		world()->set_dirty();

		if(  min_height > min_hgt_nocheck( koord(x,y) )  ) {
			min_height = min_hgt_nocheck( koord(x,y) );
		}
	}
	return (n+3)>>2;
}


namespace {
	struct vertex_height_t {
		hex_vertex_t v;
		sint8 h;
	};
}


// Iterative LIFO walk of the 3-neighbour hex vertex graph, replacing
// the direct recursion the square `raise_grid_to` body was originally
// ported from.  Bounds + slot dedup are applied at dequeue, so an
// out-of-range `h` cannot leak into `grid_hgts` even if a caller
// passes one (the recursion variant wrote `h` first and bailed
// after).  Hex vertex adjacency (flat-top, canonical E/SE corners
// only): (q,r,E) neighbours (q,r,SE) (q,r-1,SE) (q+1,r-1,SE).
void surface_t::raise_vertex_to(sint16 q, sint16 r, hex_corner_t::type c, sint8 h)
{
	const sint8 h_min = get_min_allowed_height();
	vector_tpl<vertex_height_t> worklist(16);
	worklist.append({canonical_vertex({koord(q, r), c}), h});

	while (!worklist.empty()) {
		const vertex_height_t job = worklist.pop_back();
		const hex_vertex_t cv = job.v; // already canonical
		const sint8 hh = job.h;

		// Out-of-range guard, before any slot mutation.
		if (hh < h_min) continue;
		// Canonical tiles span x ∈ [-1, W-1], y ∈ [-1, H].
		if ((uint16)(cv.tile.x + 1) > (uint16)cached_grid_size.x ||
		    (uint16)(cv.tile.y + 1) > (uint16)(cached_grid_size.y + 1)) {
			continue;
		}
		const uint32 slot = vertex_slot_index(cv, cached_grid_size.x);
		if (grid_hgts[slot] >= hh) continue;
		// Map-gen / lake-creation natural writer — both channels move together.
		grid_hgts[slot] = hh;
		natural_grid_hgts[slot] = hh;
		if (hh <= h_min) continue; // neighbour h would be out of range

		const sint8 hh1 = hh - 1;
		hex_vertex_t nb[3];
		vertex_neighbours(cv, nb);
		for (int i = 0; i < 3; i++) {
			worklist.append({nb[i], hh1});
		}
	}
}


void surface_t::lower_vertex_to(sint16 q, sint16 r, hex_corner_t::type c, sint8 h)
{
	const sint8 h_max = get_max_allowed_height();
	vector_tpl<vertex_height_t> worklist(16);
	worklist.append({canonical_vertex({koord(q, r), c}), h});

	while (!worklist.empty()) {
		const vertex_height_t job = worklist.pop_back();
		const hex_vertex_t cv = job.v;
		const sint8 hh = job.h;

		if (hh > h_max) continue;
		if ((uint16)(cv.tile.x + 1) > (uint16)cached_grid_size.x ||
		    (uint16)(cv.tile.y + 1) > (uint16)(cached_grid_size.y + 1)) {
			continue;
		}
		const uint32 slot = vertex_slot_index(cv, cached_grid_size.x);
		if (grid_hgts[slot] <= hh) continue;
		grid_hgts[slot] = hh;
		natural_grid_hgts[slot] = hh;
		if (hh >= h_max) continue;

		const sint8 hh1 = hh + 1;
		hex_vertex_t nb[3];
		vertex_neighbours(cv, nb);
		for (int i = 0; i < 3; i++) {
			worklist.append({nb[i], hh1});
		}
	}
}


bool surface_t::can_flatten_tile(player_t *player, koord k, sint8 hgt, bool keep_water, bool make_underwater_hill)
{
	return flatten_tile(player, k, hgt, keep_water, make_underwater_hill, true /* just check */);
}


// make a flat level at this position (only used for AI at the moment)
bool surface_t::flatten_tile(player_t *player, koord k, sint8 hgt, bool keep_water, bool make_underwater_hill, bool justcheck)
{
	int n = 0;
	bool ok = true;
	const grund_t *gr = lookup_kartenboden(k);
	const slope_t::type slope = gr->get_grund_hang();
	const sint8 old_hgt = make_underwater_hill  &&  gr->is_water() ? min_hgt(k) : gr->get_hoehe();
	const sint8 max_hgt = old_hgt + slope_t::max_diff(slope);

	if(  max_hgt > hgt  ) {

		terraformer_t digger(terraformer_t::lower, world());
		digger.add_node(k.x, k.y, hgt);
		digger.generate_affected_tile_list();

		ok = digger.can_lower_all(player) == NULL;

		if (ok  &&  !justcheck) {
			n += digger.apply();
		}
	}

	if(  ok  &&  old_hgt < hgt  ) {
		terraformer_t digger(terraformer_t::raise, world());
		digger.add_node(k.x, k.y, hgt);
		digger.generate_affected_tile_list();

		ok = digger.can_raise_all(player, keep_water) == NULL;

		if (ok  &&  !justcheck) {
			n += digger.apply();
		}
	}

	// was changed => pay for it
	if(n>0) {
		n = (n+3) / 4;
		player_t::book_construction_costs(player, n * world()->get_settings().cst_alter_land, k, ignore_wt);
	}

	return ok;
}


slope_t::type surface_t::calc_natural_slope( const koord k ) const
{
	if(!is_within_grid_limits(k.x, k.y)) {
		return slope_t::flat;
	}

	// Read the 6 hex corner heights for tile k via the per-vertex
	// accessor; derive the slope from the deltas above the minimum
	// corner.  Deltas are clamped to 3 (the max corner height under
	// the 6-corner base-4 encoding) so pathological terrain can't
	// overflow into an unrelated slope value.
	const int hE  = lookup_hgt_nocheck(k, hex_corner_t::E);
	const int hSE = lookup_hgt_nocheck(k, hex_corner_t::SE);
	const int hSW = lookup_hgt_nocheck(k, hex_corner_t::SW);
	const int hW  = lookup_hgt_nocheck(k, hex_corner_t::W);
	const int hNW = lookup_hgt_nocheck(k, hex_corner_t::NW);
	const int hNE = lookup_hgt_nocheck(k, hex_corner_t::NE);

	const int mini = min( min( min(hE, hSE), min(hSW, hW) ),
	                      min(hNW, hNE) );

	const int dE  = min(hE  - mini, 3);
	const int dSE = min(hSE - mini, 3);
	const int dSW = min(hSW - mini, 3);
	const int dW  = min(hW  - mini, 3);
	const int dNW = min(hNW - mini, 3);
	const int dNE = min(hNE - mini, 3);

	return encode_corners_hex(dE, dSE, dSW, dW, dNW, dNE);
}


slope_t::type surface_t::recalc_natural_slope( const koord k, sint8 &new_height ) const
{
	grund_t *gr = lookup_kartenboden(k);
	if(  !gr  ) {
		return slope_t::flat;
	}

	// Read from the natural channel so artificial overlays at shared
	// vertices (set-slope tool, foundation placement) don't bias what
	// we report as the natural slope.  Per-vertex storage on the
	// natural channel is canonical-by-construction, so neighbour
	// views agree without averaging.  The extra logic this function
	// keeps is rejecting the candidate when it would collide with
	// ground stacked above or below the tile.
	slope_t::type new_slope;
	sint8 candidate_height = 0;
	get_natural_height_slope_from_grid(k, candidate_height, new_slope);

	const sint8 max_hdiff = ground_desc_t::double_grounds ? 2 : 1;
	const sint8 old_height = gr->get_hoehe();

	bool not_ok = slope_t::max_diff(new_slope) > max_hdiff;
	if(  candidate_height != old_height  ) {
		not_ok |= lookup(koord3d(k, candidate_height)) != NULL;
		if(  old_height > candidate_height  ) {
			not_ok |= lookup(koord3d(k, old_height - 1)) != NULL;
		}
		if(  old_height < candidate_height  ) {
			not_ok |= lookup(koord3d(k, old_height + 1)) != NULL;
		}
	}

	if(  not_ok  ) {
		new_height = old_height;
		return gr->get_grund_hang();
	}

	new_height = candidate_height;
	return new_slope;
}


void surface_t::init_height_to_climate()
{
	// mark unused as arctic
	memset( num_climates_at_height, 0, lengthof(num_climates_at_height) );

	const settings_t &settings = world()->get_settings();

	// now just add them, the later climates will win (we will do a fineer assessment later
	for( int cl=0;  cl<MAX_CLIMATES;  cl++ ) {
		DBG_DEBUG( "init_height_to_climate()", "climate %i, start %i end %i", cl,  settings.get_climate_borders( cl, 0 ),  settings.get_climate_borders( cl, 1 ) );
		for( sint8 h = max(groundwater, settings.get_climate_borders( cl, 0 )); h < settings.get_climate_borders( cl, 1 ); h++ ) {
			if(  num_climates_at_height[ h - groundwater ] == 0  ) {
				// default climate for this height is the first matching one
				height_to_climate[ h - groundwater ] = (uint8)cl;
			}
			num_climates_at_height[h-groundwater]++;
		}
	}
	for( int h = 0; h < 128; h++ ) {
		if(  num_climates_at_height[h]==0  ) {
			if( h == 0 ) {
				height_to_climate[ h ] = desert_climate;
			}
			else if( h - groundwater > settings.get_climate_borders( arctic_climate, 1 ) ) {
				height_to_climate[ h ] = arctic_climate;
			}
			else {
				height_to_climate[ h ] = temperate_climate;
			}
			num_climates_at_height[ h ] = 1;
		}
		DBG_DEBUG( "init_height_to_climate()", "Height %i, climate %i, num_climates %i", h - groundwater, height_to_climate[ h ], num_climates_at_height[ h ] );
	}
}


void surface_t::rotate_transitions(koord k)
{
	planquadrat_t *pl = access(k);
	if(  !pl  ) {
		return;
	}

	uint8 climate_corners = pl->get_climate_corners();
	if(  climate_corners != 0  ) {
		// 90° map rotate is not a hex symmetry; approximate by one 60° step
		// on the six-corner mask (matches slope_t::rotate60 on slopes).
		climate_corners = (uint8)(
			(((climate_corners & 0x3f) << 1) | ((climate_corners & 0x3f) >> 5)) & 0x3f);
		pl->set_climate_corners( climate_corners );
	}
}


void surface_t::recalc_transitions(koord k)
{
	planquadrat_t *pl = access(k);
	if(  !pl  ) {
		return;
	}

	grund_t *gr = pl->get_kartenboden();
	if(  !gr->is_water()  ) {
		// Hex climate transitions: each tile corner is a world vertex shared
		// by three tiles; compare heights at all three names and climates
		// of the matching neighbours (same rule as legacy four-corner path).
		uint8 climate_corners = 0;
		const climate climate0 = get_climate(k);

		for(  uint8 ci = 0;  ci < (uint8)hex_corner_t::count;  ci++  ) {
			hex_vertex_t owners[3];
			vertex_owners(k, (hex_corner_t::type)ci, owners);
			const sint8 h_here = vertex_corner_height(owners[0]);

			climate transition_climate = water_climate;
			climate min_climate = arctic_climate;

			for(  int oi = 1;  oi < 3;  oi++  ) {
				if(  h_here == vertex_corner_height(owners[oi])  ) {
					const climate climatej = climate_at_clamped(owners[oi].tile);
					climatej > transition_climate ? transition_climate = climatej : 0;
					climatej < min_climate ? min_climate = climatej : 0;
				}
			}

			if(  min_climate == water_climate  ||  transition_climate > climate0  ) {
				climate_corners |= (uint8)(1u << ci);
			}
		}
		pl->set_climate_transition_flag( climate_corners != 0 );
		pl->set_climate_corners( climate_corners );
	}

	gr->calc_image();
}


void surface_t::calc_climate(koord k, bool recalc)
{
	planquadrat_t *pl = access(k);
	if(  !pl  ) {
		return;
	}

	if( (unsigned)k.x >= climate_map.get_width()  ||  (unsigned)k.y >= climate_map.get_height()  ) {
		// not initialised yet (may happend during river creation)
		return;
	}

	grund_t *gr = pl->get_kartenboden();
	if(  gr  ) {
		if(  !gr->is_water()  ) {
			bool beach = false;
			climate default_cl = (climate)climate_map.at( k.x, k.y );
			if(  gr->get_pos().z == groundwater  ) {
				for(  size_t i = 0;  i < lengthof(koord::neighbours) && !beach;  i++  ) {
					grund_t *gr2 = lookup_kartenboden( k + koord::neighbours[i] );
					if(  gr2 && gr2->is_water()  ) {
						beach = true;
					}
				}
			}

			if( beach ) {
				pl->set_climate( desert_climate );
			}
			else if(  default_cl>water_climate  &&  default_cl<=arctic_climate  &&  world()->get_settings().get_climate_borders(default_cl,false)<=gr->get_pos().z  &&  world()->get_settings().get_climate_borders(default_cl,true)>gr->get_pos().z  ) {
				// if possible keep (or revert) to original climate
				pl->set_climate( default_cl );
			}
			else {
				pl->set_climate( get_climate_at_height( max( gr->get_pos().z, groundwater + 1 ) ) );
			}
		}
		else {
			pl->set_climate( water_climate );
		}
		pl->set_climate_transition_flag(false);
		pl->set_climate_corners(0);
	}

	if(  recalc  ) {
		recalc_transitions(k);
		for(  size_t i = 0;  i < lengthof(koord::neighbours);  i++  ) {
			recalc_transitions( k + koord::neighbours[i] );
		}
	}
}
