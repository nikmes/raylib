# Sprint 042 (P8-S2) — Sampling, edges and system states

| | |
|---|---|
| Roadmap phase | [Phase 8 — Input](../phases/08-input/index.md) |
| Sprint | P8-S2 |
| Branch / base commit | `master` @ the sprint 041 close (`4d000e0e`, 2026-11-21) |
| Started / closed | — / — |
| Build number(s) | pspgu `b24` (unchanged); `libraylib.a` builds with `PSP_PAD_POLL`, `PSP_PAD_LATCH`, `PSP_PAD_FREEZE` and `PSP_PAD_CYCLE`; 041's `PSP_PAD_FAIL` is preserved on the read arm |
| Tasks | 0/6 done |

## 0. Handover

**Situation.** Sprint 041 closed the contract: zeroed and validated read, right axes 0, triggers −1, one static
button map, one-frame `lastButtonPressed`, strict/control device runs, held-button failure, and the T5 gate. This
sprint decides how that state is sampled. `sceCtrlReadBufferPositive(&pad, 1)` can block until the next VBLANK on
a fast frame and consumes only one queued sample on a slow frame; a tap shorter than the sampling period is lost.
HOLD and interception turn held buttons into apparent releases, so a still-held button can look newly pressed
when input resumes.

**Read before acting**, in this order:

1. `../sprints/041-phase8-gamepad-contract.md` (P8-S1) §7 and §9 — the device values, the failure hook and the T5
   gate.
2. `../phases/08-input/index.md` — “Gaps” 3–6 and 9; the sampling, latch, HOLD and interception tasks.
3. `/home/psp/pspdev/psp/sdk/include/pspctrl.h:250-369` — the read/peek/latch declarations. The header does **not**
   document the peek return value; do not infer it from the read's return.
4. `src/platforms/rcore_psp.c:417-560` — the current poll order: previous, reset, read, strict failure, map,
   axes, edge.
5. `src/rcore.c:923` — `PollInputEvents()` is called once per frame from the main loop.
6. `../memory-map.md` §5 Phase 8 and §6 — the phase criterion and the measurement protocol.

**First action.** Add `FRAME_MS`, `TAP` and `FRAMES` to `psp-tests/pad_api`, and a numeric
`PSP_PAD_POLL=read|peek|readn` mapping to `src/Makefile`. Run a **100 ms** probe with `read` and `peek` first,
logging the raw return value and the button state. That probe decides how `peek` success is detected before the
full matrix.

**Hard rules.**

- **One reader in the library.** `sceCtrlReadBuffer*`, `sceCtrlPeekBuffer*` and `sceCtrlReadLatch` are global
  resources. Only the thread that calls `PollInputEvents()` reads or consumes them; other code uses `Peek`.
  `psp-tests/basic_shapes` calls `sceCtrlReadBufferPositive` in its own `updateController`; it is a **known second
  reader** and must not be used as input evidence or run during the A/B. Its direct read is a Phase 13 cleanup.
- **Return semantics differ by arm.** `ReadBufferPositive` returns a sample count: success is `ret > 0`. The
  `PeekBufferPositive` return is not documented in the public header; measure it in the first probe. Do not apply
  the read's `ret <= 0` failure test to `peek`; a successful peek may return 0.
- **A failed read changes nothing.** On the failure predicate for the chosen arm, log once per transition and
  return before mapping, axes or edges. The last good snapshot stays visible. Do not apply the latch on a failed
  snapshot.
- **Clear the latch on failure or freeze.** `sceCtrlReadLatch()` is cumulative. On a failed or frozen sample, read
  and discard it so a make from that window cannot fire on recovery/resync. Never apply a stale latch.
- **HOLD and interception freeze both snapshots.** In the `PSP_PAD_FREEZE=1` build, read first; if the newest
  sample has `PSP_CTRL_HOME` or `PSP_CTRL_HOLD`, leave both snapshots untouched, clear `lastButtonPressed` and
  return. On the first clean sample, resync `previous=current` before the edge search and suppress that frame's
  press. Letting current drop to 0 during the disabled window emits a release edge when it clears.
- **The `PSP_PAD_FREEZE=0` control keeps the old order.** Save previous before the read, no freeze branch. Do not
  share one order for both arms or the A/B is not a control.
- **Latch edges report one button per frame.** `GetGamepadButtonPressed()` is single-value. Translate the first
  `uiMake` bit in enum order and log the remaining bits. A snapshot press takes priority over a latch press in
  the same frame; the latch is the missed history.
- **`Peek` is not a substitute for the latch.** It gives the newest snapshot, never the tap between polls. The
  latch is the only mechanism that can recover a sub-frame tap.
- **Preserve 041's failure hook.** `PSP_PAD_FAIL` is test-only and remains supported on the `PSP_PAD_POLL=read`
  arm (Cross or Square held for 60 consecutive polls). The `peek`/`readn` arms do not need to support it; T5
  reruns the held-button test with `read`.
- **Do not assume `TimeStamp` units.** The header does not document them. In the first run, log `pad.TimeStamp`
  with `sceKernelGetSystemTimeWide()` at the same poll; convert to ms only after the scale is confirmed.
- **Short arms.** `FRAME_MS=16|33|100` with `FRAMES=180|180|60`; a 100 ms arm at the sample's old 1800 frames
  would take three minutes. `TAP=1` only prompts/measures; it cannot inject a controller event.
- **Counters at boot and every 30 polls during the A/B** (300 in the shipped build), so a 60-frame arm still
  produces evidence. The last periodic line at or before `DONE` is the final row; the sample's `DONE` is
  separate.
- **Defaults stay on the control until §7 selects an arm.** `PSP_PAD_POLL=read`, `PSP_PAD_LATCH=0`,
  `PSP_PAD_FREEZE=0`, `PSP_PAD_CYCLE=0`; change only after the table is in hand.
- **No public API change and no renderer change.** The gate is byte-identical.
- **Stage only this sprint's paths.** `git status` before every commit.
- Commit messages carry the task ID: `sprint 042 T2: ...`.

**Commands.**

```sh
export PSPDEV=/home/psp/pspdev
export PATH=$PATH:$PSPDEV/bin
set -e

# usbhostfs_pc is already running with this directory as host0.
HOST0=/tmp/s3

# Relink the sample against the staged library, run, capture after DONE, reset.
run_arm() {
    (
        cd /home/psp/raylib-psp/psp-tests/pad_api
        make clean && make FRAME_MS=$2 FRAMES=$3
        cp pad_api.prx "$HOST0/app.prx"
    )
    (
        cd "$HOST0"
        timeout 20 pspsh -e "reset"; sleep 6
        ( printf './app.prx\n'; sleep 12 ) | timeout 25 pspsh
        pspsh -e "scrshot host0:/$1.raw"        # after DONE only
    )
}

# 1. Probe peek return semantics at 100 ms.
cd /home/psp/raylib-psp/src
make clean && make PLATFORM=PLATFORM_PSP -j8 PSP_PAD_POLL=read  PSP_PAD_LATCH=0 PSP_PAD_FREEZE=0 PSP_PAD_CYCLE=0
cp libraylib.a $PSPDEV/psp/lib/libraylib.a
run_arm s042-probe-read 100 60

make clean && make PLATFORM=PLATFORM_PSP -j8 PSP_PAD_POLL=peek  PSP_PAD_LATCH=0 PSP_PAD_FREEZE=0 PSP_PAD_CYCLE=0
cp libraylib.a $PSPDEV/psp/lib/libraylib.a
run_arm s042-probe-peek 100 60

# 2. Full matrix: read/peek/readn at 16/33/100 ms.
# run_arm <name> <frame_ms> <frames>: 180 for 16/33 ms, 60 for 100 ms.
```

**Do not touch.** The 041 contract defaults, button map and axis values; calibration, radial deadzone and the idle
threshold (043); keyboard/mouse and DS3 (044); the renderer and pspgu.

**Done means** §8: the sampling arm and cycle are chosen by numbers, a sub-frame tap is reported or counted, no
spurious press survives HOLD/interception, the counters say where the time went, and 041's failure hook and the
renderer gate still pass.

## 1. Objective

Five falsifiable results:

1. The three poll arms (`read`, `peek`, `readn`) are measured at 16, 33 and 100 ms with block time, return codes
   and queue lag. The adopted arm has no blocking on a fast frame and bounded lag on a slow one.
2. A tap shorter than the frame interval is reported by the latch or counted as lost; `latchOnly` matches the
   observed taps. With `readn`, a tap already present in the ORed snapshot is still reported; `latchOnly=0` is
   then valid.
3. Holding a button through HOME/exit or HOLD produces zero spurious `GetGamepadButtonPressed()` events in the
   fixed build; the control build shows the spurious press and the counters attribute it.
4. The poll path has one library reader, logs its counters, and leaves 041's contract and failure hook intact.
5. `PSP_PAD_CYCLE=0` (VBLANK) and `10000` (100 Hz) are compared at 16/100 ms; the chosen cycle is recorded.

## 2. Why now

Sprint 041 proved the contract; this sprint proves the delivery of samples into it. Without it, “input feels
laggy” cannot be attributed to the contract (041) or the sampling (042), and the phase's sub-frame tap and HOLD
criteria would be measured on a strategy about to change. The counters are also what 043 uses to distinguish a
calibration problem from a sampling problem. The sampling-cycle choice is a phase task; 042 closes it with a
measured decision rather than leaving it implicit.

## 3. Scope

**In:** `src/platforms/rcore_psp.c` poll path; `src/Makefile` (`PSP_PAD_POLL`, `PSP_PAD_LATCH`,
`PSP_PAD_FREEZE`, `PSP_PAD_CYCLE`); `psp-tests/pad_api` (`FRAME_MS`, `TAP`, `FRAMES`, counter display).

**Out:** calibration and deadzone (043); idle threshold (043); keyboard/mouse, DS3 and the extra-bit decisions
(044); the 041 contract defaults; the renderer. Removing `basic_shapes`' direct `sceCtrl` read belongs to Phase 13.

## 4. Preconditions

- [ ] Sprint 041 is closed (4d000e0e); its evidence and failure hook are archived
- [ ] The control log (`read`, latch 0, freeze 0, cycle 0) is captured in §7
- [ ] `pad_api` has `FRAME_MS`, `TAP` and short `FRAMES`
- [ ] `PSP_PAD_POLL` maps `read|peek|readn` to numeric defines
- [ ] Counters exist for `ret` per arm, block time, queue lag, `latchOnly`, `spurious`, `frozen`
- [ ] The first 100 ms probe confirms the peek return and the `TimeStamp` scale
- [ ] Device loop available; `reset` leaves partition 2 at 25,149,440 bytes
- [ ] Sample builds with `PSP_LARGE_MEMORY=0`

## 5. Tasks

Status vocabulary: `todo` · `doing` · `done` · `blocked` · `dropped`.

| ID | task | status | verification | evidence |
|---|---|---|---|---|
| T1 | **Poll arms**: `read\|peek\|readn` at 16/33/100 ms; a `read` vs `peek` arm with `PSP_DISABLE_VSYNC=1` at 16 ms; confirm peek return and `TimeStamp` units | `todo` | table in §7; adopted arm named with numbers; no unbounded lag | — |
| T2 | **Latch edges**: `PSP_PAD_LATCH=0\|1`; map `uiMake` into `lastButtonPressed`; `latchOnly`; test with `read` at 100 ms | `todo` | `TAP=1` at 100 ms: snapshot miss plus latch report; `latchOnly` matches; `readn` variant recorded | — |
| T3 | **HOLD/interception**: `PSP_PAD_FREEZE=0\|1`; read-first order in `=1`; freeze/resync/suppress; `spurious` | `todo` | HOME/exit and HOLD runs: fixed spurious 0; control >0; held button stays down | — |
| T4 | **Counters and one-reader rule**: `PAD` line at boot/every 30 polls; per-arm return counters; one library reader | `todo` | counters present in a 60-frame arm; `grep` shows one reader; `basic_shapes` exception recorded | — |
| T5 | **Regression**: 041 contract, `PSP_PAD_FAIL` held-button run, `basic_shapes`, `batch_check` gate | `todo` | contract values unchanged; failure hook still logs/retains; gate hash unchanged | — |
| T6 | **Cycle decision**: `PSP_PAD_CYCLE=0\|10000`, adopted poll, 16/100 ms | `todo` | block/lag table; chosen cycle recorded; no queue unbounded | — |

### T1 design

- Makefile mapping (numeric defines; strings cannot be compared by the preprocessor):
  ```make
  PSP_PAD_POLL ?= read
  ifeq ($(PSP_PAD_POLL),peek)
      CUSTOM_CFLAGS += -DPSP_PAD_POLL=1
  else ifeq ($(PSP_PAD_POLL),readn)
      CUSTOM_CFLAGS += -DPSP_PAD_POLL=2
  else
      CUSTOM_CFLAGS += -DPSP_PAD_POLL=0
  endif
  PSP_PAD_LATCH ?= 0
  CUSTOM_CFLAGS += -DPSP_PAD_LATCH=$(PSP_PAD_LATCH)
  PSP_PAD_FREEZE ?= 0
  CUSTOM_CFLAGS += -DPSP_PAD_FREEZE=$(PSP_PAD_FREEZE)
  PSP_PAD_CYCLE ?= 0
  CUSTOM_CFLAGS += -DPSP_PAD_CYCLE=$(PSP_PAD_CYCLE)
  ```
- Add `#ifndef` fallbacks in `rcore_psp.c` (`PSP_PAD_POLL 0`, `PSP_PAD_LATCH 0`, `PSP_PAD_FREEZE 0`,
  `PSP_PAD_CYCLE 0`) so a direct compile still works, as 041 did for strict/fail.
- `read`: `int n = sceCtrlReadBufferPositive(&pad, 1);` success `n > 0`; 0/negative is failure.
- `peek`: `int ret = sceCtrlPeekBufferPositive(&pad, 1);` do **not** treat 0 as failure until the probe says
  otherwise. Log `ret` and `pad.Buttons`; the header does not document the return, and PGE's emulator returns 0
  for both calls, so it is not evidence.
- `readn`: `SceCtrlData padN[6]; int n = sceCtrlReadBufferPositive(padN, 6);` success `n > 0`; newest sample is
  `padN[n-1]`; OR `Buttons` across `0..n-1` for edges; newest axes and `TimeStamp`; `n > 1` is a missed-cycle
  counter. Check HOME/HOLD on the **newest** sample, not the ORed mask.
- `PSP_DISABLE_VSYNC=1` with `read` at 16 ms is the blocking probe; `peek` is the non-blocking comparison.
  `FRAME_MS` targets the interval: measure `GetTime()` around the frame, then `WaitTime(target - elapsed)`.
- `pad_api` `FRAME_MS`/`FRAMES`: 180 frames at 16/33 ms, 60 at 100 ms; `TAP=1` only prompts the operator to tap
  CROSS once between frames. The sample logs `PAD state` every 60 frames and `PAD pressed` on a change.
- `TimeStamp`: log raw `pad.TimeStamp` and `sceKernelGetSystemTimeWide()` in the 100 ms probe. Convert to ms only
  after the scale is confirmed; otherwise report raw ticks.
- `PSP_PAD_FAIL`: keep the 041 hook on the `read` arm; it increments on 60 consecutive CROSS/Square polls and sets
  the failure. Do not apply the read hook to `peek`.

### T2 design

- Read `sceCtrlReadLatch(&latch)` once per frame, after a successful snapshot. `uiMake` is cumulative since the
  previous latch read; the call clears it.
- Map `uiMake` through the same `pspButtonMap` table. Take the first set mapped bit in `GAMEPAD_BUTTON_*` order;
  log `latchMake=0x%08X` and the unmapped bits.
- Priority: if the snapshot found a new press, keep it; otherwise use the latch. For each latch bit not in the
  snapshot's newly-pressed set, increment `latchOnly` once per bit. The 100 ms test expects one.
- On a failed snapshot, return before applying the latch; read and discard it so it cannot fire on recovery. On a
  frozen sample, do the same: discard the latch, never apply it on the first clean sample.
- Test with `PSP_PAD_POLL=read` at 100 ms so the tap can fall between snapshots. With `readn`, the ORed snapshot
  may already contain the tap; record `latchOnly=0` as a valid result, not a failure.
- A chord still reports one `lastButtonPressed`; log all `uiMake` bits so the loss is visible.

### T3 design

- `PSP_PAD_FREEZE=1`: read first, then check the newest sample:
  ```c
  if ((pad.Buttons & (PSP_CTRL_HOME | PSP_CTRL_HOLD)) != 0)
  {
      CORE.Input.Gamepad.lastButtonPressed = 0;
      pspFrozen = 1; pspFrozenFrames++;
      return;                          // snapshots untouched
  }
  if (pspFrozen) { pspResync = 1; pspFrozen = 0; }
  // save previous, reset last, map current, axes
  if (pspResync)
  {
      memcpy(previous, current, ...);  // resync
      pspResync = 0;
      CORE.Input.Gamepad.lastButtonPressed = 0;
  }
  else { /* normal edge search */ }
  ```
  A button held through the window stays down with no edge; a release during the window is lost by design; log
  the frozen duration.
- `PSP_PAD_FREEZE=0` keeps the current save-before-read order and no freeze branch. Track `suppressedPrev` from
  the previous sample in both arms: increment `spurious` when a press is found while `suppressedPrev` is true.
  The fixed build suppresses the press, so its `spurious` is 0; the control build should show >0.
- Test sequences:
  1. Hold CROSS, press HOME (or open the exit dialog), return, release. Fixed: no new `PAD pressed`; control:
     one spurious.
  2. Hold CROSS, flip HOLD on and off, release. Same expectation.
- The sample runs 1800 frames; enough for both sequences. The log carries the frozen frame count.

### T4 design

- `PAD` line at boot and every 30 polls during the A/B:
  `PAD poll=… latch=… freeze=… cycle=… ret0=… retneg=… retgt1=… blockus_max=… lagms_max=… latchOnly=… spurious=… frozen=…`.
- `ret0` is a failure for `read`/`readn`; for `peek` it is a success if the probe confirmed it. Log the raw
  return so the interpretation is visible.
- `blockus_max` uses `sceKernelGetSystemTimeWide()` around the call. `lagms_max` is the newest sample's
  `TimeStamp` delta once its unit is confirmed; otherwise log raw ticks.
- `grep -n "sceCtrlRead\\|sceCtrlPeek\\|sceCtrlReadLatch" src/platforms/rcore_psp.c` must show the reads inside
  `PollInputEvents()` and the latch beside them. Any other library call site is a bug.
- `psp-tests/basic_shapes` is the known second reader; do not run it during the A/B and do not count it as a
  library pass. Phase 13 owns removing its direct read.
- If a second thread is added later, it must use `PeekBufferPositive`; this sprint documents the rule and does
  not create a thread.

### T5 design

- Re-run the 041 contract checks: `pad_api` at rest, strict `=1` and control `=0` values (axes 2/0, right 0/−1,
  triggers −1/0).
- Rebuild `PSP_PAD_POLL=read PSP_PAD_FAIL=1`, hold CROSS or SQUARE until the read fails; one `PAD read failed`,
  the held button stays down, and a release recovers. Restore `PSP_PAD_FAIL=0`.
- `basic_shapes` is a render regression only; note its direct `sceCtrl` read.
- `batch_check WRAP=0` must stay byte-identical to `par_pspgl.raw` (`b69830b8…`).
- No public header or renderer file in the sprint diff.

### T6 design

- `PSP_PAD_CYCLE=0` is VBLANK sampling (the current `InitPlatform` value). `10000` is 100 Hz; the valid
  asynchronous range is 5555–20000 µs. Set `sceCtrlSetSamplingCycle(PSP_PAD_CYCLE)` in `InitPlatform` instead of
  the hard-coded 0.
- Run the adopted poll arm at 16 and 100 ms with both cycles; record block time, queue lag and `retgt1`. Choose 0
  unless the fixed rate removes an unbounded queue without adding block; a null result keeps 0 and records why.
- Keep the default on 0 until the table is in hand. If 0 wins, fixed-rate remains a follow-up for a vsync-off
  build, not a hidden default.

### Task log (append-only)

- 2026-11-21 — sprint rewritten for implementation readiness; all tasks `todo`, no device run

## 6. Budgets

| item | budget | measured |
|---|---|---|
| poll call | ≤ 0.2 ms typical; `peek` no sampling-period block | |
| queue lag | ≤ 1 sample interval at 16 ms; bounded at 100 ms | |
| lost taps | 0 after the latch is enabled (or counted) | |
| spurious presses through HOLD/interception | 0 in the fixed build | |
| frame time | no renderer change; input cost only | |
| cycle | choose by table, not preference | |

## 7. Measurements

Protocol: device only; one variable per run; control → change → control again. Every row carries its build number.

| run | build | frame | poll | ret0/neg/>1 | block µs | queue lag ms | latchOnly | spurious |
|---|---|---|---|---|---|---|---|---|
| control | `read`, latch 0, freeze 0, cycle 0 | 16 ms | read | | | | | |
| change | `peek` | 16 ms | peek | | | | | |
| change | `readn` | 16 ms | readn | | | | | |
| control | `read` | 100 ms | read | | | | | |
| change | `peek` | 100 ms | peek | | | | | |
| change | `readn` | 100 ms | readn | | | | | |
| vsync off | `read`, `PSP_DISABLE_VSYNC=1` | 16 ms | read | | | | | |
| vsync off | `peek`, `PSP_DISABLE_VSYNC=1` | 16 ms | peek | | | | | |
| latch | `read`, `PSP_PAD_LATCH=1` | 100 ms + `TAP=1` | read | | | | | |
| freeze control | `read`, `PSP_PAD_FREEZE=0` | 1800 frames, HOME/HOLD | read | | | | | |
| freeze fixed | `read`, `PSP_PAD_FREEZE=1` | 1800 frames, HOME/HOLD | read | | | | | |
| cycle 0 | adopted poll, `PSP_PAD_CYCLE=0` | 16/100 ms | | | | | | |
| cycle 100 Hz | adopted poll, `PSP_PAD_CYCLE=10000` | 16/100 ms | | | | | | |

## 8. Exit criteria

- [ ] the three poll arms are measured at 16/33/100 ms; the adopted arm and reason are recorded
- [ ] the peek return and `TimeStamp` scale are confirmed by the first probe, not assumed
- [ ] `peek` does not block; `read`'s block is quantified; `readn`'s merge rule is documented
- [ ] a sub-frame tap is reported by the latch or counted as lost; `latchOnly` matches the observed taps
- [ ] `readn`'s ORed snapshot case is recorded (`latchOnly=0` is valid when the tap was already captured)
- [ ] HOLD and interception produce zero spurious presses in the fixed build; the control build shows the cause
- [ ] the held button is still down when input returns, and its eventual release fires once
- [ ] `PAD` counters are logged at boot and every 30 polls during the A/B
- [ ] only `PollInputEvents()` reads or latches the library controller; `basic_shapes` is recorded as a known
      sample exception
- [ ] 041's contract values and `PSP_PAD_FAIL` held-button behavior are unchanged
- [ ] the renderer gate and `basic_shapes` are unchanged
- [ ] `PSP_PAD_CYCLE=0` vs `10000` is measured and the chosen cycle recorded
- [ ] no public header or renderer file appears in the sprint diff

## 9. Result

Not run yet.

## 10. Follow-ups

- **Sprint 043 (P8-S3)** — calibration, radial deadzone, magnitude clamp and the idle threshold, using these
  counters to separate sampling from calibration. The 041 at-rest `LY=-0.21` is 043's first case.
- **Sprint 044 (P8-S4)** — keyboard/mouse, DS3, extra bits and the phase close.
- Remove `basic_shapes`' direct `sceCtrl` read in Phase 13; it is a second reader.
- If `readn` is adopted, a buffer larger than 6 is a separate experiment; do not change it without a table.
- If cycle 0 wins, fixed-rate sampling remains a follow-up for a `PSP_DISABLE_VSYNC` build.
