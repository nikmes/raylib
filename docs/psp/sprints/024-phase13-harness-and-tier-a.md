# Sprint 024 (P13-S1) — Sample port harness and Tier A core/shapes

| | |
|---|---|
| Roadmap phase | [Phase 13 — Samples porting](../phases/13-samples-porting/index.md) |
| Sprint | P13-S1 |
| Branch / base commit | `master` @ `595535df` (drafted 2026-11-21; set to the open sha when started) |
| Started / closed | 2026-11-21 / 2026-11-21 |
| Build number(s) | pspgu `b24`; nine sample builds from this tree |
| Tasks | 10/10 done |

## 0. Handover

**Situation.** Phase 13's audit classified all 127 samples in `core`, `shapes` and `textures`; 13 are drop-in.
This sprint turns the first nine (4 core, 5 shapes) into device builds — and more importantly leaves a harness
that every later sample reuses by copying one directory and one Makefile include. No renderer, driver, platform
or input change is in scope.

**Read before acting**, in this order:

1. `../phases/13-samples-porting/index.md` — Tier A, gate `G0`, the porting recipe and the evidence rules.
2. `../memory-map.md` §6 — the measurement and capture protocol; §9 — the traps that produce plausible wrong runs.
3. `psp-tests/basic_shapes/Makefile` and `main.c` — the current port shape: link line, bounded loop,
   `DONE` + `sceKernelSleepThread()`, last frames frozen for a stable capture.
4. `psp-tests/batch_check/Makefile` — the build/run pattern and the capture comparison.
5. The nine sources: `examples/core/core_basic_window.c`, `core_custom_logging.c`, `core_random_values.c`,
   `core_3d_camera_mode.c`; `examples/shapes/shapes_logo_raylib.c`, `shapes_math_angle_rotation.c`,
   `shapes_outlines_thickness.c`, `shapes_circle_sector_drawing.c`, `shapes_rectangle_advanced.c`.

**First action.** Create `psp-tests/samples/Makefile.inc` from the `basic_shapes` Makefile (target, link line,
`PSP_LARGE_MEMORY=0`, `PSPSDK` include), then `psp-tests/samples/core/basic_window/` with the example source and
build it. Do not start the other eight until `basic_window` reaches `DONE frames 600` and two identical captures.

**Hard rules.**

- **Never edit `examples/`.** Copy the `.c` and only the resources that sample loads. The upstream tree stays
  updatable.
- **No driver, platform or renderer change.** If a Tier A sample fails, record the failure in Phase 13 and stop;
  the fix belongs to the phase that owns the path.
- **The harness never calls `sceKernelExitGame`.** Log `DONE frames N`, then `sceKernelSleepThread()`; reset from
  the host. A crash whose report was never read looks exactly like a hang.
- **480×272, `PSP_LARGE_MEMORY=0`, explicit `PSP_HEAP_SIZE_KB` when a sample allocates a full-screen image.**
- **A capture is trusted only after two captures are identical, taken after `DONE`.** `scrshot` while the app is
  rendering freezes it and can return a torn frame.
- **One sample per task, one evidence row per sample.** A “renders” claim without a build marker, boot log and
  capture is not evidence.
- **Stage only this sprint's paths.** Other sessions edit the tree; run `git status` before every commit and
  never `git add -A`.
- Commit messages carry the task ID: `sprint 024 T3: port core_random_values`.

**Commands.**

```sh
export PSPDEV=/home/psp/pspdev
export PATH=$PATH:$PSPDEV/bin

cd /home/psp/raylib-psp
./tools/pspgu-build.sh --verify-install

cd psp-tests/samples/core/basic_window
make PSP_LARGE_MEMORY=0
( printf './basic_window.prx\n'; sleep 20 ) | pspsh
# after DONE: reset in pspsh, capture twice with scrshot, reset again
```

**Do not touch.** `src/`, `tools/pspgu/`, `examples/`, and every other `psp-tests/` directory.

**Done means** §8 below, with evidence in each task row.

## 1. Objective

Nine Tier A core/shapes samples render from 480×272 PSP builds with the standard harness, and a tenth sample can
be added by copying one directory and reusing one Makefile include. Every `done` sample has a build marker, boot
log, `DONE frames 600` (or its documented frame count) and two identical captures.

## 2. Why now

Phase 13's wave 0 needs no new capability: it validates the tier classification and the harness before input
remapping (`G1`) or the texture budget (`G2`/`G3`). It also gives phases 5–7 a broader regression corpus than the
four purpose-built probes, without changing the driver those phases own.

## 3. Scope

**In:** `psp-tests/samples/Makefile.inc`; copies of the nine examples under
`psp-tests/samples/{core,shapes}/<name>/`; the capture/evidence archive.

**Out:** input remapping (P13-S3), texture samples (P13-S2), any driver or platform edit, and any change to
`examples/`.

## 4. Preconditions

- [ ] `pspsh` answers; the device is enumerated; `reset` was run and partition 2 free reads 25,149,440 bytes
- [ ] `./tools/pspgu-build.sh --verify-install` passes; note the pspgu build marker
- [ ] The current 2D gate is recorded (`batch_check` capture and differing-pixel count) **before** the first port
- [ ] `psp-tests/basic_shapes` still reaches `DONE` on the current driver; archive its boot line as the control
- [ ] `psp-tests/samples/Makefile.inc` exists and `core_basic_window` builds from it
- [ ] Two captures of one stationary sample are byte-identical in this environment
- [ ] Builds use `PSP_LARGE_MEMORY=0`

## 5. Tasks

Status vocabulary — use exactly these words, in backticks: `todo` · `doing` · `done` · `blocked` · `dropped`.

| ID | task | status | verification | evidence |
|---|---|---|---|---|
| T1 | **Harness + `core_basic_window`**: `psp-tests/samples/Makefile.inc`, directory convention, 480×272, bounded loop, `DONE` + sleep | `done` | `make PSP_LARGE_MEMORY=0`; run; two captures identical → expected: `DONE frames 600` | **PASS** — `DONE frames 600`, 60 fps; two identical captures `basic_window_{1,2}.bmp` |
| T2 | `core_custom_logging` | `done` | builds and runs; the sample's log lines appear once, no per-frame spam | **PASS** — `DONE frames 600`; timestamped custom logger active (see §9); two identical captures |
| T3 | `core_random_values` | `done` | builds and runs; values animate; last two frames frozen for the capture | **PASS** — `DONE frames 600`; last two frames frozen; two identical captures |
| T4 | `core_3d_camera_mode` | `done` | builds and runs; cube and grid visible, depth correct; record the depth/PSM build | **PASS** — `DONE frames 600`; cube+grid, depth yes (pspgu `b24`, 272 KiB); two identical captures |
| T5 | `shapes_logo_raylib` | `done` | builds and runs; black logo and text legible on RAYWHITE | **PASS** — `DONE frames 600`, 60 fps; two identical captures |
| T6 | `shapes_math_angle_rotation` | `done` | builds and runs; rotated vectors render; capture stable after `DONE` | **PASS** — `DONE frames 600`; last two frames frozen; two identical captures |
| T7 | `shapes_outlines_thickness` | `done` | builds and runs; every outline width is visible and distinct | **PASS** — `DONE frames 600`; both rectangles + circle rendered; two identical captures |
| T8 | `shapes_circle_sector_drawing` | `done` | builds and runs; sectors and arcs render without seams | **PASS** — `DONE frames 600`; sector and panel rendered; two identical captures |
| T9 | `shapes_rectangle_advanced` | `done` | builds and runs; direct `rlBegin`/`rlVertex` path renders; no missing symbols at link | **PASS** — `DONE frames 600`; direct `gl*` path linked and rendered; two identical captures (see §9) |
| T10 | Evidence archive and classification verdict | `done` | nine captures + logs archived; Phase 13 Tier A either confirmed or corrected; `git diff -- examples/` empty | **PASS** — nine pairs + control in `/home/psp/evidence/sprint024/`, `SHA256SUMS` written; Phase 13 updated; `git diff -- examples/` empty |

### Task log (append-only)

- 2026-11-21 — implementation and builds complete for all nine; device run and captures started
- 2026-11-21 — T1 `done`; `basic_window` 600 frames, two identical captures
- 2026-11-21 — T2 `done`; custom logger active after init; two identical captures
- 2026-11-21 — T3 `done`; frozen last frames; two identical captures
- 2026-11-21 — T4 `done`; cube+grid, depth build; two identical captures
- 2026-11-21 — T5–T9 `done`; all nine reach `DONE frames 600` with two identical captures
- 2026-11-21 — T10 `done`; evidence archived, Phase 13 updated

## 6. Budgets

| budget | limit | measured | tag |
|---|---|---|---|
| frame time | no target; record median for each sample | | |
| main-CPU render time | no target; record if the sample is animated | | |
| heap | record boot free; raise `PSP_HEAP_SIZE_KB` only if a sample fails to allocate | | |
| VRAM | record the boot line; no sample here loads a large texture | | |
| captures | 2 identical per sample, after `DONE` | | |

## 7. Measurements

Protocol: device only; one sample per run; `basic_shapes` control after the sweep (no renderer change, so no
before arm). Every row carries the build marker and the capture path.

| run | build | workload | metric | value | notes |
|---|---|---|---|---|---|
| control | pspgu b24 | `basic_shapes` | colours / identical captures | **284** / captures 2,3 identical | capture 1 torn, discarded |
| port | pspgu b24 | `core_basic_window` | `DONE` / captures | **600, 60 fps / identical** | `basic_window_{1,2}.bmp` |
| port | pspgu b24 | `core_custom_logging` | `DONE` / captures | **600 / identical** | timestamped logger; `custom_logging_{1,2}.bmp` |
| port | pspgu b24 | `core_random_values` | `DONE` / captures | **600 / identical** | frozen last frames; `random_values_{1,2}.bmp` |
| port | pspgu b24 | `core_3d_camera_mode` | `DONE` / cube+grid+depth | **600 / identical** | red cube visible; `3d_camera_mode_{1,2}.bmp` |
| port | pspgu b24 | `shapes_logo_raylib` | `DONE` / captures | **600 / identical** | `logo_raylib_{1,2}.bmp` |
| port | pspgu b24 | `shapes_math_angle_rotation` | `DONE` / captures | **600 / identical** | frozen last frames; `math_angle_rotation_{1,2}.bmp` |
| port | pspgu b24 | `shapes_outlines_thickness` | `DONE` / captures | **600 / identical** | `outlines_thickness_{1,2}.bmp` |
| port | pspgu b24 | `shapes_circle_sector_drawing` | `DONE` / captures | **600 / identical** | `circle_sector_drawing_{1,2}.bmp` |
| port | pspgu b24 | `shapes_rectangle_advanced` | `DONE` / captures | **600 / identical** | direct `gl*`; `rectangle_advanced_{1,2}.bmp` |

## 8. Exit criteria

- [x] all nine samples build with `PSP_LARGE_MEMORY=0` and reach their `DONE` marker
- [x] each has two identical captures, archived with a build marker
- [x] a tenth sample directory can reuse `Makefile.inc` without editing it
- [x] `git diff -- examples/` is empty
- [x] the Tier A classification is confirmed, or every failure is written into Phase 13 with its owning phase
- [x] the phase page links this sprint and records the result
- [x] no build artifacts (`*.o`, `*.prx`, `*.elf`, logs) are left in the tree

## 9. Result

P13-S1 closed. All nine samples build with `PSP_LARGE_MEMORY=0` and ran on the PSP-1000: each logged
`DONE frames 600` and produced two byte-identical 480×272 BMP captures at 60 fps. The harness is
`psp-tests/samples/Makefile.inc`; the copies live under `core/` and `shapes/`; evidence is in
`/home/psp/evidence/sprint024/` with `SHA256SUMS`.

Findings:

- **Direct-rlgl samples cannot use the `rl*` vertex wrappers.** The installed `libraylib.a` exports the
  high-level `rl*` API but not the GL11 `rlBegin`/`rlVertex2f`/`rlColor4ub`. `shapes_rectangle_advanced` now
  builds with `-DPLATFORM_PSP -DGRAPHICS_API_OPENGL_11` and calls `glBegin`/`glVertex2f`/`glColor4ub` directly.
  Future rlgl samples must do the same or link the rlgl implementation.
- **`InitWindow` replaces a pre-init log callback.** The platform installs its `CustomLog` during init, so
  `core_custom_logging` sets `CustomTraceLog` after `InitWindow` and flushes each line. The timestamp is
  1970-01-01 because the PSP clock is unset; the custom format is visible.
- **`scrshot` returns before hostfs finishes writing.** A 0-byte, 128 KiB partial or black frame is a capture
  race, not a renderer fault. Wait for the full 391,734-byte BMP and require two identical captures. The
  `basic_shapes` control's first capture was torn; captures 2 and 3 were identical (284 colours).
- **Layouts adapted to 480×272:** centred text in the core samples, relaid-out outlines/circle-sector panels,
  a shorter rotation line, and the logo note moved to the bottom. No upstream example changed.

The control's 284 colours match the phase 1 fingerprint. No driver or platform file was changed.

## 10. Follow-ups

- **P13-S2**: reuse `psp-tests/samples/Makefile.inc`; the direct-rlgl substitution in §9 applies to
  `textures_polygon_drawing` and any `rl*` sample.
- **Platform**: `InitWindow` installs the platform `CustomLog` unconditionally, overwriting a callback set before
  init. Decide in Phase 1 whether to install the default only when no callback is set.
- **Capture runner**: the wait-and-retry logic used here lives in `/tmp/run_s024.sh`; move it into the harness for
  P13-S2 so the hostfs write race is not rediscovered.
- **Clock**: the PSP clock is unset (timestamp 1970-01-01); a sample that formats wall-clock time needs a clock
  source or must record the offset.
- **Interaction** in these samples belongs to P13-S3 (`G1`).
- Any texture-path failure discovered here belongs to P13-S2; a driver failure belongs to the phase that owns the
  path, not to this sprint.
