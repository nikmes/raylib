# Phase 5 — Native GU backend

| | |
|---|---|
| State | not started — **reviewed after sprint 009, shape decided (§0)** |
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

**Finding 3: dropping the depth buffer drops 3D.** `BeginMode3D` enables `GL_DEPTH_TEST`; with no depth buffer
every 3D sample breaks. Sprint 009's 2D→`DrawCube`+`DrawMesh` scene is the regression test. Either ship phase 5 as a
2D-only backend with `BeginMode3D` documented as unsupported, or budget the +272 KiB depth buffer and keep 3D. This
must be decided before the first line — it changes the VRAM accounting the phase exits on.

**Finding 4: what the measured case is, after sprint 009.** The ceiling is 1,600 sprites at 60 fps, held there by
`work (11.0 ms) + GE fill (~5.5 ms)` against a 16.683 ms period. A backend can move the *work* half only: it removes
pspGL's per-vertex conversion (~0.44 µs/vertex) and gives `GU_TRANSFORM_2D`/`GU_SPRITES`, but `DrawTexturePro`'s
4.95 µs of arithmetic stays exactly where it is. So the phase's own exit number must be about vertices and fill, not
about "no worse than pspGL" — which phase 4 already achieved.

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
- [ ] The framebuffer format decision (§0 finding 2): `GU_PSM_5650` to stay pixel-identical, and a written note that
      `5551` would shift every capture
- [ ] The depth decision (§0 finding 3): 2D-only, or +272 KiB and 3D kept — this sets the VRAM budget the phase exits on
- [ ] A worst-case display-list size measured from a real frame (N=5,000 bunnymark = 9 draws, 30,150 vertices), not
      guessed from the GL11 path's 1,025 submissions

## Tasks

The deliverable is a **drop-in `libGL.a`-shaped implementation of the 52 symbols** that the library actually needs
(§0), written over `sceGu`/`sceGum`, linked instead of pspGL. Tasks are ordered so the thing links, then draws, then
optimises.

| # | task | verification |
|---|---|---|
| 1 | **skeleton and link**: the 52 symbols, each a stub that logs and returns; `make PLATFORM=PLATFORM_PSP` links and boots to a cleared screen with **no pspGL objects** in the ELF | `psp-nm app.elf \| grep -c pspgl_` is 0; the stubs' log shows the boot path's call order, which is the de-facto initialisation sequence |
| 2 | **egl layer**: config, context, surface, `eglSwapBuffers` with the same vsync semantics as today (bunnymark times the swap), `eglGetError` | `psp-tests/bunnymark` boots and reaches `DONE` with counters intact; `SPLIT ... swap=` non-zero |
| 3 | **state mapping**: a table of every `glEnable`/`glDisable` bit the library uses, plus `glBlendFunc`, `glDepthFunc/Mask`, `glColorMask`, `glCullFace`, `glFrontFace`, `glScissor`, `glViewport`, `glShadeModel`, `glPolygonMode`, `glLineWidth` → GU commands, emitted only when the value changes | the state trace of one `batch_check` frame matches the 31 flush points the batcher has; no command re-emitted with the same value |
| 4 | **matrices**: `glMatrixMode`, `glLoadIdentity`, `glPushMatrix`, `glPopMatrix`, `glMultMatrixf`, `glOrtho`, `glFrustum`, `glRotatef`, `glScalef`, `glTranslatef` over the VFPU (GUM or hand-written), composed and pushed as float24 | `batch_check` 2D scene identical; the camera-relative 3D scene from sprint 009 renders without trembling |
| 5 | **vertex arrays**: the four `*Pointer` calls, `glEnable/DisableClientState`, `glDrawArrays`, `glDrawElements`, with the 24-byte layout as the native fast path (it already is pspGL's native GE layout) and per-attribute conversion for everything else. `GL_SPRITES_PSP` (mode 10) → `GU_SPRITES` | `arrayVerts`/frame unchanged; the same batch reaches the GE without a copy for our own vertices |
| 6 | **textures**: `glGenTextures`, `glBindTexture`, `glTexImage2D`, `glTexSubImage2D`, `glTexParameteri`, `glPixelStorei`, `glDeleteTextures` — swizzling, format conversion, VRAM allocation and eviction | the same textures load; a swizzle bug shows as diagonal garbage, so compare a 32×32 and a 512×512 texture capture |
| 7 | **readback and queries**: `glReadPixels` (`TakeScreenshot`), `glGetFloatv`, `glGetString` | `TakeScreenshot` produces a valid PNG; `glGetString(GL_VERSION)` returns something non-NULL so rlgl's init logs do not crash |
| 8 | **display lists**: two rotating, 16-byte aligned, sized from a measured worst case; submit frame N, sync frame N-1 (`GU_SYNC_WHAT_QUEUED`); never a per-frame `FINISH` | `SPLIT work/swap` shows no full wait in steady state; a deliberately undersized list asserts instead of overrunning |
| 9 | **cache discipline**: `sceKernelDcacheWritebackRange()` over exactly the ranges the CPU wrote, then publish the pointer | a texture uploaded then drawn in the same frame is correct on hardware (this is the classic PSP-only corruption) |
| 10 | **VRAM accounting at boot**: framebuffers, textures, free bytes, largest block | printed once at boot and matching the budget in §"What we already know" |
| 11 | **the depth decision** (§0 finding 3): 2D-only with `BeginMode3D` documented unsupported, **or** +272 KiB depth and 3D kept | written down before task 1, and the 3D scene either passes or is explicitly out of scope |

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
- [ ] VRAM high-water documented against the 2 MiB budget, including the depth decision
- [ ] the 3D verdict: supported, or explicitly unsupported with `BeginMode3D` documented

**Not** an exit criterion: "frame time ≤ best pspGL result". Phase 4 already achieved that (16.683 ms at N=1,000)
without any of this work, so it cannot certify it.

## Sprints and features

None yet. Use the [sprint template](../templates/sprint.md).
