# Sprint 030 (P13-S7) — Texture budget

| | |
|---|---|
| Roadmap phase | [Phase 13 — Samples porting](../phases/13-samples-porting/index.md) |
| Sprint | P13-S7 |
| Branch / base commit | `master` @ `e01028fe` (drafted 2026-11-21; set to the open sha when started) |
| Started / closed | — / — |
| Build number(s) | pspgu default `b24`; four variant content hashes recorded in T1 |
| Tasks | 10/10 done, 1 blocked, 1 dropped |

## 0. Handover

**Situation.** P13-S1 and P13-S2 are done: Tier A is complete and 21 samples have two identical captures each.
The remaining texture samples are code-correct but blocked by the 1 MiB padded-texture budget. This is the first
`G2` sprint: it builds the driver in four configurations (depth on/off × 8888/auto), ports the seven samples that
fit under one of them, and closes the deferred 45° `image_rotate` case. `image_drawing` is **not** in this sprint:
its font atlas comes from a non-binary RGBA image and still needs a downscale, which belongs to `G3`/P13-S8.

**Read before acting**, in this order:

1. `../phases/13-samples-porting/index.md` — the two texture limits (1024 buffer / 512 sampling window, and the
   VRAM budget), the asset table, `G2`/`G3`, and the P13-S7/P13-S8 split.
2. `../memory-map.md` §3 and §14 — the framebuffer/depth/texture budget, pspgu PSM rules, allocator failure
   behaviour; §6 — capture protocol.
3. `../sprints/015-phase6-npot-and-vram.md` — padding, the 512-byte allocator, `PSP_GU_TEXFMT=auto`.
4. `../sprints/020-phase7-tilemap-and-windowing.md` — the 512-window split; `../sprints/025-phase13-texture-paths.md`
   — the sample harness, assets and capture runner.
5. `tools/pspgu-build.sh` and `tools/pspgu/Makefile` — the `--depth=0` and `PSP_GU_TEXFMT=auto` build options.
6. The eight sources under `examples/textures/` listed in §5, plus `examples/textures/textures_image_rotate.c`.

**First action.** Archive the installed default `libGL.a` and its content hash, then build `--depth=0` and
`PSP_GU_TEXFMT=auto ./tools/pspgu-build.sh --depth=0`. Build `textures_image_kernel` under the auto arm first
(`cat.png` is RGB, 512×512 at 5650 = 524,288 B); after explicit device permission, run it and record the texture
and boot lines. Do not start with the multi-texture samples.

**Hard rules.**

- **This sprint is `G2` only.** `image_drawing` (non-binary font atlas), `sprite_stacking` and
  `image_generation` belong to `G3`/P13-S8. Do not downscale an asset here.
- **One variable per arm.** Record depth (yes/no), texfmt (8888/auto), heap, the `libGL.a` content hash and the
  boot line for every measurement row.
- **The default build is the parity gate.** Before the sprint and after restoring the default, `batch_check` must
  stay at 0 differing pixels and `basic_shapes` at 284 colours.
- **A failed allocation is a failed task.** `LoadTexture` returning a texture whose log line is missing, or a
  `PSPGu: out of VRAM` line, is not a pass; choose a different arm or move the sample to P13-S8.
- **`PSP_GU_TEXFMT=auto` is a measurement, not the default.** Do not change the committed default; variants are
  installed and archived per arm, then the default is restored.
- **Quote the rendered-config boot line**, including `depth yes/no` and `texfmt`.
- **Capture after `DONE` only.** `scrshot` returns before hostfs finishes writing; wait for the full 391,734-byte
  BMP and require two identical captures.
- **No `examples/` edits.** Copy into `psp-tests/samples/textures/`.
- **Device access is permission-gated.** Do not start `pspsh`, `usbhostfs_pc`, or run a PRX until the user
  explicitly approves the device session. Builds, archives and static checks are host-only.
- **Rebuild the sample after installing an arm.** Samples link `libGL.a` statically; a stale PRX would test the
  previous arm.
- **Stage only this sprint's paths.** Other sessions edit the tree; `git status` before every commit.
- Commit messages carry the task ID: `sprint 030 T3: image_kernel under auto`.

**Commands.**

```sh
export PSPDEV=/home/psp/pspdev
export PATH=$PSPDEV/bin:$PATH

cd /home/psp/raylib-psp
./tools/pspgu-build.sh --check
./tools/pspgu-build.sh --depth=0
PSP_GU_TEXFMT=auto ./tools/pspgu-build.sh --depth=0
./tools/pspgu-build.sh --texfmt=auto
./tools/pspgu-build.sh                    # restore the default 8888/depth build

cd psp-tests/samples/textures/image_kernel
make clean && make PSP_LARGE_MEMORY=0

# Device commands only after explicit permission:
cp image_kernel.prx /tmp/s3/app.prx
timeout 20 pspsh -e "reset"
# ( printf './app.prx\n'; sleep 25 ) | pspsh
# then two scrshot captures; wait for the full 391,734-byte BMP
```

**Do not touch.** `src/`, `tools/pspgu/*.c` (the driver source), `examples/`, and every other `psp-tests/`
directory. The variants are build options, not driver changes.

**Done means** §8 below, with evidence in each task row.

## 1. Objective

Seven `G2` texture samples and the 45° `image_rotate` case render on the PSP-1000 under a recorded build
configuration whose boot/texture lines prove the padded texture fits. The default 8888/depth build remains the
parity gate, and `image_drawing` is explicitly moved to P13-S8 with the font-atlas reason recorded.

## 2. Why now

P13-S2 closed the samples that fit the 8888/depth budget. The rest are blocked only by the 1 MiB padded size.
`G2` levers already exist — `PSP_GU_DEPTH=0`, `PSP_GU_TEXFMT=auto`, and an explicit heap — so these samples can
be ported now without waiting for input (`G1`) or render targets (`G4`). The sprint also turns the audit's
"1 MiB" classification into measured per-sample build arms.

## 3. Scope

**In:** the four driver build arms, seven sample copies (`image_kernel`, `image_processing`, `image_text`,
`polygon_drawing`, `raw_data`, `background_scrolling`, `blend_modes`), the 45° `image_rotate` case, the evidence
archive, and the Phase 13/memory-map updates.

**Out:** `image_channel` (six textures; P13-S8), `image_drawing` (font atlas downscale; P13-S8), `sprite_stacking`
and `image_generation` (P13-S8), `textures_sprite_explosion` (audio + asset; P13-S11), any asset downscale, and
any driver source change.

## 4. Preconditions

- [ ] P13-S1/S2 closed; the harness and assets are in place (`psp-tests/samples/Makefile.inc`, `host0:/resources`)
- [ ] The default `libGL.a` is a pspgu build and its content hash is recorded (`./tools/pspgu-build.sh --check`)
- [ ] `batch_check` and `basic_shapes` pass on the default build before the first variant
- [ ] Device permission obtained; `pspsh` answers; `reset` was run; the device is enumerated
- [ ] The capture runner (copy the P13-S2 pattern to `psp-tests/samples/run_sample.sh`) waits for the full
      391,734-byte BMP and verifies two identical captures
- [ ] Assets staged at `host0:/resources` using the P13-S2 usbhostfs root (`/tmp/s3/resources`):
      `cat.png`, `parrots.png`, `KAISG.ttf`, `fudesumi.raw`,
      `cyberpunk_street_background.png`, `cyberpunk_street_midground.png`,
      `cyberpunk_street_foreground.png`
- [ ] `PSP_HEAP_SIZE_KB` set explicitly in the sample Makefile (not in the shared `Makefile.inc`) for samples
      that hold full-screen CPU images; start at 4096 for `image_text` and lower it if the boot line allows

## 5. Tasks

Status vocabulary — use exactly these words, in backticks: `todo` · `doing` · `done` · `blocked` · `dropped`.

| ID | task | status | verification | evidence |
|---|---|---|---|---|
| T1a | **Build matrix, runner and basic_shapes gate**: four arms archived; `run_sample.sh` added; `basic_shapes` 284/identical | `done` | four arms; runner; basic_shapes 284 and identical captures | **DONE** — arms/libs in `/home/psp/evidence/sprint030/libs/`, `CONTENT_SHA256`/`LIBS_SHA256`; `basic_shapes` **284**, two identical captures at 30 fps; runner executable | — |
| T1b | **batch_check parity gate**: default build vs sprint-015 `par_pspgl.raw` | `blocked` | 0 differing pixels | **BLOCKED** — two captures identical, but **66,680 bytes** differ from `par_pspgl.raw`; installed `libraylib.a` was rebuilt at 19:39 (sha `7c36…`, 1,283,620 B), not the sprint-015 baseline; needs a clean baseline build | — |
| T2 | `textures_image_channel` — **dropped from S7**: six textures (five 512² fudesumi channel images + a 480×272 checked background); move to P13-S8 | `dropped` | moved to S8 with the six-texture reason | — |
| T3 | `textures_image_kernel` — four 200×450 → 256×512 5650 (262,144 each) = 1,048,576 B; depth-off+auto; 0.5 scale | `done` | four texture lines 5650; total fits; `DONE`; two identical captures | **PASS** — free 179,712 B; 60 fps; 6,033-colour capture; identical |
| T4 | `textures_image_processing` — parrots via `ImageFormat` RGBA → 512×512 4444 = 524,288 B; `auto`; fixed grayscale; 0.5 scale | `done` | texture 4444; processed image visible; `DONE`; two identical captures | **PASS** — free 425,472 B; 60 fps; 45-colour grayscale; identical |
| T5 | `textures_image_text` — parrots 5650 524,288 B + TTF atlas; `depth-off+auto`; 64 px requested, 32 px accepted | `done` | atlas size/PSM measured; total fits; `DONE`; two identical captures | **PASS** — 64 px atlas **1024×512 rejected**; 32 px/94 glyphs gives **512×256 8888 524,288 B**; free 179,712 B; 60 fps; identical |
| T6 | `textures_polygon_drawing` — cat 5650 524,288 B; direct `gl*`; `auto` | `done` | texture fits; direct `gl*` links; `DONE`; two identical captures | **PASS** — free 425,472 B; 60 fps; 2,579-colour capture; identical |
| T7 | `textures_raw_data` — fudesumi 8888 1,048,576 B + checked 240×136 4444 131,072 B = 1,179,648 B; depth-off+auto | `done` | both textures fit; checked reduced and drawn 2×; `DONE`; two identical captures | **PASS** — free **48,640 B** (very tight); 60 fps; 2,031-colour capture; identical |
| T8 | `textures_background_scrolling` — three 4444 textures = 1,048,576 B; depth-off+auto | `done` | three textures fit; `DONE`; two identical captures | **PASS** — free 179,712 B; 60 fps; 123-colour capture; identical |
| T9 | `textures_blend_modes` — background 262,144 + foreground 524,288 = 786,432 B; `auto` | `done` | both fit; blend renders; `DONE`; two identical captures | **PASS** — free 163,328 B; 60 fps; 80-colour capture; identical |
| T10 | `textures_image_rotate` 45° — 362² 5650 524,288 B + two 256² 5650 131,072 B = 786,432 B; depth-off+auto | `done` | 45° frame renders; `DONE`; two identical captures | **PASS** — free 441,856 B; 60 fps; 77-colour capture; identical |
| T11 | Evidence, classification and phase write-back | `done` | `/home/psp/evidence/sprint030/` has libs, content hashes, eight sample pairs/logs, two gate pairs/logs and `SHA256SUMS`; `image_channel`/`image_drawing` S8; Phase 13 updated; `git diff -- examples/` empty | **PASS** — 26 SHA entries; default pspgu restored; examples untouched |

### Task log (append-only)

- 2026-11-21 — sprint written from the placeholder; scope corrected to 8 samples + 45° image_rotate;
  `image_drawing` moved to P13-S8
- 2026-11-21 — implementation audit: `image_channel` creates six textures → moved to P13-S8;
  `image_kernel` creates four → depth-off+auto; `image_processing` becomes 4444; `raw_data` also generates a
  checked texture, so its total is 1,179,648 B
- 2026-11-21 — T1 host-only: four arms archived; pre-sprint `libGL.a` was pspGL, pspgu default restored;
  `run_sample.sh` added; device parity gate pending permission
- 2026-11-21 — T3 host-only: `image_kernel` ported (four 200×450 crops at 0.5 scale) and built under
  depth-off+auto; device run pending permission
- 2026-11-21 — T1a `done`: four arms + runner + `basic_shapes` 284/identical
- 2026-11-21 — T1b `blocked`: `batch_check` pairs identical but 66,680 bytes vs sprint-015 `par_pspgl.raw`;
  installed `libraylib.a` rebuilt at 19:39 (sha `7c36…`), not the sprint-015 baseline
- 2026-11-21 — T3–T10 `done`: all eight samples `DONE 600`, 60 fps, two identical captures
- 2026-11-21 — T5: 64 px TTF atlas rejected (1024×512); 32 px/94 glyphs fits 512×256; sample re-run
- 2026-11-21 — T11 `done`: 26 SHA entries; default pspgu restored; examples untouched

## 6. Budgets

| budget | limit | measured | tag |
|---|---|---|---|
| frame time | no target; record fps per sample | | |
| heap | explicit `PSP_HEAP_SIZE_KB`; record boot free | | |
| VRAM | each texture must fit the boot line of its arm; record used/free | | |
| captures | 2 identical per sample, after `DONE` | | |

## 7. Measurements

Protocol: device only; one variable per arm; the default build is the control. Every row carries the arm and the
sample's own boot/texture lines.

### Build matrix

| arm | command | depth | texfmt | free after font | gate |
|---|---|---|---|---|---|
| default | `./tools/pspgu-build.sh` | yes | 8888 | 916,992 B `[derived]` | `basic_shapes` 284/identical; `batch_check` 66,680 bytes vs sprint-015 (T1b blocked) |
| depth-off | `./tools/pspgu-build.sh --depth=0` | no | 8888 | 1,195,520 B `[derived]` | no surviving sample uses this arm; kept for the matrix |
| auto | `./tools/pspgu-build.sh --texfmt=auto` | yes | auto | 949,760 B `[derived]` | `image_processing`, `polygon_drawing`, `blend_modes` |
| depth-off+auto | `PSP_GU_TEXFMT=auto ./tools/pspgu-build.sh --depth=0` | no | auto | 1,228,288 B `[derived]` | `image_kernel`, `image_text`, `raw_data`, `background_scrolling`, `image_rotate` |

### Samples

| sample | arm | texture bytes / PSM | fps | result |
|---|---|---|---|---|
| `image_kernel` | depth-off+auto | 1,048,576 total / 5650 | 60 | free 179,712 B; two identical |
| `image_processing` | auto | 524,288 / 4444 | 60 | free 425,472 B; two identical |
| `image_text` | depth-off+auto | 524,288 (parrots, 5650) + 524,288 (32 px atlas, 8888) | 60 | free 179,712 B; 64 px atlas rejected; two identical |
| `polygon_drawing` | auto | 524,288 / 5650 | 60 | free 425,472 B; two identical |
| `raw_data` | depth-off+auto | 1,179,648 total / 8888 + 4444 | 60 | free **48,640 B**; two identical |
| `background_scrolling` | depth-off+auto | 1,048,576 total / 4444 | 60 | free 179,712 B; two identical |
| `blend_modes` | auto | 786,432 total / 4444 | 60 | free 163,328 B; two identical |
| `image_rotate` 45° | depth-off+auto | 786,432 total / 5650 | 60 | free 441,856 B; two identical |

## 8. Exit criteria

- [x] four arms built and archived with hashes; `basic_shapes` is 284 with identical captures
- [ ] default `batch_check` is 0 differing pixels (T1b blocked: 66,680 bytes vs sprint-015; shared `libraylib.a` rebuild)
- [x] the seven samples plus the 45° `image_rotate` render under a recorded arm, each with two identical captures
- [x] every texture line proves the padded texture fits the arm's boot free bytes
- [x] `image_text`'s atlas is measured: 64 px rejected (1024×512), 32 px fits (512×256 8888)
- [x] `image_channel` and `image_drawing` are recorded as P13-S8
- [x] the default 8888/depth library is restored and verified with `--check`
- [x] no `examples/` file is modified; build artifacts are cleaned before commit

## 9. Result

Eight S7 samples and the 45° `image_rotate` ran on the PSP-1000: all logged `DONE frames 600` at 60 fps and
produced two identical captures. The arm split worked: 5650 for RGB sources, 4444 for all-255/binary alpha,
8888 for the non-binary fudesumi image and the TTF atlas.

The largest port issues found and fixed:

- `image_channel` creates six textures → dropped to P13-S8.
- `image_kernel` creates four 200×450 textures → depth-off+auto, 1,048,576 B, four panels at 0.5 scale.
- `image_processing` formats parrots to RGBA → 4444; fixed `COLOR_GRAYSCALE` exercises the update path.
- `raw_data` also generates a 960×480 checked texture → reduced to 240×136; total 1,179,648 B, only **48,640 B**
  free.
- `image_text` at 64 px produced a **1024×512** atlas, rejected by the driver; 32 px/94 glyphs fits
  512×256 (8888).
- `image_rotate` 45° re-added; the three textures total 786,432 B at 5650.

T1b is blocked outside this sprint: the installed `libraylib.a` was rebuilt at 19:39 (sha `7c36…`), so
`batch_check` no longer matches the sprint-015 pspGL reference (66,680 bytes differ). `basic_shapes` still passes
at 284/identical. A clean baseline build is needed before the batch_check gate can be re-run.

## 10. Follow-ups

- **T1b**: rebuild `libraylib.a` from a known commit in a clean tree, then re-run `batch_check` vs
  `par_pspgl.raw`; the current shared library changed under this sprint.
- **P13-S8**: `image_channel` (six textures), `image_drawing` (font atlas), `sprite_stacking`, `image_generation`,
  and the explosion asset.
- `image_text` is now 32 px; if 64 px matters, a smaller codepoint set or a pre-baked atlas is needed, not a
  larger texture.
- `raw_data` leaves only 48,640 B; any additional texture in that scene must be smaller or a different arm.
- The three `auto` samples (`image_processing`, `polygon_drawing`, `blend_modes`) and the five depth-off+auto
  samples are the recorded arm choices for later texture sprints.