# Sprint 096 (P10-S26) — Merge-path economy: index cache, static strips, expansion cost

| | |
|---|---|
| Roadmap phase | [Phase 10 — 3D and advanced](../phases/10-3d-advanced/index.md) |
| Sprint | P10-S26 |
| Branch / base commit | `master` @ `33e7f18f` — decision: [redesign §8](../phases/10-3d-advanced/pspgu-redesign.md) |
| Started / closed | 2026-11-28 (written) / 2026-11-28 (closed) |
| Build number(s) | pspgu `b38` only if a driver change is adopted; the index cache is a driver + raylib change |
| Tasks | 5/6 — T1, T2 (a), T4, T5, T6 done; T2b/T3 deferred to the ceiling sprint |

## 0. Handover

The 3D redesign was written on M3D's evidence that a thin per-object draw beats CPU expansion. After 090, 092 and
091 T4b it has not:

| path | cost |
|---|---|
| **merged (shipped default)** | **~55 µs/cube**, 32.6 ms work at 400 cubes |
| per-object, thinned | ~95 µs/call |
| per-object with *all* remaining surface removed | ~63 µs/call |

The [decision](../phases/10-3d-advanced/pspgu-redesign.md#8-decision-2026-11-28-merge-first-per-object-opt-in) is
therefore **merge-first**: merging stays the architecture, the per-object path stays opt-in for content the merge key
cannot express, and effort moves to the merge path's own costs — all of which already have numbers:

- **index-run copy**: 31 µs per 1,000 indices, 14,400 per frame ≈ **0.45 ms** (085 T3b);
- **strips for static meshes**: **−1.073 ms GE (−58 %) at 0 px** on a vertex-bound mesh (085 T3a);
- **merge expansion**: the CPU cost that makes merging 55 µs/cube, never attributed line by line.

## 1. Objective

Reduce the merged path's per-frame CPU and GE cost with three measured items, and settle the last open question of
the redesign — whether the per-object surface is removable enough to matter — with one run.

## 2. Why now

The shipped frame is the merged one; every per-object win since 090 is latent. This is the first sprint in the phase
that targets the path the demos actually use.

## 3. Scope

**In:** the surface breakdown that can reverse the decision (T1); the index-run cache (T2); load-time stripification
for meshes flagged static (T3); a line-by-line attribution of the merge expansion, and one measured improvement if the
attribution names one (T4); gates and pair (T5); close (T6).

**Out:** further per-object thinning (091 T4a stays open and parked), submission, 2D, offline asset conversion.

## 4. Preconditions

- [x] the decision is recorded and the reversal rule is explicit (redesign §8)
- [x] `CALLPROBE` reports the split (091 §9.1-9.4)
- [x] the index-copy cost and the strip win are measured (085 §9.3)
- [ ] the merge expansion has a probe; T4 builds it before claiming anything

## 5. Tasks

| # | Task | Status | Acceptance | Result |
|---|---|---|---|---|
| T1 | **The last breakdown.** One probe run splitting the ~48 µs of unexplained per-object surface (pointer/state, push/pop, texture enable, DrawMesh bookkeeping). No code change unless a counter is missing | `done` | the 48 µs is allocated or declared unallocatable; **≥30 µs removable ⇒ stop the merge-first decision and re-open the thin entry** | §9.1: `state` 14.5 µs (≈10.6 real), below the bar; merge-first stands |
| T2 | **Index-run cache.** Cache a run's indices in a persistent pool keyed by `(pointer, count, first)`, written back once; a key change is a miss; the pool never overwrites a block whose list is in flight | `done` (T2a); T2b deferred | copies/frame fall; the 3D showcase keeps correct pixels; hit/miss counters; `arenaOverflows=0` | §9.2: the raylib-side reuse is on by default and fires — `PSP: merged index run reused 400 frames in a row (ic=36)`. The *persistent driver-side* copy (T2b) is deferred to the ceiling sprint with the copy removal (§9.6). Its pair value is **not reproducible across sessions**: §9.7 |
| T3 | **Static-mesh strips.** Stripify at load for meshes flagged static (not the merge path), draw `GU_TRIANGLE_STRIP`, and measure against the list on a mesh-heavy scene | `todo` | 0 diff; strip vertex count and GE time reported; if it cannot be used by the merge path, say so and record the ceiling | — |
| T4 | **Expansion attribution.** Probe the merge expansion line by line (transform, pack, arena copy, index rebase) and improve the largest line only if one dominates | `dropped` | attribution sums to the measured expansion ±10 %; one measured change or none | §9.3: the normal `sqrtf` is not the cost — GRID=8 work 6.147 → 6.251, and the GRID=20 win was a split artifact |
| T5 | **Gates + pair.** Full pixel table, the pair below a vblank step, two runs per point | `done` | 613 / 284 / 0 / 0 / clip; `work` reported; no regression | 2026-11-28, shipped build: `batch_check` **613**, `basic_shapes` **284**, checksum `86b1e3c2 / c4b13eb4 / 670d502c` (stable, and invariant with 092 T4 absent). 2D **33.799 ms** (work 20.241), unchanged. 3D **49.980 ms** (work 33.639) — the frame landed on the third vblank period; §9.7 explains why that is not a regression and not an adoption signal |
| T6 | **Close.** Adopt or drop each item with its number; state the per-object path's status | `done` | one paragraph per item; driver state explicit | T1 done (merge-first stands), T2a adopted and firing, T4 dropped, T2b/T3 deferred to the ceiling sprint, T5 gated. No default change. The per-object path stays parked at ~95 µs/call against ~55 µs/cube |

## 6. Design notes

**T1 is a decision task, not an optimisation.** Its only job is to stop the question from being re-litigated. The
rule is written: ≥30 µs removable reverses the direction; less closes the redesign with that number.

**The index cache's hard part is lifetime, not speed.** A block handed to `sceGuDrawArray` must outlive the list that
references it. The pool is main RAM and persistent; a block may only be overwritten when no live list references it.
The two-lists-in-flight rotation gives the rule a concrete test: the same proof 089's cache would have needed.

**Strips and merging are different paths.** The merge path expands instances per frame and cannot use a static strip
buffer; T3 applies to meshes drawn as themselves. Its value is therefore bounded by how much of a scene is static
single meshes — measure that before building.

**The expansion is where merging's 55 µs/cube lives.** 14,400 vertices per frame at 400 cubes is ~460 KiB of packed
output; if one line dominates (transform, pack, or the arena copy), that is the only one worth changing.

**Measurement discipline.** Below a vblank step, `work` beside `avg`, two runs per point, and a counter for every
skipped block so a dead feature is visible as dead.

## 7. Gates

| gate | expected |
|---|---|
| `batch_check` / `basic_shapes` | 613 / 284 |
| `scene_3d SCENE=both`, `MESH_SCENE`, RTT | 0 diff |
| `clip_probe` | 237/720 |
| benchmark pair | below a step; `work` reported; no regression |
| frame behaviour | `arenaOverflows=0`; index-cache hits reported |

## 8. Exit criteria

- [ ] T1's breakdown exists and the decision is either confirmed or reversed with a number
- [ ] the index cache is adopted or dropped with the copies/frame it removed
- [ ] strips are adopted for static meshes with 0 diff, or their ceiling is stated
- [ ] the expansion attribution sums, and any change is measured
- [ ] gates green; pair re-run below a step
- [ ] per-object path's status (opt-in, default-off) is stated at close

## 9. Result

### 9.2 T2a — merged index reuse (`done`, 2026-11-28)

The merged index buffer depends only on `(ic, vc, mesh->indices)`, never on the instance
transforms, so `pspMergeBuild` keeps the buffer when its key and instance count are unchanged. Logs after 400
consecutive reuses, so a dead feature shows as silence.

| build | avg | work | swap | fps |
|---|---|---|---|---|
| pre-T2a (merge-era binary) | 40.366 / 40.531 | 32.615 / 32.616 | 7.75 / 7.92 | 24.8 |
| T2a | **33.441 / 33.443** | **30.756 / 30.754** | 2.68 / 2.69 | **29.9** |

Alternated on the same console, reset between: the old binary reproduces 40.4 twice and the new one 33.4 twice, so
this is not drift. The reuse log fires in every run.

**The frame-time win is real:** `avg` fell from 40.4 to 33.4 ms (24.8 → 29.9 fps), reproduced by alternating the two
binaries. **Its magnitude is a step effect** — the frame crossed 33.3 ms downwards — and the "work falls 1.9-2.6 ms"
line is split-derived, not an independent measurement. T2a's underlying saving at a sub-step size is too small to
see (at GRID=8 the loop is ~2,300 indices). So: a real 7 ms frame, a small real cost saving, and a step in between.
This is the first change in the phase that improves the **shipped** 3D build.

**Caveat, recorded rather than smoothed:** 9,900-14,000 index writes should cost ~0.3-0.45 ms, not 2 ms. The likely
extra is cache traffic — the loop rewrites ~28 kB of paged memory and leaves the buffer cold for the driver's
`memcpy` — but that is a hypothesis. The number is the measured one; the mechanism is not claimed.

### 9.3 T4 — the normal path (`dropped`)

The merge key already proves uniform scale and an orthogonal basis (`pspMergeEligible`), so `|M*n| = sqrt(sx)` and the
per-vertex `sqrtf` looked redundant. Arm `PSP_MERGE_FAST_NORMAL=1`: one reciprocal per instance, two loop variants
(a per-vertex branch cost 2.3 ms when it sat inside the loop). Gated on the lit static scene: **0 pixels differ**.

At GRID=20 it looked like **−4.4 ms of work** (30.77 → 26.35), twice each. It was not:

| arm | GRID=8 `work` (below every step) |
|---|---|
| baseline | 6.147 / 6.153 |
| fast normal | 6.251 / 6.248 |

**0.1 ms slower**, not 0.7 faster. The GRID=20 pair had both arms at `avg 33.44`, so a 4.4 ms "work" difference was
mirrored by 4.4 ms of `swap` — the split moving, not the frame. This is the recipe's landmine 7 in its purest form:
at a step, `work = avg − swap` is a difference of two quantised values and must not be compared.

Dropped. `PSP_MERGE_FAST_NORMAL` stays at 0 with its gate recorded.

### 9.4 T4 — the attribution, done with vsync off (`MERGEX`)

The merge path bypasses `DrawMesh`, so it had no timer. `MERGEX` times `pspMergeBuild` and the flush separately,
and the run is taken **free-running** (`--vsync 0`), where nothing is quantised:

| component | per frame, GRID=20 |
|---|---|
| merge build (CPU expansion) | **10.6 ms** (5,282 µs × 2 runs) |
| merge flush (state + arena copy + draw) | **4.9 ms** (2,468 µs × 2) |
| everything else | ~25.6 ms |
| frame, free-running | 41.1 ms (`swap` 0.18 — collapsed, as expected) |

So the merged path's 15.5 ms is almost exactly half expansion, half copy: `pspMergeBuild` writes 14,400 vertices at
~730 ns each, and the flush `memcpy`s 460 KiB of vertices plus 29 KiB of indices into the arena and writes them back.

**The next lever is the copy, not the loop.** The merged vertices are a static buffer rebuilt each frame; if it were
double-buffered by frame parity and handed to the driver directly, the 4.9 ms copy and its writeback disappear, at
460 KiB × 2 of main RAM. That is a lifetime-shaped change, like T2b, and it is worth ten times T2b.

**Note on the modes:** the free-running frame is 41 ms against 33.4 locked, because with vsync the GE hides behind
the next frame's CPU. That difference is the pipelining the three-buffer design buys, and it is why adoption numbers
stay at vsync 1.

### 9.5 The determinism gate (`PSP_FramebufferHash`)

Capture-at-DONE cannot see scrambled geometry (it differed from itself by 35 %). `PSP_FramebufferHash()` hashes the
buffer the frame just drew, after waiting for the list that drew it; `psp_showcase CHECKSUM=1` switches the animation
to frame-indexed (`t = frames/60`) and prints the hash at frames 60/120/240.

| run | frame 60 | 120 | 240 |
|---|---|---|---|
| 1 | `d53fd604` | `f0498eaa` | `313b0556` |
| 2 | `d53fd604` | `f0498eaa` | `313b0556` |

Deterministic across runs, so it is a gate: any change that scrambles indices or reuses a buffer too early moves it.

**Correction, 2026-11-28 (092 T4).** Those values were deterministic only because both runs drew the same FPS digit.
The frame the gate hashes contains the HUD line `DrawText(TextFormat("%d fps", GetFPS()), ...)` — a *measured* number.
A no-op change (092 T4, which nothing in the tree calls) shifted frame time by microseconds, the digit changed, and
all three hashes moved to `24a74236/c4b57d40/64960f58`. Capturing both builds at a fixed frame and diffing the raw
images localised it exactly: **12 differing pixels, all inside that one glyph** (x 20–23, y 43–49), 0 elsewhere.

The checksum arm now draws `"checksum build"` instead, and the gate is invariant under a code-size change:

| build | frame 60 | 120 | 240 |
|---|---|---|---|
| new baseline, run 1 | `86b1e3c2` | `c4b13eb4` | `670d502c` |
| new baseline, run 2 | `86b1e3c2` | `c4b13eb4` | `670d502c` |
| same scene, 092 T4 absent | `86b1e3c2` | `c4b13eb4` | `670d502c` |

The old triple is retired. Two further instrument notes, both measured today:

- **The hash is all-or-nothing.** FNV over 130,560 pixels means one changed pixel rewrites it, so the hash answers
  "identical or not", never "how much". When it moves, capture both arms at a fixed frame (`CHECKSUM=1 FRAMES=241`,
  then `scrshot`) and count pixels — that is what found this.
- **`lib_hash` in `bench/results/*.json` is a build identity, not a content identity.** `psp-ar` writes member
  timestamps, so three clean builds of identical sources produced three hashes (`e3168c2f`, `d6edc865`, `4e3bc117`).
  Two builds can be compared only by their recorded git revision plus the driver content hash; the library hash
  distinguishes *runs of the same install*.

### 9.6 The copy removal does not move the frame — re-prioritised

The four-buffer direct merge (hand the vertices to the driver, skip the 460 KiB copy) would remove ~4.3 ms of work:

- **vsync on:** frame 33.44 ms, work 30.8 → 26.5. Still locked at 33.3. **`avg` does not move.** By this sprint's own
  rule — adopt only what moves the pair — it would be dropped.
- **What it actually buys:** headroom, i.e. a higher 30 fps cube ceiling (roughly 400 → 480) and lower CPU/battery on
  the merged path.

So it is not the next task. If it is done, its acceptance must be a **ceiling measurement**, not the pair: the last
cube count at 30 fps with and without the change, on the same layout, with the hash gate at 0 differences.

**Not done:** T2b, the persistent driver-side copy with a two-list lifetime proof. The driver still copies the run
into the arena; only the rebuild is gone.

**Pixel check: the instrument failed, and that is recorded.** Capture-at-DONE, each binary twice, same console:

| comparison | pixels differing |
|---|---|
| old vs old (identical binary) | **45,639 (35 %)** |
| new vs new (identical binary) | 1,735 (1.3 %) |
| old vs new | 79,207 (60.7 %) |

The old binary does not reproduce its own last frame — at 40.4 ms per frame it sits between vblank steps, so the
displayed buffer at capture time differs run to run. A cross-build comparison built on that is worthless, and the
60.7 % figure must not be read as a defect. It is not evidence either way.

**The gate that works.** `scene_3d SCENE=both INSTANCES=64`, a static 64-instance grid of one mesh, which takes the
merge path and is frame-deterministic. Built twice — `PSP_MERGE_IDX_REUSE=1` and `=0` — and captured:

| arms | pixels differing |
|---|---|
| reuse on vs off | **0 of 130,560** |

Both arms report the same `STAT` median (16.684 ms), as expected at 64 instances where the index loop is small. So
the reuse is **pixel-identical on the merge path**, and the `PSP_MERGE_IDX_REUSE` switch remains for future A/Bs.

**What the failed capture means:** T2a's correctness no longer rests on the structural argument alone — the skip is taken only when the run key
matches *and* the run has not grown, so the reused prefix is byte-identical — plus the fact that the scenes render.
That is weaker than a pixel gate and must be stated as such. The gate this sprint needs is a **frozen-frame** scene
(the `batch_check` pattern: two identical final frames) or `scene_3d SCENE=both`, not a capture of a running demo.

### 9.7 The pair at GRID=20 is bistable — T2a's value cannot be re-read (2026-11-28)

Closing the sprint with 092 T4 in the library produced a 3D pair of **49.98 ms**, not the 33.44 recorded in §9.2. It is
not a regression, and it is not an adoption signal either. Same staged binary, run twice, reset between:

| arm | run 1 | run 2 | work | swap |
|---|---|---|---|---|
| shipped (reuse on) | 50.000 | 49.875 | 33.63 / 33.65 | 16.37 / 16.23 |
| `PSP_MERGE_IDX_REUSE=0` | 41.242 | 41.242 | 32.82 / 32.82 | 8.42 / 8.42 |
| shipped, free-running (`--vsync 0`) | 35.919 | — | 35.74 | 0.19 |

Three facts in that table:

1. **The CPU work did not grow.** Free-running is 35.9 ms, and the two vsync arms differ in `work` by 0.8 ms — a
   difference of two quantised values, i.e. the artefact §9.6 renamed. What changed is *which vblank period* the
   frame lands on: 50.0 = three periods, 41.2 = two and a half.
2. **Reuse on is currently the worse arm, and 096 §9.2 measured the opposite** (on 33.441/33.443, off 40.366/40.531).
   Both arms in that A/B were stable; both arms here are stable; they disagree. A 9 ms swing between sessions with
   no code difference is the step, not the change.
3. **`psp-ar` stamps are why "same build" is hard to pin down.** Three clean builds of identical sources produced
   three `lib_hash` values today (`e3168c2f`, `d6edc865`, `4e3bc117`, `84719b8c`), so a recorded hash proves an
   install, not a content.

The decision this sprint cannot make — whether reuse-on stays the default — is moved to the ceiling sprint, which
uses a step-free metric (the last cube count that holds 30 fps). Until then the default stays as adopted: the
mechanism is sound, the reuse is proven alive by its log line, and flipping a default on a bistable pair would be
chasing the instrument.

Recipe consequences, written into `docs/psp/recipes/benchmarking.md`: interleave the arms inside one session, report
the swap wait per run, and decide sub-period gains with free-running time or a ceiling count.

### 9.1 T1 — the last breakdown (`done`)

One probe run, GRID=14, merge off, hint on, with a new `state=` leg (texture enable + the three pointer setups):

| leg | µs/call |
|---|---|
| `DrawMesh` total | 125-127 |
| cull | 25.0 |
| driver | 26.9 |
| surface (residual) | 73.7 |
| compose | 4.4 |
| mult | 8.4 |
| **state** (texture + pointers) | **14.5** |

`state` is 14.5 µs and carries one timing pair of its own (~4 µs), so the block a state-skip could remove is **~10–11 µs**
— well under the 30 µs the reversal rule required. **The merge-first decision stands.**

The residual after subtracting the measured legs is still ~46 µs, of which ~15 is instrument overhead; the remaining
~30 µs sits in push/pop, colour, the four `rlDisableStatePointer` calls and DrawMesh's own bookkeeping. No single
removable block in it reaches the bar either, so it is recorded as unattributed rather than as an invitation for
another thinning sprint.

**Consequence for 091:** T4a (state skip) is worth ~10 µs/call on a path that is opt-in and still ~40 % above
merging. It stays parked with this number, not pursued.

### 9.8 The ceiling, measured — the step-free metric this sprint deferred to (2026-11-28)

The ceiling sweep the close deferred (T2b/T3). Tool: `psp-tests/matrix_probe MODE=scene PROBE=0`, the same one 078 used,
so the numbers are comparable to its 270. `COUNT` cubes in a 10-wide grid, `FRAMES=200`, **two runs per point**, reset
between; a point holds when the median is at the period **and** p95 does not drop.

| point | 60 fps arm | 30 fps arm |
|---|---|---|
| pass | **250** — median 16.683, p95 16.70, work 15.56 / 15.62 | **570** — median 33.369, p95 33.38, work 32.05 / 31.92 |
| fail | **251** — median 33.367, work 16.88 | **575** — p95 50.05, work 32.87 |

Adjacent counts, both arms stable across their two runs, so these are boundaries rather than samples. **60 fps = 250,
30 fps = 570.**

**Against 078.** That sprint recorded the merged arm at **270 instances / 60 fps** with work 15.245 measured at 270.
Today 270 reads median 33.35 with work 16.60 — 1.35 ms more work at the same count, and the ceiling is 250. Per cube
that is 62 µs against 56 µs, a **7–10 % regression** on the path the demos ship, introduced somewhere between b33 (078)
and `973198f1`. Candidates in that window, in the order they were measured: 079's sincos, 080's lighting scoping, 085's
opt-in arms, 089's arena 2×2, the upstream raylib merge of 2026-11-25 (which touched `rmodels.c`/`rlgl.h`), and 090's
clean hint. None is suspected preferentially; this is the measurement that says where to look next.

**What the ceilings are for.** They are the acceptance metric for the copy removal (a 4.3 ms saving on the merged path
is ~75 cubes at 30 fps) and for strips: an arm that does not move 250 or 570 has not earned its code. A 30 fps addition
of one cube is 57 µs of work, so the resolution is fine enough to see the copy removal and nothing smaller.

## 10. Follow-ups

- **The ceiling sweep decides T2a and T2b.** Measured (§9.8): **250 at 60 fps, 570 at 30 fps** on the shipped build.
  The copy removal must move one of them; a 4.3 ms saving is ~75 cubes at 30 fps. The 7–10 % gap to 078's 270 at the
  same count is the first thing to explain.
- If T1 reverses the decision, 091 T4a and the thin entry resume and this sprint's items still stand.
- If strips cannot serve the merge path, the GE-side lever for merged content is closed and only the CPU expansion
  remains.
- The index cache, once proven, should be documented in the A/B recipe as the example of a lifetime-safe cache.
- **The 3D demo needs a size that is not on a step** before anyone quotes its `avg` again. GRID=20 has read 33.44,
  40.4, 41.2, 41.9 and 50.0 for the same work; a smaller grid (below one period) or the ceiling count is the fix.

## 11. References

- [redesign §8](../phases/10-3d-advanced/pspgu-redesign.md) — the decision and the reversal rule
- sprint 085 §9.3 (strips and the index-copy measurement), §9.1 (the split)
- sprint 091 §9.1-9.4 (the per-call split and T4b), 089 §9.1 (the lifetime proof the cache needs)
- `tools/pspgu/gl_draw.c:749-793` (index copy sites), `src/rmodels.c` (merge expansion), `tools/pspgu/pspgu_internal.h`
