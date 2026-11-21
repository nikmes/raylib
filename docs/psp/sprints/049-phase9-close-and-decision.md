# Sprint 049 (P9-S5) — Close route A, set budgets, decide the next route

| | |
|---|---|
| Roadmap phase | [Phase 9 — Audio](../phases/09-audio/index.md) |
| Sprint | P9-S5 |
| Branch / base commit | P9-S4 close sha (pin at start) |
| Started / closed | — / — |
| Build number(s) | `audio b5`; pspgu `b24` unchanged |
| Tasks | 0/8 done |

## 0. Handover

**Situation.** P9-S1…S4 delivered the raylib-way route: raudio enabled, a real custom backend, the `Sound`
API, and `Music`/`AudioStream`. This sprint closes route A. It does not add features: it proves clean
lifecycle, measures the budget, runs the full audio API regression, and records whether the SAS/ME routes
are worth prototyping (P9-S6) or should be deferred.

**Read before acting**, in this order:

1. `../phases/09-audio/index.md` — all exit criteria and the three routes.
2. `../sprints/045…048` §9 — the build marker, link line, backend counters, maximum sound count and
   streaming decision.
3. `../memory-map.md` §5 Phase 9, §6, §8 (decision log), §9 (traps), §11.4 (SDK/reference index).
4. `src/raudio.c:466-550` — `InitAudioDevice`/`CloseAudioDevice`: `ma_device_uninit()` must join the
   backend thread before the mutex is destroyed.
5. `pspdev/psp/sdk/include/pspthreadman.h:118-133` — `SceKernelThreadRunStatus.runClocks`;
   `pspaudio.h:171-180` — `sceAudioGetChannelRestLen`/`RestLength`.
6. The current `psp-tests/audio_smoke` — it is the regression harness; do not create a second app.

**First action.** Run the existing `audio_smoke` five times with `reset` between runs and compare the boot
line, `DONE`, channel number and counters. A single run proves nothing about leaks; five init/close cycles
and five process runs are the baseline for T1/T2.

**Hard rules.**

- **Do not change audio behavior in this sprint.** No new API, no backend semantics, no pitch/pan changes.
  If a defect appears, record it and fix it in the owning sprint; this sprint only accepts evidence.
- **A clean close means no thread and no channel.** `ma_device_uninit()` joins; the backend releases in
  `onDeviceUninit()` after the join, not before.
- **The steady-state frame loop contains no `sceAudio`/`sceSas` call.** Init/close on the main thread is
  expected; “free” means per frame.
- **Measure on the PSP-1000 only.** PPSSPP does not answer CPU or dropout questions.
- **Use `reset`, never `kill`,** between runs; keep the pspsh pipe attached; never send `exit`.
- **No “feels fine” evidence.** A 5-minute run without counters is not a soak.
- Commit messages carry the task ID: `sprint 049 T3: record the audio budget`.

**Commands.**

```sh
export PSPDEV=/home/psp/pspdev
export PATH=$PATH:$PSPDEV/bin

cd /home/psp/raylib-psp/src && make PLATFORM=PLATFORM_PSP -j8
cp libraylib.a $PSPDEV/psp/lib/libraylib.a

cd ../psp-tests/audio_smoke
make clean && make MODE=soak
( printf './audio_smoke.prx\n'; sleep 320 ) | pspsh
# after DONE: pspsh -e "reset"; repeat for the next arm
```

**Do not touch.** `src/rlgl.h`, pspgu, renderer gates, the `Sound`/`Music` semantics, and the SAS/ME
reference trees.

**Done means** §8, with a 5-minute soak, full API matrix, budgets and a recorded decision.

## 1. Objective

Route A meets every Phase 9 exit criterion on the PSP-1000: sounds and one streamed track play together at
60 fps, five minutes pass without a dropout, the frame loop is free of `sceAudio`/`sceSas`, clean
init/close/re-init leaks no thread or channel, and the SAS/ME decision is recorded with the numbers behind it
(or the reason it is deferred).

## 2. Why now

This is the phase gate. Without a clean close and budgets, route A is a demo, not an implementation. The
decision also determines whether P9-S6/S7 exist: if route A is sufficient, the correct outcome is to defer
the ME work, not to build it because the page mentions it.

## 3. Scope

**In:** lifecycle/leak tests; 5-minute soak; CPU/heap/`.text` budgets; full audio API regression; renderer
gates; route decision; documentation write-back.

**Out:** SAS/ME implementation (P9-S6/S7), new audio features, decoder changes, and any renderer change.

## 4. Preconditions

- [ ] P9-S4 closed; music + 8 sounds at 60 fps; the streaming decision is recorded
- [ ] `reset` was run; partition 2 free reads 25,149,440 bytes
- [ ] The P9-S4 build is archived with its boot line and gate hashes
- [ ] A 5-minute run can be left unattended; the app logs `DONE` and sleeps
- [ ] `PSP_HEAP_SIZE_KB` is declared; builds use `PSP_LARGE_MEMORY=0`
- [ ] The full audio API matrix from `src/raylib.h:1682-1755` is available in the test

## 5. Tasks

Status vocabulary — use exactly these words, in backticks: `todo` · `doing` · `done` · `blocked` · `dropped`.

| ID | task | status | verification | evidence |
|---|---|---|---|---|
| T1 | **Lifecycle**: 10× `InitAudioDevice`/`CloseAudioDevice` in one process; 5× process start/stop with `reset` | `todo` | no leaked channel/thread; free bytes return to the same value; no crash | — |
| T2 | **Clean close**: `CloseAudioDevice()` joins the backend thread; `onDeviceUninit()` releases the channel | `todo` | thread list and channel reservation before/after; no audio after close | — |
| T3 | **5-minute soak**: music + 8 sounds, counters each second | `todo` | zero underruns; rest length never 0 at output; frame p95 | — |
| T4 | **Budgets**: game frame, audio thread `runClocks`, heap, `.text`, stack | `todo` | numbers in §7; no per-frame allocation | — |
| T5 | **Full API regression**: every function in `src/raylib.h:1682-1755` is exercised or explicitly marked unsupported | `todo` | matrix complete; all supported calls pass | — |
| T6 | **Renderer regression**: `batch_check` and 018–021 gates | `todo` | byte-identical captures | — |
| T7 | **Route decision**: compare route A against the Phase 9 budgets; if short, authorize P9-S6 with the precise deficit; if sufficient, defer B/C with numbers | `todo` | decision text with the metric, threshold and result | — |
| T8 | **Write-back**: memory map §2/§3/§8/§9, Phase 9 page, environment build/link line | `todo` | docs updated; link line includes `-lpspaudio -lpthread -latomic`; `git diff -- examples/` empty | — |

### Task log (append-only)

- 2026-11-21 — sprint drafted; not started

## 6. Budgets

These are the numbers the phase will carry. Set them from P9-S4, then hold route A to them.

| budget | limit | measured | tag |
|---|---|---|---|
| frame time | ≤ 16.7 ms (60 fps) | | |
| game-thread audio work | ~0 ms in the steady frame | | |
| audio thread CPU | ≤ 2 ms/frame target | | |
| heap | no per-frame allocation; record working set | | |
| dropout | 0 in 5 minutes | | |
| channels/threads | 1 channel, 1 audio thread, 0 leaked | | |

## 7. Measurements

| run | build | workload | metric | value | notes |
|---|---|---|---|---|---|
| lifecycle | `audio b5` | 10 init/close | free bytes, threads, channels | | |
| soak | `audio b5` | music + 8 sounds, 5 min | underruns, frame p95, runClocks | | |
| API | `audio b5` | full matrix | pass/fail per function | | |
| gates | `audio b5` | `batch_check` + 018–021 | hashes | | must match control |

## 8. Exit criteria

- [ ] all Phase 9 exit criteria for route A are met, with numbers in §7
- [ ] five minutes without dropout or underrun, counters confirming
- [ ] clean init/close/re-init leaves no thread or channel
- [ ] the steady-state frame loop has no `sceAudio`/`sceSas` call
- [ ] every supported raylib audio function in the matrix passes; unsupported ones are documented
- [ ] `batch_check` and 018–021 gates are unchanged
- [ ] the SAS/ME route decision is recorded: defer with numbers, or authorize P9-S6 with a precise deficit
- [ ] facts and build/link lines are written back to the memory map and Phase 9 page

## 9. Result

Not started.

## 10. Follow-ups

- If T7 authorizes P9-S6, open it with the deficit in the handover; do not re-run the route-A baseline.
- If route A is sufficient, mark P9-S6/S7 `dropped` with the numbers from §7 and close Phase 9.
- If the API matrix finds an unsupported function, name the phase that owns it (audio or another module);
  do not silently leave it out.
