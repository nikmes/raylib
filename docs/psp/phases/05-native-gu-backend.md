# Phase 5 — Native GU backend

| | |
|---|---|
| State | not started |
| Depends on | Phase 4 (a batcher to port, and screenshots to match) |
| Exit criterion | identical output to phase 4 with no OpenGL and no pspGL in the path, frame time no worse than the best pspGL result, VRAM accounting printed at boot |

## What this phase is about

Removing the middleman. `pspGL` is a GL 1.1 implementation over the GE — capable, but a 2007-era one, and
every GL call it serves is an interpretation of something the GE could be told directly. raylib already
supports this shape of work: `GRAPHICS_API_OPENGL_SOFTWARE` is nothing but `#define GRAPHICS_API_OPENGL_11`,
which is raylib's own admission that "rlgl speaks GL 1.1, and whatever library supplies those symbols is the
backend". A `GRAPHICS_API_PSP_GU` backend replaces the supplier.

## What it is worth, measured (re-priced after sprint 008)

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

- [ ] Phase 4 batcher works and is measured
- [ ] The GL 1.1 call list actually used by rlgl is known (147 distinct entry points exist; the GL11 path uses
      a subset) — that list is the specification
- [ ] Phase 4's screenshots exist as the correctness oracle

## Tasks

- [ ] New backend selected by `GRAPHICS_API_PSP_GU`; platform layer untouched (it only asks for a window,
      input and swap)
- [ ] Two rotating display lists, 16-byte aligned, sized from a real worst case; submit frame N, sync frame N-1
      (`GU_SYNC_WHAT_QUEUED`) — no per-frame `FINISH` stall
- [ ] Per-list vertex arenas with explicit lifetimes: never rewrite memory the GE may still read
- [ ] Framebuffer `GU_PSM_5551`, 512-px stride for a 480-wide view, **no depth buffer**
- [ ] Pixel-exact 2D: `sceGuOffset(2048 - W/2, 2048 - H/2)`, `sceGuViewport(2048, 2048, W, H)`,
      `sceGumOrtho(0, W, H, 0, -1, 1)`
- [ ] Camera-relative transforms on any 3D path (float24)
- [ ] `sceKernelDcacheWritebackRange()` over exactly the ranges the CPU wrote, then publish
- [ ] Boot-time VRAM accounting: framebuffers, textures, free bytes, largest block
- [ ] Assert on list capacity instead of relying on `sceGuGetMemory` (it cannot fail, it overruns)

## What we already know

- 2 MiB EDRAM; 816 KiB for two 5551 framebuffers plus depth, of which **272 KiB is depth we do not need**
- Vertex colour is `0xAABBGGRR`; s16 positions are normalised; matrices are float24; there is no X/Y clip
- `sceGuGetMemory` has no capacity check and never returns NULL — the guards people write are dead code
- The reference engine reached 1024 sprites per call with 16 B / 24 B vertex structs and float positions

## Exit criteria

- [ ] output matches phase 4 within an explained margin
- [ ] frame time ≤ best pspGL result at every bunny count — **already true after sprint 008** (16.683 ms at
      N=1,000), so this alone cannot justify the phase: see the re-pricing above
- [ ] no full GE wait in a steady-state frame (counters, not feel)
- [ ] VRAM high-water documented against the budget

## Sprints and features

None yet. Use the [sprint template](../templates/sprint.md).
