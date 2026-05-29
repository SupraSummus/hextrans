# Implementation plan: script-tool → scenario reservation flow

Turns the architecture in `scenario-data-sender.md` (§9) into an
actionable, phased plan. Each phase is independently testable and
mergeable; later phases depend on earlier ones but earlier ones deliver
value on their own.

Target feature: in an online scenario a player drags a rectangle with a
script tool to **request** a land reservation; the request is stored
server-side; an admin reviews it; approval becomes an engine rule that
grants that player build access to the rectangle.

References use the upstream `simutrans` branch.

---

## Design decision to settle first

The one fork that shapes everything downstream is the **client → server
transport** for a request:

- **Option A — reuse `default_param` (no engine change).** The request
  tool issues one command whose `default_param` carries the serialized
  rect; the server-side scenario reads it in `is_work_allowed_here`.
  Smallest footprint, fully expressible today, but overloads a *query*
  hook with a *mutation* and piggybacks on a stringly-typed param.
- **Option B — dedicated `tool_message(pl, data)` scenario callback.** A
  generic "a tool sent the scenario a message" primitive routed through
  the existing `nwc_tool_t` command path, returning ack/err. A small,
  honest engine addition (prissi's post-5 suggestion); clearer intent,
  no abuse of `is_work_allowed_here`.

**Recommendation:** prototype on **A** to prove the data flow end to end
with zero engine risk, then land **B** as the production transport. The
phases below are written so A→B is a swap at one seam (Phase 2), not a
rewrite.

Everything that is **not** the transport (request store, admin review,
grants, enforcement) is identical under both options and is mostly
scenario-script (pakset) work, not engine work.

---

## Phase 0 — Test scaffold and baseline

**Goal:** be able to exercise scripted-tool ↔ scenario behaviour
automatically, before changing anything.

- Adapt the forum example (attach 34013: `script_mark_rect` +
  `script_confirm_rect` + `test-mark-rect` scenario) into the repo's
  scenario test layout (under `simutrans/script/` test scenarios / the
  existing test harness used by `is_work_allowed_here` tests).
- Add a **local** (single-player, `SCRIPTED`) test that drives the
  existing committed `send_data()` → `tool_data_to_scenario()` path and
  asserts the scenario received the payload. This pins current behaviour
  so Phase 1 refactors are safe.

**Exit:** a green local test that sends a table from a script tool and
observes it in the scenario VM.

**Risk:** low. No production code changes.

---

## Phase 1 — Clean the local mechanism (fix the §1 bugs)

**Goal:** make the committed Yona mechanism correct for what it can
actually do — **local** (`SCRIPTED`) scenarios — and establish the
scenario-side callback shape Phase 2/3 reuse.

Changes (all in files the patch already touches):

- `simtool-scripted.cc`: in `tool_exec_script_t::init`, register
  `intern_send_data` **inside one `init_vm` guard** and drop the
  duplicate `init_vm` call (mirror the two-click variant). Remove the
  unused registry tool lookup in both senders.
- `simtool-scripted.cc`: null-guard `welt->get_scenario()`; return a
  clear error when there is no scenario / it is not `SCRIPTED`.
- `simtool-scripted.cc`: `intern_send_data` **returns the scenario eval
  error** as the Squirrel return value (stop matching the magic
  `"Error compiling string buffer"` string).
- `scenario.cc` / `scenario.h`: replace the blind `eval_string` into the
  root table with a **named callback** —
  `tool_data_to_scenario(pl, data)` (or `on_tool_message`) — that
  decodes the payload into a local and calls a scenario function
  returning a result/error string. Register the name in
  `src/simutrans/script/api/api_skeleton.cc` next to
  `is_work_allowed_here`.
- `tool_base.nut` + `script_base.nut`: surface the returned error from
  `send_data()`; de-duplicate `recursive_send` against the existing
  `recursive_save` (one shared serializer).

**Exit:** Phase 0 test still green; a denied/error case round-trips a
message back to the tool; no global `sender` clobber.

**Risk:** low–medium, self-contained, no network code. **Independently
mergeable** and useful for single-player scenarios on its own.

---

## Phase 2 — Network transport (one request, one crossing)

**Goal:** a request from a client-side tool reaches the **server**
scenario exactly once, validated.

Option A (prototype):
- Request tool puts the serialized rect in the work command's
  `default_param`.
- Confirm it arrives in `scenario_t::is_work_allowed_here` **server-side**
  (`scenario.cc:603`, called from `nwc_tool_t::clone`,
  `network_cmd_ingame.cc:1224`); record there.
- **Validate server-side**: clamp rect to map bounds, check the
  requesting player may request in that area. Never trust client data.

Option B (production):
- Add a `tool_message`-style callback dispatched through `nwc_tool_t`
  (reuse the relay that already carries tool commands and the
  scenario-check block at `network_cmd_ingame.cc:1217-1242`).
- Same server-side validation; returns ack/err to the sender via the
  existing `TOOL_ERROR_MESSAGE` reply path.

Cross-cutting:
- Keep the payload **compact** (rect corners, not tile lists) and bound
  its size against the engine's packet caps (cf. recent
  packet-limit/cap-wire-count work on this repo).
- Update the **list of tools relayed/queried** if the request tool's id
  is not currently forwarded (prissi post 22: "the list of tools needs
  an update").

**Exit:** on a local server + client, a single drag produces one stored
request server-side; an invalid request is rejected with a message back
to the client.

**Risk:** **highest** — touches the network command path. Needs a
two-process (server + client) test, not just a unit test.

---

## Phase 3 — Request store + admin review surface

**Goal:** persist pending requests and let an admin see and act on them.

Mostly **scenario-script (pakset)** work; minimal engine.

- Scenario store: `persistent.requests` table in `scenario.nut`, with
  add / list / remove helpers. Saved/restored for free via `persistent`
  (`scenario.cc:1077,1094`). Server-side only.
- Admin view (the client-visibility gap): surface the queue to a
  client-side admin through an **existing synced display channel** —
  preferred: a chat/message via the message API (ties into Yona's
  topic-23257 idea); alternative: a generated **read-only** marker. The
  authoritative store stays server-side; only a display copy crosses.
- Admin approval tool: converts a chosen request into
  `rules.allow_way_tool_rect(owner, …, rect)` and removes it from
  `persistent.requests`.

**Exit:** request appears in the admin's view; approving it grants the
player and clears it; denying it clears it with a notice.

**Risk:** low–medium; concentrated in pakset script + one display
channel choice.

---

## Phase 4 — Enforcement, hardening, known gaps

- **Grant enforcement** is automatic: the rule distributes to clients
  and is checked locally every action (no work here beyond Phase 3).
- **Label protection** (only if labels are used anywhere as a store):
  Yona could not stop players renaming/deleting the coordinate label.
  Protecting it needs a `TOOL_RENAME` rule, which is **blocked** by the
  missing "default param for script tools" (forum topic 23274) — track
  as an external dependency, or avoid labels-as-store entirely (the
  Phase 3 design does).
- **Popup-message buffer bug** (forum post 19): previous popup reappears
  because the buffer is not cleared on script error/restart — separate
  bug, fix independently if it bites testing.
- Two-click tools only get **endpoint** scenario checks server-side
  (`network_cmd_ingame.cc:1226-1229`), never per-tile — the batched
  design avoids needing per-tile online checks, but document the
  limitation.

---

## Sequencing and dependencies

```
Phase 0 (tests) ──► Phase 1 (clean local) ──► Phase 2 (transport A→B)
                                                      │
                                                      ▼
                                          Phase 3 (store + admin)
                                                      │
                                                      ▼
                                          Phase 4 (enforce + gaps)
```

- Phases 0–1 are pure cleanup of what already exists and can merge first.
- Phase 2 is the risky network seam; the A→B fork lives entirely here.
- Phases 3–4 are mostly pakset/scenario script, gated on Phase 2.

## What is explicitly out of scope

Running tool scripts in lockstep; a client-side VM-to-VM data pipe (the
original patch shape); any engine-side "reservation"/"ownership" data
structure. The engine stays domain-agnostic throughout — the only
candidate engine addition is the generic `tool_message` callback in
Phase 2 Option B.
