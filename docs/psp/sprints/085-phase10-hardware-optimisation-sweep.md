# Sprint 085 (P10-S15) — Hardware optimisation sweep: formats, submission, clear, coherence and GE/CPU attribution

| | |
|---|---|
| Roadmap phase | [Phase 10 — 3D and advanced](../phases/10-3d-advanced/index.md) |
| Sprint | P10-S15 |
| Branch / base commit | `master` @ `65012409` (pspgu `b37`; docs commit on top of the b37 driver) |
| Started / closed | 2026-11-23 (written) / — |
| Build number(s) | pspgu `b38` for any adopted driver change; `libraylib.a` `9a1d97210d7a1df6` (`PSP_BATCH=1 PSP_MERGE_DRAWS=1`); new probes opt-in |
| Tasks | 0/8 done |

## 0. Handover

**Situation.** Phase 10 closed at P10-S12 and the two follow-up sprints (P10-S13/P10-S14) added state and
reference-idea checks. The b37 driver is the current baseline: 3D showcase **33.677 ms** work, 2D showcase
**21.578 ms** work, driver hash `9798af84e81b269e`, library `9a1d97210d7a1df6` (`psp-tests/bench/results/b37.json`).
The 2D/3D gates are 0 diff and the launcher embeds both showcases.

A new list of PSP optimisations has arrived from a general hardware guide. Most of it is already adopted here, or
was measured and dropped for a specific workload. This sprint is therefore **not** a re-run of the list. It isolates
the few untested paths, gives each a workload where the claimed bottleneck could actually exist, and adopts only a
pixel-clean, repeatable win. Every speed claim needs T1's GE/CPU split; today the harness has `work` and `swap` but
not GE time.

**The b37 baseline is the control.** Do not compare a new build against historical b35/b36 rows without re-running
the pair. One tag per sprint (`b38`), forced object removal before every driver build, and two runs per point.

### What the list asks for, and what is already true here

| area | technique | our state | decision |
|---|---|---|---|
| Geometry | S16 vertices | adopted for packed meshes: `PspGeMesh`, 28-byte stride, `direct=1` (061) | baseline; no task |
| Geometry | S8 vertices | dropped in 077 T5 **without a device run** because the measured scenes are draw-call bound and parity-bound | re-test only on a new **vertex-bound** workload (T2) |
| Geometry | triangle strips | `gl_draw.c:1292-1293` maps `GL_TRIANGLE_STRIP/FAN`, but the raylib batcher emits only lines/triangles/quads; no 3D strip workload exists | T3 |
| Geometry | reduced indexing | indices are copied/rebased per chunk; 066 deferred a direct index path; `PspGeMesh` indices still travel per draw | T3 |
| Textures | CLUT4 | measured in 017: 8,256 B vs 32,768 B, upload 0.080 vs 0.532 ms; **dropped for 2D** (not VRAM-bound), 3D revisit deferred | T4, conditional on a texture-fetch-bound 3D scene |
| Textures | CLUT8 | never measured | T4 |
| Textures | swizzled data | adopted in 012; upload is swizzled, pixel parity 0 | baseline; no task |
| Framebuffer | 16-bit colour | 5650, three buffers + depth, adopted | baseline; no task |
| Framebuffer | 32/64-px clear stripes | `GU_FAST_CLEAR_BIT` measured **null** in 079 T2 (0.265 → 0.268 ms); manual stripe clear untested | T5, low prior |
| Submission | batched display lists | merged clean runs adopted in 078 (N=500 43.22 → 26.67 ms); static `GU_CALL` sub-lists untested | T6 |
| Submission | fewer GE sync points | targeted two-lists-in-flight waits, 13-19 µs, no full wait (069/076); all `sceGeListSync`/`TexSync` sites not audited as a set | T6 |
| Coherence | writeback and buffer lifetime | per-list 64-byte-aligned arenas, writeback before every GE pointer, texture pinning across two frames | T7 verification; no feature unless a bug is found |
| Profiling | GE and CPU time separately | `work = avg - swap`; no GE time; `sceGeDrawSync(1)` is explicitly not a GE timer (see the guide) | **T1 first** |

## 1. Objective

Falsifiable, per candidate:

1. T1 produces a stable split of one frame into **GE time**, **CPU render work** and **swap wait** on the device,
   reproducible against the b37 control.
2. Each later task runs a control and a control-again (or interleaved variants) on a workload where its claimed
   bottleneck exists, and reports the pixel result.
3. A task is **adopted** only if the measured GE or work delta exceeds the harness repeatability (0.05 ms work;
   GE time as measured by T1) and every pixel gate stays 0. Otherwise it is **dropped with the number**.
4. No task changes the default driver without the gate table in §7.

## 2. Why now

The current pair is stable and fast enough to serve as a control, and the remaining list items are second-order
features: they do not help a draw-call-bound 3D scene or a batcher-bound 2D scene by construction. Before spending
code on them, the project needs the missing measurement split (T1) and a workload per claim (dense mesh, CLUT
texture, clear-only frame, 1,000-mesh submission). This is the last systematic sweep of the hardware levers; it is
measurement-only unless a task wins.

## 3. Scope

**In:** T1 GE/CPU attribution; S8 on a vertex-bound mesh; triangle strips and a static index path; CLUT4/8 on a 3D
texture; clear-path variants; static `GU_CALL` lists and a sync audit; a coherence stress probe; gates and close.

**Out:** GUM, per-sprite submission, 32-64 px **blit** strips (023 null), compact 5551 colour, removing the CPU clip,
the Media Engine, audio, streaming, and any change to the 2D `batch_check` pixel contract.

## 4. Preconditions

- [x] pspgu `b37` installed and the b37 pair recorded (`psp-tests/bench/results/b37.json`)
- [x] clean `libraylib.a` (`9a1d97210d7a1df6`) staged with headers
- [x] PSPLink session and the memory card install are stable; ask for approval before each device session
- [ ] working tree isolated from unrelated launcher/docs changes before the first commit
- [ ] the T1 probe exists and reproduces the b37 work/swap pair

## 5. Tasks

| ID | task | status | verification | evidence |
|---|---|---|---|---|
| T1 | **GE/CPU attribution.** Add a serial-replay probe: build one frame with `sceGuStart(GU_SEND, list)`, `sceGuSendList` and `sceGuSync(GU_SYNC_FINISH, GU_SYNC_WHAT_DONE)` while the CPU is idle; report GE ms, CPU render ms, swap ms, and the existing per-phase counters (clip, cull, submit). Do not use `sceGeDrawSync(1)` as a GE timer | `todo` | GE + CPU + swap reconstructs the b37 `avg` within one frame; two runs agree; no game change | — |
| T2 | **S8 positions.** Add an opt-in `GU_VERTEX_8BIT` packed path (scale 128 about the mesh centre, same scale-matrix rule as s16). Workload: a dense mesh (≥10k vertices) drawn once per frame, not the 400-cube draw-call-bound scene. A/B S16 vs S8 | `todo` | T1 GE time and work; pixel parity on a dedicated mesh capture; adopt only if it beats the control and 0 diff | — |
| T3 | **Strips and index cache.** (a) Stripify a dense mesh and draw `GU_TRIANGLE_STRIP` via a probe or a `PspGeMesh` strip path; compare vertex/index count, GE time and pixels with `GU_TRIANGLES`. (b) For static `PspGeMesh` indices, copy the index run once per mesh/list (or hand the app pointer after a writeback) instead of per draw; measure the CPU copy | `todo` | T1 split; 0 diff; per-list lifetime proof; no `arenaOverflows` | — |
| T4 | **CLUT4/CLUT8, 3D only.** If T1 shows a texture-fetch-bound 3D scene, quantise a texture offline to 16/256 colours, upload `GU_PSM_T4/T8` with `sceGuClutLoad`, and compare 8888/5650: VRAM, upload, GE time, pixel parity. A palette path that changes pixels is opt-in, never the default | `todo` | VRAM bytes, upload ms, T1 GE time, capture; drop with the number if no fetch/bandwidth win | — |
| T5 | **Clear path.** A/B `sceGuClear` (control), `GU_FAST_CLEAR_BIT` (expected null, 079 T2) and a manual 32/64-px stripe clear on 5650 and an 8888 RTT target; measure with `clear_probe` and T1 | `todo` | clear-only work; 0 diff; drop if < 0.05 ms or no GE-time change | — |
| T6 | **Static display lists and sync audit.** (a) Build a static `GU_CALL` sub-list for a `PspGeMesh` and batch kicks on a 1,000-mesh scene (cull on/off); (b) inventory every `sceGeListSync`/`sceGuTexSync`/`sceGuSync` (`egl.c:393,473,535,570,642,762`; `gl_texture.c:353,362`) and remove only a sync with a proven non-dependency | `todo` | CPU submission and `listWaits`/`displayWaits`; 0 diff; no full wait; `arenaOverflows=0` | — |
| T7 | **Coherence stress.** A loader thread writes vertices/indices and a texture while the main thread submits; writeback then publish; compare captures to a single-thread control over N frames. Audit every pointer handed to `sceGuDrawArray` against the per-list arena and texture pinning | `todo` | 0 corrupted frames; if a missing writeback/alias is found, fix and re-run the gates | — |
| T8 | **Gates and close.** Full gate table, the pair twice, docs, commit, restore the chosen driver | `todo` | §7 all green; two runs; strict docs build | — |

Status vocabulary: `todo` / `doing` / `done` / `dropped`. IDs are stable.

## 6. Design notes

**T1 — GE time is a serial replay, not a poll.** `sceGeDrawSync(1)` reports "drawing" while a direct list is open,
so it measures the CPU's render phase. Build the same frame with `GU_SEND`, send it, then `sceGuSync` with the CPU
idle. Subtract a CPU-only build to separate list construction from GE execution. The existing `PspCountersSwap*`
pair stays the swap measure; `work = avg - swap` remains the CPU-side number until T1 lands.

**T2 — S8 is a new workload, not a re-open of 077 T5.** 077 dropped s8 because the scenes were draw-call bound and
the parity gate is exact. S8 is only meaningful when vertex fetch matters: a dense, single-draw mesh. Keep the
s16 path and the 28-byte `PspGeMesh` default; the s8 arm is opt-in and gated.

**T3 — the driver can express strips, the batcher cannot.** `gl_draw.c:1292-1293` maps the primitives, and the CPU
clip pass only touches `GU_TRIANGLES`. A strip probe must therefore supply its own indexed vertex buffer and prove
winding and edge parity. For the index cache, the constraint is the list lifetime: an app index array may be
reused before the GE runs it; only a static `PspGeMesh` may hand the pointer directly, after writeback.

**T4 — CLUT is a pipeline decision.** The runtime upload path has no CLUT (`gl_texture.c`); a palette path needs an
offline quantiser and a VRAM CLUT entry. The 017 result was a 2D glyph atlas; the 3D question is texture-fetch
traffic on the shared bus. Do not adopt a palette that changes the pixel gate; record it as an opt-in quality trade.

**T5 — fast clear was already null.** `GU_FAST_CLEAR_BIT` measured 0.265 → 0.268 ms in 079 T2. The only new arm is a
manual stripe clear, and only if the clear is a measurable share of a real frame; a clear-only probe alone is not
enough to adopt.

**T6 — CALL lists need a lifetime and a kick count.** A static sub-list is legal only when its vertices and indices
are static; matrices stay in the parent list. Measure `sceGuCallList`/stall updates on a 1,000-mesh scene, not on the
4-draw showcase. The sync audit starts from the existing targeted waits: no full GE wait, no `arenaOverflows`.

**T7 — coherence is verified, not assumed.** The driver already writes back vertices/indices/textures and keeps one
arena per rotating list. The stress probe is the evidence; only a failing frame justifies code.

## 7. Gates

Every task keeps these green; a speed claim with a pixel change is a drop.

| gate | expected |
|---|---|
| `batch_check` 2D | 0 diff / 613 colours |
| `scene_3d SCENE=both` | 0 diff |
| `MESH_SCENE=1` | 0 diff |
| RTT | 0 diff |
| `clip_probe` | 237/720 pixels as recorded |
| `basic_shapes` | 284 colours |
| frame behaviour | no full GE wait; `arenaOverflows=0`; list waits reported |
| benchmark pair | two runs per point; `work` and T1 GE time; no adoption under 0.05 ms |

## 8. Exit criteria

- [ ] T1's split is reproducible and used by every adoption decision
- [ ] every T2-T7 task has a measured adopt/drop with a number and a pixel result
- [ ] adopted changes: `b38`, gate table 0, pair re-run, docs updated
- [ ] dropped changes: recorded here and in the Phase 10 table; no code left behind
- [ ] full gates and a strict docs build pass
- [ ] driver state at close is explicit (pspgu or pspGL)

## 9. Result

Pending — no device runs yet.

## 10. Follow-ups

- If T1 works, add `GE ms` to `tools/psp-bench.sh` output so future sprints do not re-derive it.
- If T3's index cache wins, revisit the 066 deferred index-buffer note.
- If T4 finds a texture-fetch wall, an offline texture pipeline (quantise + CLUT) becomes its own sprint.
- If T2/T3/T4 lose, record the bound: the renderer is draw-call/batcher bound, not vertex/texture-fetch bound.

## 11. References

- `docs/psp/recipes/benchmarking.md` and `tools/psp-bench.sh` — the pair, work/swap, JSON fields
- sprints 017 (CLUT4), 023 (cache strips), 061 (s16), 066 (index deferral), 069/076 (targeted waits),
  077 (compact formats), 078 (merged clean runs), 079 (PGE/fast clear), 080/081 (state and reference checks)
- `tools/pspgu/gl_draw.c` (`guPrimitive`, `guVertexFormat`, index paths), `gl_texture.c` (no CLUT),
  `gl_state.c` (clear), `egl.c` (list waits), `gl_matrix.c` (s16/s8 scale rule)
- public: PPSSPP GE documentation (vertex pipeline, display lists, raster rules), pspsdk `samples/gu` and
  `pspgu.h`. No vendor SDK documentation or code is used.
