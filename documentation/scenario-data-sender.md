# Script tool → scenario communication: investigation and architecture

Write-up of the investigation into Yona-TYT's forum patch
"[Script API] Simple data sender from script tool to scenario"
(<https://forum.simutrans.com/index.php/topic,23273>), the engine
architecture it runs into, and a proposed design for the feature it was
trying to enable (server-side review of player land-reservation
requests in an online scenario).

Written against the upstream `simutrans` branch — this is an upstream
Script-API concern, not hex-port work. File/line references are to that
branch.

> **Alternative being kept in parallel:** a more ambitious framing puts
> land ownership directly in the engine instead — see
> `land-ownership-alternative.md`. Neither is committed to yet.

---

## 1. The original patch

A one-way **script-tool → scenario** channel built on the engine's
existing persistent-state serialization trick.

- **Script side** (`tool_base.nut`): adds a global `sender <- {}` table
  plus `send_data()` / `recursive_send()` / `is_identifier()` — a
  near-verbatim copy of the existing `save()` / `recursive_save()` in
  `script_base.nut`. `send_data()` serializes `sender` into a Squirrel
  *source* string `"sender = { … }"` and passes it to a C++-registered
  `intern_send_data(str)` (and clears `sender`).
- **C++ side** (`simtool-scripted.cc`): `intern_send_data` →
  `scenario_t::tool_data_to_scenario(str)` → `script->eval_string(str)`,
  executing the string in the **scenario** VM, populating a `sender`
  global there.

Patch revisions: attach 33934 (v1, 2024-11-27), 33982 (v2,
2024-12-11). Examples: 33983 / 34013. In the final example the author
abandoned this channel and stored rect coords in a map **text label**,
forced by the network blocker in §5.

### Confirmed bugs (corroborating prissi's review)

1. **Double `init_vm`** in `tool_exec_script_t::init`
   (`simtool-scripted.cc:208`): registers `intern_send_data` inside one
   `init_vm` guard, then calls `init_vm` again in the return; when
   `info->restart` is set the second call reloads the VM and wipes the
   registration. The two-click variant (`:283`) does it correctly.
2. **`scenario` can be NULL.** `get_scenario()` (`simworld.h:631`)
   returns the raw member, NULL at `simworld.cc:533,2054`; the sender
   dereferences it unguarded.
3. **Dead code.** Both senders look the tool pointer out of the registry
   and never use it (copy-paste from `script_mark_tile`).
4. **Fragile errors.** v2 string-matches `"Error compiling string
   buffer"` and otherwise swallows the error; it never reaches the
   calling script.
5. **No scenario entry point.** Evals straight into the scenario root
   table, clobbering a global, with no callback — unlike every other
   scenario hook, which is a named function returning a result.

---

## 2. Engine vs. script: the layering

The engine is **domain-agnostic**. It has no concept of "land",
"ownership", or "reservation" — and in fact no concept of land
ownership *at all*: only built objects (ways, buildings, stops) have an
owner, never bare terrain. Yona's "reservation" is assembled entirely
in scenario Squirrel out of generic primitives:

- **Scripted tools** — run arbitrary Squirrel on player action.
- **The scenario rule system** (`forbidden_t`, bound as `rules.*`) — a
  generic permission table of `(player, tool_id, waytype, param,
  rectangle, error-message)` allow/forbid entries. This is the closest
  thing to "ownership", but it only means "player P may/may-not use
  tool T in rectangle R."
- **Map labels** (`label_x`) — generic text-on-tile objects.
- The proposed data pipe — a generic "eval a string in the scenario
  VM".

A "reservation" = forbid a tool for everyone, then
`rules.allow_way_tool_rect(owner, …, rect)` for one player; the *record*
of who reserved what is a label whose text is `"x1,y1,x2,y2"`. All
domain meaning lives in `scenario.nut`. The engine ships agnostic
primitives; scenarios compose gameplay on top.

---

## 3. Two storage systems, both persisted by the engine

| | Engine rule table | Script `persistent` table |
|---|---|---|
| Lives in | C++ `forbidden_tools[MAX_PLAYER_COUNT]` (`scenario.h:179`) | the scenario VM |
| Saved as | **binary**, each `forbidden_t::rdwr` (`scenario.cc:1105-1115`) | **string**, via `save` → `eval_string` (`scenario.cc:1077,1094`) |
| Reconstructed by | re-reading bytes (not the script) | eval'ing the saved string into the VM |
| Available to a network client | **yes** (distributed as data) | **no** (client has no VM) |
| Expresses | static spatial permission rects | arbitrary script-defined state |
| Guard | — | savegame with `system(` in the string is rejected (`scenario.cc:1032`) |

The rule table must be engine-resident because it is consulted
synchronously on every tool action, **including on clients that never
run the script**. The `persistent` table is the script's general save
store but exists only where the VM runs (server / local).

---

## 4. Lockstep and determinism

Network play is **lockstep**: every participant runs the same
simulation and must agree, each sync step, on a checklist
(`utils/checklist.h`):

```cpp
uint32 hash; uint32 random_seed;
uint16 halt_entry; uint16 line_entry; uint16 convoy_entry;
```

i.e. the shared RNG seed plus a hash/counts of world state. Alongside
it, the **stream of tool commands** (`nwc_tool_t`) is replicated and
executed identically by all. **No script state is in the checklist.**

Determinism is *engineered*, not assumed:

- The synchronized simulation is **integer-only**. The shared RNG is an
  integer Mersenne Twister over `uint32` (`simrandom.cc:48-108`);
  integer math with fixed-width types is bit-identical across CPUs
  (endianness only matters at serialization).
- There are **two RNGs**: `simrand()` (integer, shared seed, in the
  checksum) for anything affecting the sim, and `sim_async_rand()` (a
  local wall-clock-seeded LCG, `simrandom.cc:144-155`) for cosmetic
  things that must never touch sync state. Tooling exists "for network
  debugging, i.e. finding hidden simrands in wrong places"
  (`simrandom.h:39`).
- Where floats *do* appear (terrain perlin noise, `simrandom.cc:177+`)
  the **output is distributed as data** — clients download the finished
  map; they don't regenerate terrain in lockstep.

Squirrel cannot be confined to this discipline (its default number type
is float, table iteration order is not portably guaranteed, scripts are
author-written). So **scripts are kept off the lockstep path entirely**:

- **Scenario scripts** run on the **server only**; a network client sets
  `what_scenario = SCRIPTED_NETWORK; script = NULL`
  (`scenario.cc:1047-1052`).
- **Scripted AI players** likewise — "scripted players run on server
  only, for now at least" (`ai_scripted.cc:240-244`).
- **Script tools** run in their own VM on the **acting client** only.

Scripts influence the synchronized world only by emitting **commands**
or producing **distributed data** (the rule table). Same rule that
governs floats: keep non-deterministic computation off the synchronized
path; let it act only through commands or distributed data.

---

## 5. The two enforcement points

Scenario permission checks happen in two distinct places, for two
distinct reasons:

1. **Server-side authoritative gate.** `nwc_tool_t::clone()` runs on the
   server before broadcasting a command (`network_cmd_ingame.cc:1217-1242`):
   it calls the scenario `is_tool_allowed` / `is_work_allowed_here` (the
   server has the script). Denied → a `TOOL_ERROR_MESSAGE` goes back to
   the sender and the command is never broadcast. This **is** a
   server-side script hook; it runs **once per committed command**, and
   only its *outcome* (broadcast-or-not) is synchronized, so no desync.
2. **Client-side instant feedback.** The builders / route-finder
   (`wegbauer.cc`, `tunnelbauer.cc`, `terraformer.cc`, `simtool.cc:3544`)
   consult the **distributed rule table** locally — instant, no script,
   no round-trip — to drive the cursor, route preview, and greying.

Neither subsumes the other: the hook is authoritative + dynamic but
once-per-command and server-only; the rule data is instant + local but
static spatial only. This is why the split exists and can't be
collapsed.

---

## 6. Why script tools are slow online

A scripted tool is a **local orchestrator** on the acting client. Its
script computation and marking (`zeiger`) are local and never networked.
But when it changes the shared world it invokes a real tool, which is
**not** network-safe (scripted tools don't override
`is_*_network_save`), so each such sub-action becomes an `nwc_tool_t`
command: client → server → schedule → broadcast → execute at a future
sync step → callback. The script **suspends** between issuing a
sub-action and its completion (`work` returns `"suspended"`,
`waiting_for_do_work`; resumed by `exec_script_base_work_callback`,
`simtool-scripted.cc:25-37`).

So per-tile work = one network round-trip per tile, serialized, each
blocking the script. A human pays this latency once per click; a script
doing bulk work pays it N times in a row — prissi's "building becomes
almost impossible … every tile is queried to the server."

**The slowness is the *presence* of the lockstep command pipeline in the
path of every action, not the lack of it.**

---

## 7. Why the patch can't work online

The data-sender's `intern_send_data` → `eval_string` runs inside the
**tool's** VM, which executes on the **client**. "Eval into the scenario
VM" therefore hits only the *local* scenario VM — which on a client is
NULL / `SCRIPTED_NETWORK`. The only thing that crosses the client→server
boundary is the **command stream**. So the patch is aimed at the one
layer a networked client tool cannot reach, and is gated to `SCRIPTED`
(local) on top of that. It is the wrong layer, mechanically — not just
buggy.

---

## 8. Where the data belongs

- **Request store → script**, in the scenario's `persistent` table, on
  the **server**. A pending reservation request is domain state; the
  engine has no business knowing about it (same reason it has no
  ownership concept). `persistent` saves and restores it for free.
- **Approved grant → engine**, as a `rules.allow_way_tool_rect(...)`
  entry in `forbidden_tools`. This *must* be engine-side: it is enforced
  on every client action with no VM.
- **Admin's view of the pending store** — the store stays in script, but
  showing it to a *client-side* admin needs an existing engine-synced
  **display** channel (chat message, or a generated read-only marker),
  because the admin's client has no scenario VM. This is a display
  concern, not a reason to move the store into the engine. (This is
  exactly why Yona reached for labels, then hit "players can rename the
  label".)

---

## 9. Proposed architecture

Principle: **do the expensive/interactive work locally on the client
(free), cross the command stream exactly once with a compact batched
payload, validate and store it server-side.** This is the inversion of
the slow per-tile pattern in §6.

### Data flow

```
CLIENT (acting player)                     SERVER (authoritative)
─────────────────────                      ──────────────────────
script tool:
  - drag a rect (local, free)
  - mark tiles via zeiger (local)
  - build ONE payload {x1,y1,x2,y2}
        │
        │  single tool command, payload in
        │  default_param  (crosses network once)
        ▼
                                  nwc_tool_t::clone (server):
                                    scenario receives the payload
                                    via is_work_allowed_here / a
                                    dedicated callback
                                        │
                                        ▼
                                  scenario script (server VM):
                                    validate (clamp to map, check
                                    player may request here)
                                    append to persistent.requests
                                        │
                          ┌─────────────┘
                          ▼
                  display to admin via a SYNCED channel
                  (chat message / read-only marker)
                          │
            admin reviews, runs an admin tool to APPROVE
                          │
                          ▼
                  scenario: rules.allow_way_tool_rect(owner,…,rect)
                          │
                          ▼ (rule table distributed as data)
ENFORCEMENT on every client, every action, no VM, no round-trip
```

### Components

- **Request tool** (script tool, client): computes the rect locally,
  marks it cosmetically, and submits **one** command. No per-tile
  networked sub-actions. If multiple requests are queued, accumulate in
  the `sender` table and flush once (the patch's `sender` +
  `send_data()` is already a batch buffer — pointed the right way).
- **Transport:** the request payload rides a single command. Either
  reuse the existing `default_param` string (no engine change), or — for
  an explicit, debuggable design — a dedicated server-side callback
  `tool_message(pl, data)` returning ack/err (see §10). The batch is the
  payload.
- **Server store:** `persistent.requests` in the scenario VM. Validated
  on arrival; **never trust the client-supplied rect** — clamp to map
  bounds and check the requesting player. Keep payloads compact (rect
  corners, not tile lists); the engine caps packet sizes, so unbounded
  batches are a non-starter.
- **Admin review surface:** a synced display channel so a client admin
  can see the queue (chat / marker). Read-only; the authoritative store
  stays server-side.
- **Grant:** approval writes a rule into `forbidden_tools`, which
  distributes to all clients and is enforced locally thereafter.

### What needs no engine change vs. what does

- **No change needed:** the whole flow is expressible today —
  payload-in-`default_param` → server `is_work_allowed_here` records into
  `persistent` → admin tool → `rules.allow_way_tool_rect`. This is the
  minimal, mergeable path.
- **Optional small engine addition:** a dedicated `tool_message(pl,
  data)` scenario callback routed through the existing `nwc_tool_t`
  command path. This *clarifies intent* (a mutation hook, vs. abusing
  the `is_work_allowed_here` query) and avoids overloading
  `default_param`. It does **not** add any domain concept to the engine
  — it stays a generic "tool sent the scenario a message" primitive.
  This is essentially prissi's post-5 suggestion ("a dedicated tool for
  sending strings to players/scenarios that returns error messages").

What is explicitly **not** proposed: a client-side VM-to-VM data pipe (the
original patch), running tool scripts in lockstep, or any engine-side
"reservation" data structure.

---

## 10. Open questions / caveats

- **Two-click route checks** validate only the endpoints server-side
  (`network_cmd_ingame.cc:1226-1229`), never every tile, so per-tile
  *dynamic* logic online remains a genuine gap regardless of approach —
  but the batched-request design sidesteps it by not needing per-tile
  online validation.
- **`is_work_allowed_here` is semantically a query.** Recording state
  inside it works but is a mild abuse; the `tool_message` callback in §9
  is the cleaner home if an engine change is on the table.
- **Mid-game propagation of server `persistent` to clients** is not
  relied upon — the request store is intentionally server-only; only a
  *display copy* crosses, via a synced channel.
- **Batching limits:** decide atomicity (all-or-nothing per rect is
  simplest) and bound payload size against the engine's packet caps.
