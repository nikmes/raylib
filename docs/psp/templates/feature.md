# Feature template

> Copy to `docs/features/<slug>.md`. A feature is a unit of behaviour with a defined API surface, a cost
> model, and a degradation story for what the hardware cannot do.

## 1. What the application gets

The raylib API surface this adds or changes: functions, structs, and the behaviour a game can observe.
Name them exactly as they appear in `raylib.h`.

## 2. Why it matters

Which roadmap phase it serves, and which sample or game loop exercises it. If nothing exercises it, say so
and question whether it belongs in this phase.

## 3. Behaviour specification

- Inputs, outputs, and edge cases (empty input, zero-size texture, negative rectangles, NaN).
- Draw order and blending semantics that must be preserved **exactly** — 2D order is part of the contract.
- What happens when the GE or pspGL cannot express it: degrade visibly and document it, never crash, never
  silently draw nothing.
- State the feature is allowed to change, and the state it must leave untouched.

## 4. Platform mapping

How the feature lands on the current backend (pspGL today, `GRAPHICS_API_PSP_GU` later):

| concern | mapping |
|---|---|
| primitive(s) emitted | e.g. `GU_SPRITES`, 2 vertices |
| state it touches | texture, blend, scissor, depth |
| when it flushes | texture change / mode change / capacity only |
| matrix path | pre-transformed on CPU, or through `sceGum` |

## 5. Cost model

Expected cost, before implementing — the implementation is then judged against it.

| cost | expected | how verified |
|---|---|---|
| draw submissions | `ceil(N/capacity)` | backend counter |
| vertices submitted | | backend counter |
| bytes per frame | | computed |
| VRAM | | boot-time accounting |
| CPU per call | | zone timing |

## 6. Portability rules

- Desktop behaviour must not change. Guard with `PLATFORM_PSP` or `GRAPHICS_API_*`, never with a runtime flag
  that leaks into other platforms.
- If a new hook is needed, it goes into the platform interface / `rlgl` backend — not into `rcore.c` business
  logic.
- If the change is upstream-worthy, keep it in a separate commit from anything PSP-only and experimental.

## 7. Test plan

- Sample(s) used, and why they exercise this feature.
- Exact sequence: buttons to press, how long to wait, what not to touch.
- What to look at on screen, and which screenshot to keep as the reference.
- What a failure looks like, so a wrong result is not mistaken for a slow one.

## 8. Reference notes and licensing

What external source informed the design, and what was **reimplemented** versus **copied**. GPL and
unlicensed sources are ideas only; if code was adapted, name the file, the author and the licence.

## 9. Done criteria

- [ ] behaviour matches §3, including the degradation case
- [ ] cost within the §5 model, or the model corrected with a measurement
- [ ] sample output byte-identical to the reference where it should be
- [ ] added to the roadmap's phase exit criteria if it changes them
