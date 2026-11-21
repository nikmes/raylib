# Sprint 033 (P13-S10) — Render targets: textures

| | |
|---|---|
| Roadmap phase | [Phase 13 — Samples porting](../phases/13-samples-porting/index.md) |
| Sprint | P13-S10 |
| Branch / base commit | set at open |
| Started / closed | — / — |
| Build number(s) | — |
| Tasks | 0/0 — placeholder |

## Status

**Placeholder — not started.** Gated by **`G4` — render targets / FBO on the native backend**, owned by
[Phase 10 — 3D and advanced](../phases/10-3d-advanced/index.md). Depends on P13-S9 proving the basic RTT path.

## Planned scope

5 texture samples:

- `textures_fog_of_war` — render target plus a texture update
- `textures_framebuffer_rendering` — 3D scene into a render target
- `textures_magnifying_glass` — render target plus blend
- `textures_mouse_painting` — render target, painting, and `LoadImageFromTexture` on save; also needs `G7`
  texture readback for the save path
- `textures_portal_window` — 3D render target with rlgl matrix work

## Why now

These are the texture-heavy RTT consumers. Once P13-S9 proves a target is sampled correctly, this sprint checks
targets combined with 3D, blending, painting and readback. `textures_mouse_painting` is the only sample that
needs both `G4` and `G7`.

## Gate and first action

- Gate: `G4`, from Phase 10; `G7` for `textures_mouse_painting`'s save path.
- First action when the gate opens: port `textures_fog_of_war`; it is the smallest target+texture case and
  exposes a blank or flipped target immediately.
- Interaction: mouse painting needs the `G1` nub mapping as well; if it is not in place, port the render path and
  leave the painting interaction `blocked` with the dependency named.

## Exit criteria (draft)

- [ ] every listed sample renders its target correctly, or is `dropped` with the missing feature named
- [ ] `textures_mouse_painting`'s save path is either working (`G7`) or explicitly `blocked`
- [ ] each `done` sample has a build marker, boot log, `DONE` line and two identical captures
- [ ] findings written back to [Phase 13](../phases/13-samples-porting/index.md)
- [ ] no file under `examples/` modified

## Follow-ups

- Replace this placeholder with a full sprint from [the template](../templates/sprint.md) when scheduled.
- Do not renumber this sprint or reuse its ID.
