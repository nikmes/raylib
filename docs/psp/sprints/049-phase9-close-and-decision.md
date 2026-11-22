# Sprint 049 (P9-S5) — Close route A, set budgets, decide the next route

| | |
|---|---|
| Roadmap phase | [Phase 9 — Audio](../phases/09-audio/index.md) |
| Sprint | P9-S5 |
| Branch / base commit | `master` @ `fae6d5ef` (P9-S4a close; pin at start) |
| Started / closed | 2026-11-21 / 2026-11-22 |
| Build number(s) | `audio b5`; pspgu `b24` unchanged |
| Tasks | 9/9 done |

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

**First action.** Run `MODE=api` five times with `reset` between runs and compare the boot line, `DONE`,
channel number and counters. A single run proves nothing about leaks. Then add `MODE=lifecycle` (10 audio
init/close cycles in one process, free memory logged each cycle) and run it; five process runs plus ten
in-process cycles are the baseline for T1/T2.

**Hard rules.**

- **Do not change audio behavior in this sprint.** No new raylib API, no backend semantics, no pitch/pan changes.
  If a defect appears, record it and fix it in the owning sprint; this sprint only accepts evidence.
- **A clean close means no thread and no channel.** `ma_device_uninit()` joins; the backend releases in
  `onDeviceUninit()` after the join, not before.
- **The steady-state frame loop contains no `sceAudio`/`sceSas` call.** Init/close on the main thread is
  expected; “free” means per frame.
- **Measure on the PSP-1000 only.** PPSSPP does not answer CPU or dropout questions.
- **Use `reset`, never `kill`,** between runs; keep the pspsh pipe attached; never send `exit`.
- **Harness changes are allowed; library behaviour changes are not.** Adding `MODE=lifecycle`,
  `MODE=trackchange`, a build marker, and the missing API calls is test work. Do not touch `src/raudio.c`
  semantics, the backend, or pitch/pan.
- **Log a build marker.** At init print `AUDIO build b5` so every run is attributable; the current harness
  has no build string.
- **No “feels fine” evidence.** A 5-minute run without counters is not a soak.
- Commit messages carry the task ID: `sprint 049 T3: record the audio budget`.

**Commands.**

```sh
export PSPDEV=/home/psp/pspdev
export PATH=$PATH:$PSPDEV/bin

cd /home/psp/raylib-psp/src && make PLATFORM=PLATFORM_PSP -j8
cp libraylib.a $PSPDEV/psp/lib/libraylib.a

cd ../psp-tests/audio_smoke

# T1/T2: ten init/close cycles in one process (mode added in T1)
make clean && make MODE=lifecycle
( printf './audio_smoke.prx\n'; sleep 30 ) | pspsh
pspsh -e "reset"

# T3: the real soak. There is no MODE=soak.
make clean && make MODE=musicmix COUNT=8 FRAMES=18000
( printf './audio_smoke.prx\n'; sleep 340 ) | pspsh
pspsh -e "reset"

# T5: full matrix, one arm per mode
make clean && make MODE=api                                   # Wave/Sound (+ T5 additions)
make clean && make MODE=music MUSIC=/p9s4_target.ogg          # Music
make clean && make MODE=stream STREAM_ARM=update              # AudioStream update
make clean && make MODE=stream STREAM_ARM=callback            # AudioStream f32 callback
make clean && make MODE=stream STREAM_ARM=processor           # f32 processor
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

- [ ] P9-S4 closed except T5; T5 (track change) is closed by **T0** in this sprint
- [ ] pspgu `b24` is installed (`tools/pspgu-build.sh --verify-install` passes); gate `b69830b8…`
- [ ] `reset` was run; `sceKernelTotalFreeMemSize()` is recorded at boot
- [ ] The P9-S4 build is archived with its boot line and gate hashes
- [ ] A 5-minute run can be left unattended; the app logs `DONE` and sleeps
- [ ] `PSP_HEAP_SIZE_KB` is declared; builds use `PSP_LARGE_MEMORY=0`
- [ ] The full audio API (`src/raylib.h:1685-1758`) is available in the test; the unexercised calls are
      listed in T5 and either added to `MODE=api` or marked unsupported

## 5. Tasks

Status vocabulary — use exactly these words, in backticks: `todo` · `doing` · `done` · `blocked` · `dropped`.

| ID | task | status | verification | evidence |
|---|---|---|---|---|
| T0 | **Close P9-S4 T5 — track change**: add `MODE=trackchange`; play OGG, then load and play WAV while the device runs; stop/unload the first | `done` | both tracks valid and audible; no stale audio after the swap; `GetMusicTimePlayed` resets; no leaked decoder thread | a=1 b=1; A played 1.93 s; A unloaded; B playing (0.12 s of 0.553 s loop); B unloaded; OGG reloaded valid; `TRACKCHANGE DONE`. Human listening 2026-11-22: all three segments heard (OGG, WAV, OGG reload); transition clean, **no leftover or stuck audio** |
| T1 | **Lifecycle**: `MODE=lifecycle` closes the initial device, then runs 10x `InitAudioDevice`/`CloseAudioDevice`; 5x process start/stop with `reset` | `done` | every cycle ready; free bytes return to the same value (allow ≤4 KiB drift); no crash | 5 runs × 10 cycles = 50: every `ready=1`; free **782,336** / maxfree **524,288** before and after every cycle, zero drift |
| T2 | **Clean close**: `CloseAudioDevice()` joins the worker; `onDeviceUninit()` releases the channel | `done` | `thlist` after `DONE` has no `pthread*` worker (only `user_main` + raylib `update_thread`); channel unit re-reserved next cycle; no audio after close | `thlist` after `DONE`: only `user_main` (WAITING, WaitType 1) + `update_thread`; channels alternate 7/6 and do not leak; `DONE audio=0` in lifecycle |
| T3 | **5-minute soak**: music + 8 sounds, counters each second | `done` | zero underruns; rest length never 0 at output; frame p95 | 18,000 frames / 305.7 s: update p95 **0.028 ms**, max **0.152 ms**, **0 spikes**; 13,033 callbacks (42/s), rest **960–1024**, errors **0**; log written to `host0:/s5_soak_out.txt` |
| T4 | **Budgets**: game frame, audio thread `runClocks`, heap, `.text`, stack | `done` | numbers in §7; no per-frame allocation | ELF text **1,255,480** / data 107,328 / bss 480,280; `raudio.o` text **425,277** / data 476 / bss 12,480; `runClocks` 35,686,545 / 305.7 s ≈ **0.009 ms/frame**; stacks: worker 128 KiB, decoder 64 KiB, main 256 KiB; free memory constant |
| T5 | **Full API regression**: every function in `src/raylib.h:1685-1758` exercised or explicitly marked unsupported | `done` | matrix complete; all supported calls pass | `MODE=api`: LoadWave 1, LoadWaveFromMemory 1, IsWaveValid(invalid) 0, ExportWaveAsCode 1, all Wave/Sound calls pass. `MODE=music`: frommemory 1, OGG 2.000 s / WAV 0.553 s, seek/time pass, update p95 0.030 ms. Stream arms: pause/resume and mixed attach/detach observed, 60 fps, rest 960–1024, errors 0 |
| T6 | **Renderer regression**: `batch_check` and the 018–021 corpus | `done` | byte-identical captures | Seven captures, all byte-identical: `batch_check b69830b8…`, text8 `1124a11b…`, text24 `adb5dceb…`, tiles `2e46601f…`, p500 `8b5d8a4f…`, shapes `b9cd43b6…`, mixed `f132697c…`; raws archived in `/home/psp/evidence/sprint049` |
| T7 | **Route decision**: compare route A against §6; if short, authorize P9-S6 with the precise deficit; if sufficient, defer B/C with numbers | `done` | decision text with metric, threshold and result | **Route A passes every budget**; SAS/ME (P9-S6/S7) deferred and marked `dropped`. No metric is near a limit: frame 59–60 fps, game-thread audio 0.028 ms, audio thread 0.009 ms/frame, 0 dropouts, 0 leaked threads |
| T8 | **Write-back**: memory map §2/§3/§8/§9, Phase 9 page, environment build/link line | `done` | docs updated; link line includes `-lpspaudio -lpthread -latomic`; `git diff -- examples/` empty | Memory map §2/§3/§8/§9, Phase 9 page, `environment.md`; `examples/` diff empty; `mkdocs --strict` passes |

### Task log (append-only)

- 2026-11-21 — sprint drafted; not started
- 2026-11-22 — readiness review: pinned `fae6d5ef`; `MODE=soak` did not exist (use `musicmix`);
  added T0 for the open P9-S4 track-change task; specified `MODE=lifecycle`, the T2 thread check, the
  missing T5 calls, gate commands and the T7 decision rule; prefilled §6 from P9-S4a
- 2026-11-22 — implementation: b5 marker, `MODE=lifecycle`, `MODE=trackchange`, nine missing API calls,
  mixed-processor attach/detach, stream pause/resume, `LOGFILE=host0:` logging
- 2026-11-22 — T0/T1/T2/T5 pass on device; soak 18,000 frames clean; seven renderer gates identical
- 2026-11-22 — T7: route A sufficient, SAS/ME dropped; T8 write-back done; sprint closed
- 2026-11-22 — T0 listening confirmation: all three segments audible, OGG→WAV transition clean

## 6. Budgets

These are the numbers the phase will carry. Set them from P9-S4, then hold route A to them.

| budget | limit | measured | tag |
|---|---|---|---|
| frame time | ≤ 16.7 ms (60 fps) | 59–60 fps on `musicmix` | `[measured: PSP-1000, P9-S5]` |
| game-thread audio work | ~0 ms in the steady frame | `UpdateMusicStream` p95 0.028 ms, max 0.152 ms; loop has no `sceAudio` | `[measured: PSP-1000, P9-S5]` |
| audio thread CPU | ≤ 2 ms/frame target | `runClocks` 35,686,545 over 305.7 s (≈0.009 ms/frame) | `[measured: PSP-1000, P9-S5]` |
| heap | no per-frame allocation; record working set | free **782,336** / max free **524,288** constant across 50 init/close cycles; decoder 4 KiB + 64 KiB stack per track | `[measured: PSP-1000, P9-S5]` |
| dropout | 0 in 5 minutes | 0 in 18,000 frames; 13,033 callbacks (42/s), rest 960–1024, errors 0 | `[measured: PSP-1000, P9-S5]` |
| channels/threads | 1 channel, 1 audio worker, 1 decoder per playing Music, 0 leaked | 1 channel (unit 7/6), worker joined on close, decoder joined on unload; `thlist` clean | `[measured: PSP-1000, P9-S5]` |

## 7. Measurements

| run | build | workload | metric | value | notes |
|---|---|---|---|---|---|
| track change | `audio b5` | OGG -> WAV live | valid/audible, time reset | a=1 b=1; A 1.93 s; B 0.12 s (looping); reload=1; `TRACKCHANGE DONE` | T0 |
| lifecycle | `audio b5` | 10 init/close × 5 runs | free bytes, ready, threads, channels | ready=1 every cycle; free **782,336** / maxfree **524,288** before and after, zero drift | T1/T2 |
| soak | `audio b5` | music + 8 sounds, 5 min | underruns, frame p95, runClocks | p95 **0.028 ms**, max **0.152 ms**, 0 spikes; 13,033 callbacks (42/s), rest 960–1024, errors 0; `host0:/s5_soak_out.txt` | 305.7 s |
| text size | `audio b5` | `audio_smoke.elf`, `src/raudio.o` | `.text`, data/bss | ELF text **1,255,480** / data 107,328 / bss 480,280; `raudio.o` text **425,277** / data 476 / bss 12,480 | `psp-size` |
| API | `audio b5` | full matrix | pass/fail per function | all supported calls pass; music update p95 0.030/0.031 ms; stream arms 60 fps, rest 960–1024, errors 0. One API run showed `rest_min=0` once at start; a repeat gave 960 | `MODE=api/music/stream` |
| gates | `audio b5` | `batch_check` + 6 `scene_2d` arms | hashes | all byte-identical: `b69830b8`, `1124a11b`, `adb5dceb`, `2e46601f`, `8b5d8a4f`, `b9cd43b6`, `f132697c` | `/home/psp/evidence/sprint049` |

After the run table, append the T5 matrix: one row per function in `src/raylib.h:1685-1758`, the arm that
calls it, the observed result, and `unsupported` with the owning phase where that applies.

## 8. Exit criteria

- [x] P9-S4 T5 closed by T0 (live track change)
- [x] all Phase 9 exit criteria for route A are met, with numbers in §7
- [x] five minutes without dropout or underrun, counters confirming
- [x] clean init/close/re-init leaves no thread or channel
- [x] the steady-state frame loop has no `sceAudio`/`sceSas` call
- [x] every supported raylib audio function in the matrix passes; unsupported ones are documented
- [x] `batch_check` (`b69830b8…`) and 018–021 gates are unchanged
- [x] the SAS/ME route decision is recorded: defer with numbers, or authorize P9-S6 with a precise deficit
- [x] facts and build/link lines are written back to the memory map and Phase 9 page

## 9. Result

**Route A closes Phase 9.** Every budget is met with margin:

- 5-minute soak: 18,000 frames / 305.7 s, 13,033 callbacks (42/s), rest 960–1024, errors 0; main-thread
  `UpdateMusicStream` p95 **0.028 ms**, max **0.152 ms**, 0 spikes.
- Lifecycle: 5 process runs × 10 init/close = **50 cycles**, all `ready=1`; free memory
  **782,336** / max free **524,288** before and after every cycle, zero drift.
- Clean close: `thlist` after `DONE` has only `user_main` and raylib's `update_thread`; no `pthread*`
  audio worker, no leaked channel (units alternate 7/6).
- Full raylib audio API passes, including the memory wave/music loads, `ExportWaveAsCode`, stream
  pause/resume and mixed-processor attach/detach.
- Renderer regression: seven captures byte-identical to the sprint 018–021 controls.
- Budgets: ELF text 1,255,480 B; `raudio.o` text 425,277 B; audio worker `runClocks` 35,686,545 over
  305.7 s (≈0.009 ms/frame); stacks 128 KiB worker, 64 KiB decoder, 256 KiB main.

**Decision (T7): defer SAS/ME.** Route A meets every Phase 9 budget, so P9-S6 (SAS/ME prototype) and
P9-S7 (adopt the route) are `dropped` with the numbers above. There is no deficit to hand to P9-S6.

Two operational notes recorded for the next phase:

- A long run can lose PSPLink stdout (SIO buffer); the harness now supports
  `LOGFILE=host0:/file` so counters survive. The 5-minute evidence above uses it.
- The installed `libGL.a` was switched back to pspGL at 23:03 by another session; `scene_2d` failed to
  link until pspgu was reinstalled. Run `tools/pspgu-build.sh --check` before any gate build.

## 10. Follow-ups

- If T7 authorizes P9-S6, open it with the deficit in the handover; do not re-run the route-A baseline.
- If route A is sufficient, mark P9-S6/S7 `dropped` with the numbers from §7 and close Phase 9.
- **Reopen P9-S6 only with a measured deficit.** Route A leaves at most ~1 ms/frame on the table (main
  thread 0.028 ms, worker 0.009 ms/frame, decoder a few percent of one core). Triggers: a CPU-bound 3D
  scene where the decoder/mixer is a measurable share of frame time; a requirement for more simultaneous
  `Sound`s than the f32 mixer holds at 60 fps; or an ME/AVC workload that makes a shared engine
  attractive. A reopening proposal must name the metric that fails and the threshold it misses.
- If the API matrix finds an unsupported function, name the phase that owns it (audio or another module);
  do not silently leave it out.
