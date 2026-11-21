# Phase 5 — Native GU backend

| | |
|---|---|
| State | **reviewed and implementation-ready, not started** — deferred: the binding constraint moved to fill, which a backend does not touch (§0) |
| Depends on | Phase 4 closed (batcher shipped, ceiling measured at 1,600 sprites) |
| Exit criterion | the 60 fps ceiling **beats** phase 4's, not merely matches it, with no pspGL symbols in the ELF and VRAM accounted at boot |

## What this phase is about

Removing the middleman. `pspGL` is a GL 1.1 implementation over the GE — capable, but a 2007-era one, and
every GL call it serves is an interpretation of something the GE could be told directly. raylib already
supports this shape of work: `GRAPHICS_API_OPENGL_SOFTWARE` is nothing but `#define GRAPHICS_API_OPENGL_11`,
which is raylib's own admission that "rlgl speaks GL 1.1, and whatever library supplies those symbols is the
backend". A `GRAPHICS_API_PSP_GU` backend replaces the supplier.

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
- [ ] A worst-case display-list size measured from a real frame (N=5,000 bunnymark = 9 draws, 30,150 vertices), not
      guessed from the GL11 path's 1,025 submissions

## Tasks

The deliverable is a **drop-in `libGL.a`-shaped implementation of the 52 symbols** that the library actually needs
(§0), written over `sceGu`/`sceGum`, linked instead of pspGL. Tasks are ordered so the thing links, then draws, then
optimises.

| # | task | verification |
|---|---|---|
| 1 | **vertical slice**: the 52 symbols as logging stubs **plus** real egl init, two framebuffers, GU display setup and `glClear` | `done` — `tools/pspgu/{pspgu_internal.h,egl.c,gl_state.c,gl_stub.c}` + `tools/pspgu-build.sh`. Boot log: `GU up: 480x272 stride 512, fb 272 KiB x2, depth yes (272 KiB), textures from 816 KiB, list 256 KiB`; `psp-nm batch_check.elf \| grep -c pspgl_` = **0**; capture is a uniform `(240,244,240)`, the same RAYWHITE-through-5650 value a pspGL build produces |
| 2 | **egl layer**: config, context, surface, `eglSwapBuffers` with the same vsync semantics as today, `eglSwapInterval` (compiled out of the 52 but needed by `PSP_DISABLE_VSYNC`), `eglGetError`; and the DEPTH bit of `glClear` handled explicitly (§0 finding 7) | `psp-tests/bunnymark` boots and reaches `DONE` with counters intact; `SPLIT ... swap=` non-zero; a `PSP_DISABLE_VSYNC=1` build still links |
| 3 | **state mapping**: a table of every `glEnable`/`glDisable` bit the library uses, plus `glBlendFunc`, `glDepthFunc/Mask`, `glColorMask`, `glCullFace`, `glFrontFace`, `glScissor`, `glViewport`, `glShadeModel`, `glPolygonMode`, `glLineWidth` → GU commands, emitted only when the value changes | the state trace of one `batch_check` frame matches the 31 flush points the batcher has; no command re-emitted with the same value |
| 4 | **matrices**: `glMatrixMode`, `glLoadIdentity`, `glPushMatrix`, `glPopMatrix`, `glMultMatrixf`, `glOrtho`, `glFrustum`, `glRotatef`, `glScalef`, `glTranslatef` in CPU memory (scalar floats for now — VFPU is phase 10's concern), **and** pushed to the GE, with `glGetFloatv` answering both queries (§0 finding 8) — `TODO: done in `gl_matrix.c`; a full-screen self-test sprite now covers the display, which is the transform proof | `batch_check` 2D scene identical; the camera-relative 3D scene from sprint 009 renders without trembling; `rlGetMatrixModelview()` returns a sane matrix |
| 5 | **vertex arrays**: the four `*Pointer` calls, `glEnable/DisableClientState`, `glDrawArrays`, `glDrawElements`, with the 24-byte layout as the native fast path (it already is pspGL's native GE layout) and per-attribute conversion for everything else. `GL_SPRITES_PSP` (mode 10) → `GU_SPRITES`; mode 1/4 for lines/triangles. **Verify the winding against the culling that is on from boot** (§0 finding 6) | `arrayVerts`/frame unchanged; the batch reaches the GE without a copy for our own vertices; one sprite renders — if culling is wrong the sprite is *invisible*, not distorted |
| 6 | **textures**: `glGenTextures`, `glBindTexture`, `glTexImage2D`, `glTexSubImage2D`, `glTexParameteri`, `glPixelStorei`, `glDeleteTextures` — swizzling, format conversion, VRAM allocation and eviction | the same textures load; a swizzle bug shows as diagonal garbage, so compare a 32×32 and a 512×512 texture capture |
| 7 | **readback**: `glReadPixels` expanding the `5650` framebuffer to RGBA8888 and flipping, plus `glGetString`. `scrshot` reads VRAM through PSPLink and proves nothing about this path (§0 finding 9) | `TakeScreenshot` produces a PNG whose pixels match a `scrshot` capture of the same frame within the 5-6-5 quantisation; `glGetString(GL_VERSION)` is non-NULL |
| 8 | **display lists**: two rotating, 16-byte aligned, sized from a measured worst case; submit frame N, sync frame N-1 (`GU_SYNC_WHAT_QUEUED`); never a per-frame `FINISH` | `SPLIT work/swap` shows no full wait in steady state; a deliberately undersized list asserts instead of overrunning |
| 9 | **cache discipline**: `sceKernelDcacheWritebackRange()` over exactly the ranges the CPU wrote, then publish the pointer | a texture uploaded then drawn in the same frame is correct on hardware (this is the classic PSP-only corruption) |
| 10 | **VRAM accounting at boot**: framebuffers, textures, free bytes, largest block | printed once at boot and matching the budget in §"What we already know" |
| 11 | **the depth switch**: allocate the 16-bit depth buffer at 512 stride when `PSP_GU_DEPTH=1` (default), treat `GL_DEPTH_BUFFER_BIT` inside `glClear` as a no-op when it is 0, and report the choice in the boot VRAM line | both configurations build and boot; the 3D scene is depth-correct with the buffer and painter-ordered without it, and the boot line shows the 272 KiB either way |

### Tasks 4-5 progress (2026-21) — five bugs, four of them invisible-until-drawn

The matrix stack and the two draw paths (`gl_draw.c`: fast path for the batcher's interleaved
24-byte vertices, converting path for `DrawMesh`'s separate arrays, plus `glDrawElements`) are
written and reach the GE. Getting the first pixels on screen cost five bugs, and each one looked
like "the driver draws nothing" while the clear kept working:

1. **Framebuffer size computed before the struct was filled** (task 1) — both buffers at offset 0.
   The boot log line caught it; a single-buffered driver still clears correctly, so it would
   have shipped.
2. **`glEnableClientState` recorded bits the draw path never read** — the fast path never
   engaged and the format lost its colour and texture bits.
3. **No data-cache writeback before the GE reads vertices.** `sceGuClear` carries its colour in
   the command list and works; vertices are read from main RAM, so without
   `sceKernelDcacheWritebackRange` the GE reads stale memory. This is task 9 in the list above
   and it is **blocking**, not an optimisation (now in `pspguWriteback`).
4. **GU's matrices are undefined after `sceGuInit`** — the view matrix has to be set to identity
   explicitly. Samples always set all three; skipping one collapses every vertex.
5. **`sceGuSetMatrix` wants the matrix in column-major memory order — no transpose.** Sending
   the GL matrix transposed put the ortho's translation in a `w` row, so every vertex was
   divided by it: geometry collapsed to a few pixels around the screen centre. This is the one
   that looks like a projection bug and is not.

State: with a raw self-test sprite the whole display fills (0,0,0)→(480,272) correctly. Through
raylib, geometry now reaches the GE but the screen goes **black** once `GL_TEXTURE_2D` is
enabled with no texture bound — the GE samples from an undefined address. That is task 6, and
until it exists the driver is not usable.

### Task 1 in detail — where the code goes and what order

- **Source lives in `tools/pspgu/`**, built and installed by `tools/pspgu-build.sh` modelled on
  `tools/pspgl-build.sh` (build → install over `$PSPDEV/psp/lib/libGL.a` → `--check`). That keeps the A/B identical
  to sprint 005/006: swap one archive, run the same binaries, compare captures. Nothing in `src/` changes.
- **Keep pspdev's headers.** We implement the *symbols*, not the headers — `GL/gl.h`, `GL/glext.h` and `GLES/egl.h`
  stay as pspGL left them, so the library and the tests compile unchanged. Forking headers would double the surface
  for no gain.
- **Files, in the order they get written:** `egl.c` (display/config/context/surface/swap + framebuffers + the boot
  sequence of finding 6) → `gl_state.c` (enable/disable, blend, depth, cull, scissor, viewport, clear) →
  `gl_matrix.c` (the ten matrix calls + `glGetFloatv`) → `gl_draw.c` (the three `*Pointer` calls, client state,
  `glDrawArrays`/`glDrawElements`, modes 10/4/1) → `gl_texture.c` (gen/bind/image/subimage/parameters) →
  `gl_query.c` (`glGetString`, `glGetFloatv`, `glReadPixels`). The first two files are the vertical slice; the rest
  are stubs until their task comes up.
- **First acceptance, before any drawing:** swap the archive, run `psp-tests/batch_check` unmodified, and expect a
  cleared screen in the requested colour, `psp-nm <elf> | grep -c pspgl_` at 0, and the boot log showing the eleven
  init calls in order. Drawing comes after that.

## What we already know

- 2 MiB EDRAM; 816 KiB for two 5551 framebuffers plus depth, of which **272 KiB is depth we do not need**
- Vertex colour is `0xAABBGGRR`; s16 positions are normalised; matrices are float24; there is no X/Y clip
- `sceGuGetMemory` has no capacity check and never returns NULL — the guards people write are dead code
- The reference engine reached 1024 sprites per call with 16 B / 24 B vertex structs and float positions

## Exit criteria

- [ ] the ELF contains **no pspGL objects**: `psp-nm app.elf | grep -c pspgl_` is 0, and `-lGL` is no longer what
      supplies `gl*`
- [ ] `batch_check` plain scene **and** the 2D → `DrawCube` + `DrawMesh` → 2D scene are byte-identical to the phase-4
      archives (`c4eb44e7…`, `58bcbb9b…`) — which requires keeping `GU_PSM_5650`
- [ ] `basic_shapes` renders with the same 284-colour fingerprint
- [ ] the 60 fps ceiling **beats 1,600 sprites**, by bisection; if fill stops it below 2,000, that is the finding and
      it is written down with the fill rate
- [ ] no full GE wait in a steady-state frame (counters, not feel)
- [ ] VRAM high-water documented against the 2 MiB budget, with and without `PSP_GU_DEPTH`
- [ ] 3D depth-correct with `PSP_GU_DEPTH=1` (the sprint-009 scene with **overlapping** cubes, which is the test the current one lacks)

**Not** an exit criterion: "frame time ≤ best pspGL result". Phase 4 already achieved that (16.683 ms at N=1,000)
without any of this work, so it cannot certify it.

## Sprints and features

None yet. Use the [sprint template](../templates/sprint.md).
