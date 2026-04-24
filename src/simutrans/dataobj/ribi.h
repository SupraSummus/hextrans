
/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 */

#ifndef DATAOBJ_RIBI_H
#define DATAOBJ_RIBI_H


#include "../simtypes.h"
#include "../simconst.h"
#include "../simdebug.h"

class koord;
class koord3d;

/**
 * Slopes of tiles.  Base-3 6-corner encoding: each corner holds
 * height 0, 1 or 2; digit positions follow hex_corner_t (E=mult 1,
 * SE=3, SW=9, W=27, NW=81, NE=243).  3^6 = 729 possible slopes,
 * needs sint16.  Double-height slopes kept.
 */
class slope_t {
public:

	typedef sint16 type;


	/**
	 * Named constants for special cases.
	 */
	enum _type {
		flat = 0,

		// 6 single-corner raised slopes (corner at height 1).  Digit
		// positions match hex_corner_t ordering.
		raised_E  = 1,   ///< E  corner, digit 0
		raised_SE = 3,   ///< SE corner, digit 1
		raised_SW = 9,   ///< SW corner, digit 2
		raised_W  = 27,  ///< W  corner, digit 3
		raised_NW = 81,  ///< NW corner, digit 4
		raised_NE = 243, ///< NE corner, digit 5

		// Square-style single-corner aliases.  Under hex these name the
		// corresponding hex corner at height 1.
		southeast = raised_SE,
		southwest = raised_SW,
		northwest = raised_NW,
		northeast = raised_NE,

		// 2-corner "edge" slopes named by their LOW edge.  Only north
		// and south correspond to actual hex edges; east and west
		// under flat-top hex are 2-corner diagonals (the 2 east /
		// 2 west corners) kept for backward compatibility with square
		// callers that reference them.  Hex-only edges (NE, SE, SW, NW)
		// have no named constant yet — use `raised_X + raised_Y` on
		// the two adjacent corners.
		north = raised_SE + raised_SW, ///< low edge N (S corners raised)
		south = raised_NE + raised_NW, ///< low edge S (N corners raised)
		east  = raised_NW + raised_SW, ///< 2 west corners raised (legacy square)
		west  = raised_NE + raised_SE, ///< 2 east corners raised (legacy square)

		all_up_one = raised_E + raised_SE + raised_SW + raised_W + raised_NW + raised_NE, ///< all corners 1 high (= 364)
		all_up_two = all_up_one * 2,                                                       ///< all corners 2 high (= 728)

		raised = all_up_two,    ///< special meaning: used as slope of bridgeheads and in terraforming tools (keep for compatibility)

		max_number = all_up_two
	};

	/// Width of the encoding (number of distinct slope values). 3^6.
	static const int max_slopes = 729;

	/*
	 * Macros to access the height of the 6 corners (base-3 digit
	 * extraction).  Corner bit positions match hex_corner_t.  Each
	 * macro returns 0, 1 or 2.
	 */
#define corner_e(i)  ((i) % 3)
#define corner_se(i) (((i) / 3) % 3)
#define corner_sw(i) (((i) / 9) % 3)
#define corner_w(i)  (((i) / 27) % 3)
#define corner_nw(i) (((i) / 81) % 3)
#define corner_ne(i) ((i) / 243)

/**
 * Build a slope from 4 square corner heights.  Left for backward
 * compatibility with square callers; sets the hex-only E and W
 * corners to flat.  New code should prefer encode_corners_hex.
 */
#define encode_corners(sw, se, ne, nw) ( (sw) * slope_t::raised_SW + (se) * slope_t::raised_SE + (ne) * slope_t::raised_NE + (nw) * slope_t::raised_NW )

/// Build a slope from 6 hex corner heights.  Argument order matches
/// hex_corner_t.
#define encode_corners_hex(e, se, sw, w, nw, ne) \
	( (e)  * slope_t::raised_E  + (se) * slope_t::raised_SE \
	+ (sw) * slope_t::raised_SW + (w)  * slope_t::raised_W  \
	+ (nw) * slope_t::raised_NW + (ne) * slope_t::raised_NE )

/// True if no corner is at height 2 (i.e. only uses 0/1), i.e. a
/// "single" slope.  Tests that every base-3 digit is < 2 by the
/// usual trick: `x % 2 == x` iff x == 0 or 1.  Equivalent to
/// `!(flags[i] & doubles)` but without the table lookup.
#define is_one_high(i) (!slope_t::has_double_corner(i))

	/// True if @p x has any corner at height 2.  Equivalent to the
	/// old `doubles` flag; used by max_diff and the is_one_high macro.
	static bool has_double_corner(type x) {
		return corner_e(x) == 2 || corner_se(x) == 2 || corner_sw(x) == 2
		    || corner_w(x) == 2 || corner_nw(x) == 2 || corner_ne(x) == 2;
	}

	/// Compute the slope opposite to @p x (flip each corner 0↔1 for
	/// single-height slopes, 0↔2 for double-height).  Returns flat if
	/// @p x does not allow ways on it.
	static type opposite(type x) { return is_single(x) ? (is_one_high(x) ? (type)(slope_t::all_up_one - x) : (type)(slope_t::all_up_two - x)) : flat; }

	/// Rotate by 60° clockwise (cyclically shift base-3 digits by 1
	/// position).  The 4-corner rotate90 is gone — hex has 6
	/// rotational positions so one step is 60°.  Callers that
	/// semantically wanted a quarter-turn (e.g. building-layout
	/// 4-cycles) are broken and need explicit auditing.
	static type rotate60(type x) { return (type)(((x % raised_NE) * 3) + (x / raised_NE)); }

	/// Returns true if @p x has all corners raised (either all 1 or all 2).
	static bool is_all_up(type x) { return x == all_up_one || x == all_up_two; }

	/// Maximum corner-height difference across this slope.  0 for
	/// flat, 1 for single-height, 2 for double-height.
	static uint8 max_diff(type x) { return x != flat ? (has_double_corner(x) ? 2 : 1) : 0; }

	/// Computes minimum corner-height difference between @p high and @p low.
	static sint8 min_diff(type high, type low) {
		return min( min( min( corner_e(high)  - corner_e(low),  corner_se(high) - corner_se(low) ),
		                 min( corner_sw(high) - corner_sw(low), corner_w(high)  - corner_w(low) ) ),
		            min( corner_nw(high) - corner_nw(low), corner_ne(high) - corner_ne(low) ) );
	}

	/// Edge slopes that host a way along one direction: 6 genuine hex
	/// edges (2 adjacent corners raised) plus 2 legacy square-diagonal
	/// slopes (east = NW+SW, west = NE+SE) — each at single and double
	/// height.  east and west don't correspond to real hex edges but
	/// are kept buildable so square-era buildings (docks, harbours)
	/// still place during the port.
	static bool is_single(type x) {
		switch (x) {
			case raised_E  + raised_SE: case 2 * (raised_E  + raised_SE):
			case raised_SE + raised_SW: case 2 * (raised_SE + raised_SW):
			case raised_SW + raised_W:  case 2 * (raised_SW + raised_W):
			case raised_W  + raised_NW: case 2 * (raised_W  + raised_NW):
			case raised_NW + raised_NE: case 2 * (raised_NW + raised_NE):
			case raised_NE + raised_E:  case 2 * (raised_NE + raised_E):
			case east: case 2 * east:
			case west: case 2 * west:
				return true;
			default:
				return false;
		}
	}

	/// Way allowed on this slope: flat, any edge slope, or all-up.
	static bool is_way(type x) { return x == flat || is_single(x) || is_all_up(x); }

	/// Does this slope's edge lie along the N-S axis?  True for the 2
	/// hex N/S edges (= ::north and ::south) at single or double
	/// height, plus all-up (way in any axis).  Legacy 2-axis split;
	/// hex has 3 axes — the NE-SW and NW-SE edge slopes aren't covered
	/// by either predicate yet.
	static bool is_way_ns(type x) {
		return x == north || x == 2 * north || x == south || x == 2 * south || is_all_up(x);
	}

	/// Does this slope's edge lie along the E-W axis?  True for the 2
	/// legacy ::east and ::west constants (which under flat-top hex
	/// are 2-corner diagonals, not real edges) plus all-up.
	static bool is_way_ew(type x) {
		return x == east || x == 2 * east || x == west || x == 2 * west || is_all_up(x);
	}
};


/**
 * Old implementation of slopes: one bit per corner.
 * Used as bitfield to refer to specific corners of a tile
 * as well as for compatibility.
 */
struct slope4_t
{
	/* bit-field:
	 * Bit 0 is set if southwest corner is raised
	 * Bit 1 is set if southeast corner is raised
	 * Bit 2 is set if northeast corner is raised
	 * Bit 3 is set if northwest corner is raised
	 *
	 * Don't get confused - the southern/southward slope has its northern corners raised
	 *
	 * Macros to access the height of the 4 corners for single slope:
	 * One bit per corner
	 */
	enum _corners {
		corner_SW = 1 << 0,
		corner_SE = 1 << 1,
		corner_NE = 1 << 2,
		corner_NW = 1 << 3
	};

	typedef sint8 type;

	type value;

public:
	explicit slope4_t(type v) : value(v) {}
	slope4_t(_corners c) : value(c) {}
};


static inline sint8 scorner_sw(slope4_t sl) { return (sl.value & slope4_t::corner_SW) != 0; } // sw corner
static inline sint8 scorner_se(slope4_t sl) { return (sl.value & slope4_t::corner_SE) != 0; } // se corner
static inline sint8 scorner_ne(slope4_t sl) { return (sl.value & slope4_t::corner_NE) != 0; } // ne corner
static inline sint8 scorner_nw(slope4_t sl) { return (sl.value & slope4_t::corner_NW) != 0; } // nw corner

static inline slope_t::type slope_from_slope4(slope4_t sl, sint8 pak_height_factor)
{
	return encode_corners(scorner_sw(sl) * pak_height_factor,
						  scorner_se(sl) * pak_height_factor,
						  scorner_ne(sl) * pak_height_factor,
						  scorner_nw(sl) * pak_height_factor);
}


/**
 * Directions in simutrans.
 * ribi_t = Richtungs-Bit = Directions-Bitfield
 */
class ribi_t {
	/// Static lookup table
	static const int flags[16];

	/// Named constants for properties of directions
	enum {
		single      = 1 << 0, ///< only one bit set, way ends here
		straight_ns = 1 << 1, ///< contains straight n/s connection
		straight_ew = 1 << 2, ///< contains straight e/w connection
		bend        = 1 << 3, ///< is a bend
		twoway      = 1 << 4, ///< two bits set
		threeway    = 1 << 5  ///< three bits set
	};

public:
	/**
	 * Named constants for all possible directions.
	 * 1=North, 2=East, 4=South, 8=West
	 */
	enum _ribi {
		none           = 0,
		north          = 1,
		east           = 2,
		northeast      = 3,
		south          = 4,
		northsouth     = 5,
		southeast      = 6,
		northsoutheast = 7,
		west           = 8,
		northwest      = 9,
		eastwest       = 10,
		northeastwest  = 11,
		southwest      = 12,
		northsouthwest = 13,
		southeastwest  = 14,
		all            = 15
	};
	typedef uint8 ribi;

	/**
	 * Named constants to translate direction to image number for vehicles, signs.
	 */
	enum _dir {
		dir_invalid   = 0,
		dir_south     = 0,
		dir_west      = 1,
		dir_southwest = 2,
		dir_southeast = 3,
		dir_north     = 4,
		dir_east      = 5,
		dir_northeast = 6,
		dir_northwest = 7
	};
	typedef uint8 dir;

private:
	/// Lookup table to compute backward direction
	static const ribi backwards[16];
	/// Lookup table ...
	static const ribi doppelr[16];
	/// Lookup table to convert ribi to dir.
	static const dir  dirs[16];

public:
	/// Lookup table to convert ribi mask to string
	static const char names[16][5];

	/// Table containing the four compass directions (now as function)
	struct _nesw {
		ribi operator [] ( const uint8 i ) const { return 1<<i; }
	};
	static const _nesw nesw;

	/// Convert building layout to ribi (four rotations), use doppelt in case of two rotations
	static const ribi layout_to_ribi[4]; // building layout to ribi (for four rotations, for two use doppelt()!

	static bool is_perpendicular(ribi x, ribi y);

#ifdef RIBI_LOOKUP
	static bool is_twoway(ribi x) { return (flags[x]&twoway)!=0; }
	static bool is_threeway(ribi x) { return (flags[x]&threeway)!=0; }
	static bool is_single(ribi x) { return (flags[x] & single) != 0; }
	static bool is_bend(ribi x) { return (flags[x] & bend) != 0; }
	static bool is_straight(ribi x) { return (flags[x] & (straight_ns | straight_ew)) != 0; }
	static bool is_straight_ns(ribi x) { return (flags[x] & straight_ns) != 0; }
	static bool is_straight_ew(ribi x) { return (flags[x] & straight_ew) != 0; }

	/// Convert single/straight direction into their doubled form (n, ns -> ns), map all others to zero
	static ribi doubles(ribi x) { return doppelr[x]; }

	/// Backward direction for single ribi's, bitwise-NOT for all others
	static ribi backward(ribi x) { return backwards[x]; }

	/// Convert ribi to dir
	static dir get_dir(ribi x) { return dirs[x]; }
#else
#ifdef USE_GCC_POPCOUNT
	static uint8 get_numways(ribi x) { return (__builtin_popcount(x)); }
	static bool is_twoway(ribi x) { return get_numways(x) == 2; }
	static bool is_threeway(ribi x) { return get_numways(x) > 2; }
	static bool is_single(ribi x) { return get_numways(x) == 1; }
#else
	static bool is_twoway(ribi x) { return (0x1668 >> x) & 1; }
	static bool is_threeway(ribi x) { return (0xE880 >> x) & 1; }
	static bool is_single(ribi x) { return (0x0116 >> x) & 1; }
#endif
	static bool is_bend(ribi x) { return (0x1248 >> x) & 1; }
	static bool is_straight(ribi x) { return (0x0536 >> x) & 1; }
	static bool is_straight_ns(ribi x) { return (0x0032 >> x) & 1; }
	static bool is_straight_ew(ribi x) { return (0x0504 >> x) & 1; }

	static ribi doubles(ribi x) { return (INT64_C(0x00000A0A00550A50) >> (x * 4)) & 0x0F; }
	static ribi backward(ribi x) { return (INT64_C(0x01234A628951C84F) >> (x * 4)) & 0x0F; }

	/// Convert ribi to dir
	static dir get_dir(ribi x) { return (INT64_C(0x0002007103006540) >> (x * 4)) & 0x7; }
#endif
	/**
	 * Same as backward, but for single directions only.
	 * Effectively does bit rotation. Avoids lookup table backwards.
	 * @returns backward(x) for single ribis, 0 for x==0.
	 */
	static inline ribi reverse_single(ribi x) {
		return ((x  |  x<<4) >> 2) & 0xf;
	}

	/// Rotate 90 degrees to the right. Does bit rotation.
	static ribi rotate90(ribi x) { return ((x  |  x<<4) >> 3) & 0xf; }
	/// Rotate 90 degrees to the left. Does bit rotation.
	static ribi rotate90l(ribi x) { return ((x  |  x<<4) >> 1) & 0xf; }
	static ribi rotate45(ribi x) { return (is_single(x) ? x|rotate90(x) : x&rotate90(x)); } // 45 to the right
	static ribi rotate45l(ribi x) { return (is_single(x) ? x|rotate90l(x) : x&rotate90l(x)); } // 45 to the left
};

/**
 * Calculate slope from directions.
 * Go upward on the slope: going north translates to slope_t::south.
 */
slope_t::type slope_type(koord dir);

/**
 * Calculate slope from directions.
 * Go upward on the slope: going north translates to slope_t::south.
 */
slope_t::type slope_type(ribi_t::ribi);

/**
 * Check if the slope is upwards, relative to the direction @p from.
 * @returns 1 for single upwards and 2 for double upwards
 */
sint16 get_sloping_upwards(const slope_t::type slope, const ribi_t::ribi from);

/**
 * Calculate direction bit from coordinate differences.
 */
ribi_t::ribi ribi_typ_intern(sint16 dx, sint16 dy);

/**
 * Calculate direction bit from direction.
 */
ribi_t::ribi ribi_type(const koord& dir);
ribi_t::ribi ribi_type(const koord3d& dir);

/**
 * Calculate direction bit from slope.
 * Note: slope_t::north (slope north) will be translated to ribi_t::south (direction south).
 */
ribi_t::ribi ribi_type(slope_t::type slope);

/**
 * Calculate direction bit for travel from @p from to @p to.
 */
template<class K1, class K2>
ribi_t::ribi ribi_type(const K1&from, const K2& to)
{
	return ribi_typ_intern(to.x - from.x, to.y - from.y);
}

#endif
