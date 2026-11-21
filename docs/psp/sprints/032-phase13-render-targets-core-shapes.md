# Sprint 032 (P13-S9) — Render targets: core and shapes

| | |
|---|---|
| Roadmap phase | [Phase 13 — Samples porting](../phases/13-samples-porting/index.md) |
| Sprint | P13-S9 |
| Branch / base commit | set at open |
| Started / closed | — / — |
| Build number(s) | — |
| Tasks | 0/0 — placeholder |

## Status

**Placeholder — not started.** Gated by **`G4` — render targets / FBO on the native backend**, owned by
[Phase 10 — 3D and advanced](../phases/10-3d-advanced/index.md) with
[Phase 5 — Native GU backend](../phases/05-native-gu-backend/index.md). Under GL11, `rlLoadFramebuffer()` returns
0 and the target texture stays blank.

## Planned scope

11 samples:

- `core_2d_camera_split_screen`
- `core_3d_camera_split_screen`
- `core_render_texture`
- `core_smooth_pixelperfect`
- `core_viewport_scaling`
- `core_window_letterbox`
- `core_vr_simulator` — also needs shaders; if shaders stay absent, this sample is `dropped`, not ported
- `shapes_bullet_hell`
- `shapes_double_pendulum`
- `shapes_lines_drawing`
- `shapes_top_down_lights`

## Why now

These are the simplest render-target consumers: a texture is rendered to and then drawn as a quad, with no
model or lighting complexity. They are the acceptance test for the RTT path before the texture-heavy cases in
P13-S10.

## Gate and first action

- Gate: `G4`, from Phase 10.
- First action when the gate opens: port `core_render_texture` first; it is the smallest RTT consumer. Confirm
  the target texture is not blank, then add a frame-boundary sync check.
- Shader decision: if `core_vr_simulator` still cannot load a shader, mark it `dropped` in Phase 13 with the
  shader reason; do not fake the effect.

## Exit criteria (draft)

- [ ] every listed sample renders through a real render target, or is `dropped` with the missing feature named
- [ ] no sample reads a render target before it is complete; the sync rule is recorded
- [ ] `core_vr_simulator` has an explicit shader decision
- [ ] each `done` sample has a build marker, boot log, `DONE` line and two identical captures
- [ ] findings written back to [Phase 13](../phases/13-samples-porting/index.md)
- [ ] no file under `examples/` modified

## Follow-ups

- Replace this placeholder with a full sprint from [the template](../templates/sprint.md) when scheduled.
- Do not renumber this sprint or reuse its ID.
