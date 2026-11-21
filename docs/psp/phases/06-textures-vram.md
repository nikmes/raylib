# Phase 6 — Textures and VRAM

| | |
|---|---|
| State | **done** — sprint 015 (NPOT, PSM, allocator, upload), sprint 016 (residency/eviction), sprint 017 (format/size matrix; CLUT and mipmaps dropped with measurements). 018 not needed |
| Depends on | Phase 5 (a backend that owns the upload path) |
| Exit criterion | no eviction when the working set fits in VRAM, upload benchmarks by format, fragmentation bounded under load/unload churn |

## What this phase is about

The 2 MiB of EDRAM is the tightest budget in the project, and texture residency is where a stable frame turns
into a stutter. Upload cost, swizzle correctness and eviction policy are one subject: a texture that has to be
re-uploaded mid-frame costs more than the draw it feeds.

## What has to be true before starting

- [x] Phase 5 backend uploads textures and passes the samples — pspgu, 2D corpus byte-identical
- [x] Boot-time VRAM accounting exists — sprint 014 reported the free tail; sprint 015 T3 replaced the bump arena
      with a 512-byte block allocator, so **used/free/largest/internal/allocs/frees** are real (largest 916,992 B in
      the churn). No largest-block number was quoted before it existed
- [x] Upload/eviction counters exist — sprint 015 T3 counts allocations, frees, failures, requested bytes, used,
      free and largest block. Eviction still does not exist; sprint 016 adds it

## Tasks

- [x] Swizzle **directly into the destination** — no memcpy to VRAM, no `sceGuCopyImage` — **sprint 015 T4**:
      8-row band (≤16 KiB stack) filled linearly, swizzled blocks written to the destination; no full-image heap
      scratch. 256×256 upload 21.088 → 19.849 ms; 128-B block/uncached alias rejected at 26.2 ms
- [x] POT padding on load; document the padding cost rather than hiding it — **sprint 015 T1**: both dimensions
      pad to powers of two, the padding is zeroed and swizzled, and `sceGuTexScale` restores the logical size.
      33×17 → 64×32 = 8,192 B (logical 2,244 B); the 33×17 checkerboard went from 281/561 wrong cells to 0/561,
      and the parity scenes stayed byte-identical
- [x] CLUT path for glyph atlases (T4/T8) only if measured to pay — **sprint 017 measured and dropped it**: T4
      8,256 B vs 32,768 B (24,512 B saved, 2.7 % of free) and 0.080 vs 0.532 ms, but no workload is VRAM-bound and
      the integration adds a palette/4-bit path. 018 not needed
- [x] Best-fit allocator over 512-byte blocks, coalescing on free, free bytes and largest block tracked
      separately — one allocator for framebuffers *and* textures — **sprint 015 T3**: 4,096 blocks over 2 MiB,
      32 KiB metadata, framebuffers/depth as fixed 8 KiB-aligned reservations, live `requested` and `internal`.
      Churn 100 × 16 textures: free and largest return exactly (916,992 → 916,992 B), 1,600 frees, 0 failures
- [x] Residency: priority-aware eviction where application priority is a strong weight; usage that ages per
      frame, never accumulating forever — **sprint 016**: `glPrioritizeTextures`, victim by priority then age,
      pin the last 2 frames, CPU shadow reload, coalesce-by-eviction. Fit: **0 evictions**; over 28×128²:
      **3,363 evictions / 3,348 reloads / 0 failed**; priority A/B swaps the victim; frag coalesces a 256²
- [x] Range-based cache writeback on upload; no whole-cache invalidate in a steady frame — one
      `pspguWriteback(base, tbw*padH*bpp)` per upload; no invalidate
- [x] No mipmaps unless a measurement says they pay — **sprint 017 measured and dropped them for 2D**: +33 % memory
      (349,504 vs 262,144 B) and an identical capture at 4× in the raw-GU probe; 3D minification is phase 10
- [x] Per-texture PSM, not a global swizzle mode — **sprint 015 T2**: default 8888 for parity; `PSP_GU_TEXFMT=auto`
      chooses 5650 for `GL_RGB` and 4444 for `GL_RGBA`/`GL_LUMINANCE_ALPHA` with binary alpha, else 8888. 32×32
      `GL_RGB` 4,096 → 2,048 B; the 128×128 LA font 65,536 → 32,768 B; texture VRAM 69,636 → 34,820 B. The `auto`
      delta is 7,843 px in `batch_check`, 0 in `basic_shapes`; the default stays 8888

## What we already know

- `vt5551.q` / `vt5650.q` convert **four RGBA8 pixels per cycle** — the scalar per-pixel loop in the reference
  engine is the thing to delete
- The reference `SwizzleFast` silently produces a blank texture when width × pixel size < 16 or `height % 8 != 0`
  — pad to whole 16×8 blocks or fall back to unswizzled
- Its allocator is best-fit with coalescing and a 4096-entry table (16 KiB, cache-resident); the same repo also
  ships a second bump allocator, and the two can hand out overlapping ranges. One allocator only
- `free()` on an EDRAM pointer is invalid — `vfree()` or nothing

## Exit criteria

- [x] no eviction or compaction in a workload whose working set fits — fit probe: **0 evictions**, 120 frames
- [x] upload/swizzle benchmarked by pixel format and size — sprint 017: 8888 32²–256² (0.499–33.332 ms), auto
      5650/4444 half the bytes (512² 524,288 B / 113–130 ms); 384²/512² 8888 not allocatable
- [x] fragmentation bounded across repeated load/unload — sprint 016: over-budget 3,363 evictions/3,348 reloads,
      0 failed; frag 3 evictions / 1 coalesce / 256² resident; no data-moving compaction
- [x] priority changes demonstrably alter eviction order — sprint 016: 1.0/0.1 victim swaps with the priorities

## Sprints and features

- [Sprint 010 — What fill actually costs, and 16-bit textures](../sprints/010-phase6-fill-and-texture-format.md)
  — **done, negative result.** Fill is rasterisation-rate-bound (~221 Mtexel/s): 16-bit textures changed the frame
  by 0.0 %, blending off by −0.2 %, 4× the area by +199.5 %. The budget is **~1.6–2.0 M drawn texels per 60 fps
  frame**, not a sprite count. 16-bit textures survive as a **memory** lever (2 B/texel in a 2 MiB budget), with
  `4444` for sprites and `5650` for opaque — the speed case for them is dead.
- [Sprint 015 — NPOT textures, per-texture format and the VRAM allocator](../sprints/015-phase6-npot-and-vram.md)
  — **done 4/4.** NPOT padding; 512-byte block allocator with free/coalesce and a real largest block; per-texture
  PSM (default 8888, `auto` 5650/4444); direct 8-row-band upload with no full-image scratch.
- [Sprint 016 — Texture residency and eviction](../sprints/016-phase6-residency-eviction.md) — **done 4/4.**
  CPU shadow, `glPrioritizeTextures`, victim by priority then age, last-2-frames pinning, reload on bind,
  coalesce-by-eviction. Fit 0 evictions; over 3,363/3,348/0; priority A/B swaps; frag coalesces. Closes exits
  1, 3 and 4.
- [Sprint 017 — Upload/swizzle matrix and the gated texture features](../sprints/017-phase6-format-matrix-and-gated-features.md)
  — **done 4/4.** Format/size matrix; CLUT T4 probe (8,256 vs 32,768 B) and mipmap probe (+33 % memory, identical
  at 4×). Both gated features **dropped with evidence**; 018 not needed. Phase 6 closes here.
