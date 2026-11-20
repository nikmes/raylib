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

## What has to be true before starting

- [ ] The batcher and the texture/VRAM layer are stable and measured
- [ ] A 3D sample exists that exercises models, lighting and a textured mesh
- [ ] Camera-relative rendering is already the convention (not a later retrofit)

## Tasks

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

## Sprints and features

None yet. Use the [sprint template](../templates/sprint.md).
