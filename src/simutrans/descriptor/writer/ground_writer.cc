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

	// The engine indexes HexLightTexture sprites by raw slope_t value,
	// so on-disk slot N must correspond to slope N. Hex slope_t is
	// sparse (base-4 per corner, 4096 codes, ~140 populated for the
	// hex lightmap); leave gaps as empty slots rather than breaking
	// on the first one. Two passes: find the last populated slope so
	// trailing gaps don't bloat the binary, then materialize.
	int last_used = -1;
	for (int slope = 0; slope < slope_t::max_slopes; slope++) {
		char buf[40];
		sprintf(buf, "image[%d][0]", slope);
		if (*obj.get(buf)) {
			last_used = slope;
		}
	}

	slist_tpl<slist_tpl<std::string> > keys;
	for (int slope = 0; slope <= last_used; slope++) {
		keys.append();
		for (int phase = 0; ; phase++) {
			char buf[40];
			sprintf(buf, "image[%d][%d]", slope, phase);
			std::string str = obj.get(buf);
			if (str.empty()) {
				break;
			}
			keys.at(slope).append(str);
		}
	}
	imagelist2d_writer_t::instance()->write_obj(fp, node, keys);

	node.check_and_write_header(fp);
}
