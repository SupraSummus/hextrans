# Hex-Grid Port — Design Plan

Status: **draft / exploratory**. Nothing in this document has been implemented.
The goal is to capture the scope of converting Simutrans from its current
square-tile isometric grid to a hexagonal grid, and to record the key design
decisions that have to be made up front.

---

## 1. Current grid — what we are replacing

Simutrans today is built around a square grid, drawn in a 2:1 isometric
projection ("diamond" tiles).

Key load-bearing structures:

| Concept | File | Notes |
|---|---|---|
| 2D coord | `src/simutrans/dataobj/koord.h:20` | `sint16 x, y` |
| 3D coord | `src/simutrans/dataobj/koord3d.h:19` | adds `sint8 z` |
| 4 cardinal neighbours | `src/simutrans/dataobj/koord.h:95` | `koord::nesw[4]` |
| 8 neighbours (incl. diagonals) | `src/simutrans/dataobj/koord.h:97` | `koord::neighbours[8]` |
| Direction / connection bitfield | `src/simutrans/dataobj/ribi.h:170` | `ribi_t`, 4 bits: N/E/S/W |
| Slope (4 corners × 3 heights) | `src/simutrans/dataobj/ribi.h:21` | `slope_t`, 81 values |
| Per-(x,y) tile stack | `src/simutrans/world/simplan.h:40` | `planquadrat_t` |
| Ground at a height | `src/simutrans/ground/grund.h:92` | `grund_t` + subclasses |
| Flat world array | `src/simutrans/world/surface.h:74` | `planquadrat_t* plan` |
| Tile access | `src/simutrans/world/surface.h:175` | `plan[x + y*width]` |
| Isometric projection | `src/simutrans/display/viewport.cc` | `(x-y, x+y)` diamond |
| A* / neighbour iteration | `src/simutrans/builder/wegbauer.cc` | loops over `neighbours[8]` |

Everything else (pathfinding, terraformer, city growth, way-building,
back-wall rendering for slopes, save-file format, ribi lookup tables) is
downstream of the above.

---

## 2. Hex geometry — the two decisions

### 2.1  Orientation: flat-top vs pointy-top

Both are viable. **Recommendation: flat-top.**

- Flat-top hex has "east" and "west" neighbours along the same row, which
  preserves the feel of building east-west rail lines and means the longer
  horizontal axis of a typical monitor is better used.
- Pointy-top would rotate that 30° and give N/S neighbours instead — more
  awkward for a city-builder that historically emphasises horizontal
  skylines.

### 2.2  Storage coordinates

Use **axial** `(q, r)` coordinates internally, stored in the existing
`koord` struct (rename the fields or just alias `x→q`, `y→r`). Convert to
**cube** `(x, y, z)` with `x+y+z=0` only inside algorithms that need it
(distance, rotation, line-draw). This keeps `planquadrat_t* plan` as a flat
`q + r*width` array with no change to save-file layout.

Hex distance in cube space:
```
dist = (|dx| + |dy| + |dz|) / 2
```
This replaces `koord_distance` (`koord.h:105`) and `shortest_distance`
(`koord.h:111`). Both are hot paths — inline carefully.

---

## 3. **Vertical stacking — FCC vs hex right prism**

> This is the question you asked about explicitly. Recommendation below, then
> the reasoning.

**Recommendation: hexagonal right prism. Do NOT use FCC / ABAB close
packing.**

### 3.1  Hex right prism (recommended)

Every hex column at axial `(q, r)` has a straight vertical stack of cells
at `z = 0, 1, 2, …`. Neighbours of a cell `(q, r, z)`:

- 6 in the same layer (the hex neighbours)
- 1 directly above `(q, r, z+1)`
- 1 directly below `(q, r, z-1)`

Total: **8 neighbours in 3D**, which happens to match the current count
(4 cardinal + 4 diagonals → 6 hex + up + down), though the semantics are
different.

Maps 1:1 onto the existing architecture:

- `planquadrat_t` stays a stack keyed by height (`simplan.h:40`) — no
  change to the container contract.
- `grund_t::pos.z` (`grund.h:92`) keeps its meaning.
- Tunnels, bridges, elevated ways, multi-level buildings, underground
  stations — all keep working because "up" is a single well-defined
  direction.
- Slope / terraforming semantics (a tile has a floor and a ceiling) are
  preserved; we only need to go from 4 corners to 6 corners (see §4).

### 3.2  FCC / close-packed layers (rejected)

Face-centred cubic close packing
([Wikipedia: close-packing of equal spheres](https://en.wikipedia.org/wiki/Close-packing_of_equal_spheres))
alternates hex layers A, B, C where each sits in the "dimples" of the one
below. Each cell then has **12 neighbours** (6 coplanar, 3 above, 3 below).

Why it is tempting:
- Mathematically elegant and genuinely isotropic.
- Natural for voxel/crystal simulations.

Why it is wrong for Simutrans:

1. **Cells are not spheres.** Simutrans stacks represent semantic layers
   — bedrock, ground, tunnel level, bridge level, sky. "Above" is a
   meaningful, singular direction. FCC turns it into a 3-way fork which
   has no game meaning.
2. **Vertical structures break.** A pillar, a building, a tunnel shaft,
   or an elevator goes straight up. In FCC "straight up" isn't a cell
   neighbour at all — you'd zig-zag through A→B→C→A, which is absurd for
   a bridge support.
3. **Save-file / `planquadrat_t` stack is no longer 1-D.** A column
   `(q,r)` would fan out across neighbouring `(q,r)` columns at each
   layer; storage and iteration become global instead of local.
4. **Pathfinding cost explodes.** Branching factor 12 instead of 8;
   heuristic becomes harder; way connectivity (ribi) would need to
   encode 3 up-edges and 3 down-edges per tile.
5. **Rendering.** Layers would be horizontally offset by
   `(hex_width/2, 0)` or similar, so back-wall / slope art has to be
   redrawn per parity. Isometric tile art is already the biggest asset
   cost in the game.
6. **There is no gameplay reason to pay any of these costs.** FCC's only
   advantage is geometric isotropy between layers, which Simutrans does
   not need because layers are not interchangeable.

Summary:

| Criterion | Hex right prism | FCC close-packed |
|---|---|---|
| 3D neighbours | 8 (6 + up + down) | 12 (6 + 3 + 3) |
| "Up" is a single cell | Yes | No |
| Maps onto existing `planquadrat_t` stack | Yes | No |
| Pillars / bridges / tunnels natural | Yes | No |
| Art asset multiplier | ×1 per slope variant | ×2 (layer parity) |
| Physical realism (sphere packing) | Lower | Higher |
| Gameplay benefit of realism | **None** | None |

Keep the right-prism model. Revisit only if we ever add a feature whose
semantics actually want 3-up / 3-down branching (e.g. crystalline
dungeon exploration).

---

## 4. Slopes — the hardest part

Current slope (`ribi.h:21`) encodes 4 corner heights × 3 values = 81
slopes in a single `sint8`. A hex has **6 corners**, so:

- Direct port: 3^6 = **729** slopes — no longer fits in `sint8`.
- Restricted: if only 0/1 per corner are allowed, 2^6 = 64 slopes — fits
  in `sint8` and covers most interesting terrain.

Design decision to make before writing any code: **drop double-height
slopes, or widen `slope_t::type` to `sint16`?** I recommend dropping
doubles for the initial port (they complicate way-building and are
rarely used) and adding them back later if needed.

All the corner macros at `ribi.h:67-72` are now hex-corner macros
(SW/S/SE/NE/N/NW for flat-top). `rotate90` becomes `rotate60`. `slope_t::
flags[81]` becomes a new table sized for the chosen corner model. Grid
vertex storage (`surface.h:85`, `grid_hgts`) also needs reshaping: a hex
has 6 vertices but each vertex is shared between 3 tiles (vs 4 tiles in
the square case), so the vertex array density changes.

---

## 5. Ribi — way connections

`ribi_t` (`ribi.h:170`) packs N/E/S/W into 4 bits, giving 16 combinations
with extensive lookup tables (`backwards[16]`, `dirs[16]`, `doppelr[16]`,
etc.). For hex:

- 6 bits per tile (one per hex-neighbour direction) → 64 combinations.
- Lookup tables grow from 16 to 64 entries.
- "Straight through" semantics (for signals, tram stops, station
  orientation): the three opposite-pairs (N↔S, NE↔SW, NW↔SE for flat-top)
  replace the two pairs (N↔S, E↔W). This is a real gameplay change —
  a hex station can have 3 orientations instead of 2.
- Save-file compatibility: the wire format of `ribi` changes. Bump the
  map save version.

Every place that does `for (int i = 0; i < 4; i++)` over ribi directions
needs to become 6. Grep:
```
grep -rn "ribi_t::" src/simutrans | wc -l
```
gives a rough feel for the blast radius (hundreds of call sites).

---

## 6. Rendering / projection

Flat-top hex in pixel space (for tile width W, height H, with H ≈ W·√3/2
for a regular hex; Simutrans is free to pick a squashed ratio to match
current vertical scale):

```
screen_x = W * (q + r/2)     // but integer: W * q + (W/2) * r
screen_y = (3H/4) * r
```

Inverse for mouse-picking is the standard fractional-axial→cube→round
routine. Both live in `viewport.cc`; today it implements the square
`(x-y, x+y)/2` diamond.

Z continues to push the sprite upward by a constant per level (current
code: `y -= z * TILE_HEIGHT_STEP`), unchanged.

Back-wall / cliff rendering currently assumes 2 walls per tile (SW + SE
faces visible in the isometric view). Flat-top hex seen from the
standard angle has **3 visible lower faces**. New art per slope.

---

## 7. Pathfinding & builders

All routing (`src/simutrans/builder/wegbauer.cc`, `route.cc`,
`terraformer.cc`, `surface.cc`, `grund.cc`) iterates
`koord::neighbours[8]`. Mechanical change: swap to a 6-entry hex
neighbour table. Heuristics that use `shortest_distance` (`koord.h:111`)
must switch to hex distance — under-estimating distance keeps A*
optimal, over-estimating breaks it, so audit each call.

City growth and factory placement use square-shaped search boxes
(`placefinder.h`, `building_placefinder.h`). Hex neighbourhoods are
circular in cube-distance; rewrite the search to iterate hex rings.

---

## 8. Save / pak compatibility

- Bump world save version; old square maps cannot be loaded as hex.
- Provide a one-shot converter (square → hex) if we want to preserve
  user maps; this is not straightforward because a square tile's 4
  corners do not map cleanly to a hex's 6 corners.
- Pak art is the biggest cost: every ground/way/building sprite needs a
  hex counterpart. Plan to ship a single minimal "hex-demo" pakset
  first; don't try to port pak64 or pak128 until the engine is stable.

---

## 9. Suggested phasing

1. **Geometry layer only**, behind a compile-time flag.
   - New `hex_koord`, `hex_slope_t`, `hex_ribi_t`.
   - Unit tests for distance, rotation, neighbour tables, line-draw.
   - No gameplay code touched yet.
2. **World storage**: teach `planquadrat_t` / `surface_t` to use hex
   coords. Still no rendering.
3. **Headless simulation**: load a flat hex map, run the clock, let
   citizens path on foot. No graphics.
4. **Rendering**: hex projection + a placeholder single-colour sprite
   per slope. Back-wall art deferred.
5. **Ways + ribi**: one way type (road), no signals.
6. **Everything else**: signals, stations, bridges, tunnels, cities,
   factories — in whatever order pain dictates.

Realistic scope: low tens of thousands of lines touched, most of the
existing pakset art obsolete. This is a fork-scale project, not a
feature branch.

---

## 10. Open questions

- Do we keep double-height slopes (729 slopes) or drop them (64 slopes)?
- Flat-top confirmed, or prefer pointy-top for some reason?
- Do we need a square→hex map converter, or start from blank maps only?
- Is the target a playable alternate mode, or an experimental fork?
