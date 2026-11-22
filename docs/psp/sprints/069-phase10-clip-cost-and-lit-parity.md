# Sprint 069 (P10-S7) — Clip cost, lit parity and Phase 10 re-close

| | |
|---|---|
| Roadmap phase | [Phase 10 — 3D and advanced](../phases/10-3d-advanced/index.md) |
| Sprint | P10-S7 |
| Branch / base commit | `master` @ `d1b5b6d8` (P10-S7 written; Phase 10 recorded closed at `7f4ad437`) |
| Started / closed | 2026-11-22 (written) / — |
| Build number(s) | pspgu `b30` (T1 changes the clip path); `libraylib.a` default `PSP_BATCH=1` |
| Tasks | 0/4 done |

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

- [ ] 067 closed at `7f4ad437`; its §7 table and the phase page's two not-met criteria are the baseline
- [ ] pspgu `b29` installed (pspGL is currently restored); pspGL `2ab7378e` available; `b30` planned
- [ ] archived lit references `p10s3-lit7.raw` and `p10s3-lit-pspgl2.raw` exist
- [ ] `scene_3d` builds with `SAMPLE=1`, `INSTANCES`, `CULL`, `LIGHT=ge`; `clip_probe` builds with `MESH3D=1`
- [ ] Device reset between runs; `DONE` + sleep; two identical captures

## 5. Tasks

| ID | task | status | verification | evidence |
|---|---|---|---|---|
| T1 | Clip rework: classify triangles once into dirty flags, then emit **runs** — clean runs from the original buffer (direct pointer + rebased indices, or one conversion), dirty runs clipped/fanned to the arena, in primitive order. Split dirty runs at triangle boundaries when over budget; counters `cleanRuns`/`dirtyRuns`/`clipVerts`; bump `b30` | `todo` | `arenaOverflows == 0`; clip probes 236/720 vs 0; fully-inside 0 diff; no seams at clean/dirty boundaries | — |
| T2 | Lit regression bisect and fix: packing on/off, cull on/off, packed normal value, colour/model mapping; **validate pspGL's 64 against an analytic Lambert value before treating it as truth**; 062 gate | `todo` | lit cube 0 diff vs a validated pspGL, or a documented correction with analytic evidence; cause identified | — |
| T3 | Sweep and final sample: `INSTANCES=0/250/500/750/1000`, `CULL=1`, two runs each at 120 frames; freeze N by work; then 300 frames twice at N | `todo` | frozen N; both final runs median/p95/work pass; `arenaOverflows=0`; adjacent failing point recorded | — |
| T4 | Re-close Phase 10: full gates, update the phase exit criteria met/not met, sprint table, memory map, archive, restore pspGL. **067 stays the historical record; 069 supersedes its timing result in the phase page**, not by editing 067's evidence | `todo` | criteria have numbers; strict build; `2ab7378e` installed | — |

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
| median | ≤16.683 ms | | |
| p95 | ≤16.75 ms | | |
| work | ≤15.0 ms | | |
| arena overflows | 0 | | |
| clip output | dirty triangles only; no per-frame reset | | |
| lit gate | 0 diff vs pspGL | | |

## 7. Measurements

Protocol: device only; two runs per point; reset between; captures after `DONE`.

| run | build | workload | metric | value | notes |
|---|---|---|---|---|---|
| control | b29 | `SCENE=flat LIGHT=ge` | colours | **128** | regression |
| change | T2 | same | diff vs archived pspGL | | target 0 (64) |
| control | b29 | `SAMPLE=1 INSTANCES=50` | median / work / overflows | 50.05 / 35.5 / 300 | |
| change | T1 | clip probes | pixels | | 236 / 720 |
| sweep | b30 | N=0/250/500/750/1000 | work, 2 runs | | freeze N |
| final | b30 | N, 300 frames, 2 runs | median / p95 / work / overflows | | target pass |
| gate | b30 | 2D/060/061/065/066 | diff | | 0 |

## 8. Exit criteria

- [ ] T1: `arenaOverflows == 0`; clip probes unchanged (236/720); inside geometry 0 diff
- [ ] T2: 062 lit gate 0 diff vs a validated pspGL (or a documented analytic correction); cause recorded
- [ ] T3: frozen N written before the final run; both runs meet median/p95/work; adjacent failure recorded
- [ ] no full GE wait in the sample; 2D/060/061/065/066 gates 0; `basic_shapes` 284
- [ ] Phase 10 criteria re-tested and each marked met or not met with numbers
- [ ] boot tag `b30`; captures archived; pspGL restored; strict build

## 9. Result

—

## 10. Follow-ups

- If 60 fps is still not reachable after the clip fix, record the achieved ceiling and close the criterion as not
  met; Phase 11 owns further optimisation.
- Index buffers, s8 normals, s16 UVs, RTT alpha and draws over the arena remain deferred.
- ME work stays dropped with Phase 9 sprint 051.
