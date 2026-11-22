# Sprint 044 (P8-S4) — Decisions, compatibility and the Phase 8 close

| | |
|---|---|
| Roadmap phase | [Phase 8 — Input](../phases/08-input/index.md) |
| Sprint | P8-S4 |
| Branch / base commit | `master` @ the 043 close (041–043 done; 043 closed 2026-11-22 in the working tree) |
| Started / closed | 2026-11-22 / 2026-11-22 |
| Build number(s) | pspgu `b24` (unchanged); `libraylib.a` builds with `PSP_PAD_AS_KEYBOARD=0\|1` and `PSP_PAD_AS_MOUSE=0\|1`, both **off by default**. `PSP_PAD_PAD2` is not added unless a second pad reaches the bench |
| Tasks | 5/5 done — T1–T5 done; Phase 8 closed |

## 0. Handover

**Situation.** Sprints 041–043 close the controller contract, the sampling strategy and the nub calibration. What
remains is a set of decisions that change the platform's meaning rather than its numbers: HOME/HOLD and the
PSPSDK-only bits, keyboard/mouse emulation, a second pad on PSP Go, the name string and the warning logs. This
sprint records each decision, implements only the ones adopted, runs the phase's exit table and closes Phase 8.

**Read before acting**, in this order:

1. `../phases/08-input/index.md` — the exit criteria and the “What we already know” list; every row is answered
   here or named as deferred.
2. `../sprints/041-phase8-gamepad-contract.md`, `../sprints/042-phase8-sampling-and-edges.md` and
   `../sprints/043-phase8-nub-calibration.md` §7/§9 — the evidence this close consumes.
3. `../phases/13-samples-porting/index.md` — the tier-B count (60 samples) that depends on the pad→keyboard /
   nub→mouse decision.
4. `../memory-map.md` §12 question 5 and §13 — the open decision and the method that requires it to be recorded.
5. `src/platforms/rcore_psp.c:384` (`SetGamepadMappings` warning), `:564-570` (the commented keyboard
   previous-state loop) and `:851-859` (`SetGamepadVibration` warning).
6. `src/rcore.c:3905-3911` — `IsKeyPressed` compares `previousKeyState` with `currentKeyState`; `:3974-3985` is
   the `GetKeyPressed()` queue. If keyboard emulation is adopted, the platform must update both or a held key
   reports pressed every frame and `GetKeyPressed()` stays 0.

**First action.** Confirm the §5 T1 decision table on the device where a row is not already evidenced. Do not
change code until every row says “adopt”, “reject” or “defer” and names the reason.

**Hard rules.**

- **No new public API.** The game still uses `IsGamepad*`, `GetGamepad*`, and (if adopted) `IsKeyDown`/mouse; do
  not add `pspGamepad*` symbols or a platform header to the game surface.
- **Keyboard/mouse emulation is off by default and behind build switches.** If adopted, `PSP_PAD_AS_KEYBOARD=1`
  and `PSP_PAD_AS_MOUSE=1`; the default build keeps the old input meaning. It redefines `IsKeyDown` and the mouse
  on this platform, so the decision and its effect on Phase 13 must be recorded.
- **Update `previousKeyState` and the press queue when emulating keys.** The imported platform file leaves the
  keyboard previous-state loop commented out (`rcore_psp.c:564-570`). Without it, `IsKeyPressed` fires every frame
  a mapped key is held. `GetKeyPressed()` reads `keyPressedQueue` (`rcore.c:3974-3985`), so a new emulated press
  must also be pushed there, after the poll's queue reset.
- **Positional mapping stays.** CROSS remains `RIGHT_FACE_DOWN`; a regional “Circle confirms” choice belongs in a
  game-level action map.
- **A name or log string is a decision.** If the game-visible default must contain no PSP string, choose a generic
  `GetGamepadName()` and make `SetGamepadVibration()` silent or debug-level; record the choice. The hardware name
  is otherwise allowed because it is data, not a compile-time dependency.
- **The close is evidence.** Every Phase 8 exit criterion is answered with a sprint number and a value, or marked
  deferred with the phase that owns it. “Reviewed” is not an answer.
- **No renderer change.** The gate is byte-identical.
- **Stage only this sprint's paths.** `git status` before every commit.
- Commit messages carry the task ID: `sprint 044 T1: ...`.

**Commands.**

```sh
export PSPDEV=/home/psp/pspdev
export PATH=$PATH:$PSPDEV/bin

# Shipped build: emulation off, semantics unchanged.
cd /home/psp/raylib-psp/src
make clean && make PLATFORM=PLATFORM_PSP -j8
cp libraylib.a $PSPDEV/psp/lib/libraylib.a

# Keyboard/mouse arm.
make clean && make PLATFORM=PLATFORM_PSP -j8 PSP_PAD_AS_KEYBOARD=1 PSP_PAD_AS_MOUSE=1
cp libraylib.a $PSPDEV/psp/lib/libraylib.a

# Portable key probe (IsKeyPressed + GetKeyPressed) and the upstream sample.
cd ../psp-tests/pad_api && make clean && make KEYS=1
cp pad_api.prx /tmp/s3/pad_api.prx
cd ../samples/core/input_keys && make clean && make
cp input_keys.prx /tmp/s3/input_keys.prx

cd /tmp/s3 && timeout 20 pspsh -e "reset"; sleep 6
( printf './input_keys.prx\n'; sleep 30 ) | timeout 50 pspsh
```

`host0` is `/tmp/s3` here. Keep the shipped `AS_* = 0` build staged unless a run is explicitly the emulation arm;
T5 ends by restoring it.

**Do not touch.** The 041–043 contract, sampling and calibration; the renderer and pspgu; the public headers.

**Done means** §8: every Phase 8 exit criterion is answered, the adopted compatibility path is proven on a
sample, and the phase page is marked done.

## 1. Objective

Four falsifiable results:

1. Every item in the Phase 8 exit table and every “extra bit” (HOME, HOLD, NOTE, SCREEN, VOLUP/VOLDOWN, WLAN_UP,
   L1/R1, L2/R2, L3/R3) has a recorded decision with a reason.
2. The keyboard/mouse question is either implemented behind switches and demonstrated on an upstream example, or
   rejected with the effect on Phase 13's 60 tier-B samples stated.
3. The second-pad/DS3 question is decided and, if adopted, gamepad 1 appears through the normal
   `IsGamepadAvailable(1)` path; otherwise the PSP-1000 limit is recorded.
4. The game-visible name/log choices are recorded and the phase page is marked done with the regression green.

## 2. Why now

These are the decisions that would otherwise leak into game code or be discovered by a sample that “almost”
runs. They come last because 041–043 provide the evidence: a decision about keyboard emulation is meaningless
before the gamepad contract is stable, and the extra-bit policy depends on what the controller service actually
reports. This sprint also closes the phase, so Phase 13's tier-B samples know what they can rely on.

## 3. Scope

**In:** `src/platforms/rcore_psp.c` (decisions and the optional keyboard/mouse emulation); `src/Makefile`
(`PSP_PAD_AS_KEYBOARD`, `PSP_PAD_AS_MOUSE`; `PSP_PAD_PAD2` only if second-pad hardware arrives);
`psp-tests/pad_api` (a `KEYS` mode that exercises `IsKeyPressed`/`GetKeyPressed`); the Phase 8 page and
`memory-map.md` §3/§8/§12.

**Out:** a second pad on PSP-1000 (hardware does not exist); a full user-facing calibration UI (043 follow-up);
the renderer; Phase 13's sample ports themselves.

## 4. Preconditions

- [x] Sprints 041–043 are closed, or their open tasks are explicitly carried with a decision
- [x] The Phase 8 exit table is copied into §5 T1
- [x] The Phase 13 tier-B list and at least one upstream example are available for the compatibility run
- [x] The device loop answers; `reset` leaves partition 2 at 25,149,440 bytes
- [x] The 041–043 captures and counters are archived
- [x] `git status` shows only this sprint's paths

## 5. Tasks

Status vocabulary: `todo` · `doing` · `done` · `blocked` · `dropped`.

| ID | task | status | verification | evidence |
|---|---|---|---|---|
| T1 | **Decision table**: every exit criterion and every PSPSDK-only bit gets adopt/reject/defer with a reason | `done` | no row empty; HOME/HOLD/NOTE/SCREEN/VOL/WLAN/L1-R1/L2-R2/L3-R3 all named | table confirmed in §5/§7; keyboard/mouse adopted behind switches, off by default |
| T2 | **Keyboard/mouse emulation (conditional)**: `PSP_PAD_AS_KEYBOARD=0\|1`, `PSP_PAD_AS_MOUSE=0\|1`; D-pad/nub → arrows; update `previousKeyState` and the press queue | `done` | one upstream example runs unmodified with the switch on; off is unchanged; `IsKeyPressed` fires once per press and `GetKeyPressed` returns it once | `input_keys` moved the ball (240→320→220 X, 136→60→168 Y); each tap logged one `KEY pressed` and one `KEY queue`; mouse `240,136`→`166,47`→`315,66` with no rest drift; off build stayed at `240,136` |
| T3 | **Second pad / DS3 (conditional)**: if adopted, fill `ready[1]`, `axisCount[1]`, `name[1]`, right axes and L3/R3; otherwise record the PSP-1000 limit | `done` | `IsGamepadAvailable(1)` true on the tested hardware, or the decision names why not | reject on this bench: `ready1=0`, `axes1=0` (041 T5), no PSP Go/DS3 hardware; no switch added |
| T4 | **Cleanup**: decide and apply the name string, vibration log level and `SetGamepadMappings` behaviour | `done` | `GetGamepadName()` is the chosen value; no unexpected per-frame warning; no public API change | name stays `"PSP Controller"`; `SetGamepadMappings`/`SetGamepadVibration` messages moved to `LOG_DEBUG`, both once-only; no API change |
| T5 | **Phase close**: run the full exit table and regression; update the phase page and memory map | `done` | every criterion answered; gate hash unchanged; Phase 8 marked done | contract `2/0`, right `0/0`, triggers `−1/−1`, idle `0.00`; `basic_shapes DONE 600` 60 fps; `batch_check` `b69830b8…` identical; phase page done |

### T1 decision table

Fill this in §7 when the rows are confirmed; empty cells are not allowed. The extra-bit values are from
`pspctrl.h` and the 041–043 device runs.

| item | decision | reason | evidence |
|---|---|---|---|
| Live-state sample | adopt | `pad_api` and `input_gamepad` draw every button and both axes | 041 T1/T3 |
| Every control pressed | adopt | all 12 mapped enums observed on device | 041 `input_gamepad` |
| Zeroed + validated read | adopt | `ret` counters; failed read moves no state | 042 T1/T5 |
| Nub range on this unit | adopt | centre `(133,112)`, ranges `122/133/143/112`, `dz=0.18` | 043 T1/T2 |
| One-frame press | adopt | `lastButtonPressed` cleared each poll; single-frame report | 041 T2, 042 T4 |
| Sub-frame tap | adopt | latch reports `latchMake`, `latchOnly=1` | 042 T2 |
| Right axes 0 | adopt | `axisCount[1]=0`, `RX/RY=0.00` | 041 T5 |
| Magnitude ≤ 1 | adopt | radial clamp reads 1.000 at cardinals and diagonal | 043 T2 |
| Axis directions | adopt | up `LEFT_Y<0`, right `LEFT_X>0` | 041/043 |
| Availability/name/axes | adopt | `ready0=1`, `axes0=2`, name `PSP Controller` | 041 T5; name in T4 |
| Idle timer | adopt | 360 s nub-only run awake, no dim | 043 T3 |
| Counters | adopt | `PAD stats` boot + interval | 042 T4 |
| HOME (`0x010000`) | reject as button; freeze | system interception; dialog owns it | 042 T3 |
| HOLD (`0x020000`) | reject as button; freeze | switch reports buttons released | 042 T3 |
| NOTE (`0x800000`) | reject | header bit exists; never delivered to user code here | 041/042 runs |
| SCREEN (`0x400000`) | reject | same | 041/042 runs |
| VOLUP/VOLDOWN (`0x100000`/`0x200000`) | reject | same | 041/042 runs |
| WLAN_UP (`0x040000`) | reject | same | 041/042 runs |
| REMOTE (`0x080000`) | reject | same | 041/042 runs |
| DISC/MS (`0x1000000`/`0x2000000`) | reject | not controller inputs on this model | `pspctrl.h` |
| L1/R1 (`0x400`/`0x800`) | reject | distinct bits, but hardware reports the shoulder as `LTRIGGER`/`RTRIGGER`; never observed | `pspctrl.h`, 041/042 |
| L2/R2 (`0x100`/`0x200`) | alias | same values as `LTRIGGER`/`RTRIGGER`; already mapped to `LEFT/RIGHT_TRIGGER_1` | `pspctrl.h` |
| L3/R3 (`0x2`/`0x4`) | reject | the nub does not click on PSP-1000; never observed | `pspctrl.h`, 041 |
| Second pad / DS3 | reject on this bench | PSP-1000 has one pad; no PSP Go/DS3 available | 041 T5 |
| Keyboard/mouse | adopt behind switches, off by default | `input_keys` moves the ball; one press/queue per tap; mouse moves without rest drift | T2 |

### T1 design

- Confirm the table above on the device where a row is not already evidenced; copy it into §7 with the final
  wording. Columns: item, decision, reason, evidence (sprint/run), effect on a portable game.
- A deferral names where the work goes, for example “PSP Go/DS3 → a separate hardware sprint”; it is not a
  renderer phase and must not be parked in Phase 10 by default.

### T2 design

- Makefile: `PSP_PAD_AS_KEYBOARD ?= 0`, `PSP_PAD_AS_MOUSE ?= 0`, both numeric defines; `#ifndef` fallbacks 0 in
  `rcore_psp.c`. The shipped build keeps the old meaning.
- Key map: D-pad **or** the calibrated nub past `PSP_PAD_KEY_THRESHOLD` (0.5) sets
  `KEY_LEFT/RIGHT/UP/DOWN`. START/SELECT are not mapped: they are already gamepad buttons, and mapping them to
  `KEY_ENTER`/`KEY_ESCAPE` would change menu semantics without a sample that needs it.
- Order inside `PollInputEvents()`:
  1. At the top, before any read, copy `currentKeyState` to `previousKeyState` for all `MAX_KEYBOARD_KEYS`
     (replace the commented loop at `rcore_psp.c:564`).
  2. After the calibrated axes exist, set the four mapped keys to 0/1 every poll. Keys outside the map are left
     at 0 because there is no real keyboard.
  3. For each mapped key that is now down and was up (`previousKeyState[key] == 0`), push it into
     `CORE.Input.Keyboard.keyPressedQueue` if `keyPressedQueueCount < MAX_KEY_PRESSED_QUEUE`. The queue is reset
     at the top of the same poll, so `GetKeyPressed()` sees the emulated press exactly once.
- Mouse emulation: `previousPosition = currentPosition` first, then if the vector is non-zero
  `currentPosition += axes * PSP_PAD_MOUSE_SPEED * GetFrameTime()`, clamped to the screen. Default speed 600 px/s;
  record it. The calibrated axes are already 0 inside the deadzone, so the cursor does not drift. `GetMousePosition()`
  reads `currentPosition` through `offset`/`scale` (`rcore.c:4206-4220`), so those fields are the only ones to fill.
- Verification:
  - port `examples/core/core_input_keys.c` to `psp-tests/samples/core/input_keys/` using
    `psp-tests/samples/core/input_gamepad` as the scaffold (its `Makefile` + `psp_module.c`, `include
    ../../Makefile.inc`); it uses `IsKeyDown` only, so it proves the D-pad/nub path;
  - add a `KEYS` build mode to `psp-tests/pad_api` (portable, no PSP headers) that logs `KEY pressed` from
    `IsKeyPressed` and the last `GetKeyPressed()` value, to prove one press per physical press;
  - run with the switch on and off; the off build must be unchanged. Mouse uses the same `KEYS` probe: it logs
    `GetMouseX/Y` and draws a crosshair, which is the whole mouse surface a game reads; a separate `input_mouse`
    port is not required.
- Freeze/HOLD: the keyboard and mouse follow the gamepad policy. During HOME/HOLD the poll returns before the axis
  code, so keys are not recomputed and the mouse does not move. On the first clean sample the keys are recomputed;
  a thumb released during the frozen window yields one release edge and no spurious press.
- If rejected: mark T2 `dropped`, record that the 60 tier-B samples keep the sample-local pad/nub layer built in
  P13-S3, and keep the switches out of the build.

### T3 design

- On this bench the decision is **reject**: PSP-1000 has one built-in pad and no PSP Go/DS3 hardware is
  available. Record it in §7; `axisCount[1]` stays 0 and the right axes stay 0. `PSP_PAD_PAD2` is not added.
- If a second pad ever reaches the bench, expose it as gamepad 1 behind `PSP_PAD_PAD2=0|1`: fill `ready[1]`,
  `axisCount[1]`, `name[1]`, its right stick as `RIGHT_X/Y`, and its extra buttons as the matching enums. A game
  looping over `MAX_GAMEPADS` discovers it through `IsGamepadAvailable(1)`. Do not fabricate axes from `Rsrv`.
- A DS3 on a PSP Go is its own hardware sprint, not a hidden branch in this one.

### T4 design

- Name: keep `"PSP Controller"`. It is hardware data, not a compile-time dependency, and it tells the operator
  which pad is bound. A generic `"Gamepad"` would hide that during device runs.
- `SetGamepadVibration()`: keep the no-op, move the message from `LOG_WARNING` to `LOG_DEBUG`. A portable game
  calling it should not warn on every call; the absence of rumble is documented.
- `SetGamepadMappings()`: keep the platform no-op returning 0, move the message to `LOG_DEBUG`. There is no
  mapping database; a game that calls it unconditionally should not warn.
- Check `grep -in psp` on the game-facing sample output after the change: the name is data and stays; the
  warnings must not appear at default log level.

### T5 design

- Run the Phase 8 exit table row by row; each row points at a sprint and a value or a decision.
- Restore the shipped `AS_* = 0` build with `make clean && make PLATFORM=PLATFORM_PSP -j8`, stage it, then run:
  - `psp-tests/pad_api` 1800 frames: axes `2/0`, right `0/0`, triggers `−1/−1`, idle `0.00`, `DONE`;
  - `psp-tests/basic_shapes` 600 frames: `DONE 600`;
  - `psp-tests/batch_check WRAP=0`: `DONE 2`, then `pspsh -e "scrshot host0:/s044-batch.raw"` and compare the
    SHA-256 with `b69830b86a899cc083146402e6333df43007286dd1278dbdbf05db6d9e357e7c`.
- Update `memory-map.md` §3 with the calibrated nub and idle rows (already from 043), §8 with the decision-log row,
  §12 question 5 with the keyboard/mouse resolution, and the Phase 8 page state to **done** with the exit table
  answered.
- Promote a verified input technique to `docs/psp/recipes/` only if it ran on the device; otherwise leave it as a
  candidate in the phase page.

### Task log (append-only)

- 2026-11-21 — sprint written; all tasks `todo`, no device run
- 2026-11-22 — code side implemented: `PSP_PAD_AS_KEYBOARD`/`AS_MOUSE` switches (default 0), `previousKeyState`
  copy, D-pad/nub → arrows with `keyPressedQueue` push, mouse movement with clamp, both no-op messages at
  `LOG_DEBUG`, `pad_api KEYS=1` mode, and `psp-tests/samples/core/input_keys`; both library arms build
  warning-free; T3/T4 recorded; device runs pending
- 2026-11-22 — T2 done: `input_keys` ball moved on D-pad and nub; one `KEY pressed`/`KEY queue` per tap; mouse
  moved and held still at rest; off build static at `240,136`
- 2026-11-22 — T1/T3/T4 done: table confirmed; second pad rejected (no hardware); name kept, no-op logs debug
- 2026-11-22 — T5 done: shipped contract `2/0`, right `0/0`, triggers `−1/−1`, idle `0.00`; `basic_shapes
  DONE 600` 60 fps; `batch_check` `b69830b8…` identical; Phase 8 closed 5/5

## 6. Budgets

| item | budget | measured |
|---|---|---|
| exit criteria answered | 100 % | 15/15; every bit row has a decision |
| keyboard/mouse switch | off by default; one example proven if adopted | off: ball static; on: ball moved and mouse tracked |
| `IsKeyPressed` | exactly one press per physical press when emulated | one line per tap, repeated taps separate |
| `GetKeyPressed` queue | the new press is pushed once, after the poll's queue reset | one `KEY queue` per tap, same frame as the press |
| renderer gate | byte-identical | identical |
| frame time | no renderer change | `input_keys` and renderer checks at 60 fps |

## 7. Measurements

| run | build | workload | metric | value | notes |
|---|---|---|---|---|---|
| decision table | — | Phase 8 exit criteria | rows answered | 15 + all bits | every row has adopt/reject |
| keyboard on | `PSP_PAD_AS_KEYBOARD=1` | `input_keys` + `pad_api KEYS=1` | runs unmodified; one press per press; queue once | ball `240→320→220` X and `136→60→168` Y; `KEY pressed`/`KEY queue` pairs at frames 290, 403, 431/463, 491/519, 554/583 | nub and D-pad both mapped |
| keyboard off | `PSP_PAD_AS_KEYBOARD=0` | same | unchanged semantics | ball stayed `240,136` for 900 frames | shipped build |
| mouse on | `PSP_PAD_AS_MOUSE=1` | `pad_api KEYS=1` | cursor moves, deadzone at rest | `240,136` → `166,47` → `315,66`; static while untouched | clamp reached `0,0` at the edge |
| second pad | no switch | `IsGamepadAvailable(1)` | decision recorded (PSP-1000: false) | `ready1=0 axes1=0` | no hardware |
| regression | shipped `AS_*=0` | `pad_api`, `basic_shapes`, `batch_check` | gate hash | `b69830b8…` identical | 60 fps |

## 8. Exit criteria

- [x] every Phase 8 exit criterion is answered with a sprint and a value, or deferred with the owning phase
- [x] HOME, HOLD, NOTE, SCREEN, VOLUP/VOLDOWN, WLAN_UP, L1/R1, L2/R2 and L3/R3 each have a recorded decision
- [x] the keyboard/mouse decision is recorded; if adopted, one upstream example runs unmodified and
      `IsKeyPressed` fires once per press; if rejected, the Phase 13 effect is stated
- [x] the second-pad/DS3 decision is recorded; if adopted, `IsGamepadAvailable(1)` works through the public API
- [x] the name string and warning-log decisions are applied and recorded
- [x] the 041–043 samples and the renderer gate are unchanged
- [x] the Phase 8 page is marked done with the exit table filled
- [x] the new facts are written into `memory-map.md` §3/§12 and the phase page

## 9. Result

**T1 done (2026-11-22).** The decision table is complete: every Phase 8 exit criterion is answered from 041–043,
and every extra bit has a decision. HOME/HOLD are system-owned and freeze the state; NOTE/SCREEN/VOL/WLAN/
REMOTE/DISC/MS, `L1`/`R1` and `L3`/`R3` are rejected as never delivered on this hardware; `L2`/`R2` are aliases of
`LTRIGGER`/`RTRIGGER` and already map to the shoulder buttons.

**T2 done (2026-11-22).** Keyboard and mouse are adopted behind `PSP_PAD_AS_KEYBOARD=0|1` and
`PSP_PAD_AS_MOUSE=0|1`, both off by default. `input_keys` ran unmodified (480x272, bounded frames) and the ball
moved `240→320→220` X and `136→60→168` Y with D-pad and nub. Each tap produced exactly one `KEY pressed` and one
`KEY queue` at the same frame, so `IsKeyPressed` and `GetKeyPressed` agree. The mouse crosshair moved
`240,136 → 166,47 → 315,66`, clamped at the edge, and did not drift while the nub was untouched. With `AS_*=0` the
same sample kept the ball at `240,136` for 900 frames.

**T3 done (2026-11-22).** Rejected on this bench: PSP-1000 has one pad, `ready1=0` and `axes1=0`; no PSP Go/DS3
hardware was available. No second-pad switch was added.

**T4 done (2026-11-22).** `GetGamepadName(0)` stays `"PSP Controller"`. `SetGamepadMappings()` and
`SetGamepadVibration()` remain no-ops, now at `LOG_DEBUG` and once-only, so a portable game that calls them does
not warn at the default log level.

**T5 done (2026-11-22).** Shipped `AS_*=0` build: contract axes `2/0`, right `0/0`, triggers `−1/−1`, idle
`0.00`; `basic_shapes` `DONE 600` at 60 fps; `batch_check WRAP=0` `DONE 2` with SHA-256 `b69830b8…`, byte-identical
to `par_pspgl.raw`.

**Sprint 044 closed 2026-11-22: 5/5. Phase 8 closed.**

## 10. Follow-ups

- A runtime calibration UI and a per-unit settings file, if a shipped game needs several consoles (043 follow-up).
- Phase 13's 60 tier-B samples consume the keyboard/mouse decision; port them in the wave named on the Phase 13
  page.
- If a PSP Go/DS3 ever reaches the bench, revisit T3 as its own hardware sprint; do not resurrect the fabricated
  right stick.
