# Sprint 029 (P13-S6) — Shapes sweep B and input testbeds

| | |
|---|---|
| Roadmap phase | [Phase 13 — Samples porting](../phases/13-samples-porting/index.md) |
| Sprint | P13-S6 |
| Branch / base commit | set at open |
| Started / closed | — / — |
| Build number(s) | — |
| Tasks | 0/0 — placeholder |

## Status

**Placeholder — not started.** Gated by **`G1` — nub/pad input remap**, owned by
[Phase 8 — Input](../phases/08-input/index.md). The four touch-only samples are **dropped**, not tasks.

## Planned scope

16 shapes samples:

- `shapes_mouse_trail`
- `shapes_outlines_testbed`
- `shapes_penrose_tile`
- `shapes_pie_chart`
- `shapes_polygon_lines`
- `shapes_rectangle_scaling`
- `shapes_recursive_tree`
- `shapes_ring_drawing`
- `shapes_rlgl_color_wheel`
- `shapes_rlgl_triangle`
- `shapes_rounded_rectangle_drawing`
- `shapes_simple_particles`
- `shapes_splines_drawing`
- `shapes_starfield_effect`
- `shapes_triangle_strip`
- `shapes_vector_angle`

Plus 5 keyboard/mouse testbeds rescued by `G1`:

- `core_input_keys`
- `core_input_mouse`
- `core_input_mouse_wheel`
- `core_keyboard_testbed`
- `core_undo_redo`

Dropped, with the reason recorded in Phase 13: `core_input_gestures`, `core_input_gestures_testbed`,
`core_input_multitouch`, `core_input_virtual_controls` — no touchscreen on a PSP.

## Why now

This closes the input wave. It exercises the remaining shape paths (rlgl triangles, raygui panels, particles,
scaling) and the keyboard/mouse testbeds, which are the acceptance test for the `G1` mapping itself.

## Gate and first action

- Gate: `G1`, delivered by Phase 8.
- First action when the gate opens: run `core_keyboard_testbed` and `core_input_mouse` first; if the remap is
  wrong, every other sample in this sprint will look broken.
- Touch samples: mark `dropped` with the one-line reason; do not invent a touch device.

## Exit criteria (draft)

- [ ] every listed sample renders and its interaction is exercised on the device
- [ ] the five testbeds show the mapped input and a correct edge (press once, not every frame)
- [ ] the four touch samples are `dropped` with a reason in Phase 13
- [ ] each `done` sample has a build marker, boot log, `DONE` line and two identical captures
- [ ] no file under `examples/` modified

## Follow-ups

- Replace this placeholder with a full sprint from [the template](../templates/sprint.md) when scheduled.
- Do not renumber this sprint or reuse its ID.
