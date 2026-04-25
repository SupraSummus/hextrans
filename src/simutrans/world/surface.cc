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

#if __has_include(<sanitizer/common_interface_defs.h>)
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


static sint8 median( sint8 a, sint8 b, sint8 c )
{
#if 0
	if(  a==b  ||  a==c  ) {
		return a;
	}
	else if(  b==c  ) {
		return b;
	}
	else {
		// noting matches
//		return (3*128+1 + a+b+c)/3-128;
		return -128;
	}
#elif 0
	if(  a<=b  ) {
		return b<=c ? b : max(a,c);
	}
	else {
		return b>c ? b : min(a,c);
	}
#else
		return (6*128+3 + a+a+b+b+c+c)/6-128;
#endif
}


surface_t::surface_t() :
	climate_map(0, 0),
	humidity_map(0, 0)
{
}


surface_t::~surface_t()
{
}


koord surface_t::get_closest_coordinate(koord outside_pos)
{
	outside_pos.clip_min(koord(0,0));
	outside_pos.clip_max(koord(get_size().x-1,get_size().y-1));

	return outside_pos;
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


void surface_t::get_height_slope_from_grid(koord k, sint8 &hgt, slope_t::type &slope)
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

		slope  = slope_t::raised_E  * min(hE  - hgt, 2);
		slope |= slope_t::raised_SE * min(hSE - hgt, 2);
		slope |= slope_t::raised_SW * min(hSW - hgt, 2);
		slope |= slope_t::raised_W  * min(hW  - hgt, 2);
		slope |= slope_t::raised_NW * min(hNW - hgt, 2);
		slope |= slope_t::raised_NE * min(hNE - hgt, 2);
	}
}


// fills array with neighbour heights
// HEX-PORT: 6 hex neighbours.  Storage shape stays [8][4] until
// calculate_natural_slope() and friends are ported away from 4-corner
// square geometry; the unused tail is zeroed so callers don't read
// uninitialised memory (their downstream output is still wrong for hex
// but doesn't crash).
void surface_t::get_neighbour_heights(const koord k, sint8 neighbour_height[8][4]) const
{
	memset(neighbour_height, 0, sizeof(sint8) * 8 * 4);
	for(  size_t i = 0;  i < lengthof(koord::neighbours);  i++  ) {
		planquadrat_t *pl2 = access( k + koord::neighbours[i] );
		if(  pl2  ) {
			grund_t *gr2 = pl2->get_kartenboden();
			slope_t::type slope_corner = gr2->get_grund_hang();
			for(  int j = 0;  j < 4;  j++  ) {
				neighbour_height[i][j] = gr2->get_hoehe() + corner_sw(slope_corner);
				slope_corner /= slope_t::southeast;
			}
		}
		else {
			switch(i) {
				case 0: // nw
					neighbour_height[i][0] = groundwater;
					neighbour_height[i][1] = max( legacy_grid_hgt( k+koord(0,0) ), get_water_hgt( k ) );
					neighbour_height[i][2] = groundwater;
					neighbour_height[i][3] = groundwater;
				break;
				case 1: // w
					neighbour_height[i][0] = groundwater;
					neighbour_height[i][1] = max( legacy_grid_hgt( k+koord(0,1) ), get_water_hgt( k ) );
					neighbour_height[i][2] = max( legacy_grid_hgt( k+koord(0,0) ), get_water_hgt( k ) );
					neighbour_height[i][3] = groundwater;
				break;
				case 2: // sw
					neighbour_height[i][0] = groundwater;
					neighbour_height[i][1] = groundwater;
					neighbour_height[i][2] = max( legacy_grid_hgt( k+koord(0,1) ), get_water_hgt( k ) );
					neighbour_height[i][3] = groundwater;
				break;
				case 3: // s
					neighbour_height[i][0] = groundwater;
					neighbour_height[i][1] = groundwater;
					neighbour_height[i][2] = max( legacy_grid_hgt( k+koord(1,1) ), get_water_hgt( k ) );
					neighbour_height[i][3] = max( legacy_grid_hgt( k+koord(0,1) ), get_water_hgt( k ) );
				break;
				case 4: // se
					neighbour_height[i][0] = groundwater;
					neighbour_height[i][1] = groundwater;
					neighbour_height[i][2] = groundwater;
					neighbour_height[i][3] = max( legacy_grid_hgt( k+koord(1,1) ), get_water_hgt( k ) );
				break;
				case 5: // e
					neighbour_height[i][0] = max( legacy_grid_hgt( k+koord(1,1) ), get_water_hgt( k ) );
					neighbour_height[i][1] = groundwater;
					neighbour_height[i][2] = groundwater;
					neighbour_height[i][3] = max( legacy_grid_hgt( k+koord(1,0) ), get_water_hgt( k ) );
				break;
				case 6: // ne
					neighbour_height[i][0] = max( legacy_grid_hgt( k+koord(1,0) ), get_water_hgt( k ) );
					neighbour_height[i][1] = groundwater;
					neighbour_height[i][2] = groundwater;
					neighbour_height[i][3] = groundwater;
				break;
				case 7: // n
					neighbour_height[i][0] = max( legacy_grid_hgt( k+koord(0,0) ), get_water_hgt( k ) );
					neighbour_height[i][1] = max( legacy_grid_hgt( k+koord(1,0) ), get_water_hgt( k ) );
					neighbour_height[i][2] = groundwater;
					neighbour_height[i][3] = groundwater;
				break;
			}

			/*neighbour_height[i][0] = groundwater;
			neighbour_height[i][1] = groundwater;
			neighbour_height[i][2] = groundwater;
			neighbour_height[i][3] = groundwater;*/
		}
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


int surface_t::grid_raise(const player_t *player, koord k, const char*&err)
{
	int n = 0;

	if(is_within_grid_limits(k)) {

		const grund_t *gr = lookup_kartenboden_gridcoords(k);
		const slope4_t corner_to_raise = get_corner_to_operate(k);

		const sint16 x = gr->get_pos().x;
		const sint16 y = gr->get_pos().y;
		const sint8 hgt = gr->get_hoehe(corner_to_raise);

		// HEX-PORT: grid_raise still picks one of the 4 legacy square
		// corners (NW / NE / SW / SE) to raise.  The hex-only E and W
		// corners are not directly addressable through the grid-point
		// GUI tool today; they track neighbours via terraformer
		// propagation.  Pass them as "hgt - o", i.e. the same "non-target"
		// target height the other 3 square corners get, so they stay
		// put if already at or above that height.
		sint8 hE, hSE, hSW, hW, hNW, hNE;
		if(  !gr->is_water()  ) {
			const sint8 f = ground_desc_t::double_grounds ?  2 : 1;
			const sint8 o = ground_desc_t::double_grounds ?  1 : 0;

			hSW = hgt - o + scorner_sw( corner_to_raise ) * f;
			hSE = hgt - o + scorner_se( corner_to_raise ) * f;
			hNE = hgt - o + scorner_ne( corner_to_raise ) * f;
			hNW = hgt - o + scorner_nw( corner_to_raise ) * f;
			hE  = hgt - o;
			hW  = hgt - o;
		}
		else {
			hE = hSE = hSW = hW = hNW = hNE = hgt;
		}

		terraformer_t digger(terraformer_t::raise, world());
		digger.add_node(x, y, hE, hSE, hSW, hW, hNW, hNE);
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


int surface_t::grid_lower(const player_t *player, koord k, const char*&err)
{
	int n = 0;

	if(is_within_grid_limits(k)) {

		const grund_t *gr = lookup_kartenboden_gridcoords(k);
		const slope4_t corner_to_lower = get_corner_to_operate(k);

		const sint16 x = gr->get_pos().x;
		const sint16 y = gr->get_pos().y;
		const sint8 hgt = gr->get_hoehe(corner_to_lower);

		// HEX-PORT: see grid_raise; E and W corners fall through to the
		// "non-target" height for the same reason.
		const sint8 f = ground_desc_t::double_grounds ?  2 : 1;
		const sint8 o = ground_desc_t::double_grounds ?  1 : 0;
		const sint8 hSW = hgt + o - scorner_sw( corner_to_lower ) * f;
		const sint8 hSE = hgt + o - scorner_se( corner_to_lower ) * f;
		const sint8 hNE = hgt + o - scorner_ne( corner_to_lower ) * f;
		const sint8 hNW = hgt + o - scorner_nw( corner_to_lower ) * f;
		const sint8 hE  = hgt + o;
		const sint8 hW  = hgt + o;

		terraformer_t digger(terraformer_t::lower, world());
		digger.add_node(x, y, hE, hSE, hSW, hW, hNW, hNE);
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


// raise height in the hgt-array
void surface_t::raise_grid_to(sint16 x, sint16 y, sint8 h)
{
	if(is_within_grid_limits(x,y)) {
		// HEX-PORT: doubled index lands on the E canonical slot of
		// tile (x-1, y-1) — see surface.h.
		const sint32 offset = (x + y*(cached_grid_size.x+1)) * 2;

		if(  grid_hgts[offset] < h  ) {
			grid_hgts[offset] = h;

			const sint8 hh = h - (ground_desc_t::double_grounds ? 2 : 1);

			// set new height of neighbor grid points
			raise_grid_to(x-1, y-1, hh);
			raise_grid_to(x  , y-1, hh);
			raise_grid_to(x+1, y-1, hh);
			raise_grid_to(x-1, y  , hh);
			raise_grid_to(x+1, y  , hh);
			raise_grid_to(x-1, y+1, hh);
			raise_grid_to(x  , y+1, hh);
			raise_grid_to(x+1, y+1, hh);
		}
	}
}


void surface_t::lower_grid_to(sint16 x, sint16 y, sint8 h)
{
	if(is_within_grid_limits(x,y)) {
		// HEX-PORT: doubled index — see raise_grid_to.
		const sint32 offset = (x + y*(cached_grid_size.x+1)) * 2;

		if(  grid_hgts[offset] > h  ) {
			grid_hgts[offset] = h;
			sint8 hh = h + 2;
			// set new height of neighbor grid points
			lower_grid_to(x-1, y-1, hh);
			lower_grid_to(x  , y-1, hh);
			lower_grid_to(x+1, y-1, hh);
			lower_grid_to(x-1, y  , hh);
			lower_grid_to(x+1, y  , hh);
			lower_grid_to(x-1, y+1, hh);
			lower_grid_to(x  , y+1, hh);
			lower_grid_to(x+1, y+1, hh);
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
	// corner.  Deltas are clamped to 2 (the max corner height under
	// the 6-corner base-3 encoding) so pathological terrain can't
	// overflow into an unrelated slope value.
	const int hE  = lookup_hgt_nocheck(k, hex_corner_t::E);
	const int hSE = lookup_hgt_nocheck(k, hex_corner_t::SE);
	const int hSW = lookup_hgt_nocheck(k, hex_corner_t::SW);
	const int hW  = lookup_hgt_nocheck(k, hex_corner_t::W);
	const int hNW = lookup_hgt_nocheck(k, hex_corner_t::NW);
	const int hNE = lookup_hgt_nocheck(k, hex_corner_t::NE);

	const int mini = min( min( min(hE, hSE), min(hSW, hW) ),
	                      min(hNW, hNE) );

	const int dE  = min(hE  - mini, 2);
	const int dSE = min(hSE - mini, 2);
	const int dSW = min(hSW - mini, 2);
	const int dW  = min(hW  - mini, 2);
	const int dNW = min(hNW - mini, 2);
	const int dNE = min(hNE - mini, 2);

	return encode_corners_hex(dE, dSE, dSW, dW, dNW, dNE);
}


slope_t::type surface_t::recalc_natural_slope( const koord k, sint8 &new_height ) const
{
	grund_t *gr = lookup_kartenboden(k);
	if(!gr) {
		return slope_t::flat;
	}
	else {
		const sint8 max_hdiff = ground_desc_t::double_grounds ? 2 : 1;

		sint8 corner_height[4];

		// get neighbour corner heights
		// HEX-PORT TODO: this whole calculate_natural_slope() block is
		// fundamentally square-grid (4 corners, 8 neighbours, & 7 mask
		// indexing) and needs rewriting once slope_t becomes 6-corner.
		// Until then we keep [8] storage; the trailing two entries are
		// zeroed by get_neighbour_heights so the masked indexing into
		// [6,7] doesn't read garbage.
		sint8 neighbour_height[8][4];
		get_neighbour_heights( k, neighbour_height );

		//check whether neighbours are foundations
		bool neighbour_fundament[8] = {};
		for(  size_t i = 0;  i < lengthof(koord::neighbours);  i++  ) {
			grund_t *gr2 = lookup_kartenboden( k + koord::neighbours[i] );
			neighbour_fundament[i] = (gr2  &&  gr2->get_typ() == grund_t::fundament);
		}

		for(  uint8 i = 0;  i < 4;  i++  ) { // 0 = sw, 1 = se etc.
			// corner_sw (i=0): tests vs neighbour 1:w (corner 2 j=1),2:sw (corner 3) and 3:s (corner 4)
			// corner_se (i=1): tests vs neighbour 3:s (corner 3 j=2),4:se (corner 4) and 5:e (corner 1)
			// corner_ne (i=2): tests vs neighbour 5:e (corner 4 j=3),6:ne (corner 1) and 7:n (corner 2)
			// corner_nw (i=3): tests vs neighbour 7:n (corner 1 j=0),0:nw (corner 2) and 1:w (corner 3)

			sint16 median_height = 0;
			uint8 natural_corners = 0;
			for(  int j = 1;  j < 4;  j++  ) {
				if(  !neighbour_fundament[(i * 2 + j) & 7]  ) {
					natural_corners++;
					median_height += neighbour_height[(i * 2 + j) & 7][(i + j) & 3];
				}
			}
			switch(  natural_corners  ) {
				case 1: {
					corner_height[i] = (sint8)median_height;
					break;
				}
				case 2: {
					corner_height[i] = median_height >> 1;
					break;
				}
				default: {
					// take the average of all 3 corners (if no natural corners just use the artificial ones anyway)
					corner_height[i] = median( neighbour_height[(i * 2 + 1) & 7][(i + 1) & 3], neighbour_height[(i * 2 + 2) & 7][(i + 2) & 3], neighbour_height[(i * 2 + 3) & 7][(i + 3) & 3] );
					break;
				}
			}
		}

		// new height of that tile ...
		sint8 min_height = min( min( corner_height[0], corner_height[1] ), min( corner_height[2], corner_height[3] ) );
		sint8 max_height = max( max( corner_height[0], corner_height[1] ), max( corner_height[2], corner_height[3] ) );
		/* check for an artificial slope on a steep sidewall */
		bool not_ok = abs( max_height - min_height ) > max_hdiff  ||  min_height == -128;

		sint8 old_height = gr->get_hoehe();
		new_height = min_height;

		// now we must make clear, that there is no ground above/below the slope
		if(  old_height!=new_height  ) {
			not_ok |= lookup(koord3d(k,new_height))!=NULL;
			if(  old_height > new_height  ) {
				not_ok |= lookup(koord3d(k,old_height-1))!=NULL;
			}
			if(  old_height < new_height  ) {
				not_ok |= lookup(koord3d(k,old_height+1))!=NULL;
			}
		}

		if(  not_ok  ) {
			/* difference too high or ground above/below
			 * we just keep it as it was ...
			 */
			new_height = old_height;
			return gr->get_grund_hang();
		}

		const sint16 d1 = min( corner_height[0] - new_height, max_hdiff );
		const sint16 d2 = min( corner_height[1] - new_height, max_hdiff );
		const sint16 d3 = min( corner_height[2] - new_height, max_hdiff );
		const sint16 d4 = min( corner_height[3] - new_height, max_hdiff );
		return encode_corners(d1, d2, d3, d4);
	}
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
		climate_corners = (climate_corners >> 1) | ((climate_corners & 1) << 3);
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
		// get neighbour corner heights
		// HEX-PORT TODO: square-grid 4-corner climate-transition logic.
		// Same situation as calculate_natural_slope() above.
		sint8 neighbour_height[8][4];
		get_neighbour_heights( k, neighbour_height );

		// look up neighbouring climates
		climate neighbour_climate[8] = {};
		for(  size_t i = 0;  i < lengthof(koord::neighbours);  i++  ) {
			koord k_neighbour = k + koord::neighbours[i];
			if(  !is_within_limits(k_neighbour)  ) {
				k_neighbour = get_closest_coordinate(k_neighbour);
			}
			neighbour_climate[i] = get_climate( k_neighbour );
		}

		uint8 climate_corners = 0;
		climate climate0 = get_climate(k);

		slope_t::type slope_corner = gr->get_grund_hang();
		for(  uint8 i = 0;  i < 4;  i++  ) { // 0 = sw, 1 = se etc.
			// corner_sw (i=0): tests vs neighbour 1:w (corner 2 j=1),2:sw (corner 3) and 3:s (corner 4)
			// corner_se (i=1): tests vs neighbour 3:s (corner 3 j=2),4:se (corner 4) and 5:e (corner 1)
			// corner_ne (i=2): tests vs neighbour 5:e (corner 4 j=3),6:ne (corner 1) and 7:n (corner 2)
			// corner_nw (i=3): tests vs neighbour 7:n (corner 1 j=0),0:nw (corner 2) and 1:w (corner 3)
			sint8 corner_height = gr->get_hoehe() + corner_sw(slope_corner);

			climate transition_climate = water_climate;
			climate min_climate = arctic_climate;

			for(  int j = 1;  j < 4;  j++  ) {
				if(  corner_height == neighbour_height[(i * 2 + j) & 7][(i + j) & 3]) {
					climate climatej = neighbour_climate[(i * 2 + j) & 7];
					climatej > transition_climate ? transition_climate = climatej : 0;
					climatej < min_climate ? min_climate = climatej : 0;
				}
			}

			if(  min_climate == water_climate  ||  transition_climate > climate0  ) {
				climate_corners |= 1 << i;
			}
			slope_corner /= slope_t::southeast;
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
