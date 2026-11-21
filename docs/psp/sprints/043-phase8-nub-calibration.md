# Sprint 043 (P8-S3) — Nub calibration and response

| | |
|---|---|
| Roadmap phase | [Phase 8 — Input](../phases/08-input/index.md) |
| Sprint | P8-S3 |
| Branch / base commit | `master` @ the sprint 042 close (this doc is written before 041/042 run) |
| Started / closed | — / — |
| Build number(s) | pspgu `b24` (unchanged); `libraylib.a` builds with `PSP_PAD_CALIB` and `PSP_PAD_IDLE`, plus `PSP_PAD_CURVE` if the optional response curve is adopted |
| Tasks | 0/5 done |

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
3. `src/platforms/rcore_psp.c:471-490` — the current normalisation and per-axis deadzone.
4. `/home/psp/pspdev/psp/sdk/include/pspctrl.h:370-390` — `sceCtrlSetIdleCancelThreshold(idlereset, idleback)`
   semantics: −1 never cancels, 0 always cancels, 1–128 requires movement.
5. `../memory-map.md` §3 — the hardware facts table this sprint adds a nub row to; §6 — the measurement protocol.
6. `psp-tests/pad_api/main.c` — the portable sample this sprint leaves untouched; the raw probe is a separate app.

**First action.** Create `psp-tests/pad_probe` (PSP-only, `pspctrl.h` allowed) that draws and logs raw `Lx`/`Ly`
at 10 Hz. Run the numbered movement sequence in §5 T1 before changing the deadzone. Do not infer the centre.

**Hard rules.**

- **Measure this console, not the model.** Record the raw min/max/centre and the reachable radius per axis. If a
  second unit appears, record it separately; never average two units into one constant.
- **Radial, not per-axis.** After centring and per-axis scaling, the deadzone is a radius in the normalised plane,
  followed by a rescale and a `sqrt(x² + y²) ≤ 1` clamp. A per-axis test makes diagonals behave differently from
  cardinals, which is the bug this sprint removes.
- **Do not assume 128 is centre and do not widen the deadzone blindly.** A 0x40 deadzone on a nominal-centre unit
  throws away usable travel; a 0.2 deadzone on an edge-of-tolerance unit drifts. Calibration chooses the value.
- **Idle threshold is a decision with a number.** `sceCtrlSetIdleCancelThreshold(0, …)` would cancel the idle
  timer even with no movement; use the measured movement threshold. Record both parameters.
- **The sign convention does not move.** Nub right → `LEFT_X > 0`, up → `LEFT_Y < 0`, as established in 041.
- **No public API change.** The calibrated values are platform constants or a private struct; a game still reads
  the same two axes.
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
make clean && make
mkdir -p /tmp/p8s3 && cp pad_probe.prx /tmp/p8s3/app.prx
cd /tmp/p8s3 && timeout 20 pspsh -e "reset"; sleep 6
( printf './app.prx\n'; sleep 45 ) | timeout 70 pspsh

# Then rebuild with the measured constants (PSP_PAD_CALIB=1) and repeat pad_api.
```

**Do not touch.** The 041 contract defaults and the 042 sampling arm; keyboard/mouse, DS3 and the extra-bit
decisions (044); the renderer, pspgu and the public headers.

**Done means** §8: the raw range is recorded for this console, the radial deadzone removes idle drift without
losing travel, the vector never exceeds 1, and a nub-only session survives the idle timeout.

## 1. Objective

Five falsifiable results:

1. Raw `Lx`/`Ly` min, max, idle centre and reachable radius are recorded for this console, per axis.
2. With calibrated values, the nub at rest reads exactly 0 on both axes; the old fixed deadzone's drift or lost
   travel is quantified against the new one.
3. Diagonal and cardinal travel produce comparable speeds at the rim, and `sqrt(LEFT_X² + LEFT_Y²) ≤ 1` everywhere,
   including at a corner.
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

- [ ] Sprints 041 and 042 closed; the contract and sampling arms are recorded
- [ ] `pad_probe` can log raw values without consuming a second reader in the same build
- [ ] The physical movement sequence in T1 is written down before the run
- [ ] The console's idle timeout is known, or a 6-minute run is accepted as the bound
- [ ] Device loop available; `reset` leaves partition 2 at 25,149,440 bytes
- [ ] `PSP_PAD_CALIB=0` control run is captured in §7 before the change

## 5. Tasks

Status vocabulary: `todo` · `doing` · `done` · `blocked` · `dropped`.

| ID | task | status | verification | evidence |
|---|---|---|---|---|
| T1 | **Raw probe**: `psp-tests/pad_probe` logs `Lx`/`Ly` at 10 Hz and draws a crosshair; run idle and at each limit | `todo` | per-axis min/max/centre/radius from five runs; the sequence and sample count are logged | — |
| T2 | **Radial deadzone and scaling**: centre, scale and deadzone from the measured values; clamp the magnitude; `PSP_PAD_CALIB=0\|1` | `todo` | idle reads 0; old vs new drift/travel; diagonal vs cardinal; magnitude ≤1 at every point | — |
| T3 | **Idle threshold**: `PSP_PAD_IDLE=0\|1`; set `sceCtrlSetIdleCancelThreshold(idlereset, idleback)` from the measured range | `todo` | a nub-only run longer than the timeout does not dim/sleep; button-only input is unaffected | — |
| T4 | **Response curve (optional)**: linear vs a measured four-band curve, adopted only if slow control needs it | `todo` | if adopted, a slow-movement test shows finer control without moving the rim; otherwise `dropped` with the reason | — |
| T5 | **Regression and writeback**: 041/042 samples, `basic_shapes`, `batch_check`; write the numbers back | `todo` | contract unchanged; gate hash unchanged; §3 row and Phase 8 page updated | — |

### T1 design

- `pad_probe` is PSP-only and may include `pspctrl.h`; the portable `pad_api` stays free of PSP symbols.
- It reads with `sceCtrlReadBufferPositive(&pad, 1)` once per 100 ms and logs
  `RAW lx=%u ly=%u ts=%u` plus an on-screen value. It does not draw the gamepad API; it is a measurement tool.
- Numbered movement sequence, one operator, one run per line:
  1. leave the nub untouched for 10 s (idle centre and drift);
  2. push fully right and hold 3 s, release;
  3. push fully left and hold 3 s, release;
  4. push fully up and hold 3 s, release;
  5. push fully down and hold 3 s, release;
  6. roll the nub around the rim once, clockwise, 5 s;
  7. roll anticlockwise once, 5 s;
  8. release for 5 s.
- From the log: `min`, `max`, idle median (centre), and `max(sqrt((Lx-cx)² + (Ly-cy)²))` at the rim. Record the
  centre separately for each axis; the nub is not symmetric.
- Five runs; require the centre and radius to agree within the run-to-run spread before baking a constant.

### T2 design

- Per axis: `dx = (float)Lx - cx`, `dy = (float)Ly - cy`; scale by the measured half-range so each axis reaches
  ±1 over its own travel: `nx = dx / rx`, `ny = dy / ry` (clamp each to ±1).
- Radial deadzone: `mag = sqrt(nx² + ny²)`; if `mag < dz`, both axes are 0. Otherwise rescale
  `k = (mag - dz) / (1 - dz)` and output `nx * k / mag`, `ny * k / mag`, then clamp the magnitude to 1.
- `PSP_PAD_CALIB=0` is the 041/042 control: `(Lx - 128) / 128`, `(Ly - 128) / 128`, per-axis 0.2. `=1` uses the
  measured centre, half-range, `dz` and the radial rescale.
- Record the old and new values for the same physical positions: idle, right rim, diagonal rim. The expected
  result is 0 at idle, no lost travel at the rim, and comparable diagonal/cardinal speed.
- Do not change the public axis count or the sign convention.

### T3 design

- `sceCtrlSetIdleCancelThreshold(idlereset, idleback)`. Start from the measured movement that a deliberate nub
  push produces; a value around `0x40` is a candidate, not an assumption. Record the exact pair.
- `PSP_PAD_IDLE=0` leaves the default (−1, analog never cancels); `=1` sets the measured pair. The A/B is the
  idle-session run, not a preference.
- The idle test must use the nub only: no button presses, no USB/PSPLink traffic, screen on, and the console's own
  idle timeout (or a 6-minute bound if the setting is unknown). Log the elapsed time and whether the display dimmed.

### T4 design

- Sony's four-band angle/speed response is a candidate, not a core requirement. Implement it only if the linear
  axis gives visibly poor slow control in a small-movement test.
- If adopted, keep it behind `PSP_PAD_CURVE=0|1`, record the band edges and speeds, and verify the rim still
  reaches ±1.
- If not adopted, mark T4 `dropped` in the task log with the reason; that is a valid result.

### T5 design

- Re-run the 041 contract checks and the 042 sampling checks; the calibrated axes must still read 0 at rest and
  retain their signs.
- Run `basic_shapes` and `batch_check`; the renderer gate must not move.
- Add a nub row to `memory-map.md` §3 with `[measured: PSP-1000, sprint 043]` and the values; update the Phase 8
  page's “What we already know” with the same numbers.

### Task log (append-only)

- 2026-11-21 — sprint written; all tasks `todo`, no device run

## 6. Budgets

| item | budget | measured |
|---|---|---|
| idle output | exactly 0 on both axes after calibration | |
| rim travel | reaches ±1 on each axis without a deadzone-sized hole | |
| magnitude | ≤ 1 at every sampled point | |
| diagonal/cardinal | within the recorded tolerance at the rim | |
| idle session | no dim/sleep past the timeout | |
| renderer gate | byte-identical | |

## 7. Measurements

Protocol: device only; one variable per run; control → change → control again. Every row carries its build number.

**Raw range** (five runs):

| run | build | axis | min | max | idle centre | rim radius |
|---|---|---|---|---|---|---|
| 1 | `pad_probe` | Lx | | | | |
| 1 | `pad_probe` | Ly | | | | |
| 2 | `pad_probe` | Lx/Ly | | | | |

**Deadzone A/B**:

| run | build | position | `PSP_PAD_CALIB=0` | `PSP_PAD_CALIB=1` | notes |
|---|---|---|---|---|---|
| idle | | untouched | | | |
| right rim | | full right | | | |
| up rim | | full up | | | |
| diagonal | | full up-right | | | |
| corner magnitude | | roll | | | |

**Idle**:

| run | build | input | elapsed | display dim/sleep | notes |
|---|---|---|---|---|---|
| control | `PSP_PAD_IDLE=0` | nub only | | | |
| change | `PSP_PAD_IDLE=1` | nub only | | | |

## 8. Exit criteria

- [ ] raw min/max/centre/radius per axis are recorded from at least five runs on this console
- [ ] calibrated idle reads exactly 0; the old/new drift and travel difference are quantified
- [ ] diagonal and cardinal speeds are comparable, and the magnitude never exceeds 1
- [ ] `PSP_PAD_CALIB=0` and `=1` are the only difference in the deadzone A/B
- [ ] a nub-only session longer than the idle timeout does not dim or sleep with the chosen threshold
- [ ] the response curve is adopted or `dropped` with a reason
- [ ] the 041/042 samples and the renderer gate are unchanged
- [ ] the values are written into `memory-map.md` §3 and the Phase 8 page

## 9. Result

Not run yet.

## 10. Follow-ups

- **Sprint 044 (P8-S4)** — decisions, keyboard/mouse, DS3 and the phase close.
- A runtime calibration UI (hold a button, sample the range, store the values) is a follow-up if a shipped game
  needs to run on several units; the bake-time path is enough for this console.
- If T4 is adopted, the curve edges and speeds belong in a recipe only after a second workload confirms them.
