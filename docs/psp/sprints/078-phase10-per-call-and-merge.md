# Sprint 078 (P10-S11) — Per-call cost and draw merging

| | |
|---|---|
| Roadmap phase | [Phase 10 — 3D and advanced](../phases/10-3d-advanced/index.md) |
| Sprint | P10-S11 |
| Branch / base commit | `master` @ `b9a6af88` (P10-S10 closed, pspGL installed) |
| Started / closed | 2026-11-23 (written) / — |
| Build number(s) | pspgu `b32` (`0d354986…`) for T1/T2; **`b33`** for the merged-draw entry point. `libraylib.a` default `PSP_BATCH=1`; new opt-in `PSP_MERGE_DRAWS=1` |
| Tasks | 0/5 done |

## 0. Handover

**Situation.** Phase 10 closed with the frozen sample at 60 fps and the cube scene at **39.85 ms** of work for
N=500 — 68 % over the frame budget. Sprint 077 attributed that to the per-call path (~51 µs per `DrawMesh`,
~25 ms at N=500) and deferred merging **without a prototype**. This sprint answers the question with numbers,
under one constraint that cannot be relaxed: **a merged batch must keep a per-instance AABB test**, because the
naive version hands the GE one big bounding box and the clip pass then classifies every vertex.

**The measured baseline** (all device numbers; sources in 069 §7, 070 §7, 077 §7/§9):

| line | number | source |
|---|---|---|
| cube scene, N=500, `PSP_MATRIX_PROBE=1` | **39.85 ms** work, 500 draws/frame | 077 T6 |
| — of which clip off (`PSPGU_NO_CLIP=1`) | 25.40 ms | 077 §7 |
| clip pass per submitted draw | **~47 µs** (~1.4 µs/vertex) | 069 T1 |
| per-`DrawMesh` baseline, clip off | **~51 µs/call** | 077 T4 |
| CPU cull | **~13 µs/call**, 218 of 500 culled at N=500 | 077 T2 |
| compose + `rlMultMatrixf` | ~1.7 + 3.5 µs (scalar) / 1.1 µs (VFPU opt-in) | 077 T3 |
| frozen sample N=50 | median 16.683/16.684, p95 16.728, work 11.960/11.991 | 077 T6 |

**What the driver does today.** `glDrawElements` has two paths. The *direct* path (`isDirectLayout`) hands the
application's own buffer to `sceGuDrawArray` with a rebased index copy in the per-list arena; `DrawMesh` uses it
with the packed s16 mesh (`PspGeMesh`). The *converting* path runs `packVertex` per vertex into the arena. Vertices
that live in the arena are safe because each list has its own arena and list reuse waits on the exact list that
used it (`pspgu.listWait*`); the 2D batcher relies on exactly that (it copies, it does not hand its per-frame
buffer to the GE).

**Why the obvious merge fails.** Concatenating 282 drawn cubes into one `glDrawElements` makes the CPU cull see
one AABB spanning the scene, which intersects the frustum, so nothing is skipped; worse, the merged draw's
bounding box crosses the guard band, so `clipNeeds` classifies all 6,768 vertices at ~1.4 µs — **~9.5 ms**,
against a 25 ms saving. A merge that does not preserve per-instance visibility loses to the clip pass.

**The rule this sprint is built on.** The clip predicate is six linear inequalities in clip space
(|x| ≤ 8w, |y| ≤ 8w, −w ≤ z ≤ w, w > 0). Clip coordinates are linear in object coordinates, so an object's eight
AABB corners satisfying all six proves that **every vertex of that object** is clip-clean. The per-instance test
the cull already performs is therefore exact, and it yields three classes: **culled**, **clean** (mergeable,
provably needs no clipping), **dirty** (crosses the guard band; stays on the per-object path). That is the
"per-instance AABB constraint": merge only what is provably clean, keep everything else per object.

**Read before acting**, in this order:

1. `077-phase10-pspgu-optimisation.md` §10 — the follow-up this sprint implements; and 077 §5 T4 for the attribution.
2. `tools/pspgu/gl_draw.c` — `isDirectLayout`, `vertexLayout`, `clipNeeds`/`clipClassify`, `drawClipped`,
   `arenaCopy`, the two `glDrawElements` arms.
3. `tools/pspgu/pspgu_internal.h` — the context struct (`arenaBytes`, `listWait*`, `arenaOverflows`), the arena
   contract, and `PSPGU_ARENA_BYTES`.
4. `src/rmodels.c` — `pspGeCullMesh`, `pspGeStoreBounds`, the `DrawMesh` PSP block, `pspGePack`/`PspGeMesh`.
5. `src/rlgl.h` §`RLPSP_BATCH_ENABLED` — the 2D batcher's flush/key logic, the model for a merge window.
6. `psp-tests/matrix_probe/main.c` `MODE=scene` — the N-cube grid this sprint measures; `psp-tests/scene_3d`.

**First action.** No driver change. Re-baseline with the shipped `b32` archive: `matrix_probe MODE=scene` at
N=100/500, `PROBE=1`, two runs, plus `PSPGU_NO_CLIP=1` at N=500. If the work numbers differ from §7 by more than
the repeatability band, stop and explain why before touching code.

**Hard rules.**

- **Ask before every PSPLink use.** Plan the runs, then wait for the user's go-ahead; the standing instruction is
  no `pspsh` command without permission for that run.
- **The 2D gate is sacred.** `batch_check` 0 diff / 613 colours after every change; `bunnymark` is the 60 fps
  reference and merging must not touch the batcher.
- **No pixel changes anywhere else.** `scene_3d SCENE=both` 0, MESH_SCENE 0, RTT 0, clip probes 237/720,
  `basic_shapes` 284.
- **No full GE wait.** Report `listWaitMaxUs` / `displayWaitMaxUs`; `arenaOverflows` must stay 0.
- **Suspect the instrument before the code.** Prove the build contains the change (`psp-nm`, boot tag, a log
  line); check that the flag you grep for is the flag that is compiled.
- **Merged memory is per-list arena memory.** Never hand a per-frame rlgl scratch buffer to `sceGuDrawArray`:
  frame N would overwrite memory frame N−1's list still points at. The extension in T3 copies into the arena for
  exactly this reason — do not "optimise" that copy away without a new lifetime proof.
- **Thresholds are frozen.** Sample N=50: median ≤16.683, p95 ≤16.75, work ≤15.0.
- **One tag bump per sprint**, `make clean` after every driver change, one app per boot, reset between runs, two
  identical captures, `DONE` + sleep, never `exit`.

## 1. Objective

Falsifiable, and pre-registered here before the work:

1. The ~51 µs per-call cost is **attributed** line by line, summing to the measured per-call delta within ±10 %.
2. With merging on, the cube scene at N=500 ≤ **31.9 ms** (20 % below 39.85) and every gate capture is **0 diff**;
   or the merge is recorded **dropped** with the measured reason.
3. The per-instance AABB rule is demonstrated, not asserted: the run reports how many instances were culled,
   merged clean, and kept dirty, and the merged draw count.

## 2. Why now

The audit and 077 removed the per-vertex fat (clip early-out) and measured the VFPU. What remains is per call:
500 calls × 51 µs. The 2D path already proves the pattern works on this hardware — one draw for thousands of
sprites — but the 3D path cannot use it directly because each mesh has its own transform and its own visibility.
077 deferred the prototype; this sprint is that prototype, with the visibility rule that makes it safe.

## 3. Scope

**In:** `src/rmodels.c` (cull classification, merge window, expansion buffers), `src/rlgl.h` (per-call trims and
the flush hooks), `src/rcore.c` (EndMode3D/EndDrawing flush points), `tools/pspgu/gl_draw.c` + `egl.c` +
`pspgu_internal.h` (the merged-draw entry point, arena sizing), `psp-tests/matrix_probe` (a merge-friendly scene
arm), the docs.

**Out:** GUM, VFPU work (stays opt-in), the ME, texture formats, the 2D batcher, public raylib API changes,
skinning, the framebuffer rotation (P10-S9), and any change to the clip predicate itself.

## 4. Preconditions

- [ ] `master` @ `b9a6af88`; P10-S10 closed; `tools/pspgu-build.sh --check` reports pspGL `2ab7378e` installed
      and `tools/pspgu/libGL.a` at `b32` (`0d354986…`)
- [ ] `matrix_probe` builds with `MODE=scene PROBE=0/1`; `scene_3d` builds with `SCENE=both`, `SAMPLE=1`
- [ ] The §7 baseline reproduces within the repeatability band
- [ ] The user has authorised the first PSPLink run

## 5. Tasks

Status vocabulary — `todo` · `doing` · `done` · `blocked` · `dropped`.

| ID | task | status | verification | evidence |
|---|---|---|---|---|
| T1 | **Attribute the per-call cost.** Counters on both sides of the boundary: rlgl (texture bind, matrix push/mult, pointer setup, colour, flush decision) and pspgu (list command bytes emitted, index rebase copy, matrix upload, draw entry). Report µs per call at N=100 and N=500, and show the sum against the measured delta between a draw loop and an empty loop | `todo` | counters sum to the delta ±10 %; no pixel change (counters only) | |
| T2 | **rlgl call-surface trims.** Skip redundant texture binds, pointer setups and colour writes when the same mesh/material/texture is drawn again; replace the `rlPushMatrix`/`rlPopMatrix` pair in `DrawMesh` with a saved matrix and one direct load where the stack is provably untouched. Identical matrix results are a gate, not an assumption | `todo` | per-call ≤46 µs from ~51 in the T1 breakdown; every gate 0 diff | |
| T3 | **Merged clean runs (`PSP_MERGE_DRAWS=1`).** Extend `pspGeCullMesh` to classify culled/clean/dirty using the exact six-plane corner test; collect consecutive clean instances with the same mesh, index buffer, texture, material colour, blend/depth state and a rotation + uniform-scale transform; expand world-space vertices (uv, colour, normal, position) and rebased indices from the **original client arrays**; emit one draw per run through a new `pspguDrawMergedArrays(prim, count, indices, verts, flags)` that bulk-copies both arrays into the per-list arena and sets the known-clean flag so the clip scan is skipped. Runs are capped and flushed on any state change, on the first non-mergeable instance, and at EndMode3D/EndDrawing. Measure with and without the clean flag | `todo` | scene arm: work ≤31.9 ms at N=500, all gates 0 diff; reported class counts; `arenaOverflows` 0 | |
| T4 | **Adopt or drop, with the number.** Pixel parity first, then speed. Adopt (default on, `PSP_MERGE_DRAWS=0` as the kill switch) only if every gate is 0 diff and the N=500 budget holds; otherwise record the delta and keep it opt-in or drop it | `todo` | decision table: arm × work × diff | |
| T5 | **Re-close.** Full gates (2D, MESH_SCENE, 061, 065, 066, 060, `basic_shapes`), sample 300 frames ×2, VRAM and main-RAM ledger updated, docs (§7/§9 and the phase page), restore pspGL, strict build | `todo` | every gate 0; thresholds hold; `2ab7378e` installed | |

### Design notes

**T1 — measure the boundary, not the whole call.** The per-call number already exists; what is missing is where it
goes. Put the counters in the functions that are actually called (rlgl's `rlEnableTexture`, `rlPushMatrix`,
`rlMultMatrixf`, `rlEnableStatePointer`, `rlColor4ub`, `rlDrawVertexArrayElements`; pspgu's `glDrawElements`,
`pspguArenaAlloc`, the index rebase loop, `pspguUploadMatrices`). Keep them behind `PSP_CALL_PROBE` and report
`draws/frame` so the sum can be checked. Do not time the clock: the 3.94 µs `sceKernelGetSystemTime` tail measured
in 070 is larger than most of the lines being timed — use batched deltas per frame where possible and say which
lines are below the resolution.

**T2 — the cheap half of the per-call cost, with no pixel risk.** `DrawMesh` re-enables the same three arrays and
rebinds the same texture for every cube. Cache the last bound pointers/stride/texture id in rlgl and skip the call
when nothing changed; pspgu already caches the matrix upload, so the remaining rlgl side is a few branches. The
matrix stack: `rlPushMatrix` copies 64 bytes and `rlPopMatrix` restores them; a single `rlMultMatrixf` on a saved
copy is equivalent if the stack depth is unchanged, which the merge window already tracks. Verify by comparing the
composed modelview bytes per draw (a `PSP_MATRIX_PROBE` check, not a pixel gate).

**T3 — the merge window and its flush set.** Consecutive `DrawMesh` calls are mergeable only while nothing else can
have drawn in between. Rely on raylib's funnel: every draw in the GL11 path goes through `rlBegin`/`rlVertex*`
(immediate), `rlSetTexture`, `rlDrawVertexArray*`, or the batch flush (`rlDrawRenderBatchActive`). Call
`pspMergeFlush()` from each of those, from `EndMode3D`, from `EndDrawing`, and whenever the key changes or a
non-clean instance appears. Missing a hook shows up as a moved pixel in a gate — treat that as a flush-set bug,
not a driver bug.

**T3 — expansion, exactly.** Take vertices from `mesh.vertices` / `mesh.normals` / `mesh.texcoords` and indices
from `mesh.indices` (they survive `UploadMesh`; `pspGePack` copies, it does not free them). Transform positions
by the draw transform only — world space — and submit with the **identity model matrix** so the GE applies the
current projection/view exactly as it does per object. Transform normals by the rotation part of the transform
(valid only for uniform scale, which the key enforces). The output vertex is GU's order:
`float2 uv, u32 colour, float3 normal, float3 position` → stride 36, which matches `vertexLayout` for
`GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_NORMAL_32BITF | GU_VERTEX_32BITF`. That the two agree is a precondition:
log `stride`/offsets once and check them against `vertexLayout`.

**T3 — the extension, and why it copies.** `pspguDrawMergedArrays` must copy the merged vertices and indices into
the current list's arena and then call `sceGuDrawArray` with the arena pointers. Do not pass the rlgl scratch
buffer: three framebuffers mean frame N−1's list is still executing when frame N builds its buffer, and rlgl's
rotation is not tied to pspgu's list rotation. The copy costs ~0.1 ms per frame at 243 KiB, which is noise next to
the 25 ms it removes. The known-clean flag sets a one-shot bit consumed by that draw: it skips `clipNeeds` but
changes nothing else (for a clean run, `clipClassify` would have returned 0 dirty and the same direct path would
run).

**T3 — arena budget.** A clean run of 282 cubes is 6,768 vertices × 36 B = 243 KiB plus ~20 KiB of indices.
`PSPGU_ARENA_BYTES` is 128 KiB per list today; raise the default to **384 KiB** (+512 KiB main RAM for two lists)
and record it in `memory-map.md`. Cap a single run at 256 KiB, flush, and continue; if a frame still exhausts the
arena, fall back to per-object for the remainder rather than resetting the arena (`arenaOverflows` is a gate).
VRAM is untouched: the merge costs no texture or framebuffer memory.

**T3 — the scene shows the rule, not the programme.** In the 10-wide grid at N=500 only a minority of the 282
drawn cubes are wholly inside the guard band; the rest straddle it and stay per-object. That is the rule working,
not a failure. To measure the programme's value, add `PACKED=1` to `matrix_probe MODE=scene`: the same COUNT cubes
in a tight cluster around the target, so nearly all are clean. **Pre-registered:** the grid arm may show little or
no gain; the packed arm is the deciding measurement for T4, and the grid arm's clean fraction is a reported number.

**T4 — parity is the gate, speed is the decision.** A merged vertex goes through float32 world-space rounding that
the per-object path does not. If that moves a pixel, the merge is not admissible by default; record the delta
(count and worst case) and either keep it opt-in or drop it. Do not adjust the gate to make the merge pass.

### Task log (append-only)

- 2026-11-23 — sprint written from 077 §10; scope and the AABB rule fixed before any code.

## 6. Budgets

| budget | limit | measured | tag |
|---|---|---|---|
| T1 attribution | sums to the measured per-call delta ±10 % | | |
| T2 per-call | ≤46 µs (from ~51) | | |
| T3 cube grid N=500 work | ≤31.9 ms (20 % below 39.85) | | |
| T3 packed arm N=500 work | ≥40 % below the same baseline | | |
| merged run share | report clean/dirty/culled per frame | | |
| captures | 0 differing pixels on every gate | | |
| sample N=50 | median ≤16.683, p95 ≤16.75, work ≤15.0 | | |
| waits | no full GE wait; report `listWaitMaxUs` | | |
| arena | `arenaOverflows` 0; merged run ≤256 KiB | | |
| memory | +512 KiB main RAM (arena); VRAM unchanged | | |

## 7. Measurements

Protocol: device only; two runs per point; reset between; captures after `DONE`.

| run | build | workload | metric | value | notes |
|---|---|---|---|---|---|
| baseline | b32 | `MODE=scene` N=100 / N=500 | work | **11.071 / 38.625 ms** | 070 §7 |
| baseline | b32 | same, `PSPGU_NO_CLIP=1` | work | **6.927 / 25.396** | clip 34 % at N=500 |
| baseline | b32 | `PSP_MATRIX_PROBE` | cull / compose / mult | **13 / 1.7 / 3.5 µs** per call | VFPU 1.1 opt-in |
| baseline | b32 | sample N=50, 300 ×2 | median / p95 / work | **16.683/16.684 · 16.728 · 11.96/11.99** | frozen |
| T1 | b32 | `PSP_CALL_PROBE` N=100/500 | µs/call by line | | sum vs delta |
| T2 | b32 | same | µs/call | | target ≤46 |
| T3 | b33 | grid N=500, merge 0/1 | work / classes | | ≤31.9 target |
| T3 | b33 | grid N=500, clean flag 0/1 | work | | isolates the scan |
| T3 | b33 | packed N=500, merge 0/1 | work / diff | | the deciding arm |
| T4 | b33 | every gate, merge 0/1 | diff / colours | | adopt or drop |
| final | b33 | sample N=50, 300 ×2 | median / p95 / work | | thresholds |
| gate | b33 | 2D/060/MESH_SCENE/061/065/066/shapes | diff / colours | | 0 / 284 |

## 8. Exit criteria

- [ ] T1: the attribution table is written and sums to the per-call delta within ±10 %
- [ ] T2: per-call ≤46 µs with identical composed matrices and 0 pixel change
- [ ] T3: merging exists behind the flag; class counts and merged draw count reported; clean flag A/B measured
- [ ] T4: adopt/drop recorded with the pixel and work numbers; the N=500 target is met or recorded as not met
- [ ] All gates 0 diff; sample thresholds hold; `arenaOverflows` 0; no full GE wait
- [ ] Arena growth and main-RAM cost recorded in `memory-map.md`; boot tag recorded; pspGL restored; strict build

## 9. Result

—

## 10. Follow-ups

- **If merging is adopted**: the same window is the home for a GL11 `DrawMeshInstanced` (currently GL33-only), and
  static instance sets could keep their expanded buffers between frames instead of rebuilding them.
- **If merging is dropped**: the per-call overhead stands; the next lever is a smaller call surface (T2 kept) and
  the cull's `glGetFloatv` cost, not more per-vertex work.
- **Cull instrumentation**: an rlgl matrix generation counter would remove the two `glGetFloatv` copies and the
  64-byte compares per mesh (077 §10, still open).
- **Clip**: with clean runs merged away, the remaining clip cost is the polygon clipper on genuinely dirty draws;
  VFPU `clipLerp`/`packClipVertex` is the only remaining experiment there.
- **VFPU**: still opt-in. The expansion transform in T3 is a scalar candidate for the same layer if merging lands.
