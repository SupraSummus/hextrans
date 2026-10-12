/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 */

#include "vehicle_base.h"

#include "road_vehicle.h"
#include "simroadtraffic.h"
#include "vehicle.h"

#include "../dataobj/environment.h"
#include "../simunits.h"
#include "../world/simworld.h"
#include "../obj/crossing.h"
#include "../simconvoi.h"


/* get dx and dy from dir (just to remind you)
 * any vehicle (including city cars and pedestrians)
 * will go this distance per sync step.
 * (however, the real dirs are only calculated during display, these are the old ones)
 *
 * HEX-PORT: under flat-top axial coords the 6 neighbour displacements
 * project onto the existing 2:1 isometric viewport as:
 *   N  (0,-1): screen (+2, -1) — "normal" tile step
 *   S  (0, 1): screen (-2, +1) — "normal" tile step
 *   SE (1, 0): screen (+2, +1) — takes over the old square "east"
 *   NW (-1,0): screen (-2, -1) — takes over the old square "west"
 *   NE (1,-1): screen (+4,  0) — visual-length of old square diagonals
 *   SW (-1,1): screen (-4,  0) — visual-length of old square diagonals
 *
 * Keyed by the hex edge, not by ribi_t::get_dir(): that slot index puts
 * SE and NW on the old square-diagonal slots, which are (0,2) and (0,-2).
 */
void vehicle_base_t::set_dxdy(ribi_t::ribi dir)
{
	switch(dir) {
		case ribi_t::north:     dx =  2; dy = -1; break;
		case ribi_t::south:     dx = -2; dy =  1; break;
		case ribi_t::southeast: dx =  2; dy =  1; break;
		case ribi_t::northwest: dx = -2; dy = -1; break;
		case ribi_t::northeast: dx =  4; dy =  0; break;
		case ribi_t::southwest: dx = -4; dy =  0; break;
		default:                dx =  0; dy =  0; break;
	}
}


// Steps across a bend tile, on the chord between the middles of the entry and
// exit edges: edges 120 degrees apart (a 60 degree turn) are 256 cos 30 apart,
// edges 60 degrees apart (a 120 degree turn) 256 / 2.
static const uint16 STEPS_PER_WIDE_BEND  = 222;
static const uint16 STEPS_PER_SHARP_BEND = VEHICLE_STEPS_PER_TILE / 2;

// The screen path through a bend is still the square-era one (TODO.md
// "Phase C"): 181 steps for a 60 degree bend, 256 for a 120 degree one, and a
// step with dx or dy 0 covers 724/1024 of one.
static const uint16 LEGACY_WIDE_BEND_SCREEN_STEPS = 181;
static const uint16 LEGACY_DIAGONAL_SCREEN_SCALE = 724;


uint16 vehicle_base_t::get_tile_steps(ribi_t::ribi edges)
{
	if(  !ribi_t::is_bend(edges)  ) {
		return VEHICLE_STEPS_PER_TILE;
	}
	// two neighbouring edges: rotating one by 60 degrees gives the other
	return (ribi_t::rotate60(edges) & edges) ? STEPS_PER_SHARP_BEND : STEPS_PER_WIDE_BEND;
}

// The lane offset tables have a row per square-era direction, which is a
// screen step dx, dy.  A hex direction takes the row of the square direction
// with its step in set_dxdy(): SE and NW the old east and west rows.  The
// (1,1) and (-1,-1) bend chords have no direction; their step (0, +-2) is the
// old SE / NW diagonal.
uint8 vehicle_base_t::get_lane_offset_row() const
{
	switch(direction) {
		case ribi_t::north:     return ribi_t::dir_north;
		case ribi_t::south:     return ribi_t::dir_south;
		case ribi_t::southeast: return ribi_t::dir_east;
		case ribi_t::northwest: return ribi_t::dir_west;
		case ribi_t::northeast: return ribi_t::dir_northeast;
		case ribi_t::southwest: return ribi_t::dir_southwest;
		default:                return dy > 0 ? ribi_t::dir_southeast : ribi_t::dir_northwest;
	}
}


// [0]=xoff [1]=yoff
sint8 vehicle_base_t::driveleft_base_offsets[8][2] =
{
	{  12,  6 },
	{ -12,  6 },
	{   0,  6 },
	{  12,  0 },
	{ -12, -6 },
	{  12, -6 },
	{   0, -6 },
	{ -12,  0 }
};

// [0]=xoff [1]=yoff
sint8 vehicle_base_t::overtaking_base_offsets[8][2];

// recalc offsets for overtaking
void vehicle_base_t::set_overtaking_offsets( bool driving_on_the_left )
{
	const sint8 sign = driving_on_the_left ? -1 : 1;
	// a tile has the internal size of
	const sint8 XOFF = 12;
	const sint8 YOFF = 6;

	overtaking_base_offsets[0][0] =  sign * XOFF;
	overtaking_base_offsets[1][0] = -sign * XOFF;
	overtaking_base_offsets[2][0] = 0;
	overtaking_base_offsets[3][0] =  sign * XOFF;
	overtaking_base_offsets[4][0] = -sign * XOFF;
	overtaking_base_offsets[5][0] =  sign * XOFF;
	overtaking_base_offsets[6][0] = 0;
	overtaking_base_offsets[7][0] =  sign * (-XOFF-YOFF);

	overtaking_base_offsets[0][1] =  sign * YOFF;
	overtaking_base_offsets[1][1] =  sign * YOFF;
	overtaking_base_offsets[2][1] =  sign * YOFF;
	overtaking_base_offsets[3][1] = 0;
	overtaking_base_offsets[4][1] = -sign * YOFF;
	overtaking_base_offsets[5][1] = -sign * YOFF;
	overtaking_base_offsets[6][1] = -sign * YOFF;
	overtaking_base_offsets[7][1] = 0;
}


/**
 * Checks if this vehicle must change the square upon next move
 * THIS IS ONLY THERE FOR LOADING OLD SAVES!
 */
bool vehicle_base_t::is_about_to_hop( const sint8 neu_xoff, const sint8 neu_yoff ) const
{
	const sint8 y_off_2 = 2*neu_yoff;
	const sint8 c_plus  = y_off_2 + neu_xoff;
	const sint8 c_minus = y_off_2 - neu_xoff;

	return ! (c_plus < OBJECT_OFFSET_STEPS*2  &&  c_minus < OBJECT_OFFSET_STEPS*2  &&  c_plus > -OBJECT_OFFSET_STEPS*2  &&  c_minus > -OBJECT_OFFSET_STEPS*2);
}


vehicle_base_t::vehicle_base_t():
	obj_t()
{
	image = IMG_EMPTY;
	set_flag( obj_t::is_vehicle );
	steps = 0;
	steps_next = VEHICLE_STEPS_PER_TILE - 1;
	use_calc_height = true;
	dx = 0;
	dy = 0;
	zoff_start = zoff_end = 0;
	disp_lane = 2;
}


vehicle_base_t::vehicle_base_t(koord3d pos):
	obj_t(pos)
{
	image = IMG_EMPTY;
	set_flag( obj_t::is_vehicle );
	pos_next = pos;
	steps = 0;
	steps_next = VEHICLE_STEPS_PER_TILE - 1;
	use_calc_height = true;
	dx = 0;
	dy = 0;
	zoff_start = zoff_end = 0;
	disp_lane = 2;
}





void vehicle_base_t::rotate90()
{
	obj_t::rotate90();
	direction = ribi_t::rotate_for_map_rotate90(direction );
	pos_next.rotate90( welt->get_map_shape() );
	// The screen step follows from the direction (calc_set_direction);
	// only the (1,1) / (-1,-1) bend chords have none, and those turn into
	// each other on the half turn.
	if(  direction != ribi_t::none  ) {
		set_dxdy( direction );
	}
	else if(  welt->get_settings().is_half_turn_rotation()  ) {
		dx = -dx;
		dy = -dy;
	}
}


void vehicle_base_t::leave_tile()
{
	grund_t* gr = welt->lookup(get_pos());

	// first: release crossing
	if (gr) {
		if (crossing_t* cr = gr->get_crossing()) {
			grund_t* gr2 = welt->lookup(pos_next);
			bool release = gr2==NULL ||  gr2 == gr;
			if (!release) {
				// check if we stay on crossing
				if (crossing_t* cr2 = gr2->get_crossing()) {
					// only release if new crossing ahead
					release = (cr->get_logic() != cr2->get_logic());
				}
			}
			if (release) {
				cr->release_crossing(this);
			}
			else {
				// release when no longer a crossing
				cr->release_crossing(this);
			}
		}
	}

	// then remove from ground (or search whole map, if failed)
	if(!get_flag(not_on_map)  &&  (gr==NULL  ||  !gr->obj_remove(this)) ) {

		// was not removed (not found?)
		dbg->error("vehicle_base_t::leave_tile()", "%s %p could not be removed from (%s)", get_name(), (void *)this, get_pos().get_str());
		DBG_MESSAGE("vehicle_base_t::leave_tile()", "checking all plan squares");

		// check, whether it is on another height ...
		const planquadrat_t *pl = welt->access( get_pos().get_2d() );
		if(  pl  ) {
			gr = pl->get_boden_von_obj(this);
			if(  gr  ) {
				gr->obj_remove(this);
				dbg->warning("vehicle_base_t::leave_tile()", "Removed %s %p from (%s)", get_name(), (void *)this, get_pos().get_str());
			}
			return;
		}

		bool ok = false;

		welt->for_each_tile( [&](koord k) {
			grund_t *gr = welt->access_nocheck( k )->get_boden_von_obj(this);
			if(gr && gr->obj_remove(this)) {
				dbg->warning("vehicle_base_t::leave_tile()", "Removed %s %p from (%s)", get_name(), (void *)this, k.get_str());
				ok = true;
			}
		} );

		if(!ok) {
			dbg->error("vehicle_base_t::leave_tile()", "%s %p was not found on any map square!", get_name(), (void *)this);
		}
	}
}


void vehicle_base_t::enter_tile(grund_t* gr)
{
	if(!gr) {
		dbg->error("vehicle_base_t::enter_tile()","'%s' new position (%i,%i,%i)!",get_name(), get_pos().x, get_pos().y, get_pos().z );
		gr = welt->lookup_kartenboden(get_pos().get_2d());
		set_pos( gr->get_pos() );
	}
	gr->obj_add(this);
}


/* THE routine for moving vehicles
 * it will drive on as log as it can
 * @return the distance actually traveled
 */
uint32 vehicle_base_t::do_drive(uint32 distance)
{

	uint32 steps_to_do = distance >> YARDS_PER_VEHICLE_STEP_SHIFT;

	if(  steps_to_do == 0  ) {
		// ok, we will not move in this steps
		return 0;
	}
	// ok, so moving ...
	if(  !get_flag(obj_t::dirty)  ) {
		mark_image_dirty( image, 0 );
		set_flag( obj_t::dirty );
	}

	grund_t *gr = NULL; // if hopped, then this is new position

	uint32 steps_target = steps_to_do + steps;

	uint32 distance_travelled; // Return value

	if(  steps_target > steps_next  ) {
		// We are going far enough to hop.

		// We'll be adding steps_next+1 for each hop, as if we
		// started at the beginning of this tile, so for an accurate
		// count of steps done we must subtract the location we started with.
		sint32 steps_done = -steps;

		// Hop as many times as possible.
		while(  steps_target > steps_next  &&  (gr = hop_check())  ) {
			// now do the update for hopping
			steps_target -= steps_next+1;
			steps_done += steps_next+1;
			koord pos_prev(get_pos().get_2d());
			hop(gr);
			use_calc_height = true;

			// set offsets
			set_xoff( (dx<0) ? OBJECT_OFFSET_STEPS : -OBJECT_OFFSET_STEPS );
			set_yoff( (dy<0) ? OBJECT_OFFSET_STEPS/2 : -OBJECT_OFFSET_STEPS/2 );
			if(dx*dy==0) {
				if(dx==0) {
					if(dy>0) {
						set_xoff( pos_prev.x!=get_pos().x ? -OBJECT_OFFSET_STEPS : OBJECT_OFFSET_STEPS );
					}
					else {
						set_xoff( pos_prev.x!=get_pos().x ? OBJECT_OFFSET_STEPS : -OBJECT_OFFSET_STEPS );
					}
				}
				else {
					if(dx>0) {
						set_yoff( pos_prev.y!=get_pos().y ? OBJECT_OFFSET_STEPS/2 : -OBJECT_OFFSET_STEPS/2 );
					}
					else {
						set_yoff( pos_prev.y!=get_pos().y ? -OBJECT_OFFSET_STEPS/2 : OBJECT_OFFSET_STEPS/2 );
					}
				}
			}
		}

		if(  steps_next == 0  ) {
			// only needed for aircrafts, which can turn on the same tile
			// the indicate the turn with this here
			steps_next = VEHICLE_STEPS_PER_TILE - 1;
			steps_target = VEHICLE_STEPS_PER_TILE - 1;
			steps_done -= VEHICLE_STEPS_PER_TILE - 1;
		}

		// Update internal status, how far we got within the tile.
		if(  steps_target <= steps_next  ) {
			steps = steps_target;
		}
		else {
			// could not go as far as we wanted (hop_check failed) => stop at end of tile
			steps = steps_next;
		}

		steps_done += steps;
		distance_travelled = steps_done << YARDS_PER_VEHICLE_STEP_SHIFT;

	}
	else {
		// Just travel to target, it's on same tile
		steps = steps_target;
		distance_travelled = distance & YARDS_VEHICLE_STEP_MASK; // round down to nearest step
	}

	if(use_calc_height) {
		calc_height(gr);
	}
	// remaining steps
	return distance_travelled;
}


sint32 vehicle_base_t::get_display_steps(uint32 s, sint16 raster_width) const
{
	// stretch the steps of a bend onto its square-era screen path
	if(  steps_next == STEPS_PER_WIDE_BEND - 1  ) {
		s = (s * (LEGACY_WIDE_BEND_SCREEN_STEPS - 1)) / steps_next;
	}
	else if(  steps_next == STEPS_PER_SHARP_BEND - 1  ) {
		s = (s * (VEHICLE_STEPS_PER_TILE - 1)) / steps_next;
	}
	sint32 display_steps = s*(uint16)raster_width;
	if(dx && dy) {
		display_steps &= 0xFFFFFC00;
	}
	else {
		display_steps = (display_steps*LEGACY_DIAGONAL_SCREEN_SCALE)>>10;
	}
	return display_steps;
}


// to make smaller steps than the tile granularity, we have to use this trick
void vehicle_base_t::get_screen_offset( int &xoff, int &yoff, const sint16 raster_width ) const
{
	// vehicles needs finer steps to appear smoother
	const sint32 display_steps = get_display_steps( steps, raster_width );
	xoff += (display_steps*dx) >> 10;
	yoff += ((display_steps*dy) >> 10) + (get_hoff(raster_width))/(4*16);
}


// Direction through a tile for the displacement start -> ende: one edge
// step (tile to next tile) or two (tile before to tile after).
// A displacement on a hex axis names its edge.  The chords of the 60 degree
// bends are off-axis and are sorted into NE / SW by their signs, except
// (1,1) and (-1,-1) (the SE-S and NW-N bends), which give none.
ribi_t::ribi vehicle_base_t::calc_chord_direction(const koord& start, const koord& ende)
{
	const sint32 di = ende.x - start.x;
	const sint32 dj = ende.y - start.y;

	if(dj < 0 && di == 0) {          // N
		return ribi_t::north;
	} else if(dj > 0 && di == 0) {   // S
		return ribi_t::south;
	} else if(di < 0 && dj == 0) {   // NW (was square "west")
		return ribi_t::northwest;
	} else if(di >0 && dj == 0) {    // SE (was square "east")
		return ribi_t::southeast;
	} else if(di > 0 && dj < 0) {    // NE
		return ribi_t::northeast;
	} else if(di < 0 && dj > 0) {    // SW
		return ribi_t::southwest;
	}
	return ribi_t::none;
}


// Steps to cross the tile cur, from the edge towards prev to the edge
// towards next.  The two-tile chord prev -> next cannot tell a 120 degree bend
// from a single step, both are one edge long.
uint8 vehicle_base_t::calc_chord_steps_next(const koord3d& prev, const koord3d& cur, const koord3d& next)
{
	const ribi_t::ribi edges = ribi_type(cur.get_2d(), prev.get_2d()) | ribi_type(cur.get_2d(), next.get_2d());
	return (uint8)(get_tile_steps(edges) - 1);
}


// calcs new direction and applies it to the vehicles
//
// HEX-PORT: dx, dy per edge are in set_dxdy(), tile length in
// calc_chord_steps_next().  The (1,1) and (-1,-1) chords come from the
// SE-S and NW-N bends and keep the legacy square-diagonal screen step
// (0,+-2) until the viewport port lands.
ribi_t::ribi vehicle_base_t::calc_set_direction(const koord3d& prev, const koord3d& cur, const koord3d& next)
{
	const ribi_t::ribi direction = calc_chord_direction(prev, next);

	steps_next = calc_chord_steps_next(prev, cur, next);
	set_dxdy(direction);
	if(direction == ribi_t::none) {
		dy = (next.y > prev.y) ? 2 : -2;
	}
	return direction;
}


// this routine calculates the new height
// beware of bridges, tunnels, slopes, ...
void vehicle_base_t::calc_height(grund_t *gr)
{
	use_calc_height = false;   // assume, we are only needed after next hop
	zoff_start = zoff_end = 0; // assume flat way

	if(gr==NULL) {
		gr = welt->lookup(get_pos());
	}
	if(gr==NULL) {
		// slope changed below a moving thing?!?
		return;
	}
	else if(gr->ist_tunnel()  &&  gr->ist_karten_boden()) {
		// tunnel portals: maybe need hiding after half tile
		use_calc_height = true; // to avoid errors if underground mode is switched
		if(  grund_t::underground_mode == grund_t::ugm_none  ||
			(grund_t::underground_mode == grund_t::ugm_level  &&  gr->get_hoehe() < grund_t::underground_level)
		) {
			// need hiding? One of the few uses of XOR: not half driven XOR exiting => not hide!
			ribi_t::ribi hang_ribi = ribi_type( gr->get_grund_hang() );
			if((steps<(steps_next/2))  ^  ((hang_ribi&direction)!=0)  ) {
				set_image(IMG_EMPTY);
			}
			else {
				calc_image();
			}
		}
	}
	else {
		// force a valid image above ground, with special handling of tunnel entraces
		if(  get_image()==IMG_EMPTY  ) {
			if(  !gr->ist_tunnel()  &&  gr->ist_karten_boden()  ) {
				calc_image();
			}
		}

		// will not work great with ways, but is very short!
		slope_t::type hang = gr->get_weg_hang();
		if(  hang  ) {
			const uint slope_height = is_one_high(hang) ? 1 : 2;
			ribi_t::ribi hang_ribi = ribi_type(hang);
			if(  ribi_t::doubles(hang_ribi)  ==  ribi_t::doubles(direction)) {
				zoff_start = hang_ribi & direction                   ? 2*slope_height : 0;  // 0 .. 4
				zoff_end   = hang_ribi & ribi_t::backward(direction) ? 2*slope_height : 0;  // 0 .. 4
			}
			else {
				// only for shadows and movingobjs ...
				zoff_start = hang_ribi & direction                   ? slope_height+1 : 0;  // 0 .. 3
				zoff_end   = hang_ribi & ribi_t::backward(direction) ? slope_height+1 : 0;  // 0 .. 3
			}
		}
		else {
			zoff_start = (gr->get_weg_yoff() * 2) / TILE_HEIGHT_STEP;
			zoff_end = zoff_start;
		}
	}
}


sint16 vehicle_base_t::get_hoff(const sint16 raster_width) const
{
	sint16 h_start = -(sint8)TILE_HEIGHT_STEP * (sint8)zoff_start;
	sint16 h_end   = -(sint8)TILE_HEIGHT_STEP * (sint8)zoff_end;
	return ((h_start*steps + h_end*(256-steps))*raster_width) >> 9;
}


/* true, if one could pass through this field
 * also used for citycars, thus defined here
 */
vehicle_base_t *vehicle_base_t::no_cars_blocking( const grund_t *gr, const convoi_t *cnv, const uint8 current_direction, const uint8 next_direction, const uint8 next_90direction )
{
	// Search vehicle
	for(  uint8 pos=1;  pos<(uint8)gr->obj_count();  pos++  ) {
		if(  vehicle_base_t* const v = obj_cast<vehicle_base_t>(gr->obj_bei(pos))  ) {
			if(  v->get_typ()==obj_t::pedestrian  ) {
				continue;
			}

			// check for car
			uint8 other_direction=255;
			bool other_moving = false;
			if(  road_vehicle_t const* const at = obj_cast<road_vehicle_t>(v)  ) {
				// ignore ourself
				if(  cnv == at->get_convoi()  ) {
					continue;
				}
				other_direction = at->get_direction();
				other_moving = at->get_convoi()->get_akt_speed() > kmh_to_speed(1);
			}
			// check for city car
			else if(  v->get_waytype() == road_wt  ) {
				other_direction = v->get_direction();
				if(  private_car_t const* const sa = obj_cast<private_car_t>(v)  ){
					other_moving = sa->get_current_speed() > 1;
				}
			}

			// ok, there is another car ...
			if(  other_direction != 255  ) {
				if(  next_direction == other_direction  &&  !ribi_t::is_threeway(gr->get_weg_ribi(road_wt))  ) {
					// cars going in the same direction and no crossing => that mean blocking ...
					return v;
				}

				const ribi_t::ribi other_90direction = (gr->get_pos().get_2d() == v->get_pos_next().get_2d()) ? other_direction : calc_direction(gr->get_pos(), v->get_pos_next());
				if(  other_90direction == next_90direction  ) {
					// Want to exit in same as other   ~50% of the time
					return v;
				}

				// HEX-PORT: 4-way crossroads collision logic baked in
				// rotate45 (one-lane cross) and rotate90 (perpendicular).
				// Under hex, crossroads have 3 or 6 arms at 60° spacing;
				// rotate45 → rotate60, rotate90 → two rotate60 calls
				// (via rotate_perpendicular).  Collision
				// behaviour is approximate; a real hex-crossroads port
				// is tracked in TODO.md → "Building / crossing cluster".
				const bool drives_on_left = welt->get_settings().is_drive_left();
				const bool across = next_direction == (drives_on_left ? ribi_t::rotate60l(next_90direction) : ribi_t::rotate60(next_90direction)); // turning across the opposite directions lane
				const bool other_across = other_direction == (drives_on_left ? ribi_t::rotate60l(other_90direction) : ribi_t::rotate60(other_90direction)); // other is turning across the opposite directions lane
				if(  other_direction == next_direction  &&  !(other_across || across)  ) {
					// entering same straight waypoint as other ~18%
					return v;
				}

				const bool straight = next_direction == next_90direction; // driving straight
				const ribi_t::ribi current_90direction = straight ? ribi_t::backward(next_90direction) : (ribi_t::ribi)((~(next_direction|ribi_t::backward(next_90direction))) & ribi_t::all);
				const bool other_straight = other_direction == other_90direction; // other is driving straight
				const bool other_exit_same_side = current_90direction == other_90direction; // other is exiting same side as we're entering
				const bool other_exit_opposite_side = ribi_t::backward(current_90direction) == other_90direction; // other is exiting side across from where we're entering
				if(  across  &&  ((ribi_t::is_perpendicular(current_90direction,other_direction)  &&  other_moving)  ||  (other_across  &&  other_exit_opposite_side)  ||  ((other_across  ||  other_straight)  &&  other_exit_same_side  &&  other_moving) ) )  {
					// other turning across in front of us from orth entry dir'n   ~4%
					return v;
				}

				const bool headon = ribi_t::backward(current_direction) == other_direction; // we're meeting the other headon
				// HEX-PORT: old rotate90 → rotate_perpendicular.
				const ribi_t::ribi perpendicular_90 = drives_on_left
					? ribi_t::rotate_perpendicular_l(next_90direction)
					: ribi_t::rotate60 (ribi_t::rotate60 (next_90direction));
				const bool other_exit_across = perpendicular_90 == other_90direction; // other is exiting by turning across the opposite directions lane
				if(  straight  &&  (ribi_t::is_perpendicular(current_90direction,other_direction)  ||  (other_across  &&  other_moving  &&  (other_exit_across  ||  (other_exit_same_side  &&  !headon))) ) ) {
					// other turning across in front of us, but allow if other is stopped - duplicating historic behaviour   ~2%
					return v;
				}
				else if(  other_direction == current_direction  &&  current_90direction == ribi_t::none  ) {
					// entering same diagonal waypoint as other   ~1%
					return v;
				}

				// else other car is not blocking   ~25%
			}
		}
	}

	// way is free
	return NULL;
}
