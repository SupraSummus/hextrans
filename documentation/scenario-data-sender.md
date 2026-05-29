# Script tool → scenario data sender

Investigation of Yona-TYT's forum patch "[Script API] Simple data
sender from script tool to scenario"
(<https://forum.simutrans.com/index.php/topic,23273>), the bugs in it,
the architectural blocker the thread uncovered, and a proposed clean
design. Written against the upstream `simutrans` branch — this is an
upstream Script-API feature, not hex-port work.

Patch revisions referenced: attach 33934 (v1, 2024-11-27) and attach
33982 (v2, 2024-12-11). Examples: attach 33983 / 34013.

## What the patch does

A one-way **script-tool → scenario** channel, built on the same
serialize-to-source-string trick the engine already uses for
persistent state.

Script side (`simutrans/script/tool_base.nut`): adds a global
`sender <- {}` table plus `send_data()`, `recursive_send()`,
`is_identifier()`. These are a near-verbatim copy of the existing
`save()` / `recursive_save()` / `is_identifier()` in
`script_base.nut`. `send_data()` serializes `sender` into a Squirrel
*source* string `"sender = { ... }"` and passes it to a C++-registered
function `intern_send_data(str)`. (v2 also clears `sender` after
sending.)

C++ side (`src/simutrans/tool/simtool-scripted.cc`): `intern_send_data`
is registered into both the one-click (`tool_exec_script_t`) and
two-click (`tool_exec_two_click_script_t`) tool VMs. Its body
(`tool_sender` / `two_click_tool_sender`) calls
`scenario_t::tool_data_to_scenario(str)` →
`script->eval_string(str)`, which executes the string in the
**scenario** VM, populating a `sender` global there (a matching
`sender <- {}` is added to `script_base.nut` so the scenario can read
it).

Round trip: tool's `sender` table → source string → scenario VM evals
it → scenario's `sender` global.

In the *final* example (attach 34013) the author abandoned this channel
and stored the rect coords in a map **text label** read back via
`world.get_label_list()` — a workaround forced by the network blocker
below.

## Confirmed bugs (matches prissi's review)

1. **Double `init_vm`** in `tool_exec_script_t::init`
   (`simtool-scripted.cc:208`). The patch inserts
   `if (init_vm(player)) { …register intern_send_data… }` and then
   keeps the original `return init_vm(player) && …`. `init_vm` reloads
   the VM when `info->restart` is set (`exec_script_base_t::init_vm`,
   `simtool-scripted.cc:95`), so the second call can wipe the
   just-registered function and registry slot. The two-click variant
   registers inside the single existing `init_vm` guard
   (`simtool-scripted.cc:283`) and is correct — the one-click variant
   should follow that shape.

2. **`scenario` can be NULL.** `karte_t::get_scenario()`
   (`simworld.h:631`) returns the raw member, which is NULL at
   `simworld.cc:533` and `:2054`. `tool_sender` dereferences it with no
   guard.

3. **Dead code.** Both senders look the tool pointer up out of the
   registry into a local `tool` and never use it — copy-paste from
   `script_mark_tile` (`simtool-scripted.cc:361`).

4. **Fragile error handling.** v2 string-compares the error against the
   literal `"Error compiling string buffer"` and otherwise swallows it;
   the real error never reaches the script that called `send_data()`.

5. **No scenario-side entry point.** The data is eval'd straight into
   the scenario root table, clobbering a global `sender`. The scenario
   gets no callback and would have to poll — unlike every other
   scenario hook (`is_work_allowed_here`, `is_tool_allowed`, …) which
   is a named function returning a result/error string.

## Architectural blocker

The scenario script's per-action hooks are **not** consulted in network
games. `scenario_t::is_work_allowed_here` calls the script only under
`if (what_scenario == SCRIPTED)` (`scenario.cc:661`), never
`SCRIPTED_NETWORK`; online, only the static `forbidden_tools` rectangle
rules are checked, by design (otherwise every mouse action waits a
server round-trip — prissi, posts 11/20). `tool_data_to_scenario` is
gated the same way, so in a network game it is a no-op.

Consequently the patch's intended use case — a player marks a rect with
a script tool, the data goes to the server scenario for an admin to
approve — has **no working path online** as written. This is the
thread's unresolved conclusion (prissi, posts 15/17/20).

prissi's suggested directions (posts 13/22):
- Add a path that relays script-tool work to the server and gives the
  scenario a real callback, or
- A variant of `is_work_allowed_here` that checks rules only (no
  per-tile script call), and
- Update the list of tool ids relayed to the server for scenario
  checks — "all tools are queried, but obviously the list of tools
  needs an update" (post 22).

prissi also floated `sq_move` (post 5) as a more versatile transport,
but it can only move stack objects between VMs on the same machine
(post 9), so it is unusable for the cross-network case and the
serialize-to-string approach stands.

## Proposed clean design (mechanism only)

Scope this to the locally-working mechanism, honestly documented as
local-only, leaving the network plumbing as a separate, larger piece.

- **Single registration.** In `tool_exec_script_t::init`, register
  `intern_send_data` inside one `init_vm` guard, matching the two-click
  variant; drop the duplicate `init_vm` call.
- **Null-safe.** Guard `welt->get_scenario()` before use; return a clear
  error if there is no scenario or it is not `SCRIPTED`.
- **Drop dead code.** Remove the unused registry tool lookup in both
  senders.
- **Propagate errors.** `intern_send_data` returns the error string from
  the scenario eval to the script (push it as the Squirrel return value)
  instead of matching a magic message; `send_data()` surfaces it.
- **Named scenario callback.** Instead of eval-ing into the root table,
  decode the payload into a local and call a defined scenario hook —
  `function tool_data_to_scenario(pl, data)` returning a result/error
  string — so the scenario is notified and can validate. This matches
  the existing callback convention and removes the silent-global
  clobber.
- **De-duplicate serialization.** `send_data`/`recursive_send` is a copy
  of `save`/`recursive_save`; factor the shared recursive serializer so
  the two paths don't drift.

Network support (relaying script-tool work to the server, a
rules-only `is_work_allowed_here` variant, and extending the relayed
tool-id list) is deliberately out of scope here and tracked separately.
