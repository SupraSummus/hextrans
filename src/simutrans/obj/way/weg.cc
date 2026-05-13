/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 */

#include <stdio.h>

#include "weg.h"

#include "schiene.h"
#include "strasse.h"
#include "monorail.h"
#include "maglev.h"
#include "narrowgauge.h"
#include "kanal.h"
#include "runway.h"

#include "../gebaeude.h"

#include "../../ground/grund.h"
#include "../../world/simworld.h"
#include "../../display/simimg.h"
#include "../../simhalt.h"
#include "../../obj/simobj.h"
#include "../../player/simplay.h"
#include "../../obj/roadsign.h"
#include "../../obj/signal.h"
#include "../../obj/crossing.h"
#include "../../utils/cbuffer.h"
#include "../../dataobj/environment.h" // TILE_HEIGHT_STEP
#include "../../dataobj/translator.h"
#include "../../dataobj/loadsave.h"
#include "../../descriptor/way_desc.h"
#include "../../descriptor/roadsign_desc.h"

#include "../../tpl/slist_tpl.h"

#ifdef MULTI_THREAD
#include "../../utils/simthread.h"
static pthread_mutex_t weg_calc_image_mutex;
static recursive_mutex_maker_t weg_cim_maker(weg_calc_image_mutex);
#endif

/**
 * Alle instantiierten Wege
 */
slist_tpl <weg_t *> alle_wege;

uint16 weg_t::cityroad_speed = 50;

/**
 * Get list of all ways
 */
const slist_tpl <weg_t*> &weg_t::get_alle_wege()
{
	return alle_wege;
}


bool weg_t::is_clipping_below_needed() const
{
	// elevated no clip?
	return desc->is_clip_below();
}


void weg_t::set_cityroad_speedlimit(uint16 new_limit)
{
	if (cityroad_speed != new_limit) {
		cityroad_speed = new_limit;
		for(weg_t *w : alle_wege) {
			if(  w->hat_gehweg()  &&  w->get_waytype() == road_wt  ) {
				if (const way_desc_t* desc = w->get_desc()) {
					w->set_max_speed(min(desc->get_topspeed(), cityroad_speed));
				}
				else {
					// should never occur ???
					w->set_max_speed(cityroad_speed);
				}
			}
		}
	}
}


// returns a way with matching waytype
weg_t* weg_t::alloc(waytype_t wt)
{
	weg_t *weg = NULL;
	switch(wt) {
		case tram_wt:
		case track_wt:
			weg = new schiene_t();
			break;
		case monorail_wt:
			weg = new monorail_t();
			break;
		case maglev_wt:
			weg = new maglev_t();
			break;
		case narrowgauge_wt:
			weg = new narrowgauge_t();
			break;
		case road_wt:
			weg = new strasse_t();
			break;
		case water_wt:
			weg = new kanal_t();
			break;
		case air_wt:
			weg = new runway_t();
			break;
		default:
			// keep compiler happy; should never reach here anyway
			assert(0);
			break;
	}
	return weg;
}


// returns a string with the "official name of the waytype"
const char *weg_t::waytype_to_string(waytype_t wt)
{
	switch(wt) {
		case tram_wt:        return "tram_track";
		case track_wt:       return "track";
		case monorail_wt:    return "monorail_track";
		case maglev_wt:      return "maglev_track";
		case narrowgauge_wt: return "narrowgauge_track";
		case road_wt:        return "road";
		case water_wt:       return "water";
		case air_wt:         return "air";
		// these are just for translation
		case powerline_wt:   return "power";
		case decoration_wt:  return "decoration";

		default:             return "invalid waytype";
	}
}


void weg_t::set_desc(const way_desc_t *b)
{
	desc = b;

	if(  hat_gehweg() &&  desc->get_wtyp() == road_wt  &&  desc->get_topspeed() > cityroad_speed  ) {
		max_speed = cityroad_speed;
	}
	else {
		max_speed = desc->get_topspeed();
	}
}


/**
 * initializes statistic array
 */
void weg_t::init_statistics()
{
	for(  int type=0;  type<MAX_WAY_STATISTICS;  type++  ) {
		for(  int month=0;  month<MAX_WAY_STAT_MONTHS;  month++  ) {
			statistics[month][type] = 0;
		}
	}
}


/**
 * Initializes all member variables
 */
void weg_t::init()
{
	ribi = ribi_maske = ribi_t::none;
	max_speed = 450;
	desc = 0;
	init_statistics();
	alle_wege.insert(this);
	flags = 0;
	image = IMG_EMPTY;
	foreground_image = IMG_EMPTY;
}


weg_t::~weg_t()
{
	alle_wege.remove(this);
	player_t *player=get_owner();
	if(player) {
		player_t::add_maintenance( player,  -desc->get_maintenance(), desc->get_finance_waytype() );
	}
}


bool weg_t::needs_crossing(const way_desc_t* other) const
{
	// certain way always needs crossing (or never)
	switch (desc->get_waytype()) {
		case powerline_wt:
			return false;
		case water_wt:
		case air_wt:
		case decoration_wt:
			return true;
		default:
			break;
	}
	switch (other->get_waytype()) {
		case powerline_wt:
			return false;
		case water_wt:
		case air_wt:
		case decoration_wt:
			return true;
		default:
			break;
	}
	// only now we can check if there is a tramway involved
	if (desc->get_styp() == type_tram) {
		// we are tramway
		return false;
	}
	if (other->get_styp() == type_tram) {
		// other is tramway
		return false;
	}
	// needs a crossing
	return true;
}

void weg_t::rdwr(loadsave_t *file)
{
	xml_tag_t t( file, "weg_t" );

	// save owner
	if(  file->is_version_atleast(99, 6)  ) {
		sint8 spnum=get_owner_nr();
		file->rdwr_byte(spnum);
		set_owner_nr(spnum);
	}

	// all connected directions.  HEX-PORT: the 4-bit mask-on-load
	// was tied to the old `uint8 ribi:4` bitfield layout; under the
	// 6-bit hex ribi this silently drops bits 4-5 (N, NE).  Widened
	// to 6-bit mask.  The on-disk byte layout is unchanged, but old
	// saves (4-bit values) load into the same low 4 bits — which
	// now mean SE/S/SW/NW under hex, not N/E/S/W.  Save-format
	// version bump needed before any hex save round-trips cleanly
	// (tracked in TODO.md).
	uint8 dummy8 = ribi;
	file->rdwr_byte(dummy8);
	if(  file->is_loading()  ) {
		ribi = dummy8 & ribi_t::all;
		ribi_maske = 0; // maske will be restored by signal/roadsing
	}

	uint16 dummy16=max_speed;
	file->rdwr_short(dummy16);
	max_speed=dummy16;

	if(  file->is_version_atleast(89, 0)  ) {
		dummy8 = flags;
		file->rdwr_byte(dummy8);
		if(  file->is_loading()  ) {
			// all other flags are restored afterwards
			flags = dummy8 & HAS_SIDEWALK;
		}
	}

	for(  int type=0;  type<MAX_WAY_STATISTICS;  type++  ) {
		for(  int month=0;  month<MAX_WAY_STAT_MONTHS;  month++  ) {
			sint32 w = statistics[month][type];
			file->rdwr_long(w);
			statistics[month][type] = (sint16)w;
			// DBG_DEBUG("weg_t::rdwr()", "statistics[%d][%d]=%d", month, type, statistics[month][type]);
		}
	}
}


void weg_t::info(cbuffer_t & buf) const
{
	obj_t::info(buf);

	buf.printf("%s %u%s", translator::translate("Max. speed:"), max_speed, translator::translate("km/h\n"));
	buf.printf("%s%s",    translator::translate("\nRibi (unmasked)"), ribi_t::names[get_ribi_unmasked()]);
	buf.printf("%s%s\n",  translator::translate("\nRibi (masked)"),   ribi_t::names[get_ribi()]);

	if(has_sign()) {
		buf.append(translator::translate("\nwith sign/signal\n"));
	}

	if(is_electrified()) {
		buf.append(translator::translate("\nelektrified"));
	}
	else {
		buf.append(translator::translate("\nnot elektrified"));
	}

#if 1
	buf.printf(translator::translate("convoi passed last\nmonth %i\n"), statistics[1][1]);
#else
	// Debug - output stats
	buf.append("\n");
	for (int type=0; type<MAX_WAY_STATISTICS; type++) {
		for (int month=0; month<MAX_WAY_STAT_MONTHS; month++) {
			buf.printf("%d ", statistics[month][type]);
		}
	buf.append("\n");
	}
#endif
	buf.append("\n");
	if (char const* const maker = get_desc()->get_copyright()) {
		buf.printf(translator::translate("Constructed by %s"), maker);
		buf.append("\n");
	}
}


/**
 * called during map rotation
 */
void weg_t::rotate90()
{
	obj_t::rotate90();
	// HEX-PORT: rotate90 → rotate_for_map_rotate90 helper,
	// tied to the karte_t::rotate90 refusal in TODO.md.
	ribi = ribi_t::rotate_for_map_rotate90(ribi );
	ribi_maske = ribi_t::rotate_for_map_rotate90(ribi_maske );
}


/**
 * counts signals on this tile;
 * It would be enough for the signals to register and unregister themselves, but this is more secure ...
 */
void weg_t::count_sign()
{
	// Either only sign or signal please ...
	flags &= ~(HAS_SIGN|HAS_SIGNAL|HAS_CROSSING);
	const grund_t *gr=welt->lookup(get_pos());
	if(gr) {
		uint8 i = 1;
		// if there is a crossing, the start index is at least three ...
		if(const crossing_t* cr = gr->get_crossing()) {
			max_speed = desc->get_topspeed(); // reset max_speed
			flags |= HAS_CROSSING;
			i = 3;
			const sint32 top_speed = cr->get_desc()->get_maxspeed( cr->get_desc()->get_waytype(0)==get_waytype() ? 0 : 1);
			max_speed = min(max_speed, top_speed);
		}
		// since way 0 is at least present here ...
		for( ;  i<gr->obj_count();  i++  ) {
			obj_t *obj=gr->obj_bei(i);
			// sign for us?
			if(  roadsign_t const* const sign = obj_cast<roadsign_t>(obj)  ) {
				if(  sign->get_desc()->get_wtyp() == get_desc()->get_wtyp()  ) {
					// here is a sign ...
					flags |= HAS_SIGN;
					return;
				}
			}
			if(  signal_t const* const signal = obj_cast<signal_t>(obj)  ) {
				if(  signal->get_desc()->get_wtyp() == get_desc()->get_wtyp()  ) {
					// here is a signal ...
					flags |= HAS_SIGNAL;
					return;
				}
			}
		}
	}
}


void weg_t::apply_image_slot(const way_image_slot_t& slot)
{
	// `none` means the way's image is not owned by way slot logic
	// right now (first way on a bridge -> bruecke_t draws; tunnel
	// mouth -> tunnel_t draws the portal sprite into the way's
	// image_id field).  Leave image / foreground_image alone for the
	// external owner; do NOT clear them here, that would erase the
	// portal / bridge sprite the borrower just wrote.
	if (slot.kind() == way_image_slot_t::kind_t::none) {
		return;
	}
	set_image           ( slot.resolve(desc, false) );
	set_foreground_image( slot.resolve(desc, true ) );
}


// Replicates `calc_image()`'s dispatch without the snow-flag refresh,
// foreground suppression or dirty marking.  Reads the cached IS_SNOW
// flag, the ground's slope, and `ribi`.  River fallback (rivers with
// no slope sprite render as flat) is folded in here so the returned
// slot is the one whose image is actually visible.
//
// Caller must have updated the IS_SNOW flag for the new tile state
// before calling -- `calc_image()` and `check_season()` both do so
// inline.
way_image_slot_t weg_t::pick_image_slot() const
{
	const grund_t* from = welt->lookup(get_pos());
	if (from == NULL || desc == NULL) {
		return way_image_slot_t::for_none();
	}
	// Tunnel mouth seen from above ground: the tunnel object draws
	// the way image on the kartenboden, so this way leaves its own
	// images as set by the tunnel building path (or IMG_EMPTY).
	if (from->ist_tunnel() && from->ist_karten_boden()
		&& (grund_t::underground_mode == grund_t::ugm_none
			|| (grund_t::underground_mode == grund_t::ugm_level
				&& from->get_hoehe() < grund_t::underground_level))) {
		return way_image_slot_t::for_none();
	}
	// First way on a bridge: bruecke_t is responsible for the image.
	if (from->ist_bruecke() && from->obj_bei(0) == this) {
		return way_image_slot_t::for_none();
	}

	const bool snow = is_snow();
	const slope_t::type hang = from->get_weg_hang();

	if (hang != slope_t::flat && !slope_allows_flat_way_chord(hang, ribi)) {
		const slope_t::type img_slope = axis_slope_for_image(hang, ribi);
		// Single-bit ribi on the slope's ramp axis -> half-slope stub.
		// The way occupies the half of the tile nearest its terminus edge;
		// the sprite covers just that half so the surface is visibly
		// "track ends on slope" instead of a full-axis crossing.  Selecting
		// by edge height (vs. min / max corner of the slope) admits both
		// narrow / wide (delta 1) and double (delta 2) ramps uniformly.
		// Engine intent is pakset-independent: a pak missing half art
		// renders blank for these stubs, the same way a missing flat
		// ribi cell does, until the pakset ships matching sprites.
		if (ribi_t::is_single(ribi)
			&& (slope_t::is_axis_slope(img_slope) || slope_t::is_planar_double_edge(img_slope))) {
			const sint8 edge_h = slope_level_edge_h(hang, ribi);
			if (edge_h >= 0) {
				const bool high_half = (edge_h == (sint8)slope_t::max_diff(img_slope));
				return way_image_slot_t::for_slope_half(img_slope, high_half, snow);
			}
		}
		const way_image_slot_t slope_slot = way_image_slot_t::for_slope(img_slope, snow);
		// River fallback: rivers without slope sprites render flat,
		// matching the surface under the slope.  The fallback is part
		// of the slot pick (not a post-resolve fixup) so the returned
		// slot describes the image actually shown.
		if (get_waytype() == water_wt
			&& desc->get_styp() == type_river
			&& slope_slot.resolve(desc, false) == IMG_EMPTY) {
			return way_image_slot_t::for_flat(ribi, snow);
		}
		return slope_slot;
	}
	return way_image_slot_t::for_flat(ribi, snow);
}


// Halt / depot foreground suppression.  When the way's body sits along
// a single edge (single-bit ribi) or a single hex axis (twoway
// straight), the building visual takes precedence and the way's
// foreground sprite would clutter the silhouette.  Bends, junctions
// and slope-image rendering keep their foreground.  Threeway
// junctions historically went through a degenerate `image_switch`
// path that skipped suppression entirely; preserve that contract by
// gating on ribi shape rather than letting halt presence override.
static bool tile_has_building(const grund_t* gr)
{
	for (uint8 i = 1; i < gr->obj_count(); i++) {
		if (dynamic_cast<gebaeude_t*>(gr->obj_bei(i))) {
			return true;
		}
	}
	return false;
}


static void apply_foreground_suppression(weg_t* w, const grund_t* from, const way_image_slot_t& slot)
{
	if (w->get_front_image() == IMG_EMPTY)                          return;
	if (slot.kind() != way_image_slot_t::kind_t::flat)              return;
	if (ribi_t::is_threeway(slot.get_ribi()))                       return;

	const ribi_t::ribi r = slot.get_ribi();
	const bool axial_for_building = ribi_t::is_twoway(r) ? ribi_t::is_straight(r) : ribi_t::is_single(r);

	if (from->is_halt() || (axial_for_building && tile_has_building(from))) {
		w->set_foreground_image(IMG_EMPTY);
	}
}


sint8 weg_t::calc_render_yoff(slope_t::type hang, const way_image_slot_t& slot, sint8 current_yoff)
{
	if (slot.kind() == way_image_slot_t::kind_t::none) {
		return current_yoff;
	}
	if (slot.kind() == way_image_slot_t::kind_t::flat) {
		const sint8 chord_h = slope_flat_way_chord_h(hang, slot.get_ribi());
		if (chord_h > 0) {
			return (sint8)(-TILE_HEIGHT_STEP * chord_h);
		}
	}
	return 0;
}


static void apply_way_render_yoff(weg_t* w, const grund_t* from, const way_image_slot_t& slot)
{
	w->set_yoff(weg_t::calc_render_yoff(from->get_weg_hang(), slot, w->get_yoff()));
}


// much faster recalculation of season image
bool weg_t::check_season(const bool calc_only_season_change)
{
	if(  calc_only_season_change  ) { // nothing depends on season, only snowline
		return true;
	}

	// no way to calculate this or no image set (not visible, in tunnel mouth, etc)
	if(  desc == NULL  ||  image == IMG_EMPTY  ) {
		return true;
	}

	grund_t *gr = welt->lookup( get_pos() );
	if(  gr->ist_bruecke()  &&  gr->obj_bei(0) == this  ) {
		// first way on a bridge (bruecke_t will set the image)
		return true;
	}

	// use snow image if above snowline and above ground
	bool snow = (gr->ist_karten_boden()  ||  !gr->ist_tunnel())  &&  (get_pos().z  + gr->get_weg_yoff()/TILE_HEIGHT_STEP >= welt->get_snowline()  ||  welt->get_climate( get_pos().get_2d() ) == arctic_climate);
	bool old_snow = (flags&IS_SNOW) != 0;
	if(  !(snow ^ old_snow)  ) {
		// season is not changing ...
		return true;
	}

	// set snow flake
	flags &= ~IS_SNOW;
	if(  snow  ) {
		flags |= IS_SNOW;
	}

	const way_image_slot_t slot = pick_image_slot();
	apply_image_slot(slot);
	apply_way_render_yoff(this, gr, slot);
	apply_foreground_suppression(this, gr, slot);
	return true;
}


#ifdef MULTI_THREAD
void weg_t::lock_mutex()
{
	pthread_mutex_lock( &weg_calc_image_mutex );
}


void weg_t::unlock_mutex()
{
	pthread_mutex_unlock( &weg_calc_image_mutex );
}
#endif


void weg_t::calc_image()
{
#ifdef MULTI_THREAD
	pthread_mutex_lock( &weg_calc_image_mutex );
#endif
	grund_t *from = welt->lookup(get_pos());
	const image_id old_image = image;
	const sint8 old_yoff = get_yoff();

	if (from == NULL || desc == NULL) {
		// Malformed state: way without a tile (enlargement) or without
		// a descriptor.  Clear directly; the slot abstraction's `none`
		// is reserved for legitimate borrowed-rendering paths and
		// must NOT clear, or it would erase tunnel / bridge sprites.
		if (from == NULL) {
			dbg->error("weg_t::calc_image()", "Own way at %s not found!", get_pos().get_str());
		}
		set_image(IMG_EMPTY);
		set_foreground_image(IMG_EMPTY);
		if (from == NULL) {
#ifdef MULTI_THREAD
			pthread_mutex_unlock(&weg_calc_image_mutex);
#endif
			return;
		}
	}
	else {
		// Refresh the IS_SNOW flag from snowline / climate so
		// `pick_image_slot()` reads consistent state.
		const bool snow = (from->ist_karten_boden() || !from->ist_tunnel())
			&& (get_pos().z + from->get_weg_yoff()/TILE_HEIGHT_STEP >= welt->get_snowline()
			    || welt->get_climate(get_pos().get_2d()) == arctic_climate);
		flags &= ~IS_SNOW;
		if (snow) flags |= IS_SNOW;

		// Single dispatch through the slot.  `pick_image_slot` returns
		// `none` for tunnel mouths and bridge first-way; for those
		// `apply_image_slot(none)` is a no-op and the borrower's
		// (bruecke_t / tunnel_t) image stays in place.
		const way_image_slot_t slot = pick_image_slot();
		apply_image_slot(slot);
		apply_way_render_yoff(this, from, slot);
		apply_foreground_suppression(this, from, slot);
	}
	if(  image!=old_image  ||  get_yoff()!=old_yoff  ) {
		mark_image_dirty(old_image, old_yoff);
		mark_image_dirty(image, get_yoff());
	}
#ifdef MULTI_THREAD
	pthread_mutex_unlock( &weg_calc_image_mutex );
#endif
}


/**
 * new month
 */
void weg_t::new_month()
{
	for (int type=0; type<MAX_WAY_STATISTICS; type++) {
		for (int month=MAX_WAY_STAT_MONTHS-1; month>0; month--) {
			statistics[month][type] = statistics[month-1][type];
		}
		statistics[0][type] = 0;
	}
}


// correct speed and maintenance
void weg_t::finish_rd()
{
	player_t *player = get_owner();
	if(  player  &&  desc  ) {
		player_t::add_maintenance( player,  desc->get_maintenance(), desc->get_finance_waytype() );
	}
}


// returns NULL, if removal is allowed
// players can remove public owned ways
const char *weg_t::get_removal_error(const player_t *player)
{
	if(  get_owner_nr()==PLAYER_PUBLIC_NR  ) {
		return NULL;
	}
	return obj_t::get_removal_error(player);
}


FLAGGED_PIXVAL weg_t::get_outline_colour() const
{
	if (env_t::show_single_ways  &&  ribi_t::is_single(ribi)) {
		return TRANSPARENT75_FLAG | OUTLINE_FLAG | gfx->palette_lookup(COL_RED);
	}

	return 0;
}
