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

## Sprints and features

None yet. Use the [sprint template](../templates/sprint.md).
