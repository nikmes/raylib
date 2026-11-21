# Sprint 059 (P11-S8) — Startup, power, teardown and close

| | |
|---|---|
| Roadmap phase | [Phase 11 — General optimisation outside the renderer](../phases/11-general-optimisation.md) |
| Sprint | P11-S8 |
| Branch / base commit | `master` @ `e307fe05` (docs; no Phase 11 code yet) |
| Started / closed | 2026-11-21 (written) / — |
| Build number(s) | pspgu `b24`; the phase's chosen library flags/config; numbers assigned when the sprint opens |
| Tasks | 0/8 done |

## 0. Handover

**Situation.** Sprints 052–058 measure and change the steady frame, the load path, the heap and the VFPU. What
is left is the boot path, the clock, teardown, suspend/resume and the phase close. The boot line now identifies
the build, but the time from module load to the first presented frame is not split, and no sprint owns the
`scePowerSetClockFrequency`/teardown/suspend checks. This sprint measures the remaining startup and power costs,
then produces the phase's final profile and exit table.

**Read before acting**, in this order:

1. `../memory-map.md` §5 Phase 11 — the exit criteria; §3 — the partition/heap facts; §6 — the measurement
   protocol; §9 — the `sceKernelExitGame`/PSPink trap and the reset loop.
2. `../phases/11-general-optimisation.md` §0 and §Exit criteria — the full list this sprint closes.
3. `../sprints/052-phase11-measurement-harness.md` §5 — the boot line and zones; `057` §5 — the heap/stacks;
   `058` §5 — the load path.
4. `src/rcore.c:4250-4400` — `InitWindow`, `CloseWindow` and the platform init/finalize order;
   `src/platforms/rcore_psp.c:586-765` — `InitPlatform`; `:765-800` — `ClosePlatform` and the
   `sceKernelExitGame` call.
5. `src/rcore.c` — `InitAudioDevice`/`CloseAudioDevice` (phase 9 state), `LoadFontDefault`, the default
   texture; `src/raudio.c` — the audio init path.
6. `$PSPDEV/psp/sdk/include/psppower.h` — `scePowerSetClockFrequency`; the PSP default clock and the 333 MHz
   A/B; `pspthreadman.h` — `sceKernelExitGame`, thread termination.

**First action.** Add the boot split to the sprint 052 boot line: host timestamp at launch, in-app timestamp at
`main`, after `InitPlatform`, after the first asset load, after `InitAudioDevice`, and at the first presented
frame. Run it once with the current build; the split decides which deferral is worth trying.

**Hard rules.**

- **Clock is a reported variable, never a design premise.** A 333 MHz win is reported with the battery/heat cost;
  it does not become the default silently.
- **Do not call `sceKernelExitGame` in a measurement app.** It drops the PSPLink session and the console to the
  XMB. Log `DONE` and sleep; `reset` from the host.
- **Teardown is tested in a separate build.** The normal samples stay resident; a `teardown_probe` may call
  `CloseWindow`/`CloseAudioDevice` twice and then exit, but it is not the measurement build.
- **Suspend/resume is a validity check, not a benchmark.** The goal is that pools, handles and pointers survive;
  no per-frame work is lost or repeated.
- **The final profile uses the chosen arms.** Do not mix flags, heap declarations or VFPU settings between rows.
- **The phase closes only when every exit criterion has a row or a written deferral.** A missing criterion is a
  blocker, not a footnote.
- **Stage only this sprint's paths.** `git status` before every commit; commit messages carry the task ID.

**Commands.**

```sh
export PSPDEV=/home/psp/pspdev
export PATH=$PSPDEV/bin:$PATH
set -e

cd /home/psp/raylib-psp
mkdir -p /home/psp/evidence/sprint059

# Boot split: host timestamp around the launch; in-app zones for main/init/load/audio/first frame.
( cd psp-tests/boot_probe && make clean && make PSP_LARGE_MEMORY=0 \
    && cp boot_probe.prx /tmp/p11s9/app.prx )
date +%s.%N > /tmp/p11s9/host-start
( cd /tmp/p11s9
  timeout 20 pspsh -e "reset"; sleep 6
  ( printf './app.prx\n'; sleep 60 ) | timeout 80 pspsh
  date +%s.%N > /tmp/p11s9/host-end )

# Clock arms: 222 (default) and 333; same workload, same build otherwise.
( cd psp-tests/boot_probe && make clean && make PSP_LARGE_MEMORY=0 CLOCK_MHZ=222 \
    && cp boot_probe.prx /tmp/p11s9/app.prx )
# run; then CLOCK_MHZ=333, run; record battery/heat notes

# Teardown: a separate probe that closes twice and exits; reset from the host afterwards.
( cd psp-tests/teardown_probe && make clean && make PSP_LARGE_MEMORY=0 \
    && cp teardown_probe.prx /tmp/p11s9/app.prx )
# run; the host `reset` ends the session; never send `exit`
```

**Do not touch.** The steady-frame optimisations from 053–058, the renderer, pspgu, the audio backend's mixing
or the input mapping. This sprint adds startup/power/teardown instrumentation and closes the phase.

**Done means** §8, with the boot split, the clock table, the teardown/suspend results, the final profile and the
phase exit table.

## 1. Objective

Six falsifiable results:

1. The boot path is split into module load (host timestamp), `main`, `InitPlatform`, asset load, audio init and
   first presented frame; each segment has a time and the sum is reconciled with the total.
2. Deferring an optional subsystem until first use is tested; the boot and first-frame costs are recorded, or the
   deferral is rejected with the reason.
3. 222 MHz and 333 MHz have frame/load rows on the same workload; the battery/heat observation is recorded as a
   reported variable, not a default change.
4. Teardown of the chosen build has no double free or double `Terminate`; a `reset`-driven loop survives repeated
   runs.
5. Suspend/resume leaves pools, handles and pointers valid; no per-frame work is lost.
6. The final full-frame/full-load profile has a named budget table with a residual; every Phase 11 exit criterion
   is either met with a row or explicitly deferred.

## 2. Why now

This is the phase close. Sprints 052–058 produce the frame, load, heap, VFPU and I/O numbers; this sprint
measures the remaining startup/power/teardown costs and assembles the evidence. It advances the phase's final
exit criteria and produces the report that phase 12 will consume.

## 3. Scope

**In:** `src/platforms/rcore_psp.c` (boot split, clock switch, teardown/suspend hooks), `src/rcore.c`
(`InitWindow`/`CloseWindow` ordering only if needed), the boot line, a `psp-tests/boot_probe` and
`psp-tests/teardown_probe`, the final profile tables and the phase close.

**Out:** steady-frame changes, renderer/pspgu, audio mixing, input mapping, new features. If a final profile
finds a regression, it is a follow-up to the owning sprint, not a new optimisation here.

## 4. Preconditions

- [ ] Sprints 052–058 are closed or their remaining items are explicitly carried
- [ ] The chosen flags, config and heap declaration are the build default and reported at boot
- [ ] The boot line and zones are in place; the capture gate is archived
- [ ] A 30-minute soak harness exists (057)
- [ ] The phase 13 tier-A corpus (or the five phase 1 samples) is available for parity
- [ ] `pspsh` answers; `reset` leaves partition 2 at 25,149,440 bytes
- [ ] `git status` understood; only this sprint's paths will be staged

## 5. Tasks

Status vocabulary: `todo` · `doing` · `done` · `blocked` · `dropped`.

| ID | task | status | verification | evidence |
|---|---|---|---|---|
| T1 | **Boot split**: host launch timestamp; `main`, `InitPlatform`, asset load, audio init and first presented frame in-app; reconcile the sum | `todo` | each segment has a median over at least three runs; the total matches the host timestamp | — |
| T2 | **Defer optional subsystems**: move font/audio/other optional init to first use; measure boot and first-frame cost | `todo` | the deferred build boots; the first-use cost is recorded; the deferral is kept or rejected | — |
| T3 | **Clock A/B**: 222 vs 333 MHz on the same workload; record frame/load and battery/heat notes | `todo` | both arms run; the clock is in the boot line; the default is unchanged unless explicitly decided | — |
| T4 | **Teardown**: close window/audio twice; no double free or double `Terminate`; repeated reset loop | `todo` | 10 runs survive; no crash at close; the log names the teardown order | — |
| T5 | **Suspend/resume**: HOME/suspend and resume; pools/handles/pointers valid; no lost per-frame work | `todo` | a resumed run continues and logs the same state; no stale handle | — |
| T6 | **Final profile**: full frame and load with the chosen arms; budget table with update/draw/swap/load/residual | `todo` | every zone row has a source and a residual; counters owned-zero | — |
| T7 | **Corpus parity and soak**: phase 13 tier-A (or five samples) captures; 30-minute soak; guards zero | `todo` | captures identical; free/largest stable; guards pass | — |
| T8 | **Phase close**: write the exit table, update memory-map §3/§5/§9 and the phase page, archive `SHA256SUMS` | `todo` | every exit criterion has a row or a deferral; docs build clean | — |

### T1 design

- The host timestamp covers module load and the PSP loader; the in-app timestamps cover `main`,
  `InitPlatform`, the first asset load, `InitAudioDevice` (if phase 9 is in) and the first `EndDrawing` that
  presents a frame.
- Three runs at least; report median and p95. The sum of the in-app segments must fit inside the host total;
  the loader/boot segment is the difference, named explicitly.
- Use `sceKernelGetSystemTimeWide()` for the in-app segments; do not use `GetTime()` (it is the 055 boundary).

### T2 design

- Candidates: the default font, audio init, a large asset load, optional format registration. Move one at a
  time; a subsystem that is needed on the first frame cannot be deferred.
- The deferral must not change the public API or the first-frame output. If the first `DrawText`/`PlaySound`
  pays the init, record that cost; a boot win that moves the cost into the first frame is not a win unless the
  first frame has slack.
- Keep the default font available for `DrawText`; a deferral that breaks the default font path is a reject.

### T3 design

- The current code does not set the clock; the PSP default is the 222 MHz arm. Add a `CLOCK_MHZ` switch in the
  test/probe and, if adopted, `scePowerSetClockFrequency(333,333,166)` in `InitPlatform()`.
- The 333 arm changes both CPU and bus; record the frame/load and the observation (heat, battery, fan if
  available). Do not make 333 the default without the user's decision, and do not compare a 333 arm against a
  222 control.
- The boot line must report the clock so a later reader can attribute the number.

### T4 design

- `ClosePlatform()` calls `sceKernelExitGame()`; that is why the measurement samples stay resident. The
  teardown probe is a separate PRX that calls `CloseWindow()` and, if audio is enabled, `CloseAudioDevice()`
  twice, then logs `DONE` and sleeps; the host `reset` ends the session.
- Check the teardown order: audio, assets, window, platform, timer. A double free is a crash or a log; a double
  `Terminate` is a hang. Record both.
- Run the reset loop 10 times; a leak or a failed second run is a result.

### T5 design

- Use the PSP's suspend/resume (HOME) with the app running; on resume, verify the frame counter, the heap map
  and the open handles. A pointer into volatile memory (if 057 implemented it) must be restored or the app must
  fail visibly.
- Do not count the suspend gap in the frame time; record it separately. A resumed frame that repeats work is a
  bug; one that drops it is acceptable if the state is consistent.

### T6 design

- Use the chosen flags/config/heap/VFPU/loader arms. The budget table has rows for update, draw, swap/GE wait,
  load, decode, audio (if in), and the residual. Every row names the counter and the build.
- The counters guard reports owned doubles/allocations/opens/logs; a non-zero value is a blocker or a written
  allowlist from 055.
- The load row is a separate table; do not fold it into the frame total.

### T7 design

- Use the phase 13 tier-A captures, or the five phase 1 samples until 13 is ready. Two captures per sample,
  identical, after `DONE`/sleep, per §6. A noisy capture is neither a bug nor correctness.
- The 30-minute soak uses the chosen heap and the steady workload; log free/largest every 60 s and the frame
  median. A shrinking largest block is a leak even if the frame is stable.
- The guard must pass in the final build; an injected failure is only for proving the guard works.

### T8 design

- The phase page's exit criteria table gets one row per criterion: met with evidence, deferred with owner/date,
  or rejected with reason. The memory map's §3/§5/§9 gets the facts this phase measured; do not paste the sprint
  tables.
- Archive the final library, PRX, logs, captures and `SHA256SUMS`; the phase 12 release gate consumes this.
- Run `mkdocs build --strict` and leave the site directory out of the commit.

### Task log (append-only)

- 2026-11-21 — sprint written; all tasks `todo`, no device run

## 6. Budgets

| budget | limit | measured | tag |
|---|---|---|---|
| boot | split and reconciled; no unexplained segment | | |
| frame | chosen arms; no regression against 052–058 | | |
| load | separate table; no frame stall | | |
| heap | stable over 30 minutes | | |
| clock | 222 vs 333 reported; default unchanged unless decided | | |
| teardown | 10 reset runs survive | | |
| correctness | tier-A/five captures identical | | |

## 7. Measurements

Protocol: device only; `reset` before every run; one variable per arm. Every row carries the boot id and clock.

**Boot split:**

| segment | run 1 | run 2 | run 3 | median | p95 | notes |
|---|---|---|---|---|---|---|
| host launch → `main` | | | | | | |
| `InitPlatform` | | | | | | |
| first asset load | | | | | | |
| audio init | | | | | | |
| first presented frame | | | | | | |
| total | | | | | | |

**Clock:**

| clock | workload | frame median | load median | battery/heat | notes |
|---|---|---|---|---|---|
| 222 MHz | | | | | |
| 333 MHz | | | | | |

**Final profile:**

| zone | median | p95 | source | notes |
|---|---|---|---|---|
| update | | | | |
| draw | | | | |
| swap/GE wait | | | | |
| load | | | | |
| residual | | | | |

**Teardown/suspend/soak:**

| test | result | notes |
|---|---|---|
| close twice | | |
| 10 reset runs | | |
| suspend/resume | | |
| soak 30 min | | |

## 8. Exit criteria

- [ ] the boot split has at least three runs and a reconciled total
- [ ] the optional-subsystem deferral is kept or rejected with the measured first-use cost
- [ ] the clock A/B is reported and the default is explicit
- [ ] teardown survives double close and 10 reset runs
- [ ] suspend/resume leaves the state valid
- [ ] the final frame/load profile has a budget table and a residual
- [ ] the 30-minute soak has stable free/largest
- [ ] the phase 13 tier-A/five-sample captures are identical
- [ ] every Phase 11 exit criterion is met with evidence or explicitly deferred with an owner
- [ ] memory-map §3/§5/§9 and the phase page are updated
- [ ] the archive has `SHA256SUMS`; `mkdocs build --strict` passes

## 9. Result

Not started. Record the boot split, the clock decision, the teardown/suspend results, the final profile and the
phase exit table. The phase closes only when the last row has a verdict.

## 10. Follow-ups

- **Phase 12** consumes the final build, flags and profile; do not re-tune them there without a new A/B.
- If the clock remains 222 MHz, record the battery/heat observation as the reason, not a guess.
- If a phase 13 sample is used for parity, name the sprint and the capture path; otherwise the five-sample gate
  is the documented fallback.
