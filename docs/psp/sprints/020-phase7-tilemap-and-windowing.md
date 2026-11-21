# Sprint 020 — Tile maps and 512-pixel windowing

| | |
|---|---|
| Roadmap phase | [Phase 7 — 2D specialisation](../phases/07-2d-specialisation/index.md) |
| Branch / base commit | `master` @ `7e26613b` (sprint 019 closed) |
| Started / closed | 2026-11-21 / 2026-11-21 |
| Build number(s) | pspgu `b24` (upload + window split); probe and scene builds |
| Tasks | 6/6 done |

## 0. Handover

**Situation.** Sprint 018 audited the 512-pixel paths and found the driver's dangerous case: a texture wider than
512 is **allocated and marked used**, then `uploadShadowToVram` logs `tbw … exceeds the 512-pixel band` and
returns (`tools/pspgu/gl_texture.c:231`). The texture id is valid and the GE samples uninitialised VRAM. Sprint
019 closed the font-shaped half by rejecting generated atlases above 512; the general case — sprites, tile
sheets, backgrounds — is still silent. The SDK envelope is now confirmed: `sceGuTexImage` requires width and
height to be powers of two **≤512**, and `tbw` (the row stride) may be **up to 1024**, block-aligned
(`psp_sdk_660/usr/local/psp/devkit/src/gu/debug.c:830-841`). A 1024-wide sheet can therefore be sampled as two
512-wide windows with `tbw=1024`, if the probe in T1 proves the swizzled block offset works.

**Read before acting**, in this order:

1. `../sprints/018-phase7-corpus-and-budget.md` §7 — the 512 audit table and the silent-upload finding.
2. `../sprints/019-phase7-text-work.md` §9 — the font guard, so the general path does not regress it.
3. `../phases/07-2d-specialisation/index.md` — R1 (window slicing), the tile-map task and the phase exit criterion.
4. `../memory-map.md` §4 — the VRAM map and the corrected width >512 wording.
5. `tools/pspgu/gl_texture.c` — `uploadShadowToVram`, `applyTexture`, `residencyAlloc`, `swizzledOffset`;
   `tools/pspgu/gl_draw.c` — `glDrawArrays`, the batched layout and `arenaCopy`.
6. `psp-tests/gated_features/main.c` — the raw-GU init and capture pattern the probe reuses.

**First action.** Write `psp-tests/gu_window` (T1) and run it before changing `tools/pspgu/`. It uploads a
1024×128 and a 128×1024 sheet twice — linear and swizzled — then draws 512-pixel windows with `tbw` equal to the
full stride and samples known pixels. No driver change until the probe says which layout works.

**Hard rules.**

- **The probe gates the design.** Do not implement windowing on the assumption that swizzled windows work. If the
  probe shows only the linear layout is correct, oversized textures are uploaded linear and the driver records
  why; if neither works, the sprint fails >512 loudly and records windowing as blocked with the probe evidence.
- **The 2D gate is sacred.** Textures ≤512 must keep the current upload, `applyTexture` and draw path bit for
  bit. `batch_check` must stay byte-identical to `par_pspgl.raw`, and the 018/019 captures must not move.
- **A silent skip is not allowed to survive.** Every >512 texture either renders through windows or is rejected
  with a log naming the size; `t->used = 1` with no upload is the bug this sprint removes.
- **Full stride, power-of-two windows.** `tbw = nextPow2(width)` (≤1024). Window `width`/`height` are powers of
  two ≤512; the last window may be smaller. `tbp` is the **swizzled** block offset (or the linear byte offset),
  never `x*bpp` for a swizzled texture. `sceGuTexScale` is set per window.
- **Draw order is not negotiable.** Splitting a primitive must not reorder it relative to its neighbours; a quad
  also must not overlap its own split pieces. Emit in primitive order, windows inside one primitive in a
  deterministic order.
- **The arena bounds the split.** Worst case is a triangle clipped into four windows, up to 12 triangles; compute
  the bound and use `pspguArenaReset` fallback rather than overwriting a list the GE may still read.
- **Do not change the allocator policy.** `residencyAlloc` already allocates `padW*padH`; this sprint changes the
  upload and draw paths, not eviction.
- **Stage only this sprint's paths.** Other sessions edit the tree; `git status` before every commit and never
  `git add -A`.
- Commit messages carry the task ID: `sprint 020 T3: ...`.

**Commands.**

```sh
export PSPDEV=/home/psp/pspdev
export PATH=$PATH:$PSPDEV/bin

cd /home/psp/raylib-psp
./tools/pspgu-build.sh                         # b24 after T2/T3
cd src && make PLATFORM=PLATFORM_PSP -j8 && cp libraylib.a $PSPDEV/psp/lib/libraylib.a

cd psp-tests/gu_window
make clean && make
cp gu_window.prx /tmp/s3/app.prx
cd /tmp/s3 && timeout 20 pspsh -e "reset"; sleep 6
( printf './app.prx\n'; sleep 12 ) | timeout 35 pspsh
pspsh -e "scrshot host0:/guwin.raw"             # after DONE only

cd /home/psp/raylib-psp/psp-tests/scene_2d
make clean && make MODE=oversize WIDE=1024 HIGH=128
make clean && make MODE=tiles ATLAS_WIDE=1
```

**Do not touch.** The text path from sprint 019, the allocator/eviction policy, the batcher's primitive choice
for textures ≤512, `DrawMesh`, and any 3D path (phase 10).

**Done means** §8: the probe verdict, tbw≤1024 uploads, oversized draws pixel-identical to the pre-sliced
reference with seams included, tile draws proportional to windows/runs not tiles, and the gate at 0.

## 1. Objective

Five falsifiable results:

1. **A probe verdict** for 1024-stride windows: linear and swizzled, horizontal and vertical, with the sampled
   colour of every window edge recorded. This is the input to T2/T3, not an assumption.
2. **Upload support to `tbw=1024`** with the 8-row band chunked so no 32 KiB stack is needed; `tbw>1024` is
   rejected loudly and the texture is not marked used.
3. **Draw-time windows**: a 1024×128 sheet drawn through two 512-wide windows is **pixel-identical** to the same
   pixels uploaded as two 512×128 textures, including the seam column. A 128×1024 sheet is identical vertically.
4. **Tile runs**: a 1024-wide tile atlas draws with submissions proportional to window/state runs, not tile
   count, and the same visible tiles as the two-texture reference.
5. **No regression**: the 2D gate, the 018 text/tile/particle captures and the 019 text captures are unchanged;
   no `tbw … exceeds` silent skip remains in any run.

## 2. Why now

Phase 6 and sprint 018 left two correctness holes above 512: the driver skips the upload, and `applyTexture`
passes `padW` (possibly 1024) to `sceGuTexImage`, which only accepts ≤512. Every 2D workload that uses a tile
sheet, a background or a large sprite can hit it; the tile-map half of Phase 7 cannot be called done while a
legal asset renders uninitialised memory. This sprint closes both and leaves the cache-strip experiment (023)
and the shape/sprite fast paths (021) untouched.

## 3. Scope

**In:** `psp-tests/gu_window`; `tools/pspgu/gl_texture.c` upload and bind; `tools/pspgu/gl_draw.c` batched draw
split; `scene_2d` oversize and wide-atlas modes; the 512 audit follow-through in the docs.

**Out:** 3D/mesh windowing (phase 10), mipmaps (dropped), CLUT (dropped), the allocator/eviction policy, the
text path, cache-strip widths (023), and shape/sprite fast paths (021).

## 4. Preconditions

- [x] Sprint 019 closed at `7e26613b`; pspgu `b23` and the text guard are in the tree
- [x] `/home/psp/evidence/sprint018/` and `sprint019/` baselines are present
- [x] `par_pspgl.raw` (`b69830b8…`) is present in `/tmp/s3`
- [x] `psp_sdk_660` debug source confirms width/height ≤512 and `tbw` ≤1024
- [x] Device loop available: `usbhostfs_pc` at `/tmp/s3`, `pspsh`, `reset` between runs
- [x] pspGL restorable (`2ab7378e`)

## 5. Tasks

| ID | task | status | verification | evidence |
|---|---|---|---|---|
| T1 | **Window probe** (`psp-tests/gu_window`): 1024×128 and 128×1024 sheets, linear and swizzled, windows 0/512 with full stride; sample edge pixels | `done` | every window edge matches the source colour; marker states verdict per layout/orientation | `VERDICT control=1 linear_h=1 swizzled_h=1 linear_v=1 swizzled_v=1`: all 8 window cases and the tbw=512 control pass. Instrument fixes before the result was believed: EDRAM **destination** writeback (not just the source), a 2 ms settle after the CPU-to-EDRAM copy, and a vertex writeback. `/home/psp/evidence/sprint020/gu_window.txt` |
| T2 | **Upload to tbw 1024**: chunk the 8-row band at 512 px, support linear and swizzled; `tbw>1024` or `padW/padH>1024` fails loudly and the texture is not used | `done` | a 1024×128 8888 uploads (no `exceeds` log), PSM/bytes correct; 2048-wide fails with one error | 1024×128 and 128×1024 upload as **524,288 B**, no `exceeds`, free falls to 392,704 B; 2048×128 logs one `exceeds the GE 1024-pixel buffer limit`, creates no texture and leaves free at 916,992 B. Band chunking keeps the stack at 16 KiB |
| T3 | **Draw-time windows**: bind clamps width/height to 512; `glDrawArrays` splits sprites/quads and clips triangles into ≤512 windows with full-stride `tbw`, UV remap and per-window `sceGuTexImage`/`TexScale`; ≤512 path untouched | `done` | oversized sprite/triangle captures equal the pre-sliced reference; no gap/duplicate at the seam; draw order preserved | A 1024×128 sprite is **2 window draws/frame** (2 switches), a rotated sprite **4/frame** (two triangles × two windows), the wide tile atlas **8/frame**, all with 0 extra binds. The triangle clipper needed a two-buffer fix (out-array overflow) before the rotated case stopped crashing |
| T4 | **Oversize scene** (`MODE=oversize WIDE=1024 HIGH=128`, vertical and `ROTATE=20`): draw 1:1 with the boundary on screen and against a two-texture reference | `done` | unrotated horizontal/vertical captures byte-identical to the reference; rotated has no seam gap and samples the expected colours | 1024×128: `39d6547a…` both windowed and two-texture reference; 128×1024: `ec18cdf7…` both. Rotated 20°: no crash, red/green seam pixels present, 4 window draws and 1,800 vertices/frame |
| T5 | **Wide tile atlas** (`MODE=tiles ATLAS_WIDE=1`, `SCROLL=1`): 1024×128 atlas of 64×64 tiles, visible grid crossing x=512; draws/frame and work; compare with a two-texture atlas | `done` | same pixels as the reference; draws scale with window/state runs, not the 28+ tiles; scrolled traversal recorded | Static: **8 window draws, 8 switches, 0 extra binds, 0.876 ms**; reference **8 binds + 8 draws, 0.925 ms**, captures `c34c01ce…` both. `SCROLL=1`: **5.2 window draws/frame, 1 bind, 0.878 ms**; reference 1,439 binds / 1,560 draws, 0.887 ms; captures match |
| T6 | **Regression**: 2D gate, 018 text/tile/particle, 019 text, mixed, VRAM/residency; a 512×512 texture still takes the old path | `done` | gate `b69830b8…`; 018/019 hashes unchanged; 0 evictions in fitting scenes; no silent `exceeds` skip | text8/24 `3.353/9.081 ms` and captures match 019; tiles `2.882`, particles 500 `2.830`, mixed `10.129` and captures match 018; 512×256 takes the old path (`windraws=0`); gate byte-identical; all runs 0 evictions/reloads/failures. 512×512 8888 (1 MiB) does not fit beside the font, so the boundary was tested at 512×256 |

Status vocabulary: `todo` / `doing` / `done` / `blocked` / `dropped`. IDs are stable.

### T1 design

- Raw GU, no raylib/pspgu: reuse `gated_features`' init (2 framebuffers at stride 512, ortho matrices, list).
- Place the texture after the framebuffers (`TEX_OFFSET`), 16-byte aligned. Build a 1024×128 8888 pattern:
  16-pixel colour bands with a one-pixel white line at x=511 and a one-pixel black line at x=512, so a window
  error is visible as a shifted/blank column.
- Upload twice: linear (`memcpy`) and swizzled (copy `swizzledOffset` from `gl_texture.c` into the probe; do not
  link pspgu).
- Window test: `sceGuTexMode(GU_PSM_8888, 0, 0, swizzle)`; for windows x=0 and x=512 call
  `sceGuTexImage(0, 512, 128, 1024, tbpWin)` and draw a 512×128 quad with
  `u ∈ [0,1]`, `v ∈ [0,1]`; for the swizzled arm `tbpWin = base + swizzledOffset(512,0,1024,4)`, for linear
  `base + 512*4`. Set `sceGuTexScale(1,1)`.
- Vertical arm: 128×1024, `tbw=128`, windows y=0/512, `tbpWin` at the y block.
- Read back the framebuffer at the four window corners and at x=511/512 after `sceGuSync(0,0)`; write a marker:
  `WINDOW layout=swizzled orient=h win=0/1 x=0/511/512 color=RRGGBB verdict=OK/FAIL`.
- The probe draws a colour-wheel/marker quad first so a missing texture is distinguishable from a black one.

### T2 design

- `uploadShadowToVram`: replace the fixed `band[8*512*4]` with a loop that copies the 8-row band in **512-pixel
  chunks**:
  - `widthBytes = tbw*bpp`; `chunkPixels = 512`; for `cx` in 0..tbw step 512, for each 8-row band copy
    `min(512, tbw-cx)` pixels per row into a 16 KiB band and write the blocks at
    `block = (by/8)*(widthBytes/16) + (cx + bx)/(16/bpp)`.
  - This keeps the stack at 16 KiB for any tbw and preserves the current code path for tbw≤512 (same loop, one
    chunk).
- Remove the `tbw>512` early return. Add a hard guard at `glTexImage2D`: if `nextPow2(width) > 1024` or
  `nextPow2(height) > 1024`, log once, free the shadow, set `t->used = 0`, leave the texture unusable (raylib
  sees id 0) — never mark a texture used without an upload.
- `applyTexture`: for `padW>512` or `padH>512`, bind the **first window**:
  `sceGuTexImage(0, min(padW,512), min(padH,512), t->tbw, base+offset)` and `TexScale(min(width,512)/winPow,
  min(height,512)/winPow)`. Record that the draw splitter must override it before any primitive.
- Test in `scene_2d` with a 1024×128 8888 texture: the boot log must show `1024x128 … (524288 B)` and no
  `tbw … exceeds` line; VRAM free drops by 524,288 B (± block rounding).

### T3 design

- Expose the bound texture's geometry to `gl_draw.c`:
  `int pspguBoundTextureInfo(int *width, int *height, int *padW, int *padH, int *tbw, int *bpp, int *swizzled, unsigned int *offset);`
  in `pspgu_internal.h`/`gl_texture.c`. `glDrawArrays` calls it when `pspgu.boundTexture` is not the default.
- Fast path unchanged: `padW<=512 && padH<=512` → current `arenaCopy` + one `sceGuDrawArray`.
- Oversized path, batched layout (24-byte vertices, `arrTexCoord.ptr`):
  - Primitive iteration: `GU_SPRITES` consumes 2 vertices, `GU_TRIANGLES` 3. Compute the primitive's texel-space
    bbox from `u*padW`, `v*padH`.
  - Window id = `(x0/512, y0/512)` for the primitive's origin; if bbox fits one window, append as is (with UVs
    remapped). If it crosses, split:
    - Sprite: expand to the axis-aligned quad (TL, BR), cut at x=512/y=512, emit up to 4 sub-quads in TL→BR
      order. Preserve colour and screen coordinates by linear interpolation of the two sprite vertices.
    - Triangle: Sutherland–Hodgman clip against each intersected window rectangle in texel space; fan-triangulate
      each result; interpolate x, y, z and colour.
  - UV remap per window: `u' = (u*padW - wx)/winPow`, `v' = (v*padH - wy)/winPow`; window `winPow` is
    `min(512, nextPow2(winWidth))`, `winHeight` similarly. `winWidth = min(512, width-wx)`,
    `winHeight = min(512, height-wy)`.
  - `tbp` per window: swizzled → `base + offset + swizzledOffset(wx, wy, tbw, bpp)`; linear →
    `base + offset + (wy*tbw + wx)*bpp`. Both are block-aligned for wx/wy = 0 or 512.
  - Emit contiguous same-window primitives in one `sceGuDrawArray`; on a window change, set
    `sceGuTexImage(0, winPow, winPowY, tbw, tbp)` and `sceGuTexScale(1,1)`, then continue. Reuse one arena
    allocation per output run; the worst-case bound is `primCount * 12 * 3` vertices (quad split), computed
    before allocating; if the arena cannot hold it, reset exactly as the immediate path does.
  - The vertex format is GU 24-byte `(u,v,color,x,y,z)`; write splits into the arena and `pspguWriteback`.
- Lines with an oversized texture are not a 2D path; log once and skip rather than sample the wrong window.
- **Order:** append split pieces immediately after their primitive; a window change flushes the current run. A
  later primitive returning to the first window is a new run (correct, and measured in T5).
- **Vertex format:** keep the same `fmt`; only UVs move.

### T4 design

- `MODE=oversize` in `scene_2d`:
  - `WIDE=1024 HIGH=128` or `WIDE=128 HIGH=1024`; generate a deterministic pattern: 16-px colour bands, white
    line at the 511/512 boundary, black line at 512/513; `LoadTextureFromImage`.
  - Horizontal: draw 1:1 at `x=-272` so texture x=512 lands at screen x=240; draw a second copy at `x=240` so
    both windows are visible.
  - Vertical: draw at `y=-376` so y=512 lands at screen y=136.
  - `ROTATE=20`: draw the full sheet scaled into 480×272 with a 20° rotation (triangles crossing the seam).
  - Reference build: upload the same pixel data as two ≤512 textures (left/right or top/bottom) and draw them as
    two quads at the same rectangles. For the unrotated arms the capture must be byte-identical; for the rotated
    arm sample the seam and the four corners and record the marker (a two-texture rotated reference is not
    geometrically identical because each half rotates about a different origin).
  - Marker: `OVERSIZE w=… h=… layout=… ref=… verdict=PASS/FAIL seam=…`.

### T5 design

- `MODE=tiles ATLAS_WIDE=1`: generate a 1024×128 atlas of 64×64 tiles (16×2), each with a distinctive 4-quadrant
  pattern; draw a 7×4 visible grid starting at tile column 6 (so columns 6–7 are window 0, 8–12 window 1).
  - `objects = 28`, expected windows = 2 per row; measure draws/frame and work at `FRAMES=300`.
  - Reference: a 512×128 + 512×128 pair assembled from the same tiles; the capture must match byte-for-byte.
  - `SCROLL=1`: pan one tile per 60 frames across the 120×68 logical map; record draws/frame and work, and that
    the draw count follows window runs, not the 28 tiles.
- Keep the 018 128×128 atlas scene untouched for regression.

### T6 design

- Rebuild `libraylib.a`, install pspgu `b24`, run:
  - `MODE=text LINES=1/8/24` and compare to the 019 hashes; `MODE=tiles`, `MODE=particles COUNT=500`,
    `MODE=mixed`, compare to 018.
  - `MODE=oversize` and `MODE=tiles ATLAS_WIDE=1` parity arms.
  - `batch_check WRAP=0` vs `par_pspgl.raw`.
  - A 512×512 8888 texture probe (scene_2d `MODE=oversize WIDE=512 HIGH=512`) must show exactly the old
    `applyTexture`/draw counters, proving the ≤512 path is untouched.
- VRAM/residency: every fitting scene 0 evictions/reloads/failures; a 1024×128 8888 texture leaves
  ~392 KiB free (from ~917 KiB) and still fits with the font.

## 6. Budgets

| item | budget | why |
|---|---|---|
| probe layouts | linear + swizzled × horizontal + vertical | one verdict per layout, not a guess |
| scene texture | 1024×128 or 128×1024 8888 = 524,288 B | fits the ~917 KiB tail with the font |
| tile atlas | 1024×128, 64×64 tiles | crosses the 512 boundary with real tiles |
| measured frames | 300, two runs | project rule |
| window worst case | 12 triangles per input triangle | arena bound computed before allocation |
| regression | 018/019 hashes + gate | the ≤512 path must not move |

## 7. Measurements

All runs at 300 frames, two per point. Evidence: `/home/psp/evidence/sprint020/`
(`gu_window.txt`, `s20_*_run{1,2}.txt|raw|log`, `gate_b23.raw`, `SHA256SUMS`). Build marker: pspgu
`b24`; `[PSPGU] GU up` still reports the b-number in the probe runs.

**Probe verdict** (`gu_window`, 4 samples per window):

| layout | orient | win | tbp offset | edge colour | verdict |
|---|---|---|---:|---|---|
| linear | h | 0 / 1 | 0 / 2048 | `0000 52aa ad55 ffff` | pass / pass |
| swizzled | h | 0 / 1 | 0 / 16384 | `0000 52aa ad55 ffff` | pass / pass |
| linear | v | 0 / 1 | 0 / 262144 | `0000 52aa ad55 ffff` | pass / pass |
| swizzled | v | 0 / 1 | 0 / 262144 | `0000 52aa ad55 ffff` | pass / pass |
| control | h | 0 | 0 (tbw 512) | `0000 52aa ad55 ffff` | pass |

`VERDICT control=1 linear_h=1 swizzled_h=1 linear_v=1 swizzled_v=1`. Instrument notes: the first runs
failed only on the draw that followed a fresh CPU-to-EDRAM copy; the fixes were a destination-range writeback
(pspgu's own upload does this), a 2 ms settle after the copy, and a vertex writeback. With those in place every
layout works, so the driver keeps the swizzled layout.

**Upload:**

| texture | bytes | tbw | log | free after | verdict |
|---|---:|---:|---|---:|---|
| 1024×128 8888 | 524,288 | 1024 | no `exceeds` | 392,704 | loaded |
| 128×1024 8888 | 524,288 | 128 | no `exceeds` | 392,704 | loaded |
| 2048×128 8888 | — | 2048 | one `exceeds the GE 1024-pixel buffer limit` | 916,992 | rejected, no texture |

**Oversize parity** (SHA-256 prefix):

| scene | reference | windowed | identical | window draws/frame |
|---|---|---|---|---|
| 1024×128 1:1 | `39d6547abb8dfa97` | `39d6547abb8dfa97` | yes | 2 |
| 128×1024 1:1 | `ec18cdf7988c2e88` | `ec18cdf7988c2e88` | yes | 2 |
| 1024×128 rotated 20° | — | `s20_osrotb` | seam red/green present | 4 |
| 512×256 (≤512 path) | — | `s20_os512b` | old path | **0** |

The 1024 windows draw in 2 submissions with 2 switches and no extra bind; the rotated sprite is two triangles,
each split into two windows (4 submissions, 1,800 vertices/frame). The two-texture reference costs 2 binds/frame
and 0.992 ms work; the windowed path 0 binds and 0.947 ms.

**Wide tile atlas** (`ATLAS_WIDE=1`, 28 tiles, columns 6–12, x=512 boundary):

| scene | objects | window draws/frame | switches/frame | binds/frame | work ms | reference match |
|---|---:|---:|---:|---:|---:|---|
| static | 28 | **8** | 8 | 0 (1/run) | 0.876 | `c34c01ce5220839c` both |
| static, two-texture reference | 28 | — | — | 8 | 0.925 | — |
| `SCROLL=1` | 28 | **5.2** | 5.2 | 0 (1/run) | 0.878 | matches `s20_tileswsref` |
| `SCROLL=1`, two-texture reference | 28 | — | — | 4.8 avg | 0.887 | — |

The windowed path emits one GE draw per window run, not per tile: 8 runs for 28 tiles, and 5.2 on average while
scrolling. The two-texture reference needs the same number of draws but one texture bind per run; windowing keeps
one bind for the whole atlas.

**Regression:**

| scene | sprint 020 | baseline | capture |
|---|---|---|---|
| text LINES=8 | 3.353 ms | 3.350 (019) | identical |
| text LINES=24 | 9.081 ms | 9.069 (019) | identical |
| tiles (128² atlas) | 2.882 ms | 3.228 (018) | identical |
| particles 500 | 2.830 ms | 3.285 (018) | identical |
| mixed | 10.129 ms | 10.053 (018) | identical |
| `batch_check` gate | `b69830b8…` | `b69830b8…` | identical |

Every fitting run: 0 evictions, 0 reloads, 0 failed. No `tbw … exceeds` silent skip appears in any 1024-wide run;
the only failure log is the deliberate 2048 rejection.

## 8. Exit criteria

- [x] the probe records a verdict for linear and swizzled windows, horizontal and vertical
- [x] a 1024×128 and a 128×1024 8888 texture upload with `tbw` 1024/128 and no `exceeds` log
- [x] a `tbw` or padded dimension above 1024 fails loudly and the texture is not marked used
- [x] oversized unrotated sprites render byte-identical to the two-texture reference, seam included
- [x] an oversized rotated sprite has no gap or duplicated column at the seam
- [x] the wide tile atlas matches the two-texture reference and its draws follow windows/runs, not tiles
- [x] the ≤512 path is unchanged: 018/019 captures and the gate are byte-identical
- [x] no `tbw … exceeds the 512-pixel band` silent skip remains in any run

## 9. Result

Sprint 020 closed 6/6. The general >512 hole from sprint 018 is closed: wide textures now upload and render
through 512-pixel windows, and anything above the GE's 1024-pixel buffer limit fails loudly instead of sampling
uninitialised VRAM.

- **Probe:** the raw-GU probe proves full-stride windows work in **linear and swizzled** layouts, horizontal and
  vertical: 1024×128 at `tbw=1024` (window 1 `tbp` 2,048 linear / 16,384 swizzled) and 128×1024 at `tbw=128`
  (window 1 `tbp` 262,144). The first runs were instrument failures — source-only writeback, no settle, no vertex
  writeback — not layout failures.
- **Upload:** the 8-row band is chunked at 512 px, so `tbw=1024` keeps the 16 KiB stack. 1024×128 and 128×1024
  load as 524,288 B with no `exceeds`; 2048×128 logs one error, creates no texture and leaves VRAM untouched.
- **Windows:** a 1024×128 sprite is 2 GE draws and 2 switches/frame, with 0 extra binds; a rotated sprite is 4
  (two triangles × two windows). The draw splitter keeps sprites and clipped triangles, remaps UVs into the window,
  sets `sceGuTexImage`/`TexScale(1,1)` per window, and batches contiguous same-window pieces.
- **Parity:** unrotated horizontal and vertical windowed captures are **byte-identical** to the two-texture
  references (`39d6547a…`, `ec18cdf7…`), seams included. The rotated capture has the red/green seam and no crash;
  the triangle clipper needed two 10-vertex buffers before that result was trustworthy.
- **Tile atlas:** 28 tiles crossing x=512 emit **8 window draws/frame with one bind** (0.876 ms); the two-texture
  reference needs 8 binds + 8 draws (0.925 ms) for identical pixels. Scrolling averages 5.2 window runs/frame with
  one bind, versus 1,439 binds over 300 frames for the reference.
- **≤512 path:** a 512×256 texture reports `windraws=0` and takes the old `applyTexture`/`glDrawArrays` path. A
  512×512 8888 texture is 1 MiB and does not fit beside the font, so the boundary was tested at 512×256.
- **Regression:** text 8/24, tiles, particles and mixed captures match 018/019; `batch_check` is byte-identical to
  pspGL (`b69830b8…`); every fitting run has 0 evictions/reloads/failures.

## 10. Follow-ups

- **Sprint 021 — sprites and shapes**: offscreen rejection and the 018 shape baseline (4 draws, 10,000
  vertices, 1 bind + 1 unbind per frame).
- **Sprint 022 — particles**: one dynamic stream per texture run; the 018 baseline is 1 draw at 2,000.
- **Sprint 023 — cache strips, regression, phase close**: the strip A/B, full sample comparison, exit table.
- A `tbw` above 1024 (or a padded dimension above 1024) is a hard limit and is now reported; assets above it must
  be split at load. This is recorded for the asset-pipeline work, not deferred silently.
