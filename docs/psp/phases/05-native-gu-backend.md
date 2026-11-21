# Phase 5 — Native GU backend

| | |
|---|---|
| State | **done** — sprint 014 closed the backend; the `DrawMesh` 2D→3D→2D parity criterion is deferred to phase 10 by recorded decision |
| Depends on | Phase 4 closed (batcher shipped, ceiling measured at 1,600 sprites) |
| Exit criterion | the 60 fps ceiling **beats** phase 4's, not merely matches it, with no pspGL symbols in the ELF and VRAM accounted at boot — **met: 2,375 sprites** |

## What this phase is about

Removing the middleman. `pspGL` is a GL 1.1 implementation over the GE — capable, but a 2007-era one, and
every GL call it serves is an interpretation of something the GE could be told directly. raylib already
supports this shape of work: `GRAPHICS_API_OPENGL_SOFTWARE` is nothing but `#define GRAPHICS_API_OPENGL_11`,
which is raylib's own admission that "rlgl speaks GL 1.1, and whatever library supplies those symbols is the
backend". A `GRAPHICS_API_PSP_GU` backend replaces the supplier.

## Sprint 014 outcome (2026-11-21) — the phase closes

**The driver wins the measurement it was built for.** The 60 fps ceiling on the final configuration is **2,375
sprites** (last pass; 2,376 fails in two runs), against pspGL's **1,600**. The one-list pspgu build reached 1,895, so
the queued submission is worth **+480 sprites**, and `batch_check` is still **byte-identical** to pspGL.

Two design points changed under measurement, and they supersede the two-buffer budget below:

- **Three framebuffers.** With two, the frame being displayed is the one just submitted, which forces a full GE wait
  before the swap. Three let the display show frame N−1 while N is drawn; the reported VRAM map is **3 × 272 KiB
  colour + 272 KiB depth = 1,088 KiB**, leaving 913,344 B for textures (69,636 B used by the corpus).
- **64-byte vertex arenas.** Each draw's vertices are copied into a per-list arena (two lists × 128 KiB). Starts are
  cache-line aligned: 16-byte alignment left one polyline sharing a dirty line with its neighbour, and `basic_shapes`
  differed from pspGL by 26–71 edge pixels; 64-byte alignment made it byte-identical.

`GU_SYNC_WHAT_QUEUED` is a non-blocking poll in this SDK (`pspge.h`: syncType 1 = peek), so memory safety is a
`sceGeListSync(qid, PSP_GE_LIST_DONE)` on the list being reused, using the qid `ge_list_executed[0]` exposes after
`sceGuStart`. Both waits (reused list and displayed frame) measure 18–26 µs in a steady frame.

**What "no full GE wait" means here.** It means no wait on the frame just submitted. The driver still waits on
**older** frames — the list whose memory is being reused (two frames old) and the frame about to be displayed (one
frame old) — because both must be complete before that memory or that buffer is used. Those waits are targeted and
counted; the removed operation is the `sceGuSync(GU_SYNC_FINISH, GU_SYNC_WHAT_DONE)` that waited for the current
frame inside `eglSwapBuffers`. A later reader should not equate "no full wait" with "no blocking call at all".

## 0. Review findings (2026-11-21) — the shape is decided and it is not the one written below

**Finding 1: there are two ways to do this and the page only described the harder one.** The page (below) proposes
`GRAPHICS_API_PSP_GU` as a fifth rlgl implementation. The alternative is to **supply the GL 1.1 symbols over GU** —
a drop-in replacement for `libGL.a` — which needs no change to `rlgl.h` at all. The page's own precondition ("the
GL 1.1 call list actually used is known, and that list is the specification") only makes sense for the second
shape, and the list is now measured:

- **52 symbols: 44 `gl*` + 8 `egl*`** (undefined across `rcore.o rshapes.o rtextures.o rtext.o rmodels.o` on the
  current default build). That is the whole specification. For comparison the GL 1.1 headers declare 147.
- The list is small because the batcher is the default: no `glBegin`/`glEnd`/`glVertex*` at all. A `PSP_BATCH=0`
  library adds those eleven immediate-mode entry points; supporting both is one extra dispatch, not a second driver.
- Everything else the library asks of GL is ordinary: `glBindTexture`, `glTexImage2D`, `glTexSubImage2D`,
  `glTexParameteri`, `glDrawArrays`, `glDrawElements`, the four `*Pointer` calls, `glEnable/Disable(ClientState)`,
  `glBlendFunc`, `glClear*`, `glDepth*`, `glColorMask`, `glCullFace`, `glFrontFace`, `glScissor`, `glViewport`,
  `glPolygonMode`, `glLineWidth`, `glShadeModel`, `glPixelStorei`, `glReadPixels`, `glGetFloatv`, `glGetString`,
  the matrix calls, and `egl{ChooseConfig,CreateContext,CreateWindowSurface,GetDisplay,GetError,Initialize,
  MakeCurrent,SwapBuffers}`.

**Decision: build the drop-in, not the fifth backend.** It keeps `rlgl.h` untouched, keeps the sprint 007–009
batcher and its pixel proof, keeps `psp-tests/*` and the `--wrap` counter harness working (the harness interposes GL
symbols, so a new backend would have made every existing counter and capture meaningless), and it makes the whole
deliverable testable one symbol at a time. `GRAPHICS_API_PSP_GU` as an rlgl backend is *dropped*.

**Finding 2: the display is 16-bit today, and the page asks for a different 16-bit format.** RAYWHITE is
(245,245,245) and comes back from a capture as **(240,240,240)**: the framebuffer quantises, so today's path is
`GU_PSM_5650`, not 8888. The task list below says `GU_PSM_5551`. Switching formats changes *every pixel of every
capture*, which collides head-on with the exit criterion "identical output to phase 4":

| format | bytes | vs today | consequence |
|---|---|---|---|
| `GU_PSM_5650` | 2 | same | pixel-identical to phase 4; keep this |
| `GU_PSM_5551` | 2 | every pixel shifts | +1 bit alpha, −1 bit green; needs a written acceptance, not a silent diff |

Two 512×272 buffers are 557 KiB either way; 8888 would be 1,114 KiB. Use 5650 and say why.

**Finding 3: the depth buffer — decided: allocate it, 16-bit, behind a switch.** `BeginMode3D` enables
`GL_DEPTH_TEST`, and **pspGL allocates a depth buffer today** (`eglCreateWindowSurface.c:86-90`, 16-bit,
`height × stride × 2` = 278,528 B), because the platform layer's config asks for `EGL_DEPTH_SIZE, 1`. So "2D-only,
no depth" would be a *regression*, invisible to every test we own: sprint 009's 2D→3D→2D scene renders two cubes
that do not overlap, so it passes with or without depth.

Decision, recorded: **allocate a 16-bit depth buffer by default**, matching today, and put it behind
`PSP_GU_DEPTH ?= 1` so a shipping 2D-only build can drop it and get 272 KiB of VRAM back. In the depth-less
configuration the `GL_DEPTH_BUFFER_BIT` inside `glClear` becomes the documented no-op of finding 7.

The budget, for the record (2 MiB of EDRAM):

| allocation | size | with depth | without depth |
|---|---|---|---|
| two 512×272×16bpp framebuffers | 544 KiB | 544 | 544 |
| 16-bit depth, 512 stride | 272 KiB | 272 | 0 |
| **free for textures** | | **1,232 KiB** | 1,504 KiB |
| 512×512 `5650` backgrounds that fit | | 2 | 2 |
| 32×32 `5650` sprites that fit | | 616 | 752 |

272 KiB is 13.3 % of VRAM and it buys the 3D phase (§10) without reopening this decision later; the switch means no
2D game has to pay it. **Lazy allocation** (allocate on first `glEnable(GL_DEPTH_TEST)`) is a follow-up, not a
phase-5 feature: it adds a mid-run allocation and a stall on the first 3D frame.

> **Superseded by sprint 014 (measured).** The queued submission needs a third colour buffer: with two, the frame
> being displayed is the one just submitted, so the swap needs a full GE wait. The shipped map is **3 × 272 KiB
> colour + 272 KiB depth = 1,088 KiB**, leaving **913,344 B** free for textures (69,636 B used by the current
> corpus); without depth it is 835,584 B used and 1,191,872 B free. The table above is the pre-T5 decision; the
> measured lines are in [sprint 014 §9](../sprints/014-phase5-ceiling-vram-readback.md).

**Finding 4: what the measured case is, after sprint 009.** The ceiling is 1,600 sprites at 60 fps, held there by
`work (11.0 ms) + GE fill (~5.5 ms)` against a 16.683 ms period. A backend can move the *work* half only: it removes
pspGL's per-vertex conversion (~0.44 µs/vertex) and gives `GU_TRANSFORM_2D`/`GU_SPRITES`, but `DrawTexturePro`'s
4.95 µs of arithmetic stays exactly where it is. So the phase's own exit number must be about vertices and fill, not
about "no worse than pspGL" — which phase 4 already achieved.

### Second review (2026-11-21) — checked against the code the implementer will hit

**Finding 5: task 1 as written cannot pass its own verification.** It says "stubs that log, links with no pspGL,
boots to a **cleared screen**" — but a stub cannot clear a screen. The first milestone has to be a *vertical slice*:
egl + two framebuffers + `glClear` producing a cleared screen, stubbed everything else. Rewritten below.

**Finding 6: the boot contract is static, so nobody has to discover it.** `rlglInit()` on GL11 issues exactly this,
in this order, before any application draw:

```c
rlglInit:  glDepthFunc(GL_LEQUAL);  glDisable(GL_DEPTH_TEST);
           glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);  glEnable(GL_BLEND);
           glCullFace(GL_BACK);  glFrontFace(GL_CCW);  glEnable(GL_CULL_FACE);
           glShadeModel(GL_SMOOTH);
           glClearColor(0,0,0,1);  glClearDepth(1.0);
           glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
```

Two consequences the drop-in must honour: **backface culling is ON from boot** — our batcher emits
`(TL,BL,BR), (TL,BR,TR)` and that winding is only known-good because pspGL accepts it, so a wrong convention gives
*invisible* sprites rather than wrong ones, and a one-triangle test belongs in task 5 — and **depth testing is off**
but the **depth clear is not**: see finding 7.

**Finding 7: every frame asks to clear a depth buffer that does not exist.** `rlClearScreenBuffers()` is
`glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT)`, and `BeginDrawing()` calls it every frame. With no depth
buffer allocated, the drop-in must treat the DEPTH bit as a documented no-op — otherwise the GE is told to clear
through a null Z pointer. This makes finding 3 (§0) a blocking decision rather than a preference.

**Finding 8: `glGetFloatv` needs a CPU-side matrix mirror.** raylib queries `GL_MODELVIEW_MATRIX` and
`GL_PROJECTION_MATRIX` (rlgl.h:5031, 5059) as well as `GL_LINE_WIDTH`, `GL_POINT_SIZE` and
`GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT`. A backend that only forwarded matrices to the GE and forgot them would fail on
`rlGetMatrixModelview()`; so the stack lives in CPU/VFPU memory *and* is pushed to the GE.

**Finding 9: `glReadPixels` is real work, not a stub.** `rlReadScreenPixels()` calls
`glReadPixels(0,0,w,h,GL_RGBA,GL_UNSIGNED_BYTE,…)` under GL11 and `TakeScreenshot()`/`LoadImageFromScreen()` use it.
The framebuffer is `5650`, so the drop-in must expand 5-6-5 to 8-8-8 and flip vertically. Note our **evidence
pipeline does not test this**: `scrshot` reads VRAM through PSPLink, bypassing GL entirely, so a broken
`glReadPixels` would go unnoticed by every capture we have. It needs its own test.

**Finding 10: `eglSwapInterval` is missing from the 52 for a silly reason** — it is compiled out unless
`PSP_DISABLE_VSYNC` is defined. Implement it anyway, or that measurement switch stops building.

**Finding 11: in the default build the only vertex path is client arrays**, and it is narrow: three `*Pointer`
calls (position 3×`f32`, texcoord 2×`f32`, colour 4×`u8`), `glDrawArrays` with mode **10** (`GL_SPRITES_PSP`), **4**
(triangles) or **1** (lines), and `glDisable(GL_TEXTURE_2D)` + `glBindTexture(0)` for texture-less runs. Immediate
mode (`glBegin`/`glVertex*`) only appears in a `PSP_BATCH=0` build — 11 extra symbols, one dispatch.

**Finding 13 (found by doing it): the app link line changes.** pspGL reaches the GE through `libpspge` and
never calls GU, so our test apps never linked `libpspgu`. A GU-based driver does, so every app needs `-lpspgu`
added to `LIBS` — done for `bunnymark` and `batch_check`, and it belongs in the docs for anyone building raylib on
PSP with this driver.

**Finding 14 (found by doing it): the `--wrap` counter harness needs the immediate-mode symbols to exist.** `--wrap`
requires `__real_glBegin` and friends at link time even when nothing calls them, and under `PSP_BATCH=1` our driver
does not define them — so `psp-tests/*` link with `WRAP=0` until task 5 adds the immediate-mode entry points
(`PSP_BATCH=0` support). `psp_counters.c` also needed its `eglSwapBuffers` wrapper moved inside the `WRAP_COUNTERS`
guard for the same reason.

**Finding 15: pspgu is reproducible where pspGL was not.** Two consecutive builds give the identical content hash
(`df1646a7…`), so `tools/pspgu-build.sh --verify-repeat` can demand equality; pspGL needed a *content* hash because
two of its objects embed `__DATE__`.

**Finding 12: the viewport recipe, with numbers.** 480×272 visible inside a 512-wide buffer:
`sceGuOffset(2048 - 240, 2048 - 136)`, `sceGuViewport(2048, 2048, 480, 272)`, scissor `(0, 0, 480, 272)`. The oracle
is the first byte-identical capture against `c4eb44e7…`; anything off by a pixel shows up there and nowhere else.

## What it is worth, measured (re-priced after sprint 008)

> The sections below were written before the review above. The measurement table stands; the "fifth backend"
> framing in the first paragraph does not — see §0.

The honest case for replacing pspGL, priced with numbers rather than with "the GE could be told directly":

| gain | measured basis | size |
|---|---|---|
| per-draw interpretation | 1,025 `glEnd` = 48.8 ms of work at N=1,000, ~45 µs each | large, but **batching already removed it** (2–3 draws/frame) |
| per-vertex conversion copy | 6,150 → 2,050 vertices moved 18 % of frame work | ~0.44 µs per vertex |
| `GU_TRANSFORM_2D`, `GU_SPRITES`, 16 B vertices | unreachable from GL11 at any price | the remaining vertex work |
| no GL state machine, no texobj interpretation, own dlist | not yet quantified | — |

And what it does **not** buy: the current ceiling is **7.08 µs per sprite in raylib's own 2D path**
(`work = 0.98 ms + 7.08 µs × sprites`, sprint 008). A backend replaces the layer *underneath* that code, so it
cannot move a cost that is charged before the backend is reached. Sprint 009 attacks that number directly, for a
tenth of the work a backend costs.

Consequence for sequencing: this phase should be justified by **what GL11 cannot express** — `GU_TRANSFORM_2D`,
2-vertex sprites without a pspGL round trip, 16-byte vertices, dlist control — not by frame time it may not be able
to move, and not before the cheaper levers (sprint 009's inline path, phase 6's `5551` texture format, which halves
the bytes per texel) have been measured. **Re-price this table when sprint 009's slope lands.**

Note on GUM specifically: 2D needs **one** ortho matrix per frame, not a matrix stack per draw, so `sceGumOrtho`
enters as a handful of calls per frame and `sceGuDrawArray` does the work. GUM's VFPU stack earns its place on the
3D path, which is phase 10.

## Reference implementations to read before task 6 finishes (and before 8 and 11)

Four engines in `/home/psp/ref` do on the GE what this phase is doing. Their memory maps are the
index; the files below are what to actually read. Nothing from them is copied — three of the four
are GPL and this tree is zlib — so read for *method*, then write it our own way.

| reference | what it is | read these | answers |
|---|---|---|---|
| **PSPRuntime** (`/home/psp/ref/PSPRuntime/PSPBasicPlatformer`) | the C++ 2D/3D engine whose bunnymark we benchmark against | `src/texturemanager.cpp:35` (`LoadTexture`), `:175` (`SwizzleFast`), `getStaticVramTexture`; `src/gpumanager.cpp:124-129` (tex mode/filter/scale/offset), `:1793` (`sceGuTexImage(0, pWidth, pHeight, pWidth, data)`), `:1082`/`:1124` (8888 for glyphs) | **the upload recipe**: pad to power of two, row-copy with the padded stride, swizzle, `tbw = pWidth`, `swizzle = 1` in `sceGuTexMode`. This is the one to diff our `gl_texture.c` against |
| **PGE** (`/home/psp/ref/PGE`) | Phoenix Game Engine, 2D+3D, PNG pipeline | `pgeTexture.c` (load → swizzle → VRAM placement, `pgeTexture.h:94`), `pgeFont.c:878-881` (4-bit `GU_PSM_T4` font atlas), `pgeGfx.c:174-175` | **texture memory and font atlases**: where VRAM placement happens, what a 4-bit glyph atlas looks like, and the tex-scale/offset pair every engine sets |
| **daedalus** (`/home/psp/ref/daedalus`) | N64 emulator with a PSP GUI — the most battle-worn GU code here | `AI_MEMORY_MAP.md` §12.4 (GU submission, framebuffer formats, VRAM, swizzling), §`16-bit framebuffer` / `Fast clear + dither` rows, `Core/DMA.cpp` (`fast_memcpy_swizzle`), `NativeTexturePSP`, `CVideoMemoryManager` | **dithering and framebuffer choice**: it documents `GU_FAST_CLEAR_BIT` + dither matrices for a 5650 buffer, which is the leading explanation for our background being flat `(240,244,240)` where pspGL gives `(240,240,240)` |
| **openTRI** (`/home/psp/ref/openTRI`) | software-rasteriser-flavoured 3D engine | `triImage.c:291-391` (swizzle + VRAM), `triGraphics.c:152` (`triInit(GU_PSM_8888, 1)`) | **a second opinion on swizzle and VRAM allocation**, and what a 8888 framebuffer costs |

Notes from reading them for task 6, in the order they were tried:

- **The upload recipe differs from ours in one way that matters**: both engines **swizzle** and pass
  `swizzle = 1`. Ours uploads unswizzled with `swizzle = 0`. That is legal and texel-identical *if
  the data really is unswizzled* — but it is the first thing to check when the right texture samples
  wrong texels.
- **`sceGuTexScale(1, 1)` and `sceGuTexOffset(0, 0)`** are set by PSPRuntime (`gpumanager.cpp:128`)
  and PGE (`pgeGfx.c:174`) after every `sceGuTexMode`. **Tried, and measurably ruled out**: adding
  them to `applyTexture` changed the capture by exactly zero pixels, so GU's default texture
  transform is already identity. The calls stay (they are what every reference does, and cost
  nothing) but they are not the bug.
- **`pgeEmulator.c` is not useful here.** Despite the name it is a set of desktop shims (SDL/GL) so
  PGE can run on a PC, i.e. the *opposite* direction. Skip it.
- **Font atlases are 4-bit (`GU_PSM_T4`) in both PGE and PSPRuntime**, with a palette. Our path
  expands grey+alpha to 8888 — correct first, cheap later; a 4-bit atlas is a phase-6 memory
  decision, and worth re-reading `pgeFont.c` when we make it.

## What has to be true before starting

- [x] Phase 4 batcher works, is measured and is the default build (sprints 007–009)
- [x] **The call list is known: 52 symbols, 44 `gl*` + 8 `egl*`** — measured, not estimated (§0). Recapture it with
      `psp-nm rcore.o rshapes.o rtextures.o rtext.o rmodels.o | grep ' U ' | awk '{print $2}' | grep -E '^(gl|egl)' |
      sort -u` against whatever build is current; it changes if raylib's modules change
- [x] Phase 4's captures exist as the oracle: `/home/psp/evidence/sprint009/` (`c4eb44e7…` 2D, `58bcbb9b…` 3D scene),
      `/home/psp/evidence/sprint008/` for the `-O2` pair
- [x] Framebuffer format (§0 finding 2): **`GU_PSM_5650`**, to stay pixel-identical; `5551` would shift every capture
- [x] Depth (§0 finding 3): **allocated, 16-bit, 272 KiB, behind `PSP_GU_DEPTH ?= 1`** — matches today, keeps 3D open,
      and a 2D-only build can switch it off
- [x] A worst-case display-list size measured from a real frame — at the ceiling (N=2,375) the list high-water is
      **1,456 bytes** of 245,760; the per-frame vertex high-water is ≤91 KiB of the 128 KiB arena

## Tasks (reviewed 2026-11-21, after tasks 1/4/5/6 came back from the device)

State vocabulary: `done` · `doing` · `todo` · `dropped`. Numbers are stable; new work gets new numbers so the
history stays readable.

| # | task | state | what the device said |
|---|---|---|---|
| 1 | vertical slice: the 52 symbols, real egl init, two framebuffers, GU display, `glClear` | `done` | boots, clears to the pspGL value, **0 pspGL symbols** |
| 2 | egl layer: config/context/surface/swap/interval, DEPTH bit of `glClear` | `done` | batched frames swap at the display rate with `batch_check` and `texture_pattern`; the `PSP_DISABLE_VSYNC` path is sprint 013 T5 |
| 3 | state map: every `glEnable` bit the library sets, blend/depth/cull/masks/shade/viewport/scissor | `done` | mapped; the library uses ten enable bits, not the twenty GL has |
| 3b | dither: match pspGL's background pattern | `done` | `sceGuEnable(GU_DITHER)`; the background pattern now matches and it was half of the frame's difference (sprint 012 T5) |
| 4 | matrices in CPU memory + pushed to the GE, `glGetFloatv` for both queries | `done` | a raw full-screen self-test sprite fills the display: transform proven. 3D path still unverified |
| 5 | vertex arrays: fast interleaved path, converting path, `glDrawElements`, modes 10/4/1 | `done` | sprites draw; the fast path engages (`fast=1` in the trace) |
| 6 | textures: gen/bind/delete/image/subimage/parameters, VRAM arena, white default | `done` | **byte-identical to pspGL** (613 colours, 0 differing pixels) after the four fixes in sprint 012 |
| 6b | texture parity: pattern test, swizzle recipe, alpha, dither | `done` | [sprint 012](../sprints/012-phase5-texture-parity.md) — the pattern test cut three deltas to one, and the last was our own LA-decode bug. **0 differing pixels** |
| 6c | non-power-of-two textures: pad the height and correct with `sceGuTexScale(realW/padW, realH/padH)` | `done` | closed in phase 6, [sprint 015](../sprints/015-phase6-npot-and-vram.md) T1: both dimensions pad to powers of two, the padding is zeroed and swizzled, and TexScale is set per texture. 33×17 checkerboard **281/561 → 0/561** wrong cells; `batch_check` and `basic_shapes` unchanged. This also retires the "TexScale is identity" claim, which holds only for power-of-two textures |
| 7 | readback: `glReadPixels` expanding 5650→RGBA8888 and flipping, plus `glGetString` | `done` | stopped-frame PNG vs `scrshot`: max diff **7**, 0 pixels >8, two captures identical. The framebuffer is RGB565 and `scrshot` writes BGR, so the comparison swaps its channels |
| 8 | display lists: two rotating, sized from a measured worst case, `GU_SYNC_WHAT_QUEUED` on the previous frame | `done` | **2,375 sprites**, queued submission, three framebuffers, per-list 128 KiB arenas; list high-water 1,456 B; targeted waits 18–26 µs; no full GE wait |
| 9 | cache discipline before the GE reads vertices or texels | `done` | it was **blocking**, not an optimisation: without it the GE read stale vertices and drew nothing |
| 10 | VRAM accounting at boot: framebuffers, depth, textures, free | `done` | one line from allocation counters: depth=1 `used=1,183,808 free=913,344`, depth=0 `used=905,280 free=1,191,872`; totals add up |
| 11 | depth switch `PSP_GU_DEPTH`, DEPTH clear bit a no-op when 0 | `done` | depth=0 boots, reserves no depth, the 2D gate is byte-identical; `glClear(GL_DEPTH_BUFFER_BIT)` drops the bit |
| 12 | **immediate-mode symbols** — new: `glBegin`/`glEnd`/`glVertex2i/2f/3f`/`glTexCoord2f`/`glColor4ub/3f/4f`/`glNormal3f`. Needed by the `--wrap` counter harness (it requires `__real_*` to exist) and by any `PSP_BATCH=0` library | `done` | sprint 013: `WRAP=1` links and runs; `PSP_BATCH=0` reports non-zero `begins`/`verts` |
| 13 | **remove the stubs** — new: once 6b/7/12 land, `gl_stub.c` should hold nothing; any remaining call logs once and asserts in a debug build instead of silently doing nothing | `done` | `gl_stub.c` deleted; `gl_query.c` holds the real symbols; 52/52 required symbols are supplied |

**Dropped:** nothing. Every original task survived the review; the list grew by five (3b, 6b, 6c, 12, 13) because
doing the work found work. All are done. The last one, **6c** (non-power-of-two textures), was closed in phase 6
by sprint 015 T1: pad both dimensions to powers of two, zero and swizzle the padding, and set
`sceGuTexScale(realW/padW, realH/padH)`.


## Sprints

Four sprints, in order. 011 and 012 are done and written up; 013 is written out in full because it is next;
014 is scoped here and gets its page when it starts.

| sprint | goal | state | exit |
|---|---|---|---|
| [011 — GE up and the vertex path](../sprints/011-phase5-ge-up.md) | framebuffers, clear, matrices, both draw paths, cache discipline | **done** | no pspGL symbols; a self-test sprite fills the screen; five bugs recorded |
| [012 — Texture parity](../sprints/012-phase5-texture-parity.md) | pattern test, `GU_TCC_RGBA` alpha, `GU_DITHER`, the LA-decode bug, swizzle | **done** — parity reached | `batch_check` 2D **byte-identical** to pspGL: 0 differing pixels, 613 colours |
| [013 — Corpus parity and the counter harness](../sprints/013-phase5-corpus-and-counters.md) | mesh scene, `basic_shapes`, the immediate-mode symbols (task 12), `--wrap` counters, `PSP_DISABLE_VSYNC` | **done** — 3D split off | `batch_check` and `basic_shapes` **byte-identical**; `WRAP=1` links and runs; vsync switch works |
| [014 — Ceiling, VRAM and readback](../sprints/014-phase5-ceiling-vram-readback.md) | rotating lists with queued sync, VRAM accounting, `PSP_GU_DEPTH=0`, `glReadPixels`/`TakeScreenshot`, stubs gone | **done** | **2,375 sprites** at 60 fps (pspGL 1,600); 2D gate 0; VRAM accounted in both depth configs; no full GE wait; no stubs; depth-correct cubes |

Where the phase stands in one paragraph: the driver is **byte-identical to pspGL on the 2D corpus** — `batch_check`
and `basic_shapes` both to 0 differing pixels, textures, alpha, dither, swizzle and all — and it now **beats pspGL's
ceiling by 48 %**: 2,375 sprites at 60 fps against 1,600, with no full GE wait in a steady frame. The display runs
three framebuffers so a completed frame is shown while the next is submitted, and each list owns a 128 KiB vertex
arena aligned to the 64-byte cache line. VRAM is accounted at boot in both depth configurations. The one criterion
not met is the 2D→3D→2D scene: `DrawMesh`/`glDrawElements` emits a draw and produces no pixels. That is **deferred to
phase 10** by recorded decision, not redefined; the overlapping-cubes depth probe that phase 5 did own passes. The
remaining known defect is non-power-of-two textures (task 6c), carried into phase 6.

### What it cost — six bugs, and what each one looked like from outside

Kept because five of the six presented identically ("nothing is drawn" or "garbage is drawn") while the clear kept
working, which is the most expensive failure mode this phase has.

| # | bug | how it presented |
|---|---|---|
| 1 | framebuffer size computed before the struct was filled | both buffers at offset 0; a single-buffered driver still clears, so it would have shipped |
| 2 | `glEnableClientState` bits never read by the draw path | fast path never engaged; the vertex format lost colour and texcoords |
| 3 | no dcache writeback before the GE reads vertices | nothing drawn at all — **blocking**, not an optimisation (task 9) |
| 4 | GU's view matrix undefined after `sceGuInit` | everything collapsed to a point |
| 5 | `sceGuSetMatrix` takes column-major, no transpose | geometry collapsed to a few pixels near the screen centre — looks like a projection bug, is not |
| 6 | LUMINANCE_ALPHA decoded as RGB565 (branched on byte count, not type) | sprite colours nonsense; the last 32.6 % of the frame |

## What we already know

- 2 MiB EDRAM; 816 KiB for two 5551 framebuffers plus depth, of which **272 KiB is depth we do not need**
- Vertex colour is `0xAABBGGRR`; s16 positions are normalised; matrices are float24; there is no X/Y clip
- `sceGuGetMemory` has no capacity check and never returns NULL — the guards people write are dead code
- The reference engine reached 1024 sprites per call with 16 B / 24 B vertex structs and float positions

## Exit criteria

- [x] the ELF contains **no pspGL objects**: `psp-nm app.elf | grep -c pspgl_` is 0, and `-lGL` is no longer what
      supplies `gl*` — verified; the installed archive reports 0 pspGL objects
- [x] `batch_check` plain scene is byte-identical to the phase-4 archive — **0 differing pixels** with depth=1 and
      depth=0
- [ ] `batch_check` **2D → `DrawCube` + `DrawMesh` → 2D** is byte-identical — **not met; deferred to phase 10 by
      recorded decision.** `glDrawElements` emits and produces no pixels (sprint 013 T1 split). The decision is
      carried in the Phase 10 page; this box stays unticked because the scene is not identical, not because the
      phase failed
- [x] `basic_shapes` renders with the same 284-colour fingerprint — **284 colours, 0 differing pixels** on both
- [x] the 60 fps ceiling **beats 1,600 sprites**, by bisection — **2,375 pass / 2,376 fail**, 2.432 M texels/frame
- [x] no full GE wait in a steady-state frame (counters, not feel) — targeted waits 18–26 µs
- [x] VRAM high-water documented against the 2 MiB budget, with and without `PSP_GU_DEPTH` — 1,183,808 B / 905,280 B,
      adding up from the allocation counter
- [x] 3D depth-correct with `PSP_GU_DEPTH=1` (overlapping cubes) — near-red drawn first holds the 6,360-px overlap;
      depth=0 makes the same pixels far-blue

**Not** an exit criterion: "frame time ≤ best pspGL result". Phase 4 already achieved that (16.683 ms at N=1,000)
without any of this work, so it cannot certify it.

## Sprint index

- [011 — GE up and the vertex path](../sprints/011-phase5-ge-up.md) — **done**
- [012 — Texture parity](../sprints/012-phase5-texture-parity.md) — **done**
- [013 — Corpus parity and the counter harness](../sprints/013-phase5-corpus-and-counters.md) — **done**
- [014 — Ceiling, VRAM and readback](../sprints/014-phase5-ceiling-vram-readback.md) — **done**
