/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 *
 * Game-side fatal-exit sink: pops a modal window if the display is up,
 * else falls back to dr_fatal_notify, then abort()s.  Linked into
 * simutrans only; nettool links its own puts+exit stub from
 * nettool_compat.cc.
 */

#include <string.h>

#include "log.h"
#include "../dataobj/environment.h"
#include "../display/simgraph.h"
#include "../gui/messagebox.h"
#include "../gui/simwin.h"
#include "../sys/simsys.h"


void log_t_platform_fatal_exit(char *buffer)
{
	env_t::verbose_debug = log_t::LEVEL_FATAL; // no more window concerning messages

	if (gfx->is_display_init()) {
		// show notification
		destroy_all_win( true );

		strcat( buffer, "PRESS ANY KEY\n" );
		fatal_news* sel = new fatal_news(buffer);

		const scr_size screen = gfx->get_screen_size();
		scr_coord xy( screen.w/2 - sel->get_windowsize().w/2, screen.h/2 - sel->get_windowsize().h/2 );
		event_t ev;

		create_win( xy, sel, w_info, magic_none );

		while(win_is_top(sel)) {
			// do not move, do not close it!
			dr_sleep(50);
			dr_prepare_flush();
			sel->draw( xy, sel->get_windowsize() );
			dr_flush();
			display_poll_event(&ev);
			// main window resized
			check_pos_win(&ev,true);

			if (IS_KEYDOWN(&ev)) {
				break;
			}
		}
	}
	else {
		// use OS means, if there
		dr_fatal_notify(buffer);
	}

	abort();
}
