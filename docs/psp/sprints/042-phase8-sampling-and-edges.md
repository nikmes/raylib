# Sprint 042 (P8-S2) — Sampling, edges and system states

| | |
|---|---|
| Roadmap phase | [Phase 8 — Input](../phases/08-input/index.md) |
| Sprint | P8-S2 |
| Branch / base commit | `master` @ the sprint 041 close (this doc is written before 041 runs) |
| Started / closed | — / — |
| Build number(s) | pspgu `b24` (unchanged); `libraylib.a` builds with `PSP_PAD_POLL`, `PSP_PAD_LATCH` and `PSP_PAD_FREEZE` |
| Tasks | 0/5 done |

## 0. Handover

**Situation.** Sprint 041 makes `CORE.Input.Gamepad` a complete contract: zeroed and validated read, right axes
0, triggers −1, a static button map and a one-frame `lastButtonPressed`. This sprint decides how that state is
sampled. The current call, `sceCtrlReadBufferPositive(&pad, 1)`, can block until the next VBLANK on a fast frame
and consumes only one queued sample on a slow frame; a tap shorter than the sampling period is lost. HOLD and
interception also turn held buttons into apparent releases, so a still-held button can look newly pressed when
input resumes.

**Read before acting**, in this order:

1. `../sprints/041-phase8-gamepad-contract.md` (P8-S1) §7 and §9 — the contract, the control arm and its numbers.
2. `../phases/08-input/index.md` — “Gaps” 3–6 and 9; the sampling, latch, HOLD and interception tasks.
3. `/home/psp/pspdev/psp/sdk/include/pspctrl.h:270-369` — `ReadBufferPositive`/`PeekBufferPositive` semantics and the
   latch fields (`uiMake`, `uiPress`, `uiBreak`, `uiRelease`).
4. `src/platforms/rcore_psp.c:408-501` — the poll order: save previous, reset `lastButtonPressed`, read, fill
   current, find the first new press.
5. `src/rcore.c:923` — `PollInputEvents()` is called once per frame from the main loop; there is no second reader.
6. `../memory-map.md` §5 Phase 8 and §6 — the phase's exit criterion and the measurement protocol.

**First action.** Add `FRAME_MS` and `TAP` modes to `psp-tests/pad_api`, plus a `PSP_PAD_POLL=read|peek|readn` switch
in `src/Makefile`. Run the three arms at `FRAME_MS=16`, `33` and `100`, with `TAP=1`, before changing the default.

**Hard rules.**

- **One reader.** `sceCtrlReadBuffer*`, `sceCtrlPeekBuffer*` and `sceCtrlReadLatch` are global resources. Only the
  thread that calls `PollInputEvents()` reads or consumes them; every other thread uses `sceCtrlPeekBuffer*`.
  `PeekBuffer*` does not consume, so it can run elsewhere only if the main reader still owns the queue.
- **A return value is evidence.** Count `ret < 0`, `ret == 0` and `ret > 1` separately. A return of 0 is a failed
  read, not “no input”; the last good snapshot stays visible, and the failure is logged once per transition.
- **HOLD and interception freeze both snapshots.** While `PSP_CTRL_HOME` or `PSP_CTRL_HOLD` is set, do not copy
  current to previous and do not update current from the suppressed sample. On the first clean sample, resync
  `previousButtonState = currentButtonState` and do not emit a press. Letting current drop to 0 during the frozen
  window would emit a release edge when it clears.
- **Latch edges report one button per frame.** `GetGamepadButtonPressed()` is a single value; translate the first
  `uiMake` bit in enum order and log the remaining bits. Do not pretend the API can return a chord.
- **`Peek` is not a substitute for the latch.** It gives the newest snapshot and never the tap that happened
  between polls. The latch is the only mechanism that can report a sub-frame tap.
- **No public API change.** `raylib.h` and the gamepad getters stay as they are. The switches are library build
  flags, not app flags, so one app binary can A/B the behaviour.
- **No renderer change.** The gate is byte-identical.
- **Stage only this sprint's paths.** `git status` before every commit.
- Commit messages carry the task ID: `sprint 042 T2: ...`.

**Commands.**

```sh
export PSPDEV=/home/psp/pspdev
export PATH=$PATH:$PSPDEV/bin

cd /home/psp/raylib-psp/src
make PLATFORM=PLATFORM_PSP -j8 PSP_PAD_POLL=read  PSP_PAD_LATCH=0 PSP_PAD_FREEZE=0
cp libraylib.a $PSPDEV/psp/lib/libraylib.a

cd ../psp-tests/pad_api
make clean && make FRAME_MS=100 TAP=1
mkdir -p /tmp/p8s2 && cp pad_api.prx /tmp/p8s2/app.prx
cd /tmp/p8s2 && timeout 20 pspsh -e "reset"; sleep 6
( printf './app.prx\n'; sleep 25 ) | timeout 45 pspsh

# Repeat for PSP_PAD_POLL=peek and =readn, then at FRAME_MS=33 and 16.
```

**Do not touch.** The contract defaults, button map and axis values from 041; calibration, radial deadzone and the
idle threshold (043); keyboard/mouse and DS3 (044); the renderer and pspgu.

**Done means** §8: the sampling arm is chosen by numbers, a sub-frame tap is either reported or counted as lost, no
spurious press survives HOLD/interception, and the counters say where the time went.

## 1. Objective

Four falsifiable results:

1. The three poll arms (`read`, `peek`, `readn`) are measured at 16, 33 and 100 ms frame intervals, with block time,
   queue lag and return codes recorded. The chosen arm is the one with no blocking on a fast frame and no unbounded
   lag on a slow one.
2. A tap shorter than the frame interval is reported by the latch or counted as lost by the snapshot; the
   `latchOnly` counter equals the observed taps the snapshot missed.
3. Holding a button through the HOME/exit dialog or the HOLD switch produces zero spurious
   `GetGamepadButtonPressed()` events, and the held button is still down when input returns.
4. The poll path has one reader, logs its counters, and leaves the public gamepad state unchanged from 041's
   contract.

## 2. Why now

Sprint 041 proved the contract; this sprint proves the delivery of samples into it. Without it, “input feels
laggy” cannot be attributed to the contract (041) or to the sampling (042), and the phase's exit criteria for a
sub-frame tap and the idle timer would be measured on a strategy that is about to change. The counters built here
are also what 043 uses to distinguish a calibration problem from a sampling problem.

## 3. Scope

**In:** `src/platforms/rcore_psp.c` poll path; `src/Makefile` (`PSP_PAD_POLL`, `PSP_PAD_LATCH`,
`PSP_PAD_FREEZE`); `psp-tests/pad_api` (`FRAME_MS`, `TAP`, counter display).

**Out:** calibration and deadzone (043); idle threshold (043); keyboard/mouse, DS3 and the extra-bit decisions
(044); the contract defaults from 041; the renderer.

## 4. Preconditions

- [ ] Sprint 041 closed; the portable sample passes its contract checks
- [ ] The control log (`PSP_PAD_POLL=read`, `PSP_PAD_LATCH=0`, `PSP_PAD_FREEZE=0`) is captured in §7
- [ ] `pad_api` can force a frame interval (`FRAME_MS`) and a short tap (`TAP`)
- [ ] Counters exist for `ret<0`, `ret==0`, `ret>1`, block time, queue lag, `latchOnly` and `spurious`
- [ ] Device loop available; `reset` leaves partition 2 at 25,149,440 bytes
- [ ] Sample builds with `PSP_LARGE_MEMORY=0`

## 5. Tasks

Status vocabulary: `todo` · `doing` · `done` · `blocked` · `dropped`.

| ID | task | status | verification | evidence |
|---|---|---|---|---|
| T1 | **Poll arms**: `PSP_PAD_POLL=read\|peek\|readn`; measure block time, return codes and queue lag at 16/33/100 ms | `todo` | table in §7; the adopted arm is named with its numbers; no unbounded lag | — |
| T2 | **Latch edges**: `PSP_PAD_LATCH=0\|1`; translate `uiMake` into `lastButtonPressed`, log remaining bits | `todo` | `TAP=1` at `FRAME_MS=100`: latch reports 1, snapshot 0, `latchOnly` increments once | — |
| T3 | **HOLD/interception**: `PSP_PAD_FREEZE=0\|1`; freeze both snapshots, suppress edges, resync on the first clean sample | `todo` | exit dialog/HOLD: spurious presses 0; held button still down on return; release fires once | — |
| T4 | **Counters and one-reader rule**: log the counters; audit that only `PollInputEvents()` reads/latches | `todo` | `PAD` log line at boot and every 300 frames; `grep` shows one reader | — |
| T5 | **Regression**: 041 sample, `basic_shapes`, `batch_check` gate | `todo` | contract values unchanged; gate hash unchanged; no renderer diff | — |

### T1 design

- `read`: `sceCtrlReadBufferPositive(&pad, 1)` — the current call. It blocks when the current sampling cycle was
  already read and consumes one queued sample otherwise.
- `peek`: `sceCtrlPeekBufferPositive(&pad, 1)` — newest sample, never blocks, never consumes. Taps between frames
  are still lost.
- `readn`: `SceCtrlData padN[6]; int n = sceCtrlReadBufferPositive(padN, 6);` — OR `Buttons` across the returned
  samples for the edge snapshot, use the newest `Lx`/`Ly` and `TimeStamp`. `n > 1` is a missed-cycle counter.
- Measure the call with `sceKernelGetSystemTimeWide()` around it and log microseconds. Queue lag is the delta
  between the newest sample's `TimeStamp` and the frame's own timestamp; log it in milliseconds.
- The default after the A/B is the arm that never blocks on a 16 ms frame and bounds the lag on the 100 ms frame.
  If `readn` wins, its buffer size and the merge rule are recorded; do not silently keep `read`.
- `pad_api`'s `FRAME_MS` uses `WaitTime()` after `EndDrawing()`, so the workload is deterministic rather than
  dependent on the renderer.

### T2 design

- `sceCtrlReadLatch(&latch)` once per frame, after the snapshot. `uiMake` is the set of buttons that transitioned
  to pressed since the previous read; the call clears the latch.
- Translate the first set bit in `GAMEPAD_BUTTON_*` enum order into `lastButtonPressed`, matching the snapshot
  path's single-value API. Log `latchMake=0x%08X` when more than one bit is set.
- `latchOnly` increments when a `uiMake` bit was not present in the snapshot's newly-pressed set. A tap shorter than
  the sampling period should produce exactly one such event at `FRAME_MS=100`.
- Do not let the latch path overwrite a snapshot press with a different button in the same frame; the snapshot is
  the current state, the latch is the missed history. Log both.

### T3 design

- Read first, then decide. If `(pad.Buttons & (PSP_CTRL_HOME | PSP_CTRL_HOLD))` is set, leave both
  `previousButtonState` and `currentButtonState` exactly as they are, clear `lastButtonPressed`, set a `frozen`
  flag and return. Do not copy the suppressed sample into either snapshot; the last good state stays visible.
- On the first sample with neither bit, set `previous = current` before the edge search and suppress that frame's
  press search. A button held through the window stays down with no edge; a button released during the window is
  lost by design. Log the frozen duration so the loss is visible, and note that the phase exit criterion only
  requires no spurious press.
- This means moving the “save previous” step after the read/HOLD check. Keeping the current order (save, read,
  then restore) also works, but it must restore the exact pre-poll snapshot, not a half-updated one.
- Test with the HOME/exit dialog and the physical HOLD switch as two separate runs; they set different bits.

### T4 design

- One `PAD` log line at boot and every 300 frames:
  `PAD poll=… ret0=… retneg=… retgt1=… blockus_max=… lagms_max=… latchOnly=… spurious=…`.
- The counters are read-only in the log; do not add a public API to inspect them.
- `grep -n "sceCtrlRead\|sceCtrlPeek\|sceCtrlReadLatch" src/platforms/rcore_psp.c` must show the reads inside
  `PollInputEvents()` and the latch beside them. Any other call site is a bug.
- If a second thread is added later (audio/loader), it must use `PeekBufferPositive`; this sprint documents the rule
  and does not create a thread.

### T5 design

- Re-run the 041 contract checks and `basic_shapes`; the input changes must not move a pixel.
- Run `batch_check` with `WRAP=0` against the archived gate; a mismatch is a build error, not a sampling effect.

### Task log (append-only)

- 2026-11-21 — sprint written; all tasks `todo`, no device run

## 6. Budgets

| item | budget | measured |
|---|---|---|
| poll call | ≤ 0.2 ms typical; no full sampling-period block for `peek` | |
| queue lag | ≤ 1 sample interval at 16 ms; bounded at 100 ms | |
| lost taps | 0 after the latch is enabled (or counted) | |
| spurious presses through HOLD/interception | 0 | |
| frame time | no renderer change; input cost only | |

## 7. Measurements

Protocol: device only; one variable per run; control → change → control again. Every row carries its build number.

| run | build | frame | poll | ret0/neg/>1 | block µs | queue lag ms | latchOnly | spurious |
|---|---|---|---|---|---|---|---|---|
| control | `read`, latch 0, freeze 0 | 16 ms | | | | | | |
| change | `peek` | 16 ms | | | | | | |
| change | `readn` | 16 ms | | | | | | |
| control | `read` | 100 ms | | | | | | |
| change | `peek` | 100 ms | | | | | | |
| change | `readn` | 100 ms | | | | | | |
| latch | chosen poll, `PSP_PAD_LATCH=1` | 100 ms + `TAP=1` | | | | | | |
| freeze | chosen poll, `PSP_PAD_FREEZE=1` | exit dialog / HOLD | | | | | | |

## 8. Exit criteria

- [ ] the three poll arms are measured at 16/33/100 ms; the adopted arm and its reason are recorded
- [ ] `peek` does not block; `read`'s block and lag are quantified; `readn`'s merge rule is documented
- [ ] a sub-frame tap is reported by the latch or counted as lost; `latchOnly` matches the observed taps
- [ ] HOLD and interception produce zero spurious `GetGamepadButtonPressed()` events
- [ ] the held button is still down when input returns, and its eventual release fires once
- [ ] `PAD` counters are logged at boot and every 300 frames
- [ ] only `PollInputEvents()` reads or latches the controller
- [ ] the 041 contract values and the renderer gate are unchanged
- [ ] no public header or renderer file appears in the sprint diff

## 9. Result

Not run yet.

## 10. Follow-ups

- **Sprint 043 (P8-S3)** — calibration, radial deadzone, magnitude clamp and the idle threshold, using this
  sprint's counters to separate sampling from calibration.
- **Sprint 044 (P8-S4)** — keyboard/mouse, DS3, extra bits and the phase close.
- If `readn` is adopted, a larger buffer than 6 is a separate experiment; do not change the buffer size without a
  table.
