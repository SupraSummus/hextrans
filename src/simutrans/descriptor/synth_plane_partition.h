/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 */

#ifndef DESCRIPTOR_SYNTH_PLANE_PARTITION_H
#define DESCRIPTOR_SYNTH_PLANE_PARTITION_H

#include "../dataobj/ribi.h"

#include <limits>

namespace synth_overlay {
namespace plane_partition {

struct hex_chord_t {
	uint8 a;
	uint8 b;
};

struct hex_region_t {
	uint8 len;
	uint8 v[hex_corner_t::count];
};

struct hex_partition_t {
	uint8 region_count;
	hex_region_t region[4];
};

static const hex_chord_t HEX_ALL_CHORDS[9] = {
	{0, 2}, {0, 3}, {0, 4},
	{1, 3}, {1, 4}, {1, 5},
	{2, 4}, {2, 5}, {3, 5},
};

static const sint8 HEX_CORNER_PROJECTED_X[hex_corner_t::count] = {  1,  0, -1, -1,  0,  1 };
static const sint8 HEX_CORNER_PROJECTED_Y[hex_corner_t::count] = {  0,  1,  1,  0, -1, -1 };

inline bool partition_chords_cross(const hex_chord_t &c1, const hex_chord_t &c2)
{
	const uint8 a = c1.a < c1.b ? c1.a : c1.b;
	const uint8 b = c1.a < c1.b ? c1.b : c1.a;
	const uint8 c = c2.a < c2.b ? c2.a : c2.b;
	const uint8 d = c2.a < c2.b ? c2.b : c2.a;
	return (a < c && c < b && b < d) || (c < a && a < d && d < b);
}

inline bool partition_append_region_ring(hex_region_t &dst, const hex_region_t &src, uint8 start_i, uint8 end_i)
{
	uint8 n = 0;
	for(  uint8 i = start_i;  i <= end_i;  i++  ) {
		if(  n >= hex_corner_t::count  ) {
			return false;
		}
		dst.v[n++] = src.v[i];
	}
	dst.len = n;
	return true;
}

inline bool partition_append_region_wrap(hex_region_t &dst, const hex_region_t &src, uint8 start_i, uint8 end_i)
{
	uint8 n = 0;
	for(  uint8 i = start_i;  i < src.len;  i++  ) {
		if(  n >= hex_corner_t::count  ) {
			return false;
		}
		dst.v[n++] = src.v[i];
	}
	for(  uint8 i = 0;  i <= end_i;  i++  ) {
		if(  n >= hex_corner_t::count  ) {
			return false;
		}
		dst.v[n++] = src.v[i];
	}
	dst.len = n;
	return true;
}

inline bool compute_regions_from_chord_mask(uint16 mask, hex_partition_t &out)
{
	hex_partition_t work;
	work.region_count = 1;
	work.region[0].len = hex_corner_t::count;
	for(  uint8 i = 0;  i < hex_corner_t::count;  i++  ) {
		work.region[0].v[i] = i;
	}

	for(  uint8 ci = 0;  ci < 9;  ci++  ) {
		if(  (mask & (uint16)(1u << ci)) == 0  ) {
			continue;
		}
		const uint8 a = HEX_ALL_CHORDS[ci].a;
		const uint8 b = HEX_ALL_CHORDS[ci].b;
		sint8 split_idx = -1;
		uint8 ai = 0;
		uint8 bi = 0;
		for(  uint8 ri = 0;  ri < work.region_count;  ri++  ) {
			const hex_region_t &reg = work.region[ri];
			sint8 a_pos = -1;
			sint8 b_pos = -1;
			for(  uint8 vi = 0;  vi < reg.len;  vi++  ) {
				if(  reg.v[vi] == a  ) { a_pos = (sint8)vi; }
				if(  reg.v[vi] == b  ) { b_pos = (sint8)vi; }
			}
			if(  a_pos >= 0 && b_pos >= 0  ) {
				split_idx = (sint8)ri;
				ai = (uint8)a_pos;
				bi = (uint8)b_pos;
				break;
			}
		}
		if(  split_idx < 0 || work.region_count >= 4  ) {
			return false;
		}

		const hex_region_t src = work.region[split_idx];
		const uint8 s = ai < bi ? ai : bi;
		const uint8 e = ai < bi ? bi : ai;

		hex_region_t r1;
		hex_region_t r2;
		if(  !partition_append_region_ring(r1, src, s, e)  ) {
			return false;
		}
		if(  !partition_append_region_wrap(r2, src, e, s)  ) {
			return false;
		}

		work.region[split_idx] = r1;
		work.region[work.region_count++] = r2;
	}

	out = work;
	return true;
}

inline sint64 partition_det3_i64(sint64 a1, sint64 a2, sint64 a3,
                                 sint64 b1, sint64 b2, sint64 b3,
                                 sint64 c1, sint64 c2, sint64 c3)
{
	return a1 * (b2 * c3 - b3 * c2)
	     - a2 * (b1 * c3 - b3 * c1)
	     + a3 * (b1 * c2 - b2 * c1);
}

inline bool region_coplanar(const hex_region_t &region, const uint8 ch[hex_corner_t::count])
{
	if(  region.len <= 3  ) {
		return true;
	}

	for(  uint8 a = 0;  a < region.len;  a++  ) {
		for(  uint8 b = a + 1;  b < region.len;  b++  ) {
			for(  uint8 c = b + 1;  c < region.len;  c++  ) {
				for(  uint8 d = c + 1;  d < region.len;  d++  ) {
					const uint8 ia = region.v[a];
					const uint8 ib = region.v[b];
					const uint8 ic = region.v[c];
					const uint8 id = region.v[d];

					const sint64 v1x = (sint64)HEX_CORNER_PROJECTED_X[ib] - (sint64)HEX_CORNER_PROJECTED_X[ia];
					const sint64 v1y = (sint64)HEX_CORNER_PROJECTED_Y[ib] - (sint64)HEX_CORNER_PROJECTED_Y[ia];
					const sint64 v1z = (sint64)ch[ib] - (sint64)ch[ia];
					const sint64 v2x = (sint64)HEX_CORNER_PROJECTED_X[ic] - (sint64)HEX_CORNER_PROJECTED_X[ia];
					const sint64 v2y = (sint64)HEX_CORNER_PROJECTED_Y[ic] - (sint64)HEX_CORNER_PROJECTED_Y[ia];
					const sint64 v2z = (sint64)ch[ic] - (sint64)ch[ia];
					const sint64 v3x = (sint64)HEX_CORNER_PROJECTED_X[id] - (sint64)HEX_CORNER_PROJECTED_X[ia];
					const sint64 v3y = (sint64)HEX_CORNER_PROJECTED_Y[id] - (sint64)HEX_CORNER_PROJECTED_Y[ia];
					const sint64 v3z = (sint64)ch[id] - (sint64)ch[ia];
					if(  partition_det3_i64(v1x, v1y, v1z, v2x, v2y, v2z, v3x, v3y, v3z) != 0  ) {
						return false;
					}
				}
			}
		}
	}
	return true;
}

inline bool region_flat_horizontal(const hex_region_t &region, const uint8 ch[hex_corner_t::count])
{
	for(  uint8 i = 1;  i < region.len;  i++  ) {
		if(  ch[region.v[i]] != ch[region.v[0]]  ) {
			return false;
		}
	}
	return true;
}

inline uint8 region_projected_area2(const hex_region_t &region)
{
	sint16 area2 = 0;
	for(  uint8 i = 0;  i < region.len;  i++  ) {
		const uint8 j = (uint8)((i + 1) % region.len);
		area2 += (sint16)HEX_CORNER_PROJECTED_X[region.v[i]] * (sint16)HEX_CORNER_PROJECTED_Y[region.v[j]]
		      - (sint16)HEX_CORNER_PROJECTED_X[region.v[j]] * (sint16)HEX_CORNER_PROJECTED_Y[region.v[i]];
	}
	return (uint8)(area2 < 0 ? -area2 : area2);
}

inline uint8 partition_flat_projected_area2(const hex_partition_t &partition, const uint8 ch[hex_corner_t::count])
{
	uint8 area2 = 0;
	for(  uint8 ri = 0;  ri < partition.region_count;  ri++  ) {
		const hex_region_t &region = partition.region[ri];
		if(  region_flat_horizontal(region, ch)  ) {
			area2 += region_projected_area2(region);
		}
	}
	return area2;
}

inline uint8 partition_height_range_sum(const hex_partition_t &partition, const uint8 ch[hex_corner_t::count])
{
	uint8 total = 0;
	for(  uint8 ri = 0;  ri < partition.region_count;  ri++  ) {
		const hex_region_t &region = partition.region[ri];
		uint8 lo = ch[region.v[0]];
		uint8 hi = ch[region.v[0]];
		for(  uint8 i = 1;  i < region.len;  i++  ) {
			const uint8 h = ch[region.v[i]];
			if(  h < lo  ) { lo = h; }
			if(  h > hi  ) { hi = h; }
		}
		total = (uint8)(total + (hi - lo));
	}
	return total;
}

inline bool find_min_partition(const uint8 ch[hex_corner_t::count], hex_partition_t &best)
{
	uint8 best_regions = std::numeric_limits<uint8>::max();
	uint8 best_range_sum = 0;
	uint8 best_flat_area2 = 0;
	bool found = false;

	for(  uint16 mask = 0;  mask < (1u << 9);  mask++  ) {
		bool ok = true;
		for(  uint8 i = 0;  i < 9 && ok;  i++  ) {
			if(  (mask & (uint16)(1u << i)) == 0  ) { continue; }
			for(  uint8 j = i + 1;  j < 9;  j++  ) {
				if(  (mask & (uint16)(1u << j)) == 0  ) { continue; }
				if(  partition_chords_cross(HEX_ALL_CHORDS[i], HEX_ALL_CHORDS[j])  ) {
					ok = false;
					break;
				}
			}
		}
		if(  !ok  ) {
			continue;
		}

		hex_partition_t candidate;
		if(  !compute_regions_from_chord_mask(mask, candidate)  ) {
			continue;
		}
		for(  uint8 ri = 0;  ri < candidate.region_count;  ri++  ) {
			if(  !region_coplanar(candidate.region[ri], ch)  ) {
				ok = false;
				break;
			}
		}
		if(  !ok  ) {
			continue;
		}
		// Region count is primary.  Tiebreak by minimum total within-region
		// height range: the metric is invariant under any symmetry of the
		// input, so mirror-symmetric heights (e.g. 012321) get a mirror-
		// symmetric partition rather than one of two arbitrary mirror images.
		// Flat regions contribute zero, so this still prefers more flat
		// regions (e.g. 000111 still picks the two-flat-triangle partition).
		// Fall back to maximum horizontal top-view area for cases that the
		// range sum cannot distinguish (e.g. 000101, 010111, where one
		// candidate has a flat quad and another has only a flat triangle).
		const uint8 range_sum = partition_height_range_sum(candidate, ch);
		const uint8 flat_area2 = partition_flat_projected_area2(candidate, ch);
		const bool better = candidate.region_count < best_regions
		    || (candidate.region_count == best_regions && range_sum < best_range_sum)
		    || (candidate.region_count == best_regions && range_sum == best_range_sum && flat_area2 > best_flat_area2);
		if(  better  ) {
			best_regions = candidate.region_count;
			best_range_sum = range_sum;
			best_flat_area2 = flat_area2;
			best = candidate;
			found = true;
			if(  best_regions == 1  ) {
				break;
			}
		}
	}

	return found;
}

} // namespace plane_partition
} // namespace synth_overlay

#endif
