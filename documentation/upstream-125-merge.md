# Upstream 125 merge plan: `upstream/master` into `origin/main`

Planning doc for the staged merge of upstream Simutrans 125
(327 commits, `a60b27cd4`..`3e9f4f5e5`).
Delete it when stage 5 has landed.

Status:
stage 1 (#1–#10) landed in PR #291,
together with the city population ledger commits
from the abandoned `claude/admiring-davinci-l0cr7t` branch (section 6);
stage 2 (#11–#93) in PR #300, after reverting the include sweep `28f4ae97a`;
stage 3 (#94–#183) in PR #305.
Stages 4 and 5 are open; issue #296 tracks them.
The merge commits record each stage's resolutions.
The reconnaissance below predates stage 1
and was written against `origin/main` `ab4b677df`.

## Reconnaissance

Basis: origin/main `ab4b677df` (2026-10-11), upstream/master `3e9f4f5e5` (2026-10-07), merge base `a60b27cd4` (= origin/simutrans tip, 2026-06-08). I did not fetch. The primary checkout's local `main` has since advanced (other agents); everything below is against `origin/main`.

Method:
- `git merge-tree --write-tree` for the single-shot merge, and for every one of the 327 prefixes (to date each conflict).
- A simulated 5-stage merge, taking the fork side on conflicts.
- A trial merge in a scratch worktree, then `g++ -fsyntax-only` (flags from `build/CMakeFiles/simutrans.dir/flags.make`) over 387 translation units. Baseline origin/main compiles all but 8 of 381 (makeobj-only writers and platform sound files, setup artifacts).
- Scratch worktrees are removed. No build, link or runtime test was run.

Notation: `#n` is the 1-based position in `git log --reverse origin/simutrans..upstream/master`. The upstream range is linear, with no merge commits.

### 1. Summary

**Counts**
- 327 commits, 2026-06-09 to 2026-10-07, including the 125.0 and 125.0.1 tags.
- 631 files changed (+72,990/−9,660): 243 under `simutrans/text`, 291 under `src` (excluding text), 25 under `tests`.
- 78 files added, 3 deleted (`dk.tab`, `gr.tab`, `no.tab`), 86 renamed (`text/{cz→cs}`, `{dk→da}`). The fork never touched `simutrans/text`, so those are clean.
- Single-shot merge: **53 conflicted files, 121 hunks, all content conflicts** (no rename or delete conflicts).
- By class: **19 TRIVIAL, 20 KEEP-FORK, 14 HEX-DESIGN.** No file is whole-file TAKE-UPSTREAM; TAKE-UPSTREAM applies to individual hunks.
- Prefix scan: **#1–#6 merge with zero conflicts**. The first conflict arrives with #7.
- Upstream adds 112 test functions in 9 new test scripts, plus the `tests/ai/stlab_probe` fixture.

**Top 5 risks**
1. **Close diagonals** (about 25 commits: #11, #13, #16, #18, #19, #21–24, #31, #83, #87, #88, #91, #200, plus the diagonal half of #223). They assume 4-bit ribi: `ribi_t::all` is now 63, and `northeast`, `northwest`, `southeast` and `southwest` are now single edges.
   - `route.cc` would restrict routing to ONE direction.
   - Upstream's `weg_t::rdwr` flags `ribi == ribi_t::all` as a close diagonal on load, so a hex 6-way junction would be misflagged.
   - The feature rests on parts the fork removed: `check_diagonal`, `is_diagonal`, `set_images`, switch images.
   - Needs an explicit neutralisation design (section 4, stage 2).
2. **API drift that fails to compile or silently changes data in files that merge cleanly** (section 3.1 and 3.2):
   - `double_slopes` is added by both sides. `way_reader.cc` auto-merges and recomputes it from image-list length, overwriting the fork's explicit `has_double_slopes=` key.
   - `bridge_desc.cc` gets a static table using removed enum names.
   - `schiene_t::reserve(c, route_index)` has a new signature (7 call sites).
   - `haltestelle_t::get_owner()` is removed.
   - `karte_t::get_city_road()` is removed.
   - `obj_t::finish_rd()` now returns `bool`.
   - The `image_t` freelist bypasses the fork's `TRACK_DESCRIPTORS` allocation chokepoint.
3. **Way builder.** `wegbauer.cc` has 30 upstream commits against 23 fork commits since the last sync, and conflicts in 4 of 5 stages. Upstream adds a way-builder dialog and script planners (way, tunnel, bridge). The same function bodies are hex-rewritten in the fork: 4-direction loops, `(start.x==ziel.x||start.y==ziel.y)` axis tests, `abs(dx)-abs(dy)` bridge length, corner indexing by `koord::north/east`.
4. **Tests.** Upstream's added test lines contain 58 `ASSERT_WAY_PATTERN` uses with string rows (the fork's helper tripwires on strings), 46 `slope.south/north/east/west`, and `dir.eastwest/northsouth`. `test_way_road.nut` was converted to CRLF by #159, which causes a whole-file conflict.
5. **Save and version scheme.**
   - Upstream moves to `SIM_VERSION_MAJOR 125` / `SIM_SAVE_MINOR 6` and gates writes on `is_version_atleast(125, 6)`.
   - Version compare is `major*1000+minor` (`loadsave.h:153-155`). The fork's 124.901 saves skip every upstream 125.x field unless the fork moves to MAJOR 125 and keeps minor 901+.
   - Pak side: building node v12 (`height_clearance`) and `MAKEOBJ_VERSION 60.12`. The fork's way node v9 does not collide.

**Recommended staging** (cut commits; effort is my estimate from conflict count and size, not measured)

| Stage | Upstream commits | Cut at | Simulated conflicts | Effort |
|---|---|---|---|---|
| 1 | #1–#10 | `a2e8541a8` | 2 | 0.5–1 day |
| 2 | #11–#93 | `315c9cf66` | 18 | 4–6 days |
| 3 | #94–#183 | `bd0e5ded5` | 18 | 4–6 days |
| 4 | #184–#268 | `bc1813b9b` | 18 | 3–5 days |
| 5 | #269–#327 | `3e9f4f5e5` | 11 | 3–5 days |

The simulated total is 67 conflict instances (versus 53 single-shot) because files like `wegbauer.cc`, `simview.cc` and `test_way_road.nut` re-conflict. Each hunk is smaller. The simulation resolves conflicts as fork-wins, so later stages are approximate.

**Abandoned branch `claude/admiring-davinci-l0cr7t`: not reusable as a merge, but 3 of its commits are worth keeping.** It merged only upstream #1–#5 (r12015), which conflict-free against current main anyway (prefix scan). It carries 3 fork-authored commits that are not on main:
- `3611241d8`: city population ledger fix.
- `9b818af87`: script API getters.
- `fe78dd2e7`: test plus `TEST_RES_2x2`.

They are still needed: upstream/master's `build_city_house` leftover-tile loop still lacks the `won -=` / `arb -=` for the old tile (read from upstream `simcity.cc`). Details in section 6.

### 2. Conflicting files (single-shot merge), by subsystem

The fork-change column is `git diff origin/simutrans origin/main`.

#### cmake/ (2)
| File | Upstream | Fork | Class and plan |
|---|---|---|---|
| `SimutransSourceList.cmake` (whole file) | #167 re-indents the list. New sources: `gui_owner`, `music_folder_frame`, `way_builder_frame` (#270), `simsocial`, `sdl3_*`, `api_ai_test`, `api_schedule_route_test` (#113) | +3 lines: `koord_hex.cc`, `way_image_keys.cc`, `way_image_slot.cc` | TRIVIAL: take upstream's file, re-add the 3 lines (and each stage's new sources) |
| `SimutransCompileOptions.cmake` | #192/#193 give Squirrel its own flags; #194/#195 `-Wno-error` for unused-function and cpp; #265 `WINVER`; SDL3_MIXER option | `USE_CURL` and `TRACK_DESCRIPTORS` options; extra Squirrel `-Wno-cast-qual/-implicit-fallthrough/-class-memaccess` | TRIVIAL: take upstream's Squirrel flag move; check the 3 extra suppressions are still covered |

#### builder/ (2)
| File | Upstream | Fork | Class and plan |
|---|---|---|---|
| `wegbauer.cc` (7 hunks) | #94 elevated ways on bridges; #105/#106/#139/#154 bridge search for AI and scripts; #133–#141 way/tunnel planner API; #159 `terraforming_leaves_way_slopes()`; #184 construction info; #271/#274/#280/#308/#309/#317/#320/#323 way-builder tool (auto bridges and tunnels, preview, `way_count_bridge`) | +187/−135: hex slope admission (`slope_allows_way_axis`, `compute_test_dir`), 6-direction loop, per-edge `check_terraforming`, `min_hgt` instead of the tripwired `lookup_hgt` | KEEP-FORK plus HEX-DESIGN. Re-apply by hand: `check_slope` rejects double-height slopes for ways without `has_double_slopes`; the `check_terraforming` overload and replay; `check_for_bridges` / `start_ziel_bridge_len` with `koord_distance`; the terraform-axis test over all 6 axes; the `must_build_bridge` / `build_tunnel_bridge` flags. Drop the `has_diagonal_image()` post-build recalcs in `build_tunnel_tile`, `build_elevated`, `build_road`, `build_track`. Keep the fork's `welt->min_hgt(to_pos)` line (upstream's hunk calls the fatal `lookup_hgt(koord)`) |
| `wegbauer.h` | the new `check_terraforming` overload; `terraforming_leaves_way_slopes()` | out-params are `slope_t::type*` (sint16 slopes) | KEEP-FORK: add the upstream declarations with `slope_t::type*` |

#### dataobj/ (4)
| File | Upstream | Fork | Class and plan |
|---|---|---|---|
| `ribi.h` (1 hunk) | #32 adds `slope_t::is_way_double(x, allow_double)` | +701/−153: 6-bit ribi, base-4 slopes, chord rules | KEEP-FORK: add `is_way_double` in hex terms, e.g. `is_way(x) && (allow_double \|\| !has_double_corner(x))`. 7 callers in `simcity.cc` and `wegbauer.cc` |
| `route.cc` (2) | #11 close-diagonal restriction in `find_route` and `intern_calc_route` | 6-direction loops | HEX-DESIGN: keep the fork side. Do not adopt `check_close_diagonals` |
| `settings.h` (1) | #271/#273 `way_count_tunnel` 16→12, new `way_count_bridge`, `set_river_number` setter | same setter, different parameter name | TRIVIAL: one setter. Pathfinding cost defaults change, which may shift expected routes in route-pinning tests (not verified) |
| `pakset_downloader.cc` (2) | #242 http→https redirect via `download_https`; #243 no shell parsing on Windows | libcurl path (`network_curl_download_url`, `parse_http_url`), zip-slip guard | KEEP-FORK: libcurl path stays. Take upstream's `download_https` / `is_safe_https_url` only inside the legacy `#else` fallback, or retire the fallback per `documentation/libcurl-port.md` |

#### descriptor/ (8)
| File | Upstream | Fork | Class and plan |
|---|---|---|---|
| `ground_desc.cc` (1, 139 lines) | #119/#224/#228 `COLOUR_DEPTH==0` early-outs | deleted `create_alpha_tile` (hex rewrite) | KEEP-FORK: drop upstream's edit to the deleted function. Place the `COLOUR_DEPTH==0` early `return` in `init_ground_textures()` AFTER the `doubleslope_to_imgnr[]` setup (see 3.2) |
| `obj_desc.h` (2) | #103 removes the ctor, `children = NULL` default member init | `nchildren` with bounds-checked `get_child`, virtual dtor under `TRACK_DESCRIPTORS`, allocation tracking | TRIVIAL: keep the fork's members with default member initialisers (`= NULL`, `= 0`) |
| `reader/image_reader.cc` (3) | #120/#224 `COLOUR_DEPTH==0` discards pixels (`img_len`, `goto adjust_image`) | alloc caps against node size, static `images_adlers` plus `clear_dedup_cache()`, placeholder realloc | KEEP-FORK: re-apply the depth-0 path around the fork's caps. Re-conflicts in stage 4 |
| `way_desc.h` (3) | #11 `close_diagonals`, `diagonals`, `double_slopes`, `switches` flags computed in `register_obj`; `front_image_lists` vs `front_images`; new `assert(is_bend)` | explicit `double_slopes` key (way node v9), 64-slot ribi lists, 54-slot slope lists, half-slope lookup, removed switch/diagonal API | HEX-DESIGN: keep the fork's flag and API. The `front_image_lists` / `front_images` split is a genuine fix worth taking, adapted to 64-slot lists |
| `way_obj_desc.h` (1) | #21 close-diagonal image accessors | −28 lines of diagonal/switch API | HEX-DESIGN: keep the fork side |
| `writer/image_writer.cc` (1) | #95 `#elif IMG_VERSION == 1 \|\| IMG_VERSION == 2`; #225 makeobj no longer uses `image_t` as scratch | `#elif defined(IMG_VERSION2)` | TRIVIAL: take upstream |
| `writer/way_obj_writer.cc` (1) | #21–#23 `frontdiagonal[close1/2]` keys | drops an unused `int slope` | TRIVIAL (remove the unused variable); skip the close keys |
| `writer/way_writer.cc` (1) | #11 `diagonal[close1/2]` keys | 64-slot ribi key table, 54 slope keys (+46/−135) | HEX-DESIGN: keep the fork's writer, skip the close keys |

#### display/ (4)
| File | Upstream | Fork | Class and plan |
|---|---|---|---|
| `simgraph0.cc` | #101 `take_screenshot(const scr_rect&, const char*)`; #326 zoom | `draw_signal_direction(..., sint16 slope)` | TRIVIAL: fork's `draw_signal_direction` plus upstream's `take_screenshot` signature |
| `simgraph16.cc` | the same, plus include moves | hex poly-clip bits, wedge-based `draw_signal_direction` | TRIVIAL: same two-line resolution, take upstream's include block |
| `simview.cc` | #109/#110/#203 schedule-route overlay (+304) | `hex_proj` based projection | HEX-DESIGN: the overlay is wired to square iso (`u=di−dj`, `v=di+dj`, `diagonal_run_end` "staircase of orthogonal steps", centre `OBJECT_OFFSET_STEPS*2/3`). The data layer (`karte_t::request_schedule_route`, `get_schedule_route`) is grid-neutral. Take the data layer, stub or redo the drawing |
| `viewport.cc` | #10 removes a `;;` | hex inverse projection | KEEP-FORK (trivial) |

#### gui/ (3), network/ (1), player/ (1), script/api/ (2)
| File | Upstream | Fork | Class and plan |
|---|---|---|---|
| `gui_waytype_tab_panel.cc` | #284 | includes pruned | TRIVIAL: add `simworld.h` |
| `factorylist_frame.cc` | #262 | include pruned | TRIVIAL: includes |
| `minimap.cc` (2) | #71 water/fundament/field restructure, #93 | hex isometric mapping; `gr->get_hoehe()+height` | KEEP-FORK: upstream's hunk calls fatal `world->lookup_hgt(koord)`, so keep the fork's height line. Re-apply field and fundament colouring, add `obj/field.h` |
| `network_file_transfer.cc` (1) | #242 `network_http_get_file(..., redirect_to)` | libcurl path (+217) | KEEP-FORK: put upstream's wrapper inside the legacy `#else` |
| `ai_passenger.cc` (1) | #25 `get_owner()` → `get_first_owner()` | hex airport edits | KEEP-FORK: the fork's line uses a removed API (compile error); apply the rename |
| `api_command.cc` (3) | #139 `max_length` for `build_bridge_at`; #140 optional `terraform` on `build_way` / `build_road`; docs | `-koord(slope)` bridge end search | KEEP-FORK: take #140 (it is the `terraform=` flag the fork's TODO asks for) and `max_length`, keep the fork's slope conversion |
| `changelog.h` | 11 doc commits | 3 lines | TRIVIAL: union |

#### obj/ (9), vehicle/ (3), world/, tool/, root
| File | Upstream | Fork | Class and plan |
|---|---|---|---|
| `way/weg.cc` (7), `weg.h` (4) | #11 etc.: flags byte becomes bitfields; `close_diagonal_state`, `switch_state`; `check_diagonal` extended; `weg_t::display`; `finish_rd()→bool` (#17) | `way_image_slot_t` rewrite; removed `check_diagonal`, `is_diagonal`, `set_images`, `has_switched`; `ribi` and `ribi_maske` widened to bytes; rdwr mask `& ribi_t::all` | HEX-DESIGN: keep the fork. Take only `finish_rd` as `bool`. `close_diagonal_state` must stay 0 |
| `way/schiene.cc` (2), `schiene.h` | #31 `reserve(c, route_index)`, `reserved2`, `enter/enter2`, `rotate90`, `display` | one-arg `reserve(c)`, no switch-image update | HEX-DESIGN. Decide the signature once (see stage 2). 7 call sites in `air_vehicle`, `rail_vehicle`, `simconvoi` |
| `wayobj.cc` (7), `wayobj.h` | #21/#91 close-diagonal logic and `calc_cached_image()`; #17 `finish_rd`; #25 | removed `diagonal`, `find_next_ribi`; 6-edge loops; slope chord | HEX-DESIGN: keep the fork. Take the non-diagonal bits |
| `bruecke.cc` (3) | #280 moves `single_img[24]` to `bridge_desc_t`; #17 | `single_height_of()` over the 36-entry `img_t` | KEEP-FORK: drop upstream's `single_img` (also in `bridge_desc.cc/.h`, see 3.1) |
| `roadsign.cc` (4), `signal.cc` (4) | #223 `get_image_id(idx, diag, left_variant)`, `calc_diagonal_index`, front and drive-on-left lists | `ribi_t::southeast/northwest`, traffic-light FSM | KEEP-FORK plus HEX-DESIGN: re-apply the `left_variant` / front-image plumbing; `diag[]` stays `NO_DIAGONAL` |
| `vehicle/air_vehicle.cc`, `rail_vehicle.cc` | #31 `reserve(cnv, i)` | one-arg `reserve` | TRIVIAL once the signature is decided |
| `vehicle/simroadtraffic.cc` | #17 `finish_rd`; `DESTINATION_CITYCARS` loop | 6-direction loops | KEEP-FORK |
| `simconvoi.cc` (4) | #7 skips airplanes in the load-time reserve; #31 `reserve(self, idx)`; #28/#37 | one-arg `reserve` | KEEP-FORK: re-apply the `air_wt` skip and the signature |
| `world/simcity.cc` (2) | #310 asymmetric townhall offset fix | hex townhall founding (`layout_to_ribi[layout%6]`, `is_single` padding) | KEEP-FORK: re-apply the intent of #310 (building offset for asymmetric townhalls) |
| `tool/simtool.cc` (3) | #266 slope tool checks fundament neighbours; #75/#18 close-diagonal preview; #13 rotate-switch-graphics tool branch | 6-neighbour loop, hex slope images | KEEP-FORK for the 6-loop plus #266 (include `grund_t::fundament`). Drop the diagonal preview and the switch rotation |
| `simmain.cc`, `simsound.cc`, `simversion.h`, `tpl/freelist_tpl.h` | #318/#319 prefs; `set_river_number(1)` (default map); version 125.0.2 | `-flatmap` hook; fork save range | TRIVIAL (keep `set_river_number(1)` before the `-flatmap` block). `simversion.h` is a design decision, see 3.3 |

#### tests/ and tools/ (7)
| File | Upstream | Fork | Class and plan |
|---|---|---|---|
| `all_tests.nut` (5) | 112 new functions, 9 new includes | PENDING-tag convention | TRIVIAL union; add new tests commented `HEX-PORT PENDING` until migrated |
| `test_way_road.nut` (whole file) | #159 converted the file to CRLF; real changes +283/−6 (`git diff --ignore-cr-at-eol`) | hex-migrated patterns | TRIVIAL: strip CR from upstream's side, then 3-way merge. `test_way_planner.nut` is also CRLF |
| `test_scenario.nut` (6) | scenario refusal text now reaches the caller: expected `"Foo Bar"` instead of `""` | hex patterns | KEEP-FORK: take the new expected strings |
| `test_halt.nut`, `test_way_bridge.nut` (+490 upstream) | new shared-halt and bridge tests | hex helpers (`lower_hex_tile`) | KEEP-FORK; import the new tests as PENDING |
| `test_transport.nut` | #178 drops a duplicated half | already rewritten | TRIVIAL: take the fork |
| `tools/run-automated-tests.sh` (2) | #258 `EXPECT_SCRIPT_ERROR` classification after the run | `SIM_FAST`, `FATAL ERROR` match | KEEP-FORK: keep both. `tools/test.py` calls this script |

### 3. Silent and non-textual conflicts

### 3.1 Compile breaks in cleanly merged code (found by the trial compile)

I resolved conflicts as "ours" and reverted the close-diagonal cluster to the fork's versions plus 5 `weg_t` stubs. The first trial's cascade (`weg.h`, `way_desc.h`) was an artifact of that resolution. Line numbers refer to the trial tree.

| Where | What fails | Cause |
|---|---|---|
| `descriptor/bridge_desc.cc` (clean) | `single_img[24]` uses `OW_Segment`, `O_Start`, `W_Start`, `O_Ramp`, `W_Ramp`, `OW_Pillar` | #280 moved the table; the fork re-laid the `img_t` enum. Also used by the `simtool.cc` bridge preview (`bridge_desc_t::single_img[img]`) |
| `obj/roadsign.cc`, `obj/signal.cc` | `ribi_t::west`, `ribi_t::east` (new `calc_diagonal_index`, single-direction front-image code), `weg_t::is_diagonal()` | #223 |
| `ground/grund.cc` (clean), `tool/simtool.cc` | `wayobj_t::calc_cached_image()` | #21/#91 |
| `world/simcity.cc` | `slope_t::is_way_double` (7 sites); `welt->get_city_road()` in two fork-added lines; missing `#include "../ground/boden.h"` | #32 moved `city_road` to static `stadt_t::city_road`; the fork's include sweep dropped `boden.h` (the abandoned merge re-added it) |
| `player/ai_passenger.cc:866` | `haltestelle_t::get_owner()` | #25 owners bitmask |
| `builder/wegbauer.cc` | `slope_t::is_single` (fork name `is_axis_slope`); `has_diagonal_image()` x4; `check_terraforming` overload; `terraforming_leaves_way_slopes` | #159, #88 |
| `obj/way/weg.h`, `obj/wayobj.h` if the fork side is kept | `finish_rd()` return type | #17 changed `obj_t::finish_rd()` to `bool` |
| `descriptor/way_desc.h` | duplicate member `double_slopes` | both sides added it |
| air/rail vehicle, `simconvoi.cc` | `schiene_t::reserve` arity | #31 |

Also seen by grep in upstream-added lines but not flagged by the trial compile (not verified why):
- `ribi_t::northsouth` / `eastwest` at upstream `simcity.cc:3695–3696`.
- `ribi_t::rotate90(dir)` at `wayobj.cc:251`.
- `schiene_t::rotate90` (new override).
- `koord::nesw[i]` at `simtool.cc:3174`.
- 4-iteration loops at `roadsign.cc:248`, `weg.cc:680`, `simtool.cc:3172`, `simroadtraffic.cc:688`.

### 3.2 Merges (and may compile) but wrong under hex

1. **`way_reader.cc` auto-merges.** `register_obj` recomputes `double_slopes` from `get_count() > 4` and `switches` from `get_count() > 16`. Read order is `read_node` then `register_obj`, so the computed value overwrites the explicit v9 `has_double_slopes=` key. The fork's 54-slot slope lists and 64-slot ribi lists make both tests always true. `test_tunnel_double` semantics break. `desc->diagonals` calls `get_diagonal_image_id(ribi_t::northeast,…)`, and upstream's new `assert(ribi_t::is_bend(ribi))` would fire in a Debug/ASAN CI build.
2. **Close-diagonal routing and reservation** (`route.cc`, `schiene.cc`, `weg.cc`, `wayobj.cc`). They use `ribi_t::all == 15`-style tests and composite `northeast`, `southeast` (`route.cc` x4, `schiene.cc` x6, `weg.cc` x3, `wayobj.cc` x4), and `weg_t::rdwr` sets `close_diagonal_state = 1` when `ribi == ribi_t::all`.
3. **`ribi_t::all` as "4-way"** also appears in `wegbauer.cc` (straight_dir logic), `air_vehicle.cc:181`, `simtool.cc` preview code, `wayobj.h:85`.
4. **`ground_desc.cc`:** upstream's `COLOUR_DEPTH==0` early `return` in `init_ground_textures()` skips the `doubleslope_to_imgnr[]` setup that `get_climate_tile` reads at runtime in the fork (`ground_desc.cc:628-631`). Effect in headless tests not verified.
5. **`image_t` freelist (#224).** `image_t::operator new/delete` (freelist) shadows `obj_desc_t::operator new`, so the fork's `obj_desc_track_new()` is bypassed. `free_all_descriptors()` and `fuzz_pak` teardown no longer see image nodes, and their `data` buffers leak. `COLOUR_DEPTH==0` also gives `image_t` a different class. Not compiled with the fuzz harness (not verified).
6. **`roadsign_t::set_dir` rework** (#223 area): one-way signs and signals on tiles with `is_threeway(ribi)` (3 or more bits, so hex Y-junctions) stop masking, and single-way signs are removed. Check against the fork's 3-axis traffic-light FSM (not verified).
7. **#232** (validate way direction for stations with explicit layouts) in `simtool.cc` overlaps the fork's `ed7baee97` (one ribi→layout mapping for stops). It auto-merged. Semantics not verified.
8. **Zoom tables** (#326): `ZOOM_NEUTRAL` 3→7, `MAX_ZOOM_FACTOR` 9→13. `hex_proj.h` has no zoom references (grep). Rendering not verified.
9. **`karte_t::rotate90()`:** upstream's `test_schedule_route_rotate` calls it through `api_schedule_route_test.cc:146`. Outcome in the fork not verified; the TODO marks it for redesign.
10. **Behaviour changes that merge cleanly:** #159 route rejection on incomplete terraforming; #94 elevated ways on bridges; #96/#107/#111/#119 building `height_clearance` (derived from image geometry when unset; hex pak images not verified).

### 3.3 Save, version and pak formats
- Upstream: `SIM_VERSION_MAJOR 125`, `SIM_SAVE_MINOR 6`, `MAKEOBJ_VERSION 60.12`. Fork: 124 / 5 / 1 with `SIM_SAVE_MINOR 901` and `SAVE_VERSION_HEX_*` 124.5/900/901.
- Version compare is `major*1000+minor`, so fork saves (124901) fail every upstream `(125, x)` gate. `environment.cc` upstream gates `music_folder` on `(125, 6)` and `horizontal_stripe_owner` / `show_construction_info` on `(124, 6)` (the latter pass for the fork).
- **Recommendation (not tested in a running game):** take MAJOR 125 and keep `SIM_SAVE_MINOR` 901 or higher (125901). Every upstream 125.x gate then passes, the fork's 124.x gates still compare lower, and old fork saves load as "older".
- #36 stops saving halt owners (no version gate; load recomputes). `weg_t::rdwr` must keep the fork's `& ribi_t::all` mask rather than upstream's `& 15`.
- Pak nodes: building v12 is new upstream; the fork's way v9 is separate. Upstream's way writer still emits v8.
- The "Save format version bump" TODO is unaffected otherwise, but the MAJOR bump should be recorded there.

### 3.4 Tests and tooling
- Square-bound tokens in upstream-added test lines: 58 `ASSERT_WAY_PATTERN`, `slope.south` 29 / `north` 6 / `east` 6 / `west` 5, `dir.south` 22, one each of `dir.eastwest`, `dir.northsouth`, `dir.west`, `dir.southeast`. The fork's script API has no `slope.south` / `slope.north` (only `south_narrow` and the like) and tripwires on string patterns.
- The 9 new test scripts: `test_ai_scripted`, `test_convoy_state`, `test_squirrel_vm` and `test_way_elevated` have almost no square tokens. `test_schedule_route` has 36, `test_way_planner` 35.
- `EXPECT_SCRIPT_ERROR` is defined in `tests/test_helpers.nut` (auto-merges). The fork's runner needs the run-script merge above, or tests using it fail.

### 3.5 Build and CI
- New files only: SDL3 backend and `Simutrans-SDL3.vcxproj`, macOS sign/notarize workflows. The workflows may trigger on fork pushes (not verified).
- Not compiled in the trial: makeobj, nettool, `src/fuzz`, Windows and SDL3 files, link.

### 4. Staging

**Stage 1: #1–#10** (`07560d46e`..`a2e8541a8`, 2026-06-09 to 06-16). Contents: chat init, full-underground tunnel fix (#3), multitile-replacement crash and city-level fix (#4/#5), airplane state-machine overhaul start (#6–#8), shared-halt marker in the halt list (#9).
- Conflicts: `viewport.cc` (cosmetic), `simconvoi.cc` (#7: `air_wt` guard around `reserve`).
- Risks: #4/#5 need `boden.h` in `simcity.cc`. Decide the `reserve` signature here: recommend adopting upstream's two-arg `reserve(c, uint32 route_index)` with the index ignored, which turns about 7 later hunks into mechanical matches.
- Cherry-pick the abandoned branch's 3 commits after this stage.
- Test: scenario suite plus airplane tests (runway geometry under hex not verified).

**Stage 2: #11–#93** (`bc593e19b`..`315c9cf66`, 06-16 to 07-25). Contents:
- close diagonals and switch graphics (to be neutralised);
- airplane overhaul (#28–#37, #44, #45, #55, #62, #63);
- multi-owner halt overhaul (`get_owner()` becomes `get_first_owner()` / `get_owners()`, `change_owner(new, current)`, `gui_owner`) (#25–#27, #36, #39–#41, #52–#54, #58, #59, #80, #81);
- `create_player` (#26), `get_target_cities` (#29), non-locked-player scripts (#43);
- `finish_rd` bool (#17), freelist iteration speedup (#92), ceeac warning sweeps.
- Simulated conflicts (18): `ribi.h`, `route.cc`, `way_desc.h`, `way_obj_desc.h`, `way_writer.cc`, `minimap.cc`, `schiene.cc/.h`, `weg.cc/.h`, `wayobj.cc/.h`, `ai_passenger.cc`, `simconvoi.cc`, `simtool.cc`, `freelist_tpl.h`, `air_vehicle.cc`, `rail_vehicle.cc`.
- Design decision: neutralise close diagonals. Options: (a) keep upstream's code compiled but dead, with `is_close_diagonal()` fixed at 0, `set_close_diagonal` a no-op, and tripwires at the entries; (b) delete it. (a) minimises future merge diff; (b) matches the fork's "tripwire over silent shim" rule only if tripwires are added. Either way keep `diagonal_flag` and `close_diagonal_state` at 0.
- Also in this stage: `get_city_road()` becomes the static `stadt_t::city_road` (#32), the owner API rename sweep, and `schiene_t::rotate90` / `reserved2`.
- Effort 4–6 days.

**Stage 3: #94–#183** (`b493a04d9`..`bd0e5ded5`, 07-25 to 08-22). Contents:
- elevated on bridges (#94), height clearance (#96, #107, #111, #114 revert, #119);
- planner APIs and the `terraform` flag (#133–#141, #154, #159);
- schedule-route overlay (#109–#116, #203);
- convoy-state API (#161, #164, #170);
- Squirrel 3.2 migration (#148–#158, `src/squirrel` +314/−175, `squirrel_32_*.h`);
- Windows IME (#168, #172–#175), the first `image_t` depth-0 change (#120), warning sweeps, timeline and starting-month fixes (#122, #123, #137, #144), OOB fixes (#176, #177).
- Simulated conflicts (18): `SourceList.cmake`, `wegbauer.cc/.h`, `ground_desc.cc`, `obj_desc.h`, `image_reader.cc`, `image_writer.cc`, `way_obj_writer.cc`, `simgraph0/16.cc`, `simview.cc`, `api_command.cc`, `changelog.h`, `all_tests.nut`, `test_halt/test_transport/test_way_bridge/test_way_road.nut`.
- Squirrel 3.2 effect on the fork's script API and fuzz harnesses: not verified.
- Effort 4–6 days.

**Stage 4: #184–#268** (`ce3f30718`..`bc1813b9b`, 08-22 to 09-16). Contents:
- contextual construction info (#184, #185);
- SDL3 backend (#205–#222, #229) and Steam decoupling;
- `image_t` freelist and depth 0, STORED_PIXVAL split (#224–#228, #235–#238);
- sign and signal diagonal, front and drive-on-left lists (#223);
- pakset download fixes (#242–#244), AI lifecycle test seam (#245, #246), language ISO renames and sorting (#260–#264), `EXPECT_SCRIPT_ERROR` runner (#258).
- Simulated conflicts (18): `CompileOptions.cmake`, `SourceList.cmake`, `pakset_downloader.cc`, `image_reader.cc`, `simgraph16.cc`, `simview.cc`, `factorylist_frame.cc`, `network_file_transfer.cc`, `roadsign.cc`, `signal.cc`, `schiene.cc`, `ai_passenger.cc`, `simtool.cc`, `simroadtraffic.cc`, `all_tests.nut`, `test_scenario.nut`, `test_way_road.nut`, `run-automated-tests.sh`.
- Risks: the `image_t` freelist vs `TRACK_DESCRIPTORS`; the `ground_desc.cc` early return; compile fixes in `roadsign.cc` / `signal.cc`.
- Effort 3–5 days.

**Stage 5: #269–#327** (`64e66dcb1`..`3e9f4f5e5`, 09-19 to 10-07). Contents: way-builder dialog and wegbauer rework (#269–#275, #280, #286, #293, #302, #308, #309, #317, #320, #323), 125.0 / 125.0.1 (#311, #324), SDL3 mixer, touch and Android (#299–#304), user preferences (#319), music folder (#318), zoom (#326), text-field menu (#279, #283, #297), asymmetric townhall (#310), socket fix (#327).
- Simulated conflicts (11): `SourceList.cmake`, `wegbauer.cc`, `settings.h`, `gui_waytype_tab_panel.cc`, `bruecke.cc`, `simmain.cc`, `simsound.cc`, `simversion.h`, `simcity.cc`, `all_tests.nut`, `test_way_road.nut`.
- Risks: the `bridge_desc.cc` table, the axis-aligned `check_terraform` tests, the MAJOR bump.
- `way_builder_frame.cc` itself has no `ribi`, `koord`, `slope` or diagonal tokens in its added lines (grep 0); the risk lives in `wegbauer.cc` and `simtool.cc`.
- Effort 3–5 days.

### 5. What to take, skip or neutralise

**Take**
- Fixes: #2, #3, #4, #5, #14, #17, #20, #64, #97 (makeobj UAF), #121, #122, #123, #137, #138, #144, #176, #177, #180–#182, #186, #198, #251, #252, #253, #254, #266, #310; MOTD buffer fixes #127, #128, #237 (check against the fork's nettool hardening, not verified); #127–#128 etc.
- Script API: #26 `world.create_player`, #29, #43, #133–#141 and #154 (planner APIs; #140's `terraform` flag retires the fork's "Missing `terraform=`" TODO), #161, #170.
- Test seam: #245/#246 `api_ai_test.cc` + `tests/ai/stlab_probe/ai.nut` + `test_ai_scripted.nut`. This is the AI test hook the fork's "AI players" TODO asks for. It attaches fixture or shipped AIs to scripted-AI players created by `world.create_player(slot, 4)`.
- Performance and robustness: #92 (freelist iteration in `sync_step`), #224/#225/#226 (headless memory; fix the tracking interplay first), #227 (atomic dirty-tile publication, multithreaded bug fix), #258 (test runner).
- Platform and tooling (low conflict): SDL3 (#207–#222, optional build), FluidSynth bumps, Android and IME fixes, translations and ISO renames (#239–#241, #260–#264), #318/#319.
- Squirrel 3.2: take, but verify against the fork's script API changes and fuzzers (not verified).

**Neutralise**
- Close diagonals: #11, #16, #18, #19, #21–#24, #31, #83, #87, #88, #91, #200, the diagonal half of #223, `calc_cached_image`, and `reserved2` / `enter`.
- Switch graphics (#13, #199).
- Schedule-route overlay drawing (#109, #110, #203): keep the data layer and tests that only read it.
- Way-builder dialog (#269 onward): merge, but its menu hookup and the square-based planner logic need hex review.

**Skip**
- macOS signing and notarize workflows (#255–#259).
- `sqai_rail` revisions merge cleanly, but the fork's TODO says that AI needs a redesign.

### 6. `claude/admiring-davinci-l0cr7t`

- Tip `a4c75fd35` merges upstream up to `5a3b9b5b5` (#5, r12015) only, on base `c4b64eeef` (main at 2026-06-08). Nine commits are not on main: 5 upstream (#1–#5), 3 fork-authored, 1 merge.
- Merging #1–#5 into current main has 0 textual conflicts (prefix scan), so its conflict resolutions (`changelog.h`, `simcity.cc` include, `simtool.cc`, `all_tests.nut`, `test_city.nut`) only existed to host its own 3 commits.
- Worth keeping:
  - `3611241d8` ledger fix: 5 lines in `build_city_house`.
  - `9b818af87`: `city_x::get_housing` / `get_jobs` / `get_homeless` / `get_unemployed` and `building_desc_x::get_level`.
  - `fe78dd2e7`: `test_city_population_ledger` and `TEST_RES_2x2`. `tools/test.py::ensure_test_pak` bakes everything under `tests/test-pak/`, so the `.dat` needs no runner change (not run).
- Use the merge commit's versions: the `.dat` under `tests/test-pak/`, and the workflow step already dropped.
- Expected conflicts when cherry-picking onto the merged tree: `changelog.h`, `all_tests.nut`. The fix itself cherry-picks cleanly onto upstream/master (merge-tree), but conflicts on current main until #4/#5 land.

### 7. Not verified

- No full build or link; `-fsyntax-only` on a crude resolution; no runtime tests (`tools/test.py` not run).
- Not compiled: makeobj, nettool, `src/fuzz`, Windows and SDL3 files.
- Not checked at runtime: airplane overhaul under hex runways; elevated ways on bridges; height clearance with `hextrans-pak128`; zoom levels; Squirrel 3.2 effect on the fork's API; the way-builder dialog; `karte_t::rotate90` behaviour in `test_schedule_route_rotate`; headless effect of the `ground_desc.cc` early return; whether the macOS workflows trigger on the fork; `roadsign_writer.cc` auto-merge correctness; the semantic overlap of #232 with `ed7baee97`; `simcity.cc:3695–3696` composite ribis.
- Effort figures are estimates.
- The per-stage conflict lists come from a simulation that takes the fork side on conflicts, so later stages are approximate.
- The staged tests of the merge-base logic (moving base against unmerged `origin/main`) produced spurious conflicts and were discarded.