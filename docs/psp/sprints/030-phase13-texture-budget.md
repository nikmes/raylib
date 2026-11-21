# Sprint 030 (P13-S7) — Texture budget

| | |
|---|---|
| Roadmap phase | [Phase 13 — Samples porting](../phases/13-samples-porting/index.md) |
| Sprint | P13-S7 |
| Branch / base commit | set at open |
| Started / closed | — / — |
| Build number(s) | — |
| Tasks | 0/0 — placeholder |

## Status

**Placeholder — not started.** Gated by **`G2` — depth-off build and/or `PSP_GU_TEXFMT=auto`, plus an explicit
heap**, owned by [Phase 6 — Textures and VRAM](../phases/06-textures-vram/index.md) and
[Phase 11 — General optimisation](../phases/11-general-optimisation.md).

## Planned scope

9 texture samples whose padded texture is 1 MiB and does not fit the 982,528 B 8888/depth budget:

- `textures_image_channel` — `fudesumi.png` 384×512
- `textures_image_drawing` — `cat.png`, `parrots.png`, `custom_jupiter_crash.png` (three textures)
- `textures_image_kernel` — `cat.png`
- `textures_image_processing` — `parrots.png`
- `textures_image_text` — `parrots.png` plus a TTF
- `textures_polygon_drawing` — `cat.png`
- `textures_raw_data` — `fudesumi.raw`
- `textures_background_scrolling` — foreground 704×192 → 1024×256 = 1 MiB
- `textures_blend_modes` — foreground 704×192 → 1 MiB

## Why now

These are the clearest case where the code path is already correct and the asset budget is the only blocker. The
sprint measures the three levers already available — depth off (+278,528 B), `PSP_GU_TEXFMT=auto` (halves opaque
RGB), and a larger heap — and records the visual delta rather than hiding it.

## Gate and first action

- Gate: `G2`, from Phase 6/11.
- First action when the gate opens: build the driver with `PSP_GU_DEPTH=0` and run `textures_image_channel`;
  record the boot VRAM line and the padded texture line. Then try `PSP_GU_TEXFMT=auto` on the same sample.
- `textures_image_drawing` holds three 1 MiB textures at once; it needs either a downscale or `auto` on the RGB
  sources. Record the combination that works.

## Exit criteria (draft)

- [ ] each sample renders under a recorded build configuration (depth, PSM, heap)
- [ ] the padded size and PSM of every texture are in the evidence row
- [ ] every downscale has a before/after differing-pixel count
- [ ] each `done` sample has a build marker, boot log, `DONE` line and two identical captures
- [ ] configurations that still do not fit are recorded with the bytes that are missing
- [ ] findings written back to [Phase 13](../phases/13-samples-porting/index.md)
- [ ] no file under `examples/` modified

## Follow-ups

- Replace this placeholder with a full sprint from [the template](../templates/sprint.md) when scheduled.
- Do not renumber this sprint or reuse its ID.
