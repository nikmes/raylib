# Phase 2 — Instrumentation and baseline

| | |
|---|---|
| State | not started |
| Depends on | Phase 1 |
| Exit criterion | two runs of the same build agree within 5 %, and every optimisation category has a baseline number |

## What this phase is about

Making the bottleneck measurable before touching it. Right now nothing in the tree counts draw submissions,
vertex traffic, texture binds, GE waits or frame CPU time, and the one counter that exists — bunnymark's
"batched draw calls" — is computed as `1 + count / MAX_BATCH_ELEMENTS`, which on the GL11 path is fiction.

## What has to be true before starting

- [ ] Phase 1 exit criteria met: five samples render on 6.2
- [ ] `reset` in `pspsh` establishes a clean partition (25,149,440 bytes free)
- [ ] A way to log from the running program that survives the frame (host0: file, or counters drawn to screen)

## Tasks

- [ ] Replace bunnymark's displayed batch count with a backend counter, or label it as logical-only
- [ ] Count at minimum: draw submissions, vertices submitted, texture binds, state changes, per-frame CPU time,
      GE wait time
- [ ] Decide where counters live — the raylib side survives any backend, the pspGL side sees more
- [ ] Run bunnymark at fixed counts (100, 1k, 5k, 10k, 20k) instead of free-running
- [ ] Record median and 95th-percentile frame time over a fixed sample window, not one number
- [ ] Archive the baseline build (`EBOOT.PBP` + sha256) so later "improvements" cannot be compared to a
      different binary
- [ ] Establish the repeatability floor: same build, two runs, how close? That number bounds every future claim

## What we already know

- The sample is vsync-locked, so frame time will sit near 16.7 ms until it can no longer hold 60 — free-running
  numbers need vsync off, or the measurement saturates
- `kill` in `pspsh` leaks the module's heap; `reset` restores the partition exactly. Measuring after a kill
  cycle measures a broken console
- PPSSPP's numbers are not admissible here at all — it does not run the ME, reports ~5 MB more free RAM, and
  its GE timing is unrelated to the device

## Exit criteria

- [ ] repeatability documented (the ±% two identical runs differ by)
- [ ] baseline table for every category in §6.2 of the performance plan at `/home/psp/raylib-performance-doc.md` (outside this site)
- [ ] counters survive the exit path (they are useless if a crash loses them)

## Sprints and features

- [Sprint 003 — Backend counters that mean something](../../sprints/003-phase2-counters.md) — **closed**: counters
  validated against a known workload (1,030.9 ends/frame at 1,000 sprites, 4.000 vertices per end) and by
  mutation (31.0 ends/frame with the draws removed). `src/` untouched; counting lives in the harness via
  `-Wl,--wrap`. Measured cost today: **92.5 µs per sprite**, ~181 sprites in a 16.7 ms frame
- [Sprint 004 — Timing, fixed-count runs, repeatability floor](../../sprints/004-phase2-baselines.md)
