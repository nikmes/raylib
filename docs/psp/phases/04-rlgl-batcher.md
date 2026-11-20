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

## Guidelines from the renderer blueprint

Source: `/home/psp/psp_sdk_660/AI_MEMORY_MAP.md` §16 (an AI-written blueprint that lives inside the Sony SDK
reference tree — it is **not** Sony documentation). Cross-checked against our own measurements below; where it
disagrees, our numbers win.

**What our toolchain actually offers** (headers checked, see [§14](../memory-map.md#14-what-pspdev-actually-provides-verified-2026-11-20)):
`GU_SPRITES 6`, `GU_TRANSFORM_2D` ("passed directly to the rasterizer"), `sceGuScissor(x, y, w, h)`,
`sceGuTexImage/TexMode/TexFlush/TexSync/ClutLoad`, `sceGuDepthRange/DepthOffset`, and `sceGumDrawArray` for the
CPU-transform path. `libpspgum.a` is the default GUM; `libpspgum_vfpu.a` is opt-in and used by no sample.

**What the blueprint says a 2D path must do:**

| rule | why it matters | how it lands in rlgl |
|---|---|---|
| Use through mode (`GU_TRANSFORM_2D`) for sprites/UI | no transform, lighting, fog or clipping; coordinates are screen pixels | the batcher's layout should be screen-space, no matrix per sprite |
| `GU_SPRITES` = 2 vertices per axis-aligned rect | 1/3 the vertices of two triangles; colour/depth from the second vertex | matches the 1024-sprites-per-call figure we measured in `PSPRuntime` |
| **In through mode UVs are texel coordinates, not 0..1**; `TexScale`/`TexOffset` are ignored | a 0..1 UV would sample inside the first texel | the batcher must convert, and this is where the unsigned-16-bit TEXCOORD trap bites |
| One texture per batch; sort pass → texture/CLUT → blend; rebind only on change | binds and CLUT loads cost GE time; draw-call count matters much less than state | phase 4's flush rule, already planned |
| Scissor takes width/height, not an end coordinate | off-by-one clips a row | `ceGuScissor(x, y, w, h)` |
| Draw wide images as vertical strips of 32–64 px | a 480-wide sprite thrashes the 8 KB texture cache | optional sprite slicing, phase 7 |
| NEAREST for 1:1 copies, LINEAR only when scaling | filtering costs texels | default filtering choice |
| Disable `GU_CULL_FACE` for 2D and restore it after | culling still applies in through mode | state reset per list |
| `GU_DITHER` off around additive glows | dither noise over alpha | state detail |
| T4/T8 + CLUT for UI and glyphs; 16-bit for art; swizzle | 1/4 to 1/2 the bytes; swizzling turns eight 16-byte reads into one 128-byte line | phase 6 |

**Frame metric the blueprint insists on** — and it is the formal version of what sprint 004 struggled with:

```text
work = frame_time - vblank_wait
```

At 60 Hz, 37 ms and 45 ms of work both present as a 50 ms frame, so a change that "did not move the frame time"
may have moved work inside the vsync bucket. We measured this the hard way: see [baselines](../baselines.md) —
the 100-bunny row is pacing, not cost. **Measure `work`, not frame time**, and remember sprint 005's finding that
the wait is not in `eglSwapBuffers` but in command-list reuse.

**Performance levers, ordered by the blueprint's expected payoff:** 16-bit framebuffer first (halves framebuffer
traffic; alpha needs 5551/4444, not 5650), then back-face culling, then draw distance/LOD, then front-to-back
opaque order, then mipmaps, then texture/CLUT sort with a bind cache. Dynamic resolution is gated on GE time
exceeding 12–15 ms.

**Anti-patterns it names, all of which apply to what we are replacing:** 8888 framebuffers, single buffering,
CPU clears, per-frame texture uploads, per-draw state changes, a VRAM texture cache without double buffering or
sync, one shared static vertex buffer for all instances, GE state left set between passes, and treating PPSSPP
timings as hardware cost.

## Acceptance target

The blueprint's phase-1 proof ("1,000 sprites at 60 fps, one bind per batch, pixel-exact 1:1 blit") is a better
acceptance criterion than "faster than before", because it is a number we already measure.

| | sprites in 16.7 ms | 1,000 sprites take |
|---|---|---|
| today (unbatched, adopted pspGL) | ~287 | **61.63 ms** |
| target | **≥1,000** | **≤16.7 ms** |

So phase 4 has to buy **3.7×** on this workload to hit 60 fps at 1,000 sprites. That is the number to design to,
and it is in the sprint's exit criteria rather than in a wish list.

## Sprints and features

None yet. Use the [sprint template](../templates/sprint.md).
