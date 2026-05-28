/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 */

#include <string>
#include <vector>
#include "../../dataobj/tabfile.h"
#include "../../dataobj/ribi.h"
#include "obj_node.h"
#include "text_writer.h"
#include "imagelist2d_writer.h"
#include "ground_writer.h"


void ground_writer_t::write_obj(FILE* fp, obj_node_t& parent, tabfileobj_t& obj)
{
	obj_node_t node(this, 0, &parent);

	write_name_and_copyright(fp, node, obj);

	// Either axis can be a raw slope_t — sidewalk / borders / ShoreTrans
	// put slope on the outer axis with a small inner (stage / mask),
	// way_ground flips it (axis outer, slope inner).  Read the populated
	// keys directly and emit a dense 2D table over their bounding box;
	// in-range holes become "-" (the empty-image marker
	// `image_writer_t` understands), trailing outer / inner gaps trim.
	int max_outer = -1;
	std::vector<int> max_inner(slope_t::max_slopes, -1);
	for (auto const& entry : obj) {
		int x, y;
		char rest = 0;
		const int n = sscanf(entry.key, "image[%d][%d]%c", &x, &y, &rest);
		if (n != 2 || x < 0 || y < 0 || x >= slope_t::max_slopes || y >= slope_t::max_slopes) {
			continue;
		}
		if (x > max_outer)    max_outer    = x;
		if (y > max_inner[x]) max_inner[x] = y;
	}

	slist_tpl<slist_tpl<std::string> > keys;
	for (int outer = 0; outer <= max_outer; outer++) {
		keys.append();
		for (int inner = 0; inner <= max_inner[outer]; inner++) {
			char buf[40];
			sprintf(buf, "image[%d][%d]", outer, inner);
			std::string str = obj.get(buf);
			keys.at(outer).append(str.empty() ? std::string("-") : str);
		}
	}
	imagelist2d_writer_t::instance()->write_obj(fp, node, keys);

	node.check_and_write_header(fp);
}
