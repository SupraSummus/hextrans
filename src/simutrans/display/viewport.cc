/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 */

#include "viewport.h"

#include "hex_proj.h"
#include "simgraph.h"
#include "../world/simworld.h"
#include "../dataobj/environment.h"
#include "../dataobj/koord3d.h"
#include "../ground/grund.h"
#include "../obj/zeiger.h"

#include <cmath>


static void pick_nearest_hex_vertex_global(karte_t *world, sint32 &grid_x, sint32 &grid_y,
                                           double frac_dq, double frac_dr,
                                           hex_corner_t::type &corner)
{
	constexpr double u = 16.0; // W/4 in the 64-unit object-offset basis.
	const double mouse_x = frac_dq * 3.0 * u;
	const double mouse_y = (frac_dq + 2.0 * frac_dr) * u;

	static const koord candidates[7] = {
		koord( 0,  0),
		koord( 1,  0), koord( 0,  1), koord(-1,  1),
		koord(-1,  0), koord( 0, -1), koord( 1, -1),
	};

	double best_d2 = HUGE_VAL;
	sint32 best_x = grid_x;
	sint32 best_y = grid_y;
	hex_corner_t::type best_corner = corner;

	for(  uint8 ti = 0;  ti < 7;  ti++  ) {
		const sint32 tile_x = grid_x + candidates[ti].x;
		const sint32 tile_y = grid_y + candidates[ti].y;
		if(  !world->is_within_limits((sint16)tile_x, (sint16)tile_y)  ) {
			continue;
		}

		const double tile_xoff = (double)hex_screen_dx(candidates[ti].x, 64);
		const double tile_yoff = (double)hex_screen_dy(candidates[ti].x, candidates[ti].y, 64);
		for(  uint8 ci = 0;  ci < hex_corner_t::count;  ci++  ) {
			const hex_corner_t::type c = (hex_corner_t::type)ci;
			const koord corner_off = hex_corner_centre_offset(c);
			const double dx = tile_xoff + corner_off.x - mouse_x;
			const double dy = tile_yoff + corner_off.y - mouse_y;
			const double d2 = dx * dx + dy * dy;
			if(  d2 < best_d2  ) {
				best_d2 = d2;
				best_x = tile_x;
				best_y = tile_y;
				best_corner = c;
			}
		}
	}

	grid_x = best_x;
	grid_y = best_y;
	corner = best_corner;
}


void viewport_t::set_viewport_ij_offset( const koord &k )
{
	view_ij_off=k;
	update_cached_values();
}


koord viewport_t::get_map2d_coord( const koord3d &viewpos ) const
{
	// Convert a 3D map coord to the 2D map coord that lands at the same
	// screen position.  Z bumps the screen-y up by
	// `tile_raster_scale_y(z*TILE_HEIGHT_STEP, W)`; under hex, axial +r
	// is the only 1-tile step that adds purely to screen-y (column step
	// also has a y component, so it can't compensate alone).  Round the
	// elevation pixel offset to the nearest integer +r step (W/2 each).
	const sint16 new_yoff = tile_raster_scale_y(viewpos.z*TILE_HEIGHT_STEP,cached_img_size);
	sint16 lines = 0;
	if(new_yoff>0) {
		lines = (new_yoff + (cached_img_size/4))/(cached_img_size/2);
	}
	else {
		lines = (new_yoff - (cached_img_size/4))/(cached_img_size/2);
	}
	return world->get_closest_coordinate( viewpos.get_2d() - koord(0, lines) );
}


koord viewport_t::get_viewport_coord( const koord& coord ) const
{
	return coord+cached_aggregated_off;
}


scr_coord viewport_t::get_screen_coord( const koord3d& pos, const koord& off) const
{
	// Hex iso projection — see display/hex_proj.h for the lattice.
	// `scr_pos_2d` is the axial (q, r) delta from the viewport's
	// top-left tile to the requested tile; add per-pixel raster
	// offset, z-elevation (screen-y only), and fine pan.
	const koord scr_pos_2d = get_viewport_coord(pos.get_2d());

	const sint32 x = hex_screen_dx(scr_pos_2d.x, cached_img_size)
		+ tile_raster_scale_x(off.x, cached_img_size)
		+ x_off;
	const sint32 y = hex_screen_dy(scr_pos_2d.x, scr_pos_2d.y, cached_img_size)
		+ tile_raster_scale_y(off.y-pos.z*TILE_HEIGHT_STEP, cached_img_size)
		+ y_off;

	return scr_coord(x,y);
}


// change the center viewport position
void viewport_t::change_world_position( koord new_ij, sint16 new_xoff, sint16 new_yoff )
{
	// Normalise (ij_off, x_off, y_off) so the fine pan offsets stay
	// within one tile step.  Under hex, an `ij_off.x` step shifts all
	// screen positions by `(-3*W/4, -W/4)`; an `ij_off.y` step shifts
	// by `(0, -W/2)`.  Absorb x first (steps of `3*W/4` in xoff also
	// drag yoff by `W/4`), then absorb the residual y into r-steps of
	// `W/2`.
	const sint16 col_dx = 3 * (cached_img_size / 4); // 3*W/4
	const sint16 col_dy =     (cached_img_size / 4); // W/4
	const sint16 row_dy = 2 * (cached_img_size / 4); // W/2

	int q_steps = 0;
	if(new_xoff > 0) {
		q_steps = (new_xoff + col_dx/2) / col_dx;
	}
	else if(new_xoff < 0) {
		q_steps = (new_xoff - col_dx/2) / col_dx;
	}
	new_ij.x -= q_steps;
	new_xoff -= col_dx * q_steps;
	new_yoff -= col_dy * q_steps;

	int r_steps = 0;
	if(new_yoff > 0) {
		r_steps = (new_yoff + row_dy/2) / row_dy;
	}
	else if(new_yoff < 0) {
		r_steps = (new_yoff - row_dy/2) / row_dy;
	}
	new_ij.y -= r_steps;
	new_yoff -= row_dy * r_steps;

	new_ij = world->get_closest_coordinate(new_ij);

	//position changed? => update and mark dirty
	if(new_ij!=ij_off  ||  new_xoff!=x_off  ||  new_yoff!=y_off) {
		ij_off = new_ij;
		x_off = new_xoff;
		y_off = new_yoff;
		world->set_dirty();
		update_cached_values();
	}
}


void viewport_t::switch_underground_mode(const koord3d& pos)
{
	if (grund_t *gr = world->lookup(pos)) {
		if (!gr->is_visible()) {
			if (gr->ist_tunnel()) {
				// position is underground (and not visible), change to sliced mode
				grund_t::set_underground_mode(grund_t::ugm_level, gr->get_hoehe());
			}
			else if (!gr->ist_karten_boden()  ||  grund_t::underground_mode != grund_t::ugm_all) {
				// position is overground, change to normal view
				// but not if we are in full underground view and gr is kartenboden
				grund_t::set_underground_mode(grund_t::ugm_none, 0);
			}
			// make dirty etc
			world->update_underground();
		}
	}
}


// change the center viewport position for a certain ground tile
// any possible convoi to follow will be disabled
void viewport_t::change_world_position( const koord3d& new_ij, bool automatic_underground)
{
	follow_convoi = convoihandle_t();
	if (automatic_underground) {
		switch_underground_mode(new_ij);
	}
	change_world_position( get_map2d_coord( new_ij ) );
}


void viewport_t::change_world_position(const koord3d& pos, const koord& off, scr_coord sc)
{
	// Pick new (ij_off, x_off, y_off) such that `pos` lands at screen
	// coord `sc`.  Inverse of `get_screen_coord`.  Under hex, screen-x
	// only depends on the q-delta; screen-y depends on both q- and
	// r-deltas, so we solve for q first and absorb the column-y
	// component before solving for r.
	const sint32 col_dx = 3 * (cached_img_size / 4); // 3*W/4
	const sint32 col_dy =     (cached_img_size / 4); // W/4
	const sint32 row_dy = 2 * (cached_img_size / 4); // W/2

	const sint32 xfix = tile_raster_scale_x(off.x, cached_img_size);
	const sint32 yfix = tile_raster_scale_y(off.y-pos.z*TILE_HEIGHT_STEP, cached_img_size);

	// Δaxial = (target tile P) - (new view origin).  P is `pos` minus
	// the constant `view_ij_off` (the centre→top-left offset).
	const sint32 P_x = pos.x - view_ij_off.x;
	const sint32 P_y = pos.y - view_ij_off.y;

	const sint32 DX = sc.x - xfix;
	const sint32 DY = sc.y - yfix;

	// Solve for the integer (dq, dr) closest to the fractional answer,
	// leaving the residual in (new_x_off, new_y_off).
	sint32 dq;
	if(DX >= 0) {
		dq = (DX + col_dx/2) / col_dx;
	}
	else {
		dq = (DX - col_dx/2) / col_dx;
	}
	const sint32 DY_after_dq = DY - col_dy * dq;
	sint32 dr;
	if(DY_after_dq >= 0) {
		dr = (DY_after_dq + row_dy/2) / row_dy;
	}
	else {
		dr = (DY_after_dq - row_dy/2) / row_dy;
	}

	const sint16 new_ij_x = P_x - dq;
	const sint16 new_ij_y = P_y - dr;
	const sint16 new_x_off = DX - dq * col_dx;
	const sint16 new_y_off = DY - dq * col_dy - dr * row_dy;

	change_world_position(koord(new_ij_x, new_ij_y), new_x_off, new_y_off);
}


grund_t* viewport_t::get_ground_on_screen_coordinate(scr_coord screen_pos, sint32 &found_i, sint32 &found_j, const bool intersect_grid, double *frac_dq, double *frac_dr) const
{
	const int rw1 = cached_img_size;

	// Shift mouse coords from sprite-anchor frame to visible-centre
	// frame, where the hex inverse projection lives.  Pakset and synth
	// tiles agree on the legacy anchor → centre offset of
	// `(W/2, hex_visible_centre_y(W))`; see hex_proj.h.
	screen_pos.x += - x_off - rw1/2;
	screen_pos.y += - y_off - hex_visible_centre_y(rw1);

	const sint32 view_origin_x = ij_off.x + get_viewport_ij_offset().x;
	const sint32 view_origin_y = ij_off.y + get_viewport_ij_offset().y;

	bool found = false;
	// uncomment to: ctrl-key selects ground
	//bool select_karten_boden = event_get_last_control_shift()==2;

	// fallback: take kartenboden if nothing else found
	grund_t *bd = NULL;
	grund_t *gr = NULL;
	// Fractional residual within the picked tile (sub-tile axial
	// offset).  Captured at the iteration that lands on the returned
	// ground so callers can recover the cursor's sub-tile position
	// (e.g. for the hex corner picker).  Reset on every loop turn
	// since the height changes the projection.
	double last_frac_dq = 0, last_frac_dr = 0;
	// for the calculation of hmin/hmax see simview.cc
	// for the definition of underground_level see grund_t::set_underground_mode
	const sint8 hmin = grund_t::underground_mode != grund_t::ugm_all ? min( world->get_groundwater() - 4, grund_t::underground_level ) : world->get_min_allowed_height();
	const sint8 hmax = grund_t::underground_mode == grund_t::ugm_all ? world->get_max_allowed_height() : min( grund_t::underground_level, world->get_max_allowed_height() );

	// find matching and visible grund
	for(sint8 hgt = hmax; hgt>=hmin; hgt--) {

		// Hex inverse: project the click onto the ground plane at
		// height `hgt` (z lifts sprites by `tile_raster_scale_y(z*TSH)`,
		// so the inverse adds that back to screen-y), then cube-round
		// the fractional axial to the nearest hex.
		const sint32 z_dy = tile_raster_scale_y(hgt*TILE_HEIGHT_STEP, rw1);
		double q_f, r_f;
		hex_screen_to_fractional(screen_pos.x, screen_pos.y + z_dy, rw1, q_f, r_f);
		const koord delta = hex_round_to_axial(q_f, r_f);
		found_i = view_origin_x + delta.x;
		found_j = view_origin_y + delta.y;
		last_frac_dq = q_f - delta.x;
		last_frac_dr = r_f - delta.y;

		gr = world->lookup(koord3d(found_i,found_j,hgt));
		if (gr != NULL) {
			found = /*select_karten_boden ? gr->ist_karten_boden() :*/ gr->is_visible();
			if( ( gr->get_typ() == grund_t::tunnelboden || gr->get_typ() == grund_t::monorailboden ) && gr->get_weg_nr(0) == NULL && !gr->get_leitung()  &&  gr->find<zeiger_t>()) {
				// This is only a dummy ground placed by tool_build_tunnel_t or tool_build_way_t as a preview.
				found = false;
			}
			if (found) {
				break;
			}

			if (bd==NULL && gr->ist_karten_boden()) {
				bd = gr;
			}
		}
		if (grund_t::underground_mode==grund_t::ugm_level && hgt==hmax) {
			// fallback in sliced mode, if no ground is under cursor
			bd = world->lookup_kartenboden(found_i,found_j);
		}
		else if (intersect_grid){
			// We try to intersect with virtual nonexistent border tiles in south and east.
			if(  (gr = world->lookup_gridcoords( koord3d( found_i, found_j, hgt ) ))  ){
				found = true;
				break;
			}
		}

		// Last resort, try to intersect with the same tile +1 height, seems to be necessary on steep slopes
		// *NOTE* Don't do it on border tiles, since it will extend the range in which the cursor will be considered to be
		// inside world limits.
		if( found_i==(world->get_size().x-1)  ||  found_j == (world->get_size().y-1) ) {
			continue;
		}
		gr = world->lookup(koord3d(found_i,found_j,hgt+1));
		if(gr != NULL) {
			found = /*select_karten_boden ? gr->ist_karten_boden() :*/ gr->is_visible();
			if( gr->is_dummy_ground() && gr->find<zeiger_t>()) {
				// This is only a dummy ground placed by tool_build_tunnel_t or tool_build_way_t as a preview.
				found = false;
			}
			if (found) {
				break;
			}
			if (bd==NULL && gr->ist_karten_boden()) {
				bd = gr;
			}
		}
	}

	if(found) {
		if (frac_dq) *frac_dq = last_frac_dq;
		if (frac_dr) *frac_dr = last_frac_dr;
		return gr;
	}
	else {
		if(bd!=NULL){
			found_i = bd->get_pos().x;
			found_j = bd->get_pos().y;
			// Kartenboden fallback: bd was captured at some height
			// other than the loop's final iteration, so last_frac_*
			// doesn't correspond to bd.  Zero the residual; callers
			// reading it get the tile centre, which maps to the E
			// corner — arbitrary but stable.
			if (frac_dq) *frac_dq = 0;
			if (frac_dr) *frac_dr = 0;
			return bd;
		}
		return NULL;
	}
}


koord3d viewport_t::get_new_cursor_position( const scr_coord &screen_pos, bool grid_coordinates, hex_corner_t::type *corner_out )
{
	const int rw4 = cached_img_size/4;

	int offset_y = 0;
	if(world->get_zeiger()->get_yoff() == Z_PLAN  ||  grid_coordinates) {
		// Grid tools pick an exact hex corner from the raw sub-tile mouse
		// position; applying the legacy square-grid y shift first would
		// bias the selected vertex downward.
	}
	else {
		// shifted by a quarter tile
		offset_y += rw4;
	}

	sint32 grid_x, grid_y;
	double frac_dq = 0, frac_dr = 0;
	const grund_t *bd = get_ground_on_screen_coordinate(scr_coord(screen_pos.x, screen_pos.y + offset_y), grid_x, grid_y, grid_coordinates, &frac_dq, &frac_dr);

	// no suitable location found (outside map, ...)
	if (!bd) {
		return koord3d::invalid;
	}

	// offset needed for the raise / lower tool.
	sint8 groff = 0;

	if( bd->is_visible()  &&  grid_coordinates) {
		// Pick the nearest rendered world vertex, not just the nearest
		// corner name on the already-rounded tile.  Shared hex vertices
		// have three owner names; choosing globally keeps the displayed
		// cursor and the tile/corner handed to the terraform tool in the
		// same frame.
		hex_corner_t::type corner = hex_corner_t::E;
		pick_nearest_hex_vertex_global(world, grid_x, grid_y, frac_dq, frac_dr, corner);
		bd = world->lookup_kartenboden_nocheck(grid_x, grid_y);
		if (corner_out) *corner_out = corner;
		const koord image_offset = hex_corner_cursor_draw_offset(corner);
		world->get_zeiger()->set_image_offset(image_offset);
		groff = bd->get_hoehe(corner) - bd->get_hoehe();
		return koord3d(grid_x, grid_y, bd->get_disp_height() + groff);
	}
	else {
		world->get_zeiger()->set_image_offset(koord(0,0));
	}

	return koord3d(grid_x, grid_y, bd->get_disp_height() + groff);
}


void viewport_t::metrics_updated()
{
	const scr_size screen = gfx->get_screen_size();
	cached_disp_width  = screen.w;
	cached_disp_height = screen.h;
	cached_img_size    = gfx->get_tile_raster_width();

	// `view_ij_off` is the axial (q, r) delta from the view-centre tile
	// (`ij_off`) to the tile at the screen top-left.  Solving the hex
	// forward projection for tile-position = centre-screen at top-left
	// pixel (-disp_w/2, -disp_h/2):
	//     -view.q · 3·W/4              = -disp_w / 2
	//     (-view.q + -2·view.r) · W/4  = -disp_h / 2
	// gives view.q = -2·disp_w / (3·W) and view.r = disp_w/(3·W) - disp_h/W.
	set_viewport_ij_offset(koord(
		- 2 * cached_disp_width  / (3 * cached_img_size),
		      cached_disp_width  / (3 * cached_img_size) - cached_disp_height / cached_img_size
		) );
}


void viewport_t::rotate90( sint16 y_size )
{
	ij_off.rotate90(y_size);
	update_cached_values();
}


viewport_t::viewport_t( karte_t *world, const koord ij_off , sint16 x_off , sint16 y_off )
	: prepared_rect()
{
	this->world = world;
	assert(world);

	follow_convoi = convoihandle_t();

	this->ij_off = ij_off;
	this->x_off = x_off;
	this->y_off = y_off;

	metrics_updated();

}
