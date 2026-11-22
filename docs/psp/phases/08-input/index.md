# Phase 8 — Input

| | |
|---|---|
| State | **done** — P8-S1–S4 closed (5/5, 6/6, 5/5 resolved, 5/5); Phase 8 closed 2026-11-22 |
| Depends on | Phase 1 (a running build to press buttons in) — independent of the renderer phases |
| Exit criterion | every control reachable through raylib's API, `GetGamepadButtonPressed()` fires for exactly the frame whose poll saw the press, and the nub's measured range is recorded on this console |

## What this phase is about

Making the device's controls usable through raylib's normal input calls, with a nub that behaves predictably.
Input is not a separate subsystem in raylib: `IsKeyDown`, `IsGamepadButtonDown` and `GetGamepadAxisMovement`
read `CORE.Input` and are platform-agnostic. A platform only has to fill that struct in `PollInputEvents()` —
which is where all of this work lands, in the platform file, not in `rcore.c`.

The immediate consumers are the upstream examples and [Phase 13](../13-samples-porting/index.md)'s tier-B
samples: 60 samples render but need their input remapped. Sprint 044 keeps emulation behind an off-by-default
switch, so they run unmodified when built with `PSP_PAD_AS_KEYBOARD=1`; the shipped build leaves the remap to the
game or the sample-local layer.

## What has to be true before starting

- [x] A build that renders something and can display live input state
- [x] The imported platform file's `PollInputEvents()` exists (`src/platforms/rcore_psp.c`); `InitPlatform()`
      already sets `sceCtrlSetSamplingCycle(0)` and `sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG)`

## Tasks

- [ ] Verify the button mapping one by one: CROSS, CIRCLE, SQUARE, TRIANGLE, D-pad, L/R shoulders, SELECT, START
- [ ] Verify edge detection on device. The imported port **does** set `lastButtonPressed` on a new press
      (`src/platforms/rcore_psp.c:497`), so the 5.5-era "never fires" task is stale. The reset at the top of
      each poll (`:426`) makes the value valid for exactly one frame; define and test that window. A press that
      begins and ends between two frames is still lost — decide whether snapshot polling is enough or latch
      data is needed
- [x] Initialise the controller read: zero `SceCtrlData` and branch on `sceCtrlReadBufferPositive()`'s return,
      so a failed or empty read cannot present stale buttons or axes
- [x] Fix the right stick: `Rx`/`Ry` are not part of the documented PSP-1000 contract, yet the port derives
      `GAMEPAD_AXIS_RIGHT_*` from them (`rcore_psp.c:483`). Zero `axisState` (clearing `axisCount` is not
      enough — `GetGamepadAxisMovement()` ignores it, `rcore.c:4119`) and decide whether a DS3 is gamepad 0's
      right stick or a second pad
- [x] Calibrate the nub: sample raw min/max/centre per unit, derive a radial deadzone from it, and clamp
      `sqrt(x² + y²)` to 1; do not assume 128 is centre or that one unit's deadzone fits another
- [x] Verify axis direction on device: nub up gives `LEFT_Y < 0`, right gives `LEFT_X > 0`, matching raylib's
      SDL/GLFW convention; nothing tests this today
- [x] Decide and document what HOME, HOLD and the extra PSPSDK bits do (NOTE, SCREEN, VOLUP/VOLDOWN, WLAN_UP,
      L1/R1, L2/R2, L3/R3). `PSP_CTRL_HOME` (bit 16) is the same bit Sony calls `SCE_CTRL_INTERCEPTED`; it is
      not a button press
- [x] Decide `sceCtrlSetIdleCancelThreshold`. Its default of −1 means nub movement does not reset the system
      idle timer
- [x] Choose the sampling strategy (`sceCtrlSetSamplingCycle`, once per frame vs fixed rate) and justify it.
      Document that `ReadBuffer*` blocks when the current cycle was already read and that `PeekBuffer*` does
      not; state how a slow frame's queued samples are handled
- [x] Instrument the read and the edges: counts of return codes (0/negative/>1), queue lag, taps seen by the
      latch but missed by the snapshot, and time blocked in `PollInputEvents()`
- [x] Decide whether to also fill `CORE.Input.Keyboard` from the pad and map the nub to the mouse. It makes
      upstream examples and [Phase 13](../13-samples-porting/index.md)'s tier-B samples run unmodified; it also
      redefines `IsKeyDown` and the mouse on this platform, so record the choice — see
      [open question 5](../../memory-map.md#12-open-questions)
- [ ] Document the one-reader rule: `sceCtrlReadBuffer*` and `sceCtrlReadLatch` are global resources owned by
      one thread; every other thread uses the non-consuming `sceCtrlPeekBuffer*`
- [ ] Keep `SetGamepadVibration()` a documented no-op — there is no rumble

## What we already know

- The imported platform file maps buttons to `GAMEPAD_BUTTON_*`, sets two axes, normalises raw 0–255 to −1…1 and
  applies a 0.2 deadzone
- It sets `axisCount[1] = 2` with a comment about PSP Go / DualShock 3, but implements nothing for a second pad
  (`ready[1]` is never set, so `IsGamepadAvailable(1)` is false)
- The same file reads the right analog axes from `pad.Rsrv[0]`/`Rsrv[1]`, which alias `Rx`/`Ry` (the header
  deprecates `Rsrv` in their favour). On a PSP-1000 those bytes are outside the documented contract, and the
  struct is not zeroed before the read, so the value exposed through
  `GetGamepadAxisMovement(0, GAMEPAD_AXIS_RIGHT_*)` is undefined — not reliably `(0 − 128) / 128 = −1` as an
  earlier note assumed. Measure it; do not infer it
- `GetGamepadAxisMovement()` (`src/rcore.c:4119`) checks `axis < MAX_GAMEPAD_AXES`, not `axisCount`, so the
  right axes stay visible even if `axisCount[0]` is reduced
- `InitPlatform()` already sets `sceCtrlSetSamplingCycle(0)` and `sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG)`
  (`rcore_psp.c:586-587`); the "default is digital only" fact below does not describe this build
- No analog triggers and no rumble exist on any PSP model — features relying on either must degrade visibly
- PPSSPP accepts synthetic input, so behaviour must be confirmed on the console

## Controller-service facts

Sources: the public PSPSDK header `pspctrl.h`, and Sony's Controller Overview / Reference in the 6.6 SDK tree
(`/home/psp/psp_sdk_660/.../document/html/peripheral/`). The Sony tree is a **reading aid only**
([§11.4](../../memory-map.md#the-sony-sdk-what-it-is-for-and-what-it-is-not)): a
behaviour used in code must be reproduced from the public header or measured here, not copied from that tree.

| fact | value | tag |
|---|---|---|
| Sampling cycle 0 | sample every VBLANK period; the default at start-up | `[external: Sony Controller Reference]` |
| Sampling cycle 5555–20000 | asynchronous sampling, period in **microseconds** (~180–50 Hz); values between 1 and 5554 are invalid | `[external: same]` |
| `sceCtrlReadBufferPositive()` | blocking if called again before the current sample exists; returns the number of samples read. With `count = 1` it consumes at most one queued sample per call | `[public: pspctrl.h]` |
| Buffer depth | 64 samples; a return value **> 1** means frames were missed and history can be merged | `[external: Sony Controller Overview]` |
| `sceCtrlPeekBufferPositive()` | reads the latest sample without consuming the buffer and does not block | `[external: same]` |
| `sceCtrlReadLatch()` | cumulative make/press/break/release since the last read; clears on read; can bridge more than 64 cycles | `[external: Sony Controller Reference]` |
| Buffer/latch ownership | global system resources; only one thread may read them, others must peek | `[external: Sony Controller Overview]` |
| Default sampling mode | digital buttons only; analog values are valid only in `PSP_CTRL_MODE_ANALOG` (= 1) | `[public: pspctrl.h]` |
| Analog range | `Lx`/`Ly` are 0–255, centre 0x80, horizontal left→right and vertical **top→bottom** | `[external: Sony Controller Overview]` |
| Centre tolerance | error up to about ±0x40, so neutral cannot be assumed tighter than 0x40–0xC0 | `[external: same]` |
| Nub on this unit | centre raw **(133, 112)**; half-ranges right 122 / left 133 / down 143 / up 112; radial deadzone **0.18**; two resting states ~19 counts apart | `[measured: PSP-1000, sprint 043]` |
| Full scale | each axis reaches 0x00/0xFF over its own travel, but **not guaranteed simultaneously** — the stick's reachable area is not a square | `[external: same]` |
| Idle timer threshold | `sceCtrlSetIdleCancelThreshold(idlereset, idleback)`: −1 = analog never cancels idle, 0 = idle timer cancelled even without movement, 1–128 = movement required on either axis | semantics `[public: pspctrl.h]`; default `[external: Sony Controller Reference]` |
| HOLD switch | while set, every digital button reads released and `PSP_CTRL_HOLD` (bit 17) is set | `[external: Sony Controller Overview]` |
| Interception | `PSP_CTRL_HOME` (bit 16) is Sony's `SCE_CTRL_INTERCEPTED`; the PS button itself is not visible to applications, and analog values continue during interception | `PSP_CTRL_HOME` value `[public: pspctrl.h]`; interception `[external: Sony Controller Overview]` |
| Analog-during-HOLD | Sony says sampling continues, but the wording about `Lx`/`Ly` is ambiguous — treat as unverified | `[external: same]` |
| DS3 on PSP Go | Sony 6.3 says the wireless controller's right stick, L3/R3 and vibration are **not available**; PSPSDK's header says `Rx`/`Ry` are valid with a DS3 on a PSP Go. The sources conflict — verify before relying on either | `[external: same]` + `[public: pspctrl.h]` |

The extra PSPSDK bits — `PSP_CTRL_NOTE`, `SCREEN`, `VOLUP`, `VOLDOWN`, `WLAN_UP`, `REMOTE`, `DISC`, `MS` — do
not appear in Sony's user-mode header. Treat them as undelivered until a device sample proves otherwise.
`[public: pspctrl.h]`

## What the reference implementations do

All rows are code scans, not measurements on our console. Their licences are in
[§7](../../memory-map.md#7-reference-sources-read-only-outside-this-repo).

| source | poll | deadzone | edges | notable |
|---|---|---|---|---|
| Sony `sample/graphics/stick` | `ReadBufferPositive`, once per frame | none (`(Lx-128)*0.01`) | `padd = cont & (cont ^ prev)` | START/SELECT on edge; axis scaled to ±1.28 |
| Sony `sample/tips/analog_ctrl` | `ReadBufferPositive`, once per frame | **0x40–0xC0** | `padd = cont & (cont ^ prev)` | idle threshold `(0x40, -1)`; a four-band angle/speed response curve |
| `openTRI` `triInput.c` | `PeekBufferPositive` | 103 < Lx/Ly < 151 (≈ ±24) | `Pressed`/`Held` bitsets; `triInputPressed()` clears the bit it returns | sampling cycle 0; all 12 buttons |
| `PGE` `pgeControls.c` | `PeekBufferPositive` | none in the axis maths | `pressed`/`held`/`released` bitsets | returns early on `PSP_CTRL_HOME`; idle threshold `(25, 25)`; magnitude clamped to 1; angle/magnitude helpers |
| `PSPRuntime` `inputmanager.cpp` | `ReadBufferPositive` | 28 raw counts, configurable | previous/current snapshot | centre hardcoded 128; `GetKeyPressed` fixed priority order |
| 5.5/6.2 port (`rcore_psp.c`) | `ReadBufferPositive` | 0.2 normalised (≈ ±25.6 raw) | snapshot, sets `lastButtonPressed` | unchecked uninitialised read; right axes from `Rx`/`Ry`; no HOLD/INTERCEPTED/idle handling |
| `3D-PSP-Tutorials` (no licence) | `PeekBufferPositive` | none | none | raw −128; ideas only, never copy |

The spread is the finding: three sources use `Peek`, three use `Read`; deadzones range from 0 to 0x40 raw; only
PGE clamps the vector magnitude; only Sony's sample sets the idle threshold deliberately.

## Gaps in the current platform file

Line references are `src/platforms/rcore_psp.c` at `37585003`.

1. **Right stick is fabricated.** `Rx`/`Ry` are read at lines 483–484 and exposed on gamepad 0. On a PSP-1000
   they are outside the documented contract, the `pad` struct is not zeroed, and `axisState` is written
   unconditionally, so `GAMEPAD_AXIS_RIGHT_*` reports an undefined value. `axisCount[1] = 2` (line 468)
   advertises a second pad that is never marked ready. Clearing `axisCount` is not enough —
   `GetGamepadAxisMovement()` ignores it (`rcore.c:4119`) — so zero `axisState` and gate on readiness, then
   decide whether a DS3 is pad 0's right stick or pad 1.
2. **The deadzone is tighter than Sony's own tolerance.** 0.2 normalised is ±25.6 raw counts, while the
   documented centre error is up to ±0x40. A unit at the edge of tolerance will drift with the stick untouched;
   a unit at nominal centre loses usable travel under a 0x40 deadzone. One constant cannot serve both, which is
   why calibration is a task, not a number. The per-axis test is also a square deadzone: calibration should
   produce a radial centre and radius followed by a magnitude clamp, because centre error and reachable travel
   differ per axis and per unit.
3. **Interception is not handled.** During the system pop-up, input is not reported and `PSP_CTRL_HOME` is set.
   The snapshot comparison then sees releases; when input resumes, a still-held button looks newly pressed and
   `GetGamepadButtonPressed()` fires spuriously.
4. **HOLD is not handled.** Digital buttons read released while the switch is on. That is harmless for most
   games, but it should be a documented decision, and the analog-during-HOLD question needs a device check.
5. **The idle timer is left at −1.** A game played only with the nub never resets the system idle timer.
6. **`ReadBufferPositive` blocks or lags; `count = 1` hides it.** At the default sampling cycle the service
   samples once per VBLANK. A frame that finishes early blocks inside `PollInputEvents()` until the next sample
   (`pspctrl.h:282-287`), which silently paces the loop to 60 Hz. A frame that overruns accumulates samples in
   the queue; `count = 1` consumes at most one per call, so input falls behind rather than catching up.
   `PeekBufferPositive` takes the newest sample without consuming; reading N and merging catches up at the cost
   of discarding intermediate edges. A tap shorter than the sampling period is lost by either snapshot path —
   that is the case `sceCtrlReadLatch()` exists for.
7. **Only one new press per frame is reported.** The loop at lines 493–501 breaks on the first newly pressed
   button in enum order; that matches raylib's single-value API, but a chord loses all but one button.
8. **No calibration path.** Centre, min and max are never sampled; the deadzone is not configurable from the
   game side; `128` is assumed for both axes, and the deadzone is axis-wise rather than radial.
9. **The read is unchecked and the struct is uninitialised.** `SceCtrlData pad;` (line 445) has no initializer
   and `sceCtrlReadBufferPositive()` (line 446) ignores the return. A failed or empty read leaves all buttons
   released and the axes at whatever was on the stack before. Zero the struct and branch on the return.
10. **Axis sign and direction are untested.** `Ly` runs top→bottom, matching raylib's SDL/GLFW convention
    (up = −1, down = +1), so the direct mapping is likely correct — but no test asserts it, and a sign error
    would be invisible for symmetric input.

## What a sprint must measure

- `sceCtrlReadBufferPositive()`'s return code per frame (0/negative/>1) and samples per read;
- queue lag: the sample's `TimeStamp` against the frame time;
- taps seen by `sceCtrlReadLatch` but missed by the snapshot, counted per run;
- time spent blocked inside `PollInputEvents()`;
- calibrated raw centre/min/max and reachable radius per axis.

These are the counters a sprint's §6 report needs; a functional pass with no numbers does not close the
measurement task.

## Candidate techniques — not recipes yet

Per [§13](../../memory-map.md#13-working-method-sprints-features-recipes) a recipe is written only after the
technique worked on hardware. These are the candidates the sources
suggest, each with the measurement that would promote it. Until then they belong here, not in `recipes/`.

| candidate | mechanism | promote when |
|---|---|---|
| Per-unit calibration sample | report raw `Lx`/`Ly` min/max/centre over a fixed window with the stick idle and at each limit; derive a radial centre, radius and scaling on this console | the derived deadzone removes drift without losing travel, and diagonals behave like cardinals |
| Latch-driven edges | call `sceCtrlReadLatch()` once per frame and translate `uiMake` into `lastButtonPressed`; catches taps shorter than a frame | a tap between two 2 fps bunnymark frames is reported with the latch and missed without it; note raylib's API reports one button per frame, so log the remaining `uiMake` bits |
| Non-blocking poll | `PeekBufferPositive` in `PollInputEvents()` instead of `ReadBufferPositive` | with vsync off, frame time no longer shows a ~16.7 ms floor or periodic blocking |
| Buffered catch-up | `ReadBufferPositive(pad, n)` with n up to 6; OR `Buttons` across the returned samples; count returns > 1 as a dropped-sample counter | slow-frame runs keep input responsive and the counter matches the missed frames |
| Vector magnitude clamp | if `sqrt(x²+y²) > 1`, scale both axes; two axes normalised by 128 independently can reach ~1.41, although the stick's reachable area is not a square | corner speed matches cardinal speed on a movement sample; the clamp is insurance regardless of the measured reachable radius |
| Idle-cancel threshold | `sceCtrlSetIdleCancelThreshold(0x40, 0x40)` (or `25, 25` as PGE uses), chosen from the calibrated range | a nub-only session longer than the system idle timeout does not dim or sleep |
| Interception/HOLD masking | while `PSP_CTRL_HOME` or `PSP_CTRL_HOLD` is set, freeze both snapshots and suppress edges, then resync `previousButtonState` on the first clean sample; letting `currentButtonState` drop to 0 during the frozen window emits a release edge when it clears | holding a button through the exit dialog produces no spurious `GetGamepadButtonPressed()` |
| Response curve | Sony's four-band angle/speed mapping (slow inside the deadzone, faster at the rim) | a game needs finer slow control than a linear axis gives; not a core requirement |
| Digital-only mode | `PSP_CTRL_MODE_DIGITAL` where the nub is unused, for the small power saving Sony notes | measured difference on battery, if anyone cares — otherwise keep analog mode always |

## Exit criteria

- [x] a sample drawing every button's live state, plus raw and normalised axis values
- [x] each control physically pressed and confirmed
- [x] `SceCtrlData` zeroed and the read's return validated; a failed or empty read is logged and moves no state
- [x] nub range recorded for **this** console, with a radial deadzone and scaling derived from it
- [x] `GetGamepadButtonPressed()` returns the pressed button for exactly one frame — the poll that saw the
      press — and 0 on the next, verified in the sample
- [x] a press that starts and ends between two slow frames is either reported or documented as lost
- [x] right axes read 0 when no right stick is present
- [x] stick magnitude never exceeds 1, including at a corner
- [x] axis directions confirmed: nub up gives `LEFT_Y < 0`, right gives `LEFT_X > 0`
- [x] `IsGamepadAvailable(0)`, `GetGamepadName(0)` and `GetGamepadAxisCount(0)` agree with what is filled
- [x] a nub-only session does not trigger the system idle timeout
- [x] HOME, HOLD, the PSPSDK-only bits and L1/R1/L2/R2/L3/R3 have a recorded decision each
- [x] keyboard/mouse emulation decision recorded, with its effect on Phase 13's tier-B samples
- [x] read-return, queue-lag and latch-vs-snapshot counters exist and are logged

## Sprints and features

All four sprints ran and closed on 2026-11-22; Phase 8 is done. The tier-B samples now know which path they can
rely on: the gamepad contract is stable, and keyboard/mouse emulation exists behind off-by-default switches.

| sprint | scope | state |
|---|---|---|
| [P8-S1 — 041 — Gamepad contract and the portable sample](../../sprints/041-phase8-gamepad-contract.md) | contract defaults, button map, edge window, portable sample | **done** — 5/5, device-validated |
| [P8-S2 — 042 — Sampling, edges and system states](../../sprints/042-phase8-sampling-and-edges.md) | poll arms, latch, HOLD/interception, counters | **done** — 6/6; peek+latch+freeze+100 Hz+stats |
| [P8-S3 — 043 — Nub calibration and response](../../sprints/043-phase8-nub-calibration.md) | raw range, radial deadzone, magnitude clamp, idle threshold | **done** — 5/5 resolved, T4 dropped; centre (133,112), dz 0.18, idle 0x38 |
| [P8-S4 — 044 — Decisions, compatibility and the Phase 8 close](../../sprints/044-phase8-decisions-and-close.md) | extra bits, keyboard/mouse, DS3, phase close | **done** — 5/5; keyboard/mouse adopted off by default, device-proven |

S1 must run first: it establishes the contract S2 and S3 measure. S4 is small if keyboard/mouse emulation is
rejected and shrinks to a close sprint; if it is adopted, T2 carries the example run and the `IsKeyPressed`
previous-state fix.
