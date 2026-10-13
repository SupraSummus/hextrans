/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 */

#ifndef PLAYER_AI_PASSENGER_H
#define PLAYER_AI_PASSENGER_H


#include "ai.h"

class marker_t;

/// Simple passenger transport AI
class ai_passenger_t : public ai_t
{
private:
	enum state {
		NR_INIT,
		NR_SAMMLE_ROUTEN,
		NR_BAUE_ROUTE1,
		NR_BAUE_AIRPORT_ROUTE,
		NR_BAUE_STRASSEN_ROUTE,
		NR_BAUE_WATER_ROUTE,
		NR_BAUE_CLEAN_UP,
		NR_SUCCESS,
		NR_WATER_SUCCESS,
		CHECK_CONVOI
	};

	// vars for the KI
	state state;

	// we will use this vehicle!
	const vehicle_desc_t *road_vehicle;

	// and the convoi will run on this track:
	const way_desc_t *road_weg ;

	// time to wait before next construction
	sint32 next_construction_steps;

	/* start and end stop position (and their size) */
	koord platz1, platz2;

	const stadt_t *start_stadt;
	const stadt_t *end_stadt; // target is town
	const gebaeude_t *end_attraction;
	fabrik_t *ziel;

	// marker field
	marker_t *marker;

	/// our hub of the city (a pax bus stop with a tile in the city) or zero;
	/// *hub_pos is set to the hub's road stop tile inside the city
	halthandle_t get_our_hub( const stadt_t *s, koord *hub_pos = NULL ) const;
	koord get_hub_pos( const stadt_t *s ) const;

	koord find_area_for_hub( const koord lo, const koord ru, const koord basis ) const;
	koord find_place_for_hub( const stadt_t *s ) const;

	/** builds harbours and ferries
	 */
	koord find_harbour_pos(karte_t* welt, const stadt_t *s );
	bool create_water_transport_vehicle(const stadt_t* start_stadt, const koord target_pos);

	// builds a simple 3x3 three stop airport with town connection road
	halthandle_t build_airport(const stadt_t* city, koord pos, int rotate);

	/** build airports and planes
	 */
	bool create_air_transport_vehicle(const stadt_t *start_stadt, const stadt_t *end_stadt);

	// helper function for bus stops intown
	void walk_city(linehandle_t line, grund_t* start, int limit);

	// tries to cover a city with bus stops that does not overlap much and cover as much as possible
	void cover_city_with_bus_route(koord start_pos, int number_of_stops);
	// number_of_stops for it: 6, or the policy's city_stops
	static int city_stops() { return ai_policy_t::passengers ? (int)ai_policy_t::passengers->city_stops : 6; }

	void create_bus_transport_vehicle(koord startpos,int vehicle_count,koord *stops,int count,bool do_wait);

	// what ai_policy_t scores a link from -> to by; to is NULL for an
	// attraction or factory at to_pos.  land[] labels the land masses
	void get_link_features( const stadt_t *from, const stadt_t *to, koord to_pos,
		const vector_tpl<uint32> &land, double features[ai_policy_t::PASSENGER_FEATURES] ) const;

	// label each land tile by the land mass it belongs to, water 0
	void label_land( vector_tpl<uint32> &land ) const;

	/* -ai_policy_passenger: score every pair of towns we do not join yet and
	 * set start_stadt, end_stadt and platz1 to the best one; false if none
	 * scores above zero or the cash reserve forbids building
	 */
	bool pick_by_policy();

	// -ai_policy_passenger: town pairs the policy picked this game, built or
	// not; it never picks one again (not saved: a policy only runs in -ai_soak)
	struct town_pair_t {
		const stadt_t *a, *b;
		bool operator==(const town_pair_t &o) const { return (a == o.a  &&  b == o.b)  ||  (a == o.b  &&  b == o.a); }
	};
	vector_tpl<town_pair_t> policy_tried;

	// -ai_policy_passenger: close our bus lines older than a year whose
	// convoys did not earn retire_line times the upkeep of their stops
	void close_unpaid_lines();

	// -ai_soak: trace_link_begin for the link start_stadt -> end_stadt or platz2
	void trace_begin();

public:
	ai_passenger_t(uint8 nr);

	// this type of AIs identifier
	uint8 get_ai_id() const OVERRIDE { return AI_PASSENGER; }

	// cannot do rail
	void set_rail_transport( bool ) OVERRIDE { rail_transport = false; }

	void report_vehicle_problem(convoihandle_t cnv,const koord3d ziel) OVERRIDE;

	void rdwr(loadsave_t *file) OVERRIDE;

	void finish_rd() OVERRIDE;

	bool set_active( bool b ) OVERRIDE;

	void step() OVERRIDE;
};

#endif
