# Sprint 034 (P13-S11) — Remaining features

| | |
|---|---|
| Roadmap phase | [Phase 13 — Samples porting](../phases/13-samples-porting/index.md) |
| Sprint | P13-S11 |
| Branch / base commit | set at open |
| Started / closed | — / — |
| Build number(s) | — |
| Tasks | 0/0 — placeholder |

## Status

**Placeholder — not started.** Gated by **`G6` clipboard and `G7` texture readback**, plus `G1` (mouse) and
`G3` (asset downscale) for the two texture samples. Owned by [Phase 8 — Input](../phases/08-input/index.md),
[Phase 6 — Textures and VRAM](../phases/06-textures-vram/index.md) and
[Phase 7 — 2D specialisation](../phases/07-2d-specialisation/index.md). It waits for P13-S1–S10 and P13-S12 to
close; the phase close itself is P13-S13.

## Planned scope

5 samples:

- `textures_sprite_button` — mouse input (`G1`)
- `textures_sprite_explosion` — mouse (`G1`) plus a 4 MiB `explosion.png` asset from P13-S8 (`G3`)
- `core_clipboard_text` — clipboard (`G6`)
- `textures_clipboard_image` — clipboard (`G6`)
- `textures_to_image` — texture readback (`G7`)

12 drops, each with a one-line reason in Phase 13:

- Touch-only: `core_input_gestures`, `core_input_gestures_testbed`, `core_input_multitouch`,
  `core_input_virtual_controls`
- Desktop-only: `core_highdpi_demo`, `core_highdpi_testbed`, `core_monitor_detector`, `core_window_flags`,
  `core_window_web`, `core_drop_files`, `core_window_should_close`, `core_automation_events`

## Why now

This sprint handles the last platform features and the drop list. It must not start until the earlier waves have
produced their evidence. The phase close and the 227-sample reconciliation are P13-S18, not this sprint.

## Gate and first action

- Gates: `G1`, `G3`, `G6`, `G7`; plus P13-S1–S10 and P13-S12 closed.
- First action when the gates open: `textures_sprite_button` for mouse input and `textures_to_image` for
  readback — they are the smallest consumers of their feature. Then mark the drops.
- Hand-off: list every sample this sprint leaves untouched, so P13-S13's audit has no gaps.

## Exit criteria (draft)

- [ ] every listed sample renders with its feature, or is `dropped` with the feature named
- [ ] all 12 drops carry a reason in Phase 13
- [ ] the five samples and twelve drops are recorded in Phase 13
- [ ] findings written back to the memory map and the gate-owning phase pages
- [ ] no file under `examples/` modified

## Follow-ups

- Replace this placeholder with a full sprint from [the template](../templates/sprint.md) when scheduled.
- The phase close moved to the future P13-S18 (all-corpus audit); P13-S13 is the AudioStream batch.
- Do not renumber this sprint or reuse its ID.
