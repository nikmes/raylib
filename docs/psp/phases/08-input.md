# Phase 8 — Input

| | |
|---|---|
| State | not started |
| Depends on | Phase 1 (a running build to press buttons in) — independent of the renderer phases |
| Exit criterion | every control reachable through raylib's API, `GetGamepadButtonPressed()` fires exactly once per press, and the nub's measured range is recorded per console |

## What this phase is about

Making the device's controls usable through raylib's normal input calls, with a nub that behaves predictably.
Input is not a separate subsystem in raylib: `IsKeyDown`, `IsGamepadButtonDown` and `GetGamepadAxisMovement`
read `CORE.Input` and are platform-agnostic. A platform only has to fill that struct in `PollInputEvents()` —
which is where all of this work lands, in the platform file, not in `rcore.c`.

## What has to be true before starting

- [ ] A build that renders something and can display live input state
- [ ] The 5.5 port's `PollInputEvents()` available as the starting point (it already polls `sceCtrl`)

## Tasks

- [ ] Verify the button mapping one by one: CROSS, CIRCLE, SQUARE, TRIANGLE, D-pad, L/R shoulders, SELECT, START
- [ ] Verify edge detection on device. The imported port **does** set `lastButtonPressed` on a new press
      (`src/platforms/rcore_psp.c:497`), so the 5.5-era "never fires" task is stale — but a press that begins
      and ends between two frames is still lost. Decide whether snapshot polling is enough or latch data is
      needed
- [ ] Fix the right stick: `Rx`/`Ry` are not part of the documented PSP-1000 contract, yet the port derives
      `GAMEPAD_AXIS_RIGHT_*` from them (`rcore_psp.c:483`). Stop reporting a right stick unless one is present
- [ ] Calibrate the nub: sample raw min/max/centre per unit, derive the deadzone from it, do not assume 128 is
      centre or that one unit's deadzone fits another
- [ ] Clamp the stick vector's magnitude to 1: two independent axes normalised by 128 can reach ~1.41 at a
      corner
- [ ] Decide and document what HOME, HOLD and the extra bits do. `PSP_CTRL_HOME` (bit 16) is the same bit Sony
      calls `SCE_CTRL_INTERCEPTED`; it is not a button press
- [ ] Decide `sceCtrlSetIdleCancelThreshold`. Its default of −1 means nub movement does not reset the system
      idle timer
- [ ] Choose the sampling strategy (`sceCtrlSetSamplingCycle`, once per frame vs fixed rate) and justify it;
      document that `ReadBuffer*` blocks and `PeekBuffer*` does not
- [ ] Decide whether to also fill `CORE.Input.Keyboard` from the pad (makes upstream examples run unmodified;
      redefines `IsKeyDown` on this platform) — [open question 5](../memory-map.md#12-open-questions)
- [ ] Document the one-reader rule: `sceCtrlReadBuffer*` and `sceCtrlReadLatch` are global resources, so one
      thread reads them and every other thread peeks
- [ ] Keep `SetGamepadVibration()` a documented no-op — there is no rumble

## What we already know

- The 5.5 port maps buttons to `GAMEPAD_BUTTON_*`, sets two axes, normalises raw 0–255 to −1…1 and applies a
  0.2 deadzone
- It sets `axisCount[1] = 2` with a comment about PSP Go / DualShock 3, but implements nothing for a second pad
  (`ready[1]` is never set, so raylib does not expose it)
- The same file reads the right analog axes from `pad.Rsrv[0]`/`Rsrv[1]`, which alias `Rx`/`Ry`. On a PSP-1000
  those bytes are outside the documented contract, so the axes are expected to sit at a constant value —
  almost certainly `(0 − 128) / 128 = −1` on both — and that value *is* exposed through
  `GetGamepadAxisMovement(0, GAMEPAD_AXIS_RIGHT_*)`
- No analog triggers and no rumble exist on any PSP model — features relying on either must degrade visibly
- PPSSPP accepts synthetic input, so behaviour must be confirmed on the console

## Controller-service facts

Sources: the public PSPSDK header `pspctrl.h`, and Sony's Controller Overview / Reference in the 6.6 SDK tree
(`/home/psp/psp_sdk_660/.../document/html/peripheral/`). The Sony tree is a **reading aid only**
([§11.4](../memory-map.md#the-sony-sdk-what-it-is-for-and-what-it-is-not)): a
behaviour used in code must be reproduced from the public header or measured here, not copied from that tree.

| fact | value | tag |
|---|---|---|
| Sampling cycle 0 | sample every VBLANK period; the default at start-up | `[external: Sony Controller Reference]` |
| Sampling cycle 5555–20000 | asynchronous sampling, period in **microseconds** (~180–50 Hz); values between 1 and 5554 are invalid | `[external: same]` |
| `sceCtrlReadBufferPositive()` | blocking if called again before the current sample exists; returns the number of samples read | `[public: pspctrl.h]` |
| Buffer depth | 64 samples; a return value **> 1** means frames were missed and history can be merged | `[external: Sony Controller Overview]` |
| `sceCtrlPeekBufferPositive()` | reads the latest sample without consuming the buffer and does not block | `[external: same]` |
| `sceCtrlReadLatch()` | cumulative make/press/break/release since the last read; clears on read; can bridge more than 64 cycles | `[external: Sony Controller Reference]` |
| Buffer/latch ownership | global system resources; only one thread may read them, others must peek | `[external: Sony Controller Overview]` |
| Default sampling mode | digital buttons only; analog values are valid only in `PSP_CTRL_MODE_ANALOG` (= 1) | `[public: pspctrl.h]` |
| Analog range | `Lx`/`Ly` are 0–255, centre 0x80, horizontal left→right and vertical **top→bottom** | `[external: Sony Controller Overview]` |
| Centre tolerance | error up to about ±0x40, so neutral cannot be assumed tighter than 0x40–0xC0 | `[external: same]` |
| Full scale | each axis reaches 0x00/0xFF over its own travel, but **not guaranteed simultaneously** — the stick's reachable area is not a square | `[external: same]` |
| Idle timer threshold | `sceCtrlSetIdleCancelThreshold(unhold, hold)`: −1 = analog never cancels idle (default), 0 = idle disabled, 1–128 = movement required | `[external: Sony Controller Reference]` |
| HOLD switch | while set, every digital button reads released and `PSP_CTRL_HOLD` (bit 17) is set | `[external: Sony Controller Overview]` |
| Interception | `PSP_CTRL_HOME` (bit 16) is Sony's `SCE_CTRL_INTERCEPTED`; the PS button itself is not visible to applications, and analog values continue during interception | `[public: pspctrl.h]` + `[external: Sony Controller Overview]` |
| Analog-during-HOLD | Sony says sampling continues, but the wording about `Lx`/`Ly` is ambiguous — treat as unverified | `[external: same]` |
| DS3 on PSP Go | Sony 6.3 says the wireless controller's right stick, L3/R3 and vibration are **not available**; PSPSDK's header says `Rx`/`Ry` are valid with a DS3 on a PSP Go. The sources conflict — verify before relying on either | `[external: same]` + `[public: pspctrl.h]` |

The extra PSPSDK bits — `PSP_CTRL_NOTE`, `SCREEN`, `VOLUP`, `VOLDOWN`, `WLAN_UP`, `REMOTE`, `DISC`, `MS` — do
not appear in Sony's user-mode header. Treat them as undelivered until a device sample proves otherwise.
`[public: pspctrl.h]`

## What the reference implementations do

All rows are code scans, not measurements on our console. Their licences are in
[§7](../memory-map.md#7-reference-sources-read-only-outside-this-repo).

| source | poll | deadzone | edges | notable |
|---|---|---|---|---|
| Sony `sample/graphics/stick` | `ReadBufferPositive`, once per frame | none (`(Lx-128)*0.01`) | `padd = cont & (cont ^ prev)` | START/SELECT on edge; axis scaled to ±1.28 |
| Sony `sample/tips/analog_ctrl` | `ReadBufferPositive`, once per frame | **0x40–0xC0** | `padd = cont & (cont ^ prev)` | idle threshold `(0x40, -1)`; a four-band angle/speed response curve |
| `openTRI` `triInput.c` | `PeekBufferPositive` | 103 < Lx/Ly < 151 (≈ ±24) | `Pressed`/`Held` bitsets; `triInputPressed()` clears the bit it returns | sampling cycle 0; all 12 buttons |
| `PGE` `pgeControls.c` | `PeekBufferPositive` | none in the axis maths | `pressed`/`held`/`released` bitsets | returns early on `PSP_CTRL_HOME`; idle threshold `(25, 25)`; magnitude clamped to 1; angle/magnitude helpers |
| `PSPRuntime` `inputmanager.cpp` | `ReadBufferPositive` | 28 raw counts, configurable | previous/current snapshot | centre hardcoded 128; `GetKeyPressed` fixed priority order |
| 5.5/6.2 port (`rcore_psp.c`) | `ReadBufferPositive` | 0.2 normalised (≈ ±25.6 raw) | snapshot, sets `lastButtonPressed` | right axes from `Rx`/`Ry`; no HOLD/INTERCEPTED/idle handling |
| `3D-PSP-Tutorials` (no licence) | `PeekBufferPositive` | none | none | raw −128; ideas only, never copy |

The spread is the finding: three sources use `Peek`, three use `Read`; deadzones range from 0 to 0x40 raw; only
PGE clamps the vector magnitude; only Sony's sample sets the idle threshold deliberately.

## Gaps in the current platform file

Line references are `src/platforms/rcore_psp.c` at `37585003`.

1. **Right stick is fabricated.** `Rx`/`Ry` are read at lines 483–484 and exposed for gamepad 0. On our
   1000 the expected value is a constant −1 on both axes. `axisCount[1] = 2` (line 468) advertises a second
   pad that is never marked ready.
2. **The deadzone is tighter than Sony's own tolerance.** 0.2 normalised is ±25.6 raw counts, while the
   documented centre error is up to ±0x40. A unit at the edge of tolerance will drift with the stick untouched;
   a unit at nominal centre loses usable travel under a 0x40 deadzone. One constant cannot serve both, which is
   why calibration is a task, not a number.
3. **Interception is not handled.** During the system pop-up, input is not reported and `PSP_CTRL_HOME` is set.
   The snapshot comparison then sees releases; when input resumes, a still-held button looks newly pressed and
   `GetGamepadButtonPressed()` fires spuriously.
4. **HOLD is not handled.** Digital buttons read released while the switch is on. That is harmless for most
   games, but it should be a documented decision, and the analog-during-HOLD question needs a device check.
5. **The idle timer is left at −1.** A game played only with the nub never resets the system idle timer.
6. **`ReadBufferPositive` can block and can lose taps.** At default sampling it matches VSYNC; with vsync off,
   or on a frame slower than the sampling period, it either blocks or returns several samples of which only
   the newest is used. A tap fully between two slow frames disappears. `ReadLatch` exists for exactly this.
7. **Only one new press per frame is reported.** The loop at lines 493–501 breaks on the first newly pressed
   button in enum order; that matches raylib's single-value API, but a chord loses all but one button.
8. **No calibration path.** Centre, min and max are never sampled; the deadzone is not configurable from the
   game side; `128` is assumed for both axes.

## Candidate techniques — not recipes yet

Per [§13](../memory-map.md#13-working-method-sprints-features-recipes) a recipe is written only after the
technique worked on hardware. These are the candidates the sources
suggest, each with the measurement that would promote it. Until then they belong here, not in `recipes/`.

| candidate | mechanism | promote when |
|---|---|---|
| Per-unit calibration sample | report raw `Lx`/`Ly` min/max/centre over a fixed window with the stick idle and at each limit; derive centre and deadzone per console | two consoles give stable, different numbers and the derived deadzone removes drift without losing travel |
| Latch-driven edges | call `sceCtrlReadLatch()` once per frame and translate `uiMake` into `lastButtonPressed`; catches taps shorter than a frame | a tap between two 2 fps bunnymark frames is reported with the latch and missed without it |
| Non-blocking poll | `PeekBufferPositive` in `PollInputEvents()` instead of `ReadBufferPositive` | with vsync off, frame time no longer shows a ~16.7 ms floor or periodic blocking |
| Buffered catch-up | `ReadBufferPositive(pad, n)` with n up to 6; OR `Buttons` across the returned samples; count returns > 1 as a dropped-sample counter | slow-frame runs keep input responsive and the counter matches the missed frames |
| Vector magnitude clamp | if `sqrt(x²+y²) > 1`, scale both axes; normalising two axes by 128 independently reaches ~1.41 at a corner | corner speed matches cardinal speed on a movement sample |
| Idle-cancel threshold | `sceCtrlSetIdleCancelThreshold(0x40, 0x40)` (or `25, 25` as PGE uses), chosen from the calibrated range | a nub-only session longer than the system idle timeout does not dim or sleep |
| Interception/HOLD masking | when `PSP_CTRL_HOME` or `PSP_CTRL_HOLD` is set, treat the frame as "no change" and resync `previousButtonState` on the next real sample | holding a button through the exit dialog produces no spurious `GetGamepadButtonPressed()` |
| Response curve | Sony's four-band angle/speed mapping (slow inside the deadzone, faster at the rim) | a game needs finer slow control than a linear axis gives; not a core requirement |
| Digital-only mode | `PSP_CTRL_MODE_DIGITAL` where the nub is unused, for the small power saving Sony notes | measured difference on battery, if anyone cares — otherwise keep analog mode always |

## Exit criteria

- [ ] a sample drawing every button's live state, plus raw and normalised axis values
- [ ] each control physically pressed and confirmed
- [ ] nub range recorded for **this** console, with the deadzone derived from it
- [ ] `GetGamepadButtonPressed()` returns the pressed button exactly once, verified in the sample
- [ ] a press that starts and ends between two slow frames is either reported or documented as lost
- [ ] right axes read 0 when no right stick is present
- [ ] stick magnitude never exceeds 1, including at a corner
- [ ] a nub-only session does not trigger the system idle timeout
- [ ] HOME, HOLD and the undocumented bits have a recorded decision each

## Sprints and features

None yet. Use the [sprint template](../templates/sprint.md).
