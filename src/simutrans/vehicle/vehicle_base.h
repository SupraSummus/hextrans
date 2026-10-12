/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 */

#ifndef VEHICLE_VEHICLE_BASE_H
#define VEHICLE_VEHICLE_BASE_H


#include "../obj/simobj.h"


class convoi_t;
class grund_t;
class overtaker_t;


/**
 * Base class for all moving things (vehicles, pedestrians, movingobj)
 */
class vehicle_base_t : public obj_t
{
protected:
	/// steps_next of the tile @p cur, entered from @p prev and left to @p next
	static uint8 calc_chord_steps_next(const koord3d& prev, const koord3d& cur, const koord3d& next);

	// [0]=xoff [1]=yoff, row get_lane_offset_row()
	static sint8 driveleft_base_offsets[8][2];
	static sint8 overtaking_base_offsets[8][2];

	/// Row of the lane offset tables for the screen step dx, dy
	uint8 get_lane_offset_row() const;

	/**
	 * Actual travel direction in screen coordinates
	 */
	ribi_t::ribi direction;

	// true on slope (make calc_height much faster)
	uint8 use_calc_height:1;

	/**
	 * Thing is moving on this lane.
	 * Possible values:
	 * (Back)
	 * 0 - sidewalk (going on the right side to w/sw/s)
	 * 1 - road     (going on the right side to w/sw/s)
	 * 2 - middle   (everything with waytype != road)
	 * 3 - road     (going on the right side to se/e/../nw)
	 * 4 - sidewalk (going on the right side to se/e/../nw)
	 * (Front)
	 */
	uint8 disp_lane:3;

	/// Screen vector of the chord the object follows through its tile, in
	/// W/8 (vehicle/screen_chord.h); xoff, yoff hold the chord's start.
	sint8 dx, dy;

	/**
	 * Sets dx, dy to a straight run across the tile along @p dir (both 0
	 * unless it is a single hex edge), for a saved direction or a new
	 * object; calc_set_direction() sets the exact chord.
	 */
	void set_dxdy(ribi_t::ribi dir);

	/// Sets dx, dy to the chord with axial span @p span (screen_chord.h).
	void set_chord(koord span);

	/// Sets xoff, yoff to the start of the chord dx, dy for an object that
	/// entered its tile from the neighbour @p to_prev (chord_entry_offset).
	void set_entry_offset(koord to_prev);

	// number of steps in this tile (255 per tile)
	uint8 steps, steps_next;

	/**
	 * Next position on our path
	 */
	koord3d pos_next;

	/**
	 * Offsets for uphill/downhill.
	 * Have to be multiplied with -TILE_HEIGHT_STEP/2.
	 * To obtain real z-offset, interpolate using steps, steps_next.
	 */
	uint8 zoff_start:4, zoff_end:4;

	// cached image
	image_id image;

	/**
	 * Vehicle movement: check whether this vehicle can enter the next tile (pos_next).
	 * @returns NULL if check fails, otherwise pointer to the next tile
	 */
	virtual grund_t* hop_check() = 0;

	/**
	 * Vehicle movement: change tiles, calls leave_tile and enter_tile.
	 * @param gr pointer to ground of new position (never NULL)
	 */
	virtual void hop(grund_t* gr) = 0;

	void calc_image() OVERRIDE = 0;

	// check for road vehicle, if next tile is free
	vehicle_base_t *no_cars_blocking( const grund_t *gr, const convoi_t *cnv, const uint8 current_direction, const uint8 next_direction, const uint8 next_90direction );

	// only needed for old way of moving vehicles to determine position at loading time
	bool is_about_to_hop( const sint8 neu_xoff, const sint8 neu_yoff ) const;

	/// Pixel offset @p s steps along the chord dx, dy at @p raster_width
	koord get_chord_offset(uint32 s, sint16 raster_width) const;

public:
	/// Steps to cross a tile entered and left by the two edges in @p edges:
	/// the chord between the middles of the two edges.  Anything but a
	/// bend is crossed straight.
	static uint16 get_tile_steps(ribi_t::ribi edges);

	static void set_overtaking_offsets( bool driving_on_the_left );

	uint32 do_drive(uint32 dist); // basis movement code

	inline void set_image( image_id b ) { image = b; }
	image_id get_image() const OVERRIDE {return image;}

	sint16 get_hoff(const sint16 raster_width=1) const;
	uint8 get_steps() const {return steps;}
	uint8 get_steps_next() const {return steps_next;}

	uint8 get_disp_lane() const { return disp_lane; }

	// to make smaller steps than the tile granularity, we have to calculate our offsets ourselves!
	virtual void get_screen_offset( int &xoff, int &yoff, const sint16 raster_width ) const;

	/**
	 * Vehicle movement: calculates z-offset of vehicles on slopes,
	 * handles vehicles that are invisible in tunnels.
	 * @param gr vehicle is on this ground
	 * @note has to be called after loading to initialize z-offsets
	 */
	void calc_height(grund_t *gr = NULL);

	void rotate90() OVERRIDE;

	/// Direction through a tile for the displacement @p start -> @p ende;
	/// what calc_set_direction() stores in `direction`.
	static ribi_t::ribi calc_chord_direction(const koord& start, const koord& ende);
	static ribi_t::ribi calc_chord_direction(const koord3d& start, const koord3d& ende) { return calc_chord_direction(start.get_2d(), ende.get_2d()); }

	/// The chords of a route classified as the vehicles' `direction`, so
	/// that the two compare on a 60 degree bend.
	template<class K1, class K2>
	static ribi_t::ribi calc_direction(const K1& from, const K2& to)
	{
		return calc_chord_direction(koord(from.x, from.y), koord(to.x, to.y));
	}

	/// Sets direction, dx, dy and steps_next for crossing the tile @p cur,
	/// entered from @p prev and left to @p next.
	ribi_t::ribi calc_set_direction(const koord3d& prev, const koord3d& cur, const koord3d& next);

	/// Same for a single step from @p start to @p ende, crossed straight.
	ribi_t::ribi calc_set_direction(const koord3d& start, const koord3d& ende) { return calc_set_direction(start, start, ende); }

	ribi_t::ribi get_direction() const {return direction;}

	/// Slot of the 8-direction sprite table for the heading on screen.
	ribi_t::dir get_sprite_dir() const;

	koord3d get_pos_next() const {return pos_next;}

	void set_pos_next(koord3d pn) { pos_next = pn; }

	waytype_t get_waytype() const OVERRIDE = 0;

	// true, if this vehicle did not moved for some time
	virtual bool is_stuck() { return true; }

	/**
	 * Vehicle movement: enter tile, add this to the ground.
	 * @pre position (obj_t::pos) needs to be updated prior to calling this functions
	 * @param gr pointer to ground (never NULL)
	 */
	virtual void enter_tile(grund_t *gr);

	/**
	 * Vehicle movement: leave tile, release reserved crossing, remove vehicle from the ground.
	 */
	virtual void leave_tile();

	virtual overtaker_t *get_overtaker() { return NULL; }

	vehicle_base_t();

	vehicle_base_t(koord3d pos);
};


template<> inline vehicle_base_t* obj_cast<vehicle_base_t>(obj_t* const d)
{
	return d->is_moving() ? static_cast<vehicle_base_t*>(d) : 0;
}


#endif
