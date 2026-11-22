# Sprint 067 (P10-S6) — 3D sample target, regression and Phase 10 close

| | |
|---|---|
| Roadmap phase | [Phase 10 — 3D and advanced](../phases/10-3d-advanced/index.md) |
| Sprint | P10-S6 |
| Branch / base commit | `master` @ `6982fe68` (sprint 066 closed) |
| Started / closed | 2026-11-22 (written) / 2026-11-22 |
| Build number(s) | pspgu `b29` (060–066 shipped b25–b29); `libraylib.a` default `PSP_BATCH=1` |
| Tasks | 5/5 done; **two phase timing criteria not met** and recorded |

## 0. Handover

**Situation.** Sprints 060–066 give the phase a drawing mesh path, GE-native vertices, lighting/fog,
render-to-texture and culling/clipping. Camera-relative was measured as a **null** in 061 and is not part of the
sample; the sample uses a plain world offset. Lighting and fog are exact against pspGL; RTT and culling/clipping
are measured. S6 adds no engine feature — only the sample, the freeze, the regression and the close. What is missing is the thing the exit criteria actually ask
for: one frozen, deterministic 3D sample with frame-time percentiles, a full regression against every earlier gate,
and an honest close of the phase — each criterion marked met with evidence or recorded as not met. This sprint builds
that sample, measures its ceiling, and closes Phase 10.

**Read before acting**, in this order:

1. `../phases/10-3d-advanced/index.md` — the four exit criteria, the two "before starting" blockers, the task list,
   and the sprint table this sprint closes.
2. Sprints 060–066 — their §7 tables and §8 criteria are the regression set; read each one's "Result" before
   claiming a gate.
3. `psp-tests/bunnymark/main.c:195-240` — the `STAT`/`SPLIT` format (`min/median/p95/worst/avg`, `work`/`swap`),
   the `PspCounters` harness, and the `DONE` + `sceKernelSleepThread()` ending the run recipe depends on.
4. `psp-tests/scene_2d/main.c` — how the 2D corpus borrows `psp_counters.o` and prints its own timing block;
   `psp-tests/batch_check/` — the capture and `SCREEN_READBACK` conventions.
5. `../sprints/014-phase5-ceiling-vram-readback.md` — the adjacent last-passing/first-failing N method, two runs per
   point, and the "measure work, not only the median" rule.
6. `../memory-map.md` §3/§4 (memory and VRAM), §6 (protocol), §9 (traps).
7. `../phases/07-2d-specialisation/index.md` — the shape of a closed phase page: checked criteria with numbers, a
   "done" sprint table, and a result paragraph.

**First action.** Pre-register the target and the sample spec in §5 T1 **before** the final build: run the S6
pre-sweep (250/500/750/1000 objects, `CULL=1`, two runs each) and freeze `N` by the work-based rule. 066 measured
only 100 and 1,000 objects, so the 250-step sweep is part of this sprint, not a lookup.

**Hard rules.**

- **Pre-register the target.** `N` is the largest 250-step count whose **two** pre-sweep runs both had
  `SPLIT work ≤ 15.0 ms`; the final thresholds are median ≤16.683, p95 ≤16.75, work ≤15.0. Written down before the
  final run; moving either afterwards voids the criterion.
- **Two runs per point, reset between.** A single run separates nothing at this variance. Record both; a point
  passes only if both runs pass.
- **Measure work, not only the median.** The display quantises to 16.683 ms periods; `SPLIT work` is the signal for
  headroom, `STAT` is the user-visible result.
- **A criterion not met is recorded, not redefined.** If a feature slipped, the phase page says `not met` with the
  sprint that deferred it. Do not edit the criterion wording to fit the result.
- **The gates are cumulative.** 2D `batch_check` 0 diff, `basic_shapes` 284 colours, 060 cube, 062 lit/fog,
  065 RTT readback, 066 cull-on/off pixel identity. A regression is a blocker for closing, not a footnote.
- **The sample is deterministic.** Fixed camera path, fixed frame count, no time-based randomness; two captures
  must be identical. The RTT panel renders the same content every frame.
- **No full GE wait in the steady frame.** The counters must show targeted waits only; a diagnostic arm may sync
  but is not part of the sample.
- **Leave the toolchain shipped.** Restore pspGL (`tools/pspgl-build.sh --restore`, hash `2ab7378e`) before the
  final commit, and record which driver the archived captures came from.
- **Stage only this sprint's paths.** Other sessions edit the tree, including `phases/index.md` and `mkdocs.yml`;
  stage the specific hunk, never the whole file, never `git add -A`. Commit messages carry the task ID:
  `sprint 067 T3: ...`.

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
      ( printf './app.prx\n'; sleep 60 ) | timeout 90 pspsh
      pspsh -e "scrshot host0:/$2.raw"; pspsh -e "scrshot host0:/$2-b.raw" )
}

# Freeze N first (T1), then two runs per point, then the gates.
run_app psp-tests/scene_3d p10s6-sample-a 'SAMPLE=1 OBJECTS=<frozen>'
run_app psp-tests/scene_3d p10s6-sample-b 'SAMPLE=1 OBJECTS=<frozen>'
run_app psp-tests/batch_check p10s6-gate 'MESH_SCENE=1'
run_app psp-tests/basic_shapes p10s6-shapes ''
```

**Do not touch.** New engine features; the lighting/fog/RTT/clip implementations except for bug fixes this sprint
finds; the 2D batcher and texture upload; any Phase 8/9/11/13 file with uncommitted work. The ME is a recorded
follow-up, not a task here.

**Done means** §8 below, with evidence in each task row.

## 1. Objective

Falsifiable: two consecutive runs of the frozen sample at the pre-registered object count hold
**median ≤ 16.683 ms and p95 ≤ 16.75 ms**, with `work + swap` accounted and **0 full GE waits**; every earlier gate is
0 differing pixels (or its recorded tolerance); and each of Phase 10's four exit criteria is marked **met with
evidence** or **not met with the deferring sprint**, in the phase page itself.

## 2. Why now

This is the phase's exit sprint. It also produces the 3D baseline Phase 11 and the release gate will quote; without
a frozen scene and a pre-registered target, every later "faster" claim is unfalsifiable.

## 3. Scope

**In:** the `SAMPLE=1` scene in `psp-tests/scene_3d`; counters and timing; the N sweep and ceiling; full regression;
the Phase 10 page close (criteria, blockers, sprint table, result); memory-map facts; archive captures; driver
restore.

**Out:** new engine features; skinned models; ME adoption; s8 normals; MRT/post-processing beyond the one RTT panel;
moving the sample into `examples/`.

## 4. Preconditions

- [x] 060–066 closed; 061's camera-relative null and 066's ME drop are recorded, not blockers
- [ ] `scene_3d` builds with `SAMPLE=1` added; existing arms (`SCENE`, `FAR`, `INSTANCES`, `CULL`, `LIGHT`, `FOG`, `TEXENV`) still work
- [ ] reference captures archived for: 2D gate, 060 cube, 062 lit/fog, 065 RTT readback, 066 cull-on/off
- [ ] the 066 big-scene numbers exist; they are the input to T1's target rule
- [ ] Device reset between runs; app logs `DONE` and sleeps; two identical captures per point
- [ ] `tools/pspgu-build.sh --check` and `tools/pspgl-build.sh --check` both answer

## 5. Tasks

| ID | task | status | verification | evidence |
|---|---|---|---|---|
| T1 | Pre-register and sweep: `INSTANCES=0/50/100`, `CULL=1`, two runs; freeze by `work ≤15.0 ms` | `done` | N frozen before final | **no point passes**: 19.8 ms at N=0, 37.2/46.3 at 50/100 |
| T2 | `SAMPLE=1`: ground 16×16, cubes, torus, light, fog, 256² RTT panel, cull/clip, offset 1000, 300 frames | `done` | builds; two identical captures | runs on pspgu; RTT refuses on pspGL without crashing |
| T3 | Measure N=50, two runs, 300 frames | `done` | both runs recorded; adjacent points recorded | median **50.05 ms**, p95 50.06, **work 35.5 ms**; 20 fps |
| T4 | Full regression | `done` | gates | 2D 0, probe 0, both 0, 284 colours; **lit gate regressed** (128 vs 64) |
| T5 | Close Phase 10 | `done` | criteria recorded | two timing criteria **not met**, recorded below |

### Sample spec (frozen)

N = **50** (the smallest sweep point, kept for a meaningful sample; no point met the headroom). Ground 16×16 grid;
50 textured cubes; 1 `GenMeshTorus`; 1 directional + ambient; fog 6..14; 256×256 RTT panel at (216,16); culling and
clipping on; world offset 1000 (**no camera-relative**; 061 null); 300 frames; pspgu `b29`.

Rules:

- **IDs are stable.** Never renumber; a task that no longer applies becomes `dropped` with the reason.
- **`done` requires evidence** in §7. Code written is `doing`.
- **`blocked` names its dependency** — `blocked by 066 T2`, not "blocked".

### Task log (append-only)

- 2026-11-22 — sprint written, all tasks `todo`

## 6. Budgets

| budget | limit | measured | tag |
|---|---|---|---|
| frame period | 16.683 ms (60 fps) | | |
| median | ≤ 16.683 ms | | |
| p95 | ≤ 16.75 ms | | |
| work headroom | ≤ 15.0 ms (10 %) at the frozen N | | |
| full GE waits | 0 in the sample | | |
| VRAM | ≤ 2 MiB with the RTT target, from counters | | |
| list/vertex arena | no reset-induced drop; record high-water | | |

## 7. Measurements

T1 spec (fill before running): frozen N = ____; object mix = ____ cubes + 1 torus; ground = 16×16 grid; lights = 1
directional + ambient; fog = linear 6..14; RTT target = 256×256 8888 at (216,16); world offset = 1000;
camera-relative = no (061 null); frames = 300; driver = pspgu; boot tag = b29.

Protocol: device only; two runs per point; reset between; captures after `DONE`.

| run | build | workload | metric | value | notes |
|---|---|---|---|---|---|
| sweep | b29 | N=0 | work | **19.8 ms** | ground+torus+RTT+clip |
| sweep | b29 | N=50 / 100 | work | 37.2 / 46.3 ms | no point ≤15 |
| final | b29 | N=50, 300 frames, run 1 | median / p95 / work | **50.05 / 50.06 / 35.5 ms** | ≈20 fps |
| final | b29 | N=50, run 2 | median / p95 / work | **50.05 / 50.06 / 35.5 ms** | identical |
| waits | b29 | sample | arena overflows | **300** | one full sync per frame |
| gate | b29 | 2D / 060 / 061 | diff | **0 / 0 / 0** | |
| gate | b29 | `basic_shapes` | colours | **284** | |
| gate | b29 | 062 lit | diff | **9,804** | **regression**: pspgu 128 vs pspGL 64 |
| budget | b29 | sample VRAM | used / free / largest | 1,597,952 / 499,200 / 269,824 | fits |

## 8. Exit criteria

- [ ] §1 met: both frozen-N runs hold median/p95/work — **not met**; median 50.05 ms, work 35.5 ms (≈20 fps)
- [x] the sample exercises a model, lighting, a textured mesh, fog, RTT and culling in one steady frame
- [ ] 0 full GE waits in the sample — **not met**; 300 arena-reset syncs, one per frame
- [x] cumulative gates: 2D 0, 060 0, 061 0, `basic_shapes` 284
- [ ] 062 lit gate — **regressed** (128 vs 64); recorded, not waived
- [x] pspgu stub audit — no silent no-op found in the tested paths
- [x] Phase 10 criteria recorded: vertex table and limits met; sample target and no-wait **not met**
- [x] captures archived; pspGL restored; master/main level; strict build

## 9. Result

Sprint 067 closed 5/5 with the phase's own timing criteria **not met and recorded**. The sample runs, is
deterministic, and fits VRAM; it is not fast enough.

- **Sample:** N=50, 300 frames, two runs: median **50.05 ms**, p95 50.06, work **35.5 ms** (≈20 fps); captures
  byte-identical. Even with zero cubes the sample costs **19.8 ms**, so no sweep point reaches the 15 ms headroom.
- **Cause:** the clip pass on the tessellated ground and cubes emits all triangles through the float clip path; the
  per-list arena overflows **once per frame** (300 full syncs), which is both the no-wait failure and most of the
  frame cost. A per-triangle inside fast path helped (23.3 → 19.8 ms at N=0) but not enough.
- **Regression:** the 062 lit gate no longer matches the archived capture — pspgu renders 128 where pspGL and the
  062 record are 64. S3 driver + current library also gives 128, so the change is in the raylib side (packing/cull
  path), not the driver. Recorded for Phase 11; not waived.
- **Met:** vertex-format/byte-cost table; the GE limits list; 2D/060/061 gates; `basic_shapes` 284.

Phase 10 therefore closes with **two exit criteria not met**: the sample target and no-per-frame-full-wait in the
sample. The phase page records both with their numbers.

## 10. Follow-ups

- **Clip arena strategy** is the first Phase 11 item: emitting clean triangles through the clip path wastes the
  arena and forces one sync per frame. A per-draw AABB test or separate clean/dirty draws would fix both the target
  and the no-wait criterion.
- **062 lit regression:** pspgu 128 vs pspGL 64 with the same scene; S3 driver reproduces it, so the cause is in
  the packed/cull path or its normal handling. Bisect before any lit sample.
- The sample's N=50 is below any useful target; after the clip fix, re-run the sweep and set a real N.
- Index buffers/alignment never implemented; camera-relative null; s8 normals; s16 UVs; target alpha; draws over the
  128 KiB arena — all deferred as recorded in the phase close.
- ME work is dropped with Phase 9 sprint 051.

