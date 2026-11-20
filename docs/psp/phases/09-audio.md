# Phase 9 — Audio

| | |
|---|---|
| State | not started |
| Depends on | Phase 1 (a running build) — independent of the renderer phases |
| Exit criterion | several sounds plus one streamed track on the device, 60 fps held, audio thread cost measured, five minutes without a dropout, no `sceAudio` call on the main thread |

## What this phase is about

`raudio` is built on miniaudio, and **miniaudio has no PSP backend** — zero references in the whole library.
That is why the 5.5 port's own boot log reads `raudio:.... not loaded (optional)`. Audio on this platform has to
be written, not enabled, so the phase starts by choosing where it goes.

## What has to be true before starting

- [ ] A build that runs for minutes without a crash
- [ ] A way to measure the audio thread separately from the frame loop
- [ ] The microSD/PCIe-equivalent question settled: assets come from the memory stick or from `host0:`

## Tasks

- [ ] Choose the integration: miniaudio's custom backend (`MA_ENABLE_CUSTOM`, `ma_backend_custom`, context
      callbacks) versus driving `sceAudio` straight from `raudio.c` — [open question 6](../memory-map.md#12-open-questions)
- [ ] Reserve output with `sceAudioChReserve()`; buffer 64–65,472 samples, multiple of 64
- [ ] Put `sceAudioOutputPannedBlocking()` on its **own thread** — it paces to playback
- [ ] Prefer `sceAudioSRCChReserve()` hardware SRC for arbitrary rates; measure against CPU resampling
- [ ] Software-mix sounds into one or two channels, leaving hardware channels free for streamed music
- [ ] Stream music: decode on the audio thread into a ring buffer, never open a file from the callback
- [ ] Respect memory-stick cost: ~17 ms per `sceIoOpen`, so few long-lived handles and large reads
- [ ] `SetSoundPitch` is resampling — profile it rather than assuming it is free
- [ ] Optional, later: move mixing/resampling to the Media Engine (see [§10.2](../memory-map.md#102-media-engine-the-second-mips-core))
- [ ] Define the exit path: stop the thread, release the channel, then exit — in that order

## What we already know

- 8 hardware channels (0–7), volume 0–`0x8000`, `PSP_AUDIO_FORMAT_STEREO` / `MONO`
- `sceAudioSRCChReserve(samplecount, freq, channels)` + `sceAudioSRCOutputBlocking()` exist for arbitrary rates
- A blocking audio call in the main loop turns frame time into the audio buffer length
- Audio mixing on the Media Engine measured 5.8 ms/frame saved on another project — a real number, but from
  different firmware and workload, so it is a hypothesis here, not a budget

## Exit criteria

- [ ] sounds and one streamed track play simultaneously, at 60 fps, measured
- [ ] audio thread CPU cost in the profile, and the main thread free of audio calls
- [ ] five minutes without dropout or underrun, counters confirming
- [ ] assets loaded from the stick survive a slow-mount case (the firmware's own fallback behaviour is the model)

## Sprints and features

None yet. Use the [sprint template](../templates/sprint.md).
