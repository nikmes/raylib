# Sprint 015 — NPOT textures, per-texture format and the VRAM allocator

| | |
|---|---|
| Roadmap phase | [Phase 6 — Textures and VRAM](../phases/06-textures-vram.md) |
| Branch / base commit | `master` @ `c31afe31` |
| Started / closed | 2026-11-21 / — |
| Build number(s) | `b16` (baseline), `b17` (T1) |
| Tasks | 1/4 done |

## 0. Handover

**Situation.** Phase 5 closed with pspgu owning the upload path and the whole 2D corpus byte-identical to pspGL.
The 60 fps ceiling is 2,375 sprites; fill and per-sprite CPU work are the remaining 2D limits, not submission.
VRAM is now the tight budget: three 272 KiB framebuffers + 272 KiB depth leave 913,344 B for textures, and the
allocator is still a **bump arena with no free** — it cannot reclaim a deleted texture and has no largest-block
metric. Non-power-of-two textures are a known defect: they fall back to linear addressing and sample wrong
(sprint 012 T2 diagnosed the fix but it was never implemented).

**Read before acting**, in this order:

1. `../memory-map.md` §3 and §“VRAM: 2 MB is the whole budget” — the new three-buffer map, the host-RAM lists and
   arenas, and the 64-byte writeback rule.
2. `../phases/06-textures-vram.md` — the phase tasks and exit criteria this sprint serves.
3. `tools/pspgu/gl_texture.c` — `upload8888`, `applyTexture`, `glTexImage2D`, `glTexSubImage2D` and the bump
   allocator in `egl.c` (`pspguVramAlloc`).
4. `../sprints/012-phase5-texture-parity.md` T2 — the exact 33×17 diagnosis and why `TexScale` is not identity
   for a padded texture.

**First action.** Build `psp-tests/texture_pattern` with `PATTERN_NPOT=1`, install pspgu, run it and capture the
**baseline** before touching `gl_texture.c`. The Makefile now honours that flag; it did not before, which is the
same `EXTRA_CFLAGS` class of trap sprint 013 recorded.

**Hard rules.**

- **The 2D parity scene must not include the NPOT texture.** pspGL hangs on a 33×17 texture; `PATTERN_NPOT` is a
  capability test with its own oracle (the generated checkerboard), not a pspGL comparison.
- **The 2D gate stays at 0 differing pixels** after every change. `batch_check` and `basic_shapes` are the
  regression gate; a texture-format change that moves a parity pixel is wrong even if it saves memory.
- **Padding is real memory.** A padded height is allocated, zeroed, swizzled and counted in VRAM. Do not report
  the logical size as the allocated size.
- **`sceGuTexScale` is per texture.** `applyTexture` must set `realW/padW`, `realH/padH` after every
  `sceGuTexMode`; a power-of-two texture sets 1,1. This is the conclusion sprint 012 deferred.
- **No “largest free block” until T3 builds one.** The current allocator can only report the free tail.
- **Check the build marker before measuring** (`[PSPGU] GU up: pspgu bNN`). `tools/pspgu-build.sh
  --verify-install` compares installed object bytes with a fresh build.
- Commit messages carry the task ID: `sprint 015 T1: ...`.

**Commands.**

```sh
export PSPDEV=/home/psp/pspdev
export PATH=$PATH:$PSPDEV/bin

cd /home/psp/raylib-psp
./tools/pspgu-build.sh                  # build + install pspgu
./tools/pspgu-build.sh --verify-install # installed objects == fresh build
./tools/pspgu-build.sh --restore        # pspGL back; --check must say 2ab7378e

cd psp-tests/texture_pattern
make clean && make PATTERN_NPOT=1
cp texture_pattern.prx /tmp/s3/app.prx
cd /tmp/s3 && pspsh -e "reset"; sleep 6
( printf './app.prx\n'; sleep 18 ) | pspsh
# after DONE and sleep only:
pspsh -e "scrshot host0:/npot_after.raw"
```

**Do not touch.** The batcher in `src/`, `gl_draw.c`'s vertex paths, the `MESH_SCENE`/`DEPTH_SCENE` renders, and
`DrawMesh` (phase 10).

**Done means** §8: NPOT samples correctly, the 2D gate is unchanged, and every task's evidence is in its row.

## 1. Objective

A 33×17 texture drawn at 1:1 renders its generated checkerboard exactly: every one of the 33×17 cells is a single
colour, the cells alternate `(0,128,255)`/`(255,255,0)` with `(x+y)&1`, and there is no shear or wraparound. The
padded allocation and `sceGuTexScale` correction are reported in bytes, and the two parity scenes still render
byte-identically.

## 2. Why now

Phase 6's first precondition — “a driver that owns VRAM” — became true when phase 5 closed. NPOT is the smallest
known defect and has a written diagnosis; the allocator and per-texture formats are the phase's actual exit work
and cannot be measured until texture allocation is honest about padding and free space.

## 3. Scope

**In:** NPOT padding + `sceGuTexScale`; per-texture PSM; a best-fit allocator with coalescing and real
free/largest-block counters; direct swizzle into VRAM.

**Out:** residency/eviction policy (sprint 016, needs T3's allocator), CLUT/T4 glyph atlases and mipmaps (only if a
measurement says they pay), `DrawMesh` (phase 10), the batcher.

## 4. Preconditions

- [x] pspgu installed, boot marker visible, `--verify-install` matches
- [x] pspGL restorable, `pspgl-build.sh --check` content hash `2ab7378e`
- [x] 2D gate re-confirmed at 0 differing pixels on the final phase-5 build
- [x] `texture_pattern` builds with `PATTERN_NPOT=1` and the Makefile actually defines it — the flag was
      missing from the Makefile; fixed here and the app now logs `PATTERN NPOT=1`
- [x] Baseline capture of the 33×17 checkerboard on `b16`, before the code change — 281 of 561 cells wrong
- [ ] `pspsh` answers; `reset` per run; capture only after `DONE` and two identical captures

## 5. Tasks

| ID | task | status | verification | evidence |
|---|---|---|---|---|
| T1 | **NPOT textures (phase task 6c)**: pad **both** dimensions to powers of two (height at least 8 rows for the swizzle), zero the padding, swizzle the padded buffer, and correct with `sceGuTexScale(realW/padW, realH/padH)`; keep the logical size for GL | `done` | 33×17 checkerboard at (160,30), 3×: every cell single-colour and matching `(x+y)&1`; no shear; `texture_pattern` non-NPOT and the 2D gate unchanged | **281/561 cells wrong → 0/561**, exactly two colours, `(x+y)&1` correct, no shear. 33×17 pads to 64×32 = **8,192 B** (the logical data is 2,244 B; the old linear allocation was 4,352 B). `batch_check` **0 differing pixels**; `basic_shapes` **284 colours, 0 differing pixels**. Driver `b17` |
| T2 | **Per-texture PSM**: opaque → `5650`, per-texel alpha → `5551`/`4444`, `8888` where fidelity requires it; one format decision per texture | `todo` | 32×32 sprite 4 KiB → 2 KiB in the VRAM line; parity captures unchanged for the scenes that do not use NPOT | — |
| T3 | **Best-fit allocator** over 512-byte blocks with coalescing; free bytes and largest block tracked separately; one allocator for framebuffers and textures | `todo` | repeated load/unload of textures returns memory; largest block is real; fragmentation bounded | — |
| T4 | **Direct swizzle into VRAM**: remove the CPU scratch + memcpy, write the destination through the uncached alias (or write back the exact range) | `todo` | upload time and peak heap measured; pixels unchanged | — |

Status vocabulary: `todo` / `doing` / `done` / `blocked` / `dropped`. IDs are stable.

### Task log (append-only)

- 2026-11-21 — sprint opened, all tasks `todo`.
- 2026-11-21 — `texture_pattern`'s Makefile now defines `PATTERN_NPOT`; before this, `make PATTERN_NPOT=1`
  silently built the default scene. The app logs `PATTERN NPOT=1` so a stale build cannot masquerade again.
- 2026-11-21 — T1 `doing`: baseline on b16, 281/561 cells wrong, parity flips at x=17.
- 2026-11-21 — T1 `done`: pad both dimensions to powers of two, swizzle the padded buffer, per-texture
  `sceGuTexScale`; b17 gives 0/561 wrong, two colours; 2D gate and `basic_shapes` byte-identical.

## 6. Budgets

| budget | limit | measured | tag |
|---|---|---|---|
| 2D gate, differing pixels | 0 | **0** (`batch_check`); **284 colours, 0** (`basic_shapes`) | `[measured: b17]` |
| NPOT correctness | every 33×17 cell single-colour, correct parity | **0/561 wrong**, two colours | `[measured: b17]` |
| NPOT padding cost, 33×17 | 64×32 = 8,192 B | **8,192 B** (logical 2,244 B; old linear 4,352 B) | `[measured]` |
| 32×32 sprite texture, bytes | 4 KiB (8888) → 2 KiB (16-bit) | not started (T2) | — |
| texture VRAM, `texture_pattern` | — | **77,956 B** after assets (was 8,516 B for the three app textures alone at the old sizes; the app total includes the font atlas) | `[measured: b17]` |
| free tail after assets (depth=1) | — | **905,024 B** in `texture_pattern`; 913,344 B in `batch_check` | `[measured: b17]` |
| largest free block | only after T3; the bump allocator cannot report it | — | — |
| upload time, 256×256 RGBA | measured, no target yet | not started (T4) | — |

## 7. Measurements

Protocol: device only, one variable per run, control → change → control again. Every row carries the boot marker.

| run | build | workload | metric | value | notes |
|---|---|---|---|---|---|
| baseline | b16 | `texture_pattern PATTERN_NPOT=1` | 33×17 cells wrong | **281 / 561**, 4 colours | correct for x=0..16, parity flips at x=17 |
| change | b17 | same | 33×17 cells wrong | **0 / 561**, 2 colours | `(x+y)&1` correct, no shear |
| control | b17 | `batch_check` | differing pixels vs pspGL | **0** | byte-identical after two identical captures |
| control | b17 | `basic_shapes` | colours / differing pixels | **284 / 0** | byte-identical to the pspGL capture |
| change | b17 | `texture_pattern` | texture VRAM after assets | **77,956 B** | 33×17 grew from 4,352 B to 8,192 B; the 4×4 grew from 64 B to 256 B |

## 8. Exit criteria

- [x] T1: a 33×17 texture samples correctly, cell by cell, with no shear or wraparound — 0/561 wrong
- [x] the padded bytes are allocated, zeroed and counted; the VRAM line reports them — 8,192 B for 33×17
- [x] the 2D gate is 0 differing pixels and `basic_shapes` is 284 colours
- [ ] T2/T3/T4 either done with evidence or explicitly carried to sprint 016 with the reason — T2–T4 remain
- [ ] phase-6 facts written back into the phase page and `memory-map.md` — phase page updated; memory-map
      padding note still to add

## 9. Result

T1 is closed. Non-power-of-two textures now pad both dimensions to powers of two, zero the padding, swizzle the
padded image, and restore the logical size with `sceGuTexScale(realW/padW, realH/padH)`. The 33×17 checkerboard
went from **281/561 wrong cells** (parity flipped at x=17) to **0/561**, exactly two colours, no shear. The two
parity scenes did not move: `batch_check` byte-identical and `basic_shapes` 284 colours / 0 differing pixels.

The cost is honest and counted: 33×17 allocates 64×32×4 = **8,192 B** instead of the old linear 4,352 B; the
logical texels are 2,244 B. T2–T4 (per-texture PSM, best-fit allocator, direct swizzle) remain for the rest of
the sprint.

## 10. Follow-ups

- Residency and priority-aware eviction: sprint 016, after T3's allocator.
- CLUT (`T4`/`T8`) glyph atlases and mipmaps: only if an upload/footprint measurement says they pay.
- Texture-cache strip width and 512-pixel windows for large sheets: phase 7.
