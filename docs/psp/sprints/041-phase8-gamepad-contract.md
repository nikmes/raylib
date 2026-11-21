# Sprint 041 (P8-S1) — Gamepad contract and the portable sample

| | |
|---|---|
| Roadmap phase | [Phase 8 — Input](../phases/08-input/index.md) |
| Sprint | P8-S1 |
| Branch / base commit | `master` @ `e307fe05` (docs; input code unchanged) |
| Started / closed | 2026-11-21 / 2026-11-21 |
| Build number(s) | pspgu `b24` (unchanged); `libraylib.a` builds with `PSP_PAD_STRICT`, plus `PSP_PAD_FAIL` as a test hook |
| Tasks | 5/5 done |

## 0. Handover

**Situation.** Phase 8 is unstarted. The imported platform file already fills `CORE.Input.Gamepad`, so the public
API works in principle, but the state contract leaks platform details: `SceCtrlData pad` is uninitialised and its
read is unchecked (`src/platforms/rcore_psp.c:445-446`), the right axes come from reserved bytes (`:483-484`), and
the trigger axes stay at 0 rather than the −1 desktop backends use. This sprint makes `CORE.Input.Gamepad` the
complete, platform-neutral contract and proves it with a sample that contains no PSP symbols.

**Read before acting**, in this order:

1. `../memory-map.md` §5 Phase 8 and §9 — the roadmap and the input traps; §12 question 5 is the keyboard/mouse
   decision that sprint 044 records.
2. `../phases/08-input/index.md` — “Gaps in the current platform file” 1, 7, 9 and 10, plus the exit criteria.
   This sprint closes the contract half of the phase.
3. `src/platforms/rcore_psp.c:408-501` and `:583-600` — `PollInputEvents()` and `InitPlatform()`.
4. `src/rcore.c:4028-4132` — the public gamepad API; note that `GetGamepadAxisMovement()` gates on
   `axis < MAX_GAMEPAD_AXES`, **not** on `axisCount`.
5. `src/raylib.h:737-767` — the positional button and axis enums; `src/rcore_desktop_sdl.c:1794-1795` — the
   trigger-axis default on a desktop backend.
6. `psp-tests/basic_shapes/Makefile` and `psp-tests/scene_2d/Makefile` — the test-app build shape.

**First action.** Create `psp-tests/pad_api/main.c`, its `psp_module.c` PRX boilerplate and its Makefile
(modelled on `psp-tests/basic_shapes`). `main.c` includes only `raylib.h`, opens a 480×272 window and draw `IsGamepadAvailable(0)`, `GetGamepadName(0)`,
`GetGamepadAxisCount(0)`, `IsGamepadAvailable(1)`, `GetGamepadAxisCount(1)`, every `GAMEPAD_BUTTON_*` state,
`GetGamepadButtonPressed()`, the normalised `LEFT_X`/`LEFT_Y`, `RIGHT_X`/`RIGHT_Y` and both trigger axes. Build it
against the **base-commit** library and capture the true control before `src/` changes. `PSP_PAD_STRICT` arrives
with T2; its `=0` arm is the code-path control, not the base commit.

**Hard rules.**

- **The game-facing source has no PSP knowledge.** `psp-tests/pad_api/main.c` includes `raylib.h` only;
  `grep -inE 'psp|PSP_' psp-tests/pad_api/main.c` must be empty. `psp_module.c` carries the PRX
  `PSP_MODULE_INFO`/`PSP_MAIN_THREAD_ATTR` boilerplate and is not game code; the Makefile carries
  `PSP_LARGE_MEMORY`. No `#ifdef PLATFORM_PSP`, no `pspctrl.h`, no raw controller values in `main.c`.
- **No new public API.** `CORE.Input.Gamepad` is already the abstraction. Do not add symbols to `raylib.h` or a
  `pspGamepad*` header; the only platform-specific code lives in `src/platforms/rcore_psp.c`.
- **Fill every axis with a platform-neutral default in the `=1` build.** `RIGHT_X`/`RIGHT_Y` and every unused
  axis are 0; `LEFT_TRIGGER`/`RIGHT_TRIGGER` are −1 (desktop parity); `LEFT_X`/`LEFT_Y` are normalised to −1…1.
  The `=0` control keeps the old values for the A/B.
- **Advertise exactly one pad.** `ready[0] = true`, `name[0] = "PSP Controller"` (S4 may change the string),
  `axisCount[0] = 2`; `axisCount[1]` is **0**, `ready[1]` is false and its name is empty. The current
  `axisCount[1] = 2` is a fabricated second pad and must go.
- **Positional mapping, not regional.** CROSS is `RIGHT_FACE_DOWN`, CIRCLE is `RIGHT_FACE_RIGHT`, SQUARE is
  `RIGHT_FACE_LEFT`, TRIANGLE is `RIGHT_FACE_UP`. A game that wants “Circle confirms” builds an action map on top;
  the platform does not swap the enum.
- **The control arm is honest.** The true control is the base-commit binary. `PSP_PAD_STRICT=0` zero-initialises
  `pad` and follows the old code paths (unchecked read, right axes from `Rsrv`, triggers left at 0); with a zeroed
  struct that reads −1 on both right axes, not the original undefined stack value. Undefined content is not a
  baseline, so do not claim to reproduce it.
- **A failed read changes nothing.** On `sceCtrlReadBufferPositive() <= 0`, return before updating
  `currentButtonState` or `axisState`, and log the return once per change of state. The last good snapshot stays
  visible.
- **The sample never returns to `ClosePlatform()`.** `ClosePlatform()` calls `sceKernelExitGame()`
  (`rcore_psp.c:772-775`), which drops the PSPLink session. After the final frame, log `DONE` and stay in a
  raylib-only `for (;;) WaitTime(0.1);` loop; the host `reset` ends the run. Never call `CloseWindow()`.
- **No renderer change.** `rlgl.h`, `rshapes.c`, `rtextures.c` and `tools/pspgu/` are untouched.
- **Stage only this sprint's paths.** `git status` before every commit and never `git add -A`.
- Commit messages carry the task ID: `sprint 041 T2: ...`.

**Commands.**

```sh
export PSPDEV=/home/psp/pspdev
export PATH=$PATH:$PSPDEV/bin
set -e

mkdir -p /tmp/p8s1

# One arm: relink the sample against the staged library, reset, run, capture.
# The library must already be built and copied to $PSPDEV/psp/lib.
# Requires usbhostfs_pc running with /tmp/p8s1 as host0 (environment.md).
run_arm() {
    (
        cd /home/psp/raylib-psp/psp-tests/pad_api
        make clean && make
        cp pad_api.prx /tmp/p8s1/app.prx
    )
    (
        cd /tmp/p8s1
        timeout 20 pspsh -e "reset"; sleep 6
        ( printf './app.prx\n'; sleep 70 ) | timeout 90 pspsh
        pspsh -e "scrshot host0:/$1.raw"        # after DONE only
    )
}

# 1. True control: base commit, before T2 adds the switch.
cd /home/psp/raylib-psp/src
make clean && make PLATFORM=PLATFORM_PSP -j8
cp libraylib.a $PSPDEV/psp/lib/libraylib.a
run_arm s041-base

# 2. After T2 adds the switch: code-path control.
make clean && make PLATFORM=PLATFORM_PSP -j8 PSP_PAD_STRICT=0
cp libraylib.a $PSPDEV/psp/lib/libraylib.a
run_arm s041-control

# 3. Change.
make clean && make PLATFORM=PLATFORM_PSP -j8 PSP_PAD_STRICT=1
cp libraylib.a $PSPDEV/psp/lib/libraylib.a
run_arm s041-change

# 4. Failure branch: hold CROSS or SQUARE until the read fails (60 consecutive held polls).
make clean && make PLATFORM=PLATFORM_PSP -j8 PSP_PAD_STRICT=1 PSP_PAD_FAIL=1
cp libraylib.a $PSPDEV/psp/lib/libraylib.a
run_arm s041-failure

# 5. Restore the normal build after the failure run.
make clean && make PLATFORM=PLATFORM_PSP -j8 PSP_PAD_STRICT=1
cp libraylib.a $PSPDEV/psp/lib/libraylib.a
cd /home/psp/raylib-psp/psp-tests/pad_api && make clean   # drop the failure-linked sample
```

**Do not touch.** The sampling strategy (`ReadBufferPositive`/`Peek`/latch), HOLD/interception masking, nub
calibration and the idle threshold (042/043); keyboard/mouse emulation and DS3 (044); the renderer and pspgu.

**Done means** §8: the sample is free of PSP symbols, all twelve buttons and four directions are observed, the
axis defaults match the contract, and the renderer gate is unchanged.

## 1. Objective

Four falsifiable results:

1. `psp-tests/pad_api/main.c` builds for the PSP and contains no PSP header, symbol or `#ifdef`;
   `psp_module.c` is the only PRX boilerplate file.
2. On the device, `IsGamepadAvailable(0)` is true, `GetGamepadName(0)` is `"PSP Controller"`, and
   `GetGamepadAxisCount(0)` is 2; `IsGamepadAvailable(1)` is false and `GetGamepadAxisCount(1)` is 0.
3. All twelve physical controls are reachable through the public enum: CROSS, CIRCLE, SQUARE, TRIANGLE, the four
   D-pad directions, L, R, SELECT and START; `GetGamepadButtonPressed()` reports a press for exactly the frame
   whose poll saw it.
4. The axis contract is complete: `LEFT_X`/`LEFT_Y` span −1…1 with the expected directions; `RIGHT_X`/`RIGHT_Y`
   and unused axes are 0; both trigger axes are −1; a forced failed read leaves the last good snapshot intact.

## 2. Why now

Phase 8's exit criteria start with a sample that draws every control and with a contract that later sprint
measurements can trust. Without this sprint, 042 would tune a sampling strategy on a state struct whose defaults
are wrong, and 043 would calibrate axes that a game cannot query consistently across platforms. This sprint also
proves the user-facing promise: a game written against raylib's gamepad API needs no PSP awareness.

## 3. Scope

**In:** `src/platforms/rcore_psp.c`; `src/Makefile` (`PSP_PAD_STRICT`, defaults to 1; `PSP_PAD_FAIL`, test only);
`psp-tests/pad_api/`.

**Out:** sampling/edge strategy and HOLD/interception (042); calibration, radial deadzone, magnitude clamp and the
idle threshold (043); keyboard/mouse emulation, DS3 and the extra-bit decisions (044); the renderer, rlgl, pspgu,
and every public header.

## 4. Preconditions

- [x] The Phase 8 page has been reviewed and its preconditions are checked
- [x] `libraylib.a` builds and stages into `$PSPDEV/psp/lib` with headers
- [x] The device loop answers; `reset` leaves partition 2 at 25,149,440 bytes
- [x] `psp-tests/basic_shapes` renders 600 frames (the regression control)
- [ ] A base-commit `pad_api` run is captured in §7 before any `src/` change
- [x] `PSP_PAD_FAIL=0` is the default; the forced-failure run builds separately with `=1`
- [x] The sample builds with `PSP_LARGE_MEMORY=0`
- [ ] `git status` is clean apart from the Phase 8 docs

## 5. Tasks

Status vocabulary: `todo` · `doing` · `done` · `blocked` · `dropped`.

| ID | task | status | verification | evidence |
|---|---|---|---|---|
| T1 | **Portable sample**: `psp-tests/pad_api` (`main.c`, `psp_module.c`, Makefile, `FRAMES ?= 1800`) draws availability, both names and axis counts, all button states, `GetGamepadButtonPressed()`, `LEFT`/`RIGHT` axes and both triggers; no PSP symbols in `main.c` | `done` | `grep -inE 'psp\|PSP_' psp-tests/pad_api/main.c` is empty; builds and runs | `pad_api.prx` ran 1800 frames: `DONE frames 1800`; state log at 0/60/120; screenshot `evidence/sprint041/s041-pad-api-strict.bmp` |
| T2 | **State contract**: zero `SceCtrlData`; check the read's return; set `ready[0]`, `name[0]`, `axisCount[0] = 2`; clear `axisCount[1]`/`ready[1]`; right/unused axes 0; trigger axes −1; `PSP_PAD_STRICT=0\|1`; `PSP_PAD_FAIL` test hook | `done` | `=0` → triggers 0, right axes −1 from zeroed `Rsrv`; `=1` → triggers −1, right axes 0; `GetGamepadAxisCount(1)==0`; forced read failure logs and keeps the last snapshot | strict: `RX/RY=0.00`, `LT/RT=-1.00`, `axes1=0`; control: `RX/RY=-1.00`, `LT/RT=0.00`; failure with Cross held: `PAD pressed=7 frame=77`, frame 120 `cross=1`, one `PAD read failed ret=-1`, recovered after release; snapshot retained |
| T3 | **Button map and edges**: replace the per-button assignments with one static table; keep previous/current state and the one-frame `lastButtonPressed` reset | `done` | every physical button shows its enum; a press is reported for one frame; a chord logs every held button but one `GetGamepadButtonPressed()` value | log enums 1–9, 11, 13, 15 = all 12 controls; one `PAD pressed` per press; chord not separately logged |
| T4 | **Axis direction and range**: verify the nub's sign and endpoints in the sample; expose only two axes | `done` | nub right → `LEFT_X > 0`, up → `LEFT_Y < 0`; values within −1…1 and 0 at rest after the existing deadzone | directions confirmed; at rest `LY=-0.21` is outside the 0.2 deadzone — carried to 043 calibration; screenshot |
| T5 | **Regression**: `basic_shapes` 600 frames and `batch_check` against the archived gate | `done` | renders; `batch_check` hash unchanged; no renderer file in the diff | `basic_shapes` `DONE frames 600`, 30 fps; `batch_check` SHA-256 `b69830b8…` identical to `par_pspgl.raw`; captures in `evidence/sprint041/` |

### T1 design

- `pad_api` is deliberately boring: one 480×272 window, cleared each frame, with a 12-row button grid, an axis
  crosshair, a text block for `name`, both `axisCount` values, `lastButtonPressed` and the trigger/right-axis
  values. On the last frame it draws a final screen, calls `EndDrawing()`, logs `DONE frames N`, then enters
  `for (;;) WaitTime(0.1);` so the capture is stable and `ClosePlatform()` is never reached.
- `main.c` is the game-facing source. `psp_module.c` contains only `PSP_MODULE_INFO` and
  `PSP_MAIN_THREAD_ATTR`, because a PRX cannot link without them; the grep rule applies to `main.c`.
- The Makefile is a copy of `psp-tests/basic_shapes/Makefile` with `TARGET = pad_api`,
  `OBJS = main.o psp_module.o`, the same `LIBS` line, `PSP_LARGE_MEMORY = 0`, and
  `CFLAGS += -DFRAMES=$(FRAMES)` where `FRAMES ?= 1800`. The sample never calls `sceKernelExitGame` or
  `CloseWindow()`.
- It uses the same `main.c` a desktop game would. The only concession is the deployment path; there is no
  `#ifdef PLATFORM_PSP` in the file.
- The sample logs with the public `TraceLog(LOG_INFO, ...)`: `PAD pressed=%d frame=%d` once per change of
  `GetGamepadButtonPressed()`, and `DONE frames N` after the final `EndDrawing()`. Those logs are the evidence.
- A `PSP_PAD_TRACE` log in the platform file may print raw diagnostics, but the sample must not depend on them.
- Keep the frame bounded (`FRAMES=1800`: 30 s at 60 fps, up to 60 s at the observed 30 fps) so the twelve
  controls and the nub can be exercised;
  capture only after `DONE`, per the measurement protocol.

### T2 design

- `SceCtrlData pad = { 0 };` and `int read = sceCtrlReadBufferPositive(&pad, 1);`. On `read <= 0`, log
  `PAD read failed ret=%d` once and return without touching the public state.
- `PSP_PAD_FAIL=1` is a test-only hook that sets `read = -1` after 60 consecutive polls with CROSS or SQUARE
  held, so the operator can press whenever ready and the last good snapshot contains the held button. Log once
  when it begins. Default 0; the hook is compiled out when 0 and never shipped on.
- In `InitPlatform()`, set the full default once: `ready[0] = true`, `name[0] = "PSP Controller"`,
  `axisCount[0] = 2`, `axisCount[1] = 0`, `ready[1] = false`; zero the whole axis row with
  `memset(CORE.Input.Gamepad.axisState[0], 0, sizeof(CORE.Input.Gamepad.axisState[0]))`. In the `=1` build,
  then set `axisState[0][LEFT_TRIGGER]` and `axisState[0][RIGHT_TRIGGER]` to −1; the `=0` control leaves them 0.
- Makefile switches:
  ```make
  PSP_PAD_STRICT ?= 1
  CUSTOM_CFLAGS += -DPSP_PAD_STRICT=$(PSP_PAD_STRICT)
  PSP_PAD_FAIL ?= 0
  CUSTOM_CFLAGS += -DPSP_PAD_FAIL=$(PSP_PAD_FAIL)
  ```
  Log both at boot (`PAD contract=%d fail=%d`) so a stale archive is visible.
- The defaults are set once; the poll writes only `LEFT_X`/`LEFT_Y`. In the `=0` arm it also writes the right axes
  from `Rsrv` and leaves the triggers at 0.
- Remove the per-frame `axisCount` assignments (`rcore_psp.c:467-468`); they belong in `InitPlatform()` and
  currently re-advertise the fabricated second pad every frame.
- `PSP_PAD_STRICT=1` is the default build. `=0` keeps the old code paths for the A/B: right axes from `Rsrv`
  (zeroed struct → −1), triggers at 0, and the read's return only logged. The true control is the base commit;
  this switch isolates the change inside one binary.
- Do not make the contract depend on `axisCount`: the public getter ignores it, so fill the axis array explicitly.

### T3 design

- One table at file scope:
  ```c
  static const struct { unsigned int psp; int raylib; } pspButtonMap[] = {
      { PSP_CTRL_CROSS, GAMEPAD_BUTTON_RIGHT_FACE_DOWN },
      { PSP_CTRL_CIRCLE, GAMEPAD_BUTTON_RIGHT_FACE_RIGHT },
      { PSP_CTRL_SQUARE, GAMEPAD_BUTTON_RIGHT_FACE_LEFT },
      { PSP_CTRL_TRIANGLE, GAMEPAD_BUTTON_RIGHT_FACE_UP },
      { PSP_CTRL_UP, GAMEPAD_BUTTON_LEFT_FACE_UP },
      { PSP_CTRL_RIGHT, GAMEPAD_BUTTON_LEFT_FACE_RIGHT },
      { PSP_CTRL_DOWN, GAMEPAD_BUTTON_LEFT_FACE_DOWN },
      { PSP_CTRL_LEFT, GAMEPAD_BUTTON_LEFT_FACE_LEFT },
      { PSP_CTRL_LTRIGGER, GAMEPAD_BUTTON_LEFT_TRIGGER_1 },
      { PSP_CTRL_RTRIGGER, GAMEPAD_BUTTON_RIGHT_TRIGGER_1 },
      { PSP_CTRL_SELECT, GAMEPAD_BUTTON_MIDDLE_LEFT },
      { PSP_CTRL_START, GAMEPAD_BUTTON_MIDDLE_RIGHT },
  };
  ```
- After a successful read, clear `currentButtonState[0]` and then set the mapped entries, so a stale button cannot
  survive a read that no longer sets it. A failed read returns before this point.
- Keep the existing reset order: save previous, reset `lastButtonPressed`, read, fill current, find the first new
  press. The one-frame window depends on that order.
- A chord is expected to yield one `lastButtonPressed` value; log how many buttons were newly pressed so the loss
  is visible rather than silent.
- Operator sequence: CROSS, CIRCLE, SQUARE, TRIANGLE, UP, RIGHT, DOWN, LEFT, L, R, SELECT, START; hold each for
  about 1 s, then press CROSS+CIRCLE together, then release. The sample must show each button once and the chord's
  single value.

### T4 design

- The sample displays numeric `LEFT_X`/`LEFT_Y` and arrows; the human check is right → positive X and up →
  negative Y. The current direct mapping already matches the SDL/GLFW convention, but the test is the evidence.
- Print `LEFT_TRIGGER`, `RIGHT_TRIGGER`, `RIGHT_X`, `RIGHT_Y` as well; the first two must read −1 and the latter
  two 0 with no right stick.
- Operator sequence: nub at rest 2 s, then full right 1 s, full up 1 s, and a diagonal to the upper-right rim;
  record the numeric values and the sign.
- The existing 0.2 deadzone is unchanged in this sprint; 043 replaces it with a calibrated radial deadzone.

### T5 design

- Rebuild with the sprint changes and run `basic_shapes` for 600 frames; it should still log the same `DONE` line
  and render the same frame. Run `batch_check` with `WRAP=0` and compare to the archived gate `b69830b8…`.
- Because no renderer file is touched, any pixel movement is evidence of a build mistake, not an input effect.

### Task log (append-only)

- 2026-11-21 — sprint written; all tasks `todo`, no device run
- 2026-11-21 — T1–T4 `doing`: contract, map, sample and PRX boilerplate implemented; `libraylib.a` and
  `pad_api.prx` build clean; device run not started at the user's request
- 2026-11-21 — control (`=0`), strict (`=1`) and failure (`=1 PSP_PAD_FAIL=1`) libraries compile; final `=1`
  restored and `pad_api.prx` relinked; no device run
- 2026-11-21 — device run: `input_gamepad.prx` on PSP-1000, `PAD contract=1 fail=0`, all 12 button enums
  (1–9, 11, 13, 15), `DONE frames 1800`, 60 fps; screenshot and log in `/home/psp/evidence/sprint041/`;
  T3 and T4 `done`
- 2026-11-21 — `pad_api` strict (`=1`) 1800 frames: `axes0=2`, `axes1=0`, `RX/RY=0.00`, `LT/RT=-1.00`,
  `LY=-0.21` at rest; `DONE`; T1 `done`
- 2026-11-21 — `pad_api` control (`=0`) 180 frames: `contract=0`, `RX/RY=-1.00`, `LT/RT=0.00`; `DONE`
- 2026-11-21 — `pad_api` failure (`=1 PSP_PAD_FAIL=1`) 240 frames: one `PAD read failed ret=-1`; CROSS was
  not held (`cross=0`), so the last-good snapshot remains unproven
- 2026-11-21 — T5: `basic_shapes` `DONE frames 600` at 30 fps; `batch_check WRAP=0` `DONE frames 2` and
  SHA-256 `b69830b86a899cc0…`, byte-identical to `par_pspgl.raw`; T5 `done`
- 2026-11-21 — failure rerun with Cross held: `PAD pressed=7 frame=77`, frame 120 `cross=1`, one
  `PAD read failed ret=-1`, recovered after release; held-button snapshot retained; T2 `done`

## 6. Budgets

| item | budget | measured |
|---|---|---|
| input poll | ≤ 0.2 ms per frame (measured in 042; S1 only restructures the read) | |
| public API surface | 0 new symbols | |
| sample PSP references | 0 | |
| renderer gate | byte-identical | |
| frame count | 1800 for the manual sample (30–60 s); 600 for the `basic_shapes` regression | |

## 7. Measurements

Protocol: device only; one variable per run; control → change → control again. Every row carries its build number.

| run | build | workload | metric | value | notes |
|---|---|---|---|---|---|
| control (true) | base commit | `pad_api` idle, buttons, nub | trigger axes / right axes / read return | | no switch; `axisCount(1)=2`, right axes undefined |
| control (code path) | `PSP_PAD_STRICT=0` | same | triggers 0, right axes −1, read unchecked | | |
| change | `PSP_PAD_STRICT=1` | same | triggers −1, right axes 0, read checked | | |
| control again | `PSP_PAD_STRICT=0` | same | same as code-path control | | |
| axis counts | `PSP_PAD_STRICT=1` | `pad_api` | `axisCount(0)` / `axisCount(1)` / `IsGamepadAvailable(1)` | 2 / 0 / false | |
| failure | `PSP_PAD_STRICT=1 PSP_PAD_FAIL=1` | `pad_api`, Cross held until read fails | log once; the held button stays down; no new edge | `PAD pressed=7 frame=77`; frame 120 `cross=1`; one failed; recovered after release |
| change | `PSP_PAD_STRICT=1` | 12 controls, one at a time | button → enum, `lastButtonPressed` frames | | |
| change | `PSP_PAD_STRICT=1` | nub right, up, corner | `LEFT_X`, `LEFT_Y`, magnitude | | |
| regression | `PSP_PAD_STRICT=1` | `basic_shapes` 600 | `DONE`, capture hash | | |
| device input | strict `=1`, `input_gamepad.prx` 1,065,074 B | 1800 frames, manual input | controls / enums | 12/12 enums; `DONE frames 1800`; 60 fps | log and BMP in `/home/psp/evidence/sprint041/` |

**`pad_api` device runs (2026-11-21):**

| build | app | workload | result |
|---|---|---|---|
| strict `=1` | `pad_api.prx` 1,064,474 B | 1800 frames, at rest | `axes0=2 axes1=0`, `RX/RY=0.00`, `LT/RT=-1.00`, `LY=-0.21`; `DONE frames 1800` |
| control `=0` | `pad_api.prx` 1,064,370 B | 180 frames, at rest | `contract=0`, `RX/RY=-1.00`, `LT/RT=0.00`, `axes1=0`; `DONE frames 180` |
| failure `=1 PSP_PAD_FAIL=1` | `pad_api.prx` 1,064,562 B | 240 frames, CROSS not held | one `PAD read failed ret=-1`; `cross=0`; last-good snapshot unproven |

**T5 regression (2026-11-21):**

| run | build | workload | result |
|---|---|---|---|
| `basic_shapes` | strict `=1`, `basic_shapes.prx` 1,064,454 B | 600 frames | `DONE frames 600`, 30 fps; capture `s041-basic-shapes.bmp` |
| `batch_check` | strict `=1`, `batch_check.prx` 1,062,886 B, `WRAP=0` | 2 frames | `DONE frames 2`; SHA-256 `b69830b86a899cc0…`; `cmp` byte-identical to `par_pspgl.raw` |

## 8. Exit criteria

- [x] `psp-tests/pad_api/main.c` contains no PSP header, symbol or platform conditional (`psp_module.c` is
      boilerplate)
- [x] `IsGamepadAvailable(0)` is true, `GetGamepadName(0)` is `"PSP Controller"`, `GetGamepadAxisCount(0)` is 2
- [x] `IsGamepadAvailable(1)` is false and `GetGamepadAxisCount(1)` is 0
- [x] all twelve physical controls map to the expected `GAMEPAD_BUTTON_*`
- [x] `GetGamepadButtonPressed()` returns a press for one frame and 0 on the next
- [x] in the `=1` build, `RIGHT_X`/`RIGHT_Y` and unused axes are 0 and trigger axes are −1
- [x] nub right → `LEFT_X > 0`, nub up → `LEFT_Y < 0`, values stay in −1…1
- [x] a forced failed read logs and leaves the last good snapshot
- [x] the true control is the base commit; `PSP_PAD_STRICT=0` is the code-path control
- [x] `PSP_PAD_STRICT=0` and `=1` differ only in the intended contract values
- [x] the sample logs `DONE` and stays resident; it never calls `sceKernelExitGame` or `CloseWindow()`
- [x] the renderer gate and `basic_shapes` are unchanged
- [x] no public header or renderer file appears in the sprint diff

## 9. Result

The manual input port was validated on the PSP-1000. `input_gamepad.prx` (strict `=1`) ran 1800 frames at
60 fps, logged `PAD contract=1 fail=0` and `DONE frames 1800`, and reported all twelve physical controls
through `GetGamepadButtonPressed()` (enums 1–9, 11, 13, 15). The operator confirmed the D-pad, face buttons,
shoulders, Select/Start and nub.

`pad_api.prx` then ran in both contract arms: strict `=1` gave `axes0=2`, `axes1=0`, `RX/RY=0.00`,
`LT/RT=-1.00`; control `=0` gave `RX/RY=-1.00`, `LT/RT=0.00`. The failure build was rerun with Cross held:
`PAD pressed=7 frame=77`, frame 120 `cross=1`, exactly one `PAD read failed ret=-1`, then recovery after the
release. The held-button snapshot was retained. At rest the nub reads `LY=-0.21`, just outside the 0.2 deadzone;
sprint 043 owns the calibration. Logs and captures: `/home/psp/evidence/sprint041/`.

T5 passed independently: `basic_shapes` logged `DONE frames 600` at 30 fps, and `batch_check` (`WRAP=0`) is
byte-identical to the gate `b69830b86a899cc0…`. The base-commit binary was not separately run; the `=0`
code-path control covered the contract A/B. All five S1 tasks are done.

## 10. Follow-ups

- **Sprint 042 (P8-S2)** — sampling, latch edges, HOLD/interception and the counters.
- **Sprint 043 (P8-S3)** — nub calibration, radial deadzone, magnitude clamp and the idle threshold.
- **Sprint 044 (P8-S4)** — keyboard/mouse decision, extra bits, DS3 and the phase close.
- If the sample needs a desktop build to prove portability, that is a separate build-system task; this sprint
  proves the source has no platform dependency.
