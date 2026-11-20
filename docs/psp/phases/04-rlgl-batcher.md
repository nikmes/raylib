# Phase 4 — PSP fixed-function rlgl batcher

| | |
|---|---|
| State | not started |
| Depends on | Phase 3 (know the cost of the library), Phase 2 (counters) |
| Exit criterion | a run of N same-texture sprites produces ~`ceil(N/capacity)` submissions, and screenshots are identical to the unbatched path |

## What this phase is about

Collapsing per-object submission while still rendering through OpenGL. Today `DrawTexturePro` binds a texture,
issues one immediate quad and unbinds — per sprite, per glyph, per shape. Under `GRAPHICS_API_OPENGL_11`,
`rlBegin` *is* `glBegin` and `rlEnd` *is* `glEnd`, so every one of those is a separate submission to pspGL.

The structural target, proven by a hand-written GU renderer: **1024 sprites per draw call** for axis-aligned
sprites, 512 for quads.

## What has to be true before starting

- [ ] Phase 3 decision recorded (which pspGL we are batching against)
- [ ] Phase 2 counters can distinguish logical draws from backend submissions
- [ ] A reference screenshot set from phase 1 to compare against

## Tasks

- [ ] Add a batch selected by `PLATFORM_PSP && GRAPHICS_API_OPENGL_11`; leave the portable GL11 path intact
- [ ] Vertex layouts: textured 24 B, colour-only 16 B, **no normals for 2D**
- [ ] Flush only on: texture object, primitive mode/layout, blend, scissor, render target, depth state,
      matrix state when vertices are not pre-transformed, capacity, `EndDrawing`
- [ ] Turn `rlSetTexture()` into a state transition: bind at submission, never unbind per sprite
- [ ] Use `GL_SPRITES_PSP` only for axis-aligned, unrotated, unflipped sprites; 4/6 vertices otherwise
- [ ] Allocate batch storage once; zero heap allocations per sprite or glyph
- [ ] Preserve draw order exactly — merge consecutive compatible draws, never sort transparent ones
- [ ] Track and report flush reasons; a flush with no state change is a bug

## What we already know

- `rlBegin`/`rlEnd` are `glBegin`/`glEnd` under GL11 (`rlgl.h` ~1429–1440); `rlSetTexture(0)` is
  `rlDisableTexture()` (~1638)
- 16-bit texture coordinates are **unsigned**: UV flipping and negative source rectangles must not silently
  wrap — the 5.5 port already disables the shapes quads path for PSP for this class of reason
- raylib's 2D order is part of the contract; any batching that reorders breaks alpha blending
- The reference engine's flush cost was ~96 KiB of display list per flush; ours must be sized in bytes and
  vertices, not "quads"

## Exit criteria

- [ ] submission count collapses from object count to state-run/capacity count (numbers, not impressions)
- [ ] screenshots byte-identical where they should be, and diffs explained where not
- [ ] no per-sprite heap allocation measurable
- [ ] frame time improved at the high bunny counts, or the reason it did not is recorded

## Sprints and features

None yet. Use the [sprint template](../templates/sprint.md).
