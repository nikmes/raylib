# Sprint 030 (P13-S7) — Texture budget

| | |
|---|---|
| Roadmap phase | [Phase 13 — Samples porting](../phases/13-samples-porting/index.md) |
| Sprint | P13-S7 |
| Branch / base commit | `master` @ `e01028fe` (drafted 2026-11-21; set to the open sha when started) |
| Started / closed | — / — |
| Build number(s) | pspgu default `b24`; four variant content hashes recorded in T1 |
| Tasks | 0/11 done |

## 0. Handover

**Situation.** P13-S1 and P13-S2 are done: Tier A is complete and 21 samples have two identical captures each.
The remaining texture samples are code-correct but blocked by the 1 MiB padded-texture budget. This is the first
`G2` sprint: it builds the driver in four configurations (depth on/off × 8888/auto), ports the eight samples that
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

Eight `G2` texture samples and the 45° `image_rotate` case render on the PSP-1000 under a recorded build
configuration whose boot/texture lines prove the padded texture fits. The default 8888/depth build remains the
parity gate, and `image_drawing` is explicitly moved to P13-S8 with the font-atlas reason recorded.

## 2. Why now

P13-S2 closed the samples that fit the 8888/depth budget. The rest are blocked only by the 1 MiB padded size.
`G2` levers already exist — `PSP_GU_DEPTH=0`, `PSP_GU_TEXFMT=auto`, and an explicit heap — so these samples can
be ported now without waiting for input (`G1`) or render targets (`G4`). The sprint also turns the audit's
"1 MiB" classification into measured per-sample build arms.

## 3. Scope

**In:** the four driver build arms, eight sample copies (`image_channel`, `image_kernel`, `image_processing`,
`image_text`, `polygon_drawing`, `raw_data`, `background_scrolling`, `blend_modes`), the 45° `image_rotate` case,
the evidence archive, and the Phase 13/memory-map updates.

**Out:** `image_drawing` (font atlas downscale; P13-S8), `sprite_stacking` and `image_generation` (P13-S8),
`textures_sprite_explosion` (audio + asset; P13-S11), any asset downscale, and any driver source change.

## 4. Preconditions

- [ ] P13-S1/S2 closed; the harness and assets are in place (`psp-tests/samples/Makefile.inc`, `host0:/resources`)
- [ ] The default `libGL.a` is a pspgu build and its content hash is recorded (`./tools/pspgu-build.sh --check`)
- [ ] `batch_check` and `basic_shapes` pass on the default build before the first variant
- [ ] Device permission obtained; `pspsh` answers; `reset` was run; the device is enumerated
- [ ] The capture runner (copy the P13-S2 pattern to `psp-tests/samples/run_sample.sh`) waits for the full
      391,734-byte BMP and verifies two identical captures
- [ ] Assets staged at `host0:/resources` using the P13-S2 usbhostfs root (`/tmp/s3/resources`):
      `fudesumi.png`, `cat.png`, `parrots.png`, `KAISG.ttf`, `fudesumi.raw`,
      `cyberpunk_street_background.png`, `cyberpunk_street_midground.png`,
      `cyberpunk_street_foreground.png`
- [ ] `PSP_HEAP_SIZE_KB` set explicitly in the sample Makefile (not in the shared `Makefile.inc`) for samples
      that hold full-screen CPU images; start at 4096 for `image_text` and lower it if the boot line allows

## 5. Tasks

Status vocabulary — use exactly these words, in backticks: `todo` · `doing` · `done` · `blocked` · `dropped`.

| ID | task | status | verification | evidence |
|---|---|---|---|---|
| T1 | **Build matrix, runner and parity gate**: build/archive default, `--depth=0`, `--texfmt=auto`, and depth-off+auto; record content hash and boot line for each; add `psp-tests/samples/run_sample.sh` with the wait/retry logic from `/tmp/run_s025.sh` (or its P13-S2 log); re-run `batch_check`/`basic_shapes` on the default | `todo` | four arms archived; runner waits for the full 391,734-byte BMP and two identical captures; default gate 0 differing pixels / 284 colours; boot lines quoted | — |
| T2 | `textures_image_channel` — `fudesumi.png` 384×512 RGBA non-binary → 512×512 8888 = 1,048,576 B; `--depth=0` | `todo` | texture line shows 512×512 8888; fits depth-off; `DONE`; two identical captures | — |
| T3 | `textures_image_kernel` — `cat.png` 384×512 RGB → 512×512 5650 = 524,288 B under `auto` | `todo` | texture line shows 5650; fits with depth on; `DONE`; two identical captures | — |
| T4 | `textures_image_processing` — `parrots.png` 512×384 RGB → 512×512 5650 = 524,288 B under `auto` | `todo` | texture line shows 5650; fits; `DONE`; two identical captures | — |
| T5 | `textures_image_text` — `parrots.png` RGB 5650 524,288 B + `KAISG.ttf` 64 px font atlas; use depth-off+auto and measure the atlas size and PSM | `todo` | atlas size and PSM recorded; total fits; if the atlas is 512² 8888 (1 MiB, anti-aliased alpha) or >512², reduce to 32 px or move to P13-S8 with the measured bytes | — |
| T6 | `textures_polygon_drawing` — `cat.png` RGB 5650 524,288 B; port with the P13-S1 `gl*` substitution (`<GL/gl.h>`, no GL11 `rl*` vertex wrappers) | `todo` | texture fits; direct `gl*` path links; `DONE`; two identical captures | — |
| T7 | `textures_raw_data` — `fudesumi.raw` 384×512 RGBA non-binary → 1,048,576 B; `--depth=0` | `todo` | texture line 8888 1 MiB; fits depth-off; `DONE`; two identical captures | — |
| T8 | `textures_background_scrolling` — background/midground 512×192 → 512×256 4444 (262,144 each), foreground 704×192 → 1024×256 4444 (524,288) = 1,048,576 B; depth-off+auto | `todo` | three textures fit; no seam; `DONE`; two identical captures | — |
| T9 | `textures_blend_modes` — background 262,144 + foreground 524,288 = 786,432 B under `auto` with depth on | `todo` | both fit; blend modes render; `DONE`; two identical captures | — |
| T10 | `textures_image_rotate` 45° — re-add `image45` to the P13-S2 copy (`NUM_TEXTURES 3`, `currentTexture = 0`); `raylib_logo.png` RGB → 362×362 → 512×512 5650 = 524,288 B; plus the two 256² rotations (262,144 each); depth-off+auto | `todo` | three textures total 1,048,576 B; 45° frame renders; `DONE`; two identical captures | — |
| T11 | Evidence, classification and phase write-back | `todo` | `/home/psp/evidence/sprint030/` has `libs/libGL-<arm>.a`, `CONTENT_SHA256`, nine logs, nine capture pairs and `SHA256SUMS`; `image_drawing` moved to P13-S8; Phase 13/memory map updated; `git diff -- examples/` empty | — |

### Task log (append-only)

- 2026-11-21 — sprint written from the placeholder; scope corrected to 8 samples + 45° image_rotate;
  `image_drawing` moved to P13-S8

## 6. Budgets

| budget | limit | measured | tag |
|---|---|---|---|
| frame time | no target; record fps per sample | | |
| heap | explicit `PSP_HEAP_SIZE_KB`; record boot free | | |
| VRAM | each texture must fit the boot line of its arm; record used/free | | |
| captures | 2 identical per sample, after `DONE` | | |

## 7. Measurements

Protocol: device only; one variable per arm; the default build is the control. Every row carries the arm,
`libGL.a` content hash, boot line, padded bytes and PSM.

### Build matrix

| arm | command | depth | texfmt | free after font (boot line) | default gate |
|---|---|---|---|---|---|
| default | `./tools/pspgu-build.sh` | yes | 8888 | | `batch_check` 0 / `basic_shapes` 284 |
| depth-off | `./tools/pspgu-build.sh --depth=0` | no | 8888 | | |
| auto | `./tools/pspgu-build.sh --texfmt=auto` | yes | auto | | |
| depth-off+auto | `PSP_GU_TEXFMT=auto ./tools/pspgu-build.sh --depth=0` | no | auto | | |

### Samples

| sample | arm | texture bytes / PSM | fps | result |
|---|---|---|---|---|
| `image_channel` | depth-off | 1,048,576 / 8888 | | |
| `image_kernel` | auto | 524,288 / 5650 | | |
| `image_processing` | auto | 524,288 / 5650 | | |
| `image_text` | depth-off+auto | 524,288 + font atlas (size and PSM measured) | | |
| `polygon_drawing` | auto | 524,288 / 5650 | | |
| `raw_data` | depth-off | 1,048,576 / 8888 | | |
| `background_scrolling` | depth-off+auto | 1,048,576 total / 4444 | | |
| `blend_modes` | auto | 786,432 total / 4444 | | |
| `image_rotate` 45° | depth-off+auto | 1,048,576 total / 5650 | | |

## 8. Exit criteria

- [ ] the four arms are built and archived with content hashes; the default gate is 0 / 284 before and after
- [ ] the eight samples plus the 45° `image_rotate` render under a recorded arm, each with two identical captures
- [ ] every texture line proves the padded texture fits the arm's boot free bytes
- [ ] `image_text`'s font atlas is measured; it either fits or is moved to P13-S8 with the bytes
- [ ] `image_drawing` is explicitly recorded as P13-S8 (non-binary font atlas requires a downscale)
- [ ] the default 8888/depth library is restored and verified with `--check`
- [ ] no `examples/` file is modified; no build artifacts are left in the tree

## 9. Result

—

## 10. Follow-ups

- **P13-S8**: `image_drawing` (font atlas from `custom_jupiter_crash.png`, 512² 8888 = 1 MiB, plus
  `parrots.png` 524,288 B, over the depth-off budget), `sprite_stacking` (11468-tall sheet), `image_generation`
  (nine full-screen textures + heap), and the explosion asset for P13-S11.
- **P13-S3** can start once Phase 8 closes `G1`; the `G2` arm selection recorded here is the default for the
  texture samples that need it.
- If `image_text`'s 64 px TTF atlas is 512² with non-binary alpha (8888, 1 MiB) or exceeds 512², a 32 px font is
  a sample-side change; record the trade rather than hiding it in the capture.
- **Do not make `PSP_GU_TEXFMT=auto` or depth-off the committed default** without the parity gate; the default
  build remains 8888/depth.
- Record the four arm content hashes in the Phase 13 page so a later run can reproduce which library produced a
  capture.
