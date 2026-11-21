# Sprint 031 (P13-S8) — Oversize assets

| | |
|---|---|
| Roadmap phase | [Phase 13 — Samples porting](../phases/13-samples-porting/index.md) |
| Sprint | P13-S8 |
| Branch / base commit | set at open |
| Started / closed | — / — |
| Build number(s) | — |
| Tasks | 0/0 — placeholder |

## Status

**Placeholder — not started.** Gated by **`G3` — downscale or slice assets beyond the 1024 buffer or over
budget**, owned by [Phase 7 — 2D specialisation](../phases/07-2d-specialisation/index.md). The 512-window split
itself already exists (sprint 020); this sprint handles the assets that still do not fit.

## Planned scope

2 texture samples plus asset prep:

- `textures_sprite_stacking` — `booth.png` 112×11468 pads to 128×16384, over the 1024 buffer; downscale or split
- `textures_image_generation` — nine full-screen images and nine textures; needs an explicit heap and a
  one-at-a-time rewrite, or is `dropped` as no longer the upstream sample

Asset prep for later sprints:

- `explosion.png` 1024×1024 = 4 MiB for `textures_sprite_explosion` (P13-S11) — downscale to 512×512 or smaller
  and record the visual delta
- the pad art (`ps3.png`, `xbox.png`, 800×450 → 1024×512 = 2 MiB) for `core_input_gamepad` (P13-S3) — downscale
  to a 480×270 or 512×256 overlay

## Why now

These are the only remaining texture samples that cannot be fixed by a build flag. The sprint decides each one
explicitly — ported with a documented downscale, or dropped — so the phase close has no vague entries.

## Gate and first action

- Gate: `G3`, from Phase 7's asset work.
- First action when the gate opens: downscale `booth.png` to a sheet that pads to ≤1024 in both dimensions and
  record the transformation. Do the same for `explosion.png` before P13-S11 needs it.
- `textures_image_generation`: try one image at a time with `PSP_HEAP_SIZE_KB(-256)`; if it still needs a rewrite,
  mark it `dropped` with that reason.

## Exit criteria (draft)

- [ ] every listed sample is either ported with a documented asset transform or `dropped` with a reason
- [ ] every downscale has a before/after differing-pixel count and the new padded bytes
- [ ] the pad art and explosion are ready for P13-S3 and P13-S11, or those dependencies are re-planned
- [ ] each `done` sample has a build marker, boot log, `DONE` line and two identical captures
- [ ] findings written back to [Phase 13](../phases/13-samples-porting/index.md)
- [ ] no file under `examples/` modified

## Follow-ups

- Replace this placeholder with a full sprint from [the template](../templates/sprint.md) when scheduled.
- Do not renumber this sprint or reuse its ID.
