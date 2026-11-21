# Sprint 028 (P13-S5) — Shapes interaction sweep A

| | |
|---|---|
| Roadmap phase | [Phase 13 — Samples porting](../phases/13-samples-porting/index.md) |
| Sprint | P13-S5 |
| Branch / base commit | set at open |
| Started / closed | — / — |
| Build number(s) | — |
| Tasks | 0/0 — placeholder |

## Status

**Placeholder — not started.** Gated by **`G1` — nub/pad input remap**, owned by
[Phase 8 — Input](../phases/08-input/index.md). The samples render already; this sprint exercises their
interaction.

## Planned scope

17 shapes samples:

- `shapes_ball_physics`
- `shapes_bouncing_ball`
- `shapes_clock_of_clocks`
- `shapes_colors_palette`
- `shapes_dashed_line`
- `shapes_digital_clock`
- `shapes_drag_puzzle`
- `shapes_easings_ball`
- `shapes_easings_box`
- `shapes_easings_rectangles`
- `shapes_easings_testbed`
- `shapes_ellipse_collision`
- `shapes_following_eyes`
- `shapes_hilbert_curve`
- `shapes_kaleidoscope`
- `shapes_lines_bezier`
- `shapes_math_sine_cosine`

## Why now

The first shapes slice after input exists. These samples cover mouse-driven geometry, key-driven animation and
raygui controls without touching textures or render targets, so a failure points at the shape or input path.

## Gate and first action

- Gate: `G1`, delivered by Phase 8.
- First action when the gate opens: run `shapes_bouncing_ball` with a pad-driven key and confirm the animation
  toggles; then sweep the list.
- Raygui samples: confirm the widget pointer follows the nub and a click is a button press, not a hold.

## Exit criteria (draft)

- [ ] every listed sample renders and its interaction is exercised on the device
- [ ] each `done` sample has a build marker, boot log, `DONE` line and two identical captures
- [ ] any shape-path failure is written into Phase 7 with the sample and the exact draw call
- [ ] findings written back to [Phase 13](../phases/13-samples-porting/index.md)
- [ ] no file under `examples/` modified

## Follow-ups

- Replace this placeholder with a full sprint from [the template](../templates/sprint.md) when scheduled.
- Do not renumber this sprint or reuse its ID.
