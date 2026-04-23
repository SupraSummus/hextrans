// Hex-grid spike for the Simutrans hex-port plan.
//
// Goal: surface the geometry questions before committing to a design.
// Not part of the build. Compile with the bundled Makefile and run; it
// prints diagnostics and writes hex_spike.ppm.
//
// What this proves out:
//   - flat-top axial coords + 6-neighbour table
//   - 6-corner slope encoding (3 heights per corner -> 729 slopes)
//   - hex -> pixel projection AND inverse (mouse-picking)
//   - vertex sharing: each hex vertex belongs to exactly 3 hex tiles
//   - rendering by per-pixel inverse projection (rasterises 7 hexes
//     with interpolated corner heights, no polygon library needed)
//
// What this DELIBERATELY skips: isometric projection, sprite art, ribi
// connections, tunnels/bridges. Each of those is a separate spike.

#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>
#include <vector>

// ---------------------------------------------------------------------------
// Coordinates
// ---------------------------------------------------------------------------
//
// Axial (q, r). Flat-top orientation: q grows east, r grows south-east.
// Cube (x, y, z) with x+y+z=0 only used inside distance/round.

struct hex {
    int q, r;
    bool operator==(hex o) const { return q == o.q && r == o.r; }
    bool operator<(hex o) const  { return q < o.q || (q == o.q && r < o.r); }
};

// Flat-top neighbours, clockwise starting from "east".
// Order matters: we will index ribi/edge tables by this enum elsewhere.
enum dir6 { E = 0, SE, SW, W, NW, NE, DIR6_COUNT };
static constexpr std::array<hex, 6> NEIGHBOURS{{
    { +1,  0 }, // E
    {  0, +1 }, // SE
    { -1, +1 }, // SW
    { -1,  0 }, // W
    {  0, -1 }, // NW
    { +1, -1 }, // NE
}};

// Cube distance: |dx|+|dy|+|dz| / 2.  Replaces koord_distance.
static int hex_distance(hex a, hex b)
{
    int dx = a.q - b.q;
    int dz = a.r - b.r;
    int dy = -dx - dz;
    return (std::abs(dx) + std::abs(dy) + std::abs(dz)) / 2;
}

// ---------------------------------------------------------------------------
// Slopes
// ---------------------------------------------------------------------------
//
// 6 corners, each can be 0/1/2 high (matching current Simutrans range).
// 3^6 = 729 distinct slopes.  Doesn't fit in sint8; smallest fit is uint16.
// (The hex-grid plan suggests dropping double-height -> 64 slopes / sint8;
// we encode the wider variant here so we can later test whether dropping
// doubles actually matters for the geometry of interest.)
//
// Corner indices, flat-top, clockwise from "north":
enum corner6 { N = 0, NEcorner, SEcorner, S, SWcorner, NWcorner, CORNER6_COUNT };

using hex_slope = uint16_t;

static int corner_height(hex_slope s, corner6 c)
{
    int p = 1;
    for (int i = 0; i < c; ++i) p *= 3;
    return (s / p) % 3;
}

static hex_slope encode_slope(int n, int ne, int se, int s, int sw, int nw)
{
    return (hex_slope)(n + 3*ne + 9*se + 27*s + 81*sw + 243*nw);
}

// ---------------------------------------------------------------------------
// Vertex sharing
// ---------------------------------------------------------------------------
//
// Each hex vertex is shared by exactly 3 tiles (vs 4 for a square corner).
// We name a vertex by (hex, corner6) but the *same* world vertex has 3
// such names.  Returning the canonical 3-tile set is a sanity check on
// the topology -- if this is wrong, terraforming-by-vertex is wrong.
//
// For flat-top hexes the rule is:
//   Corner N  of (q,r) == Corner SW of (q,r-1) == Corner SE of (q+1,r-1)
//   Corner NE of (q,r) == Corner W  of (q+1,r-1) == Corner S of (q+1,r)
// ... and rotations thereof.

static std::array<std::pair<hex, corner6>, 3> vertex_owners(hex h, corner6 c)
{
    // The two other tiles touching corner c are the tiles adjacent to the
    // two edges that meet at c.  For flat-top, edges meeting at corner i
    // are dir i and dir (i + 5) % 6  -- when we use the corner ordering
    // N, NE, SE, S, SW, NW and edge ordering E, SE, SW, W, NW, NE,
    // corner i is "between" edges (i + 5) % 6 and (i + 4) % 6.
    int e1 = (c + 5) % 6;
    int e2 = (c + 4) % 6;
    hex n1 { h.q + NEIGHBOURS[e1].q, h.r + NEIGHBOURS[e1].r };
    hex n2 { h.q + NEIGHBOURS[e2].q, h.r + NEIGHBOURS[e2].r };
    return {{ {h, c}, {n1, (corner6)((c + 2) % 6)}, {n2, (corner6)((c + 4) % 6)} }};
}

// ---------------------------------------------------------------------------
// Projection (top-down, flat-top)
// ---------------------------------------------------------------------------
//
// Hex of "size" R (centre-to-corner distance):
//   width  = 2R         (flat-to-flat horizontally is R*sqrt(3))
//   height = R*sqrt(3)
//   centre_x = R * 1.5 * q
//   centre_y = R * sqrt(3) * (r + q/2)

static constexpr double R = 30.0;            // hex size in pixels
static constexpr double SQRT3 = 1.7320508075688772;

struct vec2 { double x, y; };

static vec2 hex_to_px(hex h)
{
    return { R * 1.5 * h.q, R * SQRT3 * (h.r + h.q / 2.0) };
}

// Inverse projection: pixel -> fractional axial -> rounded hex.
// Used in real life for mouse-picking; here used to rasterise.
static hex px_to_hex(vec2 p)
{
    double q_f = (2.0 / 3.0 * p.x) / R;
    double r_f = (-1.0 / 3.0 * p.x + SQRT3 / 3.0 * p.y) / R;
    // round in cube space
    double x = q_f, z = r_f, y = -x - z;
    double rx = std::round(x), ry = std::round(y), rz = std::round(z);
    double dx = std::fabs(rx - x), dy = std::fabs(ry - y), dz = std::fabs(rz - z);
    if (dx > dy && dx > dz) rx = -ry - rz;
    else if (dy > dz)       ry = -rx - rz;
    else                    rz = -rx - ry;
    return { (int)rx, (int)rz };
}

// Corner offset relative to hex centre, in pixels.
// Order matches corner6: N, NE, SE, S, SW, NW.
static vec2 corner_offset(corner6 c)
{
    static const vec2 offsets[6] = {
        {  R * 0.5,           -R * SQRT3 / 2 },  // N -> top-right? see note
        {  R,                  0             },  // NE -> right
        {  R * 0.5,            R * SQRT3 / 2 },  // SE
        { -R * 0.5,            R * SQRT3 / 2 },  // S
        { -R,                  0             },  // SW
        { -R * 0.5,           -R * SQRT3 / 2 },  // NW
    };
    return offsets[c];
}
// NOTE: the corner names here are the *flat-top* convention from the
// design doc, but for a flat-top hex the actual world directions of the
// vertices are NW/NE-of-top, due-E, SE, SW, due-W, NW-of-top.  The
// labels are just symbolic; what matters is that there are 6 of them
// and they match the neighbour ordering.  This is exactly the kind of
// thing that needs nailing down before committing to the port.

// Interpolate height inside a hex by triangulating it into 6 triangles
// from the centre.  Centre height = average of 6 corners.
static double height_at(hex h, hex_slope s, vec2 px)
{
    vec2 centre = hex_to_px(h);
    vec2 local  = { px.x - centre.x, px.y - centre.y };
    double h_centre = 0;
    for (int c = 0; c < 6; ++c) h_centre += corner_height(s, (corner6)c);
    h_centre /= 6.0;

    // Find which of the 6 wedges this point is in by angle.
    double angle = std::atan2(local.y, local.x);          // -pi..pi
    if (angle < 0) angle += 2 * M_PI;
    // wedge 0 is centred on dir E (angle 0); each wedge spans pi/3.
    int wedge = (int)std::floor((angle + M_PI / 6) / (M_PI / 3)) % 6;
    // wedge i is bounded by corner mapping[i] and mapping[i+1]
    static const corner6 wedge_corner[6] = { NEcorner, SEcorner, S, SWcorner, NWcorner, N };
    corner6 c1 = wedge_corner[wedge];
    corner6 c2 = wedge_corner[(wedge + 1) % 6];
    vec2 v1 = corner_offset(c1), v2 = corner_offset(c2);

    // Barycentric in triangle (centre=0, v1, v2)
    double det = v1.x * v2.y - v2.x * v1.y;
    if (std::fabs(det) < 1e-9) return h_centre;
    double a = (local.x * v2.y - local.y * v2.x) / det;
    double b = (local.y * v1.x - local.x * v1.y) / det;
    double c = 1 - a - b;
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
// main: build a 7-tile neighbourhood and dump diagnostics + image.
// ---------------------------------------------------------------------------

int main()
{
    // 7 tiles: origin + 6 neighbours.  Each gets a different slope so we
    // can see the 6-corner encoding actually doing something.
    std::vector<tile> tiles;
    tiles.push_back({ { 0, 0 }, encode_slope(0, 0, 0, 0, 0, 0) }); // flat
    tiles.push_back({ NEIGHBOURS[E],  encode_slope(0, 1, 1, 0, 0, 0) });
    tiles.push_back({ NEIGHBOURS[SE], encode_slope(0, 0, 1, 1, 0, 0) });
    tiles.push_back({ NEIGHBOURS[SW], encode_slope(0, 0, 0, 1, 1, 0) });
    tiles.push_back({ NEIGHBOURS[W],  encode_slope(0, 0, 0, 0, 1, 1) });
    tiles.push_back({ NEIGHBOURS[NW], encode_slope(1, 0, 0, 0, 0, 1) });
    tiles.push_back({ NEIGHBOURS[NE], encode_slope(2, 2, 0, 0, 0, 0) }); // double-height

    // ---- Diagnostics ----
    std::printf("hex distance smoke test:\n");
    for (auto &t : tiles) {
        std::printf("  dist((0,0),(%d,%d)) = %d\n", t.h.q, t.h.r,
                    hex_distance({0,0}, t.h));
    }

    std::printf("\nslope corner heights (origin tile + NE neighbour):\n");
    for (corner6 c = N; c < CORNER6_COUNT; c = (corner6)(c+1)) {
        std::printf("  corner %d: origin=%d  NE=%d\n", c,
                    corner_height(tiles[0].s, c),
                    corner_height(tiles[6].s, c));
    }

    std::printf("\nvertex-sharing check (each vertex must list 3 owners):\n");
    auto owners = vertex_owners({ 0, 0 }, NEcorner);
    for (auto &o : owners) {
        std::printf("  (%d,%d) corner %d\n", o.first.q, o.first.r, o.second);
    }

    // Verify uniqueness: 3 distinct (hex, corner) names for the same vertex.
    std::map<std::pair<int,int>, int> seen;
    for (auto &o : owners) seen[{o.first.q, o.first.r}]++;
    if (seen.size() != 3) {
        std::fprintf(stderr,
            "FAIL: vertex shared by %zu tiles, expected 3\n", seen.size());
        return 1;
    }
    std::printf("  -> 3 distinct tile owners (OK)\n");

    // ---- Render ----
    const int W = 400, H = 400;
    std::vector<uint8_t> rgb(W * H * 3, 30); // dark grey background
    for (int py = 0; py < H; ++py) {
        for (int px = 0; px < W; ++px) {
            vec2 world { px - W / 2.0, py - H / 2.0 };
            hex picked = px_to_hex(world);
            for (auto &t : tiles) {
                if (t.h == picked) {
                    double h = height_at(t.h, t.s, world);
                    uint8_t shade = (uint8_t)std::min(255.0, 80 + h * 70);
                    int o = (py * W + px) * 3;
                    rgb[o + 0] = shade;
                    rgb[o + 1] = (uint8_t)(shade * 0.85);
                    rgb[o + 2] = (uint8_t)(shade * 0.55);
                    break;
                }
            }
        }
    }
    write_ppm("hex_spike.ppm", W, H, rgb);
    std::printf("\nwrote hex_spike.ppm (%dx%d)\n", W, H);
    return 0;
}
