# Phase 6 — Textures and VRAM

| | |
|---|---|
| State | not started |
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
  — phase 4's ceiling is `work 11.0 ms + fill ~5.5 ms` in a 16.683 ms period, and only the first half has been
  attacked. This sprint finds out whether fill is texel bytes (16-bit uploads fix it), pixel area (fewer sprites
  fix it) or blend traffic (opaque passes fix it) before anything is implemented, then bisects the ceiling again
