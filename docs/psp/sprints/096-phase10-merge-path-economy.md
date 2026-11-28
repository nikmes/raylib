# Sprint 096 (P10-S26) — Merge-path economy: index cache, static strips, expansion cost

| | |
|---|---|
| Roadmap phase | [Phase 10 — 3D and advanced](../phases/10-3d-advanced/index.md) |
| Sprint | P10-S26 |
| Branch / base commit | `master` @ `33e7f18f` — decision: [redesign §8](../phases/10-3d-advanced/pspgu-redesign.md) |
| Started / closed | 2026-11-28 (written) / — |
| Build number(s) | pspgu `b38` only if a driver change is adopted; the index cache is a driver + raylib change |
| Tasks | 2/6 — T1 and T2a done; T2b (driver copy) open |

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
| T2 | **Index-run cache.** Cache a run's indices in a persistent pool keyed by `(pointer, count, first)`, written back once; a key change is a miss; the pool never overwrites a block whose list is in flight | `todo` | copies/frame fall; the 3D showcase keeps correct pixels; hit/miss counters; `arenaOverflows=0` | — |
| T3 | **Static-mesh strips.** Stripify at load for meshes flagged static (not the merge path), draw `GU_TRIANGLE_STRIP`, and measure against the list on a mesh-heavy scene | `todo` | 0 diff; strip vertex count and GE time reported; if it cannot be used by the merge path, say so and record the ceiling | — |
| T4 | **Expansion attribution.** Probe the merge expansion line by line (transform, pack, arena copy, index rebase) and improve the largest line only if one dominates | `todo` | attribution sums to the measured expansion ±10 %; one measured change or none | — |
| T5 | **Gates + pair.** Full pixel table, the pair below a vblank step, two runs per point | `todo` | 613 / 284 / 0 / 0 / clip; `work` reported; no regression | — |
| T6 | **Close.** Adopt or drop each item with its number; state the per-object path's status | `todo` | one paragraph per item; driver state explicit | — |

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

**Work falls 1.9-2.6 ms** (depending on which earlier build is the baseline) and the average falls **7 ms** because
the frame crosses the 33.3 ms vblank step downwards — the cliff, in our favour. This is the first change in the
phase that improves the **shipped** 3D build.

**Caveat, recorded rather than smoothed:** 9,900-14,000 index writes should cost ~0.3-0.45 ms, not 2 ms. The likely
extra is cache traffic — the loop rewrites ~28 kB of paged memory and leaves the buffer cold for the driver's
`memcpy` — but that is a hypothesis. The number is the measured one; the mechanism is not claimed.

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

## 10. Follow-ups

- If T1 reverses the decision, 091 T4a and the thin entry resume and this sprint's items still stand.
- If strips cannot serve the merge path, the GE-side lever for merged content is closed and only the CPU expansion
  remains.
- The index cache, once proven, should be documented in the A/B recipe as the example of a lifetime-safe cache.

## 11. References

- [redesign §8](../phases/10-3d-advanced/pspgu-redesign.md) — the decision and the reversal rule
- sprint 085 §9.3 (strips and the index-copy measurement), §9.1 (the split)
- sprint 091 §9.1-9.4 (the per-call split and T4b), 089 §9.1 (the lifetime proof the cache needs)
- `tools/pspgu/gl_draw.c:749-793` (index copy sites), `src/rmodels.c` (merge expansion), `tools/pspgu/pspgu_internal.h`
