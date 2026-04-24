/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 */

#include "terraformer.h"

#include "../dataobj/scenario.h"
#include "../descriptor/ground_desc.h"
#include "../tool/simmenu.h"
#include "simworld.h"


namespace {
	constexpr uint8 HEX_N = hex_corner_t::count; // 6

	// Read the 6 current corner heights of tile (x, y).  For water
	// tiles the vertex storage may hold a below-water terrain value;
	// clamp each against the water height to match the terrain surface
	// the terraformer reasons about.
	void read_corners(const karte_t *welt, sint16 x, sint16 y, sint8 out[HEX_N])
	{
		const grund_t *gr = welt->lookup_kartenboden_nocheck(x, y);
		const koord k(x, y);
		const sint8 water_hgt = welt->get_water_hgt_nocheck(x, y);
		if (gr->is_water()) {
			for (uint8 c = 0; c < HEX_N; c++) {
				const sint8 vh = welt->lookup_hgt_nocheck(k, (hex_corner_t::type)c);
				out[c] = min(water_hgt, vh);
			}
		}
		else {
			const sint8 h0 = gr->get_hoehe();
			const slope_t::type sl = gr->get_grund_hang();
			out[hex_corner_t::E ] = h0 + corner_e (sl);
			out[hex_corner_t::SE] = h0 + corner_se(sl);
			out[hex_corner_t::SW] = h0 + corner_sw(sl);
			out[hex_corner_t::W ] = h0 + corner_w (sl);
			out[hex_corner_t::NW] = h0 + corner_nw(sl);
			out[hex_corner_t::NE] = h0 + corner_ne(sl);
		}
	}

	// Min and max of a 6-corner height array.
	void min_max_corners(const sint8 h[HEX_N], sint8 &h_min, sint8 &h_max)
	{
		h_min = h[0];
		h_max = h[0];
		for (uint8 c = 1; c < HEX_N; c++) {
			h_min = min(h_min, h[c]);
			h_max = max(h_max, h[c]);
		}
	}

	// Slope of a tile whose 6 corner heights are hn[], seen from a
	// displayed base of disp_hneu.  Under the 6-corner base-3 encoding
	// the per-corner deltas are in [0, 2] by construction of the
	// propagation rule; callers clamp against water_hgt before calling.
	slope_t::type slope_from_corners(const sint8 hn[HEX_N], sint8 water_hgt, sint8 &disp_hneu)
	{
		sint8 h_min, h_max_ignored;
		min_max_corners(hn, h_min, h_max_ignored);
		disp_hneu = max(h_min, water_hgt);
		const sint8 dE  = max(hn[hex_corner_t::E ], water_hgt) - disp_hneu;
		const sint8 dSE = max(hn[hex_corner_t::SE], water_hgt) - disp_hneu;
		const sint8 dSW = max(hn[hex_corner_t::SW], water_hgt) - disp_hneu;
		const sint8 dW  = max(hn[hex_corner_t::W ], water_hgt) - disp_hneu;
		const sint8 dNW = max(hn[hex_corner_t::NW], water_hgt) - disp_hneu;
		const sint8 dNE = max(hn[hex_corner_t::NE], water_hgt) - disp_hneu;
		return encode_corners_hex(dE, dSE, dSW, dW, dNW, dNE);
	}
}


terraformer_t::node_t::node_t() :
	x(-1),
	y(-1),
	changed(0)
{
	for (uint8 c = 0; c < HEX_N; c++) {
		h[c] = 0;
	}
}


terraformer_t::node_t::node_t(sint16 x_, sint16 y_, const sint8 h_[HEX_N], uint8 c) :
	x(x_),
	y(y_),
	changed(c)
{
	for (uint8 i = 0; i < HEX_N; i++) {
		h[i] = h_[i];
	}
}


bool terraformer_t::node_t::operator==(const terraformer_t::node_t &a) const
{
	return (a.x==x) && (a.y==y);
}


terraformer_t::terraformer_t(operation_t op, karte_t *welt) :
	actual_flag(1),
	ready(false),
	op(op),
	welt(welt)
{
}


bool terraformer_t::node_t::comp(const terraformer_t::node_t &a, const terraformer_t::node_t &b)
{
	int diff = a.x - b.x;
	if (diff == 0) {
		diff = a.y - b.y;
	}
	return diff<0;
}


void terraformer_t::add_node_internal(sint16 x, sint16 y, const sint8 h[HEX_N])
{
	if (!welt->is_within_limits(x,y)) {
		return;
	}

	const node_t test(x, y, h, (uint8)(actual_flag^3));
	node_t *other = list.insert_unique_ordered(test, node_t::comp);

	const sint8 factor = (op == terraformer_t::raise) ? +1 : -1;

	if (other) {
		for (uint8 c = 0; c < HEX_N; c++) {
			if (factor * other->h[c] < factor * test.h[c]) {
				other->h[c] = test.h[c];
				other->changed |= (uint8)(actual_flag ^ 3);
				ready = false;
			}
		}
	}
	else {
		ready = false;
	}
}


void terraformer_t::add_node(sint16 x, sint16 y, sint8 hE, sint8 hSE, sint8 hSW, sint8 hW, sint8 hNW, sint8 hNE)
{
	const sint8 h[HEX_N] = { hE, hSE, hSW, hW, hNW, hNE };
	add_node_internal(x, y, h);
}


void terraformer_t::add_node(sint16 x, sint16 y, sint8 hgt)
{
	const sint8 h[HEX_N] = { hgt, hgt, hgt, hgt, hgt, hgt };
	add_node_internal(x, y, h);
}


void terraformer_t::generate_affected_tile_list()
{
	while( !ready) {
		actual_flag ^= 3; // flip bits
		// clear new_flag bit
		for(node_t& i : list) {
			i.changed &= actual_flag;
		}

		// process nodes with actual_flag set
		ready = true;
		for(uint32 j=0; j < list.get_count(); j++) {
			node_t& i = list[j];
			if (i.changed & actual_flag) {
				i.changed &= ~actual_flag;
				if (op == terraformer_t::raise) {
					prepare_raise(i);
				}
				else {
					prepare_lower(i);
				}
			}
		}
	}
}


const char *terraformer_t::can_raise_all(const player_t *player, bool keep_water) const
{
	assert(op == terraformer_t::raise);
	assert(ready);

	for(node_t const& i : list) {
		if (const char *err = can_raise_tile_to(i, player, keep_water)) {
			return err;
		}
	}
	return NULL;
}


const char *terraformer_t::can_lower_all(const player_t *player) const
{
	assert(op == terraformer_t::lower);
	assert(ready);

	for(node_t const& i : list) {
		if (const char *err = can_lower_tile_to(i, player)) {
			return err;
		}
	}

	return NULL;
}


int terraformer_t::apply()
{
	assert(ready);
	int n = 0;

	if (op == terraformer_t::raise) {
		for(node_t const& i : list) {
			n += raise_to(i);
		}
	}
	else {
		for(node_t const& i : list) {
			n += lower_to(i);
		}
	}

	return n;
}


// Hex edge propagation.  For each of the 6 edges of tile (x, y), if
// one of the 2 corners on that edge would be raised (resp. lowered),
// queue a node on the neighbour tile across that edge.  The neighbour
// sees the edge with corners (i+4)%6 and (i+3)%6 in its own hex_corner_t
// frame — this is the "flip" that makes K's corner i the same world
// vertex as neighbour's corner (i+4)%6.  The other 4 corners of the
// neighbour are free to be max_hdiff below the higher of the two shared
// target heights (raise) / above the lower (lower).
void terraformer_t::prepare_raise(const node_t node)
{
	assert(welt->is_within_limits(node.x, node.y));

	sint8 h0[HEX_N];
	read_corners(welt, node.x, node.y, h0);

	// new height (target clamped up from current)
	sint8 hn[HEX_N];
	bool any_up = false;
	for (uint8 c = 0; c < HEX_N; c++) {
		hn[c] = max(node.h[c], h0[c]);
		if (h0[c] < node.h[c]) {
			any_up = true;
		}
	}

	const grund_t *gr = welt->lookup_kartenboden_nocheck(node.x, node.y);
	if (!gr->is_water() && !any_up) {
		return;
	}

	// max-hdiff self-check (matches pre-port invariant; water tiles may
	// temporarily violate it during multi-pass raise_to updates).
	sint8 hneu, hmaxneu;
	min_max_corners(hn, hneu, hmaxneu);
	const uint8 max_hdiff = ground_desc_t::double_grounds ? 2 : 1;
	const bool ok = (hmaxneu - hneu <= max_hdiff);
	if (!ok && !gr->is_water()) {
		assert(false);
	}

	// propagate across each of the 6 hex edges
	for (uint8 e = 0; e < HEX_N; e++) {
		const uint8 cA = e;                 // first corner of edge e
		const uint8 cB = (e + 1) % HEX_N;   // second corner
		if (h0[cA] >= node.h[cA] && h0[cB] >= node.h[cB]) {
			continue; // nothing raised on this edge
		}

		const koord nb = koord(node.x, node.y) + koord::neighbours[e];
		const sint8 floor_h = (sint8)(max(node.h[cA], node.h[cB]) - max_hdiff);

		// neighbour sees K's corner cA as corner (e+4)%6 and K's corner
		// cB as corner (e+3)%6 in its own frame.
		const uint8 nA = (e + 4) % HEX_N;
		const uint8 nB = (e + 3) % HEX_N;

		sint8 nh[HEX_N];
		for (uint8 c = 0; c < HEX_N; c++) {
			nh[c] = floor_h;
		}
		nh[nA] = node.h[cA];
		nh[nB] = node.h[cB];

		add_node_internal(nb.x, nb.y, nh);
	}
}


void terraformer_t::prepare_lower(const node_t node)
{
	assert(welt->is_within_limits(node.x, node.y));

	sint8 h0[HEX_N];
	read_corners(welt, node.x, node.y, h0);

	const uint8 max_hdiff = ground_desc_t::double_grounds ? 2 : 1;

	for (uint8 e = 0; e < HEX_N; e++) {
		const uint8 cA = e;
		const uint8 cB = (e + 1) % HEX_N;
		if (h0[cA] <= node.h[cA] && h0[cB] <= node.h[cB]) {
			continue; // nothing to lower on this edge
		}

		const koord nb = koord(node.x, node.y) + koord::neighbours[e];
		const sint8 ceil_h = (sint8)(min(node.h[cA], node.h[cB]) + max_hdiff);

		const uint8 nA = (e + 4) % HEX_N;
		const uint8 nB = (e + 3) % HEX_N;

		sint8 nh[HEX_N];
		for (uint8 c = 0; c < HEX_N; c++) {
			nh[c] = ceil_h;
		}
		nh[nA] = node.h[cA];
		nh[nB] = node.h[cB];

		add_node_internal(nb.x, nb.y, nh);
	}
}


const char *terraformer_t::can_raise_tile_to(const node_t &node, const player_t *player, bool keep_water) const
{
	assert(welt->is_within_limits(node.x,node.y));

	const grund_t *gr = welt->lookup_kartenboden_nocheck(node.x,node.y);
	const sint8 water_hgt = welt->get_water_hgt_nocheck(node.x,node.y);

	sint8 min_hgt, max_hgt;
	min_max_corners(node.h, min_hgt, max_hgt);

	if(  gr->is_water()  &&  keep_water  &&  max_hgt > water_hgt  ) {
		return "";
	}

	return can_raise_plan_to(player, node.x, node.y, max_hgt);
}


const char* terraformer_t::can_lower_tile_to(const node_t &node, const player_t *player) const
{
	assert(welt->is_within_limits(node.x,node.y));

	sint8 hneu, max_hgt_ignored;
	min_max_corners(node.h, hneu, max_hgt_ignored);

	if( hneu < welt->get_min_allowed_height() ) {
		return "Maximum tile height difference reached.";
	}

	// check if need to lower water height for higher neighbouring tiles
	for(  size_t i = 0 ;  i < lengthof(koord::neighbours) ;  i++  ) {
		const koord neighbour = koord( node.x, node.y ) + koord::neighbours[i];
		if(  welt->is_within_limits(neighbour)  &&  welt->get_water_hgt_nocheck(neighbour) > hneu  ) {
			if (!welt->is_plan_height_changeable( neighbour.x, neighbour.y )) {
				return "";
			}
		}
	}

	return can_lower_plan_to(player, node.x, node.y, hneu );
}


int terraformer_t::raise_to(const node_t &node)
{
	assert(welt->is_within_limits(node.x,node.y));

	int n = 0;
	grund_t *gr = welt->lookup_kartenboden_nocheck(node.x,node.y);
	const sint8 water_hgt = welt->get_water_hgt_nocheck(node.x,node.y);
	const koord k(node.x, node.y);

	sint8 h0[HEX_N];
	read_corners(welt, node.x, node.y, h0);

	sint8 hn[HEX_N];
	bool any_up = false;
	for (uint8 c = 0; c < HEX_N; c++) {
		hn[c] = max(node.h[c], h0[c]);
		if (h0[c] < node.h[c]) {
			any_up = true;
		}
	}

	if (!gr->is_water() && !any_up) {
		return 0;
	}

	sint8 hneu, hmaxneu;
	min_max_corners(hn, hneu, hmaxneu);

	const uint8 max_hdiff = ground_desc_t::double_grounds ? 2 : 1;

	sint8 disp_hneu;
	const slope_t::type sneu = slope_from_corners(hn, water_hgt, disp_hneu);

	const bool ok = (hmaxneu - hneu <= max_hdiff);
	if (!ok && !gr->is_water()) {
		assert(false);
	}

	bool recalc_climate = gr->is_water()  ||  welt->get_settings().get_climate_generator() == settings_t::HEIGHT_BASED;

	// change height and slope, for water tiles only if they will become land
	if(  !gr->is_water()  ||  (hmaxneu > water_hgt  ||  (hneu == water_hgt  &&  hmaxneu == water_hgt)  )  ) {
		gr->set_pos( koord3d( node.x, node.y, disp_hneu ) );
		gr->set_grund_hang( sneu );
		welt->access_nocheck(node.x,node.y)->angehoben();
		welt->set_water_hgt_nocheck(node.x, node.y, welt->get_groundwater()-4);
	}

	// write all 6 hex corners.  Canonical per-vertex storage means no
	// map-edge special cases: each corner resolves to a slot that the
	// allocator sized for it (south/east phantom rows).
	for (uint8 c = 0; c < HEX_N; c++) {
		welt->set_grid_hgt_nocheck(k, (hex_corner_t::type)c, hn[c]);
	}
	if (recalc_climate) {
		welt->calc_climate(k, true);
	}

	for (uint8 c = 0; c < HEX_N; c++) {
		n += hn[c] - h0[c];
	}

	welt->lookup_kartenboden_nocheck(node.x,node.y)->calc_image();

	if ( (node.x+2) < welt->get_size().x ) {
		welt->lookup_kartenboden_nocheck(node.x+1,node.y)->calc_image();
	}

	if ( (node.y+2) < welt->get_size().y ) {
		welt->lookup_kartenboden_nocheck(node.x,node.y+1)->calc_image();
	}

	return n;
}


int terraformer_t::lower_to(const node_t &node)
{
	assert(welt->is_within_limits(node.x,node.y));

	int n = 0;
	grund_t *gr = welt->lookup_kartenboden_nocheck(node.x,node.y);
	sint8 water_hgt = welt->get_water_hgt_nocheck(node.x,node.y);
	const koord k(node.x, node.y);

	sint8 h0[HEX_N];
	read_corners(welt, node.x, node.y, h0);

	sint8 hn[HEX_N];
	bool any_down = false;
	for (uint8 c = 0; c < HEX_N; c++) {
		hn[c] = min(node.h[c], h0[c]);
		if (h0[c] > node.h[c]) {
			any_down = true;
		}
	}

	// nothing to do?  For water tiles fall through on NW-only change so
	// the water table gets rechecked; for land tiles bail if unchanged.
	if (gr->is_water()) {
		if (h0[hex_corner_t::NW] <= node.h[hex_corner_t::NW]) {
			return 0;
		}
	}
	else if (!any_down) {
		return 0;
	}

	sint8 hneu, hmaxneu;
	min_max_corners(hn, hneu, hmaxneu);

	if(  hneu >= water_hgt  ) {
		// calculate water table from surrounding tiles - start off with height on this tile
		sint8 water_table = water_hgt >= gr->get_hoehe() ? water_hgt : welt->get_groundwater() - 4;

		// For each corner that bottoms out at the new tile minimum
		// height, the 2 neighbours sharing that corner are candidates
		// for water-table propagation: corner c is shared with
		// neighbours at edges (c+5)%6 and c.
		uint8 neighbour_flags = 0; // 6 bits, one per neighbour
		for (uint8 c = 0; c < HEX_N; c++) {
			if (hn[c] == hneu) {
				neighbour_flags |= (uint8)(1 << ((c + HEX_N - 1) % HEX_N));
				neighbour_flags |= (uint8)(1 << c);
			}
		}

		for(  size_t i = 0;  i < lengthof(koord::neighbours) ;  i++  ) {
			const koord neighbour = k + koord::neighbours[i];
			if(  welt->is_within_limits( neighbour )  &&  ((neighbour_flags >> i) & 1)  ) {
				grund_t *gr2 = welt->lookup_kartenboden_nocheck( neighbour );
				const sint8 water_hgt_neighbour = welt->get_water_hgt_nocheck( neighbour );
				if(  gr2  &&  (water_hgt_neighbour >= gr2->get_hoehe())  &&  water_hgt_neighbour <= hneu  ) {
					water_table = max( water_table, water_hgt_neighbour );
				}
			}
		}

		for(  size_t i = 0;  i < lengthof(koord::neighbours) ;  i++  ) {
			const koord neighbour = k + koord::neighbours[i];
			if(  welt->is_within_limits( neighbour )  ) {
				grund_t *gr2 = welt->lookup_kartenboden_nocheck( neighbour );
				if(  gr2  &&  gr2->get_hoehe() < water_table  ) {
					water_table = welt->get_groundwater() - 4;
					break;
				}
			}
		}

		// only allow water table to be lowered (except for case of sea level)
		// this prevents severe (errors!
		if(  water_table < welt->get_water_hgt_nocheck(node.x,node.y)  ) {
			water_hgt = water_table;
			welt->set_water_hgt_nocheck(node.x, node.y, water_table );
		}
	}

	sint8 disp_hneu;
	const slope_t::type sneu = slope_from_corners(hn, water_hgt, disp_hneu);

	bool recalc_climate = welt->get_settings().get_climate_generator() == settings_t::HEIGHT_BASED;

	// change height and slope for land tiles only
	if(  !gr->is_water()  ||  (hmaxneu > water_hgt)  ) {
		gr->set_pos( koord3d( node.x, node.y, disp_hneu ) );
		gr->set_grund_hang( (slope_t::type)sneu );
		welt->access_nocheck(node.x,node.y)->abgesenkt();
	}

	for (uint8 c = 0; c < HEX_N; c++) {
		welt->set_grid_hgt_nocheck(k, (hex_corner_t::type)c, hn[c]);
	}

	// water heights
	// lower water height for higher neighbouring tiles
	for(  size_t i = 0;  i < lengthof(koord::neighbours);  i++  ) {
		const koord neighbour = k + koord::neighbours[i];
		if(  welt->is_within_limits( neighbour )  ) {
			const sint8 water_hgt_neighbour = welt->get_water_hgt_nocheck( neighbour );
			if(water_hgt_neighbour > hneu  ) {
				if(  welt->min_hgt_nocheck( neighbour ) < water_hgt_neighbour  ) {
					// Raise every corner of the neighbour to at least
					// the neighbour's water level so the tile stays a
					// water tile after the water-height change.  The
					// square version recursed through raise_grid_to to
					// maintain max_hdiff across grid points; that
					// recursion was square-shaped (8-neighbour stencil)
					// and is deliberately dropped here — the wider
					// max_hdiff invariant across the affected area is
					// already maintained by the terraformer's own
					// propagation (add_node_internal) on the tiles we
					// touch.
					for (uint8 c = 0; c < HEX_N; c++) {
						const hex_corner_t::type cc = (hex_corner_t::type)c;
						if (welt->lookup_hgt_nocheck(neighbour, cc) < water_hgt_neighbour) {
							welt->set_grid_hgt_nocheck(neighbour, cc, water_hgt_neighbour);
						}
					}
				}

				welt->set_water_hgt_nocheck( neighbour, hneu );
				welt->access_nocheck(neighbour)->correct_water();
				recalc_climate = true;
			}
		}
	}

	if (recalc_climate) {
		welt->calc_climate( k, false );
		for(  size_t i = 0;  i < lengthof(koord::neighbours);  i++  ) {
			const koord neighbour = k + koord::neighbours[i];
			welt->calc_climate( neighbour, false );
		}
	}

	// recalc landscape images - need to extend 2 in each direction
	for(  sint16 j = node.y - 2;  j <= node.y + 2;  j++  ) {
		for(  sint16 i = node.x - 2;  i <= node.x + 2;  i++  ) {
			if(  welt->is_within_limits( i, j )  ) {
				welt->recalc_transitions( koord (i, j ) );
			}
		}
	}

	for (uint8 c = 0; c < HEX_N; c++) {
		n += h0[c] - hn[c];
	}

	welt->lookup_kartenboden_nocheck(node.x,node.y)->calc_image();
	if( (node.x+2) < welt->get_size().x ) {
		welt->lookup_kartenboden_nocheck(node.x+1,node.y)->calc_image();
	}

	if( (node.y+2) < welt->get_size().y ) {
		welt->lookup_kartenboden_nocheck(node.x,node.y+1)->calc_image();
	}

	return n;
}


const char *terraformer_t::can_lower_plan_to(const player_t *player, sint16 x, sint16 y, sint8 h) const
{
	const planquadrat_t *plan = welt->access(x,y);
	const settings_t &settings = welt->get_settings();

	if(  plan==NULL  ) {
		return "";
	}

	if(  h < welt->get_groundwater() - 3  ) {
		return "Watertable reached";
	}

	const sint8 hmax = plan->get_kartenboden()->get_hoehe();
	if(  (hmax == h  ||  hmax == h - 1)  &&  (plan->get_kartenboden()->get_grund_hang() == 0  ||  welt->is_plan_height_changeable( x, y ))  ) {
		return NULL;
	}

	if(  !welt->is_plan_height_changeable(x, y)  ) {
		return "";
	}

	// tunnel slope below?
	const grund_t *gr = plan->get_boden_in_hoehe( h - 1 );
	if(  !gr  ) {
		gr = plan->get_boden_in_hoehe( h - 2 );
	}

	if(  !gr  && settings.get_way_height_clearance()==2  ) {
		gr = plan->get_boden_in_hoehe( h - 3 );
	}

	if(  gr  &&  h < gr->get_pos().z + slope_t::max_diff( gr->get_weg_hang() ) + settings.get_way_height_clearance()  ) {
		return "";
	}

	// tunnel below?
	while(h < hmax) {
		if(plan->get_boden_in_hoehe(h)) {
			return "";
		}
		h ++;
	}

	// check allowance by scenario
	if (welt->get_scenario()->is_scripted()) {
		return welt->get_scenario()->is_work_allowed_here(player, TOOL_LOWER_LAND|GENERAL_TOOL, ignore_wt, 0, plan->get_kartenboden()->get_pos());
	}

	return NULL;
}


const char *terraformer_t::can_raise_plan_to(const player_t *player, sint16 x, sint16 y, sint8 h) const
{
	const planquadrat_t *plan = welt->access(x,y);
	if(  plan == NULL  ||  !welt->is_plan_height_changeable(x, y)  ) {
		return "";
	}

	// irgendwo eine Bruecke im Weg?
	const sint8 hmin = plan->get_kartenboden()->get_hoehe();
	while(h > hmin) {
		if(plan->get_boden_in_hoehe(h)) {
			return "";
		}
		h --;
	}

	// check allowance by scenario
	if (welt->get_scenario()->is_scripted()) {
		return welt->get_scenario()->is_work_allowed_here(player, TOOL_RAISE_LAND|GENERAL_TOOL, ignore_wt, 0, plan->get_kartenboden()->get_pos());
	}

	return NULL;
}
