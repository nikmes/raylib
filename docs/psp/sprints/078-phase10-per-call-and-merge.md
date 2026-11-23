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
N=500 — 2.4× the 16.683 ms frame period. Sprint 077 attributed that to the per-call path (~51 µs per `DrawMesh`,
~25 ms at N=500) and deferred merging **without a prototype**. This sprint answers the question with numbers,
under one constraint that cannot be relaxed: **a merged batch must keep a per-instance AABB test**, because the
naive version hands the GE one big bounding box and the clip pass then classifies every vertex.

**The measured baseline** (all device numbers; sources in 069 §7, 070 §7, 077 §7/§9):

| line | number | source |
|---|---|---|
| cube scene, N=500, `PSP_MATRIX_PROBE=1`, **b32** | **39.85 ms** work, 500 draws/frame | 077 T6 |
| cube scene, N=500, b30 | 38.625 ms | 070 §7 |
| — of which clip off (`PSPGU_NO_CLIP=1`), b30 | 25.40 ms | 077 §7 |
| clip pass per submitted draw, b30 | **~47 µs** (~1.4 µs/vertex) | 069 T1 |
| per-`DrawMesh` baseline, clip off, b30 | **~51 µs/call** (25.4 ms / 500) | 077 T4 |
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
the cull already performs is therefore exact up to float ordering (T3 adds a margin for that), and it yields three
classes: **culled**, **clean** (mergeable, provably needs no clipping), **dirty** (crosses the guard band; stays on
the per-object path). That is the "per-instance AABB constraint": merge only what is provably clean, keep
everything else per object.

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
3. The per-instance AABB rule is demonstrated, not asserted: every run reports how many instances were culled,
   merged clean, and kept dirty, and how many merged draws were emitted.
4. **The clean fraction is a reported number, not an assumption.** The guard band is 8× the screen half-extent, so
   an instance is dirty only when it spans a huge clip range (a near-plane crosser, or geometry larger than the
   view) — most drawn objects should be clean. If the grid's clean fraction is below 50 %, the 31.9 ms target is
   out of reach arithmetically for that scene; the packed arm then decides T4 and the grid number is recorded as a
   property of the scene, not as a failure of the merge.

## 2. Why now

The audit and 077 removed the per-vertex fat (clip early-out) and measured the VFPU. What remains is per call:
500 calls × 51 µs. The 2D path already proves the pattern works on this hardware — one draw for thousands of
sprites — but the 3D path cannot use it directly because each mesh has its own transform and its own visibility.
077 deferred the prototype; this sprint is that prototype, with the visibility rule that makes it safe.

## 3. Scope

**In:** `src/rmodels.c` (cull classification, merge run and expansion buffers), `src/rlgl.h` (per-call trims and
the two flush hooks), `tools/pspgu/gl_draw.c` + `egl.c` + `pspgu_internal.h` (the merged-draw entry point, arena
sizing), `psp-tests/matrix_probe` (a merge-friendly scene arm), the docs.

**Out:** GUM, VFPU work (stays opt-in), the ME, texture formats, the 2D batcher, public raylib API changes,
skinning, the framebuffer rotation (P10-S9), and any change to the clip predicate itself.

## 4. Preconditions

- [ ] `master` @ `b9a6af88`; P10-S10 closed; `tools/pspgu-build.sh --check` reports pspGL `2ab7378e` installed
      and `tools/pspgu/libGL.a` at `b32` (`0d354986…`)
- [ ] `matrix_probe` builds with `MODE=scene PROBE=0/1`; `scene_3d` builds with `SCENE=both`, `SAMPLE=1`
- [ ] The §7 baseline reproduces within the repeatability band
- [ ] The A/B is **two `libraylib.a` builds** (`PSP_MERGE_DRAWS=0/1`, as `PSP_BATCH=0` in Phase 4); the app
      binary is identical, so the same `.prx` diff is a library change only
- [ ] The user has authorised the first PSPLink run

## 5. Tasks

Status vocabulary — `todo` · `doing` · `done` · `blocked` · `dropped`.

| ID | task | status | verification | evidence |
|---|---|---|---|---|
| T1 | **Attribute the per-call cost.** Counters on both sides of the boundary: rlgl (texture bind, matrix push/mult, pointer setup, colour, flush decision) and pspgu (list command bytes emitted, index rebase copy, matrix upload, draw entry). Report µs per call at N=100 and N=500, and show the sum against the measured delta between a draw loop and an empty loop | `todo` | counters sum to the delta ±10 %; no pixel change (counters only) | |
| T2 | **rlgl call-surface trims.** Skip redundant texture binds, pointer setups and colour writes when the same mesh/material/texture is drawn again. (The matrix push/pop pair stays: this rlgl has no `rlLoadMatrixf` and the two copies are noise — checked, see the design note.) Composed matrices must be byte-identical, a gate rather than an assumption | `todo` | per-call ≤46 µs from ~51 in the T1 breakdown; every gate 0 diff | |
| T3 | **Merged clean runs (`PSP_MERGE_DRAWS=1`).** Extend `pspGeCullMesh` to classify culled/clean/dirty with the six-plane corner test plus a safety margin; collect clean instances into a run that survives culled instances and is broken only by a dirty one, a key change (mesh, indices, texture, material colour, format, blend/depth) or a capacity limit; expand world-space vertices and run-local indices from the **original client arrays**; emit one draw per run through a new `pspguDrawMergedArrays(prim, count, indices, verts, flags)` that bulk-copies both arrays into the per-list arena, applies the run's global colour, and skips the clip scan when the known-clean flag is set. Flush on `rlBegin`, on `rlDrawRenderBatchActive`, and when the modelview no longer matches the run. Measure with and without the clean flag | `todo` | grid arm work ≤31.9 ms at N=500; packed arm ≥40 % cut; all gates 0 diff; class counts reported; `arenaOverflows` 0 | |
| T4 | **Adopt or drop, with the number.** Pixel parity first, then speed. Adopt (default on, `PSP_MERGE_DRAWS=0` as the kill switch) only if every gate is 0 diff and the N=500 budget holds; otherwise record the delta and keep it opt-in or drop it | `todo` | decision table: arm × work × diff | |
| T5 | **Re-close.** Full gates (2D, MESH_SCENE, 061, 065, 066, 060, `bunnymark` 60 fps, `basic_shapes` 284), CULL=0 vs 1 with merging on, sample 300 frames ×2, VRAM and main-RAM ledger updated, docs (§7/§9 and the phase page), restore pspGL, strict build | `todo` | every gate 0; thresholds hold; `2ab7378e` installed | |

### Design notes

**T1 — measure the boundary, not the whole call.** The per-call number already exists; what is missing is where it
goes. Put the counters in the functions that are actually called (rlgl's `rlEnableTexture`, `rlPushMatrix`,
`rlMultMatrixf`, `rlEnableStatePointer`, `rlColor4ub`, `rlDrawVertexArrayElements`; pspgu's `glDrawElements`,
`pspguArenaAlloc`, the index rebase loop, `pspguUploadMatrices`). Keep them behind `PSP_CALL_PROBE` and report
`draws/frame` so the sum can be checked. Do not time the clock: the 3.94 µs `sceKernelGetSystemTime` tail measured
in 070 is larger than most of the lines being timed — use batched deltas per frame where possible and say which
lines are below the resolution.

**T2 — the cheap half, with no pixel risk.** `DrawMesh` re-enables the same three arrays and rebinds the same
texture for every cube. Cache the last bound pointers/stride/texture id in rlgl and skip the call when nothing
changed; pspgu already caches the matrix upload, so the rlgl side is a few branches. **The matrix stack is not the
lever and was checked:** this rlgl has no `rlLoadMatrixf`, and `rlPushMatrix`/`rlPopMatrix` are two 64-byte copies
— noise against 51 µs. Leave the push/pop pair alone. Verify the trims by comparing the composed modelview bytes
per draw (a `PSP_MATRIX_PROBE` check, not a pixel gate), since a skipped bind must not change what the GE sees.

**T3 — the run and its flush set.** Two facts decide this, both verified in the tree. `rlDrawRenderBatchActive()`
is called at the top of **EndMode3D and EndDrawing** (and by every batch flush), so it is the single ordering hook
for the end of a 3D pass. And **texture binds must not flush**: `DrawMesh` calls `rlEnableTexture` before the
classification, and the culled branch calls `rlDisableTexture` — at N=500 that is 218 cubes, so a texture-driven
flush would split the run 218 times and merging would be worthless. Therefore:

- **flush** on `rlBegin` (any immediate-mode draw), on `rlDrawRenderBatchActive`, when the key changes, when the
  current modelview differs from the run's, when a dirty instance arrives, and on capacity;
- **do not flush** on `rlEnableTexture` / `rlDisableTexture`; the run records its texture and binds it when the run
  is emitted, so a bind between instances cannot reorder anything the merge emits;
- **culled instances do not break a run** — they emit nothing, so a run may span them; only a *dirty* instance
  forces the run out first (order preservation: the dirty draw must come after everything before it).

Missing a hook shows up as a moved pixel in a gate — treat that as a flush-set bug, not a driver bug.

```
DrawMesh(mesh, material, transform):
    if (!enabled || !eligible(mesh, transform)) { pspMergeFlush(); per_object(); return; }
    cls = classify(mesh, transform)             // culled / clean / dirty
    if (cls == CULLED) return                   // emits nothing; does not break the run
    if (cls == DIRTY) { pspMergeFlush(); per_object(); return; }
    if (key_changed() || modelview_changed() || !room_for(mesh)) pspMergeFlush();
    run_append(mesh, transform);

pspMergeFlush():
    if (run.count == 0) return;
    expand(run);                                // world positions, rotation normals, run-local indices
    bind(run.texture); color(run.colour);
    ok = pspguDrawMergedArrays(GL_TRIANGLES, run.icount, run.idx, run.verts, FLAG_CLEAN);
    if (!ok) replay_per_object(run);
    run.count = 0;
```

**T3 — eligibility.** An instance enters a run only if all of these hold, otherwise it is drawn per object with the
run flushed first: bounds exist (`pspGeStoreBounds` found an AABB; without one the rule cannot be applied and
culling itself is off), `mesh.indices != NULL`, `mesh.texcoords != NULL`, `mesh.normals != NULL`,
`mesh.colors == NULL` (that case already takes the generic colour path), `animVertices == NULL` and
`animNormals == NULL`, `rlpspCulling` is on (the classification *is* the cull; with `CULL=0` merging must be inert,
and that is the equivalence gate), and the transform is translation + rotation + uniform scale (non-uniform scale
or shear would need an inverse-transpose normal matrix; fall back instead). The key is (vertices pointer, indices
pointer, texture id, material diffuse colour, format bits); blend and depth state are global and must not change
inside a run.

**T3 — expansion, exactly.** Take vertices from `mesh.vertices` / `mesh.normals` / `mesh.texcoords` and indices
from `mesh.indices` (they survive `UploadMesh`; `pspGePack` copies, it does not free them). Transform positions
by the draw transform only — **world space** — then draw with the camera's modelview **unchanged**: make no
`rlMultMatrixf` model call at all. (Do *not* `rlLoadIdentity`; that would wipe the view matrix and the whole scene
would leave the screen.) Transform normals by the rotation part. The run vertex is GU's order without a colour
field: `float2 uv, float3 normal, float3 position` → offsets 0 / 8 / 20, stride 32, which is `vertexLayout` for
`GU_TEXTURE_32BITF | GU_NORMAL_32BITF | GU_VERTEX_32BITF`. The colour is the GE global colour, exactly as on the
per-object path (`applyGlobalColor`), which is why material colour is in the key. Log stride/offsets once and check
them against `vertexLayout`. Build run-local indices (`index + i*vertexCount`) in the rlgl scratch.

**T3 — the extension, and why it copies.** `pspguDrawMergedArrays(prim, count, indices, verts, flags)` copies both
arrays into the current list's arena and calls `sceGuDrawArray` with the arena pointers, after `pspguUploadMatrices`,
`guVertexFormat() | GU_INDEX_16BIT` (the same format assembly `glDrawElements` uses) and `applyGlobalColor`.
Do not pass the rlgl scratch buffer: three framebuffers mean frame N−1's list is still executing when frame N
builds its buffer, and rlgl's rotation is not tied to pspgu's list rotation. **Contract:** returns 0 when the arena
cannot take the run; rlgl then draws those instances per object (never a reset — `arenaOverflows` is a gate). The
known-clean flag is one-shot for that draw: it skips `clipNeeds` and changes nothing else, because a clean run
would have classified 0 dirty and taken the same direct path.

**T3 — the classification margin.** The cull composes `(P*V)*M`; the clipper composes through the modelview and
projection separately. The two float32 orders disagree by a hair at the guard band, so classify with a margin
around 8.0 — require |x|,|y| ≤ 7.9w and z within ±0.95w — and rely on the GE's own band (measured 8.53 NDC) for
the slack. A vertex the CPU calls clean can then never need the clipper in the clipper's own arithmetic.

**T3 — arena budget.** A clean run of 282 cubes is 6,768 vertices × 32 B = 216,576 B (212 KiB) plus 20,304 B of
indices. `PSPGU_ARENA_BYTES` is 128 KiB per list today; raise the default to **384 KiB** (+512 KiB main RAM for
two lists) and record it in `memory-map.md`. Cap a single run at 256 KiB, flush, and continue; the extension's
0 return covers the rest. VRAM is untouched: the merge costs no texture or framebuffer memory.

**T3 — the scene and the clean fraction.** The guard band is 8× the screen half-extent, so in the 10-wide grid a
cube is dirty only if it is a near-plane crosser or larger than the view — expected to be a handful, not a
minority. The `PACKED=1` arm added to `matrix_probe MODE=scene` (the same COUNT cubes in a tight cluster) removes
the near-plane crossers and bounds the best case, so the two arms together bracket the achievable gain. Report both
fractions; if the grid fraction is below 50 %, stop and explain why before believing the merge numbers.

**T4 — parity is the gate, speed is the decision.** The GE applies model transforms in **float24** (15 fraction
bits; phase page, "float24 matrix precision is a trap"), so a CPU float32 world-space expansion *cannot* be
bit-identical to the per-object path by construction — the difference is in the rounding of the model stage, not
in the view/projection the GE still applies. Expect a small diff on distant or edge-on geometry, and measure it:
count and worst case. The gate is the same `.prx` captured against the pspGL archive and against the
`PSP_MERGE_DRAWS=0` library build. If that moves a pixel, the merge is not admissible by default; record the delta
and keep it opt-in or drop it. Do not adjust the gate to make the merge pass.

### Task log (append-only)

- 2026-11-23 — sprint written from 077 §10; scope and the AABB rule fixed before any code.
- 2026-11-23 — implementation-readiness review against the tree. Corrected: the flush set (texture binds must not
  flush, or 218 culls split every run; `rlDrawRenderBatchActive` covers EndMode3D and EndDrawing; culled instances
  do not break a run), the layout (32 B without a colour field, GE global colour), the ambiguous "identity model
  matrix" (skip the model multiply; do **not** `rlLoadIdentity`), T2's matrix-stack item (no `rlLoadMatrixf` in
  this rlgl; push/pop is noise), the classification margin (7.9w / ±0.95w), and the baseline table's build labels
  (39.85 is b32; 38.625 and 25.40 are b30).

## 6. Budgets

| budget | limit | measured | tag |
|---|---|---|---|
| T1 attribution | sums to the measured per-call delta ±10 % | | |
| T2 per-call | ≤46 µs (from ~51) | | |
| T3 cube grid N=500 work | ≤31.9 ms (20 % below 39.85) | | |
| T3 packed arm N=500 work | ≥40 % below the same baseline | | |
| merged run share | report clean/dirty/culled per frame; grid clean fraction ≥50 % or explain | | |
| layout check | merged stride 32, offsets 0/8/20 match `vertexLayout` | | |
| captures | 0 differing pixels on every gate, against pspGL and against `PSP_MERGE_DRAWS=0` | | |
| sample N=50 | median ≤16.683, p95 ≤16.75, work ≤15.0 | | |
| waits | no full GE wait; report `listWaitMaxUs` | | |
| arena | `arenaOverflows` 0; merged run ≤256 KiB | | |
| memory | +512 KiB main RAM (arena); VRAM unchanged | | |

## 7. Measurements

Protocol: device only; two runs per point; reset between; captures after `DONE`.

| run | build | workload | metric | value | notes |
|---|---|---|---|---|---|
| baseline | b30 | `MODE=scene` N=100 / N=500 | work | **11.071 / 38.625 ms** | 070 §7 |
| baseline | b30 | same, `PSPGU_NO_CLIP=1` | work | **6.927 / 25.396** | clip 34 % at N=500 |
| baseline | b32 | `MODE=scene` N=500 | work | **39.85 ms** | 077 T6; the 31.9 target is 20 % below this |
| baseline | b32 | `PSP_MATRIX_PROBE` | cull / compose / mult | **13 / 1.7 / 3.5 µs** per call | VFPU 1.1 opt-in |
| baseline | b32 | sample N=50, 300 ×2 | median / p95 / work | **16.683/16.684 · 16.728 · 11.96/11.99** | frozen |
| T1 | b32 | `PSP_CALL_PROBE` N=100/500 | µs/call by line | | sum vs delta |
| T2 | b32 | same | µs/call | | target ≤46 |
| T3 | b33 | grid N=500, merge 0/1 | work / classes / clean fraction | | ≤31.9 target |
| T3 | b33 | grid N=500, clean flag 0/1 | work | | isolates the scan |
| T3 | b33 | packed N=500, merge 0/1 | work / diff | | the deciding arm |
| T3 | b33 | first frame | stride / offsets log | | 32 · 0/8/20 |
| T4 | b33 | every gate, merge 0/1 | diff / colours | | adopt or drop |
| T4 | b33 | CULL=0 vs CULL=1, merge on | diff | | 0 expected |
| final | b33 | sample N=50, 300 ×2 | median / p95 / work | | thresholds |
| gate | b33 | 2D/060/MESH_SCENE/061/065/066/shapes | diff / colours | | 0 / 284 |

## 8. Exit criteria

- [ ] T1: the attribution table is written and sums to the per-call delta within ±10 %
- [ ] T2: per-call ≤46 µs with identical composed matrices and 0 pixel change
- [ ] T3: merging exists behind the flag; class counts, clean fraction and merged draw count reported; the merged
      path is proven active (boot log shows runs > 0, not just "no crash"); clean flag A/B measured
- [ ] T4: adopt/drop recorded with the pixel and work numbers; the N=500 target is met or recorded as not met
- [ ] All gates 0 diff (against pspGL and against `PSP_MERGE_DRAWS=0`); CULL=0 vs 1 0 diff; sample thresholds
      hold; `arenaOverflows` 0; no full GE wait
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
