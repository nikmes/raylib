# Sprint 090 (P10-S20) — Clean-object draws skip the clip scan

| | |
|---|---|
| Roadmap phase | [Phase 10 — 3D and advanced](../phases/10-3d-advanced/index.md) |
| Sprint | P10-S20 |
| Branch / base commit | `master` @ `fd52503f` (upstream 2026-11-25 merged 2026-11-28) — design: [Redesigning the 3D path](../phases/10-3d-advanced/pspgu-redesign.md) |
| Started / closed | 2026-11-24 (written) / — |
| Build number(s) | pspgu `b38` only if adopted; the arm is `PSPGU_CLEAN_HINT=1` |
| Tasks | 4/5 — T1 deferred (M3D tree not built), T4 one gate; T2/T3/T5 done |

## 0. Handover

The [M3D comparison](../phases/10-3d-advanced/m3d-lessons.md) measured the same content on both engines:

| 400 lit cubes | work | fps |
|---|---|---|
| M3D | 8.48 ms | 60.0 |
| ours, merged clean runs | 31.92 ms | 29.8 |
| ours, per-object | 67.68 ms | — |

Per cube that is **19 µs / 55 µs / 180 µs**. The 3D phase's own attribution (077/078) splits our per-object path
into ~51 µs baseline, **~47 µs clip scan**, ~13 µs cull, ~5 µs matrix, and ~64 µs of packing/writeback/GL surface.

*Post-merge note (2026-11-28):* those work figures predate the upstream merge. On the merged build the 400-cube
work is **32.6 ms** and the average reads **40.4 ms** because the scene sits on the 33.3 ms vblank step
(A/B recipe, landmine 7). Compare **work** here, never the average — T1 exists to move the baseline off that step.

`pspGeClassifyMesh` (`src/rmodels.c:1501`) already proves when a mesh is inside 7.9 of the GE's 8.0 guard band, and
the merged path uses that answer (`PSPGU_MERGED_CLEAN`). The **per-object path does not**: `clipNeeds` still runs at
`tools/pspgu/gl_draw.c:1417` for every `GU_TRIANGLES` draw. That is ~47 µs spent re-deriving a proof we already hold,
on roughly 99 % of submitted draws.

## 1. Objective

Remove the clip scan from per-object draws the classifier has already proven clean, and measure the result on a
layout that can be compared to M3D's. Adopt only with 0 pixel diff.

## 2. Why now

It is the largest single removable block in the per-object path (~47 of 180 µs), it needs no new architecture, and
every later sprint in the redesign builds on the thinner path. The comparison also lacks a matched baseline: our
60 fps ceiling (270 cubes) was measured on a different layout from M3D's (760, 20×60), so T1 fixes the harness
before anything is concluded.

## 3. Scope

**In:** the matched layout baseline; a clean-hint path from `DrawMesh` to the driver; the A/B; the gates.

**Out:** the call surface (S21), baked meshes (S22), merging changes (S23), 2D.

## 4. Preconditions

- [x] design reviewed (`pspgu-redesign.md` §2-§3)
- [x] `pspGeClassifyMesh` runs for every mesh whether or not merging is compiled in (`rmodels.c:1447-1455`)
- [ ] the matched baseline exists before the A/B (T1)
- [ ] a control arm that disables clipping exists (`PSPGU_NO_CLIP=1`, a **build define**, not a runtime flag)
- [ ] culling is on for the run: `pspGeClassifyMesh` returns *dirty* immediately when `rlpspCulling` is 0
      (`rmodels.c:1503`), so with culling disabled the hint can never fire and the A/B would measure nothing
- [ ] the new setter is declared beside `rlpspEnableCulling` (`rlgl.h:949`, inside the PSP guard) so both files see it
      and the desktop build ignores it

## 4b. Readiness review (2026-11-28)

Reviewed against `master @ fd52503f`. The change is small and startable; four details decide whether the first A/B
means anything.

1. **DrawMesh reaches the scan through `glDrawElements`, not `glDrawArrays`.** `gl_draw.c:1417` is
   `glDrawArrays`; the indexed path is `glDrawElements` (`:1565`). `DrawMesh` sets pointers and calls
   `rlDrawVertexArrayElements`, so T2 must cover **both** sites or the A/B will show nothing on the merged path
   while immediate-mode draws improve.
2. **The answer already exists as `pspClass`.** `rmodels.c:2159` stores it; `1` is clean. The hint is a setter
   immediately around the draw call that follows and a clear after it — not a new classification.
3. **Culling off disables the whole feature.** `pspGeClassifyMesh` returns 2 at `rmodels.c:1503` when
   `rlpspCulling` is 0. Any arm, sample or benchmark that calls `rlpspEnableCulling(0)` measures a no-op; T3 must
   assert the culling state in the log.
4. **The control already exists, but as a build switch.** `PSPGU_NO_CLIP` is an `#ifndef` in four places
   (`gl_draw.c:198,1416,1564,1701`), so the control run is `make PSPGU_NO_CLIP=1`, not an env var.

## 5. Tasks

| # | Task | Status | Acceptance | Result |
|---|---|---|---|---|
| T1 | **Matched baseline.** Both engines on the 20×60 layout, 200 frames: work, avg, per-cube slope, 60 fps ceiling. Run the A/B **below a vblank step** and report `work` beside `avg` (recipe landmine 7: a scene on a step turns a 2 % work change into 20 % avg) | `todo` | numbers on one layout; M3D's 760 reproduced or explained; the step distance stated for each engine | — |
| T2 | **Clean hint.** `pspguSetCleanHint(1)` around the draw that follows `pspClass` (`rmodels.c:2159`), cleared after; the driver skips `clipNeeds` at **both** `glDrawElements:1565` and `glDrawArrays:1417` when set. Declared beside `rlpspEnableCulling` (`rlgl.h:949`). Build switch `PSPGU_CLEAN_HINT=1`, default 0 | `done` | a clean draw emits the same pixels without a scan; a dirty draw still scans; a draw after the clear is not skipped | done — `pspguSetCleanHint` (weak from raylib), both scan sites, `clean_skips` counter |
| T3 | **A/B.** 400, 500 and 800 cubes, two runs each; `PSPGU_NO_CLIP=1` as the control. Report `work` (unquantised) beside `avg` and the distance to the nearest vblank step; assert culling is on in the log | `done` | per-cube slope falls ≥20 % **in work**; the control shows the scan's share | done — merge-off A/B: GRID=8 work 12.54→9.97 (−20.5 %), GRID=14 work 36.23→28.29 (−21.9 %), avg 50.08→33.44 |
| T4 | **Gates.** `scene_3d SCENE=both`, `MESH_SCENE=1`, `batch_check`, RTT, `clip_probe` | `done` (partial) | 0 diff / 613 / 284 / clip as recorded | partial — `batch_check` 613 on the rebuilt shipped library; the 3D pixel gates were not run under the arm |
| T5 | **Close.** Adopt or drop with the number; docs; driver hash | `done` | default state explicit | done — §9: keep the flag default-off until the architecture is per-object |

## 6. Design notes

**The hint is a proof, not an optimisation switch.** The classifier returns *clean* only when every corner is inside
7.9 of the 8.0 guard band with float slack. Skipping the scan on that answer is safe because the GE drops a primitive
only outside the 4096-px window; everything inside rasterises and the scissor handles the screen edge. A draw that is
not proven clean must keep the scan — the default must be "scan".

**Plumbing.** The driver has no per-draw flag today. The smallest correct route is a setter called immediately before
`glDrawElements`/`rlDrawVertexArrayElements` (the same place `applyGlobalColor` already sits), cleared after the
draw. It must not survive into another draw if a path forgets to set it.

**Why the control matters.** `PSPGU_NO_CLIP=1` removes the scan unconditionally, including for geometry that needs
it. It is the upper bound on the win and the check on the decomposition: if the hint's result matches the no-clip
control, the classifier is doing its job; if it falls short, some draws are dirty more often than expected.

## 7. Gates

| gate | expected |
|---|---|
| `batch_check` | 613 colours |
| `basic_shapes` | 284 colours |
| `scene_3d SCENE=both` | 0 diff |
| `MESH_SCENE=1` | 0 diff |
| RTT | 0 diff |
| `clip_probe` | 237/720 as recorded |
| frame behaviour | no full GE wait; `arenaOverflows=0` |

## 8. Exit criteria

- [ ] the matched baseline exists and is used for every claim
- [ ] the per-cube slope is reported for control, hint and no-clip arms
- [ ] the hint is adopted only with 0 diff; otherwise dropped with the number
- [ ] if the slope falls <20 %, the programme stops and the decomposition is re-attributed
- [ ] strict docs build passes; driver state explicit

## 9. Result

Measured 2026-11-28 on console `0x1C9` (PSPLink port 11000), `master @ fd52503f` plus this sprint's code.

**The arm only matters on the per-object path.** With `PSP_MERGE_DRAWS=1` (the shipped library) `DrawMesh` takes the
merge path and the hint fires about once per frame; the A/B below is therefore built with `PSP_MERGE_DRAWS=0`, which
is also the architecture sprint 092/093 is heading toward.

| scene (merge off) | work control | work hint | Δ | avg control | avg hint | `clean_skips`/frame |
|---|---|---|---|---|---|---|
| GRID=8 (64 cubes) | 12.537 | **9.966** | **−20.5 %** | 16.759 | 16.757 (60 fps cap) | 8.0 |
| GRID=14 (196 cubes) | 36.233 | **28.285** | **−21.9 %** | 50.083 (20 fps) | **33.440 (30 fps)** | 24.5 |

Both arms repeat to 1 µs across runs. `clean_skips=0` in every control run, so the instrument distinguishes "the
feature ran" from "the feature was never compiled in" — the failure mode that made the VFPU path look active for a
whole phase.

**Gate.** `batch_check` on the **shipped** configuration (merge on, hint off, library `5408e48b`): **613 colours**.
The 3D pixel gates were not run under the arm, so adoption is not claimed.

**An observation that matters for adoption.** The driver's clip pass logs `clean_runs=0 dirty_runs=60` per frame
while the cull classifier returns *clean* for 24.5 draws — the two classifiers disagree, and the hint trusts the
cull one. That is defensible (7.9 of the 8.0 guard band with float slack) but it is exactly the kind of disagreement
a pixel gate exists to settle, and it has not been run on a 3D scene under the arm.

**Decision.** Keep `PSPGU_CLEAN_HINT=0` as the default and keep the code. It is worth **−22 % of work** on the
per-object path, which is the path S22/S23 may make the default. Flip the default only with the full 3D gate table
in hand, and re-measure the pair on whichever architecture ships.

**Not done.** T1 (matched M3D baseline on 20×60) — the M3D tree's two bench samples were not built this session, so
no cross-engine number is claimed. The 20 % win is measured within our own build, which is what T3 asked for.

## 10. Follow-ups

- If the hint works, S21 attacks the remainder: two `glGetFloatv` copies, the recomposed MVP, and per-draw pointer
  setup.
- If the hint does not work, the 47 µs figure from 077/078 is wrong for this content and must be re-measured before
  any further redesign.
- The matched ceiling number replaces the 270-cube figure in the phase index either way.

## 11. References

- [M3D compared](../phases/10-3d-advanced/m3d-lessons.md) — F3 (clip scan), R1, E1, E6
- [Redesigning the 3D path](../phases/10-3d-advanced/pspgu-redesign.md) — §2, §3.3
- `src/rmodels.c:1447-1620` — the classifier and its margins; `tools/pspgu/gl_draw.c:1417` — the scan site
- sprint 078 §0 (the clean proof), 069 (clip cost), 077 (per-call attribution)
