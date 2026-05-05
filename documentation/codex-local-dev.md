# Developing Simutrans with Codex on a local checkout

Companion to `claude-code-web-dev.md`.  That doc covers the remote
Claude Code on the web setup; this one covers the bits that only matter
when running OpenAI Codex (or any agent) against a local clone with the
graphics binary already built.

## Local headless quick recipe

In a local checkout where pak64 is already available at `simutrans/pak/`
and the CMake binary is `build/simutrans/simutrans`, wire the scenario
through Simutrans' user addon path, then run headless:

```sh
mkdir -p ~/simutrans/addons/pak/scenario
ln -sTf "$(pwd)/tests" \
    ~/simutrans/addons/pak/scenario/automated-tests

cd simutrans
ASAN_OPTIONS="print_stacktrace=1:abort_on_error=1:detect_leaks=0" \
UBSAN_OPTIONS="print_stacktrace=1:abort_on_error=1" \
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy \
timeout 240s ../build/simutrans/simutrans \
    -use_workdir -objects pak -lang en -scenario automated-tests \
    -addons -debug 2 -nosound -mute
```

A successful run prints `[N/M] test_...` lines and ends with `Tests
completed successfully.`  The game keeps running after that when
started directly; stop it with `pkill -f
"build/simutrans/simutrans.*automated-tests"` or use
`tools/run-automated-tests.sh` once its prerequisites are present.

## Prerequisites that bit us

`tools/run-automated-tests.sh` expects a repo-root `./sim`; the CMake
build does not create one.  Use `ln -sf build/simutrans/simutrans sim`
if you want to run the wrapper.

`-use_workdir` still looked for addon scenarios under
`~/simutrans/addons/pak/scenario`, not the repo-local
`simutrans/addons/...`, so linking only inside the checkout made the
scenario fail to compile.

Without `SDL_AUDIODRIVER=dummy -nosound -mute`, startup emitted
`pa_write() failed while trying to wake up the mainloop` and did not
reach the useful scenario output in this session.

In sandboxed terminal environments, Simutrans may be blocked from
writing user logs/settings and autosaves (`script-scenario.log`,
`settings.xml`, `autosave-pak.sv_`).  Run the scenario command in an
unsandboxed shell, or approve the command if the environment supports
per-command write escalation.

ASAN leak detection must stay disabled (`detect_leaks=0`), matching
CI; otherwise LeakSanitizer can abort before the scenario signal is
useful.
