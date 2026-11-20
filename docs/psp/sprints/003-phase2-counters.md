# Sprint 003 — Backend counters that mean something

| | |
|---|---|
| Roadmap phase | [Phase 2 — Instrumentation](../phases/02-instrumentation.md) |
| Branch / base commit | `psp-6.2` @ `r3` build |
| Started / closed | — / — |
| Build number(s) | `r4` |
| Tasks | 0/7 done |

## 0. Handover

**Situation.** Raylib 6.2 runs on the PSP through pspGL, unbatched: every sprite is a bind, a `glBegin`, four
vertices, a `glEnd`, an unbind. Nothing in the tree counts any of that. Your job: counters you can trust, proven
against a workload whose answer is known in advance.

**Read before acting:**

1. [Memory map](../memory-map.md) §2 — specifically why `rlBegin` *is* `glBegin` on the GL11 path.
2. [Memory map](../memory-map.md) §4 — the two separate cost models this instrumentation must later tell apart.
3. [Phase 2 page](../phases/02-instrumentation.md).

**First action.** Decide where counters live (raylib side or pspGL side) and write the decision into this
sprint before writing code. The raylib side survives any backend; the pspGL side sees more detail but drags the
fork in early.

**Hard rules.**

- **The counter must be falsifiable.** On the unbatched path, 1000 sprites should produce ≈1000 submissions.
  If your counter says 1, it is counting draw *calls* after some merge you did not know about — that is a
  finding, record it.
- **Mutation test is mandatory**: disable the draw, submissions must drop to 0 while logical sprites stay at
  1000. If both drop, you are counting intent.
- Increment each counter in exactly one place.
- Logs are written open → write → close. A file's length is recorded on close, so a buffered line does not
  survive a crash.
- Counter overhead must stay under 0.2 ms per frame — measure it, do not assume.

**Do not touch.** Any rendering behaviour. This sprint measures the current path; it does not improve it. If
you find yourself optimising, stop and note it as a follow-up for phase 4.

**Done means** §8 below, including the mutation-test rows in §7.

## 1. Objective

Counters for draw submissions, vertices submitted, texture binds and state changes exist and are **proven
correct against a known workload** — on the unbatched GL11 path, submissions must scale with object count, so
1000 sprites should produce on the order of 1000 submissions. A counter that cannot be validated this way is
not instrumentation, it is decoration.

## 2. Why now

Phases 3 and 4 both claim to reduce submission count. Without a counter that has been checked against a
workload whose answer is known in advance, neither claim can be evaluated — and the first "improvement" would
be indistinguishable from the counter changing.

## 3. Scope

**In:** counter storage, increment sites, a way to read them out (on screen and in a log), bunnymark's
displayed figure.

**Out:** timing instrumentation (sprint 004), any rendering change, any pspGL modification.

## 4. Preconditions

- [ ] Phase 1 closed: five samples green
- [ ] A build that can print text on screen (raylib's own `DrawText`, or the debug screen)
- [ ] `host0:` writable, so counters can be committed to a file line by line

## 5. Tasks

| ID | task | status | verification | evidence |
|---|---|---|---|---|
| T1 | decide where counters live: raylib side (survives any backend) or pspGL side (sees more detail). | `todo` | — | — |
| T2 | count submissions at the point where a draw actually leaves the CPU | `todo` | with 1000 sprites on the immediate path, count ≈ 1000 (one quad each) → expected: this is the falsifiable check for the whole sprint | — |
| T3 | count vertices, texture binds, and state changes, each incremented in exactly one place | `todo` | binds ≈ sprites in bunnymark (texture set per sprite today); state changes ≈ 1 per run | — |
| T4 | add flush-reason counting when a flush exists (`texture change`, `mode change`, `capacity`, | `todo` | — | — |
| T5 | replace bunnymark's `1 + count/MAX_BATCH_ELEMENTS` with the real counter, or label the old figure | `todo` | — | — |
| T6 | read-out: one line per second to `host0:/counters.log`, open/write/close each time | `todo` | — | — |
| T7 | mutation test: deliberately disable the draw (early return) and confirm the submission counter | `todo` | — | — |

Status vocabulary: `todo` / `doing` / `done` / `blocked` / `dropped`. IDs are stable — never
renumber; a task that no longer applies is `dropped` with the reason in the log below.

### Task log (append-only)

- _(not started)_

## 6. Budgets

| budget | limit | measured | tag |
|---|---|---|---|
| counter overhead | < 0.2 ms per frame | | |
| counters per frame | ≤ 8 | | |

## 7. Measurements

| run | build | workload | metric | value | expected |
|---|---|---|---|---|---|
| `r4` | bunnymark 100 | submissions | ≈100 | 1 quad each, unbatched |
| `r4` | bunnymark 1000 | submissions | ≈1000 | the falsifiable check |
| `r4` | bunnymark 1000 | texture binds | ≈1000 | bind/unbind per sprite |
| `r4` | mutation | submissions | 0 | draws disabled |
| `r4` | mutation | logical sprites | 1000 | unchanged — this is the point |

## 8. Exit criteria

- [ ] the 1000-sprite check lands within a documented tolerance, or the discrepancy is explained
- [ ] mutation test shows the counter follows the draw, not the intent
- [ ] counters survive the exit path and are readable from a log file
- [ ] bunnymark's displayed figure no longer claims a batch that does not exist

## 9. Result

(To fill in on close. If submissions turn out *not* to be ≈ N — for example if rlgl already merges something —
that is a finding that changes phase 4's premise and must be written into the memory map.)

## 10. Follow-ups

- Timing (CPU per zone, GE wait) is sprint 004. Splitting counting from timing keeps each measurable on its own.
- If the counter home decision was "pspGL side", the pspGL fork becomes a build dependency earlier than phase 3
  planned — note that, it changes the ordering.
