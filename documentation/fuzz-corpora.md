# Fuzz corpora

What lives in `src/fuzz/corpus/`, why, and what does not.  The
committed corpus is a **regression-and-wiring gate**, not a coverage
corpus.  CI replays it in one-shot mode (`-runs=0`, no mutation) under
ASAN/UBSAN on every push; any input that crashes, leaks, or trips a
sanitizer fails the job.  The large evolving corpus that a mutation
campaign produces is deliberately *not* committed — see "Out-of-band"
below.

One subdirectory per harness: `nettool/`, `pak/`, `command_preauth/`,
`savegame/` (see "Savegames" below).
The harnesses themselves are documented in `AGENTS.md` (search
"fuzz_").

## Two committed categories

**Generated seeds** (`smoke_*`).  Well-formed inputs, one per command
/ node / dispatch arm, emitted by `src/fuzz/corpus/gen_smoke_seeds.py`.
Their job is twofold: prove the harness still builds, initialises, and
dispatches on every push (a build or wiring regression fails the
replay even with no bug present), and give an active campaign a
well-formed starting point so the mutator reaches deep handler code
instead of bouncing off the header parse.  They are *not* bug
reproducers and do not pin a specific defect.  The bytes of each seed
are spelled out and commented in the generator, so a seed stays
readable and regenerable rather than an opaque blob.

**Minimized reproducers** (named after the bug — `oom_image_len`,
`double_free_shared_image_child`, `xref_unterminated_name`, …).  One
file per fixed bug, committed **in the same commit that fixes the
bug**, produced by `-minimize_crash=1 -runs=... <crash-input>`.  Each
pins one defect so the replay fails if it ever regresses.  The git
log of any such file points at its fix commit; that is the record of
what it guards.

Nothing else is committed.  In particular, the corpus is **not** a
coverage corpus: a file earns its place by pinning a regression or by
seeding a command, not by adding a coverage edge.

## Out-of-band: the evolving mutation corpus

Active mutation (`-max_total_time=N <corpus-dir>`, or AFL++/Centipede)
grows a working corpus of thousands of inputs.  That set is kept out
of the repo: it churns on every campaign, its files are opaque, and it
adds no regression value the minimized reproducers don't already give.
This matches standard libFuzzer / OSS-Fuzz practice, where the
coverage corpus lives in a bucket or queue dir, not in source control.
Run campaigns from a scratch copy of the committed seeds; commit back
only a finding, never the grown corpus.

## When a campaign finds something

*A crash, leak, or sanitizer hit* → fix the bug, then commit one
minimized reproducer named after it alongside the fix.  This is the
only path by which a non-seed file enters the corpus.

*A whole command or code path the seeds never reach* (a structural
gap, not a single interesting input) → close it in
`gen_smoke_seeds.py` as a new documented seed, not as a committed
mutated blob.  The transposed-id fix that revived the dead `nwc_tool_t`
seeds is the worked example: the gap was in the generator, so the fix
was in the generator.

*A single coverage-distinct input that is neither a bug nor a
structural gap* → do not commit it.  It does not pin a behaviour, so
in the `-runs=0` gate it is only one more file to replay for a
coverage delta the next engine change may erase.  Leave it in the
out-of-band corpus.

## Leak detection

The three parser harnesses replay under `detect_leaks=1` in CI, and each is
built to stay leak-clean per iteration so a leak in replay is a real
regression (`fuzz_pak` tears down the descriptor DAG;
`fuzz_command_preauth` resets `socket_list_t`, and its fatal hook throws
into an exception-safe `read_from_packet` so a rejected input unwinds
leak-clean — see `AGENTS.md`).  Active mutation
campaigns run with `detect_leaks=1` too, except `fuzz_command` (the
in-process world harness), whose per-rebuild freelist retention is
reachable, not a pluggable leak, so it runs `detect_leaks=0` — the
measurements are in `TODO.md`.
`fuzz_savegame` replays with `detect_leaks=0`:
the engine standup leaves allocations LeakSanitizer reports at exit,
and with one input per process there is no per-input leak to find.

## Savegames

`savegame/` holds gzipped save files,
which `fuzz_savegame` reads the way the game does.
The seed `ai_soak.sve` is a 32×32 world
with cities, industry and both AIs' lines and convoys,
saved by `-ai_soak 32 1 -until 1931.3` (see `AGENTS.md`);
CI also replays the `tests/*.sve` fixtures.
A reproducer is the shortest prefix of the crashing save
that still crashes at the same place, gzipped.
The loader reads a save front to back,
so the bytes after the crash do not matter,
and a fixed build stops at the end of the prefix with a fatal.

To mutate, unpack the seeds,
since mutated gzip bytes rarely get past the decompressor,
and run the AFL++ build of the harness:

```sh
mkdir -p in && for f in src/fuzz/corpus/savegame/*.sve tests/*.sve; do
  zcat "$f" > in/"$(basename "$f" .sve)"; done
SAN="-fsanitize=address,undefined -fno-sanitize-recover=all \
  -fno-sanitize=signed-integer-overflow,shift,function"
cmake -S . -B build-fuzz-afl -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_C_COMPILER=afl-clang-fast -DCMAKE_CXX_COMPILER=afl-clang-fast++ \
  -DSIMUTRANS_BUILD_FUZZERS=ON -DSIMUTRANS_BACKEND=none \
  -DCMAKE_C_FLAGS="$SAN" -DCMAKE_CXX_FLAGS="$SAN" \
  -DCMAKE_EXE_LINKER_FLAGS=-fsanitize=address,undefined
cmake --build build-fuzz-afl --target svnheader fuzz_savegame
AFL_FUZZER_LOOPCOUNT=1 SIMUTRANS_FUZZ_BASE=$PWD/simutrans \
ASAN_OPTIONS=detect_leaks=0:abort_on_error=1:symbolize=0 \
  afl-fuzz -i in -o out -m none -t 2000 -- build-fuzz-afl/src/fuzz/fuzz_savegame
```

The campaign leaves out the signed overflow check:
a save picks every number,
and sums such as the finance history totals overflow without touching memory.
That is why the sanitizers are set by hand;
`SIMUTRANS_SANITIZE_UNDEFINED` would add the check back after them.
CI's replay keeps it,
so a reproducer that overflows on the way to its bug
needs that value set back in range by hand.
A save loads in well under a second;
one that runs into `-t 2000` asks for a huge map.
