
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
 * Slopes of tiles.  Base-4 6-corner encoding: each corner holds
 * height 0, 1, 2 or 3; digit positions follow hex_corner_t (E=mult 1,
 * SE=4, SW=16, W=64, NW=256, NE=1024).  4^6 = 4096 possible slopes,
 * needs sint16.  Triple-height slopes kept.
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
		raised_E  = 1,    ///< E  corner, digit 0
		raised_SE = 4,    ///< SE corner, digit 1
		raised_SW = 16,   ///< SW corner, digit 2
		raised_W  = 64,   ///< W  corner, digit 3
		raised_NW = 256,  ///< NW corner, digit 4
		raised_NE = 1024, ///< NE corner, digit 5

		// Square-style single-corner aliases.  Under hex these name the
		// corresponding hex corner at height 1.
		southeast = raised_SE,
		southwest = raised_SW,
		northwest = raised_NW,
		northeast = raised_NE,

		// 2-corner "edge" slopes named by their LOW edge.  ::north and
		// ::south are the legacy bare names for the N and S hex edges;
		// the 4 hex-only edges (NE, SE, SW, NW) carry the `_edge` suffix
		// to disambiguate from the single-corner aliases above.  Track
		// running on the matching axis climbs 0→1 along its length.
		north   = raised_SE + raised_SW, ///< low edge N  (S corners raised)
		south   = raised_NE + raised_NW, ///< low edge S  (N corners raised)
		ne_edge = raised_SW + raised_W,  ///< low edge NE
		se_edge = raised_W  + raised_NW, ///< low edge SE
		sw_edge = raised_NE + raised_E,  ///< low edge SW
		nw_edge = raised_E  + raised_SE, ///< low edge NW

		// 4-corner "wide" axis slopes — narrow edge slope plus the 2
		// perpendicular side corners (the corners on the axis that
		// crosses the slope's axis at 90°).  Track on the same axis
		// sees the same 0→1 climb as the narrow variant; only the
		// off-axis ground shape differs.
		north_wide = north   + raised_E  + raised_W,  ///< low edge N, wide  (NS axis perpendicular = E, W)
		south_wide = south   + raised_E  + raised_W,  ///< low edge S, wide
		ne_wide    = ne_edge + raised_SE + raised_NW, ///< low edge NE, wide (NE-SW axis perpendicular = SE, NW)
		sw_wide    = sw_edge + raised_SE + raised_NW, ///< low edge SW, wide
		nw_wide    = nw_edge + raised_SW + raised_NE, ///< low edge NW, wide (NW-SE axis perpendicular = SW, NE)
		se_wide    = se_edge + raised_SW + raised_NE, ///< low edge SE, wide

		// Legacy 2-corner square diagonals.  NOT hex edges and NOT
		// way-buildable; kept as named values only because the
		// legacy_slope4 conversion helper at the bottom still needs to
		// map old square-era table indices to *some* slope value.
		east    = raised_NW + raised_SW, ///< 2 west corners raised (legacy square)
		west    = raised_NE + raised_SE, ///< 2 east corners raised (legacy square)

		all_up_one   = raised_E + raised_SE + raised_SW + raised_W + raised_NW + raised_NE, ///< all corners 1 high (= 1365)
		all_up_two   = all_up_one * 2, ///< all corners 2 high (= 2730)
		all_up_three = all_up_one * 3, ///< all corners 3 high (= 4095)

		raised = all_up_two,    ///< special meaning: used as slope of bridgeheads and in terraforming tools (keep for compatibility)

		max_number = all_up_three
	};

	/// Width of the encoding (number of distinct slope values). 4^6.
	static const int max_slopes = 4096;

	/*
	 * Macros to access the height of the 6 corners (base-4 digit
	 * extraction).  Corner bit positions match hex_corner_t.  Each
	 * macro returns 0, 1, 2, or 3.
	 */
#define corner_e(i)  ((i) % 4)
#define corner_se(i) (((i) / 4) % 4)
#define corner_sw(i) (((i) / 16) % 4)
#define corner_w(i)  (((i) / 64) % 4)
#define corner_nw(i) (((i) / 256) % 4)
#define corner_ne(i) ((i) / 1024)

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

	// True planar double-height edge ramps.  These are the 012210
	// family: two low-edge corners at 0, two high-edge corners at 2,
	// and the side corners at 1.  They face the same uphill direction
	// as the matching narrow edge slope.
	static constexpr type north_double = encode_corners_hex(1, 2, 2, 1, 0, 0);
	static constexpr type ne_double    = encode_corners_hex(0, 1, 2, 2, 1, 0);
	static constexpr type se_double    = encode_corners_hex(0, 0, 1, 2, 2, 1);
	static constexpr type south_double = encode_corners_hex(1, 0, 0, 1, 2, 2);
	static constexpr type sw_double    = encode_corners_hex(2, 1, 0, 0, 1, 2);
	static constexpr type nw_double    = encode_corners_hex(2, 2, 1, 0, 0, 1);

/// True if no corner exceeds height 1 (all base-4 digits are 0 or 1).
#define is_one_high(i) (!slope_t::has_double_corner(i))

	/// True if @p x has any corner at height >= 2.
	static bool has_double_corner(type x) {
		return corner_e(x) >= 2 || corner_se(x) >= 2 || corner_sw(x) >= 2
		    || corner_w(x) >= 2 || corner_nw(x) >= 2 || corner_ne(x) >= 2;
	}

	/// Compute the slope on the same axis but with the high and low
	/// edges swapped (180° hex rotation).  Returns flat if @p x is not
	/// a way-buildable edge slope.  Narrow → narrow, wide → wide; the
	/// pre-hex "corner-complement" formula is gone — under the 12-slope
	/// set it would have flipped narrow ↔ wide on the opposite axis,
	/// which no caller wants.
	static type opposite(type x)
	{
		if (!is_axis_slope(x)) return flat;
		return rotate60(rotate60(rotate60(x)));
	}

	/// Rotate by 60° clockwise (cyclically shift base-4 digits by 1
	/// position).  The 4-corner rotate90 is gone — hex has 6
	/// rotational positions so one step is 60°.  Callers that
	/// semantically wanted a quarter-turn (e.g. building-layout
	/// 4-cycles) are broken and need explicit auditing.
	static type rotate60(type x) { return (type)(((x % raised_NE) * 4) + (x / raised_NE)); }

	/// Returns true if @p x has all corners raised (any uniform height).
	static bool is_all_up(type x) { return x == all_up_one || x == all_up_two || x == all_up_three; }

	/// Maximum corner height delta across this slope.  0 for flat, 1-3 otherwise.
	/// ~35 callers in vehicle / bridge / road-builder / signal code assume the
	/// upstream square base-3 cap (`<= 2`) for clearance, collision and
	/// image-select branches.  Base-4 encoding allows max-corner = 3, so those
	/// sites may compute wrong on high-delta hex terrain — audit when next
	/// touched for hex correctness.
	static uint8 max_diff(type x) {
		return (uint8)max(max(max(corner_e(x), corner_se(x)), max(corner_sw(x), corner_w(x))), max(corner_nw(x), corner_ne(x)));
	}

	/// Minimum encoded corner height.  Canonical terrain slopes keep this
	/// at 0 and carry any common vertical offset in the tile's z position.
	static uint8 min_corner_height(type x) {
		return (uint8)min( min( min( corner_e(x),  corner_se(x) ),
		                       min( corner_sw(x), corner_w(x) ) ),
		                  min( corner_nw(x), corner_ne(x) ) );
	}

	/// Remove the common encoded corner height from @p x.
	static type lower_min_corner(type x) {
		const uint8 h = min_corner_height(x);
		return h == 0 ? x : (type)(x - h * all_up_one);
	}

	/// Computes minimum corner-height difference between @p high and @p low.
	static sint8 min_diff(type high, type low) {
		return min( min( min( corner_e(high)  - corner_e(low),  corner_se(high) - corner_se(low) ),
		                 min( corner_sw(high) - corner_sw(low), corner_w(high)  - corner_w(low) ) ),
		            min( corner_nw(high) - corner_nw(low), corner_ne(high) - corner_ne(low) ) );
	}

	/// Project a 6-corner hex slope onto a 4-corner subset by absorbing
	/// the 2 hex-only corners (E, W) into their adjacent square corners
	/// (E → max with SE and NE, W → max with SW and NW), dropping E
	/// and W, and clamping each square corner to height 1.  Result has
	/// `corner_e == corner_w == 0` and `max_diff <= 1` always.
	///
	/// The square pakset has no sprite for hex corners or double-height
	/// hex slopes; rendering uses this projection to pick the closest
	/// legal square sprite.  Lossy on purpose — the 4 hex-only edge
	/// slopes collapse pairwise (NE-edge and SE-edge → "west"; NW-edge
	/// and SW-edge → "east"), and double-height hex slopes lose a
	/// height step.  Centralised here so every consumer lands on the
	/// same rule until real hex sprite art arrives.
	static type project_to_square(type slope) {
		const uint8 e  = corner_e(slope);
		const uint8 w  = corner_w(slope);
		const uint8 se = (uint8)min(1, max(corner_se(slope), e));
		const uint8 ne = (uint8)min(1, max(corner_ne(slope), e));
		const uint8 sw = (uint8)min(1, max(corner_sw(slope), w));
		const uint8 nw = (uint8)min(1, max(corner_nw(slope), w));
		return encode_corners(sw, se, ne, nw);
	}

	/// Edge slopes that host a way along one direction: 6 genuine hex
	/// edges (2 adjacent corners raised) and 6 corresponding "wide"
	/// variants (4 corners raised — same low edge but the 2
	/// perpendicular side corners are also lifted).  All 12 are
	/// single-height; double-height edges and square-era diagonals
	/// (east, west) are no longer way-buildable.
	static bool is_axis_slope(type x) {
		switch (x) {
			case north:      case south:
			case ne_edge:    case se_edge:
			case sw_edge:    case nw_edge:
			case north_wide: case south_wide:
			case ne_wide:    case se_wide:
			case sw_wide:    case nw_wide:
				return true;
			default:
				return false;
		}
	}

	/// Does an axis crossing edges (a0, a1) entering and (b0, b1) exiting
	/// admit a way?  The four corners are the axis's two edges' endpoints,
	/// in hex_corner_t order.  Two cases:
	///   1. Ramp: both edges internally level, height delta 0 or 1.  The
	///      way slopes uniformly between them.
	///   2. Flat chord at some height H: each edge has at least one corner
	///      at H, i.e. the edges' height intervals
	///      [min(a0,a1)..max(a0,a1)] and [min(b0,b1)..max(b0,b1)] overlap.
	///      The way sits at the lowest such H, resting on the level corners
	///      of each edge.  A raised corner that's not at H is "off the way"
	///      — perimeter geometry the way passes by but doesn't touch.
	///      Excludes double-corner slopes.
	///
	/// The chord rule is symmetric in the two corners of each edge, so
	/// mirror-symmetric configurations get the same answer.
	static bool is_way_axis(type x, uint8 a0, uint8 a1, uint8 b0, uint8 b1) {
		if (a0 == a1 && b0 == b1 && (a0 == b0 || a0 + 1 == b0 || b0 + 1 == a0)) {
			return true; // ramp
		}
		return chord_h_axis(x, a0, a1, b0, b1) >= 0;
	}

	/// Returns the height H at which a flat way sits on this axis, or -1
	/// if no flat chord is admissible (axis is a ramp, or not way-buildable
	/// at all).  H is the lowest height in the overlap of the two edges'
	/// height intervals — the way rests on level corners on both edges.
	static sint8 chord_h_axis(type x, uint8 a0, uint8 a1, uint8 b0, uint8 b1) {
		if (has_double_corner(x)) return -1;
		const uint8 a_lo = a0 < a1 ? a0 : a1;
		const uint8 a_hi = a0 < a1 ? a1 : a0;
		const uint8 b_lo = b0 < b1 ? b0 : b1;
		const uint8 b_hi = b0 < b1 ? b1 : b0;
		const uint8 lo = a_lo > b_lo ? a_lo : b_lo;
		const uint8 hi = a_hi < b_hi ? a_hi : b_hi;
		return lo <= hi ? (sint8)lo : (sint8)-1;
	}

	/// Way buildable on this slope: at least one of the three hex axes
	/// admits it.  Includes the named edge ramps, their wide variants,
	/// opposite-corner saddles, single-corner peaks, and side chords
	/// across ramps.  flat and uniform all-up fall out of any axis (all
	/// four crossed corners equal).
	static bool is_way(type x) {
		return is_way_axis(x, corner_nw(x), corner_ne(x), corner_se(x), corner_sw(x))
		    || is_way_axis(x, corner_ne(x), corner_e(x),  corner_sw(x), corner_w(x))
		    || is_way_axis(x, corner_w(x),  corner_nw(x), corner_e(x),  corner_se(x));
	}

	static bool is_planar_double_edge(type x) {
		switch (x) {
			case north_double: case ne_double: case se_double:
			case south_double: case sw_double: case nw_double:
				return true;
			default:
				return false;
		}
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
 * Convert a square-era slope-table value (0..80) to the current
 * slope encoding.  The old table was four base-3 digits in SW, SE,
 * NE, NW order, so values such as 36/4/12/28 mean the four cardinal
 * slopes and 72/8/24/56 their double-height variants.
 */
static constexpr slope_t::type slope_from_legacy_slope4_table(sint16 sl)
{
	return encode_corners(sl % 3, (sl / 3) % 3, (sl / 9) % 3, (sl / 27) % 3);
}

static_assert(slope_from_legacy_slope4_table(36) == slope_t::south, "legacy south slope value changed");
static_assert(slope_from_legacy_slope4_table(4) == slope_t::north, "legacy north slope value changed");
static_assert(slope_from_legacy_slope4_table(12) == slope_t::west, "legacy west slope value changed");
static_assert(slope_from_legacy_slope4_table(28) == slope_t::east, "legacy east slope value changed");


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
		all       = 63,     ///< all 6 edges set

		/// The 3 "upper" hex edges (N, NE, NW) — exactly one bit per
		/// hex axis.  Masking any 2-bit opposite-pair ribi with this
		/// extracts one end: N-S → N, NE-SW → NE, NW-SE → NW.  The
		/// square-era "pick one end" idiom used the 4-bit combo
		/// constants `northeast` (N|E) and `southwest` (S|W) — under
		/// hex those are single edges and drop 2 of 3 bridge axes.
		upper_half = north | northeast | northwest,
		lower_half = south | southeast | southwest
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

	/// Hex-axis identifier for a straight @p x: returns the upper-half
	/// representative (`north`, `northwest`, or `northeast`) of the
	/// axis the set bits lie on, or `none` if @p x is empty or
	/// non-straight (a bend / multi-axis ribi).  Folds a 6-edge ribi
	/// onto its 3-axis equivalence class — the natural replacement for
	/// callers that historically used `is_straight_ns` to split a
	/// 2-axis world (now 3 axes) and want a single value to walk along.
	/// The returned ribi is also a valid `koord::step` argument for
	/// the canonical "forward" direction along that axis.
	static ribi straight_axis(ribi x) {
		const ribi ns_axis   = (ribi)(north     | south);
		const ribi nwse_axis = (ribi)(northwest | southeast);
		const ribi nesw_axis = (ribi)(northeast | southwest);
		if (x == none)              return none;
		if ((x & ~ns_axis)   == 0)  return north;
		if ((x & ~nwse_axis) == 0)  return northwest;
		if ((x & ~nesw_axis) == 0)  return northeast;
		return none;
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

/// Resolve the four crossed corners for the axis through @p r.  Both
/// ribis on the same axis yield the same corners; non-axis ribis (none,
/// multi-bit) return false with corners untouched.
static inline bool slope_corners_along_axis(slope_t::type sl, ribi_t::ribi r, uint8& a0, uint8& a1, uint8& b0, uint8& b1)
{
	switch (ribi_t::straight_axis(r)) {
		case ribi_t::north:     a0 = corner_nw(sl); a1 = corner_ne(sl); b0 = corner_se(sl); b1 = corner_sw(sl); return true;
		case ribi_t::northeast: a0 = corner_ne(sl); a1 = corner_e(sl);  b0 = corner_sw(sl); b1 = corner_w(sl);  return true;
		case ribi_t::northwest: a0 = corner_w(sl);  a1 = corner_nw(sl); b0 = corner_e(sl);  b1 = corner_se(sl); return true;
		default:                return false;
	}
}

/// Way buildable along the axis through @p r — see slope_t::is_way_axis.
static inline bool slope_allows_way_axis(slope_t::type sl, ribi_t::ribi r)
{
	uint8 a0, a1, b0, b1;
	return slope_corners_along_axis(sl, r, a0, a1, b0, b1) && slope_t::is_way_axis(sl, a0, a1, b0, b1);
}

/// Height at which a flat way sits along the axis through @p r, or -1 if
/// the axis is a ramp / not way-buildable / non-axis — see slope_t::chord_h_axis.
static inline sint8 slope_chord_h_along_axis(slope_t::type sl, ribi_t::ribi r)
{
	uint8 a0, a1, b0, b1;
	if (!slope_corners_along_axis(sl, r, a0, a1, b0, b1)) return -1;
	return slope_t::chord_h_axis(sl, a0, a1, b0, b1);
}

/// Convenience: way along this axis sits at constant height (flat
/// chord) rather than ramping.  Drives "render this slope's way /
/// wayobj as flat".  Accepts multi-bit ribi (bends, junctions): all
/// axes spanned by @p r must admit a flat chord at the same height.
static inline bool slope_allows_flat_way_chord_axis(slope_t::type sl, ribi_t::ribi r)
{
	sint8 h = -1;
	if (r & (ribi_t::north | ribi_t::south)) {
		const sint8 ah = slope_chord_h_along_axis(sl, ribi_t::north);
		if (ah < 0) return false;
		h = ah;
	}
	if (r & (ribi_t::northeast | ribi_t::southwest)) {
		const sint8 ah = slope_chord_h_along_axis(sl, ribi_t::northeast);
		if (ah < 0) return false;
		if (h < 0) h = ah; else if (h != ah) return false;
	}
	if (r & (ribi_t::northwest | ribi_t::southeast)) {
		const sint8 ah = slope_chord_h_along_axis(sl, ribi_t::northwest);
		if (ah < 0) return false;
		if (h < 0) h = ah; else if (h != ah) return false;
	}
	return h >= 0;
}

/// Height delta (above tile base) at which a way sits at the edge in
/// direction @p r.  Flat chord: returns the chord's H.  Ramp: returns
/// the edge's level corner height (entry on the axis's canonical
/// direction, exit on its opposite).  Non-axis ribi or flat slope: 0.
static inline sint8 slope_way_h_at_edge(slope_t::type sl, ribi_t::ribi r)
{
	uint8 a0, a1, b0, b1;
	if (!slope_corners_along_axis(sl, r, a0, a1, b0, b1)) return 0;
	const sint8 chord_h = slope_t::chord_h_axis(sl, a0, a1, b0, b1);
	if (chord_h >= 0) return chord_h;
	return (r == ribi_t::straight_axis(r)) ? (sint8)a0 : (sint8)b0;
}

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
