# Land ownership: the engine-native alternative, narrowed

An alternative to the agnostic scenario approach
(`scenario-data-sender.md` / `…-plan.md`). This started as a large
design space; probing the code narrowed it to **one binary decision**.
Recorded here, undecided, for when there's a concrete product need.

References use the upstream `simutrans` branch.

## Framing

All land owned; by default the public-service player owns it and permits
everyone (so the feature is inert until an owner restricts access).
Override (script or UI) to require owning land to build, and a
purchase / seller's permission to acquire it.

## What the code already gives us (verified)

Every primitive a parcel-based land market needs already exists **and is
network-correct**:

| Need | Existing primitive | Synced |
|---|---|---|
| Own / permit a parcel, enforce on build | `rules.allow/forbid_*` → `forbidden_tools` (`scenario.h:179`), checked in every builder via `is_work_allowed_here` | yes — `nwc_scenario_rules_t` (`scenario.cc:413-417`) |
| Change ownership at runtime | same | yes |
| Move money buyer → seller | `book_cash` in the script API (`api_player.cc:273`, via `player_book_account`), routed as a command | yes |
| Player files a buy request | script-tool work command → server-side scenario | yes |
| Pending offers / request store | scenario `persistent` (server) + message display | server-side |

Two design questions that looked open are **decided by the grain**, not
by us:

- **Granularity = parcel/rectangle.** The permission system is rect-based
  (sorted-list / binary-search matching); per-tile fights it.
- **Transfer = command-driven.** Money and rule changes already travel as
  synced commands; no new transport.

## The single remaining decision

The permission machinery is **scenario-gated**: `is_work_allowed_here`
and friends early-return unless `what_scenario` is `SCRIPTED` /
`SCRIPTED_NETWORK` (`scenario.cc:570,605,732,1022`). That is the only
fork left:

- **Scenario context is acceptable** → build it as a **pure scenario
  script, zero engine changes**. Every primitive above is ready. This
  *is* the agnostic plan in PR #244, now proven sufficient (not assumed).
  The data-sender patch stays unnecessary — the buy request rides the
  command path; the scenario does `book_cash` + `rules.allow_*`
  server-side.
- **Must work in plain (non-scenario) multiplayer** → the *only* real
  engine work: lift or replicate the scenario gate so the permission
  check (and a build-time money/ownership hook) runs without a scenario
  loaded. Everything else is unchanged.

## Status

**Undecided** (the binary above is a product call, not a code call).
Until it's answered, the agnostic scenario plan in PR #244 stands as the
default, since it is the zero-engine-change branch and a strict subset
of the work either way.

## Out of scope (unchanged)

Running tool scripts in lockstep; a client-side VM-to-VM data pipe;
per-tile ownership. The engine stays as-is unless the plain-MP branch is
chosen.
