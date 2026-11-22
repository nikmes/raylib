# Sprint 066 (P10-S5) — Culling, guard-band clipping and the ME option

| | |
|---|---|
| Roadmap phase | [Phase 10 — 3D and advanced](../phases/10-3d-advanced/index.md) |
| Sprint | P10-S5 |
| Branch / base commit | `master` @ `6df56cb8` (sprint 065 closed) |
| Started / closed | 2026-11-22 (written) / 2026-11-22 |
| Build number(s) | pspgu `b29` (060–065 shipped b25–b28); `libraylib.a` default `PSP_BATCH=1` |
| Tasks | 5/6 done; T5 `dropped` (Phase 9 ME route dropped) |

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
| T1 | Reproduce the drop and the near plane | `done` | boundary x; near clip vs drop | draws at screen x 2280, dropped at 2290; near triangle 0 px unclipped |
| T2 | CPU clip + re-tessellation, converting and direct fallback; `b29` | `done` | clip vs no-clip; inside 0 diff | far triangle **236 px vs 0**; near **720 vs 0**; inside geometry 0 diff |
| T3 | Object culling with load-time bounds and live matrices | `done` | cull-on vs cull-off pixels; culled count | **0 pixel diff**; 5,501 culls over 6 frames (~917/frame) |
| T4 | 1,000-object measurement | `done` | work reduction | 233.6 ms → 50–51 ms (4.5×); proportional target **not met** (see §9) |
| T5 | ME shadow culling | `dropped` | — | Phase 9 sprint 051 (ME takeover) is `dropped`; no runtime exists |
| T6 | Gates and write-back | `done` | 2D/060/062/065, shapes | 0 / 0 / 0 / 0; 284 colours |

### Design notes (as shipped)

**Clip to NDC, not 4096.** A vertex leaves the guard band at GU coordinate 4096; for a 480-wide ortho that is
screen x = 2048 + (4096-2048) = ... measured: the triangle draws at x 2280 and is dropped at 2290, matching NDC
|x| = 8.53. The pass clips against the six NDC planes (the visible screen), which removes the drop case and handles
the near plane in the same code.

**Clip pipeline.** Every triangle draw is scanned once: vertices are transformed by `pspguGetMVP()` (projection ×
modelview, including the packed mesh's scale/centre); a clean draw keeps the existing direct/converting path. A
dirty draw is clipped polygon-by-polygon against the six planes, interpolating world position, UV, colour and
normal by the plane parameter, and re-fanned. Output is float-format (32-bit vertex) regardless of the source type,
so no s16 requantisation is needed. Only triangles are clipped; lines/points are a recorded limit. The pass also
covers z, so a vertex behind the camera no longer discards the triangle (720 px vs 0 unclipped).

**Bounds and culling.** `UploadMesh` stores an AABB keyed by `vboId` for every static mesh (packed or not);
`DrawMesh` reads the live projection/modelview with `glGetFloatv`, composes `P·V·M`, transforms the 8 corners and
skips the draw when all are outside one plane. `rlpspEnableCulling(0)` gives the no-cull arm.

**Cost.** The scan is O(vertices) per triangle draw, and culling is ~50 µs per `DrawMesh` call in this scene; both are
visible in the work table, not hidden.

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
| boundary | b29 | 2D triangle, far vertex | pixels | 2280 → 20,948; 2290 → 0 | drop at NDC 8.53 |
| near | b29 | 3D triangle, vertex behind camera | pixels | clip **720**, no-clip **0** | z clipping included |
| far | b29 | 3D triangle, vertex at x=40 | pixels | clip **236**, no-clip **0** | direct/converting path |
| cull | b29 | 1,000 cubes, 6 frames | culled / work | **917/frame**; 233.6 → 50–51 ms | pixel diff **0** vs no-cull |
| gate | b29 | 2D / 060 / 062 / 065 | diff | 0 / 0 / 0 / 0 | |
| gate | b29 | `basic_shapes` | colours | **284** | |

## 8. Exit criteria

- [x] §1 met for the guard band and near plane, with clip/no-clip evidence
- [x] the boundary is measured (2280 draws / 2290 drops), not inferred
- [x] clipping preserves winding and attributes; fully-inside geometry 0 diff
- [x] culling is conservative; cull-on vs cull-off 0 pixels; straddling objects tested
- [x] clip/cull counters exist (`clip tris`, `CULL total`)
- [x] ME outcome recorded: **dropped**, Phase 9 sprint 051 dropped the takeover
- [x] 2D 0; 060/062/065 gates 0; `basic_shapes` 284
- [ ] work reduction proportional to culled draws (±10 %) — **not met**: 4.5× vs 92 % culled; recorded in §9
- [x] limits recorded: triangles only; direct draws fall back to pack+clip; no occlusion queries; no LOD

## 9. Result

Sprint 066 is closed 5/6 with T5 dropped. The guard-band failure is real and the clip pass fixes it; culling removes
92 % of a 1,000-object scene with no pixel change, though the work reduction is 4.5× rather than the 10× the culled
fraction suggests.

- **Guard band:** measured at screen x 2280 (draws) / 2290 (dropped), matching NDC 8.53 for a 480-wide viewport.
  A 3D triangle with a vertex at x=40 draws **236 px clipped** and **0 px** unclipped. A vertex behind the camera:
  **720 vs 0**.
- **Culling:** 1,000 cubes, 917 culled per frame, **0 pixel difference**; work 233.6 → 50–51 ms. The baseline is
  inflated by the clip pass and arena resets on the 83 visible-edge draws, so the reduction is 4.5×, not
  proportional; the criterion is recorded as not met rather than redefined.
- **Costs found:** the clip scan is O(vertices) per triangle draw; culling is ~50 µs per `DrawMesh` call; the
  cull-off baseline spends most of its time clipping off-screen cubes and syncing the arena.
- **ME:** Phase 9's sprint 051 (takeover) is `dropped`; there is no runtime to shadow-test, so T5 is `dropped`, not
  faked.
- **Gates:** 2D 0, 060 probe 0, 062 lit 0, 065 RTT 0, `basic_shapes` 284. Boot tag `b29`.

## 10. Follow-ups

- The 2D batched path is not clipped; a 2D triangle with a vertex outside the guard band still drops. Only 3D
  array draws matter for the corpus, but this is a real limit.
- Lines and points are not clipped; add them if a line-of-sight or particle path needs it.
- Culling cost (~50 µs/draw) is significant at 1,000 objects; a flat array of per-object bounds and one matrix
  pass would cut it. Measure before adopting.
- The clip scan transforms every vertex of every triangle draw; a cheap screen-space AABB pre-test per draw would
  skip most of it.
- ME culling is impossible without the Phase 9 runtime; the CPU path remains the only measured result.

