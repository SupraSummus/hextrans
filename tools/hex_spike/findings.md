# Hex spike — findings

A 30-minute spike to surface the geometry questions before committing
to the hex-grid port plan. See `hex_spike.cc` for the code,
`hex_spike.png` for the render.

```
make run
```

## What the spike confirmed

- **Axial coords + 6-neighbour table work as advertised.** All 6
  neighbours of `(0,0)` are at hex-distance 1.
- **6-corner slope encoding fits in `uint16_t`** (3⁶ = 729 < 65536).
  Round-trip per-corner read works.
- **Hex→pixel projection has a clean inverse.** The renderer in the
  spike rasterises by per-pixel inverse projection — i.e. the same code
  path you would use for mouse-picking.
- **Vertex sharing checks out: each hex vertex is owned by exactly 3
  tiles.** The spike enumerates the 3 (tile, corner) names for one
  vertex and asserts they map to 3 distinct tiles.

## What the spike surprised us with

### 1. Per-tile slope storage cannot be the source of truth

The render shows visible **discontinuities at shared vertices**: the
"NE corner of origin" and the "S corner of (1,0)" are the same world
vertex, but the spike stores slopes per-tile and lets each tile pick
its own corner heights. The shading therefore has step-changes along
shared edges.

This isn't a rendering bug — it's a *data-model* finding. In
square-grid Simutrans this is solved by storing vertex heights in
`surface_t::grid_hgts` (one height per (x+1, y+1) grid point) and
**deriving** per-tile slopes from those heights. The hex port has to
do the same, but harder:

- Vertices in a hex grid are shared by **3 tiles** (vs 4 for square),
  so an editor's "raise this vertex" tool affects 3 tiles, not 4.
- The per-vertex storage is no longer a regular `(x+1) × (y+1)` grid;
  it's a denser irregular topology. Index scheme TBD.
- The slope-from-vertex-heights derivation has 2× more corners per
  tile (6 vs 4) and so 2× more constraints to satisfy when validating
  whether a way / building / industry can sit on a tile.

This was implicit in the design plan but the picture makes it
visceral. **Before any further engine work the per-vertex height
storage and indexing scheme has to be designed first.** That's the
real critical-path item, not slope encoding width.

### 2. Direction labels are surprisingly contentious

A flat-top hex has 6 *vertices* at angles 0°, 60°, … 300° from the
centre. None of them is "north" — flat-top has N and S as **edges**,
not corners. The spike intentionally uses the misleading labels
`N, NE, SE, S, SW, NW` for corners (so you can see how easy it is to
slip up); the actual corners are `E, SE, SW, W, NW, NE`. Likewise
the 6 *edge* directions for ribi are `NE, E, SE, SW, W, NW` (no N or
S edges in flat-top).

This sounds like a bikeshed but it's not — every existing site in the
Simutrans codebase that says `north` / `nesw` will have to be ported
to a new naming, and getting it wrong gives you a 30°-off grid that
compiles fine. **Pin the naming convention in a header before
touching anything else.**

### 3. Rendering by inverse projection is fast enough to consider

The spike rasterises 7 hexes by checking every pixel of a 400×400
image — `O(W·H)` instead of `O(tiles · pixels_per_tile)`. The full
160k-pixel render takes single-digit milliseconds on this machine.
For a real engine you'd still want polygon rasterisation, but for
spikes / tools / map preview, inverse projection is dramatically
simpler and good enough.

## What the spike deliberately did NOT prove

- **Isometric projection.** The spike renders top-down. The current
  game renders 2:1 isometric "diamond" tiles, and porting that to
  flat-top hexes is its own non-trivial geometry exercise (and an
  art-pipeline question).
- **Way / ribi connections.** No edges are drawn. The 6-bit ribi
  encoding is mechanical; the open question is signal / station
  orientation semantics (3 axes vs 2).
- **Z-stacking.** Single layer only. The plan's right-prism stacking
  is trivial in geometry; nothing here exercises it.
- **Performance.** A 7-tile spike says nothing about whether the
  inverse-projection rasteriser scales to a 4096×4096 game map.

## Recommended next spike

If the hex port is going to continue, the next spike should be:

> **Per-vertex height storage + derived slopes for a 5×5 hex region.**
> Implement an `unraise_vertex` / `raise_vertex` editor and prove that
> the resulting per-tile slopes are always self-consistent (no
> visible discontinuities). This is the data-model question the plan
> document under-cooked.

Estimated effort: similar to this spike (a few hundred lines, one
afternoon). After that the next decision gate is whether to keep
investing or to step back.
