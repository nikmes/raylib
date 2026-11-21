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

- [x] Phase 5 backend uploads textures and passes the samples — pspgu, 2D corpus byte-identical
- [x] Boot-time VRAM accounting exists **for the free tail** — sprint 014 reports used/free/peak from the
      allocation counter. The **largest block** does not exist yet: the allocator is a bump arena. It arrives
      with T3 of sprint 015, and until then no “largest free block” may be quoted
- [x] Upload/eviction counters exist — sprint 015 T3 counts allocations, frees, failures, requested bytes, used,
      free and largest block. Eviction still does not exist; sprint 016 adds it

## Tasks

- [ ] Swizzle **directly into the destination** — no memcpy to VRAM, no `sceGuCopyImage`
- [x] POT padding on load; document the padding cost rather than hiding it — **sprint 015 T1**: both dimensions
      pad to powers of two, the padding is zeroed and swizzled, and `sceGuTexScale` restores the logical size.
      33×17 → 64×32 = 8,192 B (logical 2,244 B); the 33×17 checkerboard went from 281/561 wrong cells to 0/561,
      and the parity scenes stayed byte-identical
- [ ] CLUT path for glyph atlases (T4/T8) only if measured to pay
- [x] Best-fit allocator over 512-byte blocks, coalescing on free, free bytes and largest block tracked
      separately — one allocator for framebuffers *and* textures — **sprint 015 T3**: 4,096 blocks over 2 MiB,
      32 KiB metadata, framebuffers/depth as fixed 8 KiB-aligned reservations, live `requested` and `internal`.
      Churn 100 × 16 textures: free and largest return exactly (916,992 → 916,992 B), 1,600 frees, 0 failures
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

- [Sprint 010 — What fill actually costs, and 16-bit textures](../sprints/010-phase6-fill-and-texture-format.md)
  — **done, negative result.** Fill is rasterisation-rate-bound (~221 Mtexel/s): 16-bit textures changed the frame
  by 0.0 %, blending off by −0.2 %, 4× the area by +199.5 %. The budget is **~1.6–2.0 M drawn texels per 60 fps
  frame**, not a sprite count. 16-bit textures survive as a **memory** lever (2 B/texel in a 2 MiB budget), with
  `4444` for sprites and `5650` for opaque — the speed case for them is dead.
- [Sprint 015 — NPOT textures, per-texture format and the VRAM allocator](../sprints/015-phase6-npot-and-vram.md)
  — **in progress.** T1 is the known 33×17 padding plus `sceGuTexScale`; T2 halves texture bytes; T3 replaces the
  bump arena with a best-fit allocator that can report a real largest block; T4 swizzles directly into VRAM.
