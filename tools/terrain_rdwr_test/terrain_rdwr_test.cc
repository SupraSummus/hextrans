// Verifies the hex-aware save/load round-trip for terrain heights.
//
// Before the fix in ground/grund.cc the restore-grid block had two bugs:
//
//   BUG 1 — wrong slot: legacy_set_grid_hgt_nocheck(koord(q,r), z) targeted
//   grid_hgts[(q + r*(W+1))*2] = E slot of tile (q-1, r-1), not (q, r).
//   The written value was pos.z + corner_nw(slope) = NW corner of tile (q, r),
//   which canonicalises to E of (q-1, r) — off by one row from the target.
//
//   BUG 2 — missing SE: SE slots were never written; they stayed at groundwater,
//   losing every tile's SE corner height.
//
// The fix writes:
//   set_grid_hgt_nocheck(k, E,  pos.z + corner_e(slope))
//   set_grid_hgt_nocheck(k, SE, pos.z + corner_se(slope))
//
// This file keeps the standalone helpers in sync with:
//   src/simutrans/dataobj/koord.{h,cc}   — vertex_slot_* / canonical_vertex
//   src/simutrans/world/surface.h        — set_grid_hgt_nocheck
//   src/simutrans/ground/grund.cc        — the load "restore grid" block
//   documentation/hex-vertex-storage.md  — layout spec

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <vector>

using sint8  = int8_t;
using uint32 = uint32_t;

// ---------------------------------------------------------------------------
// Slope encoding — base-3 with 6 hex corners (E, SE, SW, W, NW, NE).
// Mirrors the macros in src/simutrans/dataobj/ribi.h.
// ---------------------------------------------------------------------------
using slope_t = int;

static int corner_e(slope_t s)  { return s % 3; }
static int corner_se(slope_t s) { return (s / 3) % 3; }

static slope_t encode_hex(int e, int se, int sw, int w, int nw, int ne)
{
    return e + se*3 + sw*9 + w*27 + nw*81 + ne*243;
}

// ---------------------------------------------------------------------------
// Per-vertex slot indexing — mirrors vertex_slot_count / vertex_slot_index.
//
// For a W×H tile map:
//   total slots = 2*(W+1)*(H+2)
//   E  slot of tile (q,r) = ((q+1) + (r+1)*(W+1)) * 2
//   SE slot of tile (q,r) = E slot + 1
// ---------------------------------------------------------------------------

static uint32 vertex_slot_count(int W, int H)
{
    return uint32(W + 1) * uint32(H + 2) * 2u;
}

static uint32 e_slot(int q, int r, int W)
{
    return uint32((q + 1) + (r + 1) * (W + 1)) * 2u;
}

static uint32 se_slot(int q, int r, int W)
{
    return e_slot(q, r, W) + 1u;
}

// ---------------------------------------------------------------------------
// Read the height at one of the 6 named corners of tile (q,r) using the
// canonical-vertex rules from hex-vertex-storage.md.
// Out-of-range canonical tiles return groundwater.
//
//   corner  canonical tile   slot
//   E       (q,   r)         E
//   SE      (q,   r)         SE
//   SW      (q-1, r+1)       E
//   W       (q-1, r)         SE
//   NW      (q-1, r)         E
//   NE      (q,   r-1)       SE
// ---------------------------------------------------------------------------

static sint8 read_corner(const sint8 *g, int q, int r, int W, int H,
                          sint8 ground, int c /* 0=E…5=NE */)
{
    int tq, tr;
    bool use_se;
    switch (c) {
    case 0: tq = q;   tr = r;   use_se = false; break;
    case 1: tq = q;   tr = r;   use_se = true;  break;
    case 2: tq = q-1; tr = r+1; use_se = false; break;
    case 3: tq = q-1; tr = r;   use_se = true;  break;
    case 4: tq = q-1; tr = r;   use_se = false; break;
    case 5: tq = q;   tr = r-1; use_se = true;  break;
    default: return ground;
    }
    if (tq < -1 || tq >= W || tr < -1 || tr > H) return ground;
    return use_se ? g[se_slot(tq, tr, W)] : g[e_slot(tq, tr, W)];
}

// ---------------------------------------------------------------------------
// Checks
// ---------------------------------------------------------------------------

static int fail_count = 0;

static void check_eq(sint8 actual, sint8 expected, const char *file, int line,
                     const char *desc)
{
    if (actual != expected) {
        fprintf(stderr, "FAIL %s:%d: got %d expected %d — %s\n",
                file, line, (int)actual, (int)expected, desc);
        fail_count++;
    }
}
#define CHECK_EQ(a, e, d) check_eq((a), (e), __FILE__, __LINE__, (d))

// ---------------------------------------------------------------------------
// test_roundtrip
//
// Sets up a W×H tile map with known per-vertex heights, derives the
// (pos_z, slope) that would be saved, applies the fixed load procedure,
// and checks that the restored grid matches the pre-save state.
//
// Heights are kept in {0,1,2} — the maximum base-3 slope digit — so that
// boundary/sentinel corners (which read as groundwater=0) cannot drag
// pos_z low enough to make any corner's delta exceed 2 and get clamped.
// ---------------------------------------------------------------------------

static void test_roundtrip()
{
    const int W = 6, H = 6;
    const sint8 groundwater = 0;
    const uint32 N = vertex_slot_count(W, H);

    std::vector<sint8> grid(N, groundwater);

    for (int r = 0; r < H; r++) {
        for (int q = 0; q < W; q++) {
            grid[e_slot(q, r, W)]  = sint8((q + r) % 3);
            grid[se_slot(q, r, W)] = sint8((q + 2*r) % 3);
        }
    }

    const std::vector<sint8> pre_save = grid;

    // SAVE: derive (pos_z, slope) for each tile.
    struct TileState { sint8 pos_z; slope_t slope; };
    std::vector<TileState> state(H * W);

    for (int r = 0; r < H; r++) {
        for (int q = 0; q < W; q++) {
            sint8 c[6];
            for (int i = 0; i < 6; i++)
                c[i] = read_corner(pre_save.data(), q, r, W, H, groundwater, i);
            sint8 pz = *std::min_element(c, c + 6);
            state[r * W + q] = { pz, encode_hex(
                std::min(int(c[0]-pz), 2), std::min(int(c[1]-pz), 2),
                std::min(int(c[2]-pz), 2), std::min(int(c[3]-pz), 2),
                std::min(int(c[4]-pz), 2), std::min(int(c[5]-pz), 2)) };
        }
    }

    // LOAD: fresh allocation, then each tile writes its two canonical slots.
    std::fill(grid.begin(), grid.end(), groundwater);

    for (int r = 0; r < H; r++) {
        for (int q = 0; q < W; q++) {
            const auto &s = state[r * W + q];
            grid[e_slot(q, r, W)]  = sint8(s.pos_z + corner_e(s.slope));
            grid[se_slot(q, r, W)] = sint8(s.pos_z + corner_se(s.slope));
        }
    }

    // Verify.
    char desc[64];
    for (int r = 0; r < H; r++) {
        for (int q = 0; q < W; q++) {
            snprintf(desc, sizeof(desc), "E  slot (%d,%d)", q, r);
            CHECK_EQ(grid[e_slot(q, r, W)],  pre_save[e_slot(q, r, W)],  desc);
            snprintf(desc, sizeof(desc), "SE slot (%d,%d)", q, r);
            CHECK_EQ(grid[se_slot(q, r, W)], pre_save[se_slot(q, r, W)], desc);
        }
    }
}

int main()
{
    test_roundtrip();

    if (fail_count == 0) {
        printf("terrain_rdwr_test: all checks passed\n");
        return 0;
    }
    fprintf(stderr, "terrain_rdwr_test: %d check(s) failed\n", fail_count);
    return 1;
}
