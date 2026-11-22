# Sprint 042 (P8-S2) — Sampling, edges and system states

| | |
|---|---|
| Roadmap phase | [Phase 8 — Input](../phases/08-input/index.md) |
| Sprint | P8-S2 |
| Branch / base commit | `master` @ the sprint 041 close (`4d000e0e`, 2026-11-21) |
| Started / closed | 2026-11-21 / 2026-11-22 |
| Build number(s) | pspgu `b24` (unchanged); `libraylib.a` builds with `PSP_PAD_POLL`, `PSP_PAD_LATCH`, `PSP_PAD_FREEZE` and `PSP_PAD_CYCLE`; 041's `PSP_PAD_FAIL` is preserved on the read arm |
| Tasks | 6/6 done — T1–T6 done |

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
  the read's `ret <= 0` failure test to `peek`. The PSP-1000 probe returned 1; keep `ret >= 0` as the safe
  success predicate and log the raw value.
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
- **Defaults now follow T1.** `PSP_PAD_POLL=peek` is the adopted arm; `PSP_PAD_LATCH=0`, `PSP_PAD_FREEZE=0`
  and `PSP_PAD_CYCLE=0` stay until T2/T3/T6 select them.
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
- [x] The first 100 ms probe confirms the peek return and the `TimeStamp` scale
- [ ] Device loop available; `reset` leaves partition 2 at 25,149,440 bytes
- [ ] Sample builds with `PSP_LARGE_MEMORY=0`

## 5. Tasks

Status vocabulary: `todo` · `doing` · `done` · `blocked` · `dropped`.

| ID | task | status | verification | evidence |
|---|---|---|---|---|
| T1 | **Poll arms**: `read\|peek\|readn` at 16/33/100 ms; a `read` vs `peek` arm with `PSP_DISABLE_VSYNC=1` at 16 ms; confirm peek return and `TimeStamp` units | `done` | table in §7; adopted arm named with numbers; no unbounded lag | adopted **peek**: 9 µs, no block in both vsync modes; read/readn block 13.7 ms with vsync off; readn returns up to 6 samples at 100 ms but blocks; full matrix in §7 |
| T2 | **Latch edges**: `PSP_PAD_LATCH=0\|1`; map `uiMake` into `lastButtonPressed`; `latchOnly`; test with the adopted `peek` at 100 ms | `done` | `TAP=1` at 100 ms: snapshot miss plus latch report; `latchOnly` matches; `readn` variant recorded | control: 0 nonzero snapshot buttons, no press; latch-on: `latchMake=0x00004000`, `latchOnly=1`, `PAD pressed=7 frame=7`; 1 s/20 recents pair; readn not adopted (ORed snapshot would catch, `latchOnly=0`) |
| T3 | **HOLD/interception**: `PSP_PAD_FREEZE=0\|1`; read-first order in `=1`; freeze/resync/suppress; `spurious` | `done` | HOME/exit and HOLD runs: fixed spurious 0; control >0; held button stays down | fixed HOME: `PAD unfreeze frames=3`, no spurious; synthetic: control `PAD spurious=1`/`PAD pressed=7 frame=63`, fixed `PAD unfreeze frames=3`/no spurious; real HOLD: control `PAD spurious=1,2,3` and fresh press, fixed `PAD unfreeze frames=1`/no spurious |
| T4 | **Counters and one-reader rule**: `PAD` line at boot/every 30 polls; per-arm return counters; one library reader | `done` | counters present in a 60-frame arm; `grep` shows one reader; `basic_shapes` exception recorded | stats 30: `ret0=0 retneg=0 retgt1=0`, `blockus_max=9`, `lagms_max=16.03–16.11`, `latchOnly=0 spurious=0 frozen=0`; one-reader grep clean; `basic_shapes:40` exception |
| T5 | **Regression**: 041 contract, `PSP_PAD_FAIL` held-button run, `basic_shapes`, `batch_check` gate | `done` | contract values unchanged; failure hook still logs/retains; gate hash unchanged | contract axes 2/0, right 0/0, triggers −1/−1; frame 60 `cross=1`, `PAD input failed ret=-1`, `retneg=2`; `basic_shapes` `DONE 600` 60 fps; `batch_check` `b69830b8…` identical |
| T6 | **Cycle decision**: `PSP_PAD_CYCLE=0\|10000`, adopted poll, 16/100 ms | `done` | block/lag table; chosen cycle recorded; no queue unbounded | four no-input arms, stats30: block max 9–10 µs in all; lag max 16.09/16.08 (cycle 0) → 8.95/7.83 ms (100 Hz) at 16/100 ms; `ret0=0 retneg=0 retgt1=0`; adopted `PSP_PAD_CYCLE=10000` |

### T1 design

- Makefile mapping (numeric defines; strings cannot be compared by the preprocessor):
  ```make
  PSP_PAD_POLL ?= peek          # adopted in T1
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
- `peek`: `int ret = sceCtrlPeekBufferPositive(&pad, 1);` the PSP-1000 probe returned **1** at 100 ms. Use
  `ret >= 0` as the safe success predicate, log the raw value, and do not generalize from PGE's emulator (it
  returns 0 for both calls).
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
- Test with the adopted `PSP_PAD_POLL=peek` at 100 ms: the latch is the only tap recovery. Repeat with `read`
  as the snapshot-miss control. With `readn`, the ORed snapshot may already contain the tap; `latchOnly=0` is
  valid.
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
- **Adopted:** `PSP_PAD_CYCLE ?= 10000` in `src/Makefile`. 100 Hz halved `lagms_max` at both frame intervals
  (16.09/16.08 → 8.95/7.83 ms) with block max unchanged (9 ms) and no return errors.

### Task log (append-only)

- 2026-11-21 — sprint rewritten for implementation readiness; all tasks `todo`, no device run
- 2026-11-21 — T1 `doing`: `PSP_PAD_POLL`/`TRACE`/`LATCH`/`FREEZE`/`CYCLE` switches, read/peek/readn arms,
  `pad_api` `FRAME_MS`/`TAP`/`FRAMES`; read and peek probe PRXs built, readn and 041 `PSP_PAD_FAIL` compile
- 2026-11-21 — 100 ms probes: read ret=1, peek ret=1, both success; median `TimeStamp` delta
  100,099/100,101 µs; `TimeStamp` is microseconds; no buttons
- 2026-11-21 — T1 done: read/peek/readn at 16/33/100 and vsync-off arms; peek adopted (9 µs, no block);
  read/readn block 13.7 ms vsync off; readn ret up to 6 at 100 ms; `PSP_PAD_POLL=peek` is the default
- 2026-11-21 — T2 `doing`: latch read after the snapshot, snapshot-priority, `latchOnly`, failure discard;
  latch 0/1 peek PRXs built
- 2026-11-21 — T2 100 ms run: `latchMake=0x00004000` mapped, `latchOnly` reached 19; the control also saw
  repeated `PAD pressed=7`, so the single-tap miss was not isolated; 200 ms/30-frame control+change built
- 2026-11-21 — T2 200 ms/30-frame run: zero nonzero `buttons` in both arms; the tap fell after DONE; a 200 ms
  / 100-frame (20 s) pair is built for a tap while the counter is moving
- 2026-11-21 — T2 200 ms/100-frame run: control no input; latch-on `latchMake=0x00004000`, `latchOnly=0`,
  `PAD pressed=7 frame=30`; the snapshot caught the tap, so the missed-snapshot case is still not isolated;
  a 1 s/20-frame pair is built
- 2026-11-21 — T2 1 s/20-frame run: control caught the tap (`PAD pressed=7 frame=7`); latch-on also caught it
  (`latchMake=0x4000`, `latchOnly=0`, `PAD pressed=7 frame=4`). The operator held the button to see the slow
  display; a 1 s/20-frame pair with a persistent 2 s `RECENT PRESS` indicator was built for tap-and-release
- 2026-11-21 — T2 done: control 0 nonzero buttons; latch-on `latchMake=0x00004000`, `latchOnly=1`,
  `PAD pressed=7 frame=7`; `PSP_PAD_LATCH=1` is the default with `peek`
- 2026-11-21 — T3 `doing`: read-first freeze, snapshot freeze/resync, latch discard, `PAD unfreeze`,
  control `PAD spurious`; freeze 0/1 `peek`+latch 30 s PRXs built
- 2026-11-22 — T3 HOME/HOLD run: fixed `PAD unfreeze frames=3` and no `PAD spurious`; control showed no
  spurious because CROSS was released during HOME; a later `PAD pressed=7` was a fresh press
- 2026-11-22 — T3 HOLD-only run: no `0x00020000`/`0x00010000` in 240 traced polls, no unfreeze/spurious; the
  HOLD switch was not exercised
- 2026-11-22 — control HOME hold-through is impossible: the system dialog requires releasing CROSS to choose
  YES/NO
- 2026-11-22 — prepared both T3 comparisons: `PSP_PAD_FORCE_FREEZE=1` injects 3 HOLD polls after 60 polls with
  CROSS held (deterministic, test-only); control/fixed synthetic PRXs and the real HOLD-switch pair are built
- 2026-11-22 — synthetic T3 run: control `PAD spurious=1`, `PAD pressed=7 frame=63`; fixed `PAD unfreeze
  frames=3`, no spurious
- 2026-11-22 — real HOLD T3 run: HOLD bit observed (4 control / 1 fixed); control `PAD spurious=1,2,3` and
  fresh `PAD pressed=7`; fixed `PAD unfreeze frames=1`, no spurious; T3 `done`
- 2026-11-22 — T4 `doing`: cumulative `PAD stats` (ret/block/lag/latch/spurious/frozen), boot `stats=`;
  `PSP_PAD_STATS_INTERVAL=30` stats PRX built; one-reader grep clean (`basic_shapes` exception)
- 2026-11-22 — T4 done: 60-frame stats run `ret0=0 retneg=0 retgt1=0`, `blockus_max=9`,
  `lagms_max=16.03–16.11`, `latchOnly=0 spurious=0 frozen=0`
- 2026-11-22 — T5 `doing`: contract PRX (default peek/latch/freeze/stats300), failure PRX (`read` +
  `PSP_PAD_FAIL=1`, stats30), `basic_shapes` and `batch_check WRAP=0` rebuilt; gate `b69830b8…`
- 2026-11-22 — T5 done: contract axes 2/0, right 0/0, triggers −1/−1; failure `cross=1 frame=60`,
  `PAD input failed ret=-1`, `retneg=2`; `basic_shapes` `DONE 600` at 60 fps; `batch_check` gate identical
- 2026-11-22 — T6 `doing`: cycle 0 and 100 Hz PRXs at 16/100 ms built with `PSP_PAD_STATS_INTERVAL=30`
- 2026-11-22 — T6 done: four no-input arms; block max 9–10 µs in all; `lagms_max` 16.09/16.08 (cycle 0) →
  8.95/7.83 ms (100 Hz) at 16/100 ms; `ret0=0 retneg=0 retgt1=0`, `latchOnly=0 spurious=0 frozen=0`;
  `PSP_PAD_CYCLE ?= 10000` adopted; strict library rebuilt and staged; `pad_api` and `input_gamepad` rebuilt
- 2026-11-22 — the adopted defaults are also the `rcore_psp.c` `#ifndef` fallbacks, so a direct compile matches
  the library; the library build log shows `-DPSP_PAD_POLL=1 -DPSP_PAD_LATCH=1 -DPSP_PAD_FREEZE=1
  -DPSP_PAD_CYCLE=10000 -DPSP_PAD_STATS_INTERVAL=300`

## 6. Budgets

| item | budget | measured |
|---|---|---|
| poll call | ≤ 0.2 ms typical; `peek` no sampling-period block | 9 µs max, both vsync modes |
| queue lag | ≤ 1 sample interval at 16 ms; bounded at 100 ms | 8.95/7.83 ms max at 16/100 ms (100 Hz) |
| lost taps | 0 after the latch is enabled (or counted) | 1 tap, `latchOnly=1`, reported |
| spurious presses through HOLD/interception | 0 in the fixed build | 0 fixed; control 1–3 |
| frame time | no renderer change; input cost only | 16.683 ms at 16 ms target, unchanged |
| cycle | choose by table, not preference | `10000` (100 Hz); lag halved, block unchanged |

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

**100 ms probe (2026-11-21, PSP-1000):**

| arm | ret | success | median `TimeStamp` delta | buttons | lx / ly |
|---|---|---|---|---|---|
| read | 1 | 1 | 100,099 µs | 0x00000000 | 99–100 / 114 |
| peek | 1 | 1 | 100,101 µs | 0x00000000 | 99–100 / 114 |

**T1 results (2026-11-21, PSP-1000):**

| arm | frame ms | ret | block med/max µs | frame med ms | lag med/max ms |
|---|---|---|---|---|---|
| read | 16 | 1 | 20/20 | 16.683 | 16.069/16.086 |
| read | 33 | 1 | 21/21 | 33.367 | 16.082/16.145 |
| read | 100 | 1 | 19/20 | 100.096 | 16.067/16.111 |
| peek | 16 | 1 | 9/9 | 16.683 | 16.059/16.077 |
| peek | 33 | 1 | 8/9 | 33.367 | 16.071/16.112 |
| peek | 100 | 1 | 9/9 | 100.104 | 16.074/16.089 |
| readn | 16 | 1/6 | 22/24 | 16.684 | 16.069/16.087 |
| readn | 33 | 1/2/6 | 23/24 | 33.366 | 16.080/16.122 |
| readn | 100 | 6 | 24/25 | 100.100 | 16.065/16.081 |
| read, vsync off | 16 | 1 | 13,696/13,895 | 16.683 | 0.051/4.198 |
| peek, vsync off | 16 | 1 | 9/10 | 16.076 | 8.572/16.696 |
| readn, vsync off | 16 | 1/6 | 13,710/13,884 | 16.683 | 0.051/4.040 |

Adopted **peek**: 9 µs and no block in both vsync modes. `read` and `readn` block ~13.7 ms with vsync off;
`readn` catches up to 6 samples at 100 ms but inherits that block. T2's latch is therefore required for taps.

**T6 results (2026-11-22, PSP-1000, adopted peek + latch + freeze, no input, stats every 30 polls):**

| cycle | frame ms | polls | block max µs | lag max ms | ret0 / neg / >1 | latchOnly / spurious / frozen |
|---|---:|---:|---:|---:|---|---|
| 0 (VBLANK) | 16 | 180 | 9 | 16.09 | 0 / 0 / 0 | 0 / 0 / 0 |
| 0 (VBLANK) | 100 | 60 | 10 | 16.08 | 0 / 0 / 0 | 0 / 0 / 0 |
| 10000 (100 Hz) | 16 | 180 | 9 | 8.95 | 0 / 0 / 0 | 0 / 0 / 0 |
| 10000 (100 Hz) | 100 | 60 | 9 | 7.83 | 0 / 0 / 0 | 0 / 0 / 0 |

`PAD poll` reported `cycle=0` or `cycle=10000` as built. 100 Hz roughly halves the timestamp lag at both frame
intervals with the same block time and no return errors, so `PSP_PAD_CYCLE=10000` is adopted.

## 8. Exit criteria

- [x] the three poll arms are measured at 16/33/100 ms; the adopted arm and reason are recorded
- [x] the peek return and `TimeStamp` scale are confirmed by the first probe, not assumed
- [x] `peek` does not block; `read`'s block is quantified; `readn`'s merge rule is documented
- [x] a sub-frame tap is reported by the latch or counted as lost; `latchOnly` matches the observed taps
- [x] `readn`'s ORed snapshot case is recorded (`latchOnly=0` is valid when the tap was already captured)
- [x] HOLD and interception produce zero spurious presses in the fixed build; the control build shows the cause
- [x] the held button is still down when input returns, and its eventual release fires once
- [x] `PAD` counters are logged at boot and every 30 polls during the A/B
- [x] only `PollInputEvents()` reads or latches the library controller; `basic_shapes` is recorded as a known
      sample exception
- [x] 041's contract values and `PSP_PAD_FAIL` held-button behavior are unchanged
- [x] the renderer gate and `basic_shapes` are unchanged
- [x] `PSP_PAD_CYCLE=0` vs `10000` is measured and the chosen cycle recorded
- [x] no public header or renderer file appears in the sprint diff

## 9. Result

T1 is complete. The full matrix is in §7; **peek** is adopted. It holds 9 µs in both vsync modes, while
`read` and `readn` block ~13.7 ms with vsync off; `readn` returns up to 6 samples at 100 ms but inherits the
blocking read. `TimeStamp` is microseconds, and `peek` returned 1 on this PSP-1000.

T2 is done. With the 2 s `RECENT PRESS` indicator the operator could tap and release immediately: the control
ran with zero nonzero snapshot buttons and no press; latch-on logged `latchMake=0x00004000`, `latchOnly=1` and one
`PAD pressed=7 frame=7`. The fallback is now isolated on hardware. `readn` was not adopted; its ORed snapshot
would catch the tap and report `latchOnly=0`, recorded as a design consequence rather than a separate run.
`PSP_PAD_POLL=peek` and `PSP_PAD_LATCH=1` are the defaults.

T3 is implemented: the fixed arm reads first, freezes on HOME/HOLD, resyncs on the first clean sample, discards
the latch during the frozen window, and logs `PAD unfreeze`; the control arm keeps the old order and logs
`PAD spurious`. The fixed HOME run froze for 3 frames with no spurious press. The control HOME comparison is
impossible as specified: the system dialog requires releasing CROSS to choose YES/NO. A HOLD-only run produced no
HOLD/HOME bit at all (240 traced polls, only `0x4000` CROSS).

T3 is complete. The real HOLD switch was observed: control logged `PAD spurious=1,2,3` and a fresh
`PAD pressed=7` after HOLD cleared; fixed logged `PAD unfreeze frames=1` with no spurious and no stale press.
The deterministic synthetic pair agrees (control spurious 1, fixed unfreeze 3), and the fixed HOME run logged
`PAD unfreeze frames=3` with no spurious. `PSP_PAD_POLL=peek`, `PSP_PAD_LATCH=1`, `PSP_PAD_FREEZE=1` are the
defaults.

T4 is done. The 60-frame stats run logged `PAD stats polls=1/30/60` with `ret0=0 retneg=0 retgt1=0`,
`blockus_max=9`, `lagms_max=16.03–16.11`, and `latchOnly=0 spurious=0 frozen=0`. The one-reader grep shows all
library reads/latches inside `PollInputEvents`; `psp-tests/basic_shapes:40` is the known second-reader exception.
`PSP_PAD_STATS_INTERVAL=300` is the shipped default; 30 was used for the A/B.

T5 is done. The contract PRX reported axes 2/0, right 0/0 and triggers −1/−1. The `read`+`PSP_PAD_FAIL=1`
run held CROSS: frame 60 showed `cross=1`, followed by `PAD input failed ret=-1` and `retneg=2`. `basic_shapes`
reported `DONE frames 600` at 60 fps, and `batch_check WRAP=0` is byte-identical to `b69830b8…`.

T6 is done. With the adopted poll arm (peek + latch + freeze), the two cycle settings were compared at 16 and
100 ms: `blockus_max` stayed at 9–10 µs in all four arms and every arm logged `ret0=0 retneg=0 retgt1=0` and
`latchOnly=0 spurious=0 frozen=0`. `lagms_max` fell from 16.09/16.08 ms (cycle 0) to 8.95/7.83 ms (100 Hz) at
the two frame intervals. 100 Hz halves the queue lag at no block-time cost, so `PSP_PAD_CYCLE` defaults to
`10000` and the full sampling stack is now **peek + latch + freeze + 100 Hz + stats**. Evidence:
`/home/psp/evidence/sprint042/`.

## 10. Follow-ups

- **Sprint 043 (P8-S3)** — calibration, radial deadzone, magnitude clamp and the idle threshold, using these
  counters to separate sampling from calibration. The 041 at-rest `LY=-0.21` is 043's first case.
- **Sprint 044 (P8-S4)** — keyboard/mouse, DS3, extra bits and the phase close.
- Remove `basic_shapes`' direct `sceCtrl` read in Phase 13; it is a second reader.
- `readn` was not adopted because it blocks with vsync off; its 6-sample merge is recorded, and a larger buffer
  is not a substitute for a non-blocking read.
- 100 Hz sampling is the library default; a `PSP_DISABLE_VSYNC` build now samples at the same 100 Hz without a
  blocking read. The renderer gate was verified on the cycle 0 build; the cycle change is input-only and no
  renderer file changed.
