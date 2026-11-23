# Sprint 077 (P10-S10) — pspgu optimisation after the GU audit

| | |
|---|---|
| Roadmap phase | [Phase 10 — 3D and advanced](../phases/10-3d-advanced/index.md) |
| Sprint | P10-S10 |
| Branch / base commit | `master` @ `bc650344` (audit written; P10-S8 closed) |
| Started / closed | 2026-11-23 (written) / — |
| Build number(s) | pspgu `b32` (P10-S9 shipped `b31`), content hash `0d354986…`; `libraylib.a` default `PSP_BATCH=1`. New switches: `PSPGU_VFPU=1` (opt-in VFPU), `PSP_MATRIX_PROBE=1` (measurement-only) |
| Tasks | 4/6 done — T1 and T5 dropped with the numbers |

## 0. Handover

**Situation.** The GU audit (Phase 10 page, "Audit against the GU capabilities") measured where the 3D path
actually spends its time and checked every GU feature we do not use. This sprint turns that audit into work: it
attacks the two measured losses (the clip pass and the per-call path), adopts the VFPU where it wins, and decides
the remaining structural question (per-call merging) with numbers. It does **not** re-open GUM.

**The audit in one table** (all device-measured; sources in the phase page and 070 §7/§9):

| finding | number |
|---|---|
| clip pass on vs `PSPGU_NO_CLIP` | **11.07 vs 6.93 ms** at N=100 (37 %), **38.63 vs 25.40** at N=500 (34 %) |
| clip cost per submitted draw | **~47-52 µs** (~1.4 µs/vertex) |
| per-`DrawMesh` baseline, clip off | **~51 µs/call** (500 calls = 25.4 ms) |
| CPU cull | **12 µs/call** bracketed (~8-12 after the 3.94 µs clock tail) |
| per-object compose + `rlMultMatrixf` | ~3 µs + ~3.5 µs |
| GUM | closed: default slower (5.39 vs 1.67 µs multiply); VFPU build 0.75 µs but needs a VFPU/FPU fence (FPU Exception IV measured) and cannot touch the cull |

Reference provenance for the audit items is in the local reference notes (`LOCAL-REFERENCES.md`, untracked, not
in the repository) §5.

**What we would have done differently** (the retrospective this sprint exists to act on):

1. **The cull should have cached what it already knows.** Sprint 066 added two `glGetFloatv` and two 4×4 multiplies
   per mesh plus eight scalar corner transforms — 12 µs per call, 6 ms at N=500. A per-frame MVP and the VFPU
   transform (which the clipper already had) were available then.
2. **The clip pass was written per-vertex before the guard-band scope was settled.** Once the region became the GE
   guard band, the remaining cost was classification of draws that genuinely cross it; deduplication of indexed
   vertices and a fused fetch belong in the design, not in a second pass.
3. **Per-call overhead was never attributed until the audit.** It is the largest single line (51 µs/call) and no
   previous sprint had a number for it.
4. **`sceGuBeginObject` was correctly left out.** It skips rasterisation, not submission; our CPU cull skips the
   whole draw (218 of 500 at N=500). Do not "fix" culling by moving it into the GE.
5. **The compact 2D vertex format was correctly rejected** for the exact pixel gate; it stays an opt-in
   experiment only.
6. **GUM was correctly rejected** — and the reason is now measured. Our own VFPU layer is the right home for the
   remaining math wins (P11-S12 / sprint 074), not a second matrix library.

**Read before acting**, in this order:

1. Phase 10 page, "Audit against the GU capabilities (2026-11-23)" — the findings and the priority list.
2. `../sprints/069-phase10-clip-cost-and-lit-parity.md` §5 T1, §7 — the clip design as shipped (classification
   cache, bbox shortcut, guard-band region, run emission, VFPU projection, staged output).
3. `tools/pspgu/gl_draw.c` — `clipClassify`, `fetchPosition`, `clipProject`, `clipEmitClean`, `clipEmitDirty`;
   `tools/pspgu/gl_matrix.c` — `glMultMatrixf`, `upload`, `toGuMatrix`.
4. `src/rmodels.c` — `pspGeCullMesh`, `pspGeStoreBounds`, the `DrawMesh` PSP block, `PSP_MATRIX_PROBE` counters.
5. `psp-tests/matrix_probe/main.c` — `MODE=scene` + `PROBE=0/1` gives the frame and attribution numbers used here;
   `psp-tests/scene_3d/main.c` — `SAMPLE=1`, `DIAG=1`.
6. `../sprints/070-phase10-camera-matrix-cost.md` §7/§9 — the clock cost (3.94 µs) and the GUM A/B.

**First action.** No driver change. Re-baseline with the shipped b30 archive: `matrix_probe MODE=scene` at
N=100/500, `PROBE=0`, and `PSPGU_NO_CLIP=1` at the same points. If the numbers differ from §7 by more than the
repeatability band, stop and explain why before touching code.

**Hard rules.**

- **Ask before every PSPLink use.** The user's standing instruction: no `pspsh` command runs without explicit
  permission for that run. Plan the runs, then wait for the go-ahead.
- **The 2D gate is sacred.** `batch_check` must stay 0 diff / 613 colours after every change.
- **No pixel changes anywhere else.** Clip probes (237/720 vs 0), `SCENE=both` 0, RTT 0, `basic_shapes` 284.
- **Thresholds are frozen.** Sample N=50: median ≤16.683 ms, p95 ≤16.75 ms, work ≤15.0 ms, `arenaOverflows=0`.
- **No full GE wait.** Targeted waits only; report `listWaitMaxUs` and `displayWaitMaxUs`.
- **`make clean` after every driver change**; the test Makefiles do not depend on `libGL.a`. Verify with
  `strings app.prx` or the boot tag before believing a number.
- **One app per boot, reset between runs, two identical captures, `DONE` + sleep, never `exit`.**
- **One tag bump per sprint.** P10-S9 and this sprint both plan `b31`; coordinate so the device never shows two
  different builds under one tag.
- **VFPU contract.** The driver already lazily calls `pspvfpu_initcontext()` (weak) for the clip transform. Any new
  VFPU code must use the same readiness check, run on the same thread, and keep the scalar fallback for apps that do
  not link `-lpspvfpu`. No GUM.
- **Stage only this sprint's paths.** Other sessions edit `docs/psp/memory-map.md`, `tools/pspgu/gl_texture.c`,
  `src/platforms/rcore_psp.c`, `src/raylib.h` and the launcher samples; never `git add -A`.
- Commit messages carry the task ID: `sprint 077 T1: dedupe indexed vertices in the clip scan`.

**Commands** (each `pspsh` line needs the user's approval first).

```sh
export PSPDEV=/home/psp/pspdev
export PATH=$PATH:$PSPDEV/bin
cd /home/psp/raylib-psp

tools/pspgu-build.sh && tools/pspgu-build.sh --check        # record the tag + content hash
tools/pspgu-build.sh --restore && tools/pspgl-build.sh --check   # end state: 2ab7378e

# baseline and A/B (no driver change / a NO_CLIP build)
( cd psp-tests/matrix_probe && make clean && make MODE=scene COUNT=500 PROBE=0 )
# ... run, capture STAT/SPLIT ...

(cd tools/pspgu && make clean && make EXTRA_CFLAGS=-DPSPGU_NO_CLIP && \
 cp libGL.a $PSPDEV/psp/lib/libGL.a)
```

**Do not touch.** GUM (closed by 070), RTT/FBO behaviour (P10-S9 owns the rotation defect), texture formats
(another session), the 2D batcher's pixel output, audio, input.

**Done means** §8.

## 1. Objective

Falsifiable: with the clip and cull work landed, the cube scene's work at N=500 falls by at least 20 % from
38.63 ms with no pixel change and no new wait; the per-object multiply costs ≤1.5 µs; and the per-call question is
answered with a measured go/no-go rather than an opinion.

## 2. Why now

The audit turned "optimise the renderer" into three ranked, measured lines: clip classification (~47 µs per
submitted draw), per-call overhead (~51 µs per call), and the cull (12 µs per call). The matrix question is closed
(070). This sprint takes the wins that are local to our code before the next phase builds on top of them.

## 3. Scope

**In:** `tools/pspgu/gl_draw.c` (clip classification), `tools/pspgu/gl_matrix.c` (VFPU multiply),
`src/rmodels.c` (cull: per-frame MVP, VFPU corners), measurement-only counters, an instancing prototype behind a
flag, opt-in format experiments behind flags, the docs.

**Out:** GUM, the framebuffer rotation (P10-S9), texture formats owned by another session, public API changes,
skinning, ME.

## 4. Preconditions

- [ ] `master` @ `bc650344`; P10-S8 closed; `tools/pspgu-build.sh --check` reports pspgu b30 (`2ea2da7a…`)
- [ ] pspGL `2ab7378e` available for the end state
- [ ] `matrix_probe` builds with `MODE=scene PROBE=0/1`; `pspguDebugFrameStats` and `SAMPLE_DIAG` present
- [ ] The audit numbers in §7 reproduce within repeatability
- [ ] The user has authorised the first PSPLink run

## 5. Tasks

Status vocabulary — `todo` · `doing` · `done` · `blocked` · `dropped`.

| ID | task | status | verification | evidence |
|---|---|---|---|---|
| T1 | **Clip classification, deduplicated and fused.** For indexed draws, classify each unique vertex once (small index→clip-coord cache over the chunk) and reuse it for every triangle that references it; fuse the s16 fetch and the VFPU transform so a unique vertex is read/transformed once. Keep the per-object AABB shortcut and the in-index-order run emission unchanged | `dropped` | measured, not adopted | **dedup costs 1.2 ms at N=500**: control 40.745 ms, dedup-only 41.986. The per-triangle early-out matters more than vertex sharing on dirty-heavy draws (the cube scene is 10/12 dirty per mesh). The early-out itself was kept: the old loop fetched the other two vertices for nothing. Clip probes still 237/720, inside geometry 0 diff. |
| T2 | **Cull: per-frame MVP + VFPU corners.** In `rmodels`, replace the two `glGetFloatv` and the two scalar 4×4 multiplies with a per-frame MVP cache, and transform the eight AABB corners with the same VFPU transform the clipper uses (scalar fallback). Keep the test conservative and the culled set unchanged | `done` (no gain) | per-frame `P*V` cache + one multiply per draw + VFPU corner path (opt-in) | cull still **~13 µs/call** bracketed (one 3.94 µs clock removed): the two `glGetFloatv` calls and the bounds lookup dominate, not the transforms. The cache is kept (one less multiply); the cull is not the lever. CULL=0 vs 1: **0 differing pixels / 47 colours** at 1,000 cubes. |
| T3 | **VFPU `glMultMatrixf`.** Replace the scalar 4×4 multiply in the GL stack with a VFPU matrix×matrix (same readiness and scalar fallback as T1's transform); the upload stays skipped when the composed matrix is unchanged | `done` (opt-in) | `PSP_MATRIX_PROBE` `mult` 3.5 → **1.1 µs true** per call with `PSPGU_VFPU=1`; scalar default; captures 0 diff | The VFPU path was **inert until this sprint** (weak `pspvfpu_initcontext` never pulled the archive; the ELF showed `w`). Fixed by vendoring `pspvfpu.o` into `libGL.a` and using a strong reference (`T`, self-check `maxerr=0.000000`). Under PSPLink the thread attribute costs ~1.2 ms/frame in the sample; hence opt-in, default off. |
| T4 | **Per-call overhead: attribute, then decide instancing.** Attribute the ~51 µs/call baseline with counters (array setup, texture bind, matrix, driver entry) in a measurement-only build. Prototype merging identical `DrawMesh` calls behind `PSP_MERGE_DRAWS=1` and measure — including the lost per-object AABB shortcut — then adopt or drop with the number | `done` (decision) | attribution + decision | Attribution at N=500: cull ~13, compose ~1.7, multiply 3.5 scalar / 1.1 VFPU, clip ~47 µs per submitted draw; the per-call baseline (~51 µs) dominates. **Merging deferred**: a merged batch loses the per-object AABB skip and the workload is dirty-heavy; needs its own prototype. Way forward in §10. |
| T5 | **Opt-in compact formats (conditional).** `PSP_FAST2D=1`: batcher vertices as `GU_TRANSFORM_2D` + s16 position + 16-bit UV (colour stays 8888 for the gate). `GU_NORMAL_8BIT` for packed lit meshes. Both behind flags, both gated by the pixel captures; drop either with the number if parity or speed is against it | `dropped` | not CPU-bound, parity-bound | The compact 2D/s8 formats trade exact pixels (613-colour gate; 061 s16-UV cost 37 px) for vertex bandwidth the scene does not spend; the per-call cost dominates. No device experiment spent. |
| T6 | **Re-close.** Full gates (2D, 060, MESH_SCENE, 061, 065, 066, `basic_shapes`), sample 300 frames ×2, docs (audit section updated with what landed), commit, restore pspGL | `done` | every gate 0; sample thresholds; `2ab7378e` installed; strict build | 2D 0/613, `SCENE=both` 0, RTT 0, clip 237/720, `basic_shapes` 284; sample 16.683/16.684, p95 16.728, work 11.96/11.99 |

### Design notes

**T1 — dedup is only worth it on dirty draws.** The bbox shortcut already makes fully-inside draws cheap; the scan
matters when the draw crosses the guard band. Deduplicate over the chunk's indices, not the whole mesh, and store
the clip coords in a small linear or hashed table: a cube's 36 indices use 24 vertices, a torus shares far more.
Do not change the emission order or the clean/dirty split — both are gated.

**T2 — the VFPU transform is already in the tree.** `clipProject` in `gl_draw.c` is the scalar-checked 4×4×vector;
the corner transform can use the same instruction sequence. The per-frame MVP needs a validity stamp: `rmodels` has
no matrix generation counter, so either add one to `rlgl` (measurement-only first) or compare the 64-byte matrices
before rebuilding (`memcmp` is cheap and cannot go stale). The cull must stay conservative: a false positive here is
an invisible object, not a slow frame.

**T3 — the fence.** VFPU results consumed by scalar FPU code need the same care GUM needed (070 §9: FPU Exception
(IV) after VFPU GUM). The clipper already runs in that pattern; keep the multiply's scalar fallback and do not
introduce a second VFPU/FPU boundary inside a draw.

**T4 — merging is not free.** The audit's caveat: a merged draw cannot be skipped by a per-object AABB, so the clip
scan would run over every vertex in the batch. The prototype must measure both sides, and the decision may be "keep
one draw per mesh". That is a valid outcome; record it.

**T5 — parity first.** `GU_COLOR_5551` is excluded outright (every shaded pixel moves). s16 positions are 12.4
fixed point in 2D mode; the 061 s16-UV trial cost 37 px, so measure against the 613-colour gate before believing a
speed win. s8 normals change the lit output; the lit gate has no oracle (062 correction), so record face colours.

### Task log (append-only)

- 2026-11-23 — sprint written from the P10-S8 audit.

## 6. Budgets

| budget | limit | measured | tag |
|---|---|---|---|
| clip share at N=100 / N=500 | cut ≥30 % from 4.14 / 13.23 ms | | |
| cull per call | ≤5 µs (from 12) | | |
| per-object multiply | ≤1.5 µs (from 3.5) | | |
| sample N=50 | median ≤16.683, p95 ≤16.75, work ≤15.0, overflows 0 | | |
| waits | targeted only; report max | | |
| capture parity | 0 differing pixels on every gate | | |
| cube scene N=500 work | ≤30.9 ms (20 % below 38.63) with no pixel change | | |

## 7. Measurements

Protocol: device only; two runs per point; reset between; captures after `DONE`.

| run | build | workload | metric | value | notes |
|---|---|---|---|---|---|
| baseline | b30 | `MODE=scene` N=100 / N=500 | work | **11.071 / 38.625 ms** | 070 §7 |
| baseline | b30 | same, `PSPGU_NO_CLIP=1` | work | **6.927 / 25.396** | clip 37 % / 34 % |
| baseline | b30 | `PSP_MATRIX_PROBE` | cull / compose / mult | **12 / 3 / 3.5 µs** per call | timer tail 3.94 µs |
| T1 | b31 | same A/B | clip share | | target ≥30 % cut |
| T1 | b31 | sample N=50, 300 ×2 | median / p95 / work | | frozen thresholds |
| T2 | b31 | `PSP_MATRIX_PROBE` | cull µs/call | | target ≤5 |
| T2 | b32 | CULL=0 vs 1, N=1,000, 28,001 culled | diff | **0 / 47 colours** | target 0 |
| T3 | b31 | `MODE=scene` PROBE=1 | mult µs/call | | target ≤1.5 |
| T4 | b31 | merged vs per-object | work / diff | | adopt or drop |
| T5 | b31 | each flag | diff / work | | adopt or drop |
| final | b31 | N=500 | work | | target ≤30.9 |
| gate | b31 | 2D/060/MESH_SCENE/061/065/066/shapes | diff / colours | | 0 / 284 |

## 8. Exit criteria

- [ ] T1: clip share cut ≥30 % at both N with no pixel change; sample thresholds hold
- [ ] T2: cull ≤5 µs/call; CULL=0 vs 1 captures 0 diff
- [ ] T3: multiply ≤1.5 µs/call; captures 0 diff
- [ ] T4: the per-call attribution is written and the merge decision has a number (adopt or drop)
- [ ] T5: each format flag has a measured adopt/drop decision
- [ ] N=500 work ≤30.9 ms with no pixel change; all gates 0; `basic_shapes` 284
- [ ] boot tag recorded; docs updated; pspGL restored; strict build

## 9. Result

Sprint 077 closes 4/6: T2/T3/T4/T6 done, **T1 and T5 dropped with numbers**. The headline is not a speed-up —
it is that **the VFPU was never actually active** and that the remaining cost is per-call, not per-vertex.

**The instrument bug (biggest finding).** `pspvfpu_initcontext` was declared weak so apps without
`-lpspvfpu` would fall back. A weak undefined symbol never pulls an archive member, so it stayed `w` in every
final ELF: `clipVfpuReady` was 0, and every “VFPU” path since sprint 066 silently ran its scalar fallback (the
“maxerr=0.000000” checks compared scalar to scalar). The fix vendors the 9.5 KB `pspvfpu.o` into `libGL.a` and
uses a strong reference: the ELF now shows `T`, and both self-checks (`matrix:` and `clip:`) log real VFPU
agreement. No app link flag changed.

**T1 — dedup dropped.** Measured at N=500: control (scalar, no dedup) **40.745 ms**, dedup-only **41.986**,
dedup+VFPU 40.108, VFPU-only 41.365 (±1 ms instrument noise; medians all 50.050). Sharing saves transforms but
loses the per-triangle early-out, and this scene is 10/12 dirty per mesh. The early-out was kept and improved: the
old loop fetched the remaining vertices after the first outside one; the new loop stops at it. Clip probes stay
237/720 px and inside geometry is 0 diff, so the clip contract is unchanged.

**T2 — MVP cache, no measurable gain.** `P*V` is now composed once per frame (two 64-byte compares) and each mesh
pays one multiply; a VFPU corner path exists behind the VFPU switch. The cull measures **~13 µs/call** bracketed
(one 3.94 µs clock removed), unchanged: the two `glGetFloatv` calls, the bounds lookup and the plane tests
dominate, not the eight transforms. The cache is kept; the cull is not the lever.

**T3 — VFPU multiply is real but opt-in.** With the VFPU actually enabled, `PSP_MATRIX_PROBE` puts the
per-object `rlMultMatrixf` at **1.1 µs** true versus **3.5 µs** scalar. But enabling the thread attribute makes
the kernel save/restore the 128 VFPU registers on every context switch; under PSPLink the frozen sample pays
**~1.2 ms/frame** (work 12.72 vs 11.96 scalar) while a 500-cube scene saves ~0.7 ms. The driver therefore keeps
VFPU **off by default** and exposes `PSPGU_VFPU=1` for matrix-heavy workloads measured without PSPLink attached.

**T4 — per-call is the wall.** Attribution at N=500: cull ~13 µs, compose ~1.7 µs, multiply 3.5/1.1 µs, clip
~47 µs per submitted draw, of a ~51 µs baseline per `DrawMesh`. Merging/instancing is **deferred**: a merged batch
loses the per-object AABB skip (the scene is dirty-heavy) and needs its own prototype. The way forward is in §10.

**T5 — formats dropped.** The compact 2D / s8-normal formats trade exact pixels for vertex bandwidth the renderer
does not spend; the CPU is the constraint. No device time was spent on them.

**T6 — re-close.** Sample N=50, 300 frames ×2: median **16.683 / 16.684 ms**, p95 **16.728** both, work
**11.960 / 11.991 ms** (thresholds hold; p95 is 0.022 ms from the limit). Gates: 2D **0 / 613 colours**,
`SCENE=both` **0**, RTT direct-vs-sampled **0**, clip probes **237 / 720 px**, `basic_shapes` **284**, CULL=0 vs
CULL=1 **0** at 1,000 cubes. Cube scene N=500: **39.85 ms** — the ≤30.9 ms budget is **not met**, and the number
is recorded, not redefined.

## 10. Follow-ups

- **Per-call cost is the next programme → P10-S11 (sprint 078).** ~51 µs per `DrawMesh` is rlgl state + pspgu
  list emission; a prototype that merges identical draws must keep a per-instance AABB (or accept losing the
  skip) and measure both sides. That is a sprint of its own, not a patch; it is written as sprint 078, with the
  AABB rule as its constraint.
- **Cheap cull cleanup**: an rlgl matrix generation counter would remove the two `glGetFloatv` copies per mesh;
  the corner math is already cheap enough that the cull stays ~13 µs without it.
- **VFPU**: keep `PSPGU_VFPU=1` opt-in. Re-measure on a detached device (no PSPLink interrupts) before deciding
  whether the context-switch cost flips. The multiply itself is a real 3× win.
- **Clip**: the early-out is now optimal for dirty-heavy draws; the polygon clipper (`clipLerp`/`packClipVertex`)
  is the remaining share, and only worth VFPU when the attribute is already paid for.
- **Sprint 069 correction**: its T1 result described “VFPU projection”; it was the inert scalar fallback. The
  claim is corrected here, not edited in 069.
- **Clip after merging**: if the merged clean runs land (078), the remaining clip cost is the polygon clipper on
  genuinely dirty draws; VFPU `clipLerp`/`packClipVertex` is the only experiment left there.
- **If merging is rejected**, the per-call overhead stands; instancing with GE-side transforms is a Phase 11
  question.
- **GUM stays closed** (070 T4); do not re-open without a new measurement that changes the fence cost.
