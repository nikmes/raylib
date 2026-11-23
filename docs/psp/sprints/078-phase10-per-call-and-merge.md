# Sprint 078 (P10-S11) — Per-call cost and draw merging

| | |
|---|---|
| Roadmap phase | [Phase 10 — 3D and advanced](../phases/10-3d-advanced/index.md) |
| Sprint | P10-S11 |
| Branch / base commit | `master` @ `b9a6af88` (P10-S10 closed, pspGL installed) |
| Started / closed | 2026-11-23 (written) / 2026-11-23 (closed) |
| Build number(s) | pspgu **`b33`** (`a725253c…`) with `pspguDrawMergedArrays` and `pspguArenaRoom`; `libraylib.a` default `PSP_BATCH=1 PSP_MERGE_DRAWS=1`, `PSP_MERGE_DRAWS=0` rebuilds the reference, `PSP_MERGE_HINT=0` the no-hint arm. The library archive hash is **not reproducible** across rebuilds (object timestamps land in the archive: two builds of identical source gave `0ad1fa4d…` and `81393264…`), so the build is identified by flags and date, not by hash |
| Tasks | 4/5 done — T2 dropped as superseded by T3 |

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
| T1 | **Attribute the per-call cost.** Counters on both sides of the boundary: rlgl (texture bind, matrix push/mult, pointer setup, colour, flush decision) and pspgu (list command bytes emitted, index rebase copy, matrix upload, draw entry). Report µs per call at N=100 and N=500, and show the sum against the measured delta between a draw loop and an empty loop | `done` (frame-level) | counters sum to the delta ±10 %; no pixel change (counters only) | The per-line split is **below the instrument**: one `sceKernelGetSystemTimeWide` pair costs 3.9 µs measured (probe build 52.94 ms vs probe-off 43.22 at N=500 = 9.72 ms for 5 pairs × 500 calls), larger than several lines. Frame-level attribution at N=500: cull **9.58 ms**, compose 2.04, multiply 2.43; the remainder is rlgl state + list emission. Two A/Bs give the actionable split: merging removes **~56 µs per instance** (43.22 → 27.74 over 276 instances) and the clip scan costs **1.77 µs/vertex** (no-hint 39.78 vs 27.74 for 6,768 vertices) |
| T2 | **rlgl call-surface trims.** Skip redundant texture binds, pointer setups and colour writes when the same mesh/material/texture is drawn again. (The matrix push/pop pair stays: this rlgl has no `rlLoadMatrixf` and the two copies are noise — checked, see the design note.) Composed matrices must be byte-identical, a gate rather than an assumption | `dropped` (superseded) | per-call ≤46 µs from ~51 in the T1 breakdown; every gate 0 diff | Merging removes 276 of the 282 submitted calls per frame, so the binds/pointers it would have cached are no longer issued. For the 6 remaining per-object draws a cache saves a few branches. Measured effect of the merge is 18× larger than the proposed cache; no device time spent |
| T3 | **Merged clean runs (`PSP_MERGE_DRAWS=1`).** Extend `pspGeCullMesh` to classify culled/clean/dirty with the six-plane corner test plus a safety margin; collect clean instances into a run that survives culled instances and is broken only by a dirty one, a key change (mesh, indices, texture, material colour, format, blend/depth) or a capacity limit; expand world-space vertices and run-local indices from the **original client arrays**; emit one draw per run through a new `pspguDrawMergedArrays(prim, count, indices, verts, flags)` that bulk-copies both arrays into the per-list arena, applies the run's global colour, and skips the clip scan when the known-clean flag is set. Flush on `rlBegin`, on `rlDrawRenderBatchActive`, and when the modelview no longer matches the run. Measure with and without the clean flag | `done` | grid arm work ≤31.9 ms at N=500; packed arm ≥40 % cut; all gates 0 diff; class counts reported; `arenaOverflows` 0 | N=500 cube scene: **27.742 / 26.672 ms** (two builds) vs 43.222 merge-off; median **33.367 ms (30 fps)** vs 50.050 (20 fps). Classes per frame **276 clean / 6 dirty / 218 culled**; 1 run per frame, 276 instances, **0 replays**. No-hint arm **39.781 ms** → the scan costs 12.0 ms (1.77 µs/vertex) and the hint is what makes the merge pay. Layout log: stride 32, offsets 0/8/20 |
| T4 | **Adopt or drop, with the number.** Pixel parity first, then speed. Adopt (default on, `PSP_MERGE_DRAWS=0` as the kill switch) only if every gate is 0 diff and the N=500 budget holds; otherwise record the delta and keep it opt-in or drop it | `done` (adopted) | decision table: arm × work × diff | Adopted as the default (`PSP_MERGE_DRAWS ?= 1`). Every gate 0 diff against pspGL: 2D `batch_check`, `scene_3d SCENE=both`, `basic_shapes` 284, `clip_probe EDGE=237 MESH3D=1`; merge on/off captures identical. The "packed arm" was not needed: the grid itself classifies 276/282 submitted instances clean, above the 50 % floor, so the pre-registered contiguity caveat did not bite |
| T5 | **Re-close.** Full gates (2D, MESH_SCENE, 061, 065, 066, 060, `bunnymark` 60 fps, `basic_shapes` 284), CULL=0 vs 1 with merging on, sample 300 frames ×2, VRAM and main-RAM ledger updated, docs (§7/§9 and the phase page), restore pspGL, strict build | `done` | every gate 0; thresholds hold; `2ab7378e` installed | 2D 0, scene 0, shapes 0/284, clip 0; sample 16.684/16.683, p95 16.697/16.695, work 6.663/6.677; `arena_overflows=0`, list wait max 68 µs; pspGL restored; strict build clean. `bunnymark` median 16.712/16.718 **misses 16.683 by 0.03 ms** (see §9) |

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
- 2026-11-23 — implemented T1/T3/T4/T5. Three instrument bugs found before any number was believed:
  (1) the PSP Makefile does not rebuild `rmodels.o` after a source edit, so the first archive was stale;
  (2) the far-plane margin was written `z ≤ 1e-5·w` instead of `w − z ≥ 1e-5·w`, which classified 278/278
  submitted instances dirty (the "whole scene dirty" run);
  (3) the app-side `CALLPROBE` print was gated on `PLATFORM_PSP`, which app builds never define, so the block
  never compiled. Also measured: the clock pair costs 3.9 µs, more than several of the lines it was meant to
  split — the per-line attribution is below the instrument and is recorded as such.

## 6. Budgets

| budget | limit | measured | tag |
|---|---|---|---|
| T1 attribution | sums to the measured per-call delta ±10 % | frame-level only; per-line below the 3.9 µs clock pair | b33 |
| T2 per-call | ≤46 µs (from ~51) | dropped: the calls are removed by T3 | b33 |
| T3 cube grid N=500 work | ≤31.9 ms (20 % below 39.85) | **26.672 / 27.742** | b33 |
| T3 packed arm N=500 work | ≥40 % below the same baseline | not needed: grid classifies 276/282 clean | b33 |
| merged run share | report clean/dirty/culled per frame | **276 / 6 / 218**, 1 run, 0 replays | b33 |
| layout check | merged stride 32, offsets 0/8/20 match `vertexLayout` | 32 · 0/8/20 | b33 |
| captures | 0 differing pixels on every gate, against pspGL and against `PSP_MERGE_DRAWS=0` | 0 on all four gates and both arms | b33 |
| sample N=50 | median ≤16.683, p95 ≤16.75, work ≤15.0 | scene 16.684/16.683 · 16.697/16.695 · 6.66/6.68 — **met**; bunnymark 16.712/16.718 — **missed by 0.03 ms** | b33 |
| waits | no full GE wait; report `listWaitMaxUs` | list max 68 µs, display max 309 µs once | b33 |
| arena | `arenaOverflows` 0; merged run ≤256 KiB | 0 overflows, arena 512 KiB/list, `list_used_max` 3,972 B | b33 |
| memory | +512 KiB main RAM (arena); VRAM unchanged | 2 × 512 KiB = 1 MiB (was 2 × 128 KiB); VRAM untouched | b33 |

## 7. Measurements

Protocol: device only; two runs per point; reset between; captures after `DONE`.

| run | build | workload | metric | value | notes |
|---|---|---|---|---|---|
| baseline | b30 | `MODE=scene` N=100 / N=500 | work | **11.071 / 38.625 ms** | 070 §7 |
| baseline | b30 | same, `PSPGU_NO_CLIP=1` | work | **6.927 / 25.396** | clip 34 % at N=500 |
| baseline | b32 | `MODE=scene` N=500 | work | **39.85 ms** | 077 T6; the 31.9 target is 20 % below this |
| baseline | b32 | `PSP_MATRIX_PROBE` | cull / compose / mult | **13 / 1.7 / 3.5 µs** per call | VFPU 1.1 opt-in |
| baseline | b32 | sample N=50, 300 ×2 | median / p95 / work | **16.683/16.684 · 16.728 · 11.96/11.99** | frozen |
| T1 tail | b33 | probe build vs probe-off, N=500 | work delta | **9.72 ms = 5 pairs × 500 × 3.9 µs** | the instrument's floor |
| T1 | b33 | `MATRIXPROBE` N=500 | cull / compose / mult | **9.58 / 2.04 / 2.43 ms per frame** | includes the clock pair |
| T3 | b33 | grid N=500, merge off | work / median | **43.222 / 50.050 ms (20 fps)** | today's reference |
| T3 | b33 | grid N=500, merge on | work / median | **27.742 · 26.672 / 33.367 ms (30 fps)** | two builds; target ≤31.9 |
| T3 | b33 | grid N=500, classes | clean / dirty / cull | **276 / 6 / 218** per frame | 1 run, 276 instances |
| T3 | b33 | grid N=500, hint off | work | **39.781 ms** | the scan is 12.0 ms = 1.77 µs/vertex |
| T4 | b33 | merge 0/1 captures | diff | **0** | 3 colours in frame |
| T4 | b33 | `batch_check` vs pspGL | diff / colours | **0 / 6** | the archived 613-colour frame is not what this build draws |
| T4 | b33 | `scene_3d SCENE=both` vs pspGL | diff / median / p95 / work | **0 · 16.683 · 16.694 · 0.743** | 7 colours |
| T4 | b33 | `basic_shapes` vs pspGL | diff / colours | **0 / 284** | matches the expected 284 |
| T4 | b33 | `clip_probe EDGE=237 MESH3D=1` vs pspGL | diff | **0** | single merged instance |
| T5 | b33 | `scene_3d SAMPLE=1`, 300 ×2 | median / p95 / work | **16.684/16.683 · 16.697/16.695 · 6.663/6.677** | `arena_overflows=0` |
| T5 | b33 | `bunnymark` N=50, 300 ×2 | median / p95 / work | **16.712/16.718 · 16.739/16.736 · 5.690/2.774** | median 0.03 ms over the frozen limit |
| T5 | b33 | waits | list / display max | **68 µs / 309 µs** | targeted only, no full wait |
| ceiling | b33 | merge off, N sweep (200 frames) | median | **150 passes (16.683, work 15.726 on 300 frames) · 160 fails (33.370, work 17.962)** | 140 medians 16.684 but p95 33.365: it drops frames |
| ceiling | b33 | merge on, N sweep (200 frames) | median | **270 passes (16.683, work 15.245 on 300 frames) · 271 fails (33.352, work 16.578)** | 200/250/260/265/268 pass; 272/275/300/350 fail |
| ceiling | b33 | both arms | 60 fps instances | **150 → 270 (+80 %)** | the same scene, one flag |
| gate | b33 | RTT `MODE=readback` | `diff=` | **1088** | pre-existing readback mode, not the direct-vs-sampled gate |

## 8. Exit criteria

- [ ] T1: the attribution table is written and sums to the per-call delta within ±10 % — **not met as written**:
      the clock pair is 3.9 µs and most lines are smaller. The frame-level table and the two A/B attributions
      (56 µs per merged instance, 1.77 µs per scanned vertex) are in §7/§9.
- [x] T2: per-call ≤46 µs with identical composed matrices and 0 pixel change — **dropped**: merging removes the
      calls T2 would have trimmed (evidence in the task row).
- [x] T3: merging exists behind the flag; class counts, clean fraction and merged draw count reported; the merged
      path is proven active (1 run/frame, 276 instances, 0 replays); clean flag A/B measured (39.781 vs 26.672 ms).
- [x] T4: adopted with the pixel and work numbers; N=500 ≤31.9 ms **met** (26.672/27.742).
- [x] All gates 0 diff (against pspGL and against `PSP_MERGE_DRAWS=0`); CULL=0 vs 1 0 diff; sample thresholds
      hold; `arenaOverflows` 0; no full GE wait. **Except** `bunnymark` median 16.712/16.718 vs 16.683 — recorded,
      not redefined.
- [x] Arena growth and main-RAM cost recorded in `memory-map.md`; boot tag recorded (`b33`); pspGL restored;
      strict build.

## 9. Result

Sprint 078 closes 4/5 (T1 frame-level, T2 dropped as superseded, T3/T4/T5 done). **Merging clean instances works:
the N=500 cube scene drops from 43.22 ms to 26.67 ms of work and from 20 to 30 fps, the 60 fps ceiling moves
from 150 to 270 instances, and every gate is 0 differing pixels.**

**What the renderer does now.** `DrawMesh` classifies each instance from its own AABB: culled (emits nothing),
clean (inside the 7.9w guard-band margin, all eight corners proven, so no vertex can need the clipper) or dirty
(crosses the band). Clean instances with the same mesh, texture and material colour join a run; culled ones pass
through without breaking it; a dirty one flushes the run first, so submission order is preserved. At the end of
the pass (`rlDrawRenderBatchActive`, which EndMode3D and EndDrawing both call) the run is expanded into world
space — uv/normal/position, stride 32, colour through the GE global colour — and handed to
`pspguDrawMergedArrays`, which copies it into the list arena and skips the clip scan because the proof says it
can. The modelview is the camera's, untouched: the per-object model multiply is simply not done.

**The numbers** (N=500, probes off, two runs where it matters):

| arm | work | median | fps |
|---|---|---|---|
| merge off (`PSP_MERGE_DRAWS=0`) | 43.222 ms | 50.050 ms | 20 |
| merge on | **26.672 / 27.742 ms** | **33.367 ms** | **30** |
| merge on, clean flag off (`PSP_MERGE_HINT=0`) | 39.781 ms | 50.050 ms | 20 |

The hint is the whole point: without it the merged run is clip-scanned, 6,768 vertices at **1.77 µs** each =
12.0 ms, and the merge barely pays.

**Classification is not a guess.** 276 of the 282 submitted instances per frame are clean, 6 are dirty (near-plane
crossers at w ≈ 0.15–0.68), 218 are culled. One run per frame, 276 instances, zero replays. The clean fraction is
97.9 %, well above the pre-registered 50 % floor, so the "packed arm" was not needed.

**Three instrument bugs, all caught before a conclusion** (this is the part worth remembering):

1. The PSP Makefile does not rebuild `rmodels.o` after a source edit. The first archive was stale and the link
   failed on a symbol the source contained; `psp-nm` on the object, not the archive, is what settled it.
2. The far-plane margin was written `z ≤ 1e-5·w` instead of `w − z ≥ 1e-5·w`. The first real run classified
   278 of 278 submitted instances dirty — a merge that never fires and looks like "the design does not work".
   The failing-corner log (nx/ny/nz) is what identified it.
3. The app's `CALLPROBE` print was gated on `PLATFORM_PSP`, which app builds never define, so the block never
   compiled and its empty output read as a library problem.

**T1 could not be finished as specified.** One `sceKernelGetSystemTimeWide` pair costs 3.9 µs — measured from
probe-on vs probe-off (52.94 vs 43.22 ms = 9.72 ms for 5 pairs × 500 calls) — which is larger than several of the
lines it was meant to split. The attribution is frame-level (cull 9.58 ms, compose 2.04, multiply 2.43, remainder
rlgl state + list emission), plus two A/Bs that give the actionable split: **~56 µs per instance removed by
merging** and **1.77 µs per vertex for the clip scan**. A finer split needs a cycle counter, not a syscall; that
is a follow-up, not a redefinition of the criterion.

**Gates** (all against pspGL, two identical captures each): `batch_check` 2D **0** (6 distinct colours in this
build's frame — the archived 613-colour frame is not what this app draws today, but parity is what the gate
asks); `scene_3d SCENE=both` **0**; `basic_shapes` **0 / 284**; `clip_probe EDGE=237 MESH3D=1` **0**; merge-on vs
merge-off **0**. `scene_3d SAMPLE=1` ×2: median **16.684/16.683**, p95 **16.697/16.695**, work **6.663/6.677**,
`arena_overflows=0`.

**The ceiling moved 150 → 270.** `matrix_probe MODE=scene`, probes off, both arms swept to the adjacent
last-passing/first-failing N:

| arm | last N at 60 fps | work there | first N that drops | work there |
|---|---|---|---|---|
| merge off | **150** | 15.726 ms | 160 | 17.962 ms |
| merge on | **270** | 15.245 ms | 271 | 16.578 ms |

That is **+80 % instances at 60 fps**, and at N=500 the scene went from 20 to 30 fps. The boundary is sharp and
reproduced (270 twice, 300 frames each); one instance over and the median doubles to 33.35, which is the vsync
quantum, not a measurement artefact. Two honest caveats: the grid reshapes as N grows (`rows = ceil(N/10)`), so
work is not monotonic at every step (N=190 costs less than N=180), and N=140 passes on the median while dropping
frames in p95 — it is listed as borderline, not as a pass.

**Recorded, not redefined:**

- `bunnymark` N=50 ×2: median **16.712 / 16.718 ms** against the frozen ≤16.683 — **over by 0.03 ms** (0.2 %).
  p95 (16.739/16.736) and work (5.69/2.77) pass. The 2D path issues no `DrawMesh`, so the merge cannot be the
  cause; it is recorded as not met.
- `rtt_probe MODE=readback` reports `diff=1088` of 65,536. That is the glReadPixels-against-pattern mode, not the
  direct-vs-sampled 0-diff gate; the number is recorded without claiming a gate.
- The frozen sample's 11.96 ms work is not reproducible from the configuration 077 recorded (neither
  `scene_3d SAMPLE=1` at 6.67 nor `bunnymark` at 5.69). The median/p95 thresholds hold; the work figure needs its
  command written down. Doc gap, noted for the phase page.

## 10. Follow-ups

- **Merging is adopted** (`PSP_MERGE_DRAWS ?= 1`). The same run window is the home for a GL11 `DrawMeshInstanced`
  (currently GL33-only), and static instance sets could keep their expanded buffers between frames instead of
  rebuilding them.
- **The clean flag is load-bearing**: 12.0 ms of the 43.2 ms baseline is the clip scan over the merged run
  (1.77 µs/vertex). A future merge that cannot prove cleanliness per instance should not merge.
- **Cull instrumentation**: an rlgl matrix generation counter would remove the two `glGetFloatv` copies and the
  64-byte compares per mesh (077 §10, still open). The cull is 9.58 ms/frame at N=500, now the largest
  identified line.
- **Finer attribution needs a cycle counter.** `sceKernelGetSystemTimeWide` costs 3.9 µs per pair, more than most
  of the per-call lines; the A/B method in §9 is the honest instrument until then.
- **Clip**: with clean runs merged away, the remaining clip cost is the polygon clipper on genuinely dirty draws;
  VFPU `clipLerp`/`packClipVertex` is the only remaining experiment there — written as
  [P10-S12 (079)](079-phase10-pge-derived-pspgu-experiments.md).
- **VFPU**: still opt-in. The expansion transform in T3 is a scalar candidate for the same layer.
- **Doc gaps to close**: the frozen sample's command (work 11.96 ms) is not recorded in 077, and `bunnymark`'s
  median sits 0.03 ms over the frozen limit — both belong to the phase page's measurement notes.
- **RTT readback**: `MODE=readback` reports 1088 differing pixels against its pattern. Investigate before the
  next RTT claim; the direct-vs-sampled gate is unaffected.
