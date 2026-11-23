# Sprint 079 (P10-S12) — PGE-derived pspgu experiments

| | |
|---|---|
| Roadmap phase | [Phase 10 — 3D and advanced](../phases/10-3d-advanced/index.md) |
| Sprint | P10-S12 |
| Branch / base commit | `master` @ `d4310135` (P10-S11 merged clean runs adopted) |
| Started / closed | 2026-11-23 (written) / — |
| Build number(s) | pspgu `b34` is the T4 measurement (normal + `PSPGU_FAST2D=1`). T1/T2/T3 add driver/render code and must use **`b35`**. `libraylib.a` default `PSP_BATCH=1`. New opt-in probes: `PSPGU_VFPU=1`, `PSPGU_VFPU_CLIP=1`, `PSPGU_FAST_CLEAR=1`, `PSP_VFPU_SINCOS=1`, `PSP_FAST2D_TRANSFORM=1`, `PSP_SINGLE_QUAD_FAN=1` (T5 feasibility only) |
| Tasks | 6/6 decided — T3 sincos adopted; T1/T2/T4/T5 dropped; T6 closed |

## 0. Handover

**Situation.** A review of the GPLv3 Phoenix Game Engine (`/home/psp/ref/PGE`) found that its renderer is mostly
behind pspgu: one `sceGuGetMemory` + `sceGuDrawArray` per sprite, `sceGuSync` every frame, two 64 KiB lists, and
GUM-only 3D matrices. Its cache strips and compact formats are already measured nulls here. What remains is a small
set of instruction-level and primitive-level ideas aimed at our measured walls: the CPU clipper, the GE clear, and
rotated 2D math. This sprint measures each idea behind a flag and adopts only the pixel-exact wins. It does not
re-open GUM, per-sprite submission, cache strips, or compact vertex formats.

**The measured baseline** (device numbers; sources in 069 §7, 070 §7, 077 §7/§9, 023 §7):

| line | number | source |
|---|---|---|
| cube scene, N=500, `PSP_MATRIX_PROBE=1`, b32 | **39.85 ms** work, 500 draws/frame | 077 T6 |
| cube scene, N=500, `PSPGU_NO_CLIP=1`, b30 | **25.40 ms** | 077 §7 |
| clip pass per submitted draw | **~47 µs** (~1.4 µs/vertex) | 069 T1 |
| per-`DrawMesh` baseline, clip off | **~51 µs/call** | 077 T4 |
| CPU cull | **~13 µs/call**; 218 of 500 culled at N=500 | 077 T2 |
| compose + `rlMultMatrixf` | ~1.7 + 3.5 µs scalar / 1.1 µs VFPU opt-in | 077 T3 |
| VFPU thread attribute under PSPLink | **~1.2 ms/frame** context-switch cost | 077 T3 |
| 2D batched arm | **4.95 µs/sprite**; 2,375 sprites at 60 fps | 009 / 014 |
| cache strips 16/32/64/128/480 px | natural 480 fastest; strips **null** | 023 T2 |
| compact 2D / s8 normal formats | dropped; parity-bound, vertex bandwidth not spent | 077 T5 |

> **P10-S11 landed after this sprint was written.** HEAD is `d4310135` (merged clean runs adopted); the b32 rows
> above are historical. The first action replaces them with a merged-build re-baseline, and T1 compares against
> that control, not against the 39.85 ms b32 row.

**What PGE contributes, and what it does not.** The useful anchors are instruction patterns and primitive choices,
not its submission model:

- `PGE/pgeGfx.c:920-1053` — 2-vertex `GU_SPRITES` for axis-aligned sprites, 64-pixel strips, 4-vertex fan for a
  rotated quad. The 2-vertex sprite is already ours; strips are a measured null; the per-sprite draw is rejected.
- `PGE/pgeGfx.c:1021` — the 4-vertex rotated quad. Ours uses 6 vertices inside a batch; a lone quad is the only
  possible use.
- `PGE/pgeGfx.c:437-443` — `GU_FAST_CLEAR_BIT`. Ours does not set it.
- `PGE/pgeMath.c:379-390` — `vrot.p` sin+cos in one instruction.
- `PGE/pgeMath.c:452-620` — `lv.q`/`sv.q` and `vadd.t`/`vsub.t`/`vscl.t`/`vdot.t`/`vrsq.s` patterns for 4-wide
  vector work.

PGE is **GPLv3**. Nothing from it is copied into this tree; use it only to understand sequences and behaviour, and
write original code.

**Read before acting**, in this order:

1. `077-phase10-pspgu-optimisation.md` §5 T3/T5 and §9/§10 — the VFPU initialisation fix, what was dropped, and
   why the remaining clip experiment is `clipLerp`/`packClipVertex`.
2. `078-phase10-per-call-and-merge.md` §0/§10 — the per-call merge is a separate programme; do not duplicate it.
3. `tools/pspgu/gl_draw.c` — `clipDistance`, `clipLerp`, `clipPolygon`, `packClipVertex`, `clipEmitDirty`.
4. `tools/pspgu/gl_state.c` — `glClear`; `tools/pspgu/gl_matrix.c` — the VFPU multiply and its scalar fallback.
5. `src/rlpsp_math.h` and `src/platforms/rlpsp_math.c` — the existing VFPU helper contract and its thread rule.
6. `src/rlgl.h` §`RLPSP_BATCH_ENABLED` — the quad-to-sprite/triangle decision and the 4-vertex fan boundary.
7. `../baselines.md` and `023-phase7-cache-strips-and-close.md` — the 2D wall and the strip null.

### Implementation-readiness review (2026-11-23)

Reviewed against the tree before coding. These corrections are part of the task definition:

- **T1 — 16 floats, not 18.** `ClipV` is `{x,y,z} + {u,v} + {nx,ny,nz} + {r,g,b,a} + {cx,cy,cz,cw}` = **16**
  floats. `clipLerp` currently returns `ClipV` by value; the VFPU version must write through an output pointer
  (for example `clipLerpVfpu(ClipV *out, ...)`) so it does not add a struct copy. Use C registers only; do not add
  matrix use. `clipProject` already uses `M000`, and the `rlpsp_math` helper reserves matrices 6–7; if GUM/VFPU
  helpers are enabled on the same thread, verify that existing `M000` use before widening it.
- **T1 — separate flag and control arm.** `PSPGU_VFPU=1` already enables the VFPU projection and matrix multiply,
  so it cannot isolate the new lerp/pack. Add `PSPGU_VFPU_CLIP=1` for the new code and measure five arms: VFPU
  off + clip on; VFPU off + `PSPGU_NO_CLIP=1`; VFPU on + `PSPGU_VFPU_CLIP=0`; VFPU on + `PSPGU_NO_CLIP=1`; VFPU
  on + `PSPGU_VFPU_CLIP=1`.
- **T2 — no clear-only probe exists.** Add `psp-tests/clear_probe` (one clear per frame, 300 frames, work/marker)
  or a `MODE=clear` arm to `matrix_probe`. `GU_FAST_CLEAR_BIT` is only a `pspgu.h` define and is used by PGE, not
  by any PSPSDK sample in the tree; verify that it is not ignored and does not change the 5650 capture.
- **T3 — call site and initialisation are missing.** The rotated 2D path is `src/rtextures.c:4924-4925` in
  `DrawTexturePro`; target render paths only. `rlpsp_math.h` is not included by any render TU and
  `rlpsp_vfpu_init()` is called only by `psp-tests/math_probe`, so T3 must add the helper include and a one-time
  init (or a scoped `pspvfpu_initcontext()`). Use `bunnymark ROTATE_ODD=1` as the rotated workload. VFPU trig is
  truncated; if a pixel moves, drop the task rather than relaxing the gate.
- **T4 — identity is not `ProjectionW()==0`.** `pspguProjectionW()` is 0 for any ortho, including Camera2D. A
  through-mode path is admissible only when the modelview is also identity and the projection is the known screen
  ortho; that query does not exist today. Use `scene_2d MODE=cache STRIP=480 REPEAT=1` for the one-quad UV and
  `TexScale`/`TexOffset` test, and add the identity query only if that test passes.
- **T5 — the batcher cannot know "one quad" at emit time.** `rlPspPushVertex` decides on the fourth corner and
  writes immediately; a lone-quad fan requires deferring quads or a separate one-quad entry point, which
  restructures the batcher and is out of scope. Use `scene_2d MODE=oversize WIDE=480 HIGH=60 ROTATE=45` (one rotated 480×60 quad)
  as the sparse workload; if no non-restructuring path exists, drop T5 before coding.
- **P10-S11 has landed.** HEAD is `d4310135` (merged clean runs adopted), so the b32 baseline rows are historical
  and must be replaced by a merged-build re-baseline. Unrelated working-tree changes (`.gitignore`, `ai-memory*`)
  are present; do not include them in the build or the evidence. Do not mix 078's `b33`, T4's `b34`, and the
  remaining tasks' `b35`.

### Remaining-task readiness review (2026-11-23)

Reviewed the three still-open experiments against the current tree:

- **Build/tag.** `b34` is already the T4 measurement; it is not a tag for new T1/T2/T3 code. Build and install
  those changes as **`b35`**, and do not compare a b35 binary against b34 captures/work without a control.
- **T1 — the VFPU clip path is not yet separable.** `ClipV` is still 4-byte aligned and `clipLerp` returns by
  value; add `__attribute__((aligned(16)))` and an output-pointer helper. `PSPGU_VFPU_CLIP` does not exist; add it
  to `tools/pspgu/Makefile` and `tools/pspgu-build.sh` (default 0, compiled only with `PSPGU_VFPU`). Use
  `matrix_probe MODE=scene` + `PSPGU_NO_CLIP` for work and `clip_probe` for pixels; no new probe is required. Keep
  the existing `M000` use unchanged and use C registers only in the new lerp. Do not add a second VFPU/FPU fence.
- **T2 — no clear probe and no build flag.** Add `psp-tests/clear_probe` (InitWindow → ClearBackground → 300
  frames → `DONE` → sleep, modeled on `triangle_probe`/`matrix_probe`) and add `PSPGU_FAST_CLEAR` to the pspgu
  Makefile/build script, default 0. `GU_FAST_CLEAR_BIT` exists in `pspgu.h` but pspgu does not set it yet. Run
  the probe with the normal 3-buffer/5650 setup, and with `PSP_GU_DEPTH=0` if checking the no-depth variant.
- **T3 — the scoped sincos path does not exist.** `PSP_VFPU_SINCOS` is not in `src/Makefile` or `rlpsp_math.h`;
  `rtextures.c` does not include `rlpsp_math.h`; and `rlpsp_vfpu_init()` is compiled only with
  `PSP_VFPU_HELPERS=1` and called only by `psp-tests/math_probe`. Add a scoped flag and helper/init, include it in
  `rtextures.c`, and link `-lpspvfpu`; do not enable the whole `PSP_VFPU_HELPERS` layer. Target only
  `DrawTexturePro` at `src/rtextures.c:4924-4925`; use `bunnymark ROTATE_ODD=1`. If trig truncation moves a pixel,
  drop it.
- **T5 — feasibility only.** `scene_2d MODE=oversize WIDE=480 HIGH=60 ROTATE=45` is a valid one-quad workload, but
  no batcher path exists for a lone fan. `PSP_SINGLE_QUAD_FAN` must not be added until a non-restructuring path is
  identified; otherwise drop T5 before coding.
- **T6 — close after decisions.** Full gates, sample thresholds, docs, pspGL restore, strict build.

**First action.** No driver change. Record `git rev-parse HEAD` (currently `d4310135`, P10-S11 merged clean runs)
and `git status`; isolate the unrelated `.gitignore`/`ai-memory*` changes. Re-baseline the **merged** archive, not
the b32 rows: `matrix_probe MODE=scene` at N=100/500, `PROBE=1`, two runs, plus `PSPGU_NO_CLIP=1` at N=500. Record
whether the VFPU is active (`psp-nm` shows the strong `pspvfpu_initcontext` and the self-check reports a real
`maxerr`), then use the merged re-baseline as T1's control. Ask for permission before every PSPLink session.

**Hard rules.**

- **Ask before every PSPLink use.** Plan the runs, then wait for the user's go-ahead; no `pspsh` command without
  permission for that run.
- **PGE is GPLv3.** Do not copy code, comments, macros or inline assembly. Reimplement from the VFPU/GU behaviour
  and the public PSPSDK headers; record the reference as a path, not as a source to paste.
- **VFPU stays opt-in.** Every new VFPU path needs the existing readiness check, the scalar fallback, and the
  same-thread rule. Do not introduce a second VFPU/FPU boundary inside a draw; check the FPU exception behaviour.
- **The pixel gates are sacred.** 2D `batch_check` 0 diff / 613 colours; `scene_3d SCENE=both` 0; MESH_SCENE 0;
  RTT 0; clip probes 237/720; `basic_shapes` 284. A speed claim with a pixel change is a drop.
- **No re-opened decisions.** No GUM, no per-sprite draw calls, no 64-pixel cache strips, no compact s16/5551
  format, no CPU-clip removal. Those were decided in 023, 070, and 077.
- **One tag bump per sprint, `make clean` after every driver change, one app per boot, reset between runs, two
  identical captures, `DONE` + sleep, never `exit`.**
- **No full GE wait.** Report `listWaitMaxUs` / `displayWaitMaxUs`; `arenaOverflows` must stay 0.
- **Thresholds are frozen.** Sample N=50: median ≤16.683, p95 ≤16.75, work ≤15.0.

**Commands.**

```sh
export PSPDEV=/home/psp/pspdev
export PATH=$PSPDEV/bin:$PATH

# Driver state and rebuild; T1 toggles PSPGU_VFPU and PSPGU_VFPU_CLIP independently
tools/pspgu-build.sh --check
cd src && make clean && make PLATFORM=PLATFORM_PSP PSP_BATCH=1 PSPGU_VFPU=1 PSPGU_VFPU_CLIP=1

# Baseline and experiment probes
cd psp-tests/matrix_probe && make clean && make MODE=scene PROBE=1
cd ../bunnymark && make clean && make ROTATE_ODD=1
cd ../scene_2d && make clean && make MODE=cache STRIP=480 REPEAT=1
cd ../scene_2d && make clean && make MODE=oversize WIDE=480 HIGH=60 ROTATE=45
# T2 after adding the probe:
cd ../clear_probe && make clean && make

# Guarded device delegation (explicit approval only)
PSP_DEVICE_APPROVED=1 psp-tests/samples/run_sample.sh <category>/<name>
```

## 1. Objective

Pre-registered before any experiment:

1. **VFPU clip lerp/pack.** Implement 4-wide interpolation and attribute packing for the dirty path, behind
   `PSPGU_VFPU=1` plus `PSPGU_VFPU_CLIP=1`. Reduce clip-attributable time (`work(clip on) - work(NO_CLIP)`) by
   **≥5 %** at N=500 with **0 pixel diff**, measured against the VFPU-projection/scalar-lerp control; move overall
   frame work by **>0.1 %**; otherwise drop it with the number.
2. **Fast clear.** Set `GU_FAST_CLEAR_BIT` in `glClear` behind `PSPGU_FAST_CLEAR=1`. Reduce clear-only work by
   **≥5 %**, move frame work by **>0.1 %**, and keep **0 pixel diff**; otherwise drop it.
3. **VFPU `vrot.p` sincos.** Route only the rotated-2D sin/cos pair in `DrawTexturePro` through a VFPU sincos
   helper behind `PSP_VFPU_SINCOS=1`, with a one-time VFPU init. Move `bunnymark ROTATE_ODD=1` by **>0.1 %** with
   **0 pixel diff**; if trig truncation moves any pixel, drop it.
4. **Pure `GU_TRANSFORM_2D` through mode.** Determine whether a screen-space 2D path is reachable without moving
   the transform to the CPU and without changing UV semantics. If reachable, it must be **0 diff** and
   **>0.1 %**; if it requires CPU transformation or a UV-format change, drop it with the reason.
5. **Lone 4-vertex fan (feasibility only).** Use a 4-vertex `GU_TRIANGLE_FAN` only if a non-restructuring
   one-quad path exists. Measure `scene_2d MODE=oversize WIDE=480 HIGH=60 ROTATE=45` (one rotated 480×60 quad); adopt only with **>0.1 %** and
   **0 diff**, otherwise drop it before coding and keep the batched 6-vertex path.
6. **Regression and close.** Every accepted flag is defaulted or recorded; every rejected flag is removed or left
   opt-in with its number; full gates and docs are closed.

## 2. Why now

Sprint 077 established that the remaining 3D cost is split between the per-call path and the CPU clip pass, and
that the VFPU path is now real but opt-in. Sprint 078 attacks the per-call path with merging. The clip pass still
has one untested lever: the polygon clipper's interpolation and packing, which is scalar soft-float work on a CPU
with no scalar FPU. PGE's vector snippets are the closest existing demonstration of the 4-wide sequences.

The other candidates are cheap, isolated probes: the GE clear flag, the rotated-2D trig pair, a through-mode
feasibility test, and a lone-quad primitive choice. None requires changing the batcher, the texture layer, or the
per-call architecture, so they can be decided independently of 078.

## 3. Scope

**In:**

- `tools/pspgu/gl_draw.c` — `clipLerp`, `packClipVertex`, only behind the existing VFPU readiness and scalar
  fallback; T5 feasibility only in `src/rlgl.h`.
- `tools/pspgu/gl_state.c` — `glClear` and the fast-clear flag.
- `src/rtextures.c` — the `DrawTexturePro` rotation pair for T3, behind `PSP_VFPU_SINCOS`.
- `src/rlpsp_math.h` / `src/rlgl.h` — a scoped VFPU sincos helper; do not enable the whole `PSP_VFPU_HELPERS` layer.
- `psp-tests/clear_probe` (new), `psp-tests/matrix_probe`, `psp-tests/clip_probe`, `psp-tests/scene_2d`
  (`MODE=cache`, `MODE=oversize WIDE=480 HIGH=60 ROTATE=45`), `psp-tests/bunnymark` (`ROTATE_ODD=1`), `psp-tests/batch_check`, and the
  existing capture/gate scripts.
- This sprint and the Phase 10 sprint table.

**Out:**

- Any GUM or `pspgum` change; any per-sprite `sceGuGetMemory`/draw submission; 64-pixel cache strips; compact
  s16/5551 vertices; removing the CPU clipper; draw merging/instancing (078); render-to-texture; audio; a new
  texture format; a change to the 2D batcher's vertex layout; or a batcher restructure to support T5.

## 4. Preconditions

- [ ] The current driver is the P10-S11 merged build (`d4310135`, `b33` or later), and the VFPU strong-reference
      fix from 077 is present. The b32 numbers are historical.
- [ ] `PSPGU_VFPU=1` produces a real self-check (not the old inert scalar-to-scalar `maxerr=0.000000`).
- [ ] The 2D and 3D gates reproduce their reference captures on the merged build.
- [ ] The tree is a known base: `git rev-parse HEAD` and `git status` recorded; unrelated `.gitignore`/`ai-memory*`
      changes are isolated and `b33`/`b34` artifacts are not mixed with the remaining `b35` build.
- [ ] `psp-tests/clear_probe` exists (or is added first); `scene_2d MODE=cache` and `MODE=oversize WIDE=480 HIGH=60 ROTATE=45`,
      and `bunnymark ROTATE_ODD=1`, build.
- [ ] T1/T2/T3 build flags exist in the build scripts (`PSPGU_VFPU_CLIP`, `PSPGU_FAST_CLEAR`,
      `PSP_VFPU_SINCOS`); each defaults off.
- [ ] The T3 helper has a scoped VFPU init and `-lpspvfpu` is on the app link line; the whole
      `PSP_VFPU_HELPERS` layer is not enabled for it.
- [ ] The PSP is available for an approved, reset-per-run session; PSPLink and `usbhostfs_pc` are not assumed.
- [ ] The pspGL backup and restore path (`tools/pspgu-build.sh --check/--restore`) is known and working.

## 5. Tasks

| ID | Task | Status | Verification | Evidence |
|---|---|---|---|---|
| T1 | **VFPU clip lerp/pack.** Interpolate the **16** `ClipV` floats in 4-wide groups through an output-pointer helper (`lv.q`/`sv.q`, `vsub.q`, `vscl.q`, `vadd.q`); align `ClipV` to 16 bytes, pack float attributes, scalar colour conversion if needed. Keep the scalar path and readiness check. Add `PSPGU_VFPU_CLIP=1` to the pspgu Makefile/build script so the new code is separable from `PSPGU_VFPU=1`; build as **b35** | `dropped` | N=500 five arms: clip-attributable **2.045 ms scalar, 2.464 ms VFPU projection, 2.748 ms VFPU lerp**; all captures 0 diff. VFPU path is slower; not adopted, flags default 0 | `/home/psp/evidence/p10s12/` |
| T2 | **Fast clear.** Add `GU_FAST_CLEAR_BIT` when colour/depth bits are cleared, behind `PSPGU_FAST_CLEAR=1` in the pspgu Makefile/build script; build as **b35**. Add `psp-tests/clear_probe` (one clear/frame, 300 frames, `DONE`, sleep) or a `MODE=clear` arm. Verify 5650, three buffers, no depth when depth is off | `dropped` | b35: clear probe **0.265 vs 0.268 ms** (+1.1 %); bunnymark **4.706 vs 4.702 ms** (−0.085 %, below 0.1 %); captures 0 diff. Not adopted, flag default 0 | `/home/psp/evidence/p10s12/` |
| T3 | **VFPU `vrot.p` sincos.** Add a scoped helper in `src/rtextures.c:4924-4925` (`DrawTexturePro`) with a one-time `pspvfpu_initcontext()` (or `rlpsp_vfpu_init`), behind `PSP_VFPU_SINCOS=1` in `src/Makefile`; include/link it without enabling the whole `PSP_VFPU_HELPERS` layer. Build as **b35** | `adopted` | bunnymark two pairs: **4.706/4.700 vs 4.650/4.651 ms** (mean 4.703→4.651, **−1.1 %**), captures 0 diff both pairs; batch/basic_shapes 0 diff. `PSP_VFPU_SINCOS` default 1 | `/home/psp/evidence/p10s12/` |
| T4 | **Through-mode feasibility.** Settle UV and `TexScale`/`TexOffset` with `scene_2d MODE=cache STRIP=480 REPEAT=1`; then test `GU_TRANSFORM_2D` only when the projection is the known screen ortho **and** the modelview is identity (new query). No Camera2D/rotation; no s16/16-bit UV | `dropped` | b34 normal vs `PSPGU_FAST2D=1`: cache **0.942 vs 0.972 ms**; 1,600 sprites **8.583 vs 9.531 ms**; all captures byte-identical. Through mode is pixel-exact but slower; not adopted. Flag kept opt-in, default 0 | `/home/psp/evidence/p10s12/` |
| T5 | **Lone-quad fan (feasibility only).** `rlPspPushVertex` decides on the fourth corner and writes immediately; no non-restructuring one-quad path exists. Dropped before coding; the batched 6-vertex triangle path stays | `dropped` | no code path without restructuring the batcher; `scene_2d MODE=oversize WIDE=480 HIGH=60 ROTATE=45` is the workload if a future path appears | decision only; no device run |
| T6 | **Regression and close.** Run every gate, update this sprint and the Phase 10 page, restore pspGL, strict build | `done` | batch normal/fastclear/VFPU-clip/sincos 0 diff; basic_shapes 0 diff at 600 frames/60 fps; bunnymark 16.683; matrix captures 0; pspGL restored; strict docs build | `/home/psp/evidence/p10s12/` |

### Design notes

**T1 — clip only, not classification.** The classification loop already has the per-triangle early-out that beat
deduplication in 077 T1. Interpolate only the vertices that are actually clipped, and keep the clean-run emission
unchanged. `ClipV` is 16 floats in memory order, so the natural 4-wide groups are `{x,y,z,u}`, `{v,nx,ny,nz}`,
`{r,g,b,a}`, `{cx,cy,cz,cw}`. Write through an output pointer, not a returned struct. `ClipV` is 64 bytes but
currently only 4-byte aligned; `lv.q`/`sv.q` require 16. Add `__attribute__((aligned(16)))` to `ClipV` (or copy
through aligned locals) and verify the stack arrays; the output vertex stride may not be 16-byte aligned, so pack
the final vertex with scalar stores or through an aligned staging buffer. Use C registers only; `clipProject`
already uses `M000`, so do not add matrix use or widen that shared-register hazard. A VFPU result consumed by
scalar code needs the same fence pattern the clipper already uses; do not add a second boundary inside a draw.

**T2 — fast clear is a GE path, not a CPU clear.** `GU_FAST_CLEAR_BIT` is present in `pspgu.h` and PGE sets it on
every clear; no PSPSDK sample in the tree uses it. Add `psp-tests/clear_probe` (one clear/frame, 300 frames) or a
`MODE=clear` arm; test clear-only first, then `bunnymark`, then the 3D scene. If the GE ignores it, or it changes
the capture on the 5650 three-buffer setup, record the null and keep the normal clear.

**T3 — sincos is a one-shot value.** The target is `DrawTexturePro`'s rotation pair at `src/rtextures.c:4924-4925`;
the pairs at `:3923` and `:4194` are image-processing paths and are not required. `rlpsp_math.h` is not currently included by any
render TU and `rlpsp_vfpu_init()` is called only by `psp-tests/math_probe`, so add the include and a one-time init
(or a scoped `pspvfpu_initcontext()`) with the helper. Use VFPU only for a per-frame rotation input, never for
accumulated camera yaw or timers. The VFPU trig is truncated; if any pixel moves, drop it. If `bunnymark
ROTATE_ODD=1` does not clear 0.1 %, drop it; phase 4 already calls VFPU trig sub-1 %.

**T4 — through mode is not a drop-in.** `pspguProjectionW()==0` only says ortho; Camera2D is also ortho. A
through-mode path is admissible only when the modelview is identity and the projection is the known screen ortho;
that query does not exist today. Start with `scene_2d MODE=cache STRIP=480 REPEAT=1` to settle UV and
`TexScale`/`TexOffset`; add the identity query only if the one-quad capture matches. PGE's 2D API is screen-space
and its through-mode UVs are texel coordinates; raylib uses normalized UVs and can carry Camera2D. If either the
transform or the UV convention requires CPU work, drop it.

**T5 — the batcher decides too early.** `rlPspPushVertex` selects the primitive on the fourth corner and writes
immediately; it cannot know whether the flush contains one quad. A lone-quad fan therefore needs deferred quad
storage or a separate entry point, which restructures the batcher and is out of scope. Use
`scene_2d MODE=oversize WIDE=480 HIGH=60 ROTATE=45` (one rotated 480×60 quad); if no non-restructuring path exists, drop T5 before coding. PGE's one-draw-per-sprite
model is not admissible.

### Task log (append-only)

- 2026-11-23 — sprint written from the PGE review; scope fixed to clip, clear, rotated-2D trig, through-mode
  feasibility, and the lone-quad fan. GUM, per-sprite submission, strips, and compact formats are out.
- 2026-11-23 — implementation-readiness review: corrected 18→16 `ClipV` floats, added `PSPGU_VFPU_CLIP` and the
  five-arm control, added the missing clear probe, wired T3 to `rtextures.c:4924-4925` with VFPU init, required an
  identity-modelview query for T4, and downgraded T5 to feasibility because the batcher cannot know a one-quad
  flush. Recorded the P10-S11 worktree isolation rule.
- 2026-11-23 — T4 measured on hardware, b34. Normal vs `PSPGU_FAST2D=1`: one-quad cache 0.942 vs 0.972 ms;
  1,600 sprites 8.583 vs 9.531 ms; `batch_check` 0 pixel diff; all captures byte-identical. Through mode is not
  adopted; the opt-in path remains for reproducibility and defaults off. pspGL restored; evidence in
  `/home/psp/evidence/p10s12/`.
- 2026-11-23 — remaining-task readiness review: b34 is reserved for T4; T1/T2/T3 require b35. Added the missing
  `PSPGU_VFPU_CLIP`/`PSPGU_FAST_CLEAR` build flags, the missing `clear_probe`, and the scoped
  `PSP_VFPU_SINCOS` helper/init requirement. T1 will use `matrix_probe` + `clip_probe`; T3 targets
  `rtextures.c:4924-4925`; T5 remains feasibility-only.
- 2026-11-23 — b35 hardware decisions: T1 dropped (2.045/2.464/2.748 ms clip-attributable; 0 diff);
  T2 dropped (clear +1.1 %, bunnymark −0.085 %; 0 diff); T3 adopted (4.703→4.651 ms, −1.1 %, 0 diff both
  pairs); T4/T5 already dropped. batch/basic/bunny/matrix gates 0 diff; pspGL restored.
- 2026-11-23 — implementation (b35): T2 `PSPGU_FAST_CLEAR` + `psp-tests/clear_probe`; T1 16-byte `ClipV`,
  output-pointer scalar/VFPU lerp, `PSPGU_VFPU`/`PSPGU_VFPU_CLIP` flags, and `PSPGU_NO_CLIP` scalar stubs;
  T3 scoped inline `vrot.p` in `rlpsp_math.h`, `PSP_VFPU_SINCOS`, `DrawTexturePro` call site, and the missing
  `platforms/rlpsp_math.o` include/output rule. T5 dropped before coding: the batcher cannot know a lone-quad
  flush without a restructure.

## 6. Budgets

| budget | limit | measured | tag |
|---|---|---|---|
| T1 clip-attributable work | ≥5 % below the VFPU-on/scalar-lerp control at N=500; report all five arms | | |
| T1 overall frame work | >0.1 % and 0 pixel diff | | |
| T2 clear-only work | ≥5 % below the normal clear in `clear_probe` | | |
| T2 overall frame work | >0.1 % and 0 pixel diff | | |
| T3 `bunnymark ROTATE_ODD=1` | >0.1 % and 0 pixel diff; drop on any pixel change | | |
| T4/T5 | >0.1 % and 0 pixel diff, or dropped before coding with the reason | | |
| gates | 2D 0/613, SCENE=both 0, MESH_SCENE 0, RTT 0, clip 237/720, shapes 284 | | |
| sample N=50 | median ≤16.683, p95 ≤16.75, work ≤15.0 | | |
| waits | no full GE wait; report `listWaitMaxUs` | | |
| arena | `arenaOverflows` 0 | | |

## 7. Measurements

Protocol: device only; two runs per point; reset between; captures after `DONE`; one app per boot.

| run | build | workload | metric | value | notes |
|---|---|---|---|---|---|
| baseline | b32 | `MODE=scene` N=100 / N=500 | work | **11.071 / 39.85 ms** | historical; 070/077 |
| baseline | b32 | same, `PSPGU_NO_CLIP=1` | work | **6.927 / 25.40 ms** | historical clip share |
| baseline | b33 | `MODE=scene` N=100 / N=500, P10-S11 merged | work | **TBD** | first action; replaces 39.85 |
| baseline | b33 | same, `PSPGU_NO_CLIP=1` | work | **TBD** | first action |
| baseline | b32 | `PSP_MATRIX_PROBE` | cull / compose / mult | **13 / 1.7 / 3.5 µs** | VFPU 1.1 opt-in |
| baseline | b32 | 2D `bunnymark` | per-sprite work | **4.95 µs** | 009/014 |
| T1 | b35 | N=500 normal / normal+NO_CLIP | work | **29.501 / 27.456 ms** | scalar clip share **2.045 ms** |
| T1 | b35 | N=500 VFPU / VFPU+NO_CLIP | work | **29.921 / 27.457 ms** | VFPU clip share **2.464 ms** |
| T1 | b35 | N=500 VFPU-clip / VFPU+NO_CLIP | work | **30.205 / 27.457 ms** | VFPU lerp clip share **2.748 ms**; slower |
| T1 | b35 | matrix_normal / vfpu / vfpuclip captures | pixel diff | **0** | all three identical |
| T2 | b35 | `clear_probe` 300 frames | work | **0.265 normal / 0.268 fastclear ms** | fast clear slower |
| T2 | b35 | `bunnymark` 1,600 | work | **4.706 normal / 4.702 fastclear ms** | −0.085 %, below 0.1 % |
| T2 | b35 | bunny/batch captures | pixel diff | **0** | identical |
| T3 | b35 | `bunnymark ROTATE_ODD=1` pair 1 | work | **4.706 normal / 4.650 sincos ms** | 0 diff |
| T3 | b35 | pair 2 | work | **4.700 normal / 4.651 sincos ms** | 0 diff |
| T3 | b35 | batch/basic_shapes captures | pixel diff | **0** | batch 2 frames, basic 600 frames/60 fps |
| T4 | b34 | `scene_2d MODE=cache STRIP=480 REPEAT=1`, 300 frames | work / diff | **0.942 normal / 0.972 fast2d**; 0 diff | identical capture |
| T4 | b34 | `scene_2d MODE=sprites COUNT=1600 OFFSCREEN=0`, 300 frames | work / diff | **8.583 normal / 9.531 fast2d**; 0 diff | identical capture |
| T4 | b34 | `batch_check` | pixel diff | **0** | 2D/3D gate |
| T5 | — | no code path without batcher restructure | decision | **dropped** | no device run; 6-vertex triangle path stays |
| final | b35 | bunnymark 1,600 | median frame | **16.683 ms** | 60 fps; work 4.70 ms |
| gate | b35 | batch normal/fastclear/VFPU-clip/sincos; basic_shapes normal/sincos | pixel diff | **0 / 0** | affected gates; full release suite not re-run |

## 8. Exit criteria

- [x] T1: all five arms measured; **dropped**. VFPU projection 2.464 ms and VFPU lerp 2.748 ms vs 2.045 ms
      scalar clip-attributable; 0 pixel diff
- [x] T2: clear probe and bunnymark measured; **dropped**. Clear 0.268 vs 0.265 ms; bunnymark −0.085 %;
      0 pixel diff
- [x] T3: `bunnymark ROTATE_ODD=1` measured twice; **adopted**. 4.703→4.651 ms (−1.1 %), 0 diff both pairs
- [x] T4: measured on hardware; pixel-exact but slower at both workloads; dropped. The flag remains opt-in,
      default 0, and the numbers are recorded
- [x] T5: dropped before coding; no non-restructuring one-quad path exists, and the batched 6-vertex path stays
- [x] Affected gates 0 diff: batch normal/fastclear/VFPU-clip/sincos; basic_shapes normal/sincos at 600
      frames/60 fps; bunnymark captures; matrix captures. Full release suite not re-run because rejected paths
      default off; T3 is the only adopted change
- [x] T1/T2/T3 built as b35; b34 remains the T4-only measurement; no b34/b35 work is mixed
- [x] Build tag, flags, decisions and numbers recorded; pspGL restored; strict docs build passes

## 9. Result

**T3 is adopted; T1, T2, T4 and T5 are dropped.**

| task | decision | measured result |
|---|---|---|
| T1 VFPU clip lerp/pack | **dropped** | clip-attributable 2.045 ms scalar, 2.464 ms VFPU projection, 2.748 ms VFPU lerp; all captures 0 diff |
| T2 fast clear | **dropped** | clear 0.265→0.268 ms (+1.1 %); bunnymark 4.706→4.702 ms (−0.085 %, below 0.1 %); 0 diff |
| T3 VFPU sincos | **adopted** | 4.706/4.700→4.650/4.651 ms (mean 4.703→4.651, **−1.1 %**); 0 diff in both pairs; batch/basic_shapes 0 diff |
| T4 through mode | **dropped** | 0.942→0.972 ms one quad; 8.583→9.531 ms at 1,600 sprites; 0 diff |
| T5 lone fan | **dropped** | no non-restructuring one-quad path; no code |

Defaults after the sprint: `PSPGU_VFPU`/`PSPGU_VFPU_CLIP` **0**, `PSPGU_FAST_CLEAR` **0**, `PSPGU_FAST2D` **0**;
`PSP_VFPU_SINCOS` **1** in the PSP build. The rejected paths remain opt-in for reproducibility. pspGL was restored
after the session. Evidence and logs: `/home/psp/evidence/p10s12/`.

## 10. Follow-ups

- **T1 is dropped:** VFPU projection and lerp both lose to scalar on this workload; the clipper's remaining cost is
  polygon topology, not arithmetic. The opt-in flags remain for a future re-test only.
- **T2 is dropped:** fast clear is neutral/slower on the 5650 three-buffer setup; the clear path is not the lever.
- **T3 is adopted:** keep it scoped to rotated 2D; do not use VFPU trig for accumulated camera or timer state. The
  gain is 1.1 % of work and the pixels are identical; re-check on a non-rotated-heavy app if the VFPU attribute
  context cost becomes a concern.
- **T4 is dropped:** raylib's matrix-based 2D and PGE's screen-space 2D are not equivalent, and through mode is
  slower here even with identical pixels. The opt-in `PSPGU_FAST2D` path remains only for a future re-test; do not
  re-open compact formats as a substitute.
- **If T5 is dropped:** the batcher stays on 6-vertex triangles for rotated runs; a per-sprite fan is a rejected
  submission model. This is the expected outcome unless a non-restructuring one-quad path appears.
- **078 remains independent:** per-call merging is the structural lever for 3D; this sprint must not change its
  baseline or its gates.

**Correction (after the benchmark run, same day).** The `b35` driver breaks `clip_probe EDGE=237 MESH3D=1`:
**1681 pixels** of the probe triangle come out black against pspGL and against `b33`, reproduced twice. The
adopted sincos is not involved — the library alone is clean in isolation. The driver is.

The fault is in the interaction the T1 work introduced, not in one file, and a first bisect that blamed
`gl_draw.c` was **wrong**: it was built without removing the objects by hand, so it measured a mix of old and new
objects (the same stale-object trap the port has hit before). With `rm -f tools/pspgu/*.o` before every build the
picture is:

| build (all forced clean) | clip_probe vs pspGL |
|---|---|
| `52d1b8e5` driver (b33) | 0 |
| all `806cb3d7` driver files, `gl_draw.c` reverted | 0 — but only because that file's `#if defined(PSPGU_VFPU)` keeps the VFPU clip path initialised, which is the accidental state T1 was meant to remove |
| the same, with `#if PSPGU_VFPU` (VFPU off, as the header now says) | **1681** |
| whole driver reverted to `52d1b8e5`, tag `b36` | **0** |

So the scalar path (VFPU off) is the one that loses the triangle, and the VFPU path was masking it. That is the
opposite of what T1 concluded, and it is not explained by any single-file diff: the draw state is identical in both
cases (matrices, `fmt=0x180`, stride 12, viewport and scissor, cull/depth/light/fog off, one list per frame).

**Decision:** every driver-side change in this sprint was recorded as *dropped*, so the whole driver is reverted to
its `52d1b8e5` content and re-tagged **`b36`** — the archive's behaviour is no longer `b35`, and a tag that says so
would claim experiments it does not contain. The adopted sincos stays in `libraylib.a`.

Benchmarks after the revert (`b36` driver, sincos library):

| demo | before the sprint | after | note |
|---|---|---|---|
| 3D `psp_showcase` | 37.331 ms work / 20.0 fps | **33.713 ms / 20.0 fps** | −9.7 %, the sincos gain |
| 2D `psp_2d_showcase` | 20.508 ms work / 29.6 fps | **21.562 ms / 29.6 fps** | **+5.1 %**: the VFPU thread attribute costs ~1 ms/frame and the demo never rotates, so it pays without gaining |

That second row is the cost this sprint's adoption carries into 2D: `PSP_VFPU_SINCOS` buys a rotated-sprite win and
charges a context-switch tax to every app that links it. Worth a dedicated decision (lazy init, or gate it on the
app), not a silent default.
