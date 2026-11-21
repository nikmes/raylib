# Sprint 026 (P13-S3) — Input remap and core input slice

| | |
|---|---|
| Roadmap phase | [Phase 13 — Samples porting](../phases/13-samples-porting/index.md) |
| Sprint | P13-S3 |
| Branch / base commit | set at open |
| Started / closed | — / — |
| Build number(s) | — |
| Tasks | 0/0 — placeholder |

## Status

**Placeholder — not started.** Gated by **`G1` — nub→`Mouse`, pad→`Keyboard`, gamepad name**, owned by
[Phase 8 — Input](../phases/08-input/index.md). Do not start before the mapping decision is recorded and the
platform layer fills the input structs.

## Planned scope

10 core samples:

- `core_input_gamepad` — also needs `GetGamepadName()` populated and the 800×450 pad art downscaled
  (1024×512 = 2 MiB does not fit; a 480×270 or 512×256 overlay does)
- `core_input_actions`
- `core_basic_screen_manager`
- `core_compute_hash`
- `core_custom_frame_control`
- `core_random_sequence`
- `core_scissor_test`
- `core_storage_values`
- `core_text_file_loading`
- `core_delta_time`

## Why now

This is the first slice after the input remap exists. It covers the gamepad, actions, screen-manager and file/UI
paths that the rest of the corpus uses, and it validates the mapping on samples whose behaviour is easy to see.

## Gate and first action

- Gate: `G1`, delivered by Phase 8.
- First action when the gate opens: build `core_input_actions` and confirm that a pad button edge reaches
  `IsGamepadButtonPressed()` exactly once; then record the mapping in Phase 8 and port the rest.
- Asset prep: downscale `ps3.png` and `xbox.png` (or draw a simpler overlay) as part of the gamepad task.

## Exit criteria (draft)

- [ ] every listed sample renders and its interaction is exercised on the device
- [ ] each `done` sample has a build marker, boot log, `DONE` line and two identical captures
- [ ] the `G1` mapping is recorded in Phase 8 and in this sprint
- [ ] findings written back to [Phase 13](../phases/13-samples-porting/index.md)
- [ ] no file under `examples/` modified

## Follow-ups

- Replace this placeholder with a full sprint from [the template](../templates/sprint.md) when scheduled.
- Do not renumber this sprint or reuse its ID.
