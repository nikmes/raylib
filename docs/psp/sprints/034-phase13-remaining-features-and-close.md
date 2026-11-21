# Sprint 034 (P13-S11) — Remaining features and phase close

| | |
|---|---|
| Roadmap phase | [Phase 13 — Samples porting](../phases/13-samples-porting/index.md) |
| Sprint | P13-S11 |
| Branch / base commit | set at open |
| Started / closed | — / — |
| Build number(s) | — |
| Tasks | 0/0 — placeholder |

## Status

**Placeholder — closing sprint, not started.** Gated by **`G5` audio, `G6` clipboard and `G7` texture readback**,
owned by [Phase 9 — Audio](../phases/09-audio/index.md),
[Phase 8 — Input](../phases/08-input/index.md) and
[Phase 6 — Textures and VRAM](../phases/06-textures-vram/index.md). It also waits for P13-S1–S10 to close.

## Planned scope

5 samples:

- `textures_sprite_button` — audio (`G5`)
- `textures_sprite_explosion` — audio (`G5`) plus a 4 MiB `explosion.png` asset from P13-S8
- `core_clipboard_text` — clipboard (`G6`)
- `textures_clipboard_image` — clipboard (`G6`)
- `textures_to_image` — texture readback (`G7`)

12 drops, each with a one-line reason in Phase 13:

- Touch-only: `core_input_gestures`, `core_input_gestures_testbed`, `core_input_multitouch`,
  `core_input_virtual_controls`
- Desktop-only: `core_highdpi_demo`, `core_highdpi_testbed`, `core_monitor_detector`, `core_window_flags`,
  `core_window_web`, `core_drop_files`, `core_window_should_close`, `core_automation_events`

Phase close:

- Fill the Phase 13 exit table: every sample either ported with evidence or `dropped` with a reason.
- Reconcile the tier counts against the device runs; no sample may appear in two tiers.
- Record which capability gates were actually delivered and which were deferred.

## Why now

This is the only sprint that can close the phase: it handles the last platform features, the drop list and the
exit audit. It must not start until the earlier waves have produced their evidence, or the close table will be
guesswork.

## Gate and first action

- Gates: `G5`, `G6`, `G7`; plus P13-S1–S10 closed.
- First action when the gates open: `textures_sprite_button` for audio and `textures_to_image` for readback — they
  are the smallest consumers of their feature. Then mark the drops.
- Close action: run the audit greps in Phase 13 and memory-map §13; every `todo`/`blocked` must become `done` or
  `dropped` with a reason.

## Exit criteria (draft)

- [ ] every listed sample renders with its feature, or is `dropped` with the feature named
- [ ] all 12 drops carry a reason in Phase 13
- [ ] the Phase 13 exit table is complete and matches the device runs
- [ ] no sample appears in two tiers; counts sum to 127
- [ ] the phase page state is updated to `done` (or the remaining gaps are explicit and owned)
- [ ] findings written back to the memory map and the gate-owning phase pages
- [ ] no file under `examples/` modified

## Follow-ups

- Replace this placeholder with a full sprint from [the template](../templates/sprint.md) when scheduled.
- Do not renumber this sprint or reuse its ID.
