# Sprint 046 (P9-S2) — Custom backend and the first PSP output

| | |
|---|---|
| Roadmap phase | [Phase 9 — Audio](../phases/09-audio/index.md) |
| Sprint | P9-S2 |
| Branch / base commit | P9-S1 close sha (pin at start) |
| Started / closed | — / — |
| Build number(s) | `audio b2`; pspgu `b24` unchanged |
| Tasks | 0/7 done |

## 0. Handover

**Situation.** P9-S1 made `libraylib.a` contain raudio and installed a **silent** custom backend: it reports
s16/44100/stereo and consumes frames without touching `sceAudio`. This sprint replaces the silent sink with a
real one: reserve one regular channel at 1024 frames, copy miniaudio's frames into a 64-byte-aligned buffer,
and block on `sceAudioOutputPannedBlocking()` from miniaudio's audio thread. One `Sound` is the proof.

**Read before acting**, in this order:

1. `../phases/09-audio/index.md` — the corrected device contract and P9-S2.
2. `../sprints/045-phase9-enable-raudio.md` §9 — the silent backend's callbacks, boot line and link line.
3. `../memory-map.md` §6 and §9 — device protocol; the traps about blocking audio, thread stacks and caches.
4. `src/raudio.c:466-530` — the device/context calls; `:2534-2645` — `OnSendAudioDataToDevice()`; do not
   change the mixer.
5. `src/external/miniaudio.h:20742-20910` — the playback thread: `onDeviceWrite()` receives a stack buffer
   and must return the frames written.
6. `pspdev/psp/sdk/include/pspaudio.h:76-180` — `PSP_AUDIO_SAMPLE_ALIGN`, `sceAudioChReserve`,
   `sceAudioOutputPannedBlocking`, `sceAudioGetChannelRestLen`.
7. Sony's `at3mix` and `sas-streaming` sound threads (`psp_sdk_660/.../sample/sound/`) for the output-call
   order only; do not copy Sony code.

**First action.** Turn `audio_smoke` into `MODE=tone`: generate a 1 kHz, 1-second, 16-bit stereo sine into a
`Wave`, `LoadSoundFromWave()`, `PlaySound()`, run 600 frames, log `DONE`, sleep. It must fail cleanly with
`IsAudioDeviceReady() == false` before the backend exists; that failure is the control.

**Hard rules.**

- **Use a regular channel.** `sceAudioChReserve(PSP_AUDIO_NEXT_CHANNEL, 1024, PSP_AUDIO_FORMAT_STEREO)`;
  1024 is already a multiple of 64 samples. Do not use `sceAudioOutput2*`/SRC in this sprint.
- **The PCM buffer is static and 64-byte aligned.** miniaudio gives `onDeviceWrite()` a stack buffer
  (`playbackDeviceData`, `miniaudio.h:20742`); never pass it to `sceAudio*`. Copy into
  `static int16_t pcm[1024*2] __attribute__((aligned(64)))`.
- **No `sceAudio` call on the game thread.** Reserve/start/stop/release happen in the backend's
  init/start/uninit callbacks; the blocking call happens in `onDeviceWrite()`.
- **Keep `AUDIO_DEVICE_FORMAT` f32.** The backend's native format is s16; miniaudio converts.
- **Blocking output must return.** On error, log the code, count it, and return `MA_ERROR`; never spin.
- **No renderer change.** `batch_check` and the 018–021 gates stay byte-identical.
- Commit messages carry the task ID: `sprint 046 T3: reserve the PSP output channel`.

**Commands.**

```sh
export PSPDEV=/home/psp/pspdev
export PATH=$PATH:$PSPDEV/bin

cd /home/psp/raylib-psp
cd src && make PLATFORM=PLATFORM_PSP -j8
cp libraylib.a $PSPDEV/psp/lib/libraylib.a

cd ../psp-tests/audio_smoke
make clean && make MODE=tone
( printf './audio_smoke.prx\n'; sleep 20 ) | pspsh
# after DONE: pspsh -e "reset"
```

**Do not touch.** `src/rlgl.h`, `src/rshapes.c`, `src/rtextures.c`, the pspgu driver, `Sound`/`Music`
semantics, and the Phase 9 page's exit criteria.

**Done means** §8, with a boot log, counters and an audible confirmation in §7.

## 1. Objective

One `Sound` plays through a real PSP channel for 600 frames at 60 fps, with the blocking output on
miniaudio's audio thread, no `sceAudio` call in the frame loop, and no underrun or error counted.

## 2. Why now

P9-S1 proved the module links; this sprint proves the real-time sink. Block size, 64-byte alignment, thread
priority and blocking semantics are the constraints every later `Sound` and `Music` result inherits. If they
are wrong, every later measurement is wrong.

## 3. Scope

**In:** `src/platforms/raudio_psp.c` device callbacks; `psp-tests/audio_smoke` tone mode; diagnostics
(channel, rest length, `runClocks`, errors); the period/priority/stack settings.

**Out:** multiple sounds, pitch/pan/volume matrices, `Music`, decoder threads, SAS/ME, and any change to
`raudio.c`'s mixer.

## 4. Preconditions

- [ ] P9-S1 closed: `raudio.o` non-empty; silent backend reports ready; link line includes `-lpspaudio -lpthread -latomic`
- [ ] `reset` was run; partition 2 free reads 25,149,440 bytes
- [ ] A control capture of `batch_check` and the 018–021 gates is archived
- [ ] The tone WAV/sine is generated deterministically; no Memory Stick I/O is needed for this sprint
- [ ] `PSP_HEAP_SIZE_KB` is declared in the test; builds use `PSP_LARGE_MEMORY=0`
- [ ] The device is audible; the operator can confirm the tone (audio cannot be captured with `scrshot`)

## 5. Tasks

Status vocabulary — use exactly these words, in backticks: `todo` · `doing` · `done` · `blocked` · `dropped`.

| ID | task | status | verification | evidence |
|---|---|---|---|---|
| T1 | **Descriptor**: report native s16, 2 channels, 44100, `periodSizeInFrames=1024`, `periodCount=2`; keep the app format f32 | `todo` | boot log shows `Format: f32 -> s16`, `Channels: 2 -> 2`, `Sample rate: 44100 -> 44100`, period 2048 | — |
| T2 | **Reserve/start/stop/uninit**: reserve a regular channel at 1024 frames in start; release in uninit; count reserve/release errors | `todo` | two `run → DONE → reset → run` cycles reserve the same channel; no leaked channel | — |
| T3 | **`onDeviceWrite`**: copy to the static 64-byte-aligned s16 buffer, handle partial blocks, call `sceAudioOutputPannedBlocking(ch, 0x8000, 0x8000, pcm)`, return frames written | `todo` | no `sceAudio` in `audio_smoke`'s frame loop; alignment assert never fires; error path returns | — |
| T4 | **Thread setup**: set `ma_context_config.threadPriority`/`threadStackSize`; create the device early; record `sceKernelGetThreadId()` in the first `onDeviceWrite()` | `todo` | thread ID and stack size logged; `sceKernelReferThreadRunStatus().runClocks` reads | — |
| T5 | **Tone test**: `audio_smoke MODE=tone` generates a 1 kHz s16 stereo sine, `LoadSoundFromWave`, `PlaySound`, 600 frames | `todo` | tone audible; `DONE frames 600`; `IsSoundPlaying` transitions correctly | — |
| T6 | **Measure**: audio-thread CPU, rest length min/max, output errors, frame time; control = silent backend | `todo` | numbers in §7; no underrun; frame time ≤ 16.7 ms | — |
| T7 | **Regression**: `batch_check` and the 018–021 gates; `git diff -- examples/` empty | `todo` | gates byte-identical to control | — |

### Task log (append-only)

- 2026-11-21 — sprint drafted; not started

## 6. Budgets

| budget | limit | measured | tag |
|---|---|---|---|
| frame time | ≤ 16.7 ms (60 fps) with one sound | | |
| audio thread CPU | record; target ≤ 1 ms/frame for one sound | | |
| heap | no allocation in `onDeviceWrite`; test allocates at load | | |
| PCM buffer | 1024 frames × 2 ch × 2 B = 4,096 B, 64-byte aligned | | |

## 7. Measurements

Audio has no screenshot evidence. A run is accepted only with a build marker, boot log, counters, and an
audible confirmation by the operator.

| run | build | workload | metric | value | notes |
|---|---|---|---|---|---|
| control | `audio b1` | silent backend, 600 frames | ready, frame time, runClocks | | no tone |
| tone | `audio b2` | 1 sound, 600 frames | audible, underruns, rest min, runClocks | | first output |
| tone again | `audio b2` | same, reset first | same counters | | repeatability |

## 8. Exit criteria

- [ ] one tone/sound is audible on the PSP-1000
- [ ] `onDeviceWrite()` uses a static 64-byte-aligned buffer and blocks on the channel
- [ ] no `sceAudio` call appears in the game frame loop
- [ ] reserve/release survive two runs with `reset` between; no leaked channel
- [ ] audio-thread CPU and rest-length counters are recorded in §7
- [ ] `batch_check` and the 018–021 gates are unchanged
- [ ] blocking-call, alignment and thread facts are written back into the Phase 9 page / memory map §9

## 9. Result

Not started.

## 10. Follow-ups

- P9-S3: multiple `Sound`s, volume/pan/pitch and the API conformance matrix.
- If `sceAudioChReserve()` fails under heap pressure, record the free bytes and move `InitAudioDevice()` earlier.
- If the blocking call holds `AUDIO.System.lock`, measure the main-thread `SetSoundVolume()` latency; do not
  change the lock without a measurement.
