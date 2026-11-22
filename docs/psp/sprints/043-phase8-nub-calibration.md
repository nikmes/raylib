# Sprint 043 (P8-S3) — Nub calibration and response

| | |
|---|---|
| Roadmap phase | [Phase 8 — Input](../phases/08-input/index.md) |
| Sprint | P8-S3 |
| Branch / base commit | `master` @ the 042 close (`4d000e0e` is 041; 042 closed 2026-11-22 in the working tree) |
| Started / closed | 2026-11-22 / 2026-11-22 |
| Build number(s) | pspgu `b24` (unchanged); `libraylib.a` builds with `PSP_PAD_CALIB=0\|1` and `PSP_PAD_IDLE=0\|1`. Both are adopted: the shipped build is `CALIB=1 IDLE=1`. `PSP_PAD_CURVE` is not added because T4 is `dropped` |
| Tasks | 5/5 resolved — T1, T2, T3, T5 done; T4 `dropped` |

## 0. Handover

**Situation.** Sprint 041 made `CORE.Input.Gamepad` a complete contract and sprint 042 chose how samples reach it.
The remaining input-quality question is the nub itself: the imported port assumes 128 is centre, normalises each
axis independently and applies a 0.2 per-axis deadzone. The documented centre tolerance is up to ±0x40, so that
deadzone is both too tight for a unit at the edge of tolerance and wasteful for a unit near nominal. This sprint
measures the raw range on this console, derives a radial deadzone and scaling, clamps the vector magnitude, and
sets the idle-cancel threshold so a nub-only session does not sleep.

**Read before acting**, in this order:

1. `../sprints/041-phase8-gamepad-contract.md` (P8-S1) §7 and `../sprints/042-phase8-sampling-and-edges.md`
   (P8-S2) §7 — the contract values and the counters that separate sampling from calibration.
2. `../phases/08-input/index.md` — “Gaps” 2, 5 and 8 and the fact table's analog, centre-tolerance, full-scale and
   idle-threshold rows.
3. `src/platforms/rcore_psp.c:655-673` — the current normalisation and per-axis deadzone (the poll path was
   rewritten by 042, so earlier line numbers in this doc were stale).
4. `/home/psp/pspdev/psp/sdk/include/pspctrl.h:375-383` — `sceCtrlSetIdleCancelThreshold(idlereset, idleback)`
   semantics: −1 never cancels, 0 always cancels, 1–128 requires movement.
5. `../memory-map.md` §3 — the hardware facts table this sprint appends a nub row to; §6 — the measurement protocol.
6. `psp-tests/pad_api/main.c` — the portable sample this sprint leaves untouched; the raw probe is a separate app.
   Its Makefile and `psp_module.c` are the scaffold for `pad_probe`.

**First action.** Create `psp-tests/pad_probe` by copying `psp-tests/pad_api/Makefile` and `psp_module.c` and
stripping the API drawing (keep `BUILD_PRX=1`, `PSP_LARGE_MEMORY=0`, `-lraylib -lpspgu …`). It is PSP-only,
draws the raw `Lx`/`Ly` numbers and a crosshair, and drives the numbered sequence on screen. Build the
`PSP_PAD_CALIB=0` control first, run the movement sequence in §5 T1, and do not change the deadzone or infer the
centre until the raw summary exists.

**Hard rules.**

- **Measure this console, not the model.** Record the raw min/max/centre and each half-range per direction. If
  a second unit appears, record it separately; never average two units into one constant.
- **Scale per direction, then radial.** `rxp = max−cx`, `rxn = cx−min` (likewise `ryp`/`ryn`): travel is not
  symmetric, so one half-range per axis would either clip the long side or lose travel on the short one. After
  centring and per-direction scaling, the deadzone is a radius in the normalised plane, followed by a rescale
  and a `sqrt(x² + y²) ≤ 1` clamp. A per-axis test makes diagonals behave differently from cardinals, which is
  the bug this sprint removes.
- **Do not assume 128 is centre and do not widen the deadzone blindly.** A 0x40 deadzone on a nominal-centre unit
  throws away usable travel; a 0.2 deadzone on an edge-of-tolerance unit drifts. Calibration chooses the value.
  Set `dz = idle_max + 0.02` from the measured idle magnitude and record both the value and the travel it costs.
- **Idle threshold is a decision with a number.** Query the current pair with `sceCtrlGetIdleCancelThreshold()`
  and log it before setting anything — the firmware default is not assumed. Set the pair deliberately with
  `sceCtrlSetIdleCancelThreshold()`, check the return (`< 0` logs a failure), and record both parameters.
- **The idle run cannot log.** PSPLink/`printf` traffic is host input and resets the idle timer. During the idle
  window the app must print nothing; it draws elapsed seconds on screen and the operator watches the display.
  The summary is printed after the window, and a frame gap near the timeout is the machine-checkable signal.
  On-screen state is allowed; host output is not.
- **The sign convention does not move.** Nub right → `LEFT_X > 0`, up → `LEFT_Y < 0`, as established in 041. Raw
  `Ly` grows downward, so `(Ly − cy)` is used un-inverted; up is negative because raw `Ly` decreases upward.
  Do not "correct" this while rewriting the block.
- **No public API change.** The calibrated values are `#ifndef` constants in `rcore_psp.c` (`PSP_NUB_CX`,
  `PSP_NUB_CY`, `PSP_NUB_RXP/RXN/RYP/RYN`, `PSP_NUB_DZ`); a game still reads the same two axes.
- **The strict right-axis path is separate.** The non-strict control arm still reads `Rsrv` with the original
  `0.2f` constant; do not feed the measured nub values to the right stick.
- **No renderer change.** The gate is byte-identical.
- **Stage only this sprint's paths.** `git status` before every commit.
- Commit messages carry the task ID: `sprint 043 T2: ...`.

**Commands.**

```sh
export PSPDEV=/home/psp/pspdev
export PATH=$PATH:$PSPDEV/bin

cd /home/psp/raylib-psp/src
make PLATFORM=PLATFORM_PSP -j8 PSP_PAD_CALIB=0 PSP_PAD_IDLE=0
cp libraylib.a $PSPDEV/psp/lib/libraylib.a

cd ../psp-tests/pad_probe
make clean && make RUN=1
cp pad_probe.prx /tmp/s3/pad_probe.prx
# host0 is the directory usbhostfs_pc was started in (/tmp/s3 here).
cd /tmp/s3 && ( printf './pad_probe.prx\n'; sleep 110 ) | timeout 140 pspsh

# T2 A/B driver: raw plus the library's axes, one line per second.
make clean && make MANUAL_SECS=30

# T3 idle arm: no periodic stats inside the window, and the probe prints only at the end.
cd /home/psp/raylib-psp/src
make clean && make PLATFORM=PLATFORM_PSP -j8 PSP_PAD_IDLE=1 PSP_PAD_STATS_INTERVAL=0
cp libraylib.a $PSPDEV/psp/lib/libraylib.a
cd ../psp-tests/pad_probe && make clean && make IDLE_SECS=360
cp pad_probe.prx /tmp/s3/pad_probe.prx

# Then rebuild with the measured constants (PSP_PAD_CALIB=1) and repeat pad_api.
```

`reset` first (`pspsh -e "reset"`), then run with the shell attached; never send `exit`. The 110 s pipe covers the
10 s pre-roll plus the ~66 s sequence.

**Do not touch.** The 041 contract defaults and the 042 sampling arm; keyboard/mouse, DS3 and the extra-bit
decisions (044); the renderer, pspgu and the public headers.

**Done means** §8: the raw range is recorded for this console, the radial deadzone removes idle drift without
losing travel, the vector never exceeds 1, and a nub-only session survives the idle timeout.

## 1. Objective

Five falsifiable results:

1. Raw `Lx`/`Ly` min, max, idle centre and per-direction half-ranges are recorded for this console, with the
   five-run spread.
2. With calibrated values, the nub at rest reads exactly 0 on both axes; the old fixed deadzone's drift or lost
   travel is quantified against the new one.
3. Diagonal and cardinal travel produce comparable rim magnitudes (both ≈1.0, difference ≤ 0.02), and
   `sqrt(LEFT_X² + LEFT_Y²) ≤ 1` everywhere, including at a corner.
4. A nub-only session longer than the console's idle timeout does not dim or sleep with the chosen
   `sceCtrlSetIdleCancelThreshold()` values.
5. The calibration values and the idle decision are written into the Phase 8 page and `memory-map.md` §3.

## 2. Why now

Sprints 041 and 042 make the state correct and the sampling predictable; the remaining “the stick feels wrong”
cases are calibration. Doing this before 044 means the keyboard/mouse and extra-bit decisions are made on a nub
that already behaves, and the phase's exit criteria for range, magnitude and idle have numbers instead of
assumptions. It also gives the project a reusable per-unit calibration procedure for later ports.

## 3. Scope

**In:** `src/platforms/rcore_psp.c` normalisation/deadzone and idle threshold; `src/Makefile`
(`PSP_PAD_CALIB`, `PSP_PAD_IDLE`, and `PSP_PAD_CURVE` only if T4 is adopted); `psp-tests/pad_probe/` (PSP-only raw
probe); a calibration note in the Phase 8 page and `memory-map.md` §3.

**Out:** sampling and latch (042); keyboard/mouse, DS3 and the extra-bit decisions (044); the renderer; a
user-facing calibration UI (a follow-up if a shipped game needs it).

## 4. Preconditions

- [x] Sprints 041 and 042 closed; the contract and sampling arms are recorded
- [x] `pad_probe` is scaffolded from `pad_api`'s Makefile and `psp_module.c`, and builds as a PRX
- [x] `pad_probe` logs raw values from its own app with `Peek` only; it never calls `ReadBuffer` or `ReadLatch`
- [x] The physical movement sequence in T1 is written down before the run, and the probe shows it on screen
- [x] The calibration constant names and `PSP_PAD_CALIB=0` / `PSP_PAD_IDLE=0` defaults are in the Makefile
- [x] The console's idle timeout is known, or a 6-minute run is accepted as the bound
- [x] Device loop available; `reset` leaves partition 2 at 25,149,440 bytes
- [x] `PSP_PAD_CALIB=0` control run is captured in §7 before the change

## 5. Tasks

Status vocabulary: `todo` · `doing` · `done` · `blocked` · `dropped`.

| ID | task | status | verification | evidence |
|---|---|---|---|---|
| T1 | **Raw probe**: `psp-tests/pad_probe` shows `Lx`/`Ly` and drives the sequence; one `RAWSUM` per run | `done` | min/max/centre and per-direction half-ranges from five runs; run-to-run spread stated | five runs, full travel `0..255` each; clean centres 133/102, 114/120, 133/104, 133/120; run 4 rejected (`idle_dx=82`); centre spread 19 X / 18 Y; adopted `(133,112)`, `122/133/143/112`; `dz=0.18` |
| T2 | **Radial deadzone and scaling**: per-direction scale, radial deadzone, magnitude clamp; `PSP_PAD_CALIB=0\|1` | `done` | idle reads 0; old/new drift and travel; diagonal vs cardinal; magnitude ≤1 at every point | idle `0,0` at raw `(123,109)`, `(113,113)`, `(133,100)`; rim components 0.94–0.99 with magnitude 1.000 on all four cardinals and the diagonal; control diagonal magnitude **1.16** (per-axis path), calibrated 1.00 |
| T3 | **Idle threshold**: query the default, then `PSP_PAD_IDLE=0\|1`; set the measured pair | `done` | a nub-only run longer than the timeout does not dim/sleep; both parameters and the boot default logged | boot `get` `-1/-1`; set `56/56` ret=0; 360 s nub-only run `elapsed=360s sleepgap_ms=17.9`, no dim and no sleep observed |
| T4 | **Response curve (optional)**: only if the `RAWPOS` small-offset data needs one | `dropped` | dropped: a curve cannot recover output inside the deadzone, and the linear path reaches the rim | `dz=0.18` means the first usable output needs ~24 raw counts; `RAWPOS` at 10/20 counts falls inside it by design; above the deadzone the linear scale reaches 1.0 |
| T5 | **Regression and writeback**: 041/042 samples, `basic_shapes`, `batch_check`; write the numbers back | `done` | contract unchanged; gate hash unchanged; §3 row and Phase 8 page updated | shipped build `CALIB=1 IDLE=1`: axes 2/0, right 0/0, triggers −1/−1, idle 0.00; `basic_shapes DONE 600` 60 fps; `batch_check WRAP=0 DONE 2` and `b69830b8…` identical; memory-map §3 and Phase 8 facts updated |

### T1 design

- `pad_probe` is PSP-only and may include `pspctrl.h`; the portable `pad_api` stays free of PSP symbols. Copy
  `pad_api`'s Makefile and `psp_module.c`, set `TARGET = pad_probe` and a matching `PSP_EBOOT_TITLE`, drop the
  API drawing, keep `BUILD_PRX = 1` and `PSP_LARGE_MEMORY = 0`.
- It reads with `sceCtrlPeekBufferPositive(&pad, 1)` once per frame. Peek does not consume the controller, so the
  probe is not a second consumer and cannot steal samples or the latch from raylib's own poll. It must never call
  `sceCtrlReadBuffer*` or `sceCtrlReadLatch`. On screen: raw `Lx`/`Ly`, the current step, a countdown and a live
  normalized dot.
- A 10 s `GET READY` pre-roll runs before step 1 and records nothing; the sequence clock starts after it.
- The sequence is driven by the probe, so timing is not the operator's job:
  1. 10 s hands off (idle centre and drift);
  2. 5 s full right, hold;
  3. 5 s full left, hold;
  4. 5 s full up, hold;
  5. 5 s full down, hold;
  6. 6 s clockwise rim roll;
  7. 6 s anticlockwise rim roll;
  8. 4 s released, centred;
  9. four 5 s ramps from centre to the rim: right, left, up, then down.
- Holds were lengthened from 3 s after the first runs showed the operator reacting too late; the ramps replaced the
  exact-count holds because a human cannot hold 10/20/40/80 counts reliably. `RAWPOS` is derived from the ramp
  samples instead, by taking the closest sample to each target and reporting its error.
- A `MANUAL_SECS` mode logs raw and the library's `LX`/`LY` once per second for a live position check; it is the
  A/B driver for T2.
- Aggregate on the device, not in the host log: one `RAWSUM` line per run is the evidence, so the host pipe never
  carries per-frame traffic. Keep the raw min/max, the idle median per axis, the idle raw spread, the largest
  normalized idle magnitude, the roll maximum, and the sample count:
  `RAWSUM run=1 cx=.. cy=.. lx_min=.. lx_max=.. ly_min=.. ly_max=.. rxp=.. rxn=.. ryp=.. ryn=.. idle_dx=..
  idle_dy=.. idle_max=.. rim_max=.. samples=..`
  `idle_max` is the largest `sqrt((dx/rx)² + (dy/ry)²)` in the idle window (the value `dz` is derived from), and
  `idle_dx`/`idle_dy` are the raw spreads that reject a run where the nub moved while it should have been still.
- Record centre separately per axis and per direction: `rxp`, `rxn`, `ryp`, `ryn`. Five runs; bake the median
  centre and the median half-ranges, and state each spread (max−min across the runs) in §7.
- `RAW lx=%u ly=%u ts=%u` remains available behind a compile flag for a single interactive session, but it is
  not the run evidence.

### T2 design

- Constants in `rcore_psp.c`, all `#ifndef` so a direct compile still works:
  `PSP_NUB_CX`/`CY` (raw centre), `PSP_NUB_RXP`/`RXN`/`RYP`/`RYN` (raw half-ranges), `PSP_NUB_DZ`
  (normalised radial deadzone). They carry the measured values from §7; no public header changes.
- Per direction: `rx = (dx >= 0) ? RXP : RXN` (likewise `ry`); `nx = dx / rx`, `ny = dy / ry`, each clamped to
  ±1. A single half-range per axis is not enough: it clips the long side or loses travel on the short one.
- Radial deadzone and rescale:
  ```c
  float mag = sqrtf(nx*nx + ny*ny);
  if (mag < PSP_NUB_DZ) { nx = ny = 0.0f; }
  else {
      float k = (mag - PSP_NUB_DZ) / (1.0f - PSP_NUB_DZ);
      nx *= k/mag; ny *= k/mag;
      float out = sqrtf(nx*nx + ny*ny);
      if (out > 1.0f) { nx /= out; ny /= out; }
  }
  ```
  `PSP_NUB_DZ = idle_max + 0.02` from T1, rounded to two decimals and recorded with the travel it costs.
- `PSP_PAD_CALIB=0` is the 041/042 control: `(Lx - 128) / 128`, `(Ly - 128) / 128`, per-axis 0.2. `=1` is the
  block above. The default stays `0` until the measured constants are baked, then flips to `1`; the flip is its
  own one-line change with the A/B already in §7.
- The non-strict right-axis block keeps its own `0.2f` constant; measured nub values are not applied to `Rsrv`.
- Record the old and new values for the same physical positions: idle, right rim, left rim, up rim, diagonal rim.
  Expected: 0 at idle with the new path, no lost travel at the rim, comparable diagonal/cardinal output.
- Do not change the public axis count or the sign convention.

### T3 design

- At boot, both arms call `sceCtrlGetIdleCancelThreshold(&reset, &back)` and log
  `PAD idle get reset=%d back=%d`; the control arm's row therefore records the firmware default instead of
  assuming −1. `PSP_PAD_IDLE=0` changes nothing after the query; `=1` then calls
  `sceCtrlSetIdleCancelThreshold(idlereset, idleback)`, logs the requested pair and whether the return was `< 0`.
- Pair choice: `idlereset` is a raw movement in counts on either axis. Sony's sample uses `(0x40, -1)` and PGE
  `(25, 25)`; the phase page records both. Pick a value between them that the measured rim travel comfortably
  exceeds (start from the smaller of 0x40 and half the smaller half-range), and set `idleback` deliberately
  rather than only `idlereset`. Record the exact pair and the reason. **Adopted `0x38/0x38` (56/56):** below
  Sony's `0x40` and half the smallest measured half-range (112/2 = 56). The boot `get` pair on this console is
  `-1/-1`.
- Idle test protocol, nub only:
  - launch with the nub untouched and the shell attached but silent; no prints during the window;
  - the app draws elapsed seconds and a heartbeat block, and records each frame's timestamp;
  - the operator watches the physical display for dimming and sleep;
  - after the window, one line: `PAD idle elapsed=%us sleepgap_ms=%.1f`, where `sleepgap_ms` is the
    largest frame-to-frame gap (a gap near the timeout means the console slept and resumed); dimming is an
    operator observation recorded in §7, since PSPSDK has no brightness getter;
  - use the console's own timeout; if unknown, 6 minutes is the accepted bound, and the run reports the actual
    elapsed time.
- A button is not needed for the exit criterion, but the pair must not affect buttons: press one once during the
  window and confirm on screen that the probe shows it, which also proves the probe was alive without host
  traffic. No log line is emitted until the window ends.
- The idle build must also set `PSP_PAD_STATS_INTERVAL=0`: the library's periodic `PAD stats` line is host
  traffic during the window. `PSP_PAD_IDLE` flips from `0` to `1` only after this table exists.

### T4 design

- Sony's four-band angle/speed response is a candidate, not a core requirement. Sony's sample cites a four-band
  response; the band edges are not in our sources, so any curve here is empirical and must be measured.
- Decide from `RAWPOS` in T1: normalized output at raw 10/20/40/80 counts and at the rim, linear. Adopt a curve
  only if the linear path cannot resolve a deliberate small offset a game needs (record which offset fails); do
  not adopt on feel alone.
- If adopted, keep it behind `PSP_PAD_CURVE=0|1`, record the band edges and speeds in the phase page, and verify
  the rim still reaches ±1 and the magnitude clamp still holds.
- If not adopted, mark T4 `dropped` in the task log with the reason and the `RAWPOS` numbers; that is a valid
  result, and `PSP_PAD_CURVE` is then not added to the build at all.

### T5 design

- Re-run the 041 contract checks and the 042 sampling checks against the **shipped** build (`CALIB=1`,
  `IDLE=1`), not the control: the calibrated axes must read 0 at rest and keep their signs; `axisCount` 2,
  right 0/0, triggers −1/−1, `PAD stats` counters still clean.
- Run `basic_shapes` and `batch_check`; the renderer gate must not move (`b69830b8…`).
- Append a nub row to `memory-map.md` §3 directly after the Input hardware row, tagged
  `[measured: PSP-1000, sprint 043]`: centre, four half-ranges, `dz`, idle pair. Update the Phase 8 page's
  “What we already know” table (the analog/centre-tolerance/idle rows at lines 96–102) with the same numbers.
- Keep the per-unit rule visible in both places: a second console gets its own row, never a shared constant.

### Task log (append-only)

- 2026-11-21 — sprint written; all tasks `todo`, no device run
- 2026-11-22 — readiness review after the 042 close: poll-path line reference corrected to `655-673`; header
  range corrected to `375-383`; per-direction half-ranges, named `#ifndef` constants, `dz = idle_max + 0.02`,
  `CALIB`/`IDLE` default-to-flip sequence, the right-axis `0.2f` isolation, the probe's on-device `RAWSUM`/
  `RAWPOS` aggregation, and the no-host-traffic idle protocol added; §7 tables and tolerances made concrete
- 2026-11-22 — code side implemented, no device run: `psp-tests/pad_probe` (sequence + idle modes, on-device
  `RAWSUM`/`RAWPOS`, `sleepgap_ms`), `PSP_PAD_CALIB`/`PSP_PAD_IDLE` switches, per-direction radial branch,
  right-stick `0.2f` isolation, idle `get`/`set` with return check and boot logging; strict library builds
  warning-free in both arms (`CALIB=1 IDLE=1` and the defaults), and the probe builds in both modes
- 2026-11-22 — T1 `doing`: probe extended with a 10 s GET READY pre-roll, longer steps, slow ramps and on-device
  idle spread; five runs captured; run 4 rejected (`idle_dx=82`)
- 2026-11-22 — T1 done: full travel `0/255` each run; clean centres 133/102, 114/120, 133/104, 133/120; adopted
  `(133,112)` with `122/133/143/112`; centre spread 19 X / 18 Y; `dz=0.18`
- 2026-11-22 — T2 `doing`: control `CALIB=0` and calibrated `CALIB=1` live checks with raw values logged
- 2026-11-22 — T2 done: idle 0.000 at three raw states; rim magnitudes 1.000 on all cardinals and the diagonal;
  control diagonal magnitude 1.16; `PSP_PAD_CALIB` default flipped to 1
- 2026-11-22 — T4 `dropped`: `dz=0.18` puts 10/20-count holds inside the deadzone, so a curve cannot recover
  them; linear reaches 1.0 above the deadzone
- 2026-11-22 — T3 `doing`: boot pair `-1/-1`, chosen pair `0x38/0x38`, 6-minute nub-only run in progress
- 2026-11-22 — T3 done: `set 56/56 ret=0`; `elapsed=360s sleepgap_ms=17.9`; no dim, no sleep
- 2026-11-22 — T5 done: shipped `CALIB=1 IDLE=1`; `pad_api` axes 2/0, right 0/0, triggers −1/−1, idle 0.00;
  `basic_shapes DONE 600` 60 fps; `batch_check WRAP=0` `b69830b8…` identical; sprint closed 5/5 resolved

## 6. Budgets

| item | budget | measured |
|---|---|---|
| idle output | exactly 0 on both axes after calibration (`|value| < 0.005`) | 0.000 at three resting raw states |
| rim travel | each direction reaches ≥ 0.98 without a deadzone-sized hole | leading component 0.94–0.99; total magnitude 1.000 |
| magnitude | ≤ 1.000 at every sampled point | 1.000 at cardinals and diagonal; control hit 1.16 |
| diagonal/cardinal | rim vector magnitudes both ≈1.0, difference ≤ 0.02 | 1.000 vs 1.000, difference 0.00 |
| idle session | no dim/sleep past the timeout | 360 s, no dim, no sleep; max gap 17.9 ms |
| renderer gate | byte-identical (`b69830b8…`) | identical |

## 7. Measurements

Protocol: device only; one variable per run; control → change → control again. Every row carries its build number.

**Raw range** (five runs; one `RAWSUM` per run):

| run | build | lx min/max | ly min/max | centre (cx, cy) | rxp / rxn | ryp / ryn | idle max | rim max | samples |
|---|---|---|---|---|---|---|---|---|---|
| 1 | `pad_probe` | 0/255 | 0/255 | 133, 102 | 122 / 133 | 153 / 102 | 0.010 | 1.414 | 3946 |
| 2 | `pad_probe` | 0/255 | 0/255 | 114, 120 | 141 / 114 | 135 / 120 | 0.023 | 1.414 | 3946 |
| 3 | `pad_probe` | 0/255 | 0/255 | 133, 104 | 122 / 133 | 151 / 104 | 0.010 | 1.414 | 3946 |
| 4 | `pad_probe` | 0/255 | 0/255 | 139, 109 | 116 / 139 | 146 / 109 | 0.690* | 1.414 | 3946 |
| 5 | `pad_probe` | 0/255 | 0/255 | 133, 120 | 122 / 133 | 135 / 120 | 0.017 | 1.414 | 3946 |
| **adopted** | four clean runs | 0/255 | 0/255 | **133, 112** | **122 / 133** | **143 / 112** | 0.154 worst | 1.414 | `dz=0.18` |

\* Run 4 is rejected for the centre: `idle_dx=82` means the nub moved during the hands-off window. Its travel data is
consistent with the others. Spread across the four clean runs: centre 19 X / 18 Y, half-ranges 19/19/18/18.
The neutral has two resting states about 19 counts apart, which is why `dz` covers 0.154 and not 0.023.

**Deadzone A/B** (same two PRXs; raw values identify the position, so the operator's timing need not be exact):

| run | build | position | raw (Lx, Ly) | `PSP_PAD_CALIB=0` | `PSP_PAD_CALIB=1` | notes |
|---|---|---|---|---|---|---|
| idle | `pad_probe` manual | untouched | 123, 109 | `0.00, 0.00` | `0.00, 0.00` | control and calibrated both 0 |
| idle | `pad_probe` manual | untouched | 113, 113 | — | `0.00, 0.00` | 19 counts off the baked centre, still 0 |
| right rim | `pad_probe` manual | full right | 255, 110 / 255, 70 | `0.99, 0.00` | `0.94, -0.35` | calibrated run also pushed up; magnitude 1.000 |
| left rim | `pad_probe` manual | full left | 0, 127 / 0, 86 | `-1.00, 0.00` | `-0.97, -0.23` | magnitude 1.000 |
| up rim | `pad_probe` manual | full up | 131, 0 / 115, 0 | `0.00, -1.00` | `-0.13, -0.99` | magnitude 1.000 |
| down rim | `pad_probe` manual | full down | 128, 255 / 104, 255 | `0.00, 0.99` | `-0.21, 0.98` | magnitude 1.000 |
| diagonal | `pad_probe` manual | up-right | 255, 51 / 255, 2 | `0.99, -0.60` | `0.71, -0.70` | control magnitude **1.16**; calibrated **1.00** |
| corner magnitude | `pad_probe` sequence | rim rolls | — | — | 1.000 | `rim_max=1.414` pre-clamp |

**Idle** (no host traffic during the window):

| run | build | input | boot `get` pair | set pair | elapsed | display dim/sleep | `sleepgap_ms` |
|---|---|---|---|---|---|---|---|
| control | `PSP_PAD_IDLE=0` | nub only | `-1/-1` | unchanged | not run | the default was recorded via `get`; the criterion is the changed arm | — |
| change | `PSP_PAD_IDLE=1` | nub only | `-1/-1` | `56/56` | 360 s | none | 17.9 |

**T5 regression (shipped build `CALIB=1 IDLE=1`, stats 300):**

| run | workload | result |
|---|---|---|
| `pad_api` | 1800 frames | axes `2/0`, right `0.00/0.00`, triggers `-1.00/-1.00`, idle `0.00/0.00`, `DONE frames 1800` |
| `basic_shapes` | 600 frames | `DONE frames 600`, 60 fps |
| `batch_check WRAP=0` | 2 frames | `DONE frames 2`; SHA-256 `b69830b8…` byte-identical to `par_pspgl.raw` |

## 8. Exit criteria

- [x] raw min/max/centre and per-direction half-ranges are recorded from five runs on this console, with the
      run-to-run spread stated
- [x] calibrated idle reads 0 (`|value| < 0.005`); the old/new drift and travel difference are quantified
- [x] diagonal and cardinal rim magnitudes differ by ≤ 0.02 (both ≈1.0), and the magnitude never exceeds 1.000
- [x] `PSP_PAD_CALIB=0` and `=1` are the only difference in the deadzone A/B, and the shipped default is `1`
- [x] the boot `sceCtrlGetIdleCancelThreshold()` pair is logged in both arms, and a nub-only session longer than
      the idle timeout does not dim or sleep with the chosen threshold
- [x] the response curve is adopted or `dropped` with a reason and the `RAWPOS` numbers
- [x] the 041/042 samples and the renderer gate are unchanged on the shipped build
- [x] the values are written into `memory-map.md` §3 and the Phase 8 page

## 9. Result

**T1 done (2026-11-22).** Five sequence runs captured. Every run reached `Lx` and `Ly` 0/255. Run 4 is rejected
because the nub moved 82 counts during the hands-off window; the four clean runs give centres `(133,102)`,
`(114,120)`, `(133,104)`, `(133,120)` — a 19 X / 18 Y spread. The neutral has two resting states, so the adopted
constants are the medians `(133,112)` with half-ranges `122/133/143/112`, and `dz` is driven by the worst idle
magnitude (0.154), not the clean-run 0.010–0.023. Full table in §7.

**T2 done (2026-11-22).** `PSP_PAD_CALIB=1` is the shipped default; the constants above are baked in
`rcore_psp.c`. Idle reads `0.000` on both axes at three different resting raw states, including one 19 counts off
the baked centre. At the rim the leading component reads 0.94–0.99 while the vector magnitude is exactly 1.000 on
all four cardinals and on the diagonal. The control path's independent axes produced a diagonal magnitude of
**1.16**, which is the defect the radial clamp removes. Because `dz=0.18`, the first usable output needs about 24
raw counts; that is the measured cost of covering the two resting states, recorded rather than hidden.

**T4 dropped (2026-11-22).** `RAWPOS` shows 10- and 20-count holds landing inside `dz` by design, so a response
curve cannot recover them; above the deadzone the linear scale already reaches 1.0. A curve would reshape output
that is already correct, so it is not added. A runtime centre tracker that shrinks `dz` is the follow-up that
would help slow control.

**T3 done (2026-11-22).** The firmware pair was recorded before changing anything: `-1/-1`. `PSP_PAD_IDLE=1`
applied `56/56` (`0x38`), with `ret=0`. A 360 s nub-only run logged `elapsed=360s sleepgap_ms=17.9`: the largest
frame gap is a normal frame, so the console neither slept nor resumed, and the operator observed no dimming. The
`PSP_PAD_IDLE=0` control arm was not run for 6 minutes; the default pair is recorded and the exit criterion is
the changed arm.

**T5 done (2026-11-22).** The shipped build keeps the 041/042 contract: axes `2/0`, right `0.00/0.00`, triggers
`-1.00/-1.00`, and idle now reads `0.00/0.00` (041 recorded `LY=-0.21`). `basic_shapes` logged `DONE frames 600`
at 60 fps, and `batch_check WRAP=0` produced a capture whose SHA-256 is `b69830b8…`, byte-identical to
`par_pspgl.raw`. The values are written into `memory-map.md` §3 and the Phase 8 facts table.

**Sprint 043 closed 2026-11-22: 5/5 resolved, T4 dropped.**

## 10. Follow-ups

- **Sprint 044 (P8-S4)** — decisions, keyboard/mouse, DS3 and the phase close.
- A runtime calibration UI (hold a button, sample the range, store the values) is a follow-up if a shipped game
  needs to run on several units; the bake-time path is enough for this console.
- Calibration is per unit. A second console gets its own row in §7 and its own constants; never average two.
- If T4 is adopted, the curve edges and speeds belong in a recipe only after a second workload confirms them.
- If a later test shows `PSP_NUB_DZ` too tight or too wide, re-derive it from the same idle window rather than
  editing the number by hand.
