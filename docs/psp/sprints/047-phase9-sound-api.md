# Sprint 047 (P9-S3) — `Sound` API and mixing

| | |
|---|---|
| Roadmap phase | [Phase 9 — Audio](../phases/09-audio/index.md) |
| Sprint | P9-S3 |
| Branch / base commit | `master` @ `1dc6f361` (P9-S2 close; pin at start) |
| Started / closed | 2026-11-21 / 2026-11-21 |
| Build number(s) | `audio b3`; pspgu `b24` unchanged |
| Tasks | 8/8 done |

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
| T1 | **API matrix**: `LoadSound` (exported WAV), `LoadSoundFromWave`, `IsSoundValid`, `PlaySound`, `StopSound`, `PauseSound`, `ResumeSound`, `IsSoundPlaying`, `UnloadSound`, `SetMasterVolume`/`GetMasterVolume` | `done` | each call observed; invalid sound/path returns false or does nothing; no crash | API arm: all checks pass; `LoadSound` from `host0:` valid; master 0.50; no crash |
| T2 | **Mix `COUNT` sounds**: 1/4/16/32 staggered sounds, same wave and different waves | `done` | 16 sounds at 60 fps; record the count where frame time leaves budget | mix16 16/16 valid @500 ms; mix32 32/32 valid @250 ms; both 60 fps; short chords audible |
| T3 | **Volume/pan/pitch**: `SetSoundVolume`, `SetSoundPan`, `SetSoundPitch`, master volume | `done` | audible and measured change; pitch changes duration; pan is left/right | levels: loud-left then quiet-right; pitch 0.5 stops at frame 119 (~1.97 s), 2.0 at frame 30 (~0.50 s); master 0.50 |
| T4 | **Wave paths**: `LoadWaveSamples`, `WaveCopy`, `WaveCrop`, `WaveFormat`, `LoadSoundAlias`, `UnloadSoundAlias` | `done` | alias shares data; unloading the alias does not free the source; copy/crop/format round-trip | API arm: copy/crop/format/samples data=1; alias valid; source still valid after alias unload |
| T5 | **`UpdateSound`**: feed f32 stereo frames into a playing sound; verify continuity and no reallocation after the first call | `done` | audio stays continuous; free-memory counter flat after warm-up | API arm: f32 stereo update completes; `raudio.c` copies into the existing buffer (no allocation path) |
| T6 | **Measure**: audio-thread `runClocks`, rest length, errors, frame time for each arm; control = P9-S2 `COUNT=1` | `done` | numbers in §7; no underrun; repeat within the project's 0.1 % threshold where applicable | all arms: 60 fps, rest 960–1024, zero errors; runClocks 415,845 / 800,408 / 789,606 (tone/mix16/mix32) |
| T7 | **Heap/limits**: free bytes before/after 32 sounds via `sbrk`/`mallinfo`, with partition-free as a control; define the maximum tested count | `done` | documented count and free-bytes figure | 32/32 valid; `sbrk` +1,142,784 B; `mallinfo` used 2,000,152 B, free 30,440 B; partition free unchanged (fixed 4 MiB heap); **32 tested, 33 not probed** |
| T8 | **Regression**: `batch_check` and the 018–021 gates; `git diff -- examples/` empty | `done` | gate `cf43ce6d…` byte-identical | `batch_check_s3.raw` hash `cf43ce6d…`, identical to both gate captures |

### Task log (append-only)

- 2026-11-21 — sprint drafted; not started
- 2026-11-21 — readiness review: S2 baseline, existing harness, LoadSound path, UpdateSound format and heap method added
- 2026-11-21 — harness modes `api`, `mix`, `pitch`, `levels`, `heap`; mix16/32 use 500/250 ms waves
- 2026-11-21 — all device arms and the gate pass; audible checks: levels, mix16/32, pitch low/long vs high/short, tone
- 2026-11-21 — heap probe: 32/32 valid, sbrk +1,142,784 B, mallinfo free 30,440 B

## 6. Budgets

| budget | limit | measured | tag |
|---|---|---|---|
| frame time | ≤ 16.7 ms (60 fps) with 16 sounds | 60 fps on tone/mix16/mix32 | `[measured: PSP-1000, P9-S3]` |
| audio thread CPU | record; target ≤ 2 ms/frame at 16 sounds | runClocks 800,408 over 10.03 s (mix16); 789,606 (mix32) | `[measured: PSP-1000, P9-S3]` |
| heap | 32 sounds fit; record free bytes | 32/32 valid; `mallinfo` 2,000,152 used / 30,440 free; `sbrk` +1,142,784 | `[measured: PSP-1000, P9-S3]` |
| per-frame allocation | none after warm-up | UpdateSound copies into a preallocated buffer | `[derived: source]` |
| rest samples | never 0 at the output call | min 960, max 1024, errors 0 | `[measured: PSP-1000, P9-S3]` |

## 7. Measurements

Audio has no screenshot evidence. Record build marker, boot log, counters and audible confirmation.

| run | build | workload | metric | value | notes |
|---|---|---|---|---|---|
| control | `audio b2` | 1 tone, 600 frames | runClocks, rest, errors | 433/43, 960–1024, 0, 415,845 | S2/tone |
| api | `audio b3` | API matrix | pass/fail, callbacks | all pass, 91 callbacks, 31/s | no held tone by design |
| mix 16 | `audio b3` | 16×500 ms | frame, runClocks, rest | 433/43, 960–1024, 0, 800,408 | short chord audible |
| mix 32 | `audio b3` | 32×250 ms | frame, runClocks, rest | 424/42, 960–1024, 0, 789,606 | similar, louder |
| pitch 0.5 | `audio b3` | 1 sound | stop frame | 119 (~1.97 s) | low/long |
| pitch 2.0 | `audio b3` | 1 sound | stop frame | 30 (~0.50 s) | high/short |
| levels | `audio b3` | left/right | audible, callbacks | loud-left, quiet-right; 44 callbacks | operator confirmed |
| heap 32 | `audio b3` | 32×100 ms, no play | valid, heap | 32/32, sbrk +1,142,784, 30,440 free | silent as designed |
| gate | `audio b3` | `batch_check` | hash | `cf43ce6d…` | unchanged |

## 8. Exit criteria

- [x] every `Sound`/`Wave` function in the matrix is exercised with evidence
- [x] 16 simultaneous sounds run at 60 fps with no underrun
- [x] volume, pan, pitch and master volume are audible/measured; pitch duration matches raylib
- [x] `LoadSound` works from a `host0:` WAV, and `LoadSoundAlias` shares data without freeing the source
- [x] `UpdateSound()` accepts f32 stereo and does not allocate per frame
- [x] 32 sounds are the maximum tested; 30,440 B remained free (33 not probed)
- [x] `batch_check` stays `cf43ce6d…`; the 018–021 gates are unchanged
- [x] the API matrix and sound-count facts are written back to the Phase 9 page

## 9. Result

**Done.** All eight arms passed. The API matrix exercised file and in-memory loads, aliases, Wave copy/crop/
format/samples and `UpdateSound`; the `levels` arm was heard as loud-left then quiet-right; pitch 0.5/2.0
stopped at frames 119/30 (low/long vs high/short); mix16/mix32 were short audible chords at 60 fps.

A 100 ms mix was too short to hear reliably; mix16 now uses **500 ms** and mix32 **250 ms**. They are still
memory-bounded: 32 sounds at 250 ms use 2,000,152 B, leave 30,440 B in the 4 MiB test heap, and all 32 are
valid. The partition-free counter is constant by design, so `sbrk`/`mallinfo` is the meaningful probe.

`batch_check` is still byte-identical to the gate:
`cf43ce6de867112b183412d31c5de9112fe7a7d59fc40620c6a426d4b2d732f8`.

Committed in this change.

## 10. Follow-ups

- P9-S4: `Music` streaming and the decoder/reader decision.
- 32 is the highest heap arm tested; probe 33+ only with a larger declared heap and update the limit.
- If pitch resampling dominates the audio thread, measure it separately before considering a fixed-point
  resampler; do not change semantics without an A/B.
- `LoadSound` from `host0:` worked; keep the deferral text for environments where it is not writable.
- Mix lengths 500/250 ms are the audible minimum found; do not shrink them back to 100 ms without a new
  listening check.
