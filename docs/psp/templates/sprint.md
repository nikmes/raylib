# Sprint template

> Copy this file to `docs/sprints/NNN-<slug>.md`. Fill every section; delete the guidance lines.
> A sprint is a time-boxed slice of **one** roadmap phase. If it spans two phases, split it.

| | |
|---|---|
| Roadmap phase | <e.g. Phase 4 — PSP rlgl batcher> |
| Branch / base commit | `psp-6.2` @ `<sha>` |
| Started / closed | YYYY-MM-DD / — |
| Build number(s) | <e.g. b7, b8> |
| Tasks | 0/0 done |

## 0. Handover

> Written for whoever picks this up with **no chat history** — a coding agent, a future you, a colleague. If a
> paragraph only makes sense to someone who was in the conversation that produced it, rewrite it. Five minutes
> of writing here saves an hour of rediscovery.

**Situation.** <Where the tree is, what already works, what this sprint adds. Three sentences maximum.>

**Read before acting**, in this order:

1. <Memory map> `../memory-map.md` §<n> — <why this section, specifically>
2. <Phase page> `../phases/<file>.md` — <the exit criterion this sprint serves>
3. <the exact file, commit or reference directory this sprint starts from, with the command to get it>

**First action.** <One concrete command or edit. Never "get familiar with the code".>

**Hard rules.**

- <the constraint that, if broken, silently produces a plausible but wrong result>
- <the licence / portability / measurement rule that applies to this sprint in particular>

**Commands.**

```sh
# environment, build, run — copy-pasteable, absolute paths
```

**Do not touch.** <files or areas outside scope, so scope creep is visible in the diff>

**Done means** §8 below, with evidence in each task row.

## 1. Objective

One paragraph, falsifiable. Not "make sprites faster" but "a run of N sprites sharing one texture produces
at most `ceil(N/1024)` draw submissions, and screenshot output is identical to the unbatched path."

## 2. Why now

Which exit criterion of the phase this advances, and what it unblocks for later phases.

## 3. Scope

**In:** the changes this sprint is allowed to touch.

**Out:** what is deliberately not being done, with the phase it belongs to instead.

## 4. Preconditions

- [ ] `pspsh` answers, USB device is `054c:01c9`
- [ ] `reset` done; partition 2 free reads 25,149,440 bytes
- [ ] Counters exist for what this sprint measures: <list them>
- [ ] Reference build archived: `<path>`, sha256 `<hash>`
- [ ] Baseline captured in §7 **before** any code change
- [ ] Sample builds with `PSP_LARGE_MEMORY=0`

## 5. Tasks

Status vocabulary — use exactly these words, in backticks, so the set stays auditable with a single grep:

`todo` · `doing` · `done` · `blocked` · `dropped`

| ID | task | status | verification | evidence |
|---|---|---|---|---|
| T1 | <the change, concretely> | `todo` | <how it is checked> → expected: <the number it should move> | — |
| T2 | | `todo` | | — |
| T3 | | `todo` | | — |

Rules:

- **IDs are stable.** Never renumber; a task that no longer applies becomes `dropped` with the reason in the
  log, so the history stays readable.
- **`done` requires evidence.** Its verification has to have been run and recorded (commit sha, measurement
  row, screenshot path). Code written is `doing`.
- **`blocked` needs a reason and a dependency** — "blocked by T3" or "blocked: pspGL rejects GL_LUMINANCE",
  not just the word.
- **Commit messages carry the ID**: `sprint 001 T4: add PLATFORM_PSP block to src/Makefile`. Then
  `git log --grep='T4'` recovers the work, and the sprint doc never becomes the only record.

### Task log (append-only)

One line per status change, oldest first. This is *when* and *why*; the table above is *now*.

- YYYY-MM-DD — sprint opened, all tasks `todo`
- YYYY-MM-DD — T1 `doing`
- YYYY-MM-DD — T1 `done`, `<sha>`

## 6. Budgets

| budget | limit | measured | tag |
|---|---|---|---|
| frame time | ≤ 16.7 ms (60 fps) | | |
| main-CPU render time | | | |
| heap | | | |
| VRAM | | | |

## 7. Measurements

Protocol: device only; one variable per run; control → change → control again. Every row carries its build
number. A screenshot without a build number is not evidence.

| run | build | workload | metric | value | notes |
|---|---|---|---|---|---|
| control | | | | | |
| change | | | | | |
| control again | | | | | |

## 8. Exit criteria

- [ ] the objective in §1 is met, with numbers in §7
- [ ] all five samples still render; screenshots archived
- [ ] no per-frame stall or wait introduced (check the counters, not the feel)
- [ ] new facts written back into `memory-map.md` §2/§3/§9

## 9. Result

What happened, including what failed and what was abandoned. If a hypothesis was wrong, say so here and
correct the place that carried it.

## 10. Follow-ups

Discovered but out of scope, each tagged with the phase it belongs to.
