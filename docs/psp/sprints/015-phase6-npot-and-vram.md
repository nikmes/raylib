# Sprint 015 — NPOT textures, per-texture format and the VRAM allocator

| | |
|---|---|
| Roadmap phase | [Phase 6 — Textures and VRAM](../phases/06-textures-vram.md) |
| Branch / base commit | `master` @ `c31afe31` (T1 landed in `719b037e`) |
| Started / closed | 2026-11-21 / — |
| Build number(s) | `b16` baseline, `b17` T1, `b19` T3, `b20` T2, `b21` T4 |
| Tasks | 2/4 done |

## 0. Handover

**Situation.** Phase 5 closed with pspgu owning the upload path and the 2D corpus byte-identical to pspGL; the
ceiling is 2,375 sprites at 60 fps. **T1 is done on `b17`**: both texture dimensions pad to powers of two, the
padding is zeroed and swizzled, and `sceGuTexScale` restores the logical size (33×17 went from 281/561 wrong cells
to 0/561, with both parity scenes unchanged). The rest of the sprint is the memory work: T2 per-texture formats,
T3 a real allocator, T4 direct swizzle. The allocator is still a **bump arena with no free**, so it cannot
reclaim a deleted texture and cannot report a largest block.

**Read before acting**, in this order:

1. `../memory-map.md` §3 and §“VRAM: 2 MB is the whole budget” — the three-buffer map, the host-RAM lists/arenas,
   the 64-byte writeback rule and the NPOT padding note.
2. `../phases/06-textures-vram.md` — the phase tasks and exit criteria this sprint serves.
3. `tools/pspgu/gl_texture.c` — `upload8888`, `applyTexture`, `glTexImage2D`, `glTexSubImage2D`; and
   `tools/pspgu/egl.c` — `pspguVramAlloc`, `pspguVramReport`, the three-buffer map.
4. `../sprints/012-phase5-texture-parity.md` T2 and `../sprints/014-phase5-ceiling-vram-readback.md` §9 — the
   NPOT diagnosis and the measured VRAM map this allocator replaces.

**First action.** Implement **T3** in `tools/pspgu/egl.c`: replace `pspguVramAlloc` with the 512-byte block
allocator described below, keeping the three framebuffers and depth as fixed reservations. T2 and T4 use its
allocation and free API, so this comes first.

**Implementation order.** T1 done → **T3** → **T2** → **T4**. T2 can be written against the old API, but its
memory claims are only evidence after T3 exists. T4 depends on T3's block (and on T1's `padW/padH`).

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
- **Check the build marker before measuring** (`[PSPGU] GU up: pspgu bNN`) and run
  `tools/pspgu-build.sh --verify-install`. The symbol content hash does not change when code does.
- Commit messages carry the task ID: `sprint 015 T3: ...`.

**Commands.**

```sh
export PSPDEV=/home/psp/pspdev
export PATH=$PATH:$PSPDEV/bin

cd /home/psp/raylib-psp
./tools/pspgu-build.sh                      # build + install pspgu
./tools/pspgu-build.sh --verify-install     # installed objects == fresh build
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
| T2 | **Per-texture PSM** behind `PSP_GU_TEXFMT` (`8888` default, `auto` measured): opaque `GL_RGB` → `5650`; alpha sources → `4444` when every alpha is 0/255 (binary), else `8888`; one decision and one log line per texture | `todo` | 32×32 texture **4,096 → 2,048 B**; `auto`'s pixel delta vs `8888` counted by scene; default `batch_check`/`basic_shapes` still 0; half-alpha pattern checked for 4-bit quantisation | — |
| T3 | **Best-fit allocator** over 512-byte blocks with coalescing; free bytes and largest block tracked separately; one allocator for framebuffers, depth and textures; `pspguVramFree` + stats | `done` | 100 load/unload cycles: free and largest return within 512 B, no failure; boot line reports used/free/largest/allocs/frees; a texture unload returns its blocks; 2D gate 0 | **PASS twice.** 100 cycles × 16 textures: `free 916,992 → 916,992`, `largest 916,992 → 916,992`, `internal 508` (the 4-byte white texel's block), `allocs 1,606 / frees 1,600 / failures 0`. Boot line `reserved=1,114,112 requested=4 used=1,114,624 free=982,528 largest=982,528 internal=508`; assets `requested=69,636 used=1,184,256 free=912,896`. 2D gate **byte-identical**. `b19`; probe `psp-tests/texture_churn`, verdict in `churn_done.txt` |
| T4 | **Direct swizzle into VRAM**: expand one **4×8-texel swizzle block** into a 128-byte stack tile and write it to the destination through the uncached alias; remove the full-texture `calloc` + memcpy | `todo` | 256×256 RGBA upload: scratch allocation gone (262,144 B → 0 heap; 128 B stack), elapsed time before/after, pixels unchanged; no whole-cache invalidate | — |

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
  source format: RGBA byte 3, LUMINANCE_ALPHA byte 1, packed-16 alpha bit; `GL_RGB` has no alpha to scan.
- Extend the upload paths to pack 5650/4444 directly (no 8888 round trip). Keep `GU_TCC_RGBA`; 5650 ignores
  alpha, 4444 uses it.
- Log once per texture: `texture %u: %ux%u src=0x%x -> PSM 0x%x (%u B)`.
- Verification: the `auto` 32×32 texture is 2,048 B; the `8888` default gate is 0; the `auto` build's pixel delta
  is counted per scene (not required to be 0); the half-alpha pattern's 50 % blend is checked within 4-bit
  quantisation. If `auto`'s delta is unacceptable, the default stays 8888 and the sprint records the measured
  trade rather than hiding it.

### T4 design (implementation-ready)

- Replace the full-texture `calloc(total)` + `swizzleFast` with a per-block path: one swizzle block is
  **16 bytes × 8 rows = 128 B** (4 texels wide at 8888, 8 at 16-bit). Expand the source texels for one block into
  a 128-byte stack tile, then write those 128 bytes to the block's swizzled destination address
  (`block = (y/8)*(tbw*bpp/16) + x/(16/bpp); dst = base + block*128`).
- Use the uncached EDRAM alias for the destination (`0x40000000 | offset`, the form the reference engines use) so
  no cache writeback is needed; if the alias is not proven, write back exactly the 128-byte block. Do not add a
  whole-cache invalidate.
- Verification: the 33×17 NPOT capture and the two parity scenes are unchanged; a 256×256 RGBA upload's elapsed
  time is logged before/after; the upload's heap peak drops from 262,144 B to 0 (128 B stack). Build the `Image`
  once and exclude PNG/file decode from the timed region: time `LoadTextureFromImage` (which calls the driver's
  `glTexImage2D`) and `UnloadTexture` in the `UPLOAD` mode, take the median of 30 runs. The source format
  conversion (LA/RGBA to 8888, or the T2 16-bit pack) happens inside the tile loop.

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

## 6. Budgets

| budget | limit | measured | tag |
|---|---|---|---|
| 2D gate, differing pixels (default) | 0 | **0** (`batch_check`); **284/0** (`basic_shapes`) | `[measured: b17]` |
| NPOT correctness | every 33×17 cell single-colour, correct parity | **0/561 wrong**, two colours | `[measured: b17]` |
| NPOT padding cost, 33×17 | 64×32 = 8,192 B | **8,192 B** (logical 2,244 B; old 4,352 B) | `[measured]` |
| 32×32 texture, `auto` | 2,048 B (4,096 B at 8888) | not started (T2) | — |
| texture VRAM after T2, `batch_check` | measured, ≤ 69,636 B | not started (T2) | — |
| block rounding / internal fragmentation | reported, not hidden | **508 B** (the 4-byte white texel's block) | `[measured: b19]` |
| free tail after assets (depth=1) | — | **912,896 B** (`batch_check` at b19); 916,992 B in the churn baseline | `[measured: b19]` |
| largest free block | real, not a free-tail alias | **982,528 B** at boot; **912,896 B** after assets; **916,992 B** at churn | `[measured: b19]` |
| churn: free/largest return | within 512 B after 100 cycles | **exact**: 916,992 → 916,992 both | `[measured: b19, two runs]` |
| allocator metadata | main RAM, cache-resident if possible | **32 KiB** (4,096 × 8 B) | `[measured]` |
| upload 256×256 RGBA, heap | 0 (128 B stack) vs 262,144 B today | not started (T4) | — |
| upload 256×256 RGBA, time | measured before/after | not started (T4) | — |
| whole-cache invalidate in a steady frame | 0 | 0 today; keep 0 | `[measured]` |

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
| control | b20 | `batch_check` default | gate | expected 0 | T2 must not move it |
| change | b20 | `batch_check` `auto` | pixel delta / texture bytes | | T2 |
| change | b20 | `texture_pattern` `auto` | 32×32 bytes | expected 2,048 | T2 |
| baseline | b20 | 256×256 upload | heap / ms | 262,144 B / | T4 |
| change | b21 | 256×256 upload | heap / ms | 0 / | T4 |

## 8. Exit criteria

- [x] T1: a 33×17 texture samples correctly, cell by cell, no shear or wraparound — 0/561 wrong
- [x] the padded bytes are allocated, zeroed and counted; the VRAM line reports them — 8,192 B for 33×17
- [x] the default 2D gate is 0 differing pixels and `basic_shapes` is 284 colours
- [ ] T2: `PSP_GU_TEXFMT=auto` implemented; 32×32 = 2,048 B; its pixel delta is counted and recorded; the default
      build is still 0 differing pixels
- [x] T3: 100-cycle churn returns free and largest within 512 B, no allocation failure; the boot line reports
      used/free/largest/allocs/frees; a texture unload returns its blocks — **exact return**, two runs
- [ ] T4: 256×256 upload uses no full-texture scratch (0 heap, 128 B stack); time measured; pixels unchanged;
      no whole-cache invalidate added
- [ ] phase-6 facts written back into the phase page and `memory-map.md`
- [ ] T2/T3/T4 either done with evidence or explicitly carried to sprint 016 with the reason

## 9. Result so far

T1 and T3 are closed. T3 replaced the bump arena with a 512-byte block best-fit allocator: the three framebuffers
and depth are fixed 8 KiB-aligned reservations, textures allocate/free with coalescing, and the boot line reports
`reserved/requested/used/free/largest/internal/allocs/frees/failures` from the table. The churn probe passed twice:
**free and largest returned exactly** (916,992 → 916,992 B), 1,600 frees, **0 failures**, with 32 KiB of metadata
in main RAM. The 2D gate did not move.

The probe also found a driver-level hazard worth carrying: a workload that uploads textures **without ever ending a
frame** fills the 240 KiB display list and hangs (it stopped at cycle ~50 with the allocator still perfect). GU has
no list-capacity check. The probe now ends a frame per cycle; the driver needs a high-water guard (follow-up).

T2 and T4 are implementation-ready and remain.

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
