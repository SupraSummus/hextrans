# Developing Simutrans in Claude Code on the web

This document describes how this fork is set up to be developed in
[Claude Code on the web](https://claude.ai/code), and how to compile and
test changes inside one of those remote sessions.

## What the session-start hook does

`.claude/hooks/session-start.sh` runs once when a remote Claude Code
session boots in this repo. It:

1. Detects the remote env via `$CLAUDE_CODE_REMOTE` (no-op locally).
2. Installs the build dependencies via `apt-get`:
   - Toolchain: `build-essential`, `cmake`, `pkg-config`, `autoconf`
   - Required libs: `libbz2-dev`, `libpng-dev`, `libfreetype-dev`,
     `libfontconfig-dev`
   - Backend: `libsdl2-dev` (so CMake selects the `sdl2` backend)
   - Optional: `libzstd-dev`, `libminiupnpc-dev`, `libfluidsynth-dev`
   - `moreutils` (for `ts` used by `tools/run-automated-tests.sh`)
3. Configures a CMake build dir at `build/` (`-DCMAKE_BUILD_TYPE=Debug`).

The hook is **synchronous**: the session does not start until apt and the
CMake configure step finish. That's slower at boot (~1–2 minutes the
first time, then cached) but guarantees `cmake --build` works
immediately and prevents Claude from racing the dependency install.

The hook is registered in `.claude/settings.json`. Once that file is on
the default branch, every future remote session in this repo picks it
up.

## Building

```sh
cmake --build build -j "$(nproc)"
```

The binary lands at `build/simutrans/simutrans`. Incremental builds are
cheap thanks to the cached build dir.

To force a graphics-less build (useful when you only care that it
compiles and don't need to run the GUI):

```sh
cmake -S . -B build -DSIMUTRANS_BACKEND=none
cmake --build build -j "$(nproc)"
```

The Makefile-based build (`autoconf && ./configure && make`) also works
inside a remote session, but CMake is what the hook primes.

## Linting

There is no separate linter. The compile itself with `-Wall` /
`-Wextra` (which CMake sets up via `cmake/SimutransCompileOptions.cmake`)
is the lint pass. Treat warnings as the lint signal — the squirrel/
vendored sources emit a lot of pre-existing fall-through warnings; only
worry about new ones in the Simutrans source under `src/simutrans/`.

## Running automated tests

This is the part that does **not** work out of the box in a remote
session and requires a manual setup step.

`tools/run-automated-tests.sh` (and the CMake `test` target) launch the
freshly built `simutrans` binary against the `automated-tests` scenario
defined in `tests/`. The scenario needs a **pakset** (graphics/data
files) installed at `simutrans/pak/`. Paksets are not part of this repo;
they're separate downloads (typically 30–100 MB) under their own
licences, distributed from the Simutrans website.

To run tests in a remote session you have to first install a pakset
into `simutrans/pak/` — for example:

```sh
# From inside the repo root, after the build:
mkdir -p simutrans/pak
# Download and unpack a pakset of your choice into simutrans/pak/.
# The pakset must be the one the tests were authored against; see
# tests/all_tests.nut for the features they exercise.
SDL_VIDEODRIVER=dummy ./tools/run-automated-tests.sh
```

`SDL_VIDEODRIVER=dummy` is required because remote sessions have no
display server; SDL falls back to a software renderer with no window.

The hook deliberately does **not** download a pakset because:

- Pakset choice is a project decision, not an env decision.
- They are large; bundling them would slow every cold session start.
- They have their own licences and update cadence.

If we settle on a canonical pakset for CI use we can teach the hook to
fetch it on demand (cached in the container).

## Smoke-checking the binary without a pakset

You can verify the binary at least starts up:

```sh
SDL_VIDEODRIVER=dummy timeout 5 \
    ./build/simutrans/simutrans -use_workdir 2>&1 | head
```

It will warn about missing pak data and then idle on the pakset
selection screen — that's the expected behaviour without a pakset.

## Switching the hook to async

The hook currently runs synchronously. To trade boot latency for the
risk of Claude racing the install, change the first lines of
`.claude/hooks/session-start.sh` to:

```bash
echo '{"async": true, "asyncTimeout": 600000}'
```

(See `~/.claude/skills/session-start-hook` for the contract.) Don't do
this until you're comfortable with the race — the first cold session
will otherwise have Claude trying to invoke `cmake --build` before
`cmake` is installed.
