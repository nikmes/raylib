# Phase 10 — 3D and advanced

| | |
|---|---|
| State | **ready to plan** — dependencies 6 and 7 are done; two blockers found in the readiness review |
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
(only 18–26 µs targeted waits). See [sprint 014](../../sprints/014-phase5-ceiling-vram-readback.md).

## What has to be true before starting

- [x] The batcher and the texture/VRAM layer are stable and measured
- [ ] A 3D sample exists that exercises models, lighting and a textured mesh — **blocker: no PSP 3D test scene
      exists yet**
- [ ] Camera-relative rendering is already the convention (not a later retrofit) — **blocker: not established**

## Readiness review (2026-11-22)

Phase 10 can start: Phases 6 and 7 are closed, so the texture/VRAM layer and the 2D paths are stable. The review
found two blockers that decide the first sprint, plus three scope gaps.

**Blocker 1 — `DrawMesh`/`glDrawElements` does not produce pixels, and the cause is visible in the code.** The
carried phase-5 evidence is a draw of 36 indices with `tex2d=0` and no pixels. `glDrawElements` converts vertices
into a fixed `PspGuVertex` record laid out as `{u,v, color, nx,ny,nz, x,y,z}` (36 B; the comment says 32), but
`guVertexFormat()` packs only the arrays the client enabled. `DrawMesh` has no texture array and does have
normals, so the format omits `GU_TEXTURE_32BITF`; GU then reads the colour at offset 0 (the u/v pair), the normal
where the colour is, and the position at the normal — garbage, and often no pixels. The immediate cube works
because it uses the batched 24-byte layout, which matches its format. **First task:** pack the converting path per
format (or use format-specific structs), and prove it with `MESH_SCENE=1` against the predicted cube bbox
x 193..287, y 92..189.

**Blocker 2 — render-to-texture has no backend.** `tools/pspgu/` implements no `glGenFramebuffers`,
`glBindFramebuffer`, `glFramebufferTexture2D` or readback; on GL11 `rlLoadFramebuffer()` returns 0
(`src/rlgl.h:4163`). `LoadRenderTexture`/`BeginTextureMode` therefore cannot work, and an app that uses them may
not even link. The phase needs a design decision before implementation: implement the GL FBO entry points over
pspgu's VRAM (sampling a render target, explicit read-after-write), or add a smaller PSP-specific target API. A
256×256 8888 target is 256 KiB and a 512×256 target 512 KiB; both fit the ~913 KiB tail but compete with textures
and the 3D depth buffer. Working implementations do exist and all take the same route, not GL FBOs — see
[Render-to-texture: what the references actually do](#render-to-texture-what-the-references-actually-do).

**Scope gaps:**

- **No 3D corpus.** There is no `psp-tests/scene_3d` or device asset that exercises a textured mesh, lighting and
  a model; `examples/models` are desktop assets not wired into the PSP harness. The first sprint should build a
  deterministic cube/model scene and a marker/capture path, as sprint 018 did for 2D.
- **Camera-relative rendering is not the convention.** The model/view path still uses world-space translations.
  The blueprint's float24 precision trap (`|T| ≈ 3000`) is real; establish the convention before converting model
  data, or every model path will need a retrofit.
- **The exit target is undefined.** "Target frame rate" needs a number and a workload: for example, 60 fps for a
  textured cube plus N instanced meshes at 480×272, or 30 fps for one skinned model, with p95 frame time.

**Proposed sprint order** (sized like Phase 7, five to six sprints):

1. **P10-S1 — DrawMesh fix and 3D mesh gate**: format-specific packing, `MESH_SCENE` parity, minimal cube/model
   probe, the 3D corpus skeleton.
2. **P10-S2 — Camera-relative and GE-native model data**: convert once at load, indexed buffers, matrix precision,
   measure a textured model.
3. **P10-S3 — Lighting, fog and texture environment**: map GE state, document what cannot be expressed, bake what
   must stay in vertex colours.
4. **P10-S4 — Render-to-texture**: FBO-or-PSP-target decision, VRAM budget, read-after-write synchronization.
5. **P10-S5 — Culling, triangle tessellation and the ME option**: static bounds, window-edge tessellation, the ME
   measured only against the CPU path.
6. **P10-S6 — 3D sample target, regression and close**, or fold into P10-S5 if the sample already meets the target.

Do not start with models or lighting: the converting path must first draw a cube, and RTT needs a backend
decision. The existing depth/matrix evidence (sprint 014 T8, sprint 013) is sound and is not re-opened.

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
      second CPU with its own cache protocol ([§10.2](../../memory-map.md#102-media-engine-the-second-mips-core))

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
| 5 | **Render-to-texture with an explicit target size** | see the bug below; the pattern is a separate target and a copy, not a resize in place | `openTRI/tri3d.c:133-200`; `PGE/pgeGfx.c:451-490`; SDK `sample/graphics/scene` |
| 6 | **`GU_SEND` dual lists** | ranked last even here: costs a frame of latency and 2× list memory, and buys nothing while the CPU is the constraint | `openTRI/triGraphics.c:396-409` |

### Render-to-texture: what the references actually do

Every working implementation in the reference set uses the same primitive: **point the GU draw buffer at a VRAM
block with `sceGuDrawBufferList`, render, restore the screen buffer, then bind the block as a texture**. None uses
a GL-style framebuffer object; on this hardware the draw buffer *is* the render target. The libgu manual gives the
contract: `fbp` must be 8192-byte aligned and `fbw` a multiple of 64 between 64 and 1024; changing the draw buffer
does not change the display buffer; and memory the GE has written may only be re-read as a texture after
`sceGuTexFlush()` **and** `sceGuTexSync()`
`[source scan: psp_sdk_660/usr/local/psp/devkit/document/html/graphics/libgu-Reference-English.htm:8509-8605, :10635-10749]`.

| Source | Mechanism | Worth taking | Not worth taking |
|---|---|---|---|
| libgu 6.60 manual | `sceGuDrawBufferList(fpf, fbp, fbw)` for mid-list buffer changes; `sceGuCopyImage` note: `TexFlush` then `TexSync` before sampling | the alignment/width rules and the cache contract | — |
| SDK `sample/graphics/scene` | renders a camera effect into a 256×256 5551 aux buffer placed after three 272 KiB frame buffers (`main.cpp:47-50`), redirects (`SceneGraph/Context.cpp:211-215`), restores screen and viewport (`:228-231`), binds with `TexMode`/`TexImage`/`TexFlush`/`TexSync` (`:232-236`), then composites a full-screen quad | the complete manual-conformant order: redirect → draw → restore → flush → sync → sample; 128 KiB target | fixed aux size; no dependency tracking — it flushes and syncs unconditionally on every camera switch |
| PGE | `pgeGfxRenderToTexture` (`pgeGfx.c:451-469`) requires a VRAM texture, calls `sceGuSync(0, 2)` and `sceKernelDcacheWritebackAll()`, redirects, sets offset/viewport/scissor; `pgeGfxRenderToScreen` (`:471-490`) restores; sample `samples/graphics/rendertotexture/rendertotexture.c:30-55` uses a 128×128 8888 target | save/restore as a pair; validate that the target is VRAM | a whole-D-cache writeback and a GE sync per switch, and `pgeTextureActivate` (`pgeTexture.c:670-696`) never issues `TexFlush`/`TexSync` — it works because the sample's target is power-of-two and the sync is global, not because the contract is met |
| openTRI | `triRendertotexture` (`triGraphics.c:604-632`) validates PSM and VRAM range then redirects; `triRendertoimage` (`:638-668`) and `triRendertoscreen` (`:671-686`) restore. 3D: `tri3dRenderbufferCreate` (`tri3d.c:133-159`), `SetRendertarget` (`:161-174`), `SetTexture` (`:176-190`) with `TexScale(logical/padded)`; call sites `sceGuTexSync()` (`tri3d.c:327` etc.) | `TexScale` for a padded target; the PSM whitelist; an explicit restore path | no sync at all before the redirect; `TexSync` without `TexFlush`; allocates `twidth*height` (`:158`) but declares `twidth*theight` (`:185`) — non-power-of-two heights over-read (already listed under hazards) |
| daedalus | no render-to-texture path found: `sceGuDrawBuffer` appears only at context setup and resolution changes (`GraphicsContextPSP.cpp:787,862,912`); `sceGuCopyImage` (`:317-319`) is an interlaced display blit; its framebuffer effects are HLE (`Ucode_FB.h:24,65,96`) | confirmation that even a full emulator does not reach for GL FBOs here | does not supply an example; a search of `Source/SysPSP` for `sceGuDrawBufferList` and "render target" returned no RTT use |

**What this decides for pspgu.** The backend needs a draw-target redirect, not FBO semantics implemented inside
the GE. Whether the public API is `glGenFramebuffers`-style or a smaller PSP-specific one, it must emit
`sceGuDrawBufferList` plus offset/viewport/scissor and restore the screen state afterwards. Three requirements
follow from the sources:

- **Synchronise read-after-write explicitly, and only then.** The manual's contract is `TexFlush` + `TexSync`
  after the GE rendered into the target. PGE's whole-cache writeback and per-switch GE sync would violate phase
  5's "no full GE wait" rule; openTRI's unconditional `TexSync` at every binding is closer but still too coarse
  `[source scan: PGE pgeGfx.c:451-490; openTRI tri3d.c:327,507,530,554,584]`.
- **Allocate and declare the same padded extent.** Pad width *and* height to the declared texture size and sample
  with `TexScale = logical / padded`, as `tri3dRenderbufferSetTexture` does. A render target is sampled through
  the same 512-pixel band path measured in sprint 020, so the window rule applies; sprint 023's strip table is a
  measured null, so do not expect texture-cache tricks to rescue a poorly sized target.
- **Budget the target as VRAM, not as a free block.** A 256×256 8888 target is 256 KiB; the SDK's 256×256 5551
  aux buffer is 128 KiB; PGE's sample target is 64 KiB. Targets compete with the texture tail and, for 3D, the
  depth buffer. Read the boot allocation counters; the texture allocator is a bump allocator with no free list.

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

None yet. Use the [sprint template](../../templates/sprint.md).
