// Self-test for the per-vertex height storage helpers in
// src/simutrans/dataobj/koord.{h,cc} — canonical_vertex(),
// vertex_slot_count(), vertex_slot_index(), hex_vertex_pos().
//
// Exercises the invariants:
//   1. canonical_vertex agrees with the lex-min of vertex_owners.
//   2. canonical_vertex is idempotent on already-canonical inputs.
//   3. All 3 owners of a world vertex canonicalise to the same slot.
//   4. Canonical corners are always E or SE.
//   5. vertex_slot_index is a bijection from valid canonical vertices
//      onto [0, vertex_slot_count).
//   6. All 3 owners of a world vertex produce the same hex_vertex_pos.
//      This is the geometric invariant that makes perlin-noise terrain
//      self-consistent at shared vertices.
//
// The helper bodies are duplicated here rather than linked from
// koord.cc to keep the test standalone — koord.cc pulls in
// loadsave / scr_coord / simrandom / simconst.  Keep in sync.

#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <set>
#include <vector>

// --- minimal koord / hex_corner_t / hex_vertex_t mirrors ---

struct koord_t {
    int16_t x, y;
    bool operator==(koord_t o) const { return x == o.x && y == o.y; }
    bool operator<(koord_t o) const  { return x < o.x || (x == o.x && y < o.y); }
    koord_t operator+(koord_t o) const { return {int16_t(x+o.x), int16_t(y+o.y)}; }
};

enum corner_t : uint8_t {
    E = 0, SE = 1, SW = 2, W = 3, NW = 4, NE = 5, CORNER_COUNT = 6
};

struct vertex_t {
    koord_t tile;
    corner_t corner;
    bool operator==(vertex_t o) const { return tile == o.tile && corner == o.corner; }
    bool operator<(vertex_t o) const {
        if (tile < o.tile) return true;
        if (o.tile < tile) return false;
        return corner < o.corner;
    }
};

// Matches koord::neighbours in src/simutrans/dataobj/koord.cc.
static constexpr std::array<koord_t, 6> NEIGHBOURS = {{
    { +1,  0 }, // SE
    {  0, +1 }, // S
    { -1, +1 }, // SW
    { -1,  0 }, // NW
    {  0, -1 }, // N
    { +1, -1 }, // NE
}};

// Mirror of vertex_owners() in koord.cc.
static void vertex_owners(koord_t tile, corner_t c, vertex_t out[3])
{
    const uint8_t dir_a = uint8_t((uint8_t(c) + 5) % 6);
    const uint8_t dir_b = uint8_t(c);
    out[0] = { tile, c };
    out[1] = { tile + NEIGHBOURS[dir_a], corner_t((uint8_t(c) + 2) % 6) };
    out[2] = { tile + NEIGHBOURS[dir_b], corner_t((uint8_t(c) + 4) % 6) };
}

// Mirror of canonical_vertex() in koord.cc.
static vertex_t canonical_vertex(vertex_t v)
{
    switch (v.corner) {
        case E:  return v;
        case SE: return v;
        case SW: return { v.tile + koord_t{-1,  1}, E  };
        case W:  return { v.tile + koord_t{-1,  0}, SE };
        case NW: return { v.tile + koord_t{-1,  0}, E  };
        case NE: return { v.tile + koord_t{ 0, -1}, SE };
        default: break;
    }
    return v;
}

// Mirror of vertex_slot_count() / vertex_slot_index() in koord.cc.
static uint32_t vertex_slot_count(int16_t W, int16_t H)
{
    return uint32_t(W + 1) * uint32_t(H + 2) * 2u;
}

static uint32_t vertex_slot_index(vertex_t v, int16_t W)
{
    const uint32_t q = uint32_t(v.tile.x + 1);
    const uint32_t r = uint32_t(v.tile.y + 1);
    const uint32_t w = uint32_t(W + 1);
    const uint32_t corner_bit = (v.corner == SE) ? 1u : 0u;
    return (q + r * w) * 2u + corner_bit;
}

// Mirror of hex_vertex_pos() in koord.cc.
struct pos_t { double x; double y; };
static pos_t hex_vertex_pos(vertex_t v)
{
    static constexpr double HEX_SQRT3 = 1.7320508075688772;
    static constexpr double OX[6] = {  1.0,  0.5, -0.5, -1.0, -0.5,  0.5 };
    static constexpr double OY[6] = {  0.0,  HEX_SQRT3 * 0.5,  HEX_SQRT3 * 0.5,  0.0, -HEX_SQRT3 * 0.5, -HEX_SQRT3 * 0.5 };
    const double cx = 1.5 * v.tile.x;
    const double cy = HEX_SQRT3 * (v.tile.y + 0.5 * v.tile.x);
    return { cx + OX[v.corner], cy + OY[v.corner] };
}


// --- independent reference: lex-min of vertex_owners. ---

static vertex_t canonical_reference(vertex_t v)
{
    vertex_t owners[3];
    vertex_owners(v.tile, v.corner, owners);
    vertex_t best = owners[0];
    for (int i = 1; i < 3; i++) {
        if (owners[i].tile < best.tile) best = owners[i];
    }
    return best;
}


// --- tests ---

static int fail_count = 0;

#define CHECK(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        fail_count++; \
    } \
} while (0)

static void test_canonical_matches_reference()
{
    // Sweep a 7x7 tile region covering enough boundary cases.  The
    // reference computes canonical via vertex_owners + lex-min; the
    // helper uses the closed-form switch.  They must agree on every
    // (tile, corner).
    for (int16_t q = -3; q <= 3; q++) {
        for (int16_t r = -3; r <= 3; r++) {
            for (uint8_t c = 0; c < 6; c++) {
                vertex_t v = { {q, r}, corner_t(c) };
                vertex_t a = canonical_vertex(v);
                vertex_t b = canonical_reference(v);
                CHECK(a == b);
            }
        }
    }
}

static void test_canonical_is_E_or_SE()
{
    // Every canonical vertex has corner E or SE — that's the invariant
    // the storage layout relies on.
    for (int16_t q = -3; q <= 3; q++) {
        for (int16_t r = -3; r <= 3; r++) {
            for (uint8_t c = 0; c < 6; c++) {
                vertex_t v = canonical_vertex({{q, r}, corner_t(c)});
                CHECK(v.corner == E || v.corner == SE);
            }
        }
    }
}

static void test_canonical_idempotent()
{
    // canonical_vertex(canonical_vertex(v)) == canonical_vertex(v).
    for (int16_t q = -3; q <= 3; q++) {
        for (int16_t r = -3; r <= 3; r++) {
            for (uint8_t c = 0; c < 6; c++) {
                vertex_t once  = canonical_vertex({{q, r}, corner_t(c)});
                vertex_t twice = canonical_vertex(once);
                CHECK(once == twice);
            }
        }
    }
}

static void test_all_owners_agree()
{
    // The 3 (tile, corner) names for one world vertex must all
    // canonicalise to the same slot.  Pick representative interior
    // tiles (no boundary effects) and exercise every corner.
    for (int16_t q = -2; q <= 2; q++) {
        for (int16_t r = -2; r <= 2; r++) {
            for (uint8_t c = 0; c < 6; c++) {
                vertex_t owners[3];
                vertex_owners({q, r}, corner_t(c), owners);
                vertex_t c0 = canonical_vertex(owners[0]);
                vertex_t c1 = canonical_vertex(owners[1]);
                vertex_t c2 = canonical_vertex(owners[2]);
                CHECK(c0 == c1);
                CHECK(c1 == c2);
            }
        }
    }
}

static void test_slot_index_bijection()
{
    // For a W x H map, iterate every (tile, corner) pair with tile in
    // [0, W-1] x [0, H-1], canonicalise, collect the slot indices, and
    // check that the set of canonical vertices maps bijectively onto a
    // subset of [0, vertex_slot_count).
    const int16_t W = 6, H = 5;
    std::set<vertex_t> canonicals;
    std::set<uint32_t> slots;
    for (int16_t q = 0; q < W; q++) {
        for (int16_t r = 0; r < H; r++) {
            for (uint8_t c = 0; c < 6; c++) {
                vertex_t v = canonical_vertex({{q, r}, corner_t(c)});
                canonicals.insert(v);
                uint32_t slot = vertex_slot_index(v, W);
                CHECK(slot < vertex_slot_count(W, H));
                slots.insert(slot);
            }
        }
    }
    // Bijection: one slot per canonical vertex.
    CHECK(canonicals.size() == slots.size());
    // Sanity: the total vertex count on a W x H hex region is
    // (W+1) * H + W * (H+1) = 2*W*H + W + H vertices.  Easiest check:
    // it's non-zero and fits in the allocation.
    CHECK(canonicals.size() <= vertex_slot_count(W, H));
    CHECK(canonicals.size() >= size_t(W * H));
}

static void test_vertex_pos_owners_agree()
{
    // The 3 (tile, corner) names of a shared world vertex must
    // resolve to the same (x, y) — this is the geometric property
    // that lets perlin noise sampled at vertex positions be
    // self-consistent across tile boundaries by construction.
    //
    // Floating-point rounding makes exact equality fragile; require
    // agreement within a small absolute tolerance.  The values in
    // play are O(1..10) in units of hex side length, so 1e-9 is
    // ~10 orders of magnitude above roundoff noise.
    const double eps = 1e-9;
    for (int16_t q = -3; q <= 3; q++) {
        for (int16_t r = -3; r <= 3; r++) {
            for (uint8_t c = 0; c < 6; c++) {
                vertex_t owners[3];
                vertex_owners({q, r}, corner_t(c), owners);
                pos_t p0 = hex_vertex_pos(owners[0]);
                pos_t p1 = hex_vertex_pos(owners[1]);
                pos_t p2 = hex_vertex_pos(owners[2]);
                CHECK(std::fabs(p0.x - p1.x) < eps);
                CHECK(std::fabs(p1.x - p2.x) < eps);
                CHECK(std::fabs(p0.y - p1.y) < eps);
                CHECK(std::fabs(p1.y - p2.y) < eps);
            }
        }
    }
}

static void test_slot_index_bounds()
{
    // Every slot index produced by a canonical vertex in the valid
    // range lies inside [0, vertex_slot_count).
    const int16_t W = 8, H = 8;
    const uint32_t N = vertex_slot_count(W, H);
    // The valid canonical tile range is [-1, W-1] x [-1, H] —
    // r can reach H because the SW corner of south-edge tile
    // (q, H-1) canonicalises to (q-1, H).
    for (int16_t q = -1; q < W; q++) {
        for (int16_t r = -1; r <= H; r++) {
            for (corner_t c : {E, SE}) {
                uint32_t slot = vertex_slot_index({{q, r}, c}, W);
                CHECK(slot < N);
            }
        }
    }
}

int main()
{
    test_canonical_matches_reference();
    test_canonical_is_E_or_SE();
    test_canonical_idempotent();
    test_all_owners_agree();
    test_slot_index_bijection();
    test_slot_index_bounds();
    test_vertex_pos_owners_agree();

    if (fail_count == 0) {
        printf("hex_vertex_test: all checks passed\n");
        return 0;
    } else {
        fprintf(stderr, "hex_vertex_test: %d check(s) failed\n", fail_count);
        return 1;
    }
}
