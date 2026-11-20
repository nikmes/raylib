# Sprint 004 — Timing, fixed-count runs, repeatability floor

| | |
|---|---|
| Roadmap phase | [Phase 2 — Instrumentation](../phases/02-instrumentation.md) |
| Branch / base commit | `psp-6.2` @ `r4` build |
| Started / closed | — / — |
| Build number(s) | `r5` |
| Tasks | 0/8 done |

## 0. Handover

**Situation.** Counters from sprint 003 work. What is missing is timing: how long a frame takes, at known
workloads, repeatably. Your job: a published baseline table plus the **repeatability floor** — how far two runs
of the identical build differ.

**Read before acting:**

1. [Memory map](../memory-map.md) §6 — the measurement protocol. Follow it literally.
2. [Memory map](../memory-map.md) §4 — the CPU-versus-GE distinction the data has to separate.
3. Sprint 003's result — the counters you will be reading.

**First action.** Measure the floor before measuring anything else: run the same build twice at one bunny count
and compute the spread. That number is the threshold below which every later "improvement" is noise.

**Hard rules.**

- **The sample is vsync-locked.** Throughput runs need `SetTargetFPS(0)` and vsync off, or every count reports
  16.7 ms and the table is meaningless. Do a separate vsync-on pass for pacing.
- Fixed bunny counts only. Pressing CROSS a number of times is not reproducible; make the count a build value.
- Device only. PPSSPP answers "does it render", never "how fast".
- `reset` before each session; archive the exact binary with its sha256 — an improvement measured against a
  different binary is not an improvement.
- PSU on AC, brightness fixed, no USB traffic during a timed run.

**Do not touch.** Rendering, counters' semantics, or any optimisation. If the table suggests where the wall is,
that belongs in §9 and phase 3, not in this sprint's commits.

**Done means** §8 below, and `docs/psp/baselines.md` published with the protocol, the hardware, the clocks and
the repeatability number.

## 1. Objective

A published baseline table: frame-time percentiles for bunnymark at 100 / 1k / 5k / 10k / 20k bunnies, with the
**repeatability floor** measured — how far two runs of the identical build differ — so every later claim has a
threshold below which a difference is noise.

## 2. Why now

Phase 2's exit criterion is exactly this table. Phase 3's A/B and phase 4's batcher both need it: without a
repeatability number, a 3 % "win" cannot be told from run-to-run variance.

## 3. Scope

**In:** CPU-side frame timing, fixed bunny counts instead of free-running, vsync-off measurement, the baseline
table, the archived baseline build.

**Out:** GE-side timing (needs a backend that can fence), pspGL counters (phase 3), any optimisation.

## 4. Preconditions

- [ ] Sprint 003 closed: counters validated
- [ ] A way to fix the bunny count without pressing buttons (`MAX_BUNNIES` / spawn count as a build-time or
      config value — pressing CROSS a fixed number of times is not reproducible enough)
- [ ] `SetTargetFPS(0)` and vsync disabled for the throughput runs, and a separate vsync-on run for pacing
- [ ] PSU on AC, screen brightness fixed, no USB traffic during a timed run

## 5. Tasks

| ID | task | status | verification | evidence |
|---|---|---|---|---|
| T1 | instrument the frame: CPU time per frame around `EndDrawing`, and a histogram, not an average | `todo` | — | — |
| T2 | make the bunny count a build parameter, run 100 / 1k / 5k / 10k / 20k | `todo` | — | — |
| T3 | run each count twice, same build, same session, and compute the spread | `todo` | — → expected: if two identical runs differ by more than ~10 %, say so — it sets the bar for "measurable" | — |
| T4 | record median, 95th percentile and worst frame over a fixed 30 s window per count | `todo` | — | — |
| T5 | a vsync-on pass at 1k and 5k, to document frame pacing separately from throughput | `todo` | — | — |
| T6 | archive the baseline build (`raylib.prx` + sha256) and the exact build command | `todo` | — | — |
| T7 | publish `docs/psp/baselines.md` with the table, the hardware, the clocks and the protocol used | `todo` | — | — |
| T8 | record what the data already suggests: at which bunny count does the frame stop fitting in | `todo` | — | — |

Status vocabulary: `todo` / `doing` / `done` / `blocked` / `dropped`. IDs are stable — never
renumber; a task that no longer applies is `dropped` with the reason in the log below.

### Task log (append-only)

- _(not started)_

## 6. Budgets

| budget | limit | measured | tag |
|---|---|---|---|
| sample window | ≥ 30 s per count | | |
| runs per count | 2 (control + repeat) | | |

## 7. Measurements

| count | median ms | p95 ms | worst ms | submissions | notes |
|---|---|---|---|---|---|
| 100 | | | | | |
| 1,000 | | | | | |
| 5,000 | | | | | |
| 10,000 | | | | | |
| 20,000 | | | | | |

Repeatability: run A vs run B at 1,000 bunnies — difference of ___ %.

## 8. Exit criteria

- [ ] the table is published with build number and sha256 of the binary used
- [ ] repeatability floor stated as a number, and adopted as the threshold for future claims
- [ ] the "wall" question answered as far as the data allows (CPU-bound, GE-bound, or not yet separable)
- [ ] Phase 2 exit criteria recorded in the phase page

## 9. Result

(To fill in on close. The most valuable outcome is a negative one: if bunnymark at 1,000 is already GE-bound,
phase 4's submission batching will not show up on that workload, and the baseline saves a week of chasing it.)

## 10. Follow-ups

- If CPU-bound at low counts and GE-bound at high counts, the crossover belongs in the memory map as a fact.
- pspGL-side counters (phase 3) would explain the gap the CPU timer cannot see.
