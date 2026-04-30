/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 */

#include <string>
#include "../../dataobj/tabfile.h"
#include "../../dataobj/ribi.h"
#include "obj_node.h"
#include "../ground_desc.h"
#include "text_writer.h"
#include "imagelist2d_writer.h"
#include "ground_writer.h"


void ground_writer_t::write_obj(FILE* fp, obj_node_t& parent, tabfileobj_t& obj)
{
	obj_node_t node(this, 0, &parent);

	write_name_and_copyright(fp, node, obj);

	// Both axes can be sparse — slope is keyed by raw slope_t (base-4
	// per corner, 4096 codes, ~140 populated for the lightmap, up to
	// ~2700 for ShoreTrans), phase by a 6-bit water-corner mask whose
	// empty value (mask=0) is never baked.  The runtime indexes the
	// emitted array directly, so trailing slope gaps are trimmed but
	// any in-range hole — including a missing phase 0 on every row —
	// is filled with "-" (the empty-image marker `image_writer_t`
	// understands).
	const int max_phases = 64; // widest second axis is the 6-bit ShoreTrans mask
	auto last_phase = [&](int slope) -> int {
		for (int phase = max_phases - 1; phase >= 0; phase--) {
			char buf[40];
			sprintf(buf, "image[%d][%d]", slope, phase);
			if (*obj.get(buf)) return phase;
		}
		return -1;
	};

	int last_slope = -1;
	for (int slope = 0; slope < slope_t::max_slopes; slope++) {
		if (last_phase(slope) >= 0) last_slope = slope;
	}

	slist_tpl<slist_tpl<std::string> > keys;
	for (int slope = 0; slope <= last_slope; slope++) {
		keys.append();
		const int last = last_phase(slope);
		for (int phase = 0; phase <= last; phase++) {
			char buf[40];
			sprintf(buf, "image[%d][%d]", slope, phase);
			std::string str = obj.get(buf);
			keys.at(slope).append(str.empty() ? std::string("-") : str);
		}
	}
	imagelist2d_writer_t::instance()->write_obj(fp, node, keys);

	node.check_and_write_header(fp);
}
