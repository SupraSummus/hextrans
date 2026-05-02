/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 */

#include <locale.h>
#include <string>
#include "../../utils/simstring.h"
#include "../../dataobj/tabfile.h"
#include "../../tpl/stringhashtable_tpl.h"
#include "../../tpl/inthashtable_tpl.h"
#include "obj_node.h"
#include "obj_writer.h"
#include "image_writer.h"
#include "text_writer.h"
#include "xref_writer.h"


const char *obj_writer_t::last_name = "";

int obj_writer_t::default_image_size = 64;


struct pak_node_stats_t {
	unsigned long long storage_bytes;
	unsigned long long image_rle_bytes;
	unsigned int image_slots;
	unsigned int image_used;
	unsigned int image_lists;
};


static unsigned node_header_size(const obj_node_info_t &node)
{
	return node.size >= LARGE_RECORD_SIZE ? EXT_OBJ_NODE_INFO_SIZE : OBJ_NODE_INFO_SIZE;
}


static uint16 decode_le16(const unsigned char *p)
{
	return (uint16)p[0] | (uint16)p[1] << 8;
}


static uint32 decode_le32(const unsigned char *p)
{
	return
		(uint32)p[0] <<  0 |
		(uint32)p[1] <<  8 |
		(uint32)p[2] << 16 |
		(uint32)p[3] << 24;
}


static unsigned long long image_rle_bytes(FILE *fp, const obj_node_info_t &node)
{
	const long pos = ftell(fp);
	unsigned long long bytes = 0;

	if (node.size >= 10) {
		unsigned char header[12];
		const size_t wanted = node.size < sizeof(header) ? node.size : sizeof(header);
		if (fread(header, wanted, 1, fp) == 1 && wanted > 6) {
			const unsigned version = header[6];
			if (version == 0 && wanted >= 8) {
				bytes = static_cast<unsigned long long>(decode_le32(header + 4)) * sizeof(uint16);
			}
			else if (version <= 2 && wanted >= 9) {
				bytes = static_cast<unsigned long long>(decode_le16(header + 7)) * sizeof(uint16);
			}
			else if (version == 3) {
				bytes = static_cast<unsigned long long>(node.size - 10);
			}
		}
	}

	fseek(fp, pos, SEEK_SET);
	return bytes;
}


static bool collect_node_stats(FILE *fp, const obj_node_info_t &node, pak_node_stats_t &stats)
{
	stats.storage_bytes += node_header_size(node) + node.size;

	if (node.type == obj_image) {
		stats.image_slots++;
		const unsigned long long image_bytes = image_rle_bytes(fp, node);
		stats.image_rle_bytes += image_bytes;
		if (image_bytes != 0) {
			stats.image_used++;
		}
	}
	else if (node.type == obj_imagelist || node.type == obj_imagelist2d) {
		stats.image_lists++;
	}

	if (fseek(fp, node.size, SEEK_CUR) != 0) {
		return false;
	}

	for (int i = 0; i < node.nchildren; i++) {
		obj_node_info_t child;
		if (!obj_node_t::read_node(fp, child)) {
			return false;
		}
		if (!collect_node_stats(fp, child, stats)) {
			return false;
		}
	}

	return true;
}


static bool text_matches(const std::string &value, const char *filter)
{
	return filter == NULL || filter[0] == '\0' || tstrcasestr(value.c_str(), filter) != NULL;
}


static void print_csv_field(const char *text)
{
	putchar('"');
	for (const char *p = text; *p; p++) {
		if (*p == '"') {
			putchar('"');
		}
		putchar(*p);
	}
	putchar('"');
}


void obj_writer_t::register_writer(bool main_obj)
{
	if (!writer_by_name) {
		writer_by_name = new stringhashtable_tpl<obj_writer_t*>;
		writer_by_type = new inthashtable_tpl<uint32, obj_writer_t*>;
	}
	if (main_obj) {
		writer_by_name->put(get_type_name(), this);
	}
	writer_by_type->put(get_type(), this);

	///    printf("This program can compile %s objects\n", get_type_name());
}


void obj_writer_t::write(FILE* fp, obj_node_t& parent, tabfileobj_t& obj)
{
	const char *type = obj.get("obj");
	const char *name = obj.get("name");

	obj_writer_t *writer = writer_by_name->get(type);
	if (!writer) {
		printf("Skipping unknown %s object %s\n", type, name);
		return;
	}
	// now get the image size
	image_writer_t::set_img_size(obj.get_int("cell_size",default_image_size));

	last_name = name;

	if (debuglevel >= log_t::LEVEL_WARN) {
		printf("      packing %s.%s\n", type, name);
	}

	writer->write_obj(fp, parent, obj);
}


void obj_writer_t::write_name_and_copyright(FILE* fp, obj_node_t& node, tabfileobj_t& obj)
{
	const char* name = obj.get("name");
	const char* msg = obj.get("copyright");

	last_name = name;
	text_writer_t::instance()->write_obj(fp, node, name);
	text_writer_t::instance()->write_obj(fp, node, msg);
}


bool obj_writer_t::dump_nodes(FILE* infp, int level, uint16 index)
{
	obj_node_info_t node;

	if (!obj_node_t::read_node(infp, node)  ) {
		return false;
	}

	const long next_pos = ftell(infp) + node.size;

	obj_writer_t* writer = writer_by_type->get(node.type);
	if (writer) {
		printf("%*s%03u %4.4s-node (%s)", 3 * level, " ", index, (const char*)&node.type, writer->get_type_name());
		bool ok = writer->dump_node(infp, node);
		printf("\n");
		if (!ok) {
			return false;
		}
	}

	if (fseek(infp, next_pos, SEEK_SET) != 0) {
		return false;
	}

	for (int child_idx = 0; child_idx < node.nchildren; child_idx++) {
		if (!dump_nodes(infp, level + 1, child_idx)) {
			return false;
		}
	}

	return true;
}


bool obj_writer_t::list_nodes(FILE *infp, const char *file_name, const char *type_filter, const char *name_filter, bool csv)
{
	obj_node_info_t node;

	if (!obj_node_t::read_node(infp, node)) {
		return false;
	}

	const long body_pos = ftell(infp);
	const long child_pos = body_pos + node.size;

	obj_writer_t *writer = writer_by_type->get((obj_type)node.type);
	const char *type_name = writer ? writer->get_type_name() : "(unknown)";

	std::string node_name;
	if (writer) {
		if (fseek(infp, child_pos, SEEK_SET) != 0) {
			return false;
		}
		node_name = writer->get_node_name(infp);
	}

	if (fseek(infp, body_pos, SEEK_SET) != 0) {
		return false;
	}

	pak_node_stats_t stats = {};
	if (!collect_node_stats(infp, node, stats)) {
		return false;
	}

	if (text_matches(type_name, type_filter) && text_matches(node_name, name_filter)) {
		const unsigned int image_empty = stats.image_slots - stats.image_used;
		const unsigned long long fill_per_mille = stats.image_slots == 0 ? 0 : (static_cast<unsigned long long>(stats.image_used) * 1000u + stats.image_slots / 2u) / stats.image_slots;
		const unsigned long long non_rle_bytes = stats.storage_bytes - stats.image_rle_bytes;

		if (csv) {
			print_csv_field(file_name);
			putchar(',');
			print_csv_field(type_name);
			putchar(',');
			print_csv_field(node_name.c_str());
			printf(",%llu,%llu,%llu,%u,%u,%u,%u.%u,%u\n",
				stats.storage_bytes,
				stats.image_rle_bytes,
				non_rle_bytes,
				stats.image_slots,
				stats.image_used,
				image_empty,
				static_cast<unsigned int>(fill_per_mille / 10u),
				static_cast<unsigned int>(fill_per_mille % 10u),
				stats.image_lists);
		}
		else {
			printf("%-48.48s  %-16.16s  %-30.30s  %12llu  %12llu  %12llu  %6u  %6u  %6u  %5u.%u  %5u\n",
				file_name,
				type_name,
				node_name.c_str(),
				stats.storage_bytes,
				stats.image_rle_bytes,
				non_rle_bytes,
				stats.image_slots,
				stats.image_used,
				image_empty,
				static_cast<unsigned int>(fill_per_mille / 10u),
				static_cast<unsigned int>(fill_per_mille % 10u),
				stats.image_lists);
		}
	}

	return true;
}


void obj_writer_t::show_capabilites()
{
	slist_tpl<obj_writer_t*> list;
	const char *min_s="A";

	while (true) {
		const char *max_s = "zzz";
		for(auto const& i : *writer_by_name) {
			if(  STRICMP(i.key, min_s) > 0  &&  STRICMP(i.key, max_s) < 0   ) {
				max_s = i.key;
			}
		}
		if(  strcmp(max_s,"zzz")==0  ) {
			break;
		}
		printf("   %s\n", max_s);
		min_s = max_s;
	}
}


std::string obj_writer_t::name_from_next_node(FILE *fp) const
{
	std::string ret;
	obj_node_info_t node;

	if (!obj_node_t::read_node( fp, node ) || node.type!=obj_text || node.size == 0xFFFFFFFFu) {
		return "";
	}

	char *buf = new char[node.size+1];
	if (!buf) {
		return "";
	}

	if (fread(buf, node.size, 1, fp) != 1) {
		delete[] buf;
		return "";
	}

	buf[node.size] = 0;
	ret = buf;
	delete[] buf;
	return ret;
}


bool obj_writer_t::skip_nodes(FILE *fp, size_t &offset)
{
	obj_node_info_t node;

	if (!obj_node_t::read_node( fp, node ) || fseek(fp, node.size, SEEK_CUR) != 0) {
		return false;
	}

	offset += node.size;

	for (int i = 0; i < node.nchildren; i++) {
		if (!skip_nodes(fp, offset)) {
			return false;
		}
	}

	return true;
}


bool obj_writer_t::dump_node(FILE */*infp*/, const obj_node_info_t& node)
{
	printf(" %5u bytes", node.size);
	return true;
}


const char* obj_writer_t::node_writer_name(FILE* infp) const
{
	obj_node_info_t node;
	obj_node_t::read_node( infp, node );
	fseek(infp, node.size, SEEK_CUR);
	obj_writer_t* writer = writer_by_type->get(node.type);
	if (writer) {
		return writer->get_type_name();
	}
	return "unknown";
}
