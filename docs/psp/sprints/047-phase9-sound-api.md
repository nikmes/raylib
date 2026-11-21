# Sprint 047 (P9-S3) — `Sound` API and mixing

| | |
|---|---|
| Roadmap phase | [Phase 9 — Audio](../phases/09-audio/index.md) |
| Sprint | P9-S3 |
| Branch / base commit | `master` @ `1dc6f361` (P9-S2 close; pin at start) |
| Started / closed | — / — |
| Build number(s) | `audio b3`; pspgu `b24` unchanged |
| Tasks | 0/8 done |

## 0. Handover

**Situation.** P9-S2 proved one `Sound` through the PSP sink: channel 7, 433 callbacks/10.03 s (43/s),
rest 960–1024, zero errors, 1 kHz tone audible, gate `cf43ce6d…`. This sprint proves the rest of raylib's
`Sound` surface through the same backend: several simultaneous sounds, pause/resume/stop, volume, pan, pitch,
`LoadSound`/`LoadSoundFromWave`, `LoadSoundAlias`, `UpdateSound`, and validity/unload behavior. No `Music`,
no decoder thread, no SAS/ME, no backend or renderer change.

**Read before acting**, in this order:

1. `../phases/09-audio/index.md` — P9-S3 and the phase exit criterion for several sounds.
2. `../sprints/046-phase9-custom-backend.md` §9 — the working sink, 128 KiB worker, counters and gate.
3. `src/raylib.h:1682-1717` — the `Sound`/`Wave` API this sprint must cover.
4. `src/raudio.c:716-745` — `SetAudioBufferPitch()` changes the data converter's rate; `:1233-1240` —
   `SetSoundPitch()`; `:958-1005` — `LoadSound`/`LoadSoundFromWave`; `:2534-2645` — the mixer.
5. `../memory-map.md` §6, §9 — one variable per run; `Sound` data is in RAM, so 32 MB is a budget.
6. `psp-tests/audio_smoke/main.c` — the S2 tone harness; extend it rather than creating a second app.
7. `psp-tests/audio_smoke/Makefile` — runtime libs are after `build.mak`; keep that order.

**First action.** Add `MODE=sound COUNT=N` to `audio_smoke`: generate deterministic s16 stereo waves at
load, create `N` sounds, start them staggered, run 600 frames, log `DONE`, sleep. Run `COUNT=1` first against
the S2 build; that is the control. Then add `COUNT=16`, and only then the volume/pan/pitch arms.

**Hard rules.**

- **The game path uses only `raylib.h`.** PSP headers and memory counters belong in the test harness.
- **`UpdateSound()` data is 32-bit float stereo.** Keep `AUDIO_DEVICE_FORMAT` f32; changing it would make
  this API lie and break the mixer.
- **No per-frame allocation.** Sounds and waves are created before the loop; `UpdateSound()` must not
  reallocate after warm-up. A flat free-memory counter is the evidence.
- **Pitch changes duration in raylib.** A higher pitch makes the sound faster and shorter; test the
  documented semantics, not a pitch-preserving stretch.
- **One variable per run.** `COUNT`, `MODE`, volume, pan, pitch and master volume are separate arms.
- **Do not touch the backend.** If a failure is in `onDeviceWrite()`, fix it in P9-S2's code with evidence;
  do not hide it in the test. Keep the two-buffer/cache path and the 128 KiB stack.
- **`LoadSound` needs one real file path.** Export the generated wave to `host0:/audio_smoke_tone.wav`, load
  it with `LoadSound`, then remove it. If `host0:` is not writable, mark that sub-check deferred and record it;
  do not invent a Memory Stick dependency.
- **The gate is `cf43ce6d…`.** `batch_check` and the 018–021 captures must stay byte-identical.
- **Stage only this sprint's paths.** Other sessions edit the tree; run `git status` before every commit.
- Commit messages carry the task ID: `sprint 047 T2: mix sixteen sounds`.

**Commands.**

```sh
export PSPDEV=/home/psp/pspdev
export PATH=$PATH:$PSPDEV/bin

cd /home/psp/raylib-psp/src && make PLATFORM=PLATFORM_PSP -j8
cp libraylib.a $PSPDEV/psp/lib/libraylib.a

cd ../psp-tests/audio_smoke
make clean && make MODE=sound COUNT=16
( printf './audio_smoke.prx\n'; sleep 24 ) | pspsh
# after DONE: pspsh -e "reset"
```

**Do not touch.** `src/rlgl.h`, pspgu, renderer gates, `Music`, `AudioStream`, SAS/ME, and the backend's
blocking output path.

**Done means** §8, with an API matrix, counters, a repeat run and audible confirmation.

## 1. Objective

Every `Sound`/`Wave` function in `raylib.h:1682-1717` behaves through the PSP backend as it does on desktop:
one sound plays with correct stop/pause/resume, volume/pan/pitch/master changes are audible and measurable,
`LoadSound`, `LoadSoundFromWave` and `LoadSoundAlias` work, `UpdateSound()` accepts f32 stereo frames, and
16 simultaneous sounds run at 60 fps without an underrun.

## 2. Why now

P9-S2 proves the sink, not the API. `Sound` is the API a game uses most, and its pitch/pan/volume paths run
inside the audio thread; measuring them now separates mixer cost from `Music` streaming in P9-S4.

## 3. Scope

**In:** `psp-tests/audio_smoke` sound modes; the `Sound`/`Wave` API matrix; audio-thread/frame-time
measurements; heap and sound-count limits; a one-file `LoadSound` path on `host0:`.

**Out:** `Music`/`AudioStream` (P9-S4), SAS/ME (P9-S6/S7), stream processors, backend changes, renderer changes.

## 4. Preconditions

- [ ] P9-S2 closed: channel 7, 433 callbacks/43 per s, rest 960–1024, errors 0, tone audible
- [ ] `reset` was run; partition 2 free reads 25,149,440 bytes
- [ ] `audio_smoke` builds with `PSP_LARGE_MEMORY=0` and `PSP_HEAP_SIZE_KB=1024`
- [ ] `host0:` is writable for the `LoadSound` file check (or the deferral is recorded)
- [ ] A deterministic wave generator exists; no Memory Stick asset is required
- [ ] The 018–021 gate hashes and `batch_check_s2.raw` (`cf43ce6d…`) are archived

## 5. Tasks

Status vocabulary — use exactly these words, in backticks: `todo` · `doing` · `done` · `blocked` · `dropped`.

| ID | task | status | verification | evidence |
|---|---|---|---|---|
| T1 | **API matrix**: `LoadSound` (exported WAV), `LoadSoundFromWave`, `IsSoundValid`, `PlaySound`, `StopSound`, `PauseSound`, `ResumeSound`, `IsSoundPlaying`, `UnloadSound`, `SetMasterVolume`/`GetMasterVolume` | `todo` | each call observed; invalid sound/path returns false or does nothing; no crash | — |
| T2 | **Mix `COUNT` sounds**: 1/4/16/32 staggered sounds, same wave and different waves | `todo` | 16 sounds at 60 fps; record the count where frame time leaves budget | — |
| T3 | **Volume/pan/pitch**: `SetSoundVolume`, `SetSoundPan`, `SetSoundPitch`, master volume | `todo` | audible and measured change; pitch changes duration; pan is left/right | — |
| T4 | **Wave paths**: `LoadWaveSamples`, `WaveCopy`, `WaveCrop`, `WaveFormat`, `LoadSoundAlias`, `UnloadSoundAlias` | `todo` | alias shares data; unloading the alias does not free the source; copy/crop/format round-trip | — |
| T5 | **`UpdateSound`**: feed f32 stereo frames into a playing sound; verify continuity and no reallocation after the first call | `todo` | audio stays continuous; free-memory counter flat after warm-up | — |
| T6 | **Measure**: audio-thread `runClocks`, rest length, errors, frame time for each arm; control = P9-S2 `COUNT=1` | `todo` | numbers in §7; no underrun; repeat within the project's 0.1 % threshold where applicable | — |
| T7 | **Heap/limits**: free bytes before/after 32 sounds via `sceKernelTotalFreeMemSize`/`sceKernelMaxFreeMemSize`; define the maximum count that fits 32 MB | `todo` | documented count and free-bytes figure | — |
| T8 | **Regression**: `batch_check` and the 018–021 gates; `git diff -- examples/` empty | `todo` | gate `cf43ce6d…` byte-identical | — |

### Task log (append-only)

- 2026-11-21 — sprint drafted; not started
- 2026-11-21 — readiness review: S2 baseline, existing harness, LoadSound path, UpdateSound format and heap method added

## 6. Budgets

| budget | limit | measured | tag |
|---|---|---|---|
| frame time | ≤ 16.7 ms (60 fps) with 16 sounds | | |
| audio thread CPU | record; target ≤ 2 ms/frame at 16 sounds | | |
| heap | 32 sounds fit; record free bytes | | |
| per-frame allocation | none after warm-up | | |
| rest samples | never 0 at the output call | | |

## 7. Measurements

Audio has no screenshot evidence. Record build marker, boot log, counters and audible confirmation.

| run | build | workload | metric | value | notes |
|---|---|---|---|---|---|
| control | `audio b2` | 1 tone, 600 frames | frame, runClocks, rest, errors | 433 callbacks, 43/s, 960–1024, 0 | S2 |
| mix 4 | `audio b3` | 4 sounds | frame, runClocks, rest | | |
| mix 16 | `audio b3` | 16 sounds | frame, runClocks, rest, free heap | | |
| mix 32 | `audio b3` | 32 sounds | frame, runClocks, rest, free heap | | |
| pitch | `audio b3` | 1 sound, pitch 0.5/2.0 | duration, runClocks | | expected duration change |
| gate | `audio b3` | `batch_check` | hash | `cf43ce6d…` | unchanged |

## 8. Exit criteria

- [ ] every `Sound`/`Wave` function in the matrix is exercised with evidence
- [ ] 16 simultaneous sounds run at 60 fps with no underrun
- [ ] volume, pan, pitch and master volume are audible/measured; pitch duration matches raylib
- [ ] `LoadSound` works from a `host0:` WAV, and `LoadSoundAlias` shares data without freeing the source
- [ ] `UpdateSound()` accepts f32 stereo and does not allocate per frame
- [ ] the maximum sound count that fits 32 MB is recorded
- [ ] `batch_check` stays `cf43ce6d…`; the 018–021 gates are unchanged
- [ ] the API matrix and sound-count facts are written back to the Phase 9 page

## 9. Result

Not started.

## 10. Follow-ups

- P9-S4: `Music` streaming and the decoder/reader decision.
- If 32 sounds exceed the heap, record the working set and move the limit into Phase 9's exit criteria.
- If pitch resampling dominates the audio thread, measure it separately before considering a fixed-point
  resampler; do not change semantics without an A/B.
- If `host0:` is not writable, keep `LoadSound` as a documented deferral; do not substitute a Memory Stick
  asset into this sprint.
