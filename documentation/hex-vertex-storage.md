# Per-vertex height storage on the hex grid

Design doc for the replacement of `surface_t::grid_hgts`.  Once the
accessor rewrite lands this doc describes live behaviour, not a plan —
the plan part lives in `TODO.md`.

## Why a canonical owner is needed

On the square grid every world vertex belongs to up to 4 tiles as a
named corner (NW of one, NE of its west neighbour, SW of its north-west
neighbour, SE of its north neighbour).  Simutrans resolves the naming
ambiguity by ignoring the tile-corner view entirely for storage: the
height lives in a flat `(W+1) × (H+1)` array of grid points, and each
tile reads its four corners by offsetting into that array.

On a flat-top hex grid every interior world vertex belongs to exactly
3 tiles as a named corner.  We keep the same "one slot per world
vertex" invariant — storing per-tile slopes is not an option, because
terraforming a shared vertex would leave the three neighbours with
inconsistent heights (see `tools/hex_spike/findings.md` §1).  The
question this doc answers is: given a `hex_vertex_t` (a (tile, corner)
pair), how do we pick the one canonical owner out of the three, and
how do we lay out the storage?

## Canonical ownership: lex-min over tile coords

Of the three (tile, corner) names for a world vertex, we pick the one
whose tile is lex-smallest under `(x, y)` ordering.  Working this out
from `vertex_owners()` in `src/simutrans/dataobj/koord.cc` gives a
short table — at any tile `(q, r)`, the corners `E` and `SE` are
always canonical, and the other four corners are just aliases for an
E-or-SE corner of a neighbour:

    corner of (q, r)      canonical (tile, corner)
    ------------------    ------------------------
    E   (angle   0°)      (q,   r),   E
    SE  (angle  60°)      (q,   r),   SE
    SW  (angle 120°)      (q-1, r+1), E
    W   (angle 180°)      (q-1, r),   SE
    NW  (angle 240°)      (q-1, r),   E
    NE  (angle 300°)      (q,   r-1), SE

Geometrically: every world vertex is the east-end of some edge, and
the tile sitting immediately west of that east-end is the one with
the smallest x-coord among the three owners; it owns the vertex as
its E or SE corner depending on whether the edge runs slightly
north-east or slightly south-east.

This particular convention was picked because (a) it matches the
hex-axial `+q` axis pointing 30° south of due-east, so E and SE are
the "downstream" corners under the natural sweep direction, and (b)
it needs no modular arithmetic at runtime — the mapping is a 6-way
switch on the corner enum.  The `vertex_owners()` primitive is still
the source of truth for correctness; `canonical_vertex()` is an
optimised restatement.

## Array layout

Storage is a flat `sint8` array sized `(W+1) * (H+2) * 2`, row-major
to match the legacy `grid_hgts[x + y*(W+1)]` stride convention:

    slot(q, r, c) = ((q+1) + (r+1) * (W+1)) * 2 + (c == SE ? 1 : 0)

The canonical tile coord `(q, r)` ranges over `[-1, W-1] × [-1, H]`.
The `q = -1` column and `r = -1` row are the familiar boundary
padding (N and W map edges).  The `r = H` row is less familiar but
unavoidable: the SW corner of a south-edge tile `(q, H-1)`
canonicalises to `(q-1, H)`, one row past the tile grid.  Flat-top
hex geometry shifts `+r` in the SW direction but never shifts `+q`,
so only the south edge needs the extension.

The total is `2 · (W+1) · (H+2)` slots — a little more than twice
the square grid's `(W+1) · (H+1)`.  Along the transition the even
slots (the E canonical corners of each tile) are still addressable
by the old `grid_hgts[x + y*(W+1)] → slot[2 · (x + y*(W+1))]`
formula: the old `(x, y)` grid point becomes the E corner of tile
`(x-1, y-1)`, and that mapping is injective, so the legacy API
continues to route each `(x, y)` to a consistent slot.

## Using the helpers

`canonical_vertex(v)` takes any of the three `hex_vertex_t` names and
returns the canonical one.  `vertex_slot_index(v, W, H)` takes a
canonical vertex and the map size and returns the flat array offset.
`vertex_slot_count(W, H)` returns the total slot count, for
allocation.  All three are pure functions and live in
`src/simutrans/dataobj/koord.{h,cc}` alongside `vertex_owners`.

Call sites that currently reach into `grid_hgts` by grid-point `(x,
y)` will migrate to `get_hoehe(koord tile, hex_corner_t::type c)` —
that rewrite is tracked separately in `TODO.md` under "Per-vertex
height storage".  This doc covers only the pure topology primitives
underneath it.
