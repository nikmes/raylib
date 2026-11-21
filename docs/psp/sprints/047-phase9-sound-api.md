# Sprint 047 (P9-S3) — `Sound` API and mixing

| | |
|---|---|
| Roadmap phase | [Phase 9 — Audio](../phases/09-audio/index.md) |
| Sprint | P9-S3 |
| Branch / base commit | P9-S2 close sha (pin at start) |
| Started / closed | — / — |
| Build number(s) | `audio b3`; pspgu `b24` unchanged |
| Tasks | 0/8 done |

## 0. Handover

**Situation.** P9-S2 proved one `Sound` through the PSP sink. This sprint proves the rest of raylib's
`Sound` surface through the same path: several simultaneous sounds, pause/resume/stop, volume, pan, pitch,
`LoadSoundFromWave()`, `LoadSoundAlias()`, `UpdateSound()` and validity/unload behavior. No `Music`, no
decoder thread, no SAS/ME, no renderer change.

**Read before acting**, in this order:

1. `../phases/09-audio/index.md` — the P9-S3 entry and the phase exit criterion for several sounds.
2. `../sprints/046-phase9-custom-backend.md` §9 — the working device sink, counters and thread ID.
3. `src/raylib.h:1682-1717` — the `Sound`/`Wave` API this sprint must cover.
4. `src/raudio.c:716-745` — `SetAudioBufferPitch()` changes the data converter's rate; `:1233-1240` —
   `SetSoundPitch()`; `:958-1005` — `LoadSound`/`LoadSoundFromWave`; `:2534-2645` — the mixer.
5. `../memory-map.md` §6, §9 — one variable per run; `Sound` data is in RAM, so a 32 MB console is a budget.
6. `psp-tests/audio_smoke/main.c` — the P9-S1/S2 harness; extend it rather than making a second app.

**First action.** Add `MODE=sound COUNT=N` to `audio_smoke`: generate deterministic s16 stereo waves at
load, create `N` sounds, start them staggered, run 600 frames, log `DONE`, sleep. Before changing
`raudio.c`, run `N=1` and `N=16` against the P9-S2 build; that is the control.

**Hard rules.**

- **The game uses only `raylib.h`.** Any counter or PSP header belongs in the test harness, not in the
  game path.
- **`UpdateSound()` data is 32-bit float stereo.** Keep `AUDIO_DEVICE_FORMAT` f32; converting it would make
  this API lie and break the mixer.
- **No per-frame allocation.** Sounds and waves are created before the loop; `UpdateSound()` must not
  reallocate beyond what raylib already does.
- **Pitch changes duration in raylib.** A higher pitch makes the sound faster and shorter; test the
  documented semantics, not a pitch-preserving stretch.
- **One variable per run.** `COUNT`, `MODE` and a single volume/pan/pitch change are separate arms.
- **Do not touch the backend.** If a failure is in `onDeviceWrite()`, fix it in P9-S2's code with evidence;
  do not hide it in the test.
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
blocking call.

**Done means** §8, with an API matrix, counters and a clean repeat run.

## 1. Objective

Every `Sound`/`Wave` function in `raylib.h:1682-1717` behaves through the PSP backend as it does on desktop:
one sound plays with correct stop/pause/resume, volume/pan/pitch changes are audible and measurable,
`LoadSoundFromWave()` and `LoadSoundAlias()` work, `UpdateSound()` accepts f32 stereo frames, and 16
simultaneous sounds run at 60 fps without an underrun.

## 2. Why now

P9-S2 proves the sink, not the API. `Sound` is the API a game uses most, and its pitch/pan/volume paths run
inside the audio thread; measuring them now separates mixer cost from `Music` streaming in P9-S4.

## 3. Scope

**In:** `psp-tests/audio_smoke` sound modes; the `Sound`/`Wave` API matrix; audio-thread and frame-time
measurements; heap and sound-count limits.

**Out:** `Music`/`AudioStream` (P9-S4), SAS/ME (P9-S6/S7), DSP processors on streams, and any backend or
renderer change.

## 4. Preconditions

- [ ] P9-S2 closed: one sound plays; 64-byte buffer and channel counters are in the log
- [ ] The P9-S2 build is archived with its boot line; use it as the `COUNT=1` control
- [ ] `reset` was run; partition 2 free reads 25,149,440 bytes
- [ ] `audio_smoke` builds with `PSP_LARGE_MEMORY=0` and a declared `PSP_HEAP_SIZE_KB`
- [ ] A deterministic wave generator is in the test; no Memory Stick asset is needed
- [ ] The 018–021 gate hashes are recorded

## 5. Tasks

Status vocabulary — use exactly these words, in backticks: `todo` · `doing` · `done` · `blocked` · `dropped`.

| ID | task | status | verification | evidence |
|---|---|---|---|---|
| T1 | **API matrix**: exercise `LoadSound`, `IsSoundValid`, `PlaySound`, `StopSound`, `PauseSound`, `ResumeSound`, `IsSoundPlaying`, `UnloadSound` | `todo` | each call observed; invalid sound paths return false/do nothing; no crash | — |
| T2 | **Mix `COUNT` sounds**: 1/4/16/32 staggered sounds, same wave and different waves | `todo` | 16 sounds at 60 fps; record the count where frame time leaves budget | — |
| T3 | **Volume/pan/pitch**: `SetSoundVolume`, `SetSoundPan`, `SetSoundPitch`, `SetMasterVolume` | `todo` | audible and measured change; pitch changes duration; pan is left/right | — |
| T4 | **Wave paths**: `LoadSoundFromWave`, `LoadSoundAlias`, `LoadWaveSamples`, `WaveCopy`/`WaveCrop` if used by the test, `UnloadSoundAlias` | `todo` | alias shares data and unload does not free the source; wave copy/crop round-trips | — |
| T5 | **`UpdateSound`**: feed f32 stereo frames into a playing sound; verify continuity and no reallocation after the first call | `todo` | audio stays continuous; heap counter flat after warm-up | — |
| T6 | **Measure**: audio-thread `runClocks`, rest length, errors, frame time for each arm; control = P9-S2 `COUNT=1` | `todo` | numbers in §7; no underrun; repeat within the project's 0.1 % threshold where applicable | — |
| T7 | **Heap/limits**: record free bytes with 32 sounds; define the maximum sound count that still fits 32 MB | `todo` | a documented count and free-bytes figure | — |
| T8 | **Regression**: `batch_check` and 018–021 gates; `git diff -- examples/` empty | `todo` | gates byte-identical | — |

### Task log (append-only)

- 2026-11-21 — sprint drafted; not started

## 6. Budgets

| budget | limit | measured | tag |
|---|---|---|---|
| frame time | ≤ 16.7 ms (60 fps) with 16 sounds | | |
| audio thread CPU | record; target ≤ 2 ms/frame at 16 sounds | | |
| heap | 32 sounds fit; record free bytes | | |
| per-frame allocation | none after warm-up | | |

## 7. Measurements

Audio has no screenshot evidence. Record build marker, boot log, counters and audible confirmation.

| run | build | workload | metric | value | notes |
|---|---|---|---|---|---|
| control | `audio b2` | 1 sound | frame time, runClocks | | P9-S2 |
| mix 4 | `audio b3` | 4 sounds | frame time, runClocks | | |
| mix 16 | `audio b3` | 16 sounds | frame time, runClocks, free heap | | |
| mix 32 | `audio b3` | 32 sounds | frame time, runClocks, free heap | | |
| pitch | `audio b3` | 1 sound, pitch 0.5/2.0 | duration, runClocks | | expected duration change |

## 8. Exit criteria

- [ ] every `Sound`/`Wave` function in the matrix is exercised with evidence
- [ ] 16 simultaneous sounds run at 60 fps with no underrun
- [ ] volume, pan and pitch are audible and measured; pitch duration semantics match raylib
- [ ] `LoadSoundAlias()` shares data and unloads without freeing the source
- [ ] `UpdateSound()` accepts f32 stereo and does not allocate per frame
- [ ] the maximum sound count that fits 32 MB is recorded
- [ ] `batch_check` and 018–021 gates are unchanged
- [ ] the API matrix and sound-count facts are written back to the Phase 9 page

## 9. Result

Not started.

## 10. Follow-ups

- P9-S4: `Music` streaming and the decoder/reader decision.
- If 32 sounds exceed the heap, record the working set and move the limit into Phase 9's exit criteria.
- If pitch resampling dominates the audio thread, measure it separately before considering a fixed-point
  resampler; do not change semantics without an A/B.
