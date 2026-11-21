# Sprint 027 (P13-S4) — Core cameras and UI

| | |
|---|---|
| Roadmap phase | [Phase 13 — Samples porting](../phases/13-samples-porting/index.md) |
| Sprint | P13-S4 |
| Branch / base commit | set at open |
| Started / closed | — / — |
| Build number(s) | — |
| Tasks | 0/0 — placeholder |

## Status

**Placeholder — not started.** Gated by **`G1` — nub/pad input remap**, owned by
[Phase 8 — Input](../phases/08-input/index.md). The camera samples need a working nub path and the
file/UI samples need the pad→key mapping.

## Planned scope

10 core samples:

- `core_2d_camera`
- `core_2d_camera_platformer`
- `core_2d_camera_mouse_zoom`
- `core_3d_camera_first_person`
- `core_3d_camera_fps`
- `core_3d_camera_free`
- `core_3d_picking`
- `core_directory_files` — `LoadDirectoryFiles` on a PSP path
- `core_screen_recording` — `LoadImageFromScreen` + GIF write; needs a writable output path
- `core_world_screen` — 3D camera and world→screen projection

## Why now

This is the camera/UI half of the input wave. It checks the nub-to-mouse mapping on camera controls and the
filesystem/readback paths that later samples reuse. It also gives Phase 10 a 3D camera baseline before the
render-target work.

## Gate and first action

- Gate: `G1`, delivered by Phase 8.
- First action when the gate opens: run `core_3d_camera_mode` (already in P13-S1) as the 3D control, then
  `core_world_screen` with a fixed camera before adding input.
- Output path: decide `ms0:/` versus `host0:/` for `core_screen_recording` and record it.

## Exit criteria (draft)

- [ ] every listed sample renders and its interaction is exercised on the device
- [ ] `core_directory_files` lists a real PSP directory; `core_text_file_loading` is in P13-S3
- [ ] `core_screen_recording` either writes a readable GIF or is `blocked` with the output-path reason
- [ ] each `done` sample has a build marker, boot log, `DONE` line and two identical captures
- [ ] findings written back to [Phase 13](../phases/13-samples-porting/index.md)
- [ ] no file under `examples/` modified

## Follow-ups

- Replace this placeholder with a full sprint from [the template](../templates/sprint.md) when scheduled.
- Do not renumber this sprint or reuse its ID.
