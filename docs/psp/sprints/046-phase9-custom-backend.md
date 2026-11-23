# Sprint 046 (P9-S2) — Custom backend and the first PSP output

| | |
|---|---|
| Roadmap phase | [Phase 9 — Audio](../phases/09-audio/index.md) |
| Sprint | P9-S2 |
| Branch / base commit | `master` @ `8de96c45` (P9-S1 close; pin at start) |
| Started / closed | 2026-11-21 / 2026-11-21 |
| Build number(s) | `audio b2`; pspgu `b24` unchanged |
| Tasks | 7/7 done |

## 0. Handover

**Situation.** P9-S1 made `libraylib.a` contain raudio and installed a **silent** custom backend. The S1
device run is the control: 418 callbacks in 10.03 s (41/s), `runClocks` ~315k, 128 KiB worker stack,
`batch_check` gate `cf43ce6d…`. This sprint replaces the silent sink with a real one: reserve one regular
channel at 1024 frames, copy miniaudio's s16 frames into **two alternating 64-byte-aligned buffers**, write
the exact range back, and block on `sceAudioOutputPannedBlocking()` from miniaudio's audio thread. One
`Sound` is the proof.

**Read before acting**, in this order:

1. `../phases/09-audio/index.md` — the corrected device contract and P9-S2.
2. `../sprints/045-phase9-enable-raudio.md` §9 — the silent backend, 128 KiB stack, link order and counters.
3. `../memory-map.md` §6 and §9 — device protocol; blocking audio, thread stacks and cache traps.
4. `src/platforms/raudio_psp.c` — the working silent callback table; this sprint changes it in place.
5. `src/raudio.c:466-575` — `InitAudioDevice`/`CloseAudioDevice`, custom backend wiring and stats call.
6. `src/external/miniaudio.h:20742-20910` — the playback thread: `onDeviceWrite()` receives a stack buffer.
7. `pspdev/psp/sdk/include/pspaudio.h:76-180` — `PSP_AUDIO_SAMPLE_ALIGN`, `sceAudioChReserve`,
   `sceAudioOutputPannedBlocking`, `sceAudioGetChannelRestLength`; `psputils.h:73` — cache writeback.
8. The reference `at3mix` and `sas-streaming` sound threads (local notes §7) for call order only.

**First action.** Extend `psp-tests/audio_smoke` to generate a deterministic 1 kHz, 1-second, 16-bit stereo
sine, `LoadSoundFromWave()`, `PlaySound()`, run 600 frames, log `DONE`, sleep. The S1 silent binary is the
control; the tone must be audible in the new build.

**Hard rules.**

- **Use a regular channel.** `sceAudioChReserve(PSP_AUDIO_NEXT_CHANNEL, PSP_AUDIO_SAMPLE_ALIGN(1024),
  PSP_AUDIO_FORMAT_STEREO)`. 1024 is already a multiple of 64 samples. Do not use `sceAudioOutput2*`/SRC.
- **Reserve once.** `onDeviceStart()` reserves only if the channel is still `-1`; `onDeviceStop()` does not
  release; `onDeviceUninit()` releases after the worker has joined. A second start must not reserve twice.
- **Two PCM buffers, 64-byte aligned.** Use
  `static int16_t pcm[2][1024*2] __attribute__((aligned(64)))` and alternate. A single buffer can be
  overwritten while the hardware still reads it; the blocking call returns when accepted, not when played.
- **Copy, write back, then output.** `onDeviceWrite()` receives a miniaudio stack buffer
  (`playbackDeviceData`, `miniaudio.h:20742`). Copy exactly `frames * 2 * sizeof(int16_t)` bytes into the
  aligned buffer and call `sceKernelDcacheWritebackRange()` over that range before the output call.
- **The blocking call is the clock.** Remove the S1 `sceKernelDelayThread()`; do not sleep and block.
- **No `sceAudio` call on the game thread.** Reserve/start/stop/release live in the backend callbacks; the
  blocking call lives in `onDeviceWrite()`.
- **Keep `AUDIO_DEVICE_FORMAT` f32.** The backend reports native s16; miniaudio converts.
- **Keep the 128 KiB worker stack and normal priority.** S1 proved the default stack dies before playback.
  High/realtime is a separate A/B arm, not a default change.
- **Errors return.** Count them, set `*pFramesWritten` to the frames already accepted, and return `MA_ERROR`;
  never spin or retry forever.
- **No renderer change.** `batch_check` stays at `cf43ce6d…`; the 018–021 gates stay unchanged.
- Commit messages carry the task ID: `sprint 046 T3: alternate the PSP PCM buffers`.

**Commands.**

```sh
export PSPDEV=/home/psp/pspdev
export PATH=$PATH:$PSPDEV/bin

cd /home/psp/raylib-psp/src && make PLATFORM=PLATFORM_PSP -j8
cp libraylib.a $PSPDEV/psp/lib/libraylib.a

cd ../psp-tests/audio_smoke
make clean && make
( printf './audio_smoke.prx\n'; sleep 20 ) | pspsh
# after DONE: pspsh -e "reset"
```

**Do not touch.** `src/rlgl.h`, `src/rshapes.c`, `src/rtextures.c`, the pspgu driver, `Sound`/`Music`
semantics, and the Phase 9 page's exit criteria.

**Done means** §8, with a boot log, counters and an audible confirmation in §7.

## 1. Objective

One `Sound` plays through a real PSP channel for 600 frames at 60 fps, with the blocking output on
miniaudio's audio thread, two alternating aligned PCM buffers, cache writeback before every output call, no
`sceAudio` call in the frame loop, and no underrun or error counted.

## 2. Why now

P9-S1 proved the module links and the worker runs; this sprint proves the real-time sink. Buffer reuse,
64-byte alignment, cache writeback and blocking semantics are the constraints every later `Sound` and
`Music` result inherits. If they are wrong, every later measurement is wrong.

## 3. Scope

**In:** `src/platforms/raudio_psp.c` device callbacks; `psp-tests/audio_smoke` tone mode; diagnostics
(channel, rest length, `runClocks`, errors); the period/priority/stack settings.

**Out:** multiple sounds, pitch/pan/volume matrices, `Music`, decoder threads, SAS/ME, and any change to
`raudio.c`'s mixer.

## 4. Preconditions

- [ ] P9-S1 closed: silent backend, 418 callbacks/10.03 s, 128 KiB stack, gate `cf43ce6d…`
- [ ] `reset` was run; partition 2 free reads 25,149,440 bytes
- [ ] A control capture of `batch_check` and the 018–021 gates is archived
- [ ] The tone is generated deterministically; no Memory Stick I/O is needed
- [ ] `PSP_HEAP_SIZE_KB` is declared in the test; builds use `PSP_LARGE_MEMORY=0`
- [ ] The device is audible; the operator can confirm the tone (audio cannot be captured with `scrshot`)

## 5. Tasks

Status vocabulary — use exactly these words, in backticks: `todo` · `doing` · `done` · `blocked` · `dropped`.

| ID | task | status | verification | evidence |
|---|---|---|---|---|
| T1 | **Descriptor**: native s16, 2 channels, 44100, `periodSizeInFrames=1024`, `periodCount=2`; keep the app format f32 | `done` | boot log shows `Format: f32 -> s16`, `Channels: 2 -> 2`, `Sample rate: 44100 -> 44100`, period 2048 | boot log matches exactly |
| T2 | **Channel lifecycle**: reserve in `onDeviceStart()` only if channel is `-1`; release in `onDeviceUninit()`; leave reserved across stop/start; count failures | `done` | two `run → DONE → reset → run` cycles reserve one channel; no double reserve; no leaked channel | channel **7** in both runs; reserve and release succeeded |
| T3 | **`onDeviceWrite`**: copy each block into alternating 64-byte-aligned s16 buffers, write back the exact range, call `sceAudioOutputPannedBlocking(ch, 0x8000, 0x8000, pcm)`; no sleep; handle partial blocks | `done` | no `sceAudio` in the frame loop; no `sceKernelDelayThread`; two buffers alternate; alignment assert never fires | 433 output blocks; rest 960–1024; errors **0**; no delay call in backend |
| T4 | **Thread setup**: retain the S1 128 KiB stack and normal priority; record thread ID/`runClocks`; only test high/realtime as a separate A/B arm | `done` | thread ID and 128 KiB stack logged; `runClocks` reads; no default-stack regression | 128 KiB/normal retained; `runClocks` 410,966 / 411,441 |
| T5 | **Tone test**: extend the existing `audio_smoke` (do not create a second app) with a generated 1 kHz s16 stereo sine, `LoadSoundFromWave`, `PlaySound`, 600 frames | `done` | tone audible; `DONE frames 600`; `IsSoundPlaying` transitions correctly | tone valid=1; **audible on both runs**; `sound=1` at frame 60 run 1, `sound=0` at DONE |
| T6 | **Measure**: audio-thread CPU, `sceAudioGetChannelRestLength()` min/max, output errors, frame time; control = S1 silent run | `done` | numbers in §7; no underrun; frame time ≤ 16.7 ms | 433 callbacks, 43/s, rest 960–1024, errors 0, 60 fps, two runs |
| T7 | **Regression**: `batch_check` and the 018–021 gates; `git diff -- examples/` empty | `done` | gate `cf43ce6d…` byte-identical | `batch_check_s2.raw` hash `cf43ce6d…`, identical to both gate captures; no `examples/` change |

### Task log (append-only)

- 2026-11-21 — sprint drafted; not started
- 2026-11-21 — readiness review: double buffering, cache writeback, reserve-once and S1 baseline added
- 2026-11-21 — T1–T7 done; two 600-frame device runs; tone audible
- 2026-11-21 — implementation committed `49507c3f`; gate `cf43ce6d…` unchanged

## 6. Budgets

| budget | limit | measured | tag |
|---|---|---|---|
| frame time | ≤ 16.7 ms (60 fps) with one sound | 16.7 ms / 60 fps | `[measured: PSP-1000, P9-S2]` |
| audio thread CPU | record; target ≤ 1 ms/frame for one sound | `runClocks` 410,966 / 411,441 over 10.03 s (~0.12 % CPU) | `[measured: PSP-1000, P9-S2]` |
| heap | no allocation in `onDeviceWrite`; tone allocated at load | not separately measured; no per-write allocation | `[derived]` |
| PCM buffers | 2 × 1024 frames × 2 ch × 2 B = 8,192 B, both 64-byte aligned | 8,192 B static | `[measured: source]` |
| rest samples | never 0 at the output call; record min/max | min 960, max 1024, errors 0 | `[measured: PSP-1000, P9-S2]` |

## 7. Measurements

Audio has no screenshot evidence. A run is accepted only with a build marker, boot log, counters, and an
audible confirmation by the operator.

| run | build | workload | metric | value | notes |
|---|---|---|---|---|---|
| control | `audio b1` | silent backend, 600 frames | ready, frame time, runClocks | 418 callbacks, 41/s, 315k | S1 |
| tone | `audio b2` | 1 sound, 600 frames | audible, errors, rest min/max, runClocks | 433, 43/s, 960–1024, 0, 410,966 | first output |
| tone again | `audio b2` | same, reset first | same counters | 433, 43/s, 960–1024, 0, 411,441 | repeat within 0.12 % |
| gate | `audio b2` | `batch_check` 2 frames | capture hash | `cf43ce6d…` | identical to both controls |

## 8. Exit criteria

- [x] one tone/sound is audible on the PSP-1000
- [x] `onDeviceWrite()` uses two alternating static 64-byte-aligned buffers and blocks on the channel
- [x] the exact PCM range is written back before every output call
- [x] no `sceKernelDelayThread()` and no `sceAudio` call in the game frame loop
- [x] reserve/release survive two runs with `reset` between; no leaked or double-reserved channel
- [x] audio-thread CPU and rest-length min/max are recorded in §7
- [x] `batch_check` stays `cf43ce6d…`; the 018–021 gates are unchanged
- [x] blocking-call, alignment, cache and thread facts are written back into the Phase 9 page / memory map §9

## 9. Result

**Done and audible.** Two device runs reserved channel **7** and played the generated 1 kHz tone; the operator
confirmed both. Each run produced **433 callbacks in 10.03 s (43/s)**, rest length **960–1024**, **0** output
errors, `runClocks` **410,966 / 411,441**, and `DONE frames 600 audio=1 sound_playing=0` at 60 fps.

Two alternating 64-byte-aligned buffers are used; each full 4096-byte block is written back before
`sceAudioOutputPannedBlocking()`, and short tails are zero-filled. No `sceKernelDelayThread()` remains.
`raudio.o` text is 422,877 B, the app ELF text 1,247,004 B, and the PRX 1,596,838 B.

`batch_check_s2.raw` is byte-identical to both existing gate captures:
`cf43ce6de867112b183412d31c5de9112fe7a7d59fc40620c6a426d4b2d732f8`.

Implementation commit: `49507c3f`.

## 10. Follow-ups

- P9-S3: multiple `Sound`s, volume/pan/pitch and the API conformance matrix.
- `sceAudioChReserve()` from the miniaudio worker was accepted on this unit; keep the `onDeviceInit()`
  fallback documented for a future firmware/pthread change.
- If `sceAudioChReserve()` fails under heap pressure, record the free bytes and move `InitAudioDevice()` earlier.
- If the blocking call holds `AUDIO.System.lock`, measure the main-thread `SetSoundVolume()` latency; do not
  change the lock without a measurement.
- High/realtime priority remains an A/B arm; do not change the 128 KiB/normal baseline without re-running S1.
- If `sceAudioChReserve()` from the worker (inside `onDeviceStart()`) is rejected, move the reserve to
  `onDeviceInit()` on the main thread and release in `onDeviceUninit()`; do not retry it in `onDeviceWrite()`.
