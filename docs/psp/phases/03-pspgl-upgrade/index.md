# Phase 3 — pspGL upgrade A/B

| | |
|---|---|
| State | not started |
| Depends on | Phase 2 (counters and a repeatability floor) |
| Exit criterion | a quantified delta between the shipped and the upgraded pspGL, and a recorded decision to adopt or not |

## What this phase is about

The first optimisation we can try is one we did not write. The `libGL.a` in pspdev v20261101 is built from
**`de4260ad`, dated 2022-03-23** — four years old — while `TheMrIron2/pspgl` carries eleven commits on top of
that exact commit: aggregate profiling infrastructure, configurable command-list capacity, corrected profiling
accounting, cached texture-matrix adjustment, exposed raw depth range and presented framebuffer.

`pspdev/pspgl`'s own HEAD is only base + 2 commits, both CI/build. So the fork is where the work is.

## What has to be true before starting

- [ ] Phase 2 baselines exist for bunnymark at fixed counts
- [ ] The baseline binary is archived with its sha256
- [ ] A way to rebuild `libGL.a` against pspdev is reproducible (not a manual one-off)

## Tasks

- [ ] Build `TheMrIron2/pspgl` HEAD against pspdev; produce a drop-in `libGL.a`
- [ ] Keep the shipped library so the A/B can be reversed without a rebuild
- [ ] Same raylib binary configuration, same workload, same clocks: control → upgraded → control again
- [ ] Run bunnymark at 100 / 1k / 5k / 10k / 20k with the phase 2 counters
- [ ] Capture the new profiling counters where the fork provides them
- [ ] Record what changed and what did not — including "no measurable difference", which is a valid result
- [ ] Decide: adopt the fork as our pspGL, keep the package, or take individual commits

## What we already know

- The two branches share `de4260ad` as merge base, so the delta is exactly 11 commits, not a rewrite.
- The fork's additions are profiling and capacity, not a different rendering model — expect better
  observability more reliably than a large frame-time win.
- pspGL currently exports 178 `gl*` symbols and the headers declare `GL_SPRITES_PSP`, `glLockArraysEXT`,
  `glPrioritizeTextures` — the fast paths phase 4 wants are already in the interface.

## Exit criteria

- [x] delta table with control and control-again, at more than one bunny count: N=1000 and N=5000, two runs per
      arm, control within 0.03 % of shipped
- [x] decision recorded in [§8](../../memory-map.md#8-decision-log) with its evidence
- [x] adopted, and the build is reproducible via `tools/pspgl-build.sh` (content hash `2ab7378e…`); licence BSD-3

**Result: −31.8 % on 2D sprite work.** −10.0 % from the fork's 11 commits (90.29 → 81.24 ms at N=1000), then
−24.1 % from `PSPGL_DLIST_SIZE_WORDS=8192` (81.24 → 61.63 ms). 86.5 → **58.3 µs per sprite**; ~287 sprites fit a
16.7 ms frame instead of ~181. Costs 512 KiB of main RAM for the command lists.

## Sprints and features

- [P3-S1 — Sprint 005 — pspGL from source, and an A/B that means something](../../sprints/005-phase3-pspgl-ab.md) —
  **closed**: fork HEAD is **10 % faster** (81.24 vs 90.30 ms at N=1000, 392.06 vs 436.43 at N=5000), control
  arm within 0.03 % of shipped so the recipe is neutral, symbol set a strict superset. Its profiler also showed
  257 submissions/frame and 2,725 µs/frame of command-list wait
- [P3-S2 — Sprint 006 — Adopt the fork, reproducibly](../../sprints/006-phase3-decide.md) — the decision is taken by
  sprint 005's data, so this sprint makes it durable: `tools/pspgl-build.sh` pinned by commit and sha256, the
  command-list size experiment the profiler motivated, the baseline re-published on the adopted library, and the
  restore path walked
