# City growth anisotropy

Measured state of the hex city-growth shape,
moved here from `TODO.md` once its fixes landed;
`tools/city_anisotropy.py` (see AGENTS.md) reproduces the numbers.

City footprints come out elongated rather than round.
`tools/city_anisotropy.py` measures it: `--run` generates a flat empty
map (via the `-flatmap` headless hook), grid-seeds cities, grows each,
and dumps their building tiles; it maps the axial `(q, r)` through the
flat-top basis and reports the gyration tensor in both the axial and
physical frame plus the m=2/m=4/m=6 angular modes.  Reporting both
frames is the point — physical elongation with a round axial blob means
the basis shear, physical elongation with an elongated axial blob means
the growth algorithm itself biases a direction.  The tool's
`--self-test` validates the math (an axial-isotropic disk reads as
physical aspect sqrt3 at bearing 60 deg).

The gross elongation is fixed.  Pre-fix baseline (flat 192x192, 25
cities spaced 40 to stay clear of each other): physical aspect 1.68
along ~60°.  Two fixes landed: `bewerte_pos`/`bewerte_loc` widened from
4 to 6 rotations, and `build()`'s candidate-start sampling confined to
the physical disk inscribed in the `[lo,ur]` box-rhombus (radius^2 =
3*min(dx,dy)^2/16 via `koord.h`'s `physical_distance_sq`, the Euclidean
metric, not the hex-step `koord_distance`).  Physical aspect is now
~1.16 (round); axial rose to ~1.67, the footprint deliberately
axial-elongated to cancel the basis shear.  Two caveats when re-running:
the disk confinement packs cities denser (~100 tiles vs ~130 — revisit
only if a pakset looks cramped), and the aspect number is only honest at
spacing that keeps neighbours from colliding (spacing 30 reads ~2.3 from
competition for ground).

The per-city m=2 angular-density mode (not a hex symmetry) is the only
residual the gyration aspect can't see, and it is now understood, not
pending.  It used to be elevated in small cities (~0.3-0.4 below ~50
tiles) by the founding skeleton: pak64's 1-layout townhall always seeded
its road South (bearings {30°,90°} — a 2-fold set, exactly what m=2
reads).  Seeding the founding road isotropically over all 6 hex faces
(`check_bau_townhall`) fixed that — `--series` now reads m=2 ~0.18 for
small cities rising to ~0.32 for large (Pearson r = +0.26), i.e. the
remaining m=2 is intrinsic street-layout texture (houses lining up along
straight streets, read from building-tile coords) that grows with the
street network, not a removable bias.  Both tile-placement layers are
isotropic anyway (rule matching is 6-fold symmetric; `physical_distance_sq`
= q^2+qr+r^2 makes the `build()` sampling disk a true physical circle).
Don't chase it further: symmetrising the cityrules to full dihedral D6
was measured to do nothing, and the multi-tile-townhall NE/SW orientation
AGENTS.md flags is a footprint-model change, not an m=2 lever.
