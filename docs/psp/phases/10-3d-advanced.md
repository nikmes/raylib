# Phase 10 — 3D and advanced

| | |
|---|---|
| State | not started |
| Depends on | Phase 6 (textures and VRAM under control) and phase 7 (2D paths settled) |
| Exit criterion | a 3D sample at target frame rate with no per-frame full GE wait |

## What this phase is about

Raylib's 3D path on the PSP is the part where the hardware's rules are least forgiving: float24 matrices, no
X/Y clipping, normalised s16 positions, a GE that skins in hardware, and a fixed-function pipeline that has to
be mapped deliberately rather than discovered. This phase also adds render-to-texture, which is where
read-after-write hazards become real.

## Carried in from phase 5 (sprint 014 decision)

Phase 5 deferred one criterion here rather than redefining it. In the `batch_check` `MESH_SCENE=1` build the
2D → `DrawCube` + `DrawMesh` → 2D scene is **not** byte-identical to the phase-4 archive. The evidence is narrow:

- `DrawMesh(GenMeshCube)` reaches the driver as `drawElements count=36 tex2d=0 fmt=0x1e3` and produces **no pixels**.
- Disabling `GL_CULL_FACE` for the draw changes nothing, so culling is ruled out.
- The converting vertex path (`PspGuVertex`, index rebasing) is the suspect; the immediate and array paths are
  byte-identical to pspGL on the 2D corpus.
- Everything else about 3D is proven: frustum, rotated view matrix, `sceGuSetMatrix` (including the `w` row),
  viewport/offset, perspective divide and immediate quads all match within a pixel, and **overlapping cubes are
  depth-correct with `PSP_GU_DEPTH=1`** (sprint 014 T8: the near-red cube drawn first holds the 6,360-pixel overlap;
  with depth off the same pixels are far-blue).

The rest of the starting point is measured, not assumed: the 60 fps ceiling is **2,375 sprites** on three
framebuffers with two rotating lists and 64-byte per-list vertex arenas, and a steady frame has **no full GE wait**
(only 18–26 µs targeted waits). See [sprint 014](../sprints/014-phase5-ceiling-vram-readback.md).

## What has to be true before starting

- [ ] The batcher and the texture/VRAM layer are stable and measured
- [ ] A 3D sample exists that exercises models, lighting and a textured mesh
- [ ] Camera-relative rendering is already the convention (not a later retrofit)

## Tasks

- [ ] **Carried from phase 5, first:** make `glDrawElements`/`DrawMesh` produce pixels — the converting path and
      index rebasing in `tools/pspgu/gl_draw.c`; verify against the predicted cube bbox x 193..287, y 92..189
- [ ] Convert model vertex data **once**, offline or at load, into GE-native formats; no per-frame conversion
- [ ] Index buffers where type and alignment allow; verify alignment costs are not paid at draw time
- [ ] Map fog, lighting and texture environment onto GE state; document explicitly what cannot be expressed
- [ ] Render-to-texture with explicit read-after-write tracking; synchronise only on a real dependency
- [ ] Static bounds and GE-side object culling for scenes with many objects
- [ ] Tessellate large triangles on the CPU where the GE would drop them at the window edge
- [ ] Optional: build display-list words or cull on the Media Engine — measured at 0 failures elsewhere, but a
      second CPU with its own cache protocol ([§10.2](../memory-map.md#102-media-engine-the-second-mips-core))

## What we already know

- Matrices keep ~16 significant bits: big world coordinates tremble while the camera moves
- No X/Y clipping after the viewport transform — a primitive with one vertex outside the 4096 window is dropped
  whole; only the near plane clips
- s16 positions are normalised (value/32768) in 3D transform mode — pair every s16 buffer with a scale matrix
- The GE skins vertices in hardware; CPU skinning is wasted work
- The ME has no VFPU, so any 3D maths moved there becomes scalar

## Exit criteria

- [ ] 3D sample holds the target frame rate, with frame-time percentiles
- [ ] no per-frame full GE wait; hazards synchronise individually
- [ ] vertex formats and their byte costs documented in the feature page
- [ ] what the GE cannot do is listed, with the fallback the engine takes

## Guidelines from the renderer blueprint

Source: `/home/psp/psp_sdk_660/AI_MEMORY_MAP.md` §16.5–16.8 (AI-written blueprint inside the Sony SDK reference
tree, **not** Sony documentation). Cross-checked against our own measurements; where they disagree, ours win.

**Vertex pipeline and formats:**

- Compact world format the blueprint recommends: `GU_TEXTURE_16BIT | GU_COLOR_5551 | GU_VERTEX_16BIT |
  GU_TRANSFORM_3D`, adding `GU_NORMAL_8BIT` if the GE lights. Interleaved, fixed field order.
- s16/s8 positions are normalised (value/32768 and value/128): quantise **about the mesh centre** and put
  `scale × 32768` plus the centre in the model matrix, or the scene collapses inside the near plane. This is the
  same trap the memory map already records, with the fix spelled out.
- Vertex colour is `0xAABBGGRR` — red in the low byte; bake it that way.
- Skin weights must sum **exactly** to 128 (u8) or 32768 (u16); the GE does not normalise them.

**Depth, clipping, passes:**

- Keep depth range `(65535, 0)` with `GU_GEQUAL` and make the near plane as large as the scene allows — that is
  the only precision lever, since depth is always 16-bit.
- For very large range, split: far geometry 0..32767, near geometry 32768..65535, one depth clear.
- `GU_CLIP_PLANES` stays on in transform mode; the far plane maps to Z=−1 and is clamped only with clipping.
- Decals: bias a few depth codes with `sceGuDepthOffset`, not with world-space offsets.
- Pass order: opaque (depth test+write, cull on) → cutout (alpha test, no blend, cull off) → translucent
  (SRC_ALPHA/ONE_MINUS_SRC_ALPHA, depth write off) → glass last.
- `GU_TFX_MODULATE` + `GU_TCC_RGBA` to keep baked lighting/AO; REPLACE flattens it.

**Lighting and skinning economics** (blueprint figures, worth knowing before designing):

| technique | effect |
|---|---|
| baked vertex colour | cheapest world lighting; a per-vertex CPU relight costs milliseconds |
| GE lighting (`sceGuAmbient` + 1 directional + up to 3 point) | moves relight off the CPU |
| GE skinning, one hot character | 7.2× faster than CPU skinning (3,080 → 428 µs) |
| GE skinning, crowd of 112 draws | 4.9 ms, **worse** than 2.0 ms of cached CPU-skinned vertices |

So GE skinning wins for a hero character and loses for crowds — animation LOD, not one policy.

**Large worlds and seams:**

- Matrices reach the GE as float24 (15 fraction bits): at |T| ≈ 3000 the translation re-rounds every frame and
  the world shakes. Camera-relative rendering (rotation-only view matrix, camera subtracted on the CPU from every
  model translation *and* from world-space vertices) is the fix.
- Seams between separately drawn meshes: shared per-frame origin plus a narrow skirt strip; requantising or
  widening coplanar polygons does not fix them.

**One place the blueprint is wrong for us, and it matters.** It states as a fixed constraint that "the GE has no
vertex shader or T&L; the CPU/VFPU transforms vertices, the GE rasterizes". That is too strong: the GE **does**
apply a 4×4 matrix per vertex (`GU_TRANSFORM_3D` + `sceGum*`, which is how our own samples render), which is
exactly why float24 matrix precision is a trap at all. What the GE lacks is programmable shading and a general
lighting model the CPU can leave to it. Do not move transforms to the CPU on the strength of that sentence —
measure, and keep camera-relative rendering for precision.

## What the four reference engines offer for 3D (ranked)

Same scan as phase 4: `PSPRuntime`, `3D-PSP-Tutorials`, `PGE`, `openTRI` (`/home/psp/ref/`), memory maps verified
against source. These are the items that matter once the 2D submission wall is gone — none of them help the
measured 58.3 µs/sprite.

| # | lever | mechanism | anchors |
|---|---|---|---|
| 1 | **CPU-side matrix maths in VFPU registers** (`vmmul.q`, `vqmul.q`, `vmov.q`) | one 4×4 multiply for N vertices instead of N; keep the loop in registers from load to store | `openTRI/triVMath_vfpu.c` (but see the erratum below) |
| 2 | **Double-buffered dynamic vertex arrays** | required by the `GU_SEND` submission model: the list borrows pointers, so a buffer may not be rewritten while the GE reads it | `openTRI/triParticle.c:162-169,760`; `triGraphics.c:396-409` |
| 3 | **GE skinning for a hero character, cached CPU skinning for crowds** | 7.2× on one hot instance, loses on 112 draws (4.9 vs 2.0 ms) — animation LOD, not one policy | blueprint §16.6, consistent with our own findings |
| 4 | **VRAM-resident, swizzled atlases for 3D materials** | second-order for CPU cost, first-order once GE texture fetch becomes visible | `openTRI/triTexman.c:406-440`, `triImage.c:256-317` |
| 5 | **Render-to-texture with an explicit target size** | see the bug below; the pattern is a separate target and a copy, not a resize in place | `openTRI/tri3d.c` |
| 6 | **`GU_SEND` dual lists** | ranked last even here: costs a frame of latency and 2× list memory, and buys nothing while the CPU is the constraint | `openTRI/triGraphics.c:396-409` |

### Do not copy — 3D-specific hazards found

- `openTRI/triVMath_vfpu.c:2008` — `vcrsp.t c010, c010, c020` writes the cross product into a source register: a
  restricted VFPU form, rejected by the assembler and undefined on hardware. `openTRI`'s memory map suggests
  "guarding for binutils" instead; that is wrong, the code is wrong.
- `openTRI/tri3d.c:386` — allocates `(ysteps*(xsteps+1)+(ysteps-1))*2` vertices then calls `sceGuDrawArray(..., 2, ...)`:
  a hardcoded count, so the whole grid renders as one strip.
- `openTRI/tri3d.c:155` vs `:185` — render target allocated as `twidth * height` but declared to the GE as
  `twidth × theight`; any non-power-of-two height makes sampling read past the allocation into whatever VRAM follows.
- `openTRI/triImage.c:291,371` — detects RAM-vs-VRAM by comparing an address against `0x04200000`, and
  `triDrawImage` never writes back a palette edited in RAM before `sceGuClutLoad` (`triFont.c:1037` does it
  correctly, so the rule was known).
- `openTRI/triTexman.c:470-495` — returns sizes for `GU_PSM_DXT1/3/5`. Those enum values exist in `pspgu.h` but the
  GE has no S3TC decoder: anything built on that branch is dead weight.
- `3D-PSP-Tutorials/LoadingScreen.cpp:32-78` — a second thread submitting GU lists and swapping buffers while the
  main thread blocks on the same display, then `sceKernelTerminateDeleteThread`.

## Sprints and features

None yet. Use the [sprint template](../templates/sprint.md).
