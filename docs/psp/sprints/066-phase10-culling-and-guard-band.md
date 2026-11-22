# Sprint 066 (P10-S5) — Culling, guard-band clipping and the ME option

| | |
|---|---|
| Roadmap phase | [Phase 10 — 3D and advanced](../phases/10-3d-advanced/index.md) |
| Sprint | P10-S5 |
| Branch / base commit | `master` @ `6df56cb8` (sprint 065 closed) |
| Started / closed | 2026-11-22 (written) / — |
| Build number(s) | pspgu `b29` (060–065 shipped b25–b28); `libraylib.a` default `PSP_BATCH=1` |
| Tasks | 0/6 done |

## 0. Handover

**Situation.** The GE has no X/Y clipping: a primitive that crosses the 4096 window is dropped whole, while only
Z is clipped `[public: PPSSPP GE docs]`; the memory map records it as a fact, not a suspicion. pspgu does not enable
`GU_CLIP_PLANES` anywhere, and nothing in the engine culls objects. This sprint measures the exact failure boundary,
adds a CPU triangle clip + re-tessellation for offending triangles (daedalus has the working pattern), adds
conservative object culling in `DrawMesh`, and measures the Media Engine option only if Phase 9 has delivered a
maintainable ME runtime.

**Read before acting**, in this order:

1. `../phases/10-3d-advanced/index.md` §What we already know (the 4096 rule), §Guidelines (guard-band-safe
   tessellation, pass order), and the S5 row.
2. 060/061 §9 — the converting and direct draw paths, the packed layout (`PspGeMesh`: centre/scale but **no bbox
   yet**) and the camera-relative null. 062/065 are closed; lighting and RTT are not in this sprint.
3. `tools/pspgu/egl.c` — GE init, which never enables `GU_CLIP_PLANES`; `gl_state.c` — the only culling state is
   `GU_CULL_FACE` (backface); `gl_draw.c` — `glDrawArrays`/`glDrawElements`, the converting/direct branches and
   `isDirectLayout`. A clip pass must run before either submission path.
4. daedalus's working reference: `Source/HLEGraphics/BaseRenderer.cpp:442-463` (cull when all three vertices share
   a clip flag), `:580-590` (the six NDC planes), `:595`/`:684` (`clip_tri_to_frustum`), `:697-707`
   (`set_clip_flags`), `:736-800` (`PrepareTrisClipped`: clip, interpolate, re-tessellate as a fan) and
   `SysPSP/HLEGraphics/RendererPSP.cpp:223` (`sceGuEnable(GU_CLIP_PLANES)`).
5. `src/rmodels.c` — `DrawMesh` is the only object-level choke point; the `PspGeMesh` table (061) holds
   centre/scale for packed meshes; add a bbox (and a bounds-only record for the float path). `src/rlgl.h` has no
   frustum culling. `glGetFloatv(GL_PROJECTION_MATRIX|GL_MODELVIEW_MATRIX)` works in pspgu (`gl_query.c`) and
   pspGL, so culling can read the live matrices without trusting `RLGL.State` (not maintained on GL11).
6. `../memory-map.md` §10.2 — ME rules (no VFPU, no coherence, watchdog, publication counters); §6 — the
   measurement protocol.
7. Phase 9's ME sprints: `050-phase9-sas-me-prototype.md` and `051-phase9-adopt-route.md`. If the ME route is not
   delivered, T5 is `blocked`, not skipped.

**First action.** Reproduce the drop on device with one triangle: project a quad so one vertex lands at screen
x = 4000, 4095, 4096, 4097 and 5000, and capture each. Then do the same across the near plane with
`GU_CLIP_PLANES` off and on. Record the exact boundary before writing any clipper.

**Hard rules.**

- **Clip to the visible screen, not the 4096 window.** The six NDC planes (`|x| ≤ w`, `|y| ≤ w`, `|z| ≤ w`) are the
  screen rectangle; anything outside is invisible even inside the guard band. Clipping there is simpler than
  mapping 4096 and removes the drop condition.
- **Bump `PSPGU_BUILD` to `b29` in T2** (T3 changes raylib, not the driver).
- **Culling must be conservative.** A false cull is a visible bug; every test scene includes objects straddling
  each frustum plane, and the cull-on capture must be pixel-identical to cull-off for everything that should be
  visible.
- **Clip arithmetic is shared at edges.** Compute the edge parameter from the same plane distance used for the
  inside test; interpolate world position, UV, colour and normal by that parameter. Two triangles sharing an edge
  must produce the same intersection, or the seam is a crack.
- **Winding is preserved.** Re-tessellation emits a fan in the original order; backface culling stays on.
- **Interpolate world attributes, not clip attributes.** The GE applies perspective correction to 3D vertices;
  linear world-space interpolation along the clipped edge is correct, linear NDC interpolation is not.
- **The near plane is measured, not assumed.** If `GU_CLIP_PLANES` changes near-plane behaviour, enabling it is
  part of T1's result and must not regress the 060/062 gates.
- **No full GE wait.** The clip pass is CPU work between list commands; it must not add a sync.
- **No per-frame allocation outside the list arena.** Clipped vertices go into the same per-list arena as the
  converted path.
- **The ME is measured in shadow mode first.** No adoption without a hardware run, a heartbeat, a watchdog and a
  decision hash against the CPU; PPSSPP does not execute ME code.
- **Strictly bounded work.** A clip budget per frame (vertices/triangles) with a counter; exceeding it logs and
  falls back to the unclipped draw so the failure is visible, never silent.
- **The 2D and 060/062/065 gates are sacred.**
- **Stage only this sprint's paths.** `git status` before every commit, never `git add -A`. Commit messages carry the
  task ID: `sprint 066 T2: ...`.

**Commands.**

```sh
export PSPDEV=/home/psp/pspdev
export PATH=$PSPDEV/bin:$PATH
set -e

cd /home/psp/raylib-psp
tools/pspgu-build.sh && tools/pspgu-build.sh --check
tools/pspgl-build.sh --check                              # 2ab7378e

build_lib() { ( cd src && make clean && make PLATFORM=PLATFORM_PSP -j8 $1 \
                && cp libraylib.a $PSPDEV/psp/lib/libraylib.a ); }

run_app() {   # $1 = app dir, $2 = tag, $3 = make vars
    ( cd "$1" && make clean && make $3 && cp *.prx /tmp/s3/app.prx )
    ( cd /tmp/s3
      timeout 20 pspsh -e "reset"; sleep 6
      ( printf './app.prx\n'; sleep 40 ) | timeout 70 pspsh
      pspsh -e "scrshot host0:/$2.raw"; pspsh -e "scrshot host0:/$2-b.raw" )
}

# Boundary first: one vertex walks out of the window; near plane second.
run_app psp-tests/clip_probe p10s5-x4000 'EDGE=4000'
run_app psp-tests/clip_probe p10s5-x5000 'EDGE=5000'
run_app psp-tests/clip_probe p10s5-near 'NEAR=1 CLIPPLANES=0'
run_app psp-tests/clip_probe p10s5-near-on 'NEAR=1 CLIPPLANES=1'

# Then the big scene: cull off/on, clip off/on.
run_app psp-tests/scene_3d p10s5-scene 'OBJECTS=1000 CULL=0 CLIP=0'
run_app psp-tests/scene_3d p10s5-scene-cull 'OBJECTS=1000 CULL=1 CLIP=1'
```

**Do not touch.** Lighting/fog state beyond the gates (S3); RTT (S4/S6); the 2D batcher, window splitter and
texture upload; `src/platforms/rcore_psp.c` timing hooks; any Phase 8/9/11/13 file with uncommitted work; any ME
repository outside this tree is referenced, never vendored.

**Done means** §8 below, with evidence in each task row.

## 1. Objective

Falsifiable: a quad with a projected vertex at x = 5,000 renders its visible part with edge pixels within 1 px of
an analytically clipped reference (today it vanishes); a triangle crossing the near plane is clipped, not dropped,
with the `GU_CLIP_PLANES` state T1 proves is required; in a 1,000-object scene, cull-on is pixel-identical to
cull-off for every visible object and removes the measured off-screen draw count; work falls in proportion to the
removed draws (±10 %); and the ME shadow run, if Phase 9 delivered the runtime, reproduces 100 % of CPU culling
decisions.

## 2. Why now

Phase 10's exit criteria include guard-band-safe geometry and object culling. Both are preconditions for the S6
sample target, and the clip pass changes how the draw paths submit vertices — doing it after the sample is built
means re-measuring the sample. The ME is included only because the phase page names it; the CPU path is the
deliverable.

## 3. Scope

**In:** a CPU clip + re-tessellation pass in pspgu's converting path; `GU_CLIP_PLANES` verification; conservative
object culling in `DrawMesh`; load-time bounds reuse; a `clip_probe` app; a 1,000-object scene arm; counters; the
ME shadow experiment (conditional); the Phase 10 limits table.

**Out:** clipping or culling on the direct GE-native path beyond a conservative fallback; GPU-side occlusion
queries (the GE has none); hierarchical/zone culling; LOD; ME adoption without a delivered Phase 9 runtime;
changing the near/far planes.

## 4. Preconditions

- [x] 060–065 closed; cube, lit, fog and RTT gates green on `b28`
- [ ] the 061 `PspGeMesh` gains a bbox (it has centre/scale only); float meshes get a bounds-only record
- [ ] pspgu `b28`/`b29` content hashes recorded; pspGL `2ab7378e` available
- [ ] Device reset between runs; app logs `DONE` and sleeps; two identical captures per point
- [ ] `clip_probe` and `scene_3d` arms compile and are proven present
- [ ] ME (T5 only): Phase 9's runtime with exception handler, watchdog and SPSC queues exists; otherwise record
      `blocked by 051 T4`

## 5. Tasks

| ID | task | status | verification | evidence |
|---|---|---|---|---|
| T1 | Reproduce the drop: `clip_probe` with one vertex projected to screen x = 3000, 4000, 4096, 5000; near-plane arm with `GU_CLIP_PLANES` off/on; pspGL A/B | `todo` | exact x where the visible part disappears; near clip vs drop; table | — |
| T2 | CPU clip + re-tessellation: transform to clip space, six NDC planes, Sutherland–Hodgman, fan re-tessellation, interpolate world attributes, preserve winding; triangles only; converting **and** direct path (direct falls back to pack+clip for a dirty draw); enable `GU_CLIP_PLANES` if T1 needs it; `b29` | `todo` | x=5000 quad matches analytic clip ≤1 px; inside geometry 0 diff; no seams; clipped count | — |
| T3 | Object culling in `DrawMesh`: store local bbox per mesh (extend `PspGeMesh`; bounds-only record for float meshes), read P/M via `glGetFloatv`, build frustum planes, transform the 8 AABB corners, skip fully-outside; no-cull arm | `todo` | cull-on vs cull-off 0 pixel difference; straddling objects stay; culled count | — |
| T4 | Big-scene measurement: `scene_3d OBJECTS=1000` with in/out/behind objects, `CULL=0|1`, `CLIP=0|1` in one session | `todo` | work/STAT table; culled draws/clipped triangles; work falls proportionally (±10 %) | — |
| T5 | ME option (conditional): shadow-mode culling over an uncached SPSC queue, decision hash vs CPU, watchdog; adopt only if it wins | `todo` | 100 % decision match; measured time; else `blocked by 051 T4` | — |
| T6 | Regression and write-back: 2D/060/062/065 gates, `basic_shapes` 284, VRAM 0 delta, clip/cull limits | `todo` | gates 0 diff; 284 colours; limits recorded | — |

### Design notes (implementation-ready)

**Guard band vs screen.** The GE drops a primitive when a vertex leaves the 4096 window, but the visible screen is
only 480×272. Clipping to NDC ±w (the screen rectangle) removes the drop case and is what the rasteriser would have
shown anyway. Do not clip to 4096.

**Clip pass.** Compute `MVP·vertex` once per vertex (float32), set `X_POS/X_NEG/Y_POS/Y_NEG/Z_POS/Z_NEG` flags, and
skip untouched triangles. For an offending triangle, clip the polygon against the six planes in clip space
(Sutherland–Hodgman), keeping the edge parameter from each plane; emit a triangle fan with world position, UV,
colour and normal interpolated by that parameter. Winding is preserved by emitting in original order. The pass runs
in `glDrawElements` before the direct/convert decision: a clean draw keeps the direct pointer; a dirty draw falls back
to the converting path and is clipped. Lines/points crossing the screen are a recorded limit.

**Direct-path fallback.** `isDirectLayout` can still hand the buffer through when no triangle is clipped. If a
triangle needs clipping, pack the draw's vertices (061 layout) and clip them; this draw pays the conversion, the
others do not. Count both cases.

**Culling.** `glGetFloatv` returns the composed projection and modelview from both drivers. Extract six frustum
planes (Gribb–Hartmann), transform the mesh's local AABB's 8 corners by the current model matrix, and skip when all
corners are outside one plane. A false cull is a failure; test objects straddling every plane. Bounds are stored once
per mesh keyed by `vboId`; the packed record gains `min/max`.

**Cost.** Clip only offending triangles; cull cost is one matrix-vector test per corner per draw. Both report
counters. The big scene measures work, not just the frame median.

**ME.** Phase 9's ME runtime is not a dependency of the CPU path. If `051 T4` has not landed, T5 is `blocked`, and the
sprint result says so; do not fake a shadow run.

Rules:

- **IDs are stable.** Never renumber; a task that no longer applies becomes `dropped` with the reason.
- **`done` requires evidence** in §7. Code written is `doing`.
- **`blocked` names its dependency** — `blocked by 051 T4`, not "blocked".

### Task log (append-only)

- 2026-11-22 — sprint written, all tasks `todo`

## 6. Budgets

| budget | limit | measured | tag |
|---|---|---|---|
| clip cost | only offending triangles; record µs per 1,000 triangles | | |
| cull cost | record µs per 1,000 objects | | |
| frame work | 1,000 objects: cull-off baseline vs cull-on; proportional drop ±10 % | | |
| vertex arena | clipped vertices fit the existing 128 KiB list arena | | |
| VRAM | 0 delta | | |
| ME | adoption only if main-CPU time drops with watchdog margin | | |

## 7. Measurements

Protocol: device only; one variable per arm; two identical captures.

| run | build | workload | metric | value | notes |
|---|---|---|---|---|---|
| control | pre-sprint | `EDGE=4000` … `5000` | drawn / dropped | | exact boundary |
| control | pre-sprint | `NEAR=1 CLIPPLANES=0/1` | clipped / dropped | | |
| change | T2 | `EDGE=5000` | edge error vs analytic | | target ≤1 px |
| change | T2 | inside-only scene | differing pixels | | target 0 |
| control | T3 | 1,000 objects, `CULL=0` | work / pixels | | |
| change | T3 | 1,000 objects, `CULL=1` | work / pixels / culled | | 0 pixel diff |
| change | T4 | objects across all planes | culled draws | | |
| change | T5 | shadow ME vs CPU | decision matches / time | | 100 % |

## 8. Exit criteria

- [ ] §1 met, with the tables in §7
- [ ] the guard-band boundary is measured and recorded, not inferred
- [ ] clipping preserves winding, attributes and shared edges; fully-inside geometry is byte-identical
- [ ] culling is conservative; straddling tests pass; cull-on vs cull-off is 0 pixels
- [ ] the clip/cull budget counters exist and are reported
- [ ] ME outcome recorded: adopted with numbers, or `blocked` with the Phase 9 dependency
- [ ] 2D gate 0 diff; 060 cube gate 0; 062/065 gates unchanged; `basic_shapes` 284 colours
- [ ] limits recorded: triangles only; direct draws fall back to pack+clip when dirty; no occlusion queries; no LOD

## 9. Result

—

## 10. Follow-ups

- Clipping on the direct GE-native path without conversion, if T4 shows the fallback costs too much → a later
  optimisation, measured first
- LOD and hierarchical culling for the S6 sample → **P10-S6** if the target needs them
- ME adoption for culling, if T5 wins → Phase 9's runtime roadmap, not a Phase 10 dependency
- Guard-band-safe tessellation at export time for large static worlds → sample-porting phase
