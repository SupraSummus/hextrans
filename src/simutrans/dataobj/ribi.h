
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
 *
 * HEX-PORT: 6-bit bitfield, one bit per flat-top hex edge.  Bit i
 * corresponds to koord::neighbours[i], i.e. iterating bits 0..5 visits
 * neighbours in the same order (SE, S, SW, NW, N, NE).  Combinations
 * still fit in uint8 (64 distinct values).
 *
 * The 4-direction combo constants (northsouth, eastwest, northeastwest,
 * ...) and the due-east / due-west single directions are GONE — flat-top
 * hex has no due-east or due-west edge.  Callers that used them must be
 * ported to explicit hex axes.  The name `northeast` now means a
 * single-edge direction (bit 5), not "north OR east" as in the 4-bit
 * layout.
 */
class ribi_t {
public:
	/**
	 * Named constants for the 6 single hex-edge directions.  Bit
	 * positions match koord::neighbours[] ordering.
	 */
	enum _ribi {
		none      = 0,
		southeast = 1 << 0, // 1   ─ neighbours[0]
		south     = 1 << 1, // 2   ─ neighbours[1]
		southwest = 1 << 2, // 4   ─ neighbours[2]
		northwest = 1 << 3, // 8   ─ neighbours[3]
		north     = 1 << 4, // 16  ─ neighbours[4]
		northeast = 1 << 5, // 32  ─ neighbours[5]
		all       = 63      ///< all 6 edges set
	};
	typedef uint8 ribi;

	/**
	 * Direction image index for vehicles / signs.  Still a 4-direction
	 * enum for sprite-raster reasons; porting this is tied to the
	 * viewport projection (see TODO.md) and a real hex sprite set.
	 * For now the enum keeps its 8 values but callers that consume
	 * `get_dir()` on a hex ribi land on whichever 4 of 6 edges the
	 * dirs[] table chooses to project onto the square dir enum.
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

public:
	/// Printable name per ribi value, 6 one-char columns + NUL.  Column
	/// order matches koord::neighbours[]: SE, S, SW, NW, N, NE; set bit
	/// shows a letter, unset bit a space.  Runtime-initialised in
	/// ribi.cc (not a compile-time literal — 64 entries are noise to
	/// hand-write, and the array is only used for debug prints).
	static char names[64][7];

	/// Accessor for the 6 compass directions as single-bit ribis:
	/// nesw[i] = 1 << i, matching koord::neighbours[i].  Name kept
	/// for grep-continuity with callers mid-port; rename when the
	/// 4-direction-era callers are all gone.
	struct _nesw {
		ribi operator [] ( const uint8 i ) const { return (ribi)(1<<i); }
	};
	static const _nesw nesw;

	/// Convert building layout index → ribi, 6 hex rotations.  Callers
	/// that semantically wanted 4 rotations (square-era buildings) need
	/// auditing — `layout_to_ribi[4]` is gone.
	static const ribi layout_to_ribi[6];

	static bool is_perpendicular(ribi x, ribi y);

	static uint8 get_numways(ribi x) {
#ifdef USE_GCC_POPCOUNT
		return (uint8)__builtin_popcount(x);
#else
		uint8 n = 0;
		for (uint8 b = x; b; b &= (uint8)(b-1)) { n++; }
		return n;
#endif
	}
	static bool is_single(ribi x)   { return get_numways(x) == 1; }
	static bool is_twoway(ribi x)   { return get_numways(x) == 2; }
	static bool is_threeway(ribi x) { return get_numways(x) >= 3; }

	/// True iff @p x's set bits all lie on a single hex axis, i.e.
	/// x is non-empty and its bits are a subset of one of the 3
	/// straight axis masks: {N, S}, {NE, SW}, {NW, SE}.  Matches the
	/// old 4-bit semantic — is_straight was true for any single
	/// direction and for the two axis-pair combos (N|S, E|W) — just
	/// extended to the 3 hex axes.  Used by way/signal/station code
	/// to decide "can we lay a straight object here".
	static bool is_straight(ribi x) {
		const ribi ns   = (ribi)(north     | south);
		const ribi nesw = (ribi)(northeast | southwest);
		const ribi nwse = (ribi)(northwest | southeast);
		return x != none
		    && ((x & ~ns) == 0 || (x & ~nesw) == 0 || (x & ~nwse) == 0);
	}
	/// True iff @p x's set bits are all on the N-S axis (north,
	/// south, or both).  Hex-only predicates for the other two axes
	/// (NE-SW, NW-SE) deliberately don't exist yet — callers using
	/// is_straight_ns historically split 2-axis logic, and hex has
	/// 3 axes; those callers need explicit triage.
	static bool is_straight_ns(ribi x) {
		const ribi ns = (ribi)(north | south);
		return x != none && (x & ~ns) == 0;
	}

	/// Bend: exactly two bits set, NOT opposite (i.e. a real corner).
	static bool is_bend(ribi x) { return is_twoway(x) && !is_straight(x); }

	/// Bitwise opposite: flip each set bit to its 180°-opposite hex
	/// edge.  Bits 0↔3, 1↔4, 2↔5 — a 3-position rotation of the
	/// 6-bit word.  For single-bit ribis returns the opposite single
	/// direction; backward(none) = none, backward(all) = all.  NOTE:
	/// the old 4-bit semantics had backward(none) = all and
	/// backward(all) = none, which were quirks of the bitwise-NOT
	/// implementation; the hex version is the clean "flip each edge".
	static ribi backward(ribi x) {
		return (ribi)(((x << 3) | (x >> 3)) & 0x3f);
	}

	/// Straight-axis extension of a single direction (e.g. N → N|S);
	/// 0 for non-single inputs.
	static ribi doubles(ribi x) {
		return is_single(x) ? (ribi)(x | backward(x)) : (ribi)0;
	}

	/// Convert ribi to dir (sprite-raster index).  Still projects onto
	/// the 8-value square dir enum until the viewport/sprite port
	/// lands.  Multi-bit inputs and NE/SE/etc. that don't have a
	/// square-era dir slot return dir_invalid.
	static dir get_dir(ribi x);

	/// Alias for `backward`.  Under the old 4-bit layout this was a
	/// bit-rotation fast path for single-direction ribis (while
	/// `backward` went through a lookup table); under hex the two
	/// converge on the same rotate-by-3 expression.  Kept for
	/// grep-continuity with existing callers.
	static inline ribi reverse_single(ribi x) { return backward(x); }

	/// Rotate 60° clockwise (bit i → bit (i+1) mod 6).  The old
	/// rotate90 is gone — a hex step is 60°, not 90°.  Callers that
	/// semantically wanted a quarter-turn (building-layout 4-cycles)
	/// are broken and need explicit auditing.
	static ribi rotate60(ribi x) {
		return (ribi)(((x << 1) | (x >> 5)) & 0x3f);
	}
	/// Rotate 60° counter-clockwise.
	static ribi rotate60l(ribi x) {
		return (ribi)(((x >> 1) | (x << 5)) & 0x3f);
	}

	/// Rotate a ribi by the amount the world rotates when the user
	/// issues a 90° map-rotate (`karte_t::rotate90`).  Called from
	/// every `obj_t::rotate90()` override that holds a ribi (ways,
	/// vehicles, signs, water flow, etc.).
	///
	/// Stubbed as `rotate60` — one-step rotation, matching the
	/// "single-bit rotation" semantic the old 4-ribi `rotate90` had
	/// on a 4-direction grid.  90° is not a hex symmetry and
	/// `karte_t::rotate90` is itself scheduled for redesign (see
	/// TODO.md); when that decision lands, update this one function
	/// body — callers don't change.
	static ribi rotate_for_map_rotate90(ribi x) { return rotate60(x); }

	/// Rotate a ribi to the "perpendicular axis" of @p x — the axis
	/// used by square-era crossroads collision-avoidance, broad-tunnel
	/// side tiles, canal orthogonality, diagonal-bend detection, and
	/// similar concepts that asked "what's 90° off this direction?".
	///
	/// Flat-top hex has no 90° axis relation; there are 3 hex axes
	/// (N-S, NE-SW, NW-SE) at 60° spacing.  Stubbed as `rotate60`:
	/// the "one step over" relation — same single-bit rotation the
	/// old `rotate90` carried on a 4-ribi, reinterpreted for the
	/// 6-ribi.  Per-caller review during the crossings-port cluster
	/// may pick something different per site (test both neighbour
	/// axes, redesign the check entirely).  The helper centralises
	/// the stub choice and names the audit surface.
	static ribi rotate_perpendicular(ribi x)   { return rotate60(x); }
	/// Counter-clockwise companion to `rotate_perpendicular`.
	static ribi rotate_perpendicular_l(ribi x) { return rotate60l(x); }
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
