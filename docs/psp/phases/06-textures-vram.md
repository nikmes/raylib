# Phase 6 — Textures and VRAM

| | |
|---|---|
| State | **in progress** — sprint 010 (probe) proved texture format is not a speed lever; the rest needs phase 5 to own VRAM |
| Depends on | Phase 5 (a backend that owns the upload path) |
| Exit criterion | no eviction when the working set fits in VRAM, upload benchmarks by format, fragmentation bounded under load/unload churn |

## What this phase is about

The 2 MiB of EDRAM is the tightest budget in the project, and texture residency is where a stable frame turns
into a stutter. Upload cost, swizzle correctness and eviction policy are one subject: a texture that has to be
re-uploaded mid-frame costs more than the draw it feeds.

## What has to be true before starting

- [ ] Phase 5 backend uploads textures and passes the samples
- [ ] Boot-time VRAM accounting exists (free bytes, largest block)
- [ ] Upload/eviction counters exist

## Tasks

- [ ] Swizzle **directly into the destination** — no memcpy to VRAM, no `sceGuCopyImage`
- [ ] POT padding on load; document the padding cost rather than hiding it
- [ ] CLUT path for glyph atlases (T4/T8) only if measured to pay
- [ ] Best-fit allocator over 512-byte blocks, coalescing on free, free bytes and largest block tracked
      separately — one allocator for framebuffers *and* textures
- [ ] Residency: priority-aware eviction where application priority is a strong weight; usage that ages per
      frame, never accumulating forever
- [ ] Range-based cache writeback on upload; no whole-cache invalidate in a steady frame
- [ ] No mipmaps unless a measurement says they pay
- [ ] Per-texture PSM, not a global swizzle mode (the global mode forces every texture to match the framebuffer)

## What we already know

- `vt5551.q` / `vt5650.q` convert **four RGBA8 pixels per cycle** — the scalar per-pixel loop in the reference
  engine is the thing to delete
- The reference `SwizzleFast` silently produces a blank texture when width × pixel size < 16 or `height % 8 != 0`
  — pad to whole 16×8 blocks or fall back to unswizzled
- Its allocator is best-fit with coalescing and a 4096-entry table (16 KiB, cache-resident); the same repo also
  ships a second bump allocator, and the two can hand out overlapping ranges. One allocator only
- `free()` on an EDRAM pointer is invalid — `vfree()` or nothing

## Exit criteria

- [ ] no eviction or compaction in a workload whose working set fits
- [ ] upload/swizzle benchmarked by pixel format and size
- [ ] fragmentation bounded across repeated load/unload
- [ ] priority changes demonstrably alter eviction order

## Sprints and features

None yet. Use the [sprint template](../templates/sprint.md).

## Sprints and features

- [Sprint 010 — What fill actually costs, and 16-bit textures](../sprints/010-phase6-fill-and-texture-format.md)
  — **done, negative result.** Fill is rasterisation-rate-bound (~221 Mtexel/s): 16-bit textures changed the frame
  by 0.0 %, blending off by −0.2 %, 4× the area by +199.5 %. The budget is **~1.6–2.0 M drawn texels per 60 fps
  frame**, not a sprite count. 16-bit textures survive as a **memory** lever (2 B/texel in a 2 MiB budget), with
  `4444` for sprites and `5650` for opaque — the speed case for them is dead
