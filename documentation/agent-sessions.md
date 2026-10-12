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
New backlog comes from read-only audits:
an agent sweeps one subsystem for a class of problem
(say, square-grid residue) and reports `file:function` findings;
the orchestrator groups them into PR-sized fix groups
and records the ones not fixed right away in `TODO.md`.
The orchestrator keeps its ordered queue of fix groups in a scratch file,
so the order survives a context compaction;
a merge or an agent report that unblocks a `TODO.md` entry
adds that entry to the queue.
When the queue runs short, a read-only triage agent
classifies every `TODO.md` entry as actionable now, blocked, stale or epic,
and proposes the next fix groups;
the stale ones are removed in a small PR of their own.

## One unit of work

One fix group is one branch `claude/<topic>` and one PR.
Each agent works in its own git worktree
(the Agent tool's worktree isolation, under `.claude/worktrees/`;
list that path in `.git/info/exclude`).
The prompt is self-contained:
what to fix and where, how to build and test,
commit identity and trailers, the branch to push,
the open PRs and what they fix
(so an agent ports a fix that exists instead of writing it again),
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
Before reporting, an agent reads its own diff with fresh eyes:
right direction?
elegant and streamlined, in the tests too?
Do the tests carry value? (No test beats a trivial one.)
Will a future dev be confused here,
or think "tldr" at the comments, docs or `TODO.md` text?
Does it open a refactor (do it if small, else one `TODO.md` line)?
Push further, wrap up, or revert?
The orchestrator asks the same questions in review.

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

Test what lands, not what was branched.
When `main` moved under an open PR in a way that touches the same code
(an upstream stage, a change to the same subsystem),
rebase the orchestrator's or agent's own branch onto `main`
and let CI run again before merging;
an upstream-stage branch gets `main` merged into it instead,
since rebasing would rewrite upstream history.
Agents rebase onto the latest `main` before their final test run.
Read the `TODO.md` edits in each diff against the backlog already on `main`:
an agent branched before an entry landed may add it a second time.
A clean `git merge-tree` against `main`
and changes in different functions are not the same code:
such a PR merges on the CI run it already has.
Check the open branches against each other too, not only against `main`:
two PRs that each merge cleanly can still conflict,
typically both adding a helper at the same place in `koord.h`
or a CI step at the same place in the workflow.
Merge the first one green and rebase the other, keeping both sides.
When a rebase brings in another PR's new code,
read that code for the problem the rebased branch fixes
(a whole-map loop added while the map-shape loops were in review).
A CI step that runs every test in another mode
(the suite on a map turned by 180°, `tools/test.py --rotate`)
reaches every PR's new tests:
before merging, rebase each open PR onto `main`, run that mode too,
and list a test that cannot pass there yet in its pending file
(`tests/rotate-pending.txt`) under the class that explains why.
Agent prompts carry the same rule.
A test that passes or fails with the game time it starts at
is fixed at its cause, not moved in `tests/all_tests.nut`.

## Ordering

Upstream stages go in before fixes that rewrite code
the same stage also rewrites;
small local fixes elsewhere run in parallel with the stages.
Consecutive upstream stages pipeline:
stage N+1 branches from stage N's branch while stage N is in review,
and the merge commit keeps the shared upstream history identical.
A multi-step plan pipelines the same way:
step N+1 branches from step N's branch while N is in review,
and once N merges the orchestrator tells the agent to rebase onto `main`,
which drops N's commits.
Refactors without a hex reason are not carried
(AGENTS.md → "Diff against the upstream `simutrans` branch").

## Supervision

Agents run in the background and the orchestrator is woken
when each finishes.
A watcher over the agents' transcripts flags an agent
that is idle for minutes, stuck in one command, or failing repeatedly.
Stop and re-scope such an agent rather than letting it loop.

A usage limit or a restart can stop every agent at once.
Prompts tell agents to commit as they go:
a worktree with commits survives and the agent resumes from its transcript
when sent a message,
while one with only uncommitted work may be cleaned up.
The orchestrator keeps one worktree with a build of its own
for rebasing open PRs and rerunning the extra suites locally.
