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
- [ ] Fix edge detection: the 5.5 port resets `lastButtonPressed` but never sets it, so
      `GetGamepadButtonPressed()` never fires
- [ ] Calibrate the nub: sample raw min/max/centre per unit, make the deadzone configurable, do not assume 128
      is centre
- [ ] Decide and document what HOME, HOLD and NOTE do — system-owned, delivered, or ignored
- [ ] Decide whether to also fill `CORE.Input.Keyboard` from the pad (makes upstream examples run unmodified;
      redefines `IsKeyDown` on this platform) — [open question 5](../memory-map.md#12-open-questions)
- [ ] Choose the sampling strategy (`sceCtrlSetSamplingCycle`, once per frame vs fixed rate) and justify it
- [ ] Keep `SetGamepadVibration()` a documented no-op — there is no rumble

## What we already know

- The 5.5 port maps buttons to `GAMEPAD_BUTTON_*`, sets two axes, normalises raw 0–255 to −1…1 and applies a
  0.2 deadzone
- It sets `axisCount[1] = 2` with a comment about PSP Go / DualShock 3, but implements nothing for a second pad
- No analog triggers and no rumble exist on any PSP model — features relying on either must degrade visibly
- PPSSPP accepts synthetic input, so behaviour must be confirmed on the console

## Exit criteria

- [ ] a sample drawing every button's live state, plus raw and normalised axis values
- [ ] each control physically pressed and confirmed
- [ ] nub range recorded for **this** console, with the deadzone derived from it
- [ ] `GetGamepadButtonPressed()` returns the pressed button exactly once, verified in the sample

## Sprints and features

None yet. Use the [sprint template](../templates/sprint.md).
