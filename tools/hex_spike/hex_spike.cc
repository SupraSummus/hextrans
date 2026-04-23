// Hex-grid spike for the Simutrans hex-port plan.
//
// Goal: surface the geometry questions before committing to a design.
// Not part of the build.  Compile with the bundled Makefile and run;
// it asserts everything important and writes hex_spike.ppm.
//
// What this proves out:
//   - flat-top axial coords + 6-neighbour table
//   - 6-corner slope encoding (3 heights per corner -> 729 slopes)
//   - hex -> pixel projection AND inverse (mouse-picking)
//   - vertex sharing: each hex vertex belongs to exactly 3 hex tiles
//   - rendering by per-pixel inverse projection
//
// Naming convention (matches src/simutrans/dataobj/koord.h):
//   - EDGES (and the 6 neighbour directions reached through them):
//       N, NE, SE, S, SW, NW.  Flat-top hexes DO have due-N and due-S
//       edges.
//   - CORNERS / vertices (6 per tile):
//       E, SE, SW, W, NW, NE.  Flat-top hexes do NOT have due-N or
//       due-S corners; the 6 vertices sit at angles 0, 60, 120, 180,
//       240, 300 from the centre.
//
// What this DELIBERATELY skips: isometric projection, sprite art, ribi
// connections, tunnels/bridges.  Each of those is a separate spike.

#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <set>
#include <vector>

// ---------------------------------------------------------------------------
// Coordinates
// ---------------------------------------------------------------------------

// Axial (q, r).  +q points 30 south of due-east in screen space.
struct hex {
    int q, r;
    bool operator==(hex o) const { return q == o.q && r == o.r; }
    bool operator<(hex o) const  { return q < o.q || (q == o.q && r < o.r); }
};

// 6 NEIGHBOUR / EDGE directions, clockwise starting from the SE
// neighbour (which is at axial (1, 0)).  Order matches
// src/simutrans/dataobj/koord.cc::neighbours[].
enum dir6 { SE = 0, S, SW, NW, N, NE, DIR6_COUNT };
static constexpr std::array<hex, DIR6_COUNT> NEIGHBOURS{{
    { +1,  0 }, // SE
    {  0, +1 }, // S
    { -1, +1 }, // SW
    { -1,  0 }, // NW
    {  0, -1 }, // N
    { +1, -1 }, // NE
}};

// Cube distance: |dx|+|dy|+|dz| / 2.  Replaces koord_distance.
static int hex_distance(hex a, hex b)
{
    const int dx = a.q - b.q;
    const int dz = a.r - b.r;
    const int dy = -dx - dz;
    return (std::abs(dx) + std::abs(dy) + std::abs(dz)) / 2;
}

// ---------------------------------------------------------------------------
// Slopes — 6 corners, each 0/1/2 high
// ---------------------------------------------------------------------------
//
// 3^6 = 729 distinct slopes, fits in uint16_t.  The hex-grid plan
// suggests dropping double-height for the initial port (2^6 = 64,
// fits in sint8); we encode the wider variant here so that the spike
// can show double-height working, leaving the choice to the port.
//
// Corners are ordered clockwise starting from the E (0) vertex.
// Layout matches src/simutrans/dataobj/koord.h's corner convention
// (E, SE, SW, W, NW, NE).

enum corner6 { E_corner = 0, SE_corner, SW_corner, W_corner, NW_corner, NE_corner, CORNER6_COUNT };

using hex_slope = uint16_t;

static int corner_height(hex_slope s, corner6 c)
{
    int p = 1;
    for (int i = 0; i < c; ++i) p *= 3;
    return (s / p) % 3;
}

static hex_slope encode_slope(int e, int se, int sw, int w, int nw, int ne)
{
    return (hex_slope)(e + 3*se + 9*sw + 27*w + 81*nw + 243*ne);
}

// ---------------------------------------------------------------------------
// Vertex sharing
// ---------------------------------------------------------------------------
//
// Each hex vertex is shared by exactly 3 tiles (vs 4 for a square
// corner).  We name a vertex by (hex, corner6) but the *same* world
// vertex has 3 such names.  Returning the canonical 3-tile set is a
// sanity check on the topology — if this is wrong, terraforming-by-
// vertex is wrong.
//
// Geometry: the corner at angle (60 * c) from centre is between the
// neighbours through the two edges adjacent to that corner.  With the
// orderings above:
//   corner E      is between SE neighbour (dir 0) and NE neighbour (dir 5)
//   corner SE     is between S  neighbour (dir 1) and SE neighbour (dir 0)
//   corner SW     is between SW neighbour (dir 2) and S  neighbour (dir 1)
//   ... and so on cyclically.
// In general: corner c is between neighbour dir (c+5)%6 and neighbour
// dir c, when corners and dirs are numbered as above.

static std::array<std::pair<hex, corner6>, 3> vertex_owners(hex h, corner6 c)
{
    const dir6 d_a = (dir6)((c + 5) % 6);
    const dir6 d_b = (dir6)(c);
    const hex n_a { h.q + NEIGHBOURS[d_a].q, h.r + NEIGHBOURS[d_a].r };
    const hex n_b { h.q + NEIGHBOURS[d_b].q, h.r + NEIGHBOURS[d_b].r };
    // The same world vertex, seen from each owner, sits at a different
    // corner index — rotated by 2 positions per owner.
    return {{
        { h,   c },
        { n_a, (corner6)((c + 2) % 6) },
        { n_b, (corner6)((c + 4) % 6) },
    }};
}

// ---------------------------------------------------------------------------
// Projection (top-down, flat-top)
// ---------------------------------------------------------------------------

static constexpr double R = 30.0;            // hex size (centre-to-corner) in pixels
static constexpr double SQRT3 = 1.7320508075688772;

struct vec2 { double x, y; };

static vec2 hex_to_px(hex h)
{
    return { R * 1.5 * h.q, R * SQRT3 * (h.r + h.q / 2.0) };
}

// Inverse projection: pixel -> fractional axial -> rounded hex.
static hex px_to_hex(vec2 p)
{
    const double q_f = (2.0 / 3.0 * p.x) / R;
    const double r_f = (-1.0 / 3.0 * p.x + SQRT3 / 3.0 * p.y) / R;
    // round in cube space
    double x = q_f, z = r_f, y = -x - z;
    double rx = std::round(x), ry = std::round(y), rz = std::round(z);
    const double dx = std::fabs(rx - x), dy = std::fabs(ry - y), dz = std::fabs(rz - z);
    if      (dx > dy && dx > dz) rx = -ry - rz;
    else if (dy > dz)            ry = -rx - rz;
    else                         rz = -rx - ry;
    return { (int)rx, (int)rz };
}

// Corner offset relative to hex centre, in pixels.
// Corner index c is at angle (60 * c) from centre.
static vec2 corner_offset(corner6 c)
{
    const double angle = (M_PI / 3.0) * c;     // 0 = E, then clockwise in screen Y-down
    return { R * std::cos(angle), R * std::sin(angle) };
}

// Interpolate height inside a hex by triangulating into 6 wedges from
// the centre and doing barycentric in the wedge containing px.
// The centre's height is the average of the 6 corner heights.
static double height_at(hex h, hex_slope s, vec2 px)
{
    const vec2 centre = hex_to_px(h);
    const vec2 local  = { px.x - centre.x, px.y - centre.y };

    double h_centre = 0;
    for (int c = 0; c < CORNER6_COUNT; ++c) h_centre += corner_height(s, (corner6)c);
    h_centre /= CORNER6_COUNT;

    double angle = std::atan2(local.y, local.x);
    if (angle < 0) angle += 2 * M_PI;
    const int wedge = (int)std::floor(angle / (M_PI / 3)) % 6;

    const corner6 c1 = (corner6)wedge;
    const corner6 c2 = (corner6)((wedge + 1) % 6);
    const vec2 v1 = corner_offset(c1), v2 = corner_offset(c2);

    const double det = v1.x * v2.y - v2.x * v1.y;
    if (std::fabs(det) < 1e-9) return h_centre;
    const double a = (local.x * v2.y - local.y * v2.x) / det;
    const double b = (local.y * v1.x - local.x * v1.y) / det;
    const double c = 1 - a - b;
    return c * h_centre + a * corner_height(s, c1) + b * corner_height(s, c2);
}

// ---------------------------------------------------------------------------
// Render
// ---------------------------------------------------------------------------

struct tile { hex h; hex_slope s; };

static void write_ppm(const char *path, int W, int H, const std::vector<uint8_t> &rgb)
{
    std::ofstream f(path, std::ios::binary);
    f << "P6\n" << W << " " << H << "\n255\n";
    f.write((const char *)rgb.data(), rgb.size());
}

// ---------------------------------------------------------------------------
// Asserts that double as the spike's regression checks
// ---------------------------------------------------------------------------

static void assert_geometry()
{
    // Every neighbour is at hex-distance 1.
    for (auto n : NEIGHBOURS) {
        assert(hex_distance({0, 0}, n) == 1);
    }

    // Slope encode/decode round-trip across all 6 corners.
    const hex_slope s = encode_slope(0, 1, 2, 0, 1, 2);
    assert(corner_height(s, E_corner)  == 0);
    assert(corner_height(s, SE_corner) == 1);
    assert(corner_height(s, SW_corner) == 2);
    assert(corner_height(s, W_corner)  == 0);
    assert(corner_height(s, NW_corner) == 1);
    assert(corner_height(s, NE_corner) == 2);

    // px_to_hex is the left inverse of hex_to_px on integer hex coords.
    for (int q = -3; q <= 3; ++q) {
        for (int r = -3; r <= 3; ++r) {
            const hex h { q, r };
            assert(px_to_hex(hex_to_px(h)) == h);
        }
    }

    // Each vertex has exactly 3 distinct tile owners, and querying any
    // of those owners with the corresponding corner returns the same set.
    for (int c = 0; c < CORNER6_COUNT; ++c) {
        const auto owners = vertex_owners({0, 0}, (corner6)c);
        std::set<hex> tiles;
        for (auto &o : owners) tiles.insert(o.first);
        assert(tiles.size() == 3);
        // Round-trip from each owner's perspective.
        for (auto &o : owners) {
            const auto from_o = vertex_owners(o.first, o.second);
            std::set<hex> from_o_tiles;
            for (auto &p : from_o) from_o_tiles.insert(p.first);
            assert(from_o_tiles == tiles);
        }
    }
}

// ---------------------------------------------------------------------------
// main: assert geometry, render a 7-tile neighbourhood
// ---------------------------------------------------------------------------

int main()
{
    assert_geometry();
    std::printf("geometry asserts: OK\n");

    // 7 tiles: origin (flat) + 6 neighbours, each with a different
    // slope so the 6-corner encoding is visibly doing something.
    const std::vector<tile> tiles = {
        { { 0, 0 },        encode_slope(0, 0, 0, 0, 0, 0) },
        { NEIGHBOURS[SE],  encode_slope(0, 0, 1, 1, 0, 0) },
        { NEIGHBOURS[S],   encode_slope(0, 0, 0, 1, 1, 0) },
        { NEIGHBOURS[SW],  encode_slope(0, 0, 0, 0, 1, 1) },
        { NEIGHBOURS[NW],  encode_slope(1, 0, 0, 0, 0, 1) },
        { NEIGHBOURS[N],   encode_slope(1, 1, 0, 0, 0, 0) },
        { NEIGHBOURS[NE],  encode_slope(2, 2, 0, 0, 0, 0) }, // double-height
    };

    const int W = 400, H = 400;
    std::vector<uint8_t> rgb(W * H * 3, 30); // dark grey background
    for (int py = 0; py < H; ++py) {
        for (int px = 0; px < W; ++px) {
            const vec2 world { px - W / 2.0, py - H / 2.0 };
            const hex picked = px_to_hex(world);
            for (auto &t : tiles) {
                if (t.h == picked) {
                    const double h = height_at(t.h, t.s, world);
                    const uint8_t shade = (uint8_t)std::min(255.0, 80 + h * 70);
                    const int o = (py * W + px) * 3;
                    rgb[o + 0] = shade;
                    rgb[o + 1] = (uint8_t)(shade * 0.85);
                    rgb[o + 2] = (uint8_t)(shade * 0.55);
                    break;
                }
            }
        }
    }
    write_ppm("hex_spike.ppm", W, H, rgb);
    std::printf("wrote hex_spike.ppm (%dx%d)\n", W, H);
    return 0;
}
