# Phase 4 — PSP fixed-function rlgl batcher

| | |
|---|---|
| State | **done** — sprints 007–009; 1,000 sprites at 60 fps, 60 fps ceiling 1,600, batched by default |
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

- [x] submission count collapses from object count to state-run/capacity count — 1,025 `glEnd` → 2–3
  `glDrawArrays` per frame at N=1,000, 5,024 → 9 at N=5,000 (sprints 007/008)
- [x] screenshots byte-identical where they should be, and diffs explained where not — `batch_check` md5
  `c4eb44e7…` on the unbatched, triangle and sprite arms, 0 differing bytes
- [x] no per-sprite heap allocation measurable — one static 4,096-vertex buffer, `rcore.o` bss +98,328 B
- [x] frame time improved at the high bunny counts — 66.733 → 16.683 ms at N=1,000 (60 fps), 316.98 → 66.733 ms at
  N=5,000. **Remaining before this phase closes: sprint 009** — the mixed-scene regression, the samples and the
  default

## Guidelines from the renderer blueprint

Sources: local reference notes (`LOCAL-REFERENCES.md`, untracked, not in the repository) §3, cross-checked
against our own measurements below; where it disagrees, our numbers win.

**What our toolchain actually offers** (headers checked, see [§14](../../memory-map.md#14-what-pspdev-actually-provides-verified-2026-11-20)):
`GU_SPRITES 6`, `GU_TRANSFORM_2D` ("passed directly to the rasterizer"), `sceGuScissor(x, y, w, h)`,
`sceGuTexImage/TexMode/TexFlush/TexSync/ClutLoad`, `sceGuDepthRange/DepthOffset`, and `sceGumDrawArray` for the
CPU-transform path. `libpspgum.a` is the default GUM; `libpspgum_vfpu.a` is opt-in and used by no sample.

**What the blueprint says a 2D path must do:**

| rule | why it matters | how it lands in rlgl |
|---|---|---|
| Use through mode (`GU_TRANSFORM_2D`) for sprites/UI | no transform, lighting, fog or clipping; coordinates are screen pixels | the batcher's layout should be screen-space, no matrix per sprite |
| `GU_SPRITES` = 2 vertices per axis-aligned rect | 1/3 the vertices of two triangles; colour/depth from the second vertex | matches the 1024-sprites-per-call figure we measured in `PSPRuntime` |
| **In through mode UVs are texel coordinates, not 0..1** — *the blueprint adds "`TexScale`/`TexOffset` are ignored", which `pspgu.h` contradicts by recommending them for texcoords in this path* | a 0..1 UV would sample inside the first texel | the batcher must convert, and this is where the unsigned-16-bit TEXCOORD trap bites. **Settle the scale/offset half with one textured quad on device before designing around it** |
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
may have moved work inside the vsync bucket. We measured this the hard way: see [baselines](../../baselines.md) —
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

## What the four reference engines agree on (ranked by payoff)

Scanned the memory maps and sources of `PSPRuntime`, `3D-PSP-Tutorials`, `PGE` and `openTRI`
(`/home/psp/ref/`). They disagree on style and agree on physics: **four independent codebases, same ranking.**
Ordered by expected effect on the measured bottleneck (58.3 µs per sprite at 1,000 sprites).

| # | lever | why it is the lever | anchors |
|---|---|---|---|
| 1 | **Accumulate vertices, one `sceGuDrawArray` per (primitive, texture)** | collapses 1,000 submissions into 1–3. Everything else is a fraction of one submission's cost | `PSPRuntime/gpumanager.cpp:207-217`, `:1740-1829`; `openTRI/triFont.c:1201-1302`; `PGE/pgeFont.c:241` |
| 2 | **One atlas, one bind per batch** | texture identity is the only remaining reason a batch splits; per-sprite rebind is 11 GU writes | `openTRI/triGraphics.c:1665,1694` (inverted); `PGE/pgeGfx.c` bind path |
| 3 | **`GU_TRANSFORM_2D` (through mode)** | `sceGumDrawArray` composes a matrix and sets up VFPU *per call*; through mode makes a draw a pure command — the largest single CPU-side removal | `3D-PSP-Tutorials` ranking #2; `PGE` G7 |
| 4 | **2-vertex `GU_SPRITES`** | halves the payload and deletes index traffic: triangle sprite is 4×24 B + 6×2 B = **108 B**, a 2-vertex sprite is **48 B** | `PSPRuntime` A3; `PGE` G7 |
| 5 | **Hoist the state writes out of the per-sprite path** | the tutorial overlay does Disable-depth / TexFunc / TexMode / TexImage / Enable-depth *per sprite*; all five are constant across a batch | `3D-PSP-Tutorials` ranking #3 |
| 6 | **Compact vertex: 8-bit UV, 16-bit position, 5551** | halves to quarters the list bytes; matters once the fixed per-draw cost is gone | `openTRI/triTypes.h:520-563`; `PGE` V-series |
| 7 | **Cache the CLUT across a batch** | one `sceGuClutMode`/`ClutLoad` per palette change instead of per sprite | `openTRI/triGraphics.c:1657-1663` |
| 8 | **Drop the depth buffer for the 2D path** | 272 KiB reclaimed (688 → 960 KiB free) and removes the per-call `GU_DEPTH_TEST` toggling | `PGE` G11 |
| 9 | **Keep `GU_SPRITES` out of the triangle path entirely** | mixing primitive types splits batches | `PSPRuntime` A2 |

**What they all rank low, and we should too:** VFPU trig (`vfpu_sinf` instead of `sinf` — free, but sub-1 % of
a submission), VRAM swizzle and residency (raise the ceiling on batch size, don't lower per-sprite time), and
`GU_SEND` dual lists (pointless when the CPU is the constraint; costs a frame of latency and 2× list memory).

### Where the engines contradict the blueprint: 64-pixel slicing

The renderer blueprint recommends drawing wide images as **32–64 px vertical strips** for texture-cache locality.
Three of the four engines do exactly that — and all three rank it as **wrong for us**: a strip is one more draw
call per 64 px of width, which is the opposite of what a submission-bound renderer needs.

| source | policy | verdict here |
|---|---|---|
| blueprint §16.5 | slice wide blits into 32–64 px strips | only after GE texture stalls are *measured* |
| `openTRI/triGraphics.c:1672-1680` | slices | a draw-call multiplier |
| `PGE/pgeGfx.c:978`, `:1088` | slices, plus a rotated path that turns 1 submission into 4 | same |
| `PSPRuntime` | does not slice for the batch path | consistent with batching |

Decision for phase 4: **do not slice**. Reintroduce only if the profiler shows texture-cache stalls dominating,
and then as a measured A/B.

### Do not copy — found in these trees

- `PSPRuntime/gpumanager.cpp:1661` — `else if (bType = ALPHA_ADD)`: assignment, always true, so every blend request
  becomes additive, and the static `prevmode` (`:1644`) is shared across instances.
- `PSPRuntime/gpumanager.cpp:463-468` — rewrites `currentTexture` without flushing, so a mixed-texture batch
  renders wholly with the last texture.
- `PSPRuntime` flush allocates ~96 KiB of command list per flush (~21 flushes then `sceGuGetMemory` overruns and
  the dead NULL check calls `sceKernelExitGame`). Preallocate once per frame instead.
- `3D-PSP-Tutorials/graphics.c:176` — `sceKernelDcacheWritebackInvalidateAll()` **per blit call**. In a
  per-sprite path that is 1,000 full D-cache flushes a frame and would dominate the entire budget by itself.
- `3D-PSP-Tutorials/LoadingScreen.cpp:32-78` — a second thread running `sceGuStart`/swap while the main thread
  blocks on the same display: two submitters, no synchronisation.
- `PGE/pgeGfx.c:421-425` — `PGE_BLEND_MODE_TRANSPARENT` sets `GU_FIX` factors of `0xFFFFFFFF`, i.e. **additive**,
  and `pgeGfxDrawTextureColored` (`:1071`) never restores it: every later draw stays additive.
- `openTRI/triTexman.c:95-115` — evicts textures the GE may still be sampling, with no `sceGuTexSync` in the free
  path (`:88-93`) even though the bind path does sync (`:357`).

## Reference in our own SDK: `psp/sdk/samples/gu/doublelist`

288 lines, compiles with our toolchain, and it demonstrates the submission discipline the batcher needs rather
than the batching itself. Read it before writing the flush path.

| what it does | where | why it matters to us |
|---|---|---|
| two command lists, one per framebuffer: `list[2][512]`, 512 words each | `doublelist.c:28` | the packet is per-buffer, so nothing is reused while the GE may still read it |
| builds each frame into the alternating packet with `sceGuStart(GU_SEND, pkt)` | `:200-202` | `GU_SEND` instead of `GU_DIRECT`: the list lives in main RAM and is handed over, not executed in place |
| `sceGuDrawBufferList(...)` switches the draw buffer **inside** the packet | `:203` | output target is part of the packet, not global state |
| `int g_packet_size = sceGuFinish();` then `sceKernelDcacheWritebackRange(pkt, g_packet_size);` | `:238-239` | **exactly the pattern we need**: writeback the precise range the GE will read, sized by what `sceGuFinish` returned — not a whole-cache flush |
| `sceGuSync(GU_SYNC_SEND, GU_SYNC_WHAT_DONE)` after an optional `usleep` | `:247` | shows the send-then-wait split, and doubles as a way to measure GE time |
| static sublists built once with `GU_CALL` | `:157-160` | the model for phase 5's static geometry |

What it does **not** show, and phase 4 has to answer: how many primitives must share a packet for the
`syscall`-per-submission cost to disappear, and what the flush rule is when texture and blend state change inside
one frame. Take the packet handling, not the batching policy.

## Acceptance target

The blueprint's phase-1 proof ("1,000 sprites at 60 fps, one bind per batch, pixel-exact 1:1 blit") is a better
acceptance criterion than "faster than before", because it is a number we already measure.

| | sprites in 16.7 ms | 1,000 sprites take |
|---|---|---|
| today (unbatched, adopted pspGL) | ~287 | **61.63 ms** |
| after sprint 007 (batched) | **~790** | **33.37 ms** |
| target | **≥1,000** | **≤16.7 ms** |

So phase 4 has to buy **3.7×** on this workload to hit 60 fps at 1,000 sprites. That is the number to design to,
and it is in the sprint's exit criteria rather than in a wish list.

**Where sprints 007 and 008 left it:** batching bought 2.0× at `-O0` and 3.0× at `-O2` (unbatched 50.05 →
batched 16.68 ms at N=1,000), 1,025 draw calls per frame became 2–3, and 1,000 sprites now run at **60 fps**. The
`sprite` primitive added ~18 % on top and moved the 60 fps ceiling from 1,200–1,300 sprites to 1,500–1,700. What
stops us there is no longer submission: it is the GE's **blended fill** — 4× the pixel area at N=1,500 takes the
frame from 16.68 to 50.05 ms with CPU work unchanged.

That re-prices the rest of the phase. Compacting vertices and shaving the vertex path are now worth single-digit
percent; the ceiling is pixels. Which lever matters next depends on one measurement that has not been taken: the
`PSPRuntime` reference's claimed ~5,000 sprites at 60 fps, which would be 5.1 M texels per frame against our measured
1.5 M. See [sprint 008 §10](../../sprints/008-phase4-sprite-primitive.md).

## Sprints and features## Sprints and features

- [P4-S1 — Sprint 007 — Batch skeleton, and the measurement that decides the phase](../../sprints/007-phase4-batch-skeleton.md)
  — **done**. The smallest batcher that separates submission cost from per-vertex cost, `PSP_BATCH=0/1` as two arms
  of one commit, and the pending-texture rule that makes batching work at all. 62.6 → 16.7 µs per sprite; the
  residual is linear in vertices and flat in pixels
- [P4-S2 — Sprint 008 — Two-vertex sprites: the last lever inside GL11](../../sprints/008-phase4-sprite-primitive.md)
  — **done**. Axis-aligned quads as `GL_SPRITES_PSP` (2 vertices, pixel-identical), and the discovery that the
  library had never been compiled with optimisation: `-O2` took 1,000 sprites from 33.37 to **16.68 ms**. The
  residual is now blended fill, not submission
- [P4-S3 — Sprint 009 — The per-sprite CPU budget, and shipping the batch](../../sprints/009-phase4-inline-vertex-path.md)
  — the last sprint of this phase. `work = 0.98 ms + 7.08 µs × sprites` is what puts the 60 fps ceiling at 1,500:
  inline the vertex push path (it is cross-translation-unit call overhead, not arithmetic), add a texture-bind
  cache, give sprite and triangle runs their own buffers so a mixed scene stops flushing per sprite (83.4 ms today),
  verify the 3D samples, and turn `PSP_BATCH` on by default
