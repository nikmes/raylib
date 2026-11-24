# Sprint 090 (P10-S20) — Clean-object draws skip the clip scan

| | |
|---|---|
| Roadmap phase | [Phase 10 — 3D and advanced](../phases/10-3d-advanced/index.md) |
| Sprint | P10-S20 |
| Branch / base commit | `master` @ `fe1171fe` — design: [Redesigning the 3D path](../phases/10-3d-advanced/pspgu-redesign.md) |
| Started / closed | 2026-11-24 (written) / — |
| Build number(s) | pspgu `b38` only if adopted; the arm is `PSPGU_CLEAN_HINT=1` |
| Tasks | 0/5 |

## 0. Handover

The [M3D comparison](../phases/10-3d-advanced/m3d-lessons.md) measured the same content on both engines:

| 400 lit cubes | work | fps |
|---|---|---|
| M3D | 8.48 ms | 60.0 |
| ours, merged clean runs | 31.92 ms | 29.8 |
| ours, per-object | 67.68 ms | — |

Per cube that is **19 µs / 55 µs / 180 µs**. The 3D phase's own attribution (077/078) splits our per-object path
into ~51 µs baseline, **~47 µs clip scan**, ~13 µs cull, ~5 µs matrix, and ~64 µs of packing/writeback/GL surface.

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
- [ ] a control arm that disables clipping exists (`PSPGU_NO_CLIP=1`)

## 5. Tasks

| # | Task | Status | Acceptance | Result |
|---|---|---|---|---|
| T1 | **Matched baseline.** Both engines on the 20×60 layout, 200 frames: work, avg, per-cube slope, 60 fps ceiling | `todo` | numbers on one layout; M3D's 760 either reproduced or explained | — |
| T2 | **Clean hint.** `DrawMesh` passes the classifier's answer to the driver; `clipNeeds` is skipped for that draw when set. Build switch `PSPGU_CLEAN_HINT=1`, default 0 | `todo` | a clean draw emits the same pixels without a scan; a dirty draw still scans | — |
| T3 | **A/B.** 400, 500 and 800 cubes, two runs each; `PSPGU_NO_CLIP=1` as the control | `todo` | per-cube slope falls ≥20 %; the control shows the scan's share | — |
| T4 | **Gates.** `scene_3d SCENE=both`, `MESH_SCENE=1`, `batch_check`, RTT, `clip_probe` | `todo` | 0 diff / 613 / 284 / clip as recorded | — |
| T5 | **Close.** Adopt or drop with the number; docs; driver hash | `todo` | default state explicit | — |

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

Pending — no device runs yet.

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
