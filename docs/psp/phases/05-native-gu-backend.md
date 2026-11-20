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
- [ ] frame time ≤ best pspGL result at every bunny count
- [ ] no full GE wait in a steady-state frame (counters, not feel)
- [ ] VRAM high-water documented against the budget

## Sprints and features

None yet. Use the [sprint template](../templates/sprint.md).
