# Phase 10 — 3D and advanced

| | |
|---|---|
| State | **closed** — P10-S7 re-tested the two criteria: the frozen sample is at 60 fps with no full GE wait. The lit "parity" claim is withdrawn with a corrected 062 note. P10-S8 closed the camera/matrix question (cache 13x, GUM dropped; the cull is the next lever). P10-S9 (076) **closed** the deferred framebuffer-rotation defect: candidate (b), 8-frame RTT 0 diff, sample 16.683/16.684 ms, pspgu `b31`. P10-S10 measured the post-audit optimisations: the VFPU path had never been active (weak symbol), dedup and compact formats dropped with numbers, the VFPU is now opt-in. P10-S11 (078) adopted merged clean runs: the N=500 cube scene 43.22 → 26.67 ms (20 → 30 fps) and the 60 fps ceiling 150 → 270 instances, all gates 0 diff; P10-S12 (079) closed: T3 VFPU sincos adopted (4.703→4.651 ms, 0 diff); T1/T2/T4/T5 dropped. **P10-S13 (080)** closed 2026-11-24: lighting/fog scoped to `BeginMode3D`/`EndMode3D`, colour gate pspgu merge 0/1 + pspGL, `scene_3d LIGHT=ge` 0 diff, scrshot tool/layout, showcase workarounds removed; b37. **P10-S15 (085)** re-opened the phase on 2026-11-23 for a measurement-only hardware optimisation sweep (closed 2026-11-24: T1/T3/T7 done, T2/T4/T5b/T6/T6a dropped, no default change; pair 33.548/33.803 ms). **P10-S16 (086)** re-opened the phase on 2026-11-24 for the one CPU-side lever 085 dropped for an engineering reason: list-local vertices (folded into P10-S17). **P10-S17 (087)** now carries all three surviving candidates — list-local vertices, index cache, baked geometry — 0/15 active, one pair per build. **P10-S18 (088)** compares our two-list pipelined submission against the single-list full-sync model used by other PSP projects — done, dual model kept |. **P10-S19 (089)** measured it: the arena stays. Upstream raylib 2026-11-25 was **merged on 2026-11-28** (`dd3d608a`) after its gates passed (613 / 284 / 2D identical); the 400-cube pair now reads 40.4 ms at the vblank step with the same 1-3 % work change — see the A/B recipe, landmine 7. **P10-S20..S24 (090-094)** carry the resulting [redesign](pspgu-redesign.md) of the 3D path — clean-object clip skip, thin call surface, baked meshes, the merge decision and the close — 0/30 tasks
| Depends on | Phase 6 (textures and VRAM under control) and phase 7 (2D paths settled) |
| Exit criterion | a 3D sample at target frame rate with no per-frame full GE wait — **met at the frozen N=50**: median 16.683 ms, p95 16.695, work 11.56 ms, 0 arena resets (069). The 60 fps ceiling is between N=50 and N=99; the cost above it is engine-side soft float (P10-S8) |

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
- [x] A 3D sample exists that exercises models, lighting and a textured mesh — 067: `SAMPLE=1` (though it does not
      meet its timing target)
- [x] Camera-relative rendering is already the convention — 061 measured it as a **null**; not required, recorded

## Readiness review (2026-11-22)

Phase 10 can start: Phases 6 and 7 are closed, so the texture/VRAM layer and the 2D paths are stable. The review
found two blockers that decide the first sprint, plus three scope gaps.

**Blocker 1 — `DrawMesh`/`glDrawElements` does not produce pixels, and the cause is visible in the code.** The
carried phase-5 trace is `drawElements count=36 tex2d=0 fmt=0x1e3` with no pixels (sprint 013 §T1 split). Decoding
the format: `0x1e3 = GU_TEXTURE_32BITF | GU_NORMAL_32BITF | GU_VERTEX_32BITF` — texture, normals and positions,
**no colour**. `DrawMesh` enables exactly those three arrays (`src/rmodels.c:1458-1472`), and `guVertexFormat()`
advertises only what was enabled. (`tex2d=0` is separate: `LoadMaterialDefault` leaves the diffuse texture id at 0
on GL11 because `rlGetTextureIdDefault()` returns 0, so texturing is never enabled; the format still carries UVs
because `mesh.texcoords` is non-NULL.) The converting path, however, always writes the fixed record
`{u,v, color, nx,ny,nz, x,y,z}`: 36 B, measured offsets colour +8, normal +12, position +24. GU's format implies
32 B with normal at +8 and position at +20, so it reads the material colour as the first normal component and the
tail of the normals as the position — garbage, and usually no pixels. The immediate cube works because it uses the
batched 24-byte layout, which matches its format. **First task (sprint 060):** pack only the enabled fields, in GU
order, with the stride computed from the format; prove it with `MESH_SCENE=1` against the predicted cube bbox
x 193..287, y 92..189.

**Blocker 2 — render-to-texture has no backend.** `tools/pspgu/` implements no `glGenFramebuffers`,
`glBindFramebuffer`, `glFramebufferTexture2D` or readback; on GL11 `rlLoadFramebuffer()` returns 0
(`src/rlgl.h:4163`). `LoadRenderTexture`/`BeginTextureMode` therefore cannot work, and an app that uses them may
not even link. The phase needs a design decision before implementation: implement the GL FBO entry points over
pspgu's VRAM (sampling a render target, explicit read-after-write), or add a smaller PSP-specific target API. A
256×256 8888 target is 256 KiB and a 512×256 target 512 KiB; both fit the ~913 KiB tail but compete with textures
and the 3D depth buffer. Working implementations do exist and all take the same route, not GL FBOs — see
[Render-to-texture: what the references actually do](#render-to-texture-what-the-references-actually-do).
Specified in [P10-S4](../../sprints/065-phase10-render-to-texture.md).

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

- [x] **Carried from phase 5, first:** make `glDrawElements`/`DrawMesh` produce pixels — the converting path and
      index rebasing in `tools/pspgu/gl_draw.c`; verified 0 diff vs pspGL, bbox x 193..286, y 92..188 (060)
- [x] Convert model vertex data **once**, offline or at load, into GE-native formats; no per-frame conversion
      — 061: s16 about the centre, 28-B stride, `direct=1 converted=0` on static meshes
- [ ] Index buffers where type and alignment allow — **deferred at close**: indices are copied per chunk; no VBO path
- [x] Map fog, lighting and texture environment onto GE state; document explicitly what cannot be expressed
      — 062: lit/texenv/fog 0 diff vs pspGL; two-sided, COMBINE, EXP fog and per-pixel specular rejected
- [x] Render-to-texture with explicit read-after-write tracking; synchronise only on a real dependency — 065:
      256×256 0 diff, dirty targets use `TexFlush`+`TexSync` only (the documented order, local notes §2, tested and rejected in P10-S9 T1)
- [x] Static bounds and GE-side object culling for scenes with many objects — 066: 917/1,000 culled,
      0 pixel change, 233.6 → 50 ms
- [x] Tessellate large triangles on the CPU where the GE would drop them at the window edge — 066:
      clip at NDC, 236 px where the GE drew 0; near plane 720 vs 0
- [ ] Optional: build display-list words or cull on the Media Engine — **dropped**: Phase 9 sprint 051 dropped
      the ME takeover — measured at 0 failures elsewhere, but a
      second CPU with its own cache protocol ([§10.2](../../memory-map.md#102-media-engine-the-second-mips-core))

## What we already know

- Matrices keep ~16 significant bits: big world coordinates tremble while the camera moves
- No X/Y clipping after the viewport transform — a primitive with one vertex outside the 4096 window is dropped
  whole; only the near plane clips
- s16 positions are normalised (value/32768) in 3D transform mode — pair every s16 buffer with a scale matrix
- The GE skins vertices in hardware; CPU skinning is wasted work
- The ME has no VFPU, so any 3D maths moved there becomes scalar

## Exit criteria

- [x] 3D sample holds the target frame rate, with frame-time percentiles — **met**: N=50, 300 frames, two runs,
      median 16.684/16.683 ms, p95 16.695, work 11.563/11.557 ms. Ceiling between N=50 and N=99 (N=100 work
      18.03 ms); 069 §7
- [x] no per-frame full GE wait; hazards synchronise individually — **met in the sample**: 0 arena resets; list
      waits 13-14 µs, display waits 19 µs; 069 §7
- [x] vertex formats and their byte costs documented in the feature page — 060/061 tables
- [x] what the GE cannot do is listed, with the fallback the engine takes — S3/S4/S5 limits tables

## Audit against the GU capabilities (2026-11-23)

Re-read the GU API surface and the samples shipped with our toolchain, then A/B the current driver and reuse the
069/070 measurements. All numbers are device-only; every behaviour cited below is either measured here or comes
from the toolchain's own headers and samples. Vendor-document provenance for the GU features checked is in the
local reference notes (`LOCAL-REFERENCES.md`, untracked, not in the repository) §5.

**What is already at or above the samples' level.** Three framebuffers with two rotating direct lists, a per-list
vertex arena and targeted waits (13-19 µs, no full GE wait at 60 fps); GE-native packed meshes on the direct path;
guard-band clipping instead of the screen edge; VFPU projection in the clipper; texture shadow/residency; RTT over
the GU draw buffer. The toolchain's `doublelist` sample is the closest reference and does less.

**GU features we do not use, and why.**

| feature (public source) | why not |
|---|---|
| `sceGuBeginObject`/`EndObject` — hardware object culling | It skips *rasterisation*, not submission. Our CPU cull removes the whole `DrawMesh` (submission **and** clip classification): at N=500, 218 of 500 calls never reach the driver. Replacing it with a GE-side box would submit all 500 at ~51 µs/call — about +11 ms before the GE does anything. Not recommended as a replacement. |
| `GU_TRANSFORM_2D` with 16-bit vertices and 16-bit textures | A 2D vertex in that format is ~12 B with no 3D T&L; ours is 24 B (f32 uv/pos, 8888 colour) with a 2D ortho. The 2D pixel gate is exact (613 colours); the 061 s16-UV trial cost 37 px, and 5551 colour would move every shaded pixel. The 2D ceiling is CPU-bound in the batcher, not vertex-bound, so this is an opt-in experiment, not a default. |
| `GU_NORMAL_8BIT`, `GU_COLOR_5551` | Bandwidth/quality trade on the packed meshes; the pack is one-time and the GE is not the bottleneck. Parity risk on the lit path. Low priority. |
| `sceGuCopyImage` | VRAM-to-VRAM only (the sample passes EDRAM pointers). It cannot DMA a main-RAM upload, so the 8-row band upload stands. |
| `GU_CLIP_PLANES` | The API exposes only the enable bit, with no documented plane setter, so it cannot replace the software near-plane clip. |

**Where the time actually goes (measured).**

| finding | number | source |
|---|---|---|
| clip pass share of the cube scene | **37 % at N=100** (11.07 vs 6.93 ms), **34 % at N=500** (38.63 vs 25.40) | `PSPGU_NO_CLIP=1` A/B, `matrix_probe MODE=scene` |
| clip cost per submitted draw | **~47-52 µs** (~1.4 µs/vertex) | same, 80 and 282 drawn draws |
| per-`DrawMesh` baseline (clip off) | **~51 µs/call** (25.4 ms / 500) | same |
| CPU cull | **12 µs/call bracketed**, ~8-12 after the 3.94 µs clock tail | 070 T3 |
| per-object compose + `rlMultMatrixf` | ~3 µs + ~3.5 µs | 070 T3 |
| 3D scene at N=500 | 38.6 ms work ≈ 500 calls × 77 µs | 070 §7 |

So the 3D path is **draw-call bound**, not matrix-bound: the matrix work GUM could touch is a few percent, which is
why 070 dropped it. The clip pass is the largest *driver-side* share and the per-call path the largest overall.

**The attacks, in order.**

- **P1 — clip classification (~47 µs/submitted draw).** Deduplicate unique vertices for indexed draws (a cube's 36
  indices reference 24 vertices; a torus shares far more), fuse the s16 fetch with the VFPU transform, and keep the
  per-object AABB shortcut — it is why most cubes are cheap. Target: halve the per-draw clip cost; measure with the
  `NO_CLIP` A/B.
- **P2 — per-call overhead (~51 µs/call).** The structural fix is instancing or merging draws, but it trades the
  per-call cost for CPU vertex expansion and *loses the per-object clip shortcut* (a merged 10k-vertex batch cannot
  be skipped by an AABB and would be classified whole). Prototype and measure; do not assume the 2D batcher's win
  transfers.
- **P3 — the cull (~12 µs/call).** Cache the MVP once per frame (two `glGetFloatv` per mesh today) and transform the
  eight corners with the VFPU transform already in the tree (8 × ~0.35 µs vs ~8 µs scalar). Estimated: the 6 ms cull
  at N=500 → ~2 ms.
- **P4 — compose/multiply (~6.5 µs/call).** Same VFPU transform; not GUM (070 T4: default GUM slower, VFPU GUM
  wins only the multiply and needs a VFPU/FPU fence — it raised FPU Exception (IV) in the probe).
- **P5 — compact-format experiments** (2D s16/5551, s8 normals) only behind flags and the pixel gates.

## Cross-engine control: M3D (2026-11-24)

A second engine on the same hardware was measured against this port, with the same content and the same capture
discipline: [M3D compared with pspgu](m3d-lessons.md). M3D has no GL layer, no CPU clipper and no CPU transform — it
bakes geometry, composes a GUM matrix per object and lets the GE transform.

| cubes (20-wide grid) | M3D work / fps | ours (merged) work / fps |
|---|---|---|
| 400 | **8.48 ms / 60.0** | 31.92 ms / 29.8 |
| 800 | **15.93 ms / 57.3** | 54.05 ms / 15.0 |
| 1,200 | **24.07 ms / 30.0** | 96.11 ms / 9.3 (154 replays) |

M3D holds 60 fps to **760 cubes**; its per-cube cost is **~19 µs** against **~55 µs** for our CPU-expanded merged
vertex and **~180 µs** for our per-object draw. The 2D side goes the other way: our batcher does 1,566 sprites in
6 draws at 20.5 ms, while both M3D 2D paths are one draw per sprite (34.9–63.8 ms).

The actionable conclusions are F1–F3 and R1–R3 in that page: the GE is not the bottleneck, our call surface is, and
the clip scan is the largest removable block for objects the classifier already proves clean. They feed the open
sprints 086–089 directly.

## Guidelines from the renderer blueprint

Sources: local reference notes (`LOCAL-REFERENCES.md`, untracked, not in the repository) §3, cross-checked against
our own measurements; where they disagree, ours win.

**Vertex pipeline and formats:**

- Compact world format the blueprint recommends: `GU_TEXTURE_16BIT | GU_COLOR_5551 | GU_VERTEX_16BIT |
  GU_TRANSFORM_3D`, adding `GU_NORMAL_8BIT` if the GE lights. Interleaved, fixed field order.
- s16/s8 positions are normalised (value/32768 and value/128): quantise **about the mesh centre** and put
  `scale × 32768` plus the centre in the model matrix, or the scene collapses inside the near plane. This is the
  same trap the memory map already records, with the fix spelled out.
- Vertex colour is `0xAABBGGRR` — red in the low byte; bake it that way.
- Skin weights must sum **exactly** to 128 (u8) or 32768 (u16); the GE does not normalise them.

**Vertex formats and byte costs (measured, sprint 060).** The converting path packs only the fields the format
advertises, in GU order: texture, colour, normal, position. `DrawMesh` sends texcoords, normals and positions but no
colour array, so its format is `0x1e3` at 32 B — not 36 B. Two format fields are not attributes: `GU_INDEX_16BIT`
must be set for indexed draws, and a format without a colour field takes its colour from `sceGuColor`. These are the
32-bit formats; P10-S2 replaces them with GE-native ones where the measurement allows.

| enabled arrays | fmt | stride |
|---|---|---|
| tex, normal, vertex (`DrawMesh` default) | `0x1e3` | 32 B |
| tex, colour, normal, vertex | `0x1ff` | 36 B |
| normal, vertex | `0x1e0` | 24 B |
| tex, colour, vertex | `0x19f` | 24 B |
| tex, vertex | `0x183` | 20 B |
| colour, vertex | `0x19c` | 16 B |
| vertex only | `0x180` | 12 B |
| GE-native (061): tex f32, normal f32, position s16 | `0x163` | 26 B fields, 28 B stride |

The GE-native row is packed once in `UploadMesh` and handed straight to GU by pspgu (`direct=1`); the table's other
rows are the converting path. Packing is gated to the pspgu driver: pspGL does not render the interleaved `GL_SHORT`
layout and stays on the float path.


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
| 3 | **GE skinning for a hero character, cached CPU skinning for crowds** | 7.2× on one hot instance, loses on 112 draws (4.9 vs 2.0 ms) — animation LOD, not one policy | local notes §3, consistent with our own findings |
| 4 | **VRAM-resident, swizzled atlases for 3D materials** | second-order for CPU cost, first-order once GE texture fetch becomes visible | `openTRI/triTexman.c:406-440`, `triImage.c:256-317` |
| 5 | **Render-to-texture with an explicit target size** | see the bug below; the pattern is a separate target and a copy, not a resize in place | `openTRI/tri3d.c:133-200`; `PGE/pgeGfx.c:451-490`; local notes §2 (reference sample) |
| 6 | **`GU_SEND` dual lists** | ranked last even here: costs a frame of latency and 2× list memory, and buys nothing while the CPU is the constraint | `openTRI/triGraphics.c:396-409` |

### Render-to-texture: what the references actually do

Every working implementation in the reference set uses the same primitive: **point the GU draw buffer at a VRAM
block with `sceGuDrawBufferList`, render, restore the screen buffer, then bind the block as a texture**. None uses
a GL-style framebuffer object; on this hardware the draw buffer *is* the render target. The draw-buffer contract
(local reference notes §2) is: `fbp` must be 8192-byte aligned and `fbw` a multiple of 64 between 64 and 1024;
changing the draw buffer does not change the display buffer; and memory the GE has written may only be re-read as a
texture after `sceGuTexFlush()` **and** `sceGuTexSync()`.

| Source | Mechanism | Worth taking | Not worth taking |
|---|---|---|---|
| local notes §2 (draw-buffer contract) | `sceGuDrawBufferList(fpf, fbp, fbw)` for mid-list buffer changes; `sceGuCopyImage` note: `TexFlush` then `TexSync` before sampling | the alignment/width rules and the cache contract | — |
| local notes §2 (reference sample) | renders a camera effect into a 256×256 5551 aux buffer placed after three 272 KiB frame buffers (`main.cpp:47-50`), redirects (`SceneGraph/Context.cpp:211-215`), restores screen and viewport (`:228-231`), binds with `TexMode`/`TexImage`/`TexFlush`/`TexSync` (`:232-236`), then composites a full-screen quad | the complete contract-conformant order: redirect → draw → restore → flush → sync → sample; 128 KiB target | fixed aux size; no dependency tracking — it flushes and syncs unconditionally on every camera switch |
| PGE | `pgeGfxRenderToTexture` (`pgeGfx.c:451-469`) requires a VRAM texture, calls `sceGuSync(0, 2)` and `sceKernelDcacheWritebackAll()`, redirects, sets offset/viewport/scissor; `pgeGfxRenderToScreen` (`:471-490`) restores; sample `samples/graphics/rendertotexture/rendertotexture.c:30-55` uses a 128×128 8888 target | save/restore as a pair; validate that the target is VRAM | a whole-D-cache writeback and a GE sync per switch, and `pgeTextureActivate` (`pgeTexture.c:670-696`) never issues `TexFlush`/`TexSync` — it works because the sample's target is power-of-two and the sync is global, not because the contract is met |
| openTRI | `triRendertotexture` (`triGraphics.c:604-632`) validates PSM and VRAM range then redirects; `triRendertoimage` (`:638-668`) and `triRendertoscreen` (`:671-686`) restore. 3D: `tri3dRenderbufferCreate` (`tri3d.c:133-159`), `SetRendertarget` (`:161-174`), `SetTexture` (`:176-190`) with `TexScale(logical/padded)`; call sites `sceGuTexSync()` (`tri3d.c:327` etc.) | `TexScale` for a padded target; the PSM whitelist; an explicit restore path | no sync at all before the redirect; `TexSync` without `TexFlush`; allocates `twidth*height` (`:158`) but declares `twidth*theight` (`:185`) — non-power-of-two heights over-read (already listed under hazards) |
| daedalus | no render-to-texture path found: `sceGuDrawBuffer` appears only at context setup and resolution changes (`GraphicsContextPSP.cpp:787,862,912`); `sceGuCopyImage` (`:317-319`) is an interlaced display blit; its framebuffer effects are HLE (`Ucode_FB.h:24,65,96`) | confirmation that even a full emulator does not reach for GL FBOs here | does not supply an example; a search of `Source/SysPSP` for `sceGuDrawBufferList` and "render target" returned no RTT use |

**What this decides for pspgu.** The backend needs a draw-target redirect, not FBO semantics implemented inside
the GE. Whether the public API is `glGenFramebuffers`-style or a smaller PSP-specific one, it must emit
`sceGuDrawBufferList` plus offset/viewport/scissor and restore the screen state afterwards. Three requirements
follow from the sources:

- **Synchronise read-after-write explicitly, and only then.** The contract in local notes §2 is `TexFlush` + `TexSync`
  after the GE rendered into the target. PGE's whole-cache writeback and per-switch GE sync would violate phase
  5's "no full GE wait" rule; openTRI's unconditional `TexSync` at every binding is closer but still too coarse
  `[source scan: PGE pgeGfx.c:451-490; openTRI tri3d.c:327,507,530,554,584]`.
- **Allocate and declare the same padded extent.** Pad width *and* height to the declared texture size and sample
  with `TexScale = logical / padded`, as `tri3dRenderbufferSetTexture` does. A render target is sampled through
  the same 512-pixel band path measured in sprint 020, so the window rule applies; sprint 023's strip table is a
  measured null, so do not expect texture-cache tricks to rescue a poorly sized target.
- **Budget the target as VRAM, not as a free block.** A 256×256 8888 target is 256 KiB; the reference 256×256 5551
  aux buffer is 128 KiB; PGE's sample target is 64 KiB. Targets compete with the texture tail and, for 3D, the
  depth buffer. Read the boot allocation counters; the allocator is a 512-byte span allocator with a free list and
  coalescing (sprint 015 T3), and `pspguVramStats` reports the largest free span — but a target must be
  8192-byte aligned and padded to a power of two to be sampled.

**Measured RTT limits (sprint 065).** One 256×256 8888 target with a 16-bit depth renderbuffer is complete and
samples byte-identically to the direct render; it costs 256 KiB colour + ~136 KiB depth and survives texture churn.
The GE's 8888 draw buffer does not retain alpha, so a target samples as opaque RGB; alpha-bearing targets are
unsupported. A 300×200 target pads to 512×256 and needs 512 KiB colour + 278 KiB depth, which the fragmented tail
cannot supply (largest free 106,496 B at creation) — rejected with numbers, not silently accepted.

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

## Phase result

Phase 10 closes with all four criteria met. The two that sprint 067 recorded as not met were re-opened and re-tested
in sprint 069, not redefined:

- **Frame rate.** The frozen sample (N=50 cubes, 16×16 ground, torus, RTT panel, one directional light, fog) holds
  **60 fps**: median 16.684/16.683 ms, p95 16.695, work 11.56 ms over two 300-frame runs, with `arenaOverflows=0`
  and targeted waits of 13-14 µs. The 60 fps ceiling is between N=50 and N=99 (N=100 is 18.03 ms work). The
  pre-registered 250-step sweep (N=0/250/500/750/1000) is recorded in 069 §7; its only passing point is N=0, which
  is why the finer N=100 bracket is the one that locates the ceiling.
- **No full GE wait.** The 300 arena-reset syncs 067 measured are gone: the clip pass classifies once, reads
  attributes only for dirty triangles, projects with VFPU, clips only triangles that leave the GE guard band, and
  stages exact-size output in the per-list arena.
- **The checker flicker** the user reported was a driver defect found during this work: `glBindFramebuffer` saved a
  stale screen target, pinning drawing to one framebuffer while the display rotates through three. **P10-S9 (076)
  fixed it** with candidate (b), `saved.draw = fbOffset[frames % 3]`. The earlier 624-800 px "RTT regression" was
  a 2-frame display-lag measurement; the 8-frame gate is **0 diff**, `fb0 = fb1 = fb2`, `FLICK 0`, sample
  16.683/16.684 ms median and 11.98-12.13 ms work. pspgu `b31`; pspGL restored.

The **lit "parity" claim from 062 is withdrawn**: the exact 062 combination renders 248/128/160 where the archived
pspGL capture reads 192/56-64/80-88, and the archived "pspgu" capture is byte-identical to pspGL — the archive-swap
bug, not a lighting match. The lighting, texture-environment and fog implementations stand; only the byte-for-byte
claim does not. 062 carries the correction note.

Deferred and recorded: index buffers/VBOs, s8 normals, s16 UVs, RTT alpha, and any draw whose clipped output
exceeds the arena. The remaining CPU cost above N=50 is rlgl's per-`DrawMesh` matrix and culling work in soft
float, specified as **P10-S8 (070)**.

## Sprints and features

| Sprint | Focus | Status |
|---|---|---|
| [P10-S1 — DrawMesh conversion fix and the 3D mesh gate](../../sprints/060-phase10-drawmesh-and-mesh-gate.md) | pack per format, chunker fix, cube gate, `scene_3d` skeleton | **done** — 0 diff vs pspGL; b25 |
| [P10-S2 — Camera-relative and GE-native model data](../../sprints/061-phase10-camera-relative-and-ge-native-models.md) | one-time s16 packing, direct interleaved path; camera-relative measured null | **done** — 0 diff, `direct=1`; T2 dropped |
| [P10-S3 — Lighting, fog and the texture environment](../../sprints/062-phase10-lighting-fog-texenv.md) | PSP-only light/fog surface, GE state mapping, texenv modes, bake measurement | **done** — lit/texenv/fog 0 diff; bake measured |
| [P10-S4 — Render-to-texture](../../sprints/065-phase10-render-to-texture.md) | weak FBO routing, 8192-aligned targets, explicit RAW, depth, readback, VRAM budget | **done** — 256² 0 diff; 300×200 rejected |
| [P10-S5 — Culling, guard-band clipping and the ME option](../../sprints/066-phase10-culling-and-guard-band.md) | NDC clip + fan, conservative culling, ME dropped | **done** — 236/720 vs 0; 917 culled, 0 diff |
| [P10-S6 — 3D sample target, regression and close](../../sprints/067-phase10-sample-target-and-close.md) | frozen sample, pre-registered ceiling, full regression, phase close | **done** — 20 fps; two criteria recorded not met |
| [P10-S7 — Clip cost, lit parity and re-close](../../sprints/069-phase10-clip-cost-and-lit-parity.md) | dirty-only clip emission, guard-band clip region, VFPU projection, framebuffer-rotation fix, lit correction, sweep, re-close | **done** — 60 fps at N=50, work 11.56 ms, 0 overflows; 237/720 px; lit corrected; b30 |
| [P10-S8 — Camera and matrix CPU cost: measure before GUM](../../sprints/070-phase10-camera-matrix-cost.md) | per-call and scene matrix cost, cached-vs-rebuild, conditional GUM A/B | **done** — cache 13x (6.31 -> 0.49 µs/pos); GUM dropped (builders 1.48 µs/frame); cull ~12 µs/call = 20 % at N=500 |
| [P10-S9 — Framebuffer rotation with render targets](../../sprints/076-phase10-framebuffer-rotation.md) | the deferred flicker: localize the RTT band, 8-frame gate, candidate (b) restore | **done** — RTT 0 diff at 8 frames, `fb0=fb1=fb2`, `FLICK 0`, sample 16.683/16.684 ms; b31; pspGL restored |
| [P10-S10 — pspgu optimisation after the GU audit](../../sprints/077-phase10-pspgu-optimisation.md) | VFPU enablement fix, clip early-out, cull MVP cache, per-call attribution, format decision | **done** — VFPU was never active (weak symbol); now opt-in `b32`; dedup/formats dropped with numbers; N=500 39.85 ms (≤30.9 budget not met); CULL 0 diff at 1,000 cubes |
| [P10-S11 — Per-call cost and draw merging](../../sprints/078-phase10-per-call-and-merge.md) | cull classification, merged clean runs under the per-instance AABB rule, `pspguDrawMergedArrays` | **done** — adopted `PSP_MERGE_DRAWS=1`: N=500 43.22 → **26.67 ms**, 20 → **30 fps**, 60 fps ceiling 150 → **270** instances, gates 0 diff, 276/282 instances merged, 0 replays; `bunnymark` median 16.712 (0.03 over); pspgu `b33` |
| [P10-S12 — PGE-derived pspgu experiments](../../sprints/079-phase10-pge-derived-pspgu-experiments.md) | VFPU clip lerp/pack, fast clear, rotated-2D sincos, through-mode feasibility, lone-quad fan | **done** — T3 sincos adopted (4.703→4.651 ms, 0 diff); T1/T2/T4/T5 dropped with numbers; b35; pspGL restored |
| [P10-S13 — 2D state scoping and colour gates](../../sprints/080-phase10-2d-state-and-colour-gates.md) | lighting/fog scoped to 3D, a non-white-material colour gate, the scrshot layout as a tool | **done** — 4/4: HUD 255 PASS, `scene_3d LIGHT=ge` 0 diff, pspgu merge 0/1 + pspGL colour gate, `batch_check` 613, `basic_shapes` 284, sample 16.684/16.684, p95 16.694/16.695; b37 |
| [P10-S14 — Reference ideas: verify, measure, adopt or drop](../../sprints/081-phase10-reference-ideas-verified.md) | depth convention equivalence, framed as a pixel gate; 5551 vs 5650 framebuffer, measured on the benchmark pair; post-close `sceGuGetMemory` check | **done** — both ideas already present: depth form 0 diff / Δwork ≤0.003 ms; 5551 → Δwork 0, 49.9 % pixels ≤4/255 → dropped; clean-worktree baseline 37.33 / 20.51 ms; `sceGuGetMemory` handed to P10-S15 T6a |
| [P10-S15 — Hardware optimisation sweep](../../sprints/085-phase10-hardware-optimisation-sweep.md) | GE/CPU split; s8 on a vertex-bound mesh; strips and an index cache; CLUT4/8 on a 3D texture; static CALL lists and a sync audit; coherence stress; 64-px sprite slicing and list-local vertices; 2D through-mode 16-bit UV and clear stripes dropped after review | **done** — written 2026-11-23, closed 2026-11-24; 9/9; T1 GE/CPU split done (§9.1): 3D CPU-bound 30.1 vs 13.1 ms, 2D GE-bound 19.5 vs 29.2 ms; T2 s8 dropped — 0.171 ms GE win, 59,538/130,560 px differ (§9.2); T3 strips −1.073 ms GE at 0 px diff, index copy 31 µs/1,000 → follow-up (§9.3); T4 CLUT8 no GE gain (5,825 vs 5,847 µs), 0 px, 5 KB vs 8 KB VRAM (§9.4); T6a list-local vertices −1.49 ms CPU on 2D but 3D blocked at 161,768 B (cause unresolved) → dropped (§9.6); T6 CALL lists cap at ≈1 µs on 4 draws, sync audit clean (§9.7); closed 2026-11-24 — §9.9: no default change; closing pair 33.548/33.803 ms; batch_check 613; driver 9798af84 |
| [P10-S16 — List-local vertices](../../sprints/086-phase10-list-local-vertices.md) | diagnose the 161,768 B ceiling; a reserve sized from the list; reopen handling; a bounds guard and a forced-fallback test; the pair A/B; the pixel table; an arena-removal arm | **folded** — written 2026-11-24, folded into P10-S17 as workstream A; §6 stands as the reserve/guard design reference |
| [P10-S17 — Adopting the measured wins](../../sprints/087-phase10-adopt-measured-wins.md) | list-local vertices (A), index-run cache (B), baked static geometry (C); one pair per build so each workstream is attributable; full pixel table; RAM ledger | **open** — written 2026-11-24; 0/15; each workstream adopted or dropped on its own gate |
| [P10-S18 — Submission models](../../sprints/088-phase10-submission-models.md) | single 1 MiB list + full sync vs two rotating lists + arenas; capacity sweeps; frame time at three build/GE ratios; RAM ledger; mid-frame reopen; pixel table on both arms | **done** — measured 2026-11-24; 7/7 (S5 dropped); single 2D 50.539 vs dual 33.800; N=100 isolated: 33.398 vs 16.759; 3D overflow 324,372 > 245,760; 5,000 bunnies refused; ΔRAM 1.22 MiB; gate 613 on both; dual model kept; §9.6 adds the command-headroom trap |
| [P10-S19 — Vertex memory 2×2](../../sprints/089-phase10-vertex-memory-2x2.md) | list-local vs arena, measured as a 2×2 (submission × vertex memory) with a per-draw allocation variant; capacity ceiling per cell; list-bytes ledger; pixel table; pair for the winner | **done** — measured 2026-11-24; 7/6 (V6 dropped); at 196 cubes arena+dual 25.099 vs list-reserve 26.309 vs list-per-draw 30.773 vs single+arena 33.399; keep the arena (§9.4) |
| [P10-S20 — Clean-object draws](../../sprints/090-phase10-clean-draw-fast-path.md) | matched baseline on one layout; the classifier's clean answer skips the clip scan for per-object draws; A/B against `PSPGU_NO_CLIP=1`; gates | **open** — written 2026-11-24; 0/5; largest single block in the per-object path (~47 of 180 µs) |
| [P10-S21 — Thin call surface](../../sprints/091-phase10-thin-call-surface.md) | per-line instrumentation; PSP-guarded matrix epoch; cached MVP from the cull; skip unchanged pointer/texture state | **open** — written 2026-11-24; 0/6; the sprint most likely to grow because it touches shared rlgl |
| [P10-S22 — Object-space baked meshes](../../sprints/092-phase10-baked-mesh-path.md) | bake once into a GU-native object-space buffer, `pspguDrawBakedMesh`, raylib integration and lifetime, memory ledger, 1,200-cube capacity | **open** — written 2026-11-24; 0/9; target 180 → ~40 µs/cube |
| [P10-S23 — World-space bake, merge decision](../../sprints/093-phase10-static-bake-and-merge-decision.md) | world-space bake arm (no matrix upload); merged vs thin at 400/800/1,200; key limits; replays; one architecture chosen | **open** — written 2026-11-24; 0/6 |
| [P10-S24 — Thin 3D close](../../sprints/094-phase10-thin-3d-close.md) | matched 60 fps ceiling on one layout; arena/replay ledger; retire or gate the loser; full gates; 087 disposition; docs | **open** — written 2026-11-24; 0/4 |
| [P10-S25 — Keeping the port mergeable](../../sprints/095-phase10-psp-code-quarantine.md) | quarantine PSP code out of core raylib files: mesh/cull/merge → `platforms/`, batcher → one included header, shape guards → one switch, audio decoder → `raudio_psp.c`; measured by line count, symbols, gates and pair | **open** — written 2026-11-26; 0/6; move-only, no behaviour change |
