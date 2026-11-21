# Sprint 017 — Upload/swizzle matrix and the gated texture features

| | |
|---|---|
| Roadmap phase | [Phase 6 — Textures and VRAM](../phases/06-textures-vram.md) |
| Branch / base commit | `master` @ after sprint 016 (record the closing sha when it starts) |
| Started / closed | — / — |
| Build number(s) | `b24` (matrix), `b25` (CLUT/mipmap probes) |
| Tasks | 0/4 done |

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

- [ ] Sprint 016 closed; residence/eviction counters exist and the fit workload is 0 evictions
- [ ] pspgu `b23` or later; default 8888 and `--texfmt=auto` both build and are pixel-gated
- [ ] `texture_pattern` and `texture_churn` capture/marker patterns are in hand
- [ ] The free region after the probe's default font is recorded (b21: 912,896 B in `batch_check`)
- [ ] pspGL restorable, content hash `2ab7378e`
- [ ] The three probes are created by T1/T2/T3 and write a marker file, as the churn probe does

## 5. Tasks

| ID | task | status | verification | evidence |
|---|---|---|---|---|
| T1 | **Upload/swizzle matrix**: `psp-tests/texture_matrix` with size and format modes; pre-built Image; 30 runs; log PSM, bytes and median/min/max; a size that cannot fit is recorded | `todo` | a complete table for 8888/5650/4444 × 32²/64²/128²/256²/384² (512² where it fits); pixels of one size/format checked | — |
| T2 | **CLUT T4 glyph probe**: raw-GU 128×128 `GU_PSM_T4` + 16-entry palette; draw the glyph-like pattern; measure bytes/upload/time and compare with the 4444 atlas | `todo` | T4 image matches the 4444 atlas within palette quantisation; bytes 8,192 + palette; decision recorded | — |
| T3 | **Mipmap probe**: 256×256 checker at 64×64 with/without a chain; measure memory and the visible difference; decide for the 2D corpus and hand 3D to phase 10 | `todo` | a capture pair; memory +33 % quantified; decision recorded | — |
| T4 | **Decision and phase closure**: write both decisions into the phase page; if CLUT pays, scope sprint 018; otherwise close phase 6 and reconcile its exit table | `todo` | phase 6 exit criteria checked; 018 either scoped or explicitly not needed | — |

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
- `psp-tests/clut_probe` is a **standalone raw-GU app** (no pspgu dependency): init GU, upload the same generated
  glyph pattern as `GU_PSM_T4`, load the palette with `sceGuClutLoad`, draw it, capture. It measures the GU path
  and the bytes; it does not change pspgu.
- Decision rule: implement in 018 only if the T4 image is correct, the upload is not worse, and the 24 KiB is worth
  the palette/CLUT integration (the atlas is the only CLUT candidate in the corpus; phase 7 may add more).
- If dropped, record: bytes saved, why the integration does not pay, and the phase-6 task as `dropped` with the
  measurement.

### T3 design

- `psp-tests/mipmap_probe` draws a 256×256 checker at 64×64 in two arms: one level (aliasing) and a generated
  1/4/16/64 chain (the GE's mip selection). Memory is +33 % (4/3 of the base); the driver currently uploads level 0
  only, so this probe is raw GU or a level-0-plus-chain upload in the probe.
- The 2D corpus is drawn ~1:1, so the expected decision is **drop for 2D** with the capture as evidence; 3D
  minification belongs to phase 10, where it may pay.
- Record the capture and the memory cost; do not change the driver's mip policy here.

### T4 design

- Update the phase page:
  - check “upload/swizzle benchmarked by pixel format and size”;
  - mark the CLUT and mipmap tasks `done`/`dropped` with the measured reason;
  - if CLUT pays, add sprint 018 to the sprint table and scope it; otherwise close phase 6 and reconcile the exit
    table (residency from 016, matrix from 017).
- Update `memory-map.md` with the matrix's format/size facts and the decisions.

### Task log (append-only)

- — sprint not started.

## 6. Budgets

| budget | limit | measured | tag |
|---|---|---|---|
| matrix rows | 8888/5650/4444 × 32²–384² (512² 16-bit) | — | — |
| upload timing | 30 runs, median, pre-built Image | — | — |
| 512² 8888 | 1,048,576 B; does not fit 913,344 B | record as not allocatable | — |
| CLUT T4 atlas | 8,192 B + 64 B palette vs 32,768 B (4444) | — | — |
| mipmap chain | +33 % of the base texture | — | — |
| 2D gate | 0 differing pixels (no driver path changes) | — | — |

## 7. Measurements

| run | build | workload | metric | value | notes |
|---|---|---|---|---|---|
| matrix | 8888 | 32²–384² RGBA | bytes / median | | |
| matrix | auto | 32²–384² RGB/RGBA/LA | PSM / bytes / median | | |
| matrix | 8888 | 512² RGBA | allocation | expected fail (1 MiB > free) | a recorded result |
| CLUT | probe | 128² T4 glyphs | bytes / upload / parity | | vs 4444 32,768 B |
| mipmap | probe | 256² → 64² | memory / visible | | 2D decision |
| control | both | `batch_check` / `basic_shapes` | differing pixels | expected 0 | no driver change |

## 8. Exit criteria

- [ ] the matrix table is complete for the formats and sizes that fit; non-fitting rows are recorded
- [ ] the CLUT T4 probe renders correctly and its bytes/upload/parity are recorded
- [ ] the mipmap probe records memory and the visible difference, with a 2D decision and a 3D hand-off
- [ ] both gated tasks are `done` with an implementation scope (018) or `dropped` with the measurement
- [ ] phase 6's exit table is reconciled; if both are dropped, phase 6 is closed here
- [ ] the 2D gate is 0 differing pixels (no driver path changed)

## 9. Result

(To fill in on close.)

## 10. Follow-ups

- Sprint 018: CLUT glyph atlas implementation, only if T2 says it pays.
- Mipmaps for 3D, phase 10; 512-pixel windowing, phase 7; streaming, phase 11.
