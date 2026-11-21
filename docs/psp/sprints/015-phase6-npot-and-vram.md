# Sprint 015 — NPOT textures, per-texture format and the VRAM allocator

| | |
|---|---|
| Roadmap phase | [Phase 6 — Textures and VRAM](../phases/06-textures-vram.md) |
| Branch / base commit | `master` @ `c31afe31` (T1 landed in `719b037e`) |
| Started / closed | 2026-11-21 / 2026-11-21 |
| Build number(s) | `b16` baseline, `b17` T1, `b19` T3, `b20` T2, `b21` T4 |
| Tasks | 4/4 done |

## 0. Handover

**Situation (closed).** Phase 5 closed with pspgu owning the upload path and the 2D corpus byte-identical to
pspGL; the ceiling is 2,375 sprites at 60 fps. This sprint closed all four texture/VRAM tasks: NPOT padding
(`b17`), a 512-byte block allocator with free and a real largest block (`b19`), per-texture PSM behind
`PSP_GU_TEXFMT` (`b20`), and a direct 8-row-band upload with no full-image heap scratch (`b21`). The default
build remains 8888 and all parity scenes are byte-identical.

**Read before acting**, in this order:

1. `../memory-map.md` §3 and §“VRAM: 2 MB is the whole budget” — the three-buffer map, the host-RAM lists/arenas,
   the 64-byte writeback rule, the NPOT padding note and the allocator/PSM facts.
2. `../phases/06-textures-vram.md` — the phase tasks this sprint served; residency/eviction is sprint 016.
3. `tools/pspgu/gl_texture.c` — `uploadTexels`, `chooseFormat`, `applyTexture`, `glTexSubImage2D`; and
   `tools/pspgu/egl.c` — the block allocator and `pspguVramReport`.
4. `../sprints/012-phase5-texture-parity.md` T2 and `../sprints/014-phase5-ceiling-vram-readback.md` §9 — the
   NPOT diagnosis and the measured VRAM map the allocator replaced.

**First action.** None: the sprint is closed. The next sprint is **016, residency and priority-aware eviction**,
which uses T3's free/largest accounting and the churn probe.

**Implementation order (as executed).** T1 → **T3** → **T2** → **T4**. T3 defined the allocation API the other
two use; T4 depends on T3's block and T1's `padW/padH`.

**Hard rules.**

- **The 2D gate stays at 0 differing pixels** after every default-build change. `batch_check` and `basic_shapes`
  are the regression gate. A 16-bit texture format is an **opt-in build** precisely because it can move pixels;
  the default stays 8888 until a measured acceptance exists.
- **The 2D parity scene must not include the NPOT texture.** pspGL hangs on a 33×17 texture; `PATTERN_NPOT` is a
  capability test with its own oracle (the generated checkerboard), not a pspGL comparison.
- **Padding is real memory.** A padded texture is allocated, zeroed, swizzled and counted. Never report the
  logical size as the allocated size.
- **`sceGuTexScale` is per texture.** `applyTexture` sets `realW/padW`, `realH/padH` after every `sceGuTexMode`.
- **No “largest free block” until T3 exists.** The bump allocator can only report the free tail.
- **Framebuffers and depth are never freed**, and they need 8 KiB alignment. The allocator must reserve them
  first and fail loudly if it cannot; a texture must never land in their range.
- **Check the build marker before measuring** (`[PSPGU] GU up: pspgu bNN, texfmt …`) and run
  `tools/pspgu-build.sh --verify-install` **with the same configuration env** (`PSP_GU_TEXFMT=auto`,
  `PSP_GU_DEPTH=0`). The archive does not store its flags; verifying an auto install with a default build
  reports STALE even though the boot line is right.
- Commit messages carry the task ID: `sprint 015 T3: ...`.

**Commands.**

```sh
export PSPDEV=/home/psp/pspdev
export PATH=$PATH:$PSPDEV/bin

cd /home/psp/raylib-psp
./tools/pspgu-build.sh                      # build + install pspgu (default 8888)
./tools/pspgu-build.sh --texfmt=auto        # measured 16-bit build
PSP_GU_TEXFMT=auto ./tools/pspgu-build.sh --verify-install   # same config as installed
./tools/pspgu-build.sh --restore            # pspGL back; --check must say 2ab7378e

# device run
cp <test>.prx /tmp/s3/app.prx
cd /tmp/s3 && pspsh -e "reset"; sleep 6
( printf './app.prx\n'; sleep 20 ) | pspsh
# after DONE and sleep only, twice:
pspsh -e "scrshot host0:/cap1.raw"; sleep 1; pspsh -e "scrshot host0:/cap2.raw"
cmp cap1.raw cap2.raw
```

**Do not touch.** The batcher in `src/`, `gl_draw.c`'s vertex paths, the `MESH_SCENE`/`DEPTH_SCENE` renders,
`DrawMesh` (phase 10), and the 2D gate scene.

**Done means** §8: NPOT correct, 2D gate 0, T2 formats measured, T3 churn accounting real, T4 upload measured.

## 1. Objective

Four falsifiable results, one per task:

1. **T1 (done):** a 33×17 texture renders its generated checkerboard exactly — every cell single-colour, parity
   `(x+y)&1`, no shear — and the two parity scenes are byte-identical.
2. **T2:** a 32×32 texture allocates **2,048 B instead of 4,096 B** in the 16-bit format build, and the build's
   pixel delta against the 8888 build is counted and recorded. The default build still renders the parity scenes
   byte-identically.
3. **T3:** after **100 load/unload cycles**, free bytes and largest free block return to their pre-churn values
   within one 512-byte block, with no allocation failure; the boot line reports `used / free / largest / allocs /
   frees`.
4. **T4:** a 256×256 RGBA upload allocates **no full-texture CPU scratch** (the current `calloc` is 262,144 B;
   the target is a 512-byte stack tile), its elapsed time is measured before/after, and every pixel is unchanged.

## 2. Why now

Phase 6's first precondition — “a driver that owns VRAM” — became true when phase 5 closed. The allocator is the
phase's structural exit work: without free/coalesce there is no largest block and no way to measure “no eviction
when the working set fits”. Per-texture formats are the measured memory lever sprint 010 identified; direct
swizzle removes the upload's largest heap temporary. Residency/eviction is deliberately the next sprint.

## 3. Scope

**In:** NPOT padding + `sceGuTexScale` (done); per-texture PSM behind a build switch; a 512-byte block allocator
with coalescing, free and largest-block counters; a churn probe; direct swizzle into the destination.

**Out:** residency/priority eviction (sprint 016; needs T3), CLUT/T4 glyph atlases and mipmaps (only if measured),
512-pixel windowing for large sheets (phase 7), `DrawMesh` (phase 10), the batcher.

**Decisions recorded before coding:**

- **T2 default is 8888.** `PSP_GU_TEXFMT=auto` is the measured 16-bit build; `PSP_GU_TEXFMT=8888` (default)
  keeps the parity gate. The phase exit asks for upload benchmarks by format, not for a default that trades
  pixels without acceptance.
- **T3 block size is 512 B**, matching the phase page and the reference engine's allocator; 2 MiB = 4,096 blocks,
  so a 4-byte-per-block table is 16 KiB and cache-resident.
- **Framebuffers are reservations, not frees.** The three 272 KiB buffers and the 272 KiB depth are allocated
  first at 8 KiB alignment and never released; textures use 64-byte alignment.

## 4. Preconditions

- [x] pspgu installed, boot marker visible, `--verify-install` matches
- [x] pspGL restorable, `pspgl-build.sh --check` content hash `2ab7378e`
- [x] 2D gate re-confirmed at 0 differing pixels on the final phase-5 build
- [x] `texture_pattern` builds with `PATTERN_NPOT=1` and the Makefile defines it; the app logs `PATTERN NPOT=1`
- [x] T1 baseline and change captured: 281/561 → 0/561 cells wrong
- [x] **Evidence archived**: `/home/psp/evidence/sprint015/` with `SHA256SUMS` — `npot_before.raw`,
      `npot_after.raw`, `t1_gate1.raw`, `t1_bs.raw`, and the oracles `par_pspgl.raw`, `bs_gl_h.raw`
- [x] `pspsh` answers; `reset` per run; capture only after `DONE`, two identical captures
- [x] Probe designs are fixed and owned by the tasks (they are not pre-existing): T3 creates
      `psp-tests/texture_churn`; T4 adds an `UPLOAD` timing mode to `texture_pattern`
- [x] The T3 baseline line is recorded before the allocator change: `texture_pattern` 77,956 B after assets,
      `batch_check` 69,636 B, free tails 905,024 / 913,344 B (b17)

## 5. Tasks

| ID | task | status | verification | evidence |
|---|---|---|---|---|
| T1 | **NPOT textures (phase task 6c)**: pad **both** dimensions to powers of two (height at least 8 rows for the swizzle), zero the padding, swizzle the padded buffer, and correct with `sceGuTexScale(realW/padW, realH/padH)`; keep the logical size for GL | `done` | 33×17 checkerboard at (160,30), 3×: every cell single-colour and matching `(x+y)&1`; no shear; `texture_pattern` non-NPOT and the 2D gate unchanged | **281/561 cells wrong → 0/561**, two colours, no shear. 33×17 pads to 64×32 = **8,192 B** (logical 2,244 B; old linear 4,352 B). `batch_check` **0 differing pixels**; `basic_shapes` **284 colours, 0**. `b17`, commit `719b037e`; captures in `/home/psp/evidence/sprint015/` |
| T2 | **Per-texture PSM** behind `PSP_GU_TEXFMT` (`8888` default, `auto` measured): opaque `GL_RGB` → `5650`; `GL_RGBA`/`GL_LUMINANCE_ALPHA` → `4444` only when every alpha is 0/255 (binary), else `8888`; one decision and one log line per texture. Also fix the packed-16 source decode (4444 was decoded as 565) | `done` | 32×32 **GL_RGB** texture **4,096 → 2,048 B**; `auto`'s pixel delta vs `8888` counted by scene; default `batch_check`/`basic_shapes` still 0; the half-alpha RGBA texture (alpha 128) stays `8888` and is checked unchanged | **Default 8888: both parity scenes byte-identical.** `auto`: 32×32 GL_RGB → **5650, 2,048 B**; drawn probe reads (200,36,80), not swapped; 128×128 LA font 65,536→32,768 B and 32×32 LA wabbit 4,096→2,048 B (both binary alpha); half-alpha RGBA (alpha 128) stays **8888, 4,096 B**. Texture VRAM: `batch_check` 69,636→**34,820 B**, `basic_shapes` 65,540→**32,772 B**. Delta vs 8888: `batch_check` **7,843 px / 13,067 bytes** (4-bit quantisation, same channel order), `basic_shapes` **0 px**. 4444 NPOT checker **0/561** wrong. `b20` |
| T3 | **Best-fit allocator** over 512-byte blocks with coalescing; free bytes and largest block tracked separately; one allocator for framebuffers, depth and textures; `pspguVramFree` + stats | `done` | 100 load/unload cycles: free and largest return within 512 B, no failure; boot line reports used/free/largest/allocs/frees; a texture unload returns its blocks; 2D gate 0 | **PASS twice.** 100 cycles × 16 textures: `free 916,992 → 916,992`, `largest 916,992 → 916,992`, `internal 508` (the 4-byte white texel's block), `allocs 1,606 / frees 1,600 / failures 0`. Boot line `reserved=1,114,112 requested=4 used=1,114,624 free=982,528 largest=982,528 internal=508`; assets `requested=69,636 used=1,184,256 free=912,896`. 2D gate **byte-identical**. `b19`; probe `psp-tests/texture_churn`, verdict in `churn_done.txt` |
| T4 | **Direct swizzle into VRAM**: no full-image `calloc`; fill one **8-row band** (≤16 KiB, cache-resident) linearly and write the swizzled blocks; cached base + one range writeback | `done` | 256×256 RGBA upload: heap scratch gone (262,144 B → 0), elapsed time before/after, pixels unchanged; no whole-cache invalidate | **21.088 → 19.849 ms** median (min 19.780, max 20.046, 30 runs; −5.9 %). Default 8888 `batch_check` and `basic_shapes` byte-identical; `auto` `batch_check` identical to b20; 5650 square (200,36,80); 4444 NPOT **0/561**. No heap: an 8-row band (≤16 KiB stack) and one `pspguWriteback`. The 128-B block + uncached alias measured **26.2 ms** and was rejected. `b21` |

Status vocabulary: `todo` / `doing` / `done` / `blocked` / `dropped`. IDs are stable.

### T3 design (implementation-ready)

- **Arena.** The whole 2 MiB EDRAM, in 512-byte blocks: 4,096 blocks. Metadata `{ uint16 blocks; uint8 free;
  uint8 fixed; uint32 requested; }[4096]` = **32 KiB** in main RAM. `requested` is live (subtracted on free), so
  `internal = used - reserved - requested` stays meaningful across churn.
- **Reservations at init.** Allocate the three colour buffers (544 blocks each, 8 KiB-aligned = 16 blocks) and
  the depth buffer (544 blocks, 8 KiB-aligned) before any texture. They are marked used and never freed. If a
  reservation fails, log and abort: rendering cannot continue.
- **Texture allocation.** 64-byte alignment (already inside a 512-byte block), best-fit over free spans, split
  the tail, coalesce with free neighbours on release. Zero is never a valid texture offset after boot; return
  a `0` failure and log `VRAM FULL` with used/free/largest.
- **API.** `unsigned int pspguVramAllocAligned(unsigned int bytes, unsigned int align)` returns
  `PSPGU_VRAM_INVALID` (`0xFFFFFFFF`) on failure; `void pspguVramFree(unsigned int offset)`;
  `void pspguVramStats(PspGuVramStats *out)` with `reserved`, `requested`, `used`, `free`, `largest`, `allocs`,
  `frees`, `failures`. The framebuffer reservations use the same allocator and may legitimately return offset
  **0**; the texture wrapper `pspguVramAlloc` maps `PSPGU_VRAM_INVALID` to 0 because textures never start at 0,
  so T1's existing `offset == 0` checks stay correct.
- **Accounting is block-rounded.** An allocation of `n` bytes consumes `ceil(n/512)` blocks; `used` is the sum of
  used blocks, `free` the sum of free blocks, `largest` the longest free span. `requested` is the sum of the
  logical byte requests. `internal = used - reserved - requested` is real internal fragmentation (block
  rounding plus alignment) and must be printed; it is not padding from the old bump line. A 4-byte white texture
  costs one 512-byte block.
- **Texture ownership.** `PspGuTexture` stores `requestedBytes` and `blocks`; `glDeleteTextures` frees the block
  span and sets `offset = 0` (a second delete is a no-op); a re-upload with the same size reuses the block, a
  different size frees and reallocates. The bump arena's “never reclaim” comment is removed. Framebuffers are
  never passed to free.
- **Reporting.** `pspguVramReport` prints `reserved / requested / used / free / largest / internal / allocs /
  frees`. A `PSPGU_VRAM_TRACE` build logs the same line after every alloc/free for the churn run; the probe
  calls `pspguVramReport("churn N")` every 10 cycles (the symbol is exported from `libGL.a`; declare it in the
  probe).
- **Churn probe.** New `psp-tests/texture_churn`: load 16 textures of 64×64 from generated images, unload them,
  repeat 100 times; log the VRAM line at cycle 0, 10, …, 100. Verification: free and largest at cycle 100 equal
  cycle 0 within 512 B; no `VRAM FULL`.
- **Fragmentation.** The same probe with alternating sizes (64×64, 128×32, 32×128) for 100 cycles; free returns;
  largest is at least `free - 512` if the frees coalesce.

### T2 design (implementation-ready)

- Add `PSP_GU_TEXFMT` (`8888` default, `auto`): `auto` chooses `GU_PSM_5650` for `GL_RGB`, `GU_PSM_4444` for
  `GL_RGBA`/`GL_LUMINANCE_ALPHA` only when a scan of the source alpha finds every value 0 or 255, otherwise
  `GU_PSM_8888`. `5650`/`4444` are 2 B/texel; `8888` stays 4. The scan is one pass during upload, keyed on the
  source format: RGBA byte 3, LUMINANCE_ALPHA byte 1; packed 16-bit sources stay 8888 (a 1-bit alpha source is
  not what this lever is for).
- **Bit order, matching the framebuffer expansion that the readback proved:** `5650` is
  `r5 | (g6 << 5) | (b5 << 11)` (red low); `4444` is `r4 | (g4 << 4) | (b4 << 8) | (a4 << 12)` (red low, alpha
  high), the GE's `0xAABBGGRR` order. If the `auto` capture shows a red/blue swap, this is the setting to
  re-check first.
- **The 32×32 byte target is a GL_RGB source.** The existing half-alpha test is RGBA with alpha 128 and must
  stay 8888; add a `TEXFMT_PROBE` mode to `texture_pattern` that loads a 32×32 `R8G8B8` image and **draws it at
  (400,220)** so the 5650 bit order is visible as a colour, not only a byte count. This is the measurement, not a
  guess.
- **The source scan decides, and the corpus is not what the first draft assumed.** The 128×128 font atlas and the
  32×32 wabbit are both `LUMINANCE_ALPHA` with **binary** alpha, so `auto` chooses 4444 for both. The half-alpha
  probe (alpha 128) stays 8888. `basic_shapes`' white/transparent glyphs are exact in 4-bit, so its delta is 0;
  `batch_check`'s tinted sprite quantises 7,843 pixels. Both are measured, not predicted.
- Extend the upload paths to pack 5650/4444 directly (no 8888 round trip). Keep `GU_TCC_RGBA`; 5650 ignores
  alpha, 4444 uses it. Fix the packed-16 source decode while here: the current code decodes all three 16-bit
  types as 565, so a 4444 source would be wrong.
- Log once per texture: `texture %u: %ux%u src=0x%x -> PSM 0x%x (%u B)`.
- Verification: the `auto` 32×32 **GL_RGB** texture is 2,048 B; the `8888` default gate is 0; the `auto` build's
  pixel delta is counted per scene (not required to be 0); the half-alpha RGBA texture stays 8888 and unchanged.
  If the corpus has no binary-alpha or opaque textures, the measured delta is 0 and the win is demonstrated by
  the GL_RGB probe plus the byte accounting — that is a real result, recorded as such.

### T4 design (implementation-ready)

- Replace the full-padded-image `calloc(total)` + `swizzleFast` with a per-band path. **Measured twice:** a
  literal 128-byte block tile with an uncached destination measured **26.2 ms** for 256×256 RGBA, because the
  block loop reads the source in a strided order and uncached writes are slow. The shipped path is:
  - fill one **8-row band** linearly (`8 × tbw × bpp`, ≤16 KiB at 512×8888), source rows read sequentially and the
    band is cache-resident;
  - write the swizzled blocks from the band (`block = (by/8)*(tbw*bpp/16) + bx/(16/bpp)`, 16 bytes per row);
  - zero the band first, so padding columns and rows are written as zero;
  - write through the **cached** EDRAM base and do one `pspguWriteback(base, tbw*padH*bpp)` at the end.
- This uses no full-image heap allocation and measured **19.849 ms** against the old **21.088 ms**. The
  `PSP_GU_NO_SWIZZLE` diagnostic keeps a linear per-texel path, including zeroed padding. The sub-image path is
  unchanged. A `tbw > 512` texture is rejected with a log; 512-pixel windowing is phase 7.
- The **sub-image** path (`glTexSubImage2D`) keeps its per-texel swizzled offset; T4 only changes full uploads.
- Verification: the 33×17 NPOT capture and the two parity scenes are unchanged. A 256×256 RGBA upload's elapsed
  time is measured **before** (b20, full scratch) and **after** (b21) with the same pre-built image. Build the
  `Image` once and exclude PNG/file decode: time `LoadTextureFromImage` + `UnloadTexture` in a new
  `UPLOAD_PROBE` mode, 30 runs, report min/median/max.
- **Heap evidence:** the upload path no longer calls `calloc`; the tile is a 128-byte stack array. The app cannot
  query the allocation peak, so the evidence is the removed 262,144-byte allocation for 256×256 RGBA plus the
  measured time, not a fabricated heap number. The source conversion (LA/RGBA to 8888, or the T2 16-bit pack)
  happens inside the tile loop.

### Task log (append-only)

- 2026-11-21 — sprint opened, all tasks `todo`.
- 2026-11-21 — `texture_pattern`'s Makefile now defines `PATTERN_NPOT`; the app logs `PATTERN NPOT=1`.
- 2026-11-21 — T1 `doing`: baseline on b16, 281/561 cells wrong, parity flips at x=17.
- 2026-11-21 — T1 `done`: b17 gives 0/561 wrong, two colours; parity scenes byte-identical; commit `719b037e`.
- 2026-11-21 — sprint reviewed for implementation readiness: T3's allocator, T2's format switch and T4's tile
  path written down; T3 is the next action.
- 2026-11-21 — T3 `doing`: 512-byte block table, fixed 8 KiB framebuffer/depth reservations, free+coalesce,
  live `requested`, boot line `reserved/requested/used/free/largest/internal`.
- 2026-11-21 — T3 `done`: churn probe `psp-tests/texture_churn`, 100 cycles × 16 textures, free and largest
  return exactly (916,992 → 916,992), 1,600 frees, 0 failures, PASS in two runs; 2D gate byte-identical.
  **Finding:** the first probe uploaded 1,600 textures with no `EndDrawing`, filled the 240 KiB list and hung at
  cycle ~50; the probe now ends a frame per cycle. A texture-only workload with no swap remains a hazard (GU has
  no list-capacity check) and is a follow-up.
- 2026-11-21 — T2 `doing`: per-texture PSM behind `PSP_GU_TEXFMT`; source decode now distinguishes 565/5551/4444;
  sub-image path is bpp-aware.
- 2026-11-21 — T2 `done`: default 8888 byte-identical on both parity scenes. `auto`: GL_RGB→5650 2,048 B; the
  LA font and wabbit are binary-alpha → 4444 (65,536→32,768 and 4,096→2,048); alpha-128 stays 8888. Delta:
  `batch_check` 7,843 px (4-bit), `basic_shapes` 0. 5650 square (200,36,80) not swapped.
- 2026-11-21 — T4 `doing`: 128-B block + uncached alias measured **26.2 ms**, rejected; 8-row band with cached
  base + one writeback measured **19.849 ms** vs 21.088 baseline.
- 2026-11-21 — T4 `done`: no full-image heap scratch; default and `auto` pixel gates identical; 5650/4444 still
  correct. **Sprint 015 closed 4/4.**

## 6. Budgets

| budget | limit | measured | tag |
|---|---|---|---|
| 2D gate, differing pixels (default) | 0 | **0** (`batch_check`); **284/0** (`basic_shapes`) | `[measured: b17]` |
| NPOT correctness | every 33×17 cell single-colour, correct parity | **0/561 wrong**, two colours | `[measured: b17]` |
| NPOT padding cost, 33×17 | 64×32 = 8,192 B | **8,192 B** (logical 2,244 B; old 4,352 B) | `[measured]` |
| 32×32 GL_RGB texture, `auto` | 2,048 B (4,096 B at 8888) | **2,048 B, PSM 5650** | `[measured: b20]` |
| texture VRAM after T2 | measured, ≤ 69,636 B | **34,820 B** (`batch_check`); **32,772 B** (`basic_shapes`) | `[measured: b20]` |
| `auto` pixel delta vs 8888 | counted, not required to be 0 | **7,843 px / 13,067 B** (`batch_check`); **0** (`basic_shapes`) | `[measured: b20]` |
| half-alpha RGBA (alpha 128) | stays 8888 | **4,096 B, PSM 8888**, unchanged | `[measured: b20]` |
| block rounding / internal fragmentation | reported, not hidden | **508 B** (the 4-byte white texel's block) | `[measured: b19]` |
| free tail after assets (depth=1) | — | **912,896 B** (`batch_check` at b19); 916,992 B in the churn baseline | `[measured: b19]` |
| largest free block | real, not a free-tail alias | **982,528 B** at boot; **912,896 B** after assets; **916,992 B** at churn | `[measured: b19]` |
| churn: free/largest return | within 512 B after 100 cycles | **exact**: 916,992 → 916,992 both | `[measured: b19, two runs]` |
| allocator metadata | main RAM, cache-resident if possible | **32 KiB** (4,096 × 8 B) | `[measured]` |
| upload 256×256 RGBA, heap | 0 full-image heap (band is stack) vs 262,144 B | **0 heap**; 8-row band (≤16 KiB stack) | `[measured: b21]` |
| upload 256×256 RGBA, time | measured before/after | **21.088 → 19.849 ms** (−5.9 %) | `[measured: b20/b21]` |
| whole-cache invalidate in a steady frame | 0 | 0; one exact-range `pspguWriteback` per upload | `[measured]` |

## 7. Measurements

Protocol: device only, one variable per run, control → change → control again. Every row carries the boot
marker. T1 captures are archived in `/home/psp/evidence/sprint015/` with `SHA256SUMS`.

| run | build | workload | metric | value | notes |
|---|---|---|---|---|---|
| baseline | b16 | `texture_pattern PATTERN_NPOT=1` | 33×17 cells wrong | **281 / 561**, 4 colours | parity flips at x=17 |
| change | b17 | same | 33×17 cells wrong | **0 / 561**, 2 colours | `(x+y)&1` correct |
| control | b17 | `batch_check` | differing pixels vs pspGL | **0** | two identical captures |
| control | b17 | `basic_shapes` | colours / differing pixels | **284 / 0** | byte-identical |
| change | b17 | `texture_pattern` | texture VRAM after assets | **77,956 B** | 33×17: 4,352 → 8,192 B; 4×4: 64 → 256 B |
| baseline | b17 | bump allocator | texture unload | **cannot free** | the old arena only moves forward |
| control | b19 | `batch_check` default | gate / VRAM line | **0 differing pixels**; `reserved=1,114,112 requested=69,636 used=1,184,256 free=912,896 largest=912,896 internal=508` | T3 did not move the gate |
| change | b19 | churn 100 × 16 textures | free / largest | **916,992 / 916,992 → 916,992 / 916,992** | two runs, `PASS`, `failures=0` |
| change | b19 | churn accounting | allocs / frees / internal | **1,606 / 1,600 / 508** | 6 permanent allocations + 1,600 churn |
| control | b20 | `batch_check` default | gate / texture bytes | **0 differing pixels**; texture 69,636 B | T2 did not move it |
| control | b20 | `basic_shapes` default | gate / texture bytes | **byte-identical**; texture 65,540 B | T2 did not move it |
| change | b20 | `batch_check` `auto` | texture bytes / delta | **34,820 B**; **7,843 px / 13,067 B** delta | same channel order (mean 3.5 vs 43 swapped on saturated pixels) |
| change | b20 | `basic_shapes` `auto` | texture bytes / delta | **32,772 B**; **0 px** delta | white/transparent glyphs are exact in 4-bit |
| change | b20 | `texture_pattern TEXFMT_PROBE` `auto` | 32×32 GL_RGB | **PSM 5650, 2,048 B**; drawn (200,36,80) | 5650 bit order correct |
| change | b20 | `texture_pattern PATTERN_NPOT` `auto` | 33×17 checker in 4444 | **0/561** cells wrong | 4444 bit order correct |
| control | b20 | half-alpha RGBA alpha 128 | PSM / bytes | **8888 / 4,096 B** | anti-aliased alpha stays 8888 |
| baseline | b20 | 256×256 RGBA upload | heap / median | **262,144 B / 21.088 ms** | full-image `calloc` + swizzle, 30 runs |
| change | b21 | same | heap / median | **0 heap / 19.849 ms** | 8-row band (≤16 KiB stack); min 19.780, max 20.046 |
| rejected | b21 | 128-B block, uncached alias | median | **26.202 ms** | strided source reads + uncached writes; reverted |
| control | b21 | `batch_check` 8888 / `auto` | pixels | **byte-identical** to b20/default | direct upload changed no pixel |
| control | b21 | `basic_shapes` 8888 | pixels | **byte-identical** to pspGL | |
| control | b21 | `texture_pattern` `auto` | 5650 / 4444 | **(200,36,80)**; **0/561** NPOT cells | bit orders still correct |

## 8. Exit criteria

- [x] T1: a 33×17 texture samples correctly, cell by cell, no shear or wraparound — 0/561 wrong
- [x] the padded bytes are allocated, zeroed and counted; the VRAM line reports them — 8,192 B for 33×17
- [x] the default 2D gate is 0 differing pixels and `basic_shapes` is 284 colours
- [x] T2: `PSP_GU_TEXFMT=auto` implemented; 32×32 GL_RGB = 2,048 B; its pixel delta is counted and recorded
      (7,843 px `batch_check`, 0 `basic_shapes`); the default build is still 0 differing pixels
- [x] T3: 100-cycle churn returns free and largest within 512 B, no allocation failure; the boot line reports
      used/free/largest/allocs/frees; a texture unload returns its blocks — **exact return**, two runs
- [x] T4: 256×256 upload uses no full-texture scratch (0 heap, 8-row band); time measured **21.088 → 19.849 ms**;
      pixels unchanged; no whole-cache invalidate added
- [x] phase-6 facts written back into the phase page and `memory-map.md`
- [x] T2/T3/T4 either done with evidence or explicitly carried to sprint 016 with the reason — all four done

## 9. Result

**Sprint 015 closed 4/4 on `b21`.** Every task changed a measurable number, and the default build never moved a
parity pixel.

| task | result |
|---|---|
| T1 NPOT | 33×17 checkerboard **281/561 → 0/561** wrong cells; 64×32 padding reported (8,192 B); parity scenes byte-identical |
| T3 allocator | 512-byte blocks, free/coalesce, real largest block; churn 100 × 16 textures returned **916,992 → 916,992 B** free and largest, 1,600 frees, **0 failures**; 32 KiB metadata |
| T2 per-texture PSM | default 8888 unchanged; `auto`: 32×32 GL_RGB **4,096 → 2,048 B**, font 65,536 → 32,768; texture VRAM **69,636 → 34,820 B**; delta 7,843 px in `batch_check`, **0** in `basic_shapes`; 5650/4444 verified by drawn colour |
| T4 direct upload | full-image heap scratch **262,144 B → 0**; upload **21.088 → 19.849 ms** (−5.9 %); default and `auto` gates byte-identical |

The 128-byte-block + uncached-alias version of T4 was measured at **26.2 ms** and rejected; the shipped path is an
8-row band (≤16 KiB stack) with a cached base and one range writeback. T2's default remains 8888 because the
`auto` delta is a measured trade, not an accepted default.

What remains in phase 6 is residency/eviction (sprint 016); CLUT atlases and mipmaps are still measurement-gated.
The churn probe also found a driver-level hazard: texture uploads with no `EndDrawing` fill the 240 KiB list; the
probe ends a frame per cycle and the driver guard is a follow-up.

## 10. Follow-ups

- Residency and priority-aware eviction: sprint 016, after T3's allocator.
- **NPOT padding can cost 4×:** a 480×272 texture pads to 512×512 = 1 MiB. Large sheets need 512-pixel windows
  (phase 7) or an explicit size guard; do not hide the padded size in the VRAM line.
- 16-bit alpha (`4444`) is a measured trade, not a default: the parity gate wins until a game accepts the delta.
- CLUT (`T4`/`T8`) glyph atlases and mipmaps: only if an upload/footprint measurement says they pay.
- Allocator metadata (32 KiB) lives in main RAM; if it ever shows in a profile, a smaller block size or a
  two-level table is the lever.
- **Texture-only workloads with no swap can overrun the 240 KiB display list.** The churn probe hit it at ~800
  texture binds; GU has no capacity check. Add a high-water guard or a periodic finish in the driver before a real
  loading screen relies on texture upload without `EndDrawing` (phase 11 or a small follow-up).
