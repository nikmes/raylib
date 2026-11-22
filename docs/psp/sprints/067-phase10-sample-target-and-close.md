# Sprint 067 (P10-S6) — 3D sample target, regression and Phase 10 close

| | |
|---|---|
| Roadmap phase | [Phase 10 — 3D and advanced](../phases/10-3d-advanced/index.md) |
| Sprint | P10-S6 |
| Branch / base commit | `master` @ `dcfe2e61` (sprint 066 written) |
| Started / closed | 2026-11-22 (written) / — |
| Build number(s) | final pspgu and `libraylib.a` for the phase; content hashes recorded in §7 |
| Tasks | 0/5 done |

## 0. Handover

**Situation.** Sprints 060–066 give the phase a drawing mesh path, camera-relative matrices, GE-native vertices,
lighting/fog, render-to-texture and culling/clipping. What is missing is the thing the exit criteria actually ask
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

**First action.** Write the target rule and the sample spec into §5 T1 **before** running the final build: the
object count is frozen from the 066 measurements, not chosen after seeing the sample's frame times.

**Hard rules.**

- **Pre-register the target.** The sample's object count is fixed by the rule in T1 from the 066 big-scene numbers;
  the acceptance thresholds (median and p95) are written before the final run. Moving either afterwards makes the
  exit criterion void.
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

- [ ] 060–066 `done`, or the missing ones listed in the phase close as not met (S6 still runs with the remainder)
- [ ] `scene_3d` builds with every arm used by the earlier sprints; `psp_counters.o` is borrowed as in `scene_2d`
- [ ] reference captures archived for: 2D gate, 060 cube, 062 lit/fog, 065 RTT readback, 066 cull-on/off
- [ ] the 066 big-scene numbers exist; they are the input to T1's target rule
- [ ] Device reset between runs; app logs `DONE` and sleeps; two identical captures per point
- [ ] `tools/pspgu-build.sh --check` and `tools/pspgl-build.sh --check` both answer

## 5. Tasks

| ID | task | status | verification | evidence |
|---|---|---|---|---|
| T1 | Freeze the target: write the sample spec (scene, lights, fog, RTT panel, cull/clip on, world offset, frame count) and the rule `N = largest 250-step count whose two 066 runs both had p95 ≤ 15.0 ms`; record N before the final run | `todo` | N and thresholds committed before measurement; scene spec in §7 | — |
| T2 | Build `SAMPLE=1`: 480×272; a tessellated ground grid; N textured cubes via `DrawMesh`; one `GenMeshTorus` as the "model"; one directional + ambient light; linear fog; a 256×256 RTT panel rendered every frame; culling and clipping on; camera-relative at world offset 1000; fixed camera path; `STAT`/`SPLIT` + counters + `DONE` | `todo` | builds on pspgu and pspGL (RTT refuses cleanly there); two identical captures | — |
| T3 | Measure: two runs per point at N and adjacent points; median/p95/worst, work/swap, draws/vertices/culled/clipped, RTT flushes/syncs, full waits, VRAM and list high-water | `todo` | adjacent last-passing/first-failing pair recorded; both frozen-N runs pass | — |
| T4 | Full regression: 2D gate, `basic_shapes` 284, `scene_2d` corpus, bunnymark N=1,000 canary, 060 cube, 062 lit/fog, 065 RTT readback, 066 cull/clip; stub audit of pspgu (every unsupported call logs once) | `todo` | 0 diff / recorded tolerances; no silent no-op found | — |
| T5 | Close Phase 10: check each exit criterion in the page with evidence or record it not met plus the deferring sprint; tick the two "before starting" blockers; mark the sprint table done with numbers; write the phase result; update memory-map facts; archive captures; restore pspGL; strict docs build | `todo` | page has no unchecked criterion without a reason; strict build passes; pspGL `2ab7378e` installed | — |

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

T1 spec (fill before running): frozen N = ____; object mix = ____ cubes + 1 torus; lights = ____; fog = ____;
RTT target = 256×256 8888; world offset = 1000; frames = 300; driver = ____.

Protocol: device only; two runs per point; reset between; captures after `DONE`.

| run | build | workload | metric | value | notes |
|---|---|---|---|---|---|
| ceiling | pspgu | N−250 | median / p95 / work | | |
| ceiling | pspgu | N | median / p95 / work | | target: both pass |
| ceiling | pspgu | N+250 | median / p95 / work | | first failing |
| gate | pspgu | `batch_check MESH_SCENE=1` | differing pixels | | target 0 |
| gate | pspgu | `basic_shapes` | colours | | target 284 |
| gate | pspgu | `scene_2d` corpus | work per scene | | vs 023 |
| gate | pspgu | bunnymark N=1,000 | work | | vs 6.565 ms |
| gate | pspgu | 060/062/065/066 captures | differing pixels | | |
| budget | pspgu | sample boot + RTT | VRAM used/largest | | |
| waits | pspgu | sample | full GE waits | | target 0 |

## 8. Exit criteria

- [ ] §1 met: both frozen-N runs hold median and p95, with the adjacent failing point recorded
- [ ] the sample exercises a model, lighting, a textured mesh, fog, RTT and culling in one steady frame
- [ ] 0 full GE waits in the sample; work + swap accounted
- [ ] every cumulative gate passes or its tolerance is recorded and explained
- [ ] the pspgu stub audit finds no silent no-op; each unsupported call logs once
- [ ] Phase 10's four exit criteria are each `[x]` with evidence or explicitly `not met` with the deferring sprint
- [ ] the phase sprint table is `done` for 060–066 with numbers; the two starting blockers are resolved
- [ ] captures archived with build hashes and driver; pspGL restored; master/main level
- [ ] strict docs build passes

## 9. Result

—

## 10. Follow-ups

- s8 normals and their lit-capture validation → Phase 11 or a Phase 10 follow-up sprint
- Direct-path clipping without conversion, if it shows in the sample's work → Phase 11
- ME culling shadow run, when Phase 9's runtime lands → Phase 9 roadmap
- MRT, post-processing effects, skinned models → sample-porting / later roadmap
- The sample's numbers become the Phase 11 baseline and the release-gate reference
