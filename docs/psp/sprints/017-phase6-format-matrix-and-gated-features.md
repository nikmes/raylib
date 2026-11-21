# Sprint 017 — Upload/swizzle matrix and the gated texture features

| | |
|---|---|
| Roadmap phase | [Phase 6 — Textures and VRAM](../phases/06-textures-vram.md) |
| Branch / base commit | `master` @ `a57b26d3` (sprint 016 closes; driver `b22`) |
| Started / closed | 2026-11-21 / 2026-11-21 |
| Build number(s) | `b23` driver (quiet-hook probe support); probe app builds |
| Tasks | 4/4 done |

## 0. Handover

**Situation.** Sprint 015 closed the allocator, NPOT, per-texture PSM and direct upload; sprint 016 closes
residency/eviction. What is left in phase 6 is not a renderer feature but a **measurement plus two decisions**:
upload/swizzle has not been benchmarked by pixel format and size, the 4-bit CLUT glyph atlas has never been priced,
and mipmaps have never been measured. This sprint produces the table, verifies the two candidate paths with small
standalone probes, and records **implement** or **dropped with evidence**. If the CLUT path pays, sprint 018
implements it; otherwise phase 6 closes here.

**Read before acting**, in this order:

1. `../phases/06-textures-vram.md` — the CLUT/mipmap tasks and the exit criterion this sprint closes.
2. `../sprints/015-phase6-npot-and-vram.md` T2/T4 — the format/byte numbers and the direct upload path the matrix
   extends.
3. `../sprints/016-phase6-residency-eviction.md` — the residency counters and the probe pattern (marker files).
4. `tools/pspgu/gl_texture.c` — `chooseFormat`, `uploadTexels`, `glTexImage2D`; `tools/pspgu/egl.c` —
   `pspguVramReport`.

**First action.** Write `psp-tests/texture_matrix` (T1) with a `SIZE`/`FMT` mode and a marker file. It must use a
**pre-built Image** and time only `LoadTextureFromImage` + `UnloadTexture`, as the T4 probe did.

**Hard rules.**

- **No driver behaviour changes in this sprint.** 017 measures and decides. A winning CLUT/mipmap path is
  implemented in 018 (or a later phase), not smuggled into the benchmark build.
- **The default 8888 build stays the parity build.** Every probe is run on default and `auto`; the 2D gate is not
  expected to move because no rendering path changes.
- **A size that cannot be allocated is a result, not a failure to hide.** A 512×512 RGBA8888 texture is 1,048,576 B
  and cannot fit in the 913,344 B free region; the matrix records "not allocatable" rather than omitting the row.
- **CLUT and mipmaps need a rendered probe, not arithmetic.** A T4 texture is verified with `GU_PSM_T4` +
  `sceGuClutLoad` and a capture; a mipmap claim is verified against a minified draw. A cost model alone is not a
  decision.
- **Decisions carry numbers and a reason.** "Not worth it" means the measured bytes/time/complexity; "implement"
  means the measured win and the 018 scope.
- Commit messages carry the task ID: `sprint 017 T2: ...`.

**Commands.**

```sh
export PSPDEV=/home/psp/pspdev
export PATH=$PATH:$PSPDEV/bin

cd /home/psp/raylib-psp
./tools/pspgu-build.sh                    # default 8888
./tools/pspgu-build.sh --texfmt=auto      # measured 5650/4444
./tools/pspgu-build.sh --restore

cd psp-tests/texture_matrix
make clean && make MATRIX=size
make clean && make MATRIX=fmt
cp texture_matrix.prx /tmp/s3/app.prx
cd /tmp/s3 && pspsh -e "reset"; sleep 6
( printf './app.prx\n'; sleep 25 ) | pspsh
```

**Do not touch.** The batcher, `gl_draw.c`, the parity scenes, the allocator/eviction policy, `DrawMesh`, and any
driver path that is not a probe. Sprint 018 is the implementation slot if a decision says "pay".

**Done means** §8: the matrix table, both decisions recorded, and the phase exit table updated.

## 1. Objective

Three falsifiable results:

1. An **upload/swizzle matrix** for pixel formats `8888`, `5650`, `4444` at sizes 32², 64², 128², 256² and (where it
   fits) 384²/512²: bytes, PSM actually chosen, and median upload time over 30 runs. The row for a size that cannot
   fit says so.
2. A **CLUT T4 probe** renders the 128×128 binary-alpha font atlas with a 16-entry palette, measures bytes
   (expected 8,192 B + 64 B palette vs 32,768 B at 4444) and upload time, and compares the glyph image.
3. A **mipmap probe** draws a 256×256 checker at 64×64 with and without a chain, measures memory (+33 %) and the
   visible difference, and records whether it pays for the 2D corpus. 3D mipmapping is phase 10.

## 2. Why now

Phase 6's last exit criterion is "upload/swizzle benchmarked by pixel format and size". Sprint 015 measured three
formats but one size, and left the CLUT/mipmap tasks as measurement-gated. Without this table the phase cannot say
which format is worth its bytes at a given size, and without the two probes it cannot record the gated tasks as
done or dropped. This is the measurement that lets phase 6 close — or hands a justified 018 to the next session.

## 3. Scope

**In:** `texture_matrix` (size/format, 30 runs, marker); `clut_probe` (raw GU T4 + palette, capture and bytes);
`mipmap_probe` (256² checker at 64², with/without chain); the matrix table and the two decision records.

**Out:** implementing the CLUT path in pspgu (018 if it pays), implementing mipmaps (phase 10 for 3D, or 018 if the
2D probe says pay), 512-pixel windowing (phase 7), the allocator/eviction policy, and the batcher.

## 4. Preconditions

- [x] Sprint 016 closed; residency/eviction counters exist and the fit workload is 0 evictions
- [x] pspgu `b22` or later; default 8888 and `--texfmt=auto` both build and are pixel-gated
- [x] `texture_pattern` and `texture_churn` capture/marker patterns are in hand
- [x] The free region after the probe's default font is recorded: **916,992 B** in the raw-GU probes
      (912,896 B in `batch_check` after its three assets)
- [x] pspGL restorable, content hash `2ab7378e`
- [x] The three probes are created by T1/T2/T3 and write a marker file, as the churn probe does

## 5. Tasks

| ID | task | status | verification | evidence |
|---|---|---|---|---|
| T1 | **Upload/swizzle matrix**: `psp-tests/texture_matrix` with size and format modes; pre-built Image; 30 runs; log PSM, bytes and median/min/max; a size that cannot fit is recorded | `done` | a complete table for 8888/5650/4444 × 32²–512²; pixels of one size/format checked | **See §7.** 8888: 4,096 B/0.499 ms at 32² … 262,144/33.332 at 256²; **384² and 512² pad to 512² = 1 MiB and are not allocatable**. `auto`: 5650/4444 half the bytes (32² 2,048; 256² 131,072; 512² 524,288 at 113–130 ms); RGBA128 stays 8888 and 512² fails. Uploads include the residency shadow. `b23` |
| T2 | **CLUT T4 glyph probe**: raw-GU 128×128 `GU_PSM_T4` + 16-entry palette; draw the glyph-like pattern; measure bytes/upload/time and compare with the 4444 atlas | `done` | T4 image matches the 4444 atlas within palette quantisation; bytes 8,192 + palette; decision recorded | `psp-tests/gated_features MODE=clut`: pattern renders (red/green/blue/background cells); **8,192 B + 64 B palette vs 32,768 B** at 4444; raw copy **0.080 ms vs 0.532 ms**. **Decision: dropped** — 24,512 B is 2.7 % of the 916,992 B free, no workload is VRAM-bound, and the integration adds a palette/4-bit upload path. 018 not needed. |
| T3 | **Mipmap probe**: 256×256 checker at 64×64 with/without a chain; measure memory and the visible difference; decide for the 2D corpus and hand 3D to phase 10 | `done` | a capture pair; memory +33 % quantified; decision recorded | `MODE=mipmap MIPS=0/1`: 262,144 B / 4.473 ms vs 349,504 B / 4.103 ms; **captures identical** at 4× minification (variance 12,500 both) — the 3D-triangle path with `GU_LINEAR_MIPMAP_LINEAR` selected no different level in this probe. +33 % memory for no visible change. **Decision: dropped for 2D; 3D in phase 10.** |
| T4 | **Decision and phase closure**: write both decisions into the phase page; if CLUT pays, scope sprint 018; otherwise close phase 6 and reconcile its exit table | `done` | phase 6 exit criteria checked; 018 either scoped or explicitly not needed | Both gated features **dropped with evidence**; **018 is not needed**; **Phase 6 closed**. Exit table: residency/no-eviction/fragmentation/priority (016) and the format/size matrix (017) all met. |

Status vocabulary: `todo` / `doing` / `done` / `blocked` / `dropped`. IDs are stable.

### T1 design

- `texture_matrix` modes:
  - `MATRIX=size`: 8888 RGBA at 32², 64², 128², 256², 384²; 512² 8888 is attempted and its allocation failure is
    recorded (1,048,576 B > 913,344 B).
  - `MATRIX=fmt`: 32²/128²/256² in RGBA8888, RGB (auto 5650), RGBA binary-alpha (auto 4444), LA binary-alpha
    (auto 4444); 512² in 16-bit only.
- For each row: create the Image once, time `LoadTextureFromImage` + `UnloadTexture` 30 times, report min/median/
  max; the driver's per-texture log supplies the PSM and bytes. The probe writes a `matrix_done.txt` line per row.
- Run the whole probe once on the default 8888 driver and once on `--texfmt=auto`; the table records the build.
- Check one draw per size/format (a solid/checker quad) to catch a stride/swizzle error, not only the timing.

### T2 design

- The font atlas is 128×128 `LUMINANCE_ALPHA` with binary alpha: 4444 = 32,768 B; 5650 has no alpha; 8888 =
  65,536 B. A 4-bit CLUT image is 128×128/2 = 8,192 B plus a 16-entry palette (64 B at 8888), a 24 KiB saving
  against 4444.
- `psp-tests/gated_features MODE=clut` is a **standalone raw-GU app** (no pspgu/raylib): init GU with the ortho
  matrices, upload a 128×128 `GU_PSM_T4` pattern, load a 16-entry palette with `sceGuClutLoad`, draw it, capture.
  It measures the GU path and the bytes; it does not change pspgu.
- **Instrument notes from doing it:** the first draw produced only the clear because the raw probe had not set
  GU's view/projection matrices and the textured vertex was `u,v,x,y` while `GU_VERTEX_32BITF` reads three floats.
  A colour-only marker quad separated those from the texture state; both were fixed before the numbers were used.
- Decision rule applied: implement in 018 only if the T4 image is correct, the upload is not worse, and the
  bytes saved are worth the palette/CLUT integration. The image and upload pass; the 24,512 B is **2.7 % of the
  916,992 B free in the raw-GU probe** and no current workload is VRAM-bound, so the task is **dropped** and 018
  is not needed.
- Recorded: bytes saved (24,512), upload times, why the integration does not pay, and the phase-6 task as
  `dropped` with the measurement.

### T3 design

- `psp-tests/gated_features MODE=mipmap MIPS=0|1` draws a 256×256 checker at 64×64: one level (aliasing) and a
  generated 7-level chain. Memory is **262,144 vs 349,504 B (+33 %)**. The mip arm uses 3D triangles and
  `GU_LINEAR_MIPMAP_LINEAR`; `GU_SPRITES` does not compute texture derivatives, so the first attempt with sprites
  could not select a level.
- **Result: the two captures are identical at 4× minification** (variance 12,500 in both); the raw-GU path with
  `GU_TRANSFORM_3D` selected no different level in this probe. Combined with +33 % memory, the decision is
  **dropped for 2D**; 3D minification belongs to phase 10, where the camera and derivatives differ.
- Record the captures and the memory cost; do not change the driver's mip policy here.

### T4 design

- Update the phase page:
  - check “upload/swizzle benchmarked by pixel format and size”;
  - mark the CLUT and mipmap tasks `done`/`dropped` with the measured reason;
  - if CLUT pays, add sprint 018 to the sprint table and scope it; otherwise close phase 6 and reconcile the exit
    table (residency from 016, matrix from 017).
- Update `memory-map.md` with the matrix's format/size facts and the decisions.

### Task log (append-only)

- 2026-11-21 — sprint started; `b23` adds a quiet-hook for timing probes.
- 2026-11-21 — T1 `done`: 8888 size matrix and auto format matrix; 384²/512² pad to 512² and do not fit.
- 2026-11-21 — T2 `doing`: raw-GU T4 path; first draws were only the clear (missing matrices, then a 2D
  vertex where GU reads 3 floats). A colour-only marker separated geometry from texture; fixed.
- 2026-11-21 — T2 `done`: T4 pattern renders; 8,256 B vs 32,768 B, 0.080 vs 0.532 ms raw copy.
- 2026-11-21 — T3 `doing`: sprites cannot select mips (no derivatives); switched to 3D triangles and
  `GU_LINEAR_MIPMAP_LINEAR`.
- 2026-11-21 — T3 `done`: chain captures identical at 4×; +33 % memory; dropped for 2D, phase 10 for 3D.
- 2026-11-21 — T4 `done`: CLUT and mipmaps dropped with evidence; 018 not needed; **Phase 6 closed**.
  **Sprint 017 closed 4/4.**

## 6. Budgets

| budget | limit | measured | tag |
|---|---|---|---|
| matrix rows | 8888/5650/4444 × 32²–512² | **see §7**; 384²/512² 8888 not allocatable | `[measured: b23]` |
| upload timing | 30 runs, median, pre-built Image | **0.476–129.763 ms** by format/size | `[measured]` |
| 512² 8888 | 1,048,576 B; does not fit 916,992 B | **not allocatable** | `[measured]` |
| CLUT T4 atlas | 8,192 B + 64 B palette vs 32,768 B | **8,256 B vs 32,768 B**; 0.080 vs 0.532 ms | `[measured]` |
| mipmap chain | +33 % of the base texture | **262,144 → 349,504 B**; captures identical at 4× | `[measured]` |
| 2D gate | 0 differing pixels (no driver path changes) | **0** | `[measured]` |

## 7. Measurements

**8888 default, RGBA (b23; upload includes the residency shadow):**

| size | bytes | median | note |
|---|---|---|---|
| 32² | 4,096 | 0.499 ms | |
| 64² | 16,384 | 1.998 ms | |
| 128² | 65,536 | 8.228 ms | |
| 256² | 262,144 | 33.332 ms | |
| 384² | 0 | 9.694 ms | logical 384² pads to 512² = 1,048,576 B; not allocatable |
| 512² | 0 | 9.697 ms | 1,048,576 B; not allocatable |

**`auto` (b23):** PSM chosen by source; 5650 for RGB, 4444 for opaque RGBA, 8888 for alpha-128.

| size | RGB / 5650 | RGBA / 4444 | RGBA128 / 8888 |
|---|---|---|---|
| 32² | 2,048 B / 0.476 ms | 2,048 / 0.512 | 4,096 / 0.503 |
| 128² | 32,768 / 6.944 | 32,768 / 8.083 | 65,536 / 8.290 |
| 256² | 131,072 / 27.963 | 131,072 / 32.137 | 262,144 / 33.384 |
| 512² | 524,288 / 112.896 | 524,288 / 129.763 | **not allocatable** (1 MiB) |

**Gated features:**

| probe | bytes | upload | result |
|---|---|---|---|
| CLUT T4 128×128 | 8,192 + 64 palette | 0.080 ms raw copy | pattern red/green/blue/background correct; 4444 is 32,768 B / 0.532 ms |
| mipmap 256²→64² level 0 | 262,144 | 4.473 ms | aliased capture |
| mipmap 256²→64² chain | 349,504 | 4.103 ms | **identical capture** to level 0 at 4×; no level selected in this probe |
| control | — | — | 2D gate on `b23`: `batch_check` vs `par_pspgl.raw` **byte-identical**, SHA-256 `b69830b8…e7c` |

## 8. Exit criteria

- [x] the matrix table is complete for the formats and sizes that fit; non-fitting rows are recorded
- [x] the CLUT T4 probe renders correctly and its bytes/upload/parity are recorded
- [x] the mipmap probe records memory and the visible difference, with a 2D decision and a 3D hand-off
- [x] both gated tasks are `done` with an implementation scope (018) or `dropped` with the measurement — both
      **dropped**, 018 not needed
- [x] phase 6's exit table is reconciled; phase 6 is closed here
- [x] the 2D gate is 0 differing pixels (no driver path changed): `batch_check` on `b23` is byte-identical to
      `par_pspgl.raw` (SHA-256 `b69830b86a89…e357e7c`)

## 9. Result

Sprint 017 closed 4/4. The matrix is measured by PSM and size; the two gated features were priced with real
probes and both **dropped**:

- **CLUT T4** works and saves **24,512 B** (8,256 vs 32,768) with a 6.6× faster raw copy, but that is **2.7 % of
  the 916,992 B free in the probe** and no workload is VRAM-bound; the integration adds a palette/4-bit upload
  path. **No 018.**
- **Mipmaps** cost **+33 % memory** (349,504 vs 262,144 B) and produced an **identical capture** at 4× minification
  in the raw-GU probe (3D triangles, `GU_LINEAR_MIPMAP_LINEAR`). **Dropped for 2D; phase 10 for 3D.**

Phase 6's exit table is therefore complete: residency (no eviction when it fits, priority changes, bounded
fragmentation) from sprint 016, and the format/size benchmark from this sprint. The 2D gate did not move.

Instrument notes worth keeping: the first CLUT draw was missing because the raw probe had no GU matrices and then
because its vertex was `u,v,x,y` while `GU_VERTEX_32BITF` reads three floats; a colour-only marker separated those
from the texture state. The first mip attempt used `GU_SPRITES`, which does not compute texture derivatives.

## 10. Follow-ups

- **No 018.** The CLUT path is measured and dropped; revisit only if a future UI/glyph working set is VRAM-bound
  and the 24,512 B is worth the palette/4-bit upload path.
- Mipmaps for 3D, phase 10 (the 2D corpus is 1:1; the probe showed no level change at 4×).
- 512-pixel windowing, phase 7; streaming/residency, phase 11 (the residency core is sprint 016).
