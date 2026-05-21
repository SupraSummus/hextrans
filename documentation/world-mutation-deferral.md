# World-mutation re-entrancy and tool dispatch

GUI event handlers run from `INT_CHECK`, which calls
`interrupt_check` (`simintr.cc:97`) which calls
`karte_t::sync_step` which calls `eventmanager->check_events()` —
so when a player clicks a button, the click handler is on a stack
that may already contain `karte_t::step` mid-iteration over
`fab_list`, `convoi_array`, `cities` or halts.  A handler that
synchronously calls `welt->init()` / `welt->load()` /
`karte_t::destroy()` frees objects the step is still iterating —
heap-use-after-free.  Forum thread 23913 (janry, 2026-05) and ASAN
both surface the canonical case: `welt_gui_t`'s Start button
calling `welt->init()` from inside its handler.

## Dispatch through the tool framework

World-mutating GUI actions fire a `tool_t` whose
`is_init_keeps_game_state()` returns `false` (the tool changes
state).  `karte_t::set_tool_api` (`world/simworld.cc:2288`) gates:

```cpp
if (tool_in->is_init_keeps_game_state()
    || (get_random_mode() & INTERACTIVE_RANDOM) == 0) {
    local_set_tool(tool_in, player);  // run now
}
else {
    command_queue_append(new nwc_tool_t(...));  // queue
}
```

`INTERACTIVE_RANDOM` is set around `check_events()` inside
`karte_t::sync_step` (`world/simworld.cc:2787-2791`), so any tool
fired from event dispatch lands in the `else` branch — queued
onto `command_queue`, drained at the top of the next
`karte_t::interactive()` loop iteration (`world/simworld.cc:6346`).
By the time the queued tool's `init()` runs, the step that
originated the click has returned; no iteration is on the stack
and `welt->init()` / `welt->load()` are safe to call.

Tools used this way carry `WFL_LOCAL | WFL_NO_CHK`: local-only
single-player control flow, not game state to replay across the
network or gate behind scenario rules.

## Tools currently using the pattern

`tool_quit_t` fires from `banner_t::show_banner` and the options
dialog; it sets `finish_loop` and lets outer `simmain.cc` rebuild
the world.  `tool_load_scenario_t` fires from
`scenario_frame_t::load_scenario` and loads a scenario.
`tool_new_world_t` fires from `welt_gui_t`'s Start button and
covers both the procedural and the heightfield branches,
discriminated by whether `env_t::default_settings.heightfield` is
empty.  `tool_load_world_t` fires from `loadsave_frame_t`'s Load
button with `default_param = "<easy_server>,<filename>"`.

## Residual outlier

`server_frame_t::action_triggered` (`gui/server_frame.cc:519,528`)
calls `welt->load("net:...")` inline when the player joins a
multiplayer server.  Same hazard, same fix shape as
`tool_load_world_t` — either reuse it (the "net:" prefix is
already handled inside `welt->load`) or add a sibling tool.  Lands
when somebody touches the multiplayer join path next; gated on
nothing else.

## Why a tool and not an ad-hoc deferral flag

A per-site `bool karte_t::pending_*` flag works for one call site
but doesn't compose: each new site needs its own flag, its own
dispatch block in `interactive()`, and its own ordering relative
to other deferrals.  The tool framework already provides FIFO
dispatch (`command_queue`), already has a registered slot
(`tool/simmenu.h::SIMPLE_TOOL_COUNT`), already integrates with
network replay for callers that need it, and already has multiple
in-tree precedents.  One mechanism, not a patchwork.
