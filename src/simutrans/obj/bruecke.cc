/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 */

#include "../world/simworld.h"
#include "../simtypes.h"
#include "simobj.h"
#include "../ground/grund.h"
#include "../player/simplay.h"
#include "../display/simimg.h"
#include "../builder/brueckenbauer.h"
#include "../dataobj/loadsave.h"
#include "../dataobj/translator.h"
#include "../dataobj/environment.h"
#include "../dataobj/pakset_manager.h"

#include "bruecke.h"



bruecke_t::bruecke_t(loadsave_t* const file) : obj_no_info_t()
{
	rdwr(file);
}


bool bruecke_t::is_clipping_below_needed() const
{
	// elevated no clip?
	return desc->is_clip_below();
}


bruecke_t::bruecke_t(koord3d pos, player_t *player, const bridge_desc_t *desc, bridge_desc_t::img_t img) :
 obj_no_info_t(pos)
{
	this->desc = desc;
	assert(img >= bridge_desc_t::NS_Segment && img < bridge_desc_t::img_t_count);
	this->img = img;
	set_owner( player );
	player_t::book_construction_costs( get_owner(), -desc->get_price(), get_pos().get_2d(), desc->get_waytype());
}


// Single-height fallback for any img_t.  The enum lays out the
// 18 single-height variants first and the 18 `*_2` variants
// immediately after, so subtracting `NS_Segment2` from a double
// index gives the matching single-height entry; ids already in
// the single block pass through unchanged.  Used by `calc_image`
// / `get_front_image` when a desc lacks the double-height set.
static bridge_desc_t::img_t single_height_of(bridge_desc_t::img_t img)
{
	return img < bridge_desc_t::NS_Segment2
	    ? img
	    : (bridge_desc_t::img_t)(img - bridge_desc_t::NS_Segment2);
}

void bruecke_t::calc_image()
{
	grund_t *gr=welt->lookup(get_pos());
	if(gr) {
		// if we are on the bridge, put the image into the ground, so we can have two ways ...
		if(  weg_t *weg0 = gr->get_weg_nr(0)  ) {
#ifdef MULTI_THREAD
			weg0->lock_mutex();
#endif
			// if on a slope then start of bridge - take the upper value
			const slope_t::type slope = gr->get_grund_hang();
			bool is_snow = welt->get_climate( get_pos().get_2d() ) == arctic_climate  ||  get_pos().z + slope_t::max_diff(slope) >= welt->get_snowline();

			// handle cases where old bridges don't have correct images
			image_id display_image=desc->get_background( img, is_snow );
			if(  display_image==IMG_EMPTY && desc->get_foreground( img, is_snow )==IMG_EMPTY  ) {
				display_image=desc->get_background( single_height_of(img), is_snow );
			}
			weg0->set_image( display_image );

			// must always set both offsets, because after roation the xoffset contains the yoffset
			weg0->set_yoff( -gr->get_weg_yoff() );
			weg0->set_xoff( 0 );

			weg0->set_foreground_image(IMG_EMPTY);
			weg0->set_flag(obj_t::dirty);
#ifdef MULTI_THREAD
			weg0->unlock_mutex();
#endif

			if(  weg_t *weg1 = gr->get_weg_nr(1)  ) {
#ifdef MULTI_THREAD
				weg1->lock_mutex();
#endif
				weg1->set_yoff( -gr->get_weg_yoff() );
				weg1->set_xoff( 0 );
#ifdef MULTI_THREAD
				weg1->unlock_mutex();
#endif
			}
		}
		set_yoff( -gr->get_weg_yoff() );
		set_xoff( 0 );
	}
}


image_id bruecke_t::get_front_image() const
{
	grund_t *gr=welt->lookup(get_pos());
	// if on a slope then start of bridge - take the upper value
	const slope_t::type slope = gr->get_grund_hang();
	bool is_snow = welt->get_climate( get_pos().get_2d() ) == arctic_climate  ||  get_pos().z + slope_t::max_diff(slope) >= welt->get_snowline();
	// handle cases where old bridges don't have correct images
	image_id display_image=desc->get_foreground( img, is_snow );
	if(  display_image==IMG_EMPTY && desc->get_background( img, is_snow )==IMG_EMPTY  ) {
		display_image=desc->get_foreground( single_height_of(img), is_snow );
	}
	return display_image;
}


void bruecke_t::rdwr(loadsave_t *file)
{
	xml_tag_t d( file, "bruecke_t" );

	obj_t::rdwr(file);

	const char *s = NULL;

	if(file->is_saving()) {
		s = desc->get_name();
	}
	file->rdwr_str(s);
	file->rdwr_enum(img);

	if(file->is_loading()) {
		if (!s) {
			dbg->fatal("bruecke_t::rdwr", "No bridge name for bridge at (%s)", get_pos().get_str());
		}

		desc = bridge_builder_t::get_desc(s);
		if(desc==NULL) {
			desc = bridge_builder_t::get_desc(translator::compatibility_name(s));
		}
		if(desc==NULL) {
			dbg->warning( "bruecke_t::rdwr", "Unknown bridge \"%s\" at (%s) will be replaced with best match!", s, get_pos().get_str() );
			pakset_manager_t::add_missing_paks( s, MISSING_BRIDGE );
		}
		free(const_cast<char *>(s));

		if(  file->is_version_less(112, 7)  &&  env_t::pak_height_conversion_factor==2  ) {
			// Pre-port (square) saves stored img enum values that do not
			// map cleanly into the hex layout: legacy O_Start / W_Start
			// (old-east / old-west) are now SE_Start / NW_Start, the
			// hex-only NE/SW directions had no slot, and the double-
			// height variants live at different ordinals.  Pre-port
			// saves are out of scope (see TODO.md "Save format version
			// bump"); refuse them rather than silently renumber.
			dbg->fatal("bruecke_t::rdwr",
			    "Pre-hex-port save (version<112,7, pak_height_conversion=2)"
			    " holds a square-era bridge img value at (%s); save format"
			    " conversion not implemented", get_pos().get_str());
		}
		assert(img >= bridge_desc_t::NS_Segment && img < bridge_desc_t::img_t_count);

	}
}


// correct speed and maintenance
void bruecke_t::finish_rd()
{
	grund_t *gr = welt->lookup(get_pos());
	if(desc==NULL) {
		if(  weg_t *weg = gr->get_weg_nr(0)  ) {
			desc = bridge_builder_t::find_bridge( weg->get_waytype(), weg->get_max_speed(), welt->get_timeline_year_month() );
			if(desc==NULL) {
				desc = bridge_builder_t::find_bridge( weg->get_waytype(), weg->get_max_speed(), 0 );
			}
			if(desc==NULL) {
				dbg->fatal("bruecke_t::finish_rd()", "Unknown bridge for type %x at (%s)", weg->get_waytype(), get_pos().get_str() );
			}
		}
		else {
			// assume this is a powerbridge, since otherwise there should be a way
			desc = bridge_builder_t::find_bridge( powerline_wt, 0, welt->get_timeline_year_month() );
			if(desc==NULL) {
				desc = bridge_builder_t::find_bridge( powerline_wt, 0, 0 );
			}
			if(desc==NULL) {
				dbg->fatal("bruecke_t::finish_rd()", "No powerline bridge to build bridge type at (%s)", get_pos().get_str() );
			}
		}
	}

	player_t *player=get_owner();
	// change maintenance
	if(desc->get_waytype()!=powerline_wt) {
		weg_t *weg = gr->get_weg(desc->get_waytype());
		if(weg==NULL) {
			dbg->error("bruecke_t::finish_rd()","Bridge without way at(%s)!", gr->get_pos().get_str() );
			weg = weg_t::alloc( desc->get_waytype() );
			gr->neuen_weg_bauen( weg, 0, NULL );
			weg->set_owner(player);
		}
		weg->set_max_speed(desc->get_topspeed());
		// take ownership of way
		weg->set_owner(player);
	}
	player_t::add_maintenance(player, desc->get_maintenance(), desc->get_finance_waytype());

	// with double heights may need to correct image on load (not all desc have double images)
	// at present only start images have 2 height variants, others to follow...
	if(  gr->ist_karten_boden()  ) {
		if(  gr->get_grund_hang() != slope_t::flat  ) {
			img = desc->get_start( gr->get_grund_hang() );
		}
	}
}


// correct speed and maintenance
void bruecke_t::cleanup( player_t *player2 )
{
	player_t *player = get_owner();
	// change maintenance, reset max-speed and y-offset
	if(  const grund_t *gr = welt->lookup(get_pos())  ) {
		if(  weg_t *weg0 = gr->get_weg( desc->get_waytype() )  ) {
			weg0->set_max_speed( weg0->get_desc()->get_topspeed() );
			// reset offsets
			weg0->set_xoff(0);
			weg0->set_yoff(0);
			if(  weg_t *weg1 = gr->get_weg_nr(1)  ) {
				weg1->set_xoff(0);
				weg1->set_yoff(0);
			}
		}
	}

	player_t::add_maintenance( player,  -desc->get_maintenance(), desc->get_finance_waytype() );
	player_t::book_construction_costs( player2, -desc->get_price(), get_pos().get_2d(), desc->get_waytype() );
}


void bruecke_t::rotate90()
{
	set_yoff(0);
	obj_t::rotate90();
	// 90° is not a hex symmetry — a hex map rotation step is 60° and
	// `karte_t::rotate90` itself is gated unreachable (see TODO.md
	// "Rotation cascade").  Leave `img` unchanged: it stays a valid
	// img_t value, and any in-tree caller that reaches here lands a
	// `dbg->fatal` further up the cascade before the wrong image
	// renders.  Replace with a real 60° img-rotation table when the
	// viewport / rotation port lands.
}


// returns NULL, if removal is allowed
// players can remove public owned ways
const char *bruecke_t::get_removal_error(const player_t *player)
{
	if (get_owner_nr()==PLAYER_PUBLIC_NR) {
		return NULL;
	}
	else {
		return obj_t::get_removal_error(player);
	}
}
