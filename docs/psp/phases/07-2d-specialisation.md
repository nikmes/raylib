# Phase 7 — 2D specialisation

| | |
|---|---|
| State | **ready** — sprint 018 (corpus, counters, boot budget) written and implementation-ready; 019–023 planned |
| Depends on | Phase 5 or 6 (batching and texture residency in place) |
| Exit criterion | text submissions scale with atlas runs, not glyph count; tile maps with atlas runs, not tile count |

## What this phase is about

The generic batcher handles "many sprites sharing state". This phase handles the four workloads that dominate
real 2D games and have their own traps: text, tile maps, shapes and particles. Each gets a fast path that
removes work *before* vertices are generated — the cheapest vertex is the one never built.

## What has to be true before starting

- [ ] Batching works; submissions already scale with state runs
- [ ] Counters can separate logical objects from submissions
- [ ] A representative workload per category exists (a UI screen, a tile map, a particle emitter)

## Tasks

- [ ] **Text**: accumulate consecutive glyphs of one atlas into a single run; no per-glyph bind
- [ ] **Sprites**: reject fully offscreen sprites before generating vertices; fast path for unrotated and
      uniformly scaled; optional horizontal slicing for large sprites if cache benchmarks justify it
- [ ] **Tile maps**: draw per atlas/state run, not per tile; verify against a large map
- [ ] **Shapes**: keep the shapes texture bound across consecutive calls; batch rectangles, triangles and lines
      into separate compatible runs
- [ ] **Particles**: one dynamic vertex stream per texture run, double or triple buffered; measure simulation
      cost separately from rendering
- [ ] Cache glyph layout where raylib semantics allow it (no re-shaping per frame)
- [ ] **Texture limit audit**: find every 2D path that can exceed 512×512 (large sprites, tile sheets, UI
      backgrounds) and slice it into 512-pixel windows with `tbw` set to the full texture stride
- [ ] **Texture-cache measurement**: per draw, record atlas footprint, atlas-run length and strip width
      (16/32/64/128 px); measure with a repeated draw as well as a single one, so reuse is visible
- [ ] **Bind audit**: assert one texture bind per atlas run in the frame counters; a glyph or tile causing a
      bind is a bug, not a slowdown
- [ ] **Boot-time 2D VRAM budget**: log free bytes and largest block after fonts/UI/hot atlases are resident;
      fail loudly rather than evict or upload mid-frame

## What we already know

- `rtext.c` draws every glyph through `DrawTexturePro`, which binds and unbinds per call — that is the single
  worst behaviour for dialogue-heavy screens
- `rshapes.c` is already forced off the shapes-quads path on PSP in the 5.5 port; that guard has a reason and
  must be understood before it is removed
- Text and sprites both need the **unsigned 16-bit TEXCOORD** rule respected: negative or zero-centred UVs wrap
- The reference engine's `MAX_BUNNIES 50000` was unreachable in practice because ~96 KiB per flush against a
  2 MiB list capped it around 21 flushes — capacity planning is part of this phase, not an afterthought

## VRAM facts that bind the 2D paths

Allocator design belongs to [Phase 6](06-textures-vram.md). What follows is only what a text, sprite, tile-map
or particle path has to assume about the memory it samples from. These facts come from the GE documentation,
the PSPSDK headers and the local engines; none of them has been re-measured by our protocol yet.

| fact | number | consequence for this phase | tag |
|---|---|---|---|
| Largest texture the GE samples | **512 × 512** | anything larger must be drawn as 512-pixel windows; `tbw` stays the full sheet stride so rows do not wrap | `[external: blueprint §16.7]` |
| Texture dimensions | power of two; `tbp` 16-byte aligned; `tbw` block-aligned | atlas padding is part of the VRAM budget, not an implementation detail | `[public: pspgu.h]` |
| Textures in main RAM | legal, but "huge speed penalty" | font and UI atlases belong in VRAM; only large or streamed sheets go to main RAM | `[public: pspgu.h]` |
| VRAM texture binding (local engines) | bound with swizzle set: `sceGuTexMode(fmt, mips, 0, 1)`; `sceGuTexSync()` before reuse | a slot that is rewritten while the GE may still sample it needs a sync, not hope; unswizzled VRAM access is legal but wastes the cache | `[source scan: openTRI triTexman.c; swizzle is a choice, not a hardware requirement]` |
| `sceGuTexImage` | emits a TEXFLUSH | cache the last bind; one bind per atlas run, never per glyph or tile | `[external: blueprint §16.7]` |
| Texture cache | **8 KiB**, coherent with nothing; only `sceGuTexFlush` empties it | keep the sampled footprint per triangle small; a changed texture needs a flush | `[external: blueprint §16.5/§16.7; not in the headers]` |
| Cache granularity | 16-byte × 8-row swizzle block = **128 B** | swizzling turns eight 16-byte reads into one line; unswizzled sheets cannot use the cache properly | `[derived from blueprint §16.7]` |
| Cache footprint of common sizes | T8 64×64 = **4 KiB**, T8 128×128 = **16 KiB**, 5551 480×272 = **255 KiB** | a 128×128 T8 texture alone is twice the cache; a full-screen 16-bit sprite is ~32× it | `[derived; sizes = w × h × bpp]` |
| Full-width band at 480 px | 480 × 2 B × 8 rows = **7,680 B** | one 8-row band of a full-width 5551 texture nearly fills the cache, leaving no room for a second | `[derived]` |
| Writing EDRAM from the CPU | kuseg0 is cached; use the uncached alias or write back the exact range | PGE and openTRI write swizzled data through the `0x40000000` alias; PSPRuntime writes cached and then calls `sceKernelDcacheWritebackInvalidateAll()` | `[source scan: PGE pgeTexture.c, openTRI triTexman.c, PSPRuntime texturemanager.cpp]` |
| VRAM ownership under pspGL | first-fit allocator in `pspgl_vidmem.c`, with eviction, migration and compaction | until phase 5/6 replaces it, any VRAM measurement includes pspGL's policy; its own accounting is `__pspgl_vidmem_avail()` | `[source scan: pspgl_vidmem.c]` |

The last two rows are the ones most likely to mislead a measurement: an upload that looks free may simply have
been absorbed by pspGL, and a "cache-friendly" strip may only be moving the same compulsory misses around.

### Atlas budget for a 2D frame

With 16-bit framebuffers and the depth buffer, **1,261,568 B** remain in VRAM. A native 2D backend has no use
for depth (see [Phase 5](05-native-gu-backend.md)), so dropping it returns **278,528 B** and leaves
**1,540,096 B** — the number a 2D-only layout should start from. All values below are POT, 16 bpp-equivalent
sizes; allocator rounding (512-byte blocks) and any render targets are extra.

| atlas | 5551 / 4444 | T8 + CLUT | T4 + CLUT |
|---|---:|---:|---:|
| 128 × 128 | 32,768 B (46) | 16,384 B (94) | 8,192 B (188) |
| 256 × 256 | 131,072 B (11) | 65,536 B (23) | 32,768 B (47) |
| 512 × 512 | 524,288 B (2) | 262,144 B (5) | 131,072 B (11) |

Parenthesised counts are how many fit in 1,540,096 B. A representative 2D set — one 512² T8 font atlas, one
512² T8 tile sheet and one 512² T4 sprite sheet — is **640 KiB**, leaving **864 KiB** for hot swaps and a
second sheet. `[derived]` The practical read: T4/T8 is not a micro-optimisation here, it is what makes a
working set fit at all.

## Candidate recipes (unverified)

These are techniques read from working GU engines and checked against the headers, but **not yet run through
our device protocol**. Under the project's rule in §13 of `memory-map.md` they are hypotheses, not recipes; do
not promote one to `docs/psp/recipes/` until a sprint has exercised it on the PSP-1000.

### R1 — Split oversized art into 512-pixel windows

Any sprite, tile sheet or background wider or taller than 512 px is drawn as separate windows of at most
512×512. Each window keeps `tbw` equal to the full sheet stride and offsets its UVs by the window origin, so a
sample never leaves the legal window and rows never wrap.

- **Why**: the GE samples at most 512×512 per `sceGuTexImage`.
- **Watch**: a window is a separate draw and therefore a separate state run; sort windows with the rest of the
  atlas runs rather than emitting them per object.
- **Verify**: a 1024×1024 test sheet at 1:1, pixel-compared across every seam; then an oversized sprite crossing
  all four windows.

### R2 — Use cache-sized strips only where texels repeat

Strip width does not reduce compulsory misses for a texture drawn once at 1:1 — every texel is read once either
way. It pays when the same texels are sampled repeatedly (tiled backgrounds, repeated sprites, minified
art), because a smaller working set can survive between draws. The blueprint recommends 32–64 px vertical
strips; openTRI independently blits mip levels in 32-texel slices with the comment "maximizing the use of the
texture-cache". Neither is a 2D-sprite measurement.

- **Why**: 8 KiB cache versus 16 KiB for one 128×128 T8 texture; a 480-wide band is 7,680 B.
- **Watch**: to get reuse, sort by strip across draws (all sprites' strip 0, then strip 1); slicing one sprite
  in isolation only changes the order of the same misses.
- **Verify**: 16/32/64/128 px variants, single draw and repeated draw, same pixels, frame time and a cache
  counter if one exists. Adopt a width only if the repeated-draw case moves.

### R3 — One POT atlas per font, glyphs batched into runs

Pack a font into one 512²-max atlas, T4 or T8 with a CLUT, and emit consecutive glyphs of that atlas as one
run. T4 uses a 16-entry CLUT (`sceGuClutLoad(2, ...)`), T8 a 256-entry one (`sceGuClutLoad(32, ...)`); the CLUT
is loaded on the bind, so a per-glyph bind pays it per glyph.

- **Why**: a 512² T8 glyph atlas is 256 KiB, a T4 one 128 KiB; a 128×128 T8 atlas is 16 KiB, already twice
  the cache. `rtext.c` currently binds per glyph through `DrawTexturePro`.
- **Watch**: the font atlas and the shapes texture may share an id in 6.5-dev (see sprint 007); do not split
  them accidentally and turn one run into two.
- **Verify**: submissions equal font runs, not glyph count; short labels and a paragraph; screenshot identical
  to the per-glyph path.

### R4 — Split residency: static UI in VRAM, streamed art in main RAM

Reserve VRAM for fonts, UI, particle textures and a small hot set; leave large world or streamed sheets in main
RAM even though sampling them is slower. Any VRAM slot rewritten while the GE may sample it needs
double-buffering or `sceGuTexSync()` first.

- **Why**: only ~1.26 MiB free with depth (~1.47 MiB without), and a single 512² 5551 sheet is 512 KiB. A
  too-large working set turns every frame into eviction.
- **Watch**: pspGL's allocator may migrate or compact a buffer that our code believes is resident. Do not pin
  addresses across frames before phase 5 owns the allocator.
- **Verify**: boot-time map, zero evictions in a scenario whose working set fits, stable frame over a five-minute
  run.

### R5 — Swizzle directly into the destination

Write swizzled texels straight into the final VRAM block through the uncached alias (or a cached buffer followed
by `sceKernelDcacheWritebackRange` over exactly that range). Do not allocate a staging block, swizzle into it and
copy, and do not invalidate the whole D-cache in a steady frame.

- **Why**: PGE's `pgeTextureSwizzle` allocates a second VRAM block, swizzles into it and frees the first;
  PSPRuntime swizzles into a cached buffer then calls `sceKernelDcacheWritebackInvalidateAll()`. Both are
  avoidable at load time and unacceptable per frame.
- **Watch**: `sceKernelDcacheWritebackRange` is in `psputils.h`; the `0x40000000` alias is the practical
  uncached path used by both local engines.
- **Verify**: texture bytes identical after upload, no whole-cache call in a frame trace, MB/s measured by
  format.

## Exit criteria

- [ ] text: submissions proportional to atlas runs, measured on short labels and paragraphs
- [ ] tile map: submissions proportional to atlas runs on a map large enough to matter
- [ ] particles: one submission per compatible system, CPU simulation cost measured separately
- [ ] every 2D path that can exceed 512×512 renders correctly through window slicing, seams included
- [ ] texture-cache behaviour measured, not assumed: a strip width is adopted only if the repeated-draw case
      moves, and the working set per draw is recorded
- [ ] no mid-frame texture upload or eviction in a representative UI, tile-map and particle scene
- [ ] boot-time 2D VRAM map printed, with fonts/UI/hot atlases resident and the largest free block recorded
- [ ] no visual regression in the samples, screenshots compared

## Sprints and features

The phase is sized as six sprints. 018 is written; 019–023 are planned and are written only when the previous
sprint's numbers are in hand (the fast paths depend on what the corpus measures).

| sprint | scope | state |
|---|---|---|
| [018 — Corpus, counters and boot budget](../sprints/018-phase7-corpus-and-budget.md) | `scene_2d`: text, tiles, particles, shapes, mixed; boundary counters; boot VRAM map; 512-pixel audit | written |
| 019 — Text runs and layout cache | consecutive glyphs of one atlas per run; cached layout; paragraph parity | planned |
| 020 — Tile maps and 512-pixel windowing | per-atlas-run tiles, scrolled traversal, window slicing with full-stride `tbw`, seam screenshots | planned |
| 021 — Sprites and shapes | offscreen rejection before vertex generation; unrotated/uniform-scale paths; shapes texture held across compatible runs | planned |
| 022 — Particles | one dynamic stream per texture run, double/triple buffered; simulation measured separately | planned |
| 023 — Cache strips, regression, close | 16/32/64/128 px strip A/B on the repeated-draw case; full sample comparison; phase exit table | planned |

Sprint 023 collapses into 022 if the strip experiment is a null result, which R2 predicts it may be; the phase
then closes after five sprints.

R1–R5 above are **candidate** recipes. A sprint that exercises one on the PSP-1000 should record the device
numbers here and then promote the verified technique to `docs/psp/recipes/`; until then they stay hypotheses.
