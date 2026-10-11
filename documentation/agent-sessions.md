# Running a session with parallel agents

How the October 2026 cleanup sessions ran,
so a new session can pick up the same way of working.
A sketch, not a contract; AGENTS.md rules still apply to every change.

## Roles

One orchestrating session plans, reviews and merges.
Up to four subagents do the work in parallel;
the container has four cores, and more agents only contend on builds.
Subagents start on Sonnet;
work that needs judgment (upstream merge stages, design calls)
or a Sonnet attempt that failed goes to Opus at medium effort.
The orchestrator keeps its own context for review
and does small edits itself, but hands anything larger to an agent.

## Where work comes from

`TODO.md` is the complete backlog.
GitHub issues are copies of the large items:
`epic` for multi-PR work, `decision` for questions to the maintainer.
The maintainer answers decisions in issue comments,
so read those at the start of a session.
`documentation/upstream-125-merge.md` holds the staged upstream merge plan.
New backlog comes from read-only audits:
an agent sweeps one subsystem for a class of problem
(say, square-grid residue) and reports `file:function` findings;
the orchestrator groups them into PR-sized fix groups
and records the ones not fixed right away in `TODO.md`.

## One unit of work

One fix group is one branch `claude/<topic>` and one PR.
Each agent works in its own git worktree
(the Agent tool's worktree isolation, under `.claude/worktrees/`;
list that path in `.git/info/exclude`).
The prompt is self-contained:
what to fix and where, how to build and test,
commit identity and trailers, the branch to push,
and the shape of the final report.
Agents configure `build/` with ccache launchers
(`-DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache`)
and build with `-j 2` while other agents run.
`tools/test.py` keeps the engine's user dir under `build/`,
so worktrees testing at the same time don't repoint each other's links.
CI builds headless, where no images are registered:
tests assert on state, not images,
and a CI-only failure is reproduced with `-DSIMUTRANS_BACKEND=none`.
Agents commit, push the branch and report; they do not open PRs.

## Review and merge

The orchestrator reads each diff adversarially, reruns the tests,
opens the PR, subscribes to its activity and merges it once CI is green.
Ordinary PRs merge by rebase, which keeps history linear.
Upstream merges merge with a merge commit only:
rebasing or squashing rewrites upstream SHAs
and breaks the merge base for the next stage.
After every merge, check the other open branches against the new `main`
(`git merge-tree --write-tree origin/main <branch>`)
and merge `main` into any that conflict.
Deleting merged branches is the maintainer's call;
list them for the maintainer.

## Ordering

Upstream stages go in before fixes that rewrite code
the same stage also rewrites;
small local fixes elsewhere run in parallel with the stages.
Refactors without a hex reason are not carried
(AGENTS.md → "Diff against the upstream `simutrans` branch").

## Supervision

Agents run in the background and the orchestrator is woken
when each finishes.
A watcher over the agents' transcripts flags an agent
that is idle for minutes, stuck in one command, or failing repeatedly.
Stop and re-scope such an agent rather than letting it loop.
