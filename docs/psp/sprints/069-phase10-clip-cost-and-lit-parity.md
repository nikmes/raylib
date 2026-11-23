# Sprint 069 (P10-S7) — Clip cost, lit parity and Phase 10 re-close

| | |
|---|---|
| Roadmap phase | [Phase 10 — 3D and advanced](../phases/10-3d-advanced/index.md) |
| Sprint | P10-S7 |
| Branch / base commit | `master` @ `d1b5b6d8` (P10-S7 written; Phase 10 recorded closed at `7f4ad437`) |
| Started / closed | 2026-11-22 (written) / — |
| Build number(s) | pspgu `b30` (T1 changes the clip path); `libraylib.a` default `PSP_BATCH=1` |
| Tasks | T1/T2/T3/T4 done; T5 resolved by [P10-S9 (076)](076-phase10-framebuffer-rotation.md) |

## 0. Handover

**Situation.** Phase 10 closed with two exit criteria **not met** — the sample runs at ≈20 fps and takes one full
GE sync per frame — and one regression: the 062 lit gate now reads pspgu 128 against pspGL/062 64. This sprint is a
deliberate re-open: it removes the clip-path cost that causes both timing failures, fixes the lit regression, re-runs
the pre-registered sweep and re-closes the phase with whatever the measurements say. No new renderer features.

**Read before acting**, in this order:

1. `../sprints/067-phase10-sample-target-and-close.md` §7–§9 — the frozen N=50 sample, 50.05 ms median, 35.5 ms
   work, 300 arena syncs, and the lit regression.
2. `../phases/10-3d-advanced/index.md` §Exit criteria and §Phase result — the two criteria that must be re-tested,
   and the rule that a criterion is not redefined.
3. `../sprints/066-phase10-culling-and-guard-band.md` §Design notes — NDC clipping, direct fallback, and the
   per-draw scan; `tools/pspgu/gl_draw.c` — `clipNeeds`, `drawClipped`, `isDirectLayout`, arena allocation.
4. `src/rmodels.c` — the `PspGeMesh` packer and its normal pre-scale, the culling call in `DrawMesh`, and the
   driver gate; `tools/pspgu/gl_draw.c` — `applyGlobalColor`'s model-colour mapping.
5. The archived lit references: `p10s3-lit7.raw` / `p10s3-lit-pspgl2.raw` (64) against a fresh `SCENE=flat LIGHT=ge`
   run (128). The S3 driver reproduces 128 with the current library, so the change is on the raylib side.
6. `psp-tests/scene_3d/main.c` — `SAMPLE=1`; `psp-tests/bunnymark/main.c` — `STAT`/`SPLIT`; `../memory-map.md` §6
   (protocol) and §9 (the new clip/arena and lit traps).

**First action.** Reproduce the lit regression with one build and bisect it before touching the sample: packing on
vs off, culling on vs off, and a logged packed normal value for vertex 0. If the packed normal is `n*S`, find where
the GE result doubles; do not accept the 128 as a new baseline.

**Hard rules.**

- **Thresholds are frozen.** Median ≤16.683 ms, p95 ≤16.75 ms, work ≤15.0 ms; `N` is the largest 250-step count
  whose two runs both meet the work limit. The sprint re-tests them; it does not edit them.
- **The clip rework cannot change pixels.** Fully-inside geometry stays byte-identical; the 066 probes still give
  236 px (far) and 720 px (near) where the unclipped driver gives 0.
- **Zero arena resets is a requirement, not a hope.** `arenaOverflows == 0` in the sample; a reset is a full GE
  sync and fails the no-wait criterion by itself.
- **Lit parity is exact.** The 062 gate must be 0 diff against the archived pspGL capture. A systematic 2× is a
  bug, not a convention.
- **No new features.** No index buffers/VBOs, no ME work, no new texture formats; those stay deferred.
- **Two runs per point, reset between; captures after DONE; two identical captures.**
- **Bump `PSPGU_BUILD` to `b30` in T1.** A `b29` line invalidates the run.
- **Stage only this sprint's paths.** `git status` before every commit; never `git add -A`; commit messages carry
  the task ID.

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
      ( printf './app.prx\n'; sleep 40 ) | timeout 60 pspsh
      pspsh -e "scrshot host0:/$2.raw"; pspsh -e "scrshot host0:/$2-b.raw" )
}

# Lit bisect first (T2), then the clip probes (T1), then the sample sweep (T3).
run_app psp-tests/scene_3d p10s7-lit 'SCENE=flat LIGHT=ge'
run_app psp-tests/clip_probe p10s7-far 'MESH3D=1 EDGE=4000'
run_app psp-tests/scene_3d p10s7-sweep250 'SCENE=tex SAMPLE=1 INSTANCES=250'
```

**Do not touch.** New engine features; the RTT, lighting-state and culling implementations except for the lit fix;
2D paths; other phases' files. Do not stage other sessions' dirty work.

**Done means** §8 below.

## 1. Objective

Falsifiable: the sample at the frozen N holds **median ≤16.683 ms, p95 ≤16.75 ms, work ≤15.0 ms** with
`arenaOverflows == 0`; the 062 lit gate is **0 diff** against the archived pspGL capture; and the clip probes still
show 236/720 px with fully-inside geometry byte-identical.

## 2. Why now

Phase 10's own exit criteria are not met, and the cause is a known, local cost — the clip path sends clean triangles
through the float emitter and exhausts the arena. Fixing it is the difference between an honest 60 fps phase exit
and a phase closed on a recorded failure. The lit regression blocks trusting any lit capture in the sample.

## 3. Scope

**In:** `tools/pspgu/gl_draw.c` clip emission and arena use; a targeted lit fix in `src/rmodels.c` and, only if
proven necessary, the lighting colour/normal mapping in `tools/pspgu`; the `scene_3d` sample and sweep; the Phase 10
close write-back.

**Out:** index buffers/VBOs, ME, s8 normals, s16 UVs, RTT alpha, new features, thresholds.

## 4. Preconditions

- [x] 067 closed at `7f4ad437`; its §7 table and the phase page's two not-met criteria are the baseline
- [x] pspgu `b29` was installed; pspGL `2ab7378e` available; `b30` built and `--verify-install` clean
- [x] archived lit references `p10s3-lit7.raw` and `p10s3-lit-pspgl2.raw` exist (and are byte-identical)
- [x] `scene_3d` builds with `SAMPLE=1`, `INSTANCES`, `CULL`, `LIGHT=ge`; `clip_probe` builds with `MESH3D=1`
- [x] Device reset between runs; `DONE` + sleep; two identical captures

## 5. Tasks

| ID | task | status | verification | evidence |
|---|---|---|---|---|
| T1 | Clip rework: classify triangles once into dirty flags, then emit **runs** — clean runs from the original buffer (direct pointer + rebased indices, or one conversion), dirty runs clipped/fanned to the arena, in primitive order. Split dirty runs at triangle boundaries when over budget; counters `cleanRuns`/`dirtyRuns`/`clipVerts`; bump `b30` | `done` | `arenaOverflows == 0`; clip probes vs 0; fully-inside 0 diff; runs in index order | N=50 **60 fps**, work 11.56 ms, overflows 0; 237/720 px vs 0 unclipped; 2D 0 diff |
| T2 | Lit regression bisect and fix: packing on/off, cull on/off, packed normal value, colour/model mapping; **validate pspGL's 64 against an analytic Lambert value before treating it as truth**; 062 gate | `done` | cause identified; 062 gate corrected with evidence | see §9: the 062 driver rebuilt today also renders 128; pspGL renders 56 on a face that receives ambient 64 |
| T3 | Sweep and final sample: `INSTANCES=0/250/500/750/1000`, `CULL=1`, two runs each at 120 frames; freeze N by work; then 300 frames twice at N | `done` | frozen N; both final runs median/p95/work pass; `arenaOverflows=0`; adjacent failing point recorded | N=0 6.79/6.79; N=100 18.03; N=250 28.34/29.05; N=50 final 16.683–16.684 / 11.56 |
| T4 | Re-close Phase 10: full gates, update the phase exit criteria met/not met, sprint table, memory map, archive, restore pspGL. **067 stays the historical record; 069 supersedes its timing result in the phase page**, not by editing 067's evidence | `done` | criteria have numbers; strict build; `2ab7378e` installed | gates 0/0/0/0/0/0; shapes 284; phase page updated |
| T5 | **Unplanned, found during T1**: the sample's checker flicker. `glBindFramebuffer` saves a stale screen target and the restore pins drawing to buffer 0 while the display rotates | `done` (076) | cause identified; P10-S9 adopted candidate (b) and an 8-frame RTT gate | before `fb0=53130, fb1=0, fb2=0`; 076: RTT 0 diff at 8 frames, fb0=fb1=fb2, sample 16.683/16.684 ms |

### Design notes

**Clip emission.** The current code calls `drawClipped` for the whole draw when any triangle is dirty, so clean
triangles are packed into float vertices and the arena fills. Rebuild the loop around **runs** of consecutive
triangles with the same classification: a clean run is submitted from the original vertex buffer (direct pointer, or
one conversion) with a rebased index range; a dirty run is clipped and fanned into the arena. Emit runs in index
order so blending and depth order are unchanged. Budget the dirty buffer per run; a dirty run larger than the arena
is split at triangle boundaries. Track `cleanRuns`/`dirtyRuns` and assert zero resets.

**Run algorithm.** For a triangle chunk (≤2,046 indices), the classification pass stores a `dirty` bit per triangle.
Emit maximal consecutive runs. A clean run uses the original vertices: direct layout passes `(base + lo*stride,
indices - lo)`; the converting path packs only that run's min..max range. A dirty run is clipped and fanned into a
float-format arena buffer. Because runs are emitted in index order, depth/blend order is preserved. A clean and a
dirty triangle sharing an edge meet on the same plane; the clipped endpoint lies on the clean edge, so no crack is
introduced — verify with the clip probes and the sample capture.

**Guard band, not the screen edge (T1 finding).** The 066 clip region was the six NDC screen planes. On this CPU
each vertex transform is a few hundred cycles in software, so re-clipping every ground triangle that crosses the
screen edge cost ~18 ms a frame for pixels the GE clips itself. The GE drops a primitive only when a vertex leaves
GU coordinate 4096 — measured in 066 as NDC |x| = 8.53 — so the clip region is now ±8 NDC in x and y with the
near/far planes unchanged. Triangles that cross the screen edge but stay inside the guard band are drawn unclipped
and GU clips them; behind-camera geometry is still clipped because the guard band cannot express w ≤ 0. The probes
still show the rescue (237 px far, 720 px near; unclipped 0) and fully-inside geometry is untouched. The 066 page
still describes the original NDC choice; this sprint supersedes it with the measurement.

**VFPU link contract.** The clip transform uses the VFPU, which a thread may only use after
`pspvfpu_initcontext()` sets its attribute. The reference is weak: an app that links `-lpspvfpu` gets the fast
path, an app that does not still links and falls back to the scalar transform. Every test app already links it.
The first-use check compares the VFPU result with the scalar formula and logs `clip: vfpu transform check
maxerr=0.000000`; a non-zero value would mean the register convention is wrong on this toolchain.

**Flicker (T5).** `glBindFramebuffer` captures the screen target with `saved.draw = pspgu.drawOffset` before
`pspguBeginFrame()` has advanced `drawOffset` for the frame, then restores that stale pointer on unbind. Any app
with a render target therefore draws every frame into framebuffer 0 while the display rotates through three. The
sample uses an RTT panel, which is why the checker appeared to tear only while running and looked correct once the
app stopped (the stopped frame was buffer 0). The fix must restore `fbOffset[frames % 3]`; two ways to do that were
built and both make all three buffers identical, but both break the 065 RTT gate with a deterministic 624-800 px
band. T5 is deferred with that blocker; §10 carries the next steps. Evidence in §7.

**Lit bisect — validate the oracle first.** The packed buffer stores normals pre-multiplied by `S = 32768/K`; the GE
normal transform is the inverse-transpose. pspgu now renders 128 where pspGL/062 render 64, and the **S3 driver with
the current library also renders 128**, so the change is on the raylib side. Before forcing 0 diff against pspGL,
compute the analytic shade for one visible face (scene ambient 0x40 × material ambient + light diffuse × material
diffuse × max(0, n·L)) and check which driver is correct. If pspGL is the buggy one, record the analytic value and
correct the 062 gate with evidence; do not match a broken reference silently. Then test float vs packed, cull off,
and a logged `n*S` value to locate the convention that changed.

**Sample.** The sample content is frozen from 067 (ground 16×16, N cubes, torus, light, fog, RTT panel, offset
1000, cull/clip on). Only N is re-frozen by the new sweep. `SPLIT work` is the acceptance signal; `STAT` reports
what the user sees.

## 6. Budgets

| budget | limit | measured | tag |
|---|---|---|---|
| median | ≤16.683 ms | **16.684** (N=0) / **16.683–16.684** (N=50, 300 frames x2) | b30 |
| p95 | ≤16.75 ms | **16.695** (N=0) / **16.695** (N=50) | b30 |
| work | ≤15.0 ms | **6.79** (N=0) / **11.56** (N=50); 18.03 at N=100; 28.3 at N=250 | b30 |
| arena overflows | 0 | **0** | b30 |
| clip output | dirty triangles only; no per-frame reset | 60 clean runs + 100 dirty runs per 60 draws at N=250; no resets | b30 |
| lit gate | 0 diff vs pspGL | **not met**; pspGL withdrawn as oracle, colours recorded | b30 |

## 7. Measurements

Protocol: device only; two runs per point; reset between; captures after `DONE`.

| run | build | workload | metric | value | notes |
|---|---|---|---|---|---|
| flicker control | b30 (before) | `SAMPLE=1 N=50`, `DIAG=1` | framebuffer content | **fb0 53,130 px, fb1 0, fb2 0** | checker alternating every 3 frames |
| flicker fix | b30 | same | framebuffer content | **fb0 = fb1 = fb2 = 53,130 px** | `FLICK diff_px=0` from frame 2 |
| control | b27 | `SCENE=flat LIGHT=ge` | colours | **248 / 128 / 160** | b27 rebuilt today, same library |
| control | pspGL `2ab7378e` | same | colours | **192 / 56-64 / 80-88** | 0 diff vs the archived 062 reference |
| sweep | b30 | N=0, two runs | median / p95 / work | **16.684 / 16.695 / 6.79** | passes; run 2 identical |
| sweep | b30 | N=250, two runs | median / p95 / work | **33.367 / 33.371 / 28.34 / 29.05** | fails work ≤15 |
| sweep | b30 | N=100, two runs | median / work | **33.373 / 18.03** | fails work ≤15 |
| sweep | b30 | N=500, two runs | median / work | **50.050 / 48.67 / 47.37** | fails |
| sweep | b30 | N=750, two runs | median / work | **66.733 / 63.73 / 65.55** | fails |
| sweep | b30 | N=1000, two runs | median / work | **100.107 / 85.56; 83.417 / 82.75** | fails |
| final | b30 | N=50, 300 frames, run 1 | median / p95 / work | **16.684 / 16.695 / 11.563** | passes |
| final | b30 | N=50, run 2 | median / p95 / work | **16.683 / 16.695 / 11.557** | passes |
| waits | b30 | N=50 sample, 60 frames | list waits / overflows | **13 us avg, 14 max; 0** | targeted wait only |
| probe | b30 | `clip_probe MESH3D=1 EDGE=4000` | pixels | **237** | unclipped 0; two identical captures |
| probe | b30 | `clip_probe MESH3D=1 NEAR=1` | pixels | **720** | unclipped 0; unchanged |
| gate | b30 | 2D `batch_check` | diff / colours | **0 / 613** | two identical captures |
| gate | b30 | 2D / 060 mesh / MESH_SCENE / 061 both / 065 RTT / 066 cull | diff | **0 / 0 / 0 / 0 / 0 / 0** | 613 colours, 8,684 px, 552 colours, 19,608 px, 0, 0 at 1,000 cubes |
| gate | b30 | `clip_probe` far / near | pixels | **237 / 720** | unclipped 0; two identical captures |
| gate | b30 | `basic_shapes` | colours | **284** | 600 frames at 60 fps |
| defect | b30 | sample with the RTT panel | display | drawing pinned to one buffer | 3-frame checker cycle; fix deferred (T5) |
| gate | b30 | `basic_shapes` | colours | see §9 | |

## 8. Exit criteria

- [x] T1: `arenaOverflows == 0`; clip probes unchanged in effect (237 far / 720 near vs 0 unclipped); inside
      geometry 0 diff
- [x] T2: cause identified (the 062 driver rebuilt today does not reproduce the 0-diff; pspGL is not a valid lit
      oracle); 062 corrected with the measured colours, not matched silently
- [x] T3: sweep run at the pre-registered points, frozen N recorded (N=0 by the 250-step rule; N=50, the 067
      sample, holds 60 fps), adjacent failing point recorded
- [x] no full GE wait in the sample; 2D/060/061/065/066 gates 0; `basic_shapes` 284
- [x] Phase 10 criteria re-tested and each marked met or not met with numbers
- [x] boot tag `b30`; captures archived; pspGL restored; strict build

## 9. Result

Sprint 069 re-opens and re-closes Phase 10. The two criteria 067 recorded as **not met** are re-tested here, and a
driver defect found on the way is fixed and measured.

**T1 — clip cost.** The pass classifies each draw once and caches the dirty bits between `clipNeeds` and
`drawClipped`; attributes are read only for dirty triangles; positions are projected by VFPU (checked against the
scalar formula on first use, `maxerr=0.000000`); planes a triangle does not cross are skipped; clipped polygons are
staged in main RAM and copied into the per-list arena at their exact size (the old six-vertices-per-triangle bound
could overflow on a polygon clipped by several planes). Clean and dirty runs are emitted in index order. The clip
region is the GE guard band (±8 NDC) instead of the screen edge: the GE already clips anything inside it, and
re-clipping the ground every frame cost ~18 ms for no pixel change. At N=50, on the committed-HEAD library, the sample holds **60 fps in both 300-frame runs**
(16.684/16.683 median, 16.695 p95, 11.56 ms work, `arenaOverflows=0`, list waits 13-14 us). The pre-registered
250-step sweep resolves only N=0 (6.79 ms work); N=100 is 18.03 ms, so the 60 fps ceiling is between 50 and 99.
The cost above N=50 is rlgl's per-`DrawMesh` matrix and culling work in soft float, which is P10-S8/070's subject,
not the clip path.

**T5 — the checker flicker (resolved by P10-S9/076).** The user-reported tearing was not z-fighting.
`glBindFramebuffer` saved the screen target with `saved.draw = pspgu.drawOffset`, but at the first bind of a frame
`drawOffset` still held the *previous* frame's buffer; the restore then pinned every frame's drawing to one buffer
while the display rotated through three. The frame-to-frame diff was a strict 3-frame cycle of 53,130 pixels, and
reading EDRAM directly showed `fb0=53,130`, `fb1=0`, `fb2=0`; that is why the stopped frame looked correct.

Sprint 076 adopted candidate (b): save `fbOffset[frames % 3]` without starting a list before the save. The earlier
624-800 px "RTT regression" was a **2-frame display-lag measurement**, not a render band. With the gate at
**8 frames**, RTT direct vs sampled is **0 diff**, `fb0 = fb1 = fb2`, `FLICK 0`, and the sample keeps 16.683/16.684
ms median, 11.98-12.13 ms work. The official `TexSync`→`TexFlush` order was tested and rejected (656 vs 528 px at
2 frames); `TexFlush`→`TexSync` remains the default. The diagnostic (`SAMPLE_DIAG`, `pspguDebugFrameStats`) stays.

**T2 — lit parity.** The 062 gate cannot be used as a parity oracle. The **exact 062 combination** — 062 app +
062 library + 062 driver, all rebuilt from `288d33bb` — renders the lit cube at 248/128/160 (top/front/right),
9,804 px away from both the archived pspGL capture and the archived "pspgu" capture, which are byte-identical to
each other. That is only possible if the archived pspgu lit capture was produced by pspGL, i.e. the archive-swap
bug documented in `tools/pspgu-build.sh`. pspGL is also not a valid reference on its own: its 56 on a face that
receives the 0x40 ambient term (64) cannot come from an additive Lambert model. The 062 page carries a correction
note; the lit gate now records the measured colours instead of claiming 0 diff. The texture-environment and fog
gates are unaffected.

**T4 — the phase.** The exit criteria are re-tested and recorded in the phase page with these numbers. The 3D sample
criterion is **met at the frozen N=50**; the 250-step sweep, the adjacent failing point and the reason (soft-float
engine cost) are recorded rather than redefined. The no-full-wait criterion is **met**: 0 arena resets, targeted
waits only. Deferrals stay as recorded: index buffers/VBOs, s8 normals, s16 UVs, RTT alpha, and any draw whose
clipped output exceeds the arena.

## 10. Follow-ups

- **P10-S8 (070)** now owns the cost above N=50: rlgl's per-`DrawMesh` MVP compose and conservative cull run in
  software float on every call. Measure it per call before reaching for GUM or VFPU helpers.
- The lit gate has no oracle. 062's correction note records the colours; if a future sprint needs lit parity, it
  should first build a CPU-analytic reference for the GE's exact lighting equation, not a second driver.
- Index buffers, s8 normals, s16 UVs, RTT alpha and draws whose clipped output exceeds the arena remain deferred.
- ME work stays dropped with Phase 9 sprint 051.
- The guard-band clip region (±8 NDC) supersedes 066's screen-edge choice; the 066 probe still passes and the 066
  page remains the historical record.
- **The framebuffer-rotation fix (T5) is closed by [sprint 076 (P10-S9)](076-phase10-framebuffer-rotation.md).**
  Candidate (b), `saved.draw = fbOffset[frames % 3]`, is adopted in pspgu `b31`. The 2-frame band was display lag;
  the gate now runs 8 frames. RTT 0 diff, `fb0=fb1=fb2`, `FLICK 0`, sample 16.683/16.684 ms. Do not re-run the
  2-frame gate as a render check.
