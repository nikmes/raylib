# Sprint 044 (P8-S4) — Decisions, compatibility and the Phase 8 close

| | |
|---|---|
| Roadmap phase | [Phase 8 — Input](../phases/08-input/index.md) |
| Sprint | P8-S4 |
| Branch / base commit | `master` @ the sprint 043 close (this doc is written before 041–043 run) |
| Started / closed | — / — |
| Build number(s) | pspgu `b24` (unchanged); `libraylib.a` builds with `PSP_PAD_AS_KEYBOARD`, `PSP_PAD_AS_MOUSE` and `PSP_PAD_PAD2` if adopted |
| Tasks | 0/5 done |

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
3. `../phases/13-samples-porting/index.md` — the tier-B count (61 samples) that depends on the pad→keyboard /
   nub→mouse decision.
4. `../memory-map.md` §12 question 5 and §13 — the open decision and the method that requires it to be recorded.
5. `src/platforms/rcore_psp.c:384` (`SetGamepadMappings` warning), `:436-442` (the commented keyboard
   previous-state loop) and `:513-527` (`SetGamepadVibration` warning).
6. `src/rcore.c:4442` — where `previousKeyState` is consumed by `IsKeyPressed`; if keyboard emulation is adopted,
   the platform must update it or every held key reports pressed every frame.

**First action.** Copy the Phase 8 exit criteria into a decision table in §5 and fill it from the 041–043 evidence.
Do not change code until every row says “adopt”, “reject” or “defer” and names the reason.

**Hard rules.**

- **No new public API.** The game still uses `IsGamepad*`, `GetGamepad*`, and (if adopted) `IsKeyDown`/mouse; do
  not add `pspGamepad*` symbols or a platform header to the game surface.
- **Keyboard/mouse emulation is off by default and behind build switches.** If adopted, `PSP_PAD_AS_KEYBOARD=1`
  and `PSP_PAD_AS_MOUSE=1`; the default build keeps the old input meaning. It redefines `IsKeyDown` and the mouse
  on this platform, so the decision and its effect on Phase 13 must be recorded.
- **Update `previousKeyState` when emulating keys.** The imported platform file leaves the keyboard previous-state
  loop commented out (`rcore_psp.c:436-442`). Without updating it, `IsKeyPressed` fires every frame a mapped key is
  held.
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

cd /home/psp/raylib-psp/src
make PLATFORM=PLATFORM_PSP -j8 PSP_PAD_AS_KEYBOARD=0 PSP_PAD_AS_MOUSE=0
cp libraylib.a $PSPDEV/psp/lib/libraylib.a

cd ../psp-tests/pad_api
make clean && make
cp pad_api.prx /tmp/p8s4/app.prx
cd /tmp/p8s4 && timeout 20 pspsh -e "reset"; sleep 6
( printf './app.prx\n'; sleep 20 ) | timeout 40 pspsh

# If keyboard/mouse emulation is adopted, build the matching upstream example
# with the same source and compare its behaviour with the switch on and off.
```

**Do not touch.** The 041–043 contract, sampling and calibration; the renderer and pspgu; the public headers.

**Done means** §8: every Phase 8 exit criterion is answered, the adopted compatibility path is proven on a
sample, and the phase page is marked done.

## 1. Objective

Four falsifiable results:

1. Every item in the Phase 8 exit table and every “extra bit” (HOME, HOLD, NOTE, SCREEN, VOLUP/VOLDOWN, WLAN_UP,
   L1/R1, L2/R2, L3/R3) has a recorded decision with a reason.
2. The keyboard/mouse question is either implemented behind switches and demonstrated on an upstream example, or
   rejected with the effect on Phase 13's tier-B samples stated.
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
(`PSP_PAD_AS_KEYBOARD`, `PSP_PAD_AS_MOUSE`, `PSP_PAD_PAD2`); `psp-tests/pad_api` (a mode that displays the
compatibility result); the Phase 8 page and `memory-map.md` §3/§8/§12.

**Out:** a second pad on PSP-1000 (hardware does not exist); a full user-facing calibration UI (043 follow-up);
the renderer; Phase 13's sample ports themselves.

## 4. Preconditions

- [ ] Sprints 041–043 are closed, or their open tasks are explicitly carried with a decision
- [ ] The Phase 8 exit table is copied into §5 T1
- [ ] The Phase 13 tier-B list and at least one upstream example are available for the compatibility run
- [ ] The device loop answers; `reset` leaves partition 2 at 25,149,440 bytes
- [ ] The 041–043 captures and counters are archived
- [ ] `git status` shows only this sprint's paths

## 5. Tasks

Status vocabulary: `todo` · `doing` · `done` · `blocked` · `dropped`.

| ID | task | status | verification | evidence |
|---|---|---|---|---|
| T1 | **Decision table**: every exit criterion and every PSPSDK-only bit gets adopt/reject/defer with a reason | `todo` | no row empty; HOME/HOLD/NOTE/SCREEN/VOL/WLAN/L1-R1/L2-R2/L3-R3 all named | — |
| T2 | **Keyboard/mouse emulation (conditional)**: `PSP_PAD_AS_KEYBOARD=0\|1`, `PSP_PAD_AS_MOUSE=0\|1`; D-pad/nub → keys and mouse; update `previousKeyState` | `todo` | one upstream example runs unmodified with the switch on; off is unchanged; `IsKeyPressed` fires once per press | — |
| T3 | **Second pad / DS3 (conditional)**: if adopted, fill `ready[1]`, `axisCount[1]`, `name[1]`, right axes and L3/R3; otherwise record the PSP-1000 limit | `todo` | `IsGamepadAvailable(1)` true on the tested hardware, or the decision names why not | — |
| T4 | **Cleanup**: decide and apply the name string, vibration log level and `SetGamepadMappings` behaviour | `todo` | `GetGamepadName()` is the chosen value; no unexpected per-frame warning; no public API change | — |
| T5 | **Phase close**: run the full exit table and regression; update the phase page and memory map | `todo` | every criterion answered; gate hash unchanged; Phase 8 marked done | — |

### T1 design

- The table has one row per Phase 8 exit criterion and one row per extra bit. Columns: item, decision, reason,
  evidence (sprint/run), effect on a portable game.
- Suggested starting points, to be confirmed by the runs, not assumed:
  - HOME: intercepted/system-owned; not a gamepad button. `PSP_CTRL_HOME` is the `SCE_CTRL_INTERCEPTED` bit.
  - HOLD: system switch; digital buttons read released; no gamepad button.
  - NOTE/SCREEN/VOLUP/VOLDOWN/WLAN_UP/REMOTE/DISC/MS: undelivered unless a device sample proves otherwise.
  - L1/R1/L2/R2/L3/R3: aliases/extras in `pspctrl.h`; ignore on PSP-1000 unless the DS3 path is adopted.
- A deferral names where the work goes, for example “PSP Go/DS3 → a separate hardware sprint”; it is not a
  renderer phase and must not be parked in Phase 10 by default.

### T2 design

- If adopted: the D-pad maps to `KEY_UP`/`KEY_DOWN`/`KEY_LEFT`/`KEY_RIGHT`; the nub maps to the arrow keys with the
  calibrated deadzone, and optionally to mouse movement with a configurable speed. START/SELECT could map to
  `KEY_ENTER`/`KEY_ESCAPE` only if a sample needs it; decide, don't guess.
- Fill `CORE.Input.Keyboard.currentKeyState` and update `previousKeyState` at the top of the poll. The imported
  file's loop is commented out; `IsKeyPressed` reads `previousKeyState` (`rcore.c:4442`), so without the update a
  held key fires every frame.
- Mouse emulation: fill `CORE.Input.Mouse.currentPosition`/`previousPosition` consistently; do not move the
  cursor when the nub is inside the deadzone.
- The switch must be off by default: a game that expected `IsKeyDown` to mean a keyboard keeps its old meaning.
- Verification uses one upstream example that renders and reads keys (`core_input_keys`) and one mouse example if
  mouse emulation is adopted. Record the example, build, and whether it runs unmodified.
- If rejected: mark T2 `dropped`, record the consequence for Phase 13's 61 tier-B samples, and keep the switch out
  of the build.

### T3 design

- PSP-1000 has one built-in pad; a second pad means a DS3 on a PSP Go or a hacked/emulated host. If that hardware
  is not on the bench, the decision is “not implemented; PSP-1000 has one pad”, and the right-stick bytes stay 0.
- If adopted, expose the second controller as gamepad 1 behind `PSP_PAD_PAD2=0|1`, not as gamepad 0's right
  stick: fill `ready[1]`, `axisCount[1]` and `name[1]`; map its right stick to `RIGHT_X/Y` and its extra buttons
  to the matching enums. A game that loops over `MAX_GAMEPADS` discovers it through `IsGamepadAvailable(1)`.
- Do not fabricate axes from reserved bytes. The 041 contract already zeroes them.

### T4 design

- Name: `GetGamepadName(0)` may be `"PSP Controller"` (hardware data) or a generic `"Gamepad"` (no PSP string).
  Record which; the choice does not affect any API behaviour.
- `SetGamepadVibration()`: keep the no-op; choose `LOG_DEBUG` or silent if the game-visible log must not mention
  PSP. Do not remove the function.
- `SetGamepadMappings()`: keep the platform no-op; if a portable game calls it unconditionally, decide whether the
  warning is acceptable or should be a silent return. No mapping database exists on this platform.
- Any string change is checked against `grep -in psp` on the game-facing sample output, not the library internals.

### T5 design

- Run the Phase 8 exit table row by row; each row points at a sprint and a value or a decision.
- Re-run `basic_shapes`, `batch_check` and the 041–043 samples; the renderer gate must be unchanged.
- Update `memory-map.md` §3 with the calibrated nub row (from 043), §8 with the decision-log row, §12 question 5
  with the keyboard/mouse resolution, and the Phase 8 page state to **done** with the exit table answered.
- Promote a verified input technique to `docs/psp/recipes/` only if it ran on the device; otherwise leave it as a
  candidate in the phase page.

### Task log (append-only)

- 2026-11-21 — sprint written; all tasks `todo`, no device run

## 6. Budgets

| item | budget | measured |
|---|---|---|
| exit criteria answered | 100 % | |
| keyboard/mouse switch | off by default; one example proven if adopted | |
| `IsKeyPressed` | exactly one press per physical press when emulated | |
| renderer gate | byte-identical | |
| frame time | no renderer change | |

## 7. Measurements

| run | build | workload | metric | value | notes |
|---|---|---|---|---|---|
| decision table | — | Phase 8 exit criteria | rows answered | | |
| keyboard on | `PSP_PAD_AS_KEYBOARD=1` | `core_input_keys` | runs unmodified; one press per press | | |
| keyboard off | `PSP_PAD_AS_KEYBOARD=0` | same | unchanged semantics | | |
| mouse on | `PSP_PAD_AS_MOUSE=1` | mouse example | cursor moves, deadzone at rest | | |
| second pad | `PSP_PAD_PAD2=1` | `IsGamepadAvailable(1)` | true on the tested hardware, or decision | | |
| regression | adopted build | `basic_shapes`, `batch_check` | gate hash | | |

## 8. Exit criteria

- [ ] every Phase 8 exit criterion is answered with a sprint and a value, or deferred with the owning phase
- [ ] HOME, HOLD, NOTE, SCREEN, VOLUP/VOLDOWN, WLAN_UP, L1/R1, L2/R2 and L3/R3 each have a recorded decision
- [ ] the keyboard/mouse decision is recorded; if adopted, one upstream example runs unmodified and
      `IsKeyPressed` fires once per press; if rejected, the Phase 13 effect is stated
- [ ] the second-pad/DS3 decision is recorded; if adopted, `IsGamepadAvailable(1)` works through the public API
- [ ] the name string and warning-log decisions are applied and recorded
- [ ] the 041–043 samples and the renderer gate are unchanged
- [ ] the Phase 8 page is marked done with the exit table filled
- [ ] the new facts are written into `memory-map.md` §3/§12 and the phase page

## 9. Result

Not run yet.

## 10. Follow-ups

- A runtime calibration UI and a per-unit settings file, if a shipped game needs several consoles (043 follow-up).
- Phase 13's tier-B samples consume the keyboard/mouse decision; port them in the wave named on the Phase 13 page.
- If a PSP Go/DS3 ever reaches the bench, revisit T3 as its own hardware sprint; do not resurrect the fabricated
  right stick.
