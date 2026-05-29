# Alternative: land ownership as an engine feature

An **alternative** to the agnostic scenario-script approach in
`scenario-data-sender.md` / `scenario-data-sender-plan.md`. Both are
kept on purpose; neither is committed to yet. This doc captures the
engine-native framing so the two can be compared.

Framing (as proposed): **all land is owned**. By default every tile
belongs to the public-service player, which **grants build access to
everyone** — so with the feature inactive nothing changes. This default
can be **overridden** (by script or UI) so that building on owned land
requires the land first, and acquiring land from another owner requires
either a purchase or the **seller's explicit permission**.

This supersedes the data-sender entirely if chosen: ownership becomes
engine state changed by dedicated commands, so no tool→scenario
messaging is needed.

References use the upstream `simutrans` branch.

---

## Why this fits the engine (and dissolves the hard problem)

Everything that made the agnostic approach awkward was a consequence of
*where the state lived* — scenario `persistent` is server-only and
invisible to clients, scenario scripts don't run on clients, and
per-tile script checks cost a network round-trip each (see
`scenario-data-sender.md` §3–§6).

If ownership is **engine world-state** instead, all three problems
disappear, because engine state is **synced to every client and saved
automatically** — exactly like object ownership, money, and the
`forbidden_tools` rule table already are:

- **Enforcement** is an integer check ("does player P own / have
  permission for tile T?") placed next to the checks that already run in
  every build path. Synchronous, client-side, no script, no round-trip;
  deterministic and lockstep-safe (player-id comparison, no floats).
- **Seller-approval** — hard in the scenario world — becomes natural: a
  sequence of ordinary commands (`offer`, `accept`) mutating synced
  engine state both clients can see. No server-only store, no display
  channel.

The engine already has the building blocks:

- per-object ownership: `simobj.h:84-105` (`owner_n`, `get_owner`)
- public-service player: `simplay.h:256` (`is_public_service`),
  `PLAYER_UNOWNED = 15` (`simconst.h:27`)
- money/cost booking inside builders:
  `player_t::book_construction_costs` (e.g. `wegbauer.cc:1121`)
- spatial permission infrastructure to mirror: `forbidden_tools`
  (`scenario.h:179`) with sorted-list / binary-search rect matching

So this extends an ownership model the engine already has for *objects*
down to *land* — not a foreign concept.

---

## Components

1. **Ownership store.** Who owns which land. Granularity is an open
   decision (below). Saved (savegame version bump) and synced like other
   world state.
2. **Activation + default.** A global setting, **off by default**. When
   off, every new check short-circuits and behaviour is unchanged. When
   on, unowned land defaults to the public-service player, which by
   default permits all — so even "on" is inert until an owner restricts
   access. Old saves load with the feature off.
3. **Enforcement seam.** Build paths consult ownership where they
   already consult `book_construction_costs` and the scenario checks
   (ways, buildings, stations, terraforming, trees, signs, bridges,
   tunnels…). One predicate, many call sites.
4. **Transfer commands.** Acquire/relinquish land as `nwc_tool_t`
   commands (buy / sell / set-price, or offer / accept). Money transfer
   is integer and deterministic.
5. **UI.** Ownership overlay, a buy/sell/set-price tool, and (if
   negotiation) offer/accept dialogs.
6. **Policy override hook.** Script or UI sets *who must permit what* —
   configuration only; per-action enforcement stays in the engine, so
   the slow per-tile script path never appears.

---

## Open decisions (no preference recorded — recommendations marked)

- **Granularity.** *Parcel / rectangle* (per-player list of owned rects,
  like `forbidden_tools`) — cheap to store/sync/check, reuses proven
  infra. **(recommended)** vs *per-tile owner id* — tile-granular, but
  ~16 MB on a 4096² map plus sync/save cost at every tile.
- **Transfer model.** *Instant market first* (set price → pay →
  transfer in one command), add seller-approval negotiation later on the
  same substrate **(recommended)** vs *seller-approval negotiation* up
  front (matches the "permission from seller" framing, more states/UI)
  vs *both from the start*.

---

## Trade-offs vs. the agnostic approach

| | Engine land ownership | Agnostic scenario (PR #244 plan) |
|---|---|---|
| Online enforcement | native, client-side, no round-trip | rule table for grants; per-action script impossible online |
| Seller-approval flow | natural (synced offers) | hard (server-only store, transport gap) |
| Reusable outside scenarios | yes — a general game mechanic | no — scenario-specific |
| Engine invasiveness | high (many build paths, save format, UI) | low–minimal |
| Backward-compat risk | needs off-by-default flag + version bump | none |
| Upstream acceptance | hard sell — changes core model | easier — small/no engine change |
| Determinism | fine (integer ownership/money) | fine |
| Makes data-sender patch | unnecessary | the data-sender *is* the mechanism |

Short version: the engine approach is architecturally cleaner **for a
real online land mechanic** and removes the problems we hit, but it is a
big, cross-cutting, upstream-sensitive feature. The agnostic approach is
small and mergeable but only ever a scenario-specific workaround.

---

## Rough phasing (if pursued)

```
1. Ownership store + off-by-default activation flag + save/version
2. Enforcement predicate wired into the build-check seam (feature-gated)
3. Transfer: instant market commands (buy/sell/set-price) + money
4. UI: ownership overlay + land tool
5. (optional) Seller-approval negotiation: synced offers + accept
6. Script/UI override hook to configure who-permits-what
```

Phases 1–2 are the spine (storage + enforcement); 3–4 make it usable;
5–6 add the negotiation and policy configuration. Each is gated behind
the activation flag so nothing ships enabled until complete.

## Out of scope (unchanged from the agnostic doc)

Running tool scripts in lockstep; a client-side VM-to-VM data pipe. The
only philosophical shift here is deliberate: land ownership *does* become
an engine concept — but a generic, economy-level one (owner + price +
permission), not a scenario-specific "reservation" type.
