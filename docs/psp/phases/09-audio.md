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

- [ ] Choose the integration: miniaudio's custom backend (`MA_ENABLE_ONLY_SPECIFIC_BACKENDS` +
      `MA_ENABLE_CUSTOM`, the `ma_backend` vtable's `onDeviceInit`/`onDeviceStart`/`onDeviceStop`/`onDeviceUninit`)
      versus driving `sceAudio` straight from `raudio.c` — [open question 6](../memory-map.md#12-open-questions)
- [ ] Define the device contract in one place: **44.1 kHz, s16, stereo**, and a period of N frames. raylib
      currently defaults to `ma_format_f32`, 2 channels, rate 0 (`src/raudio.c:285-292`); a PSP backend that reports
      s16/44100 lets miniaudio convert at load time instead of on the audio thread
- [ ] Reserve output with `sceAudioChReserve()`; 64–65,472 frames, buffer size aligned to 64 bytes (a multiple of
      16 stereo frames), 64-byte-aligned buffer memory
- [ ] Put `sceAudioOutputPannedBlocking()` on its **own thread**, never in the frame loop — it paces to playback,
      and the thread must not be starved for more than a few hundred microseconds
- [ ] Use at least a double buffer; the blocking call returns when the block is accepted, not when it has finished
      playing
- [ ] Split decoder/reader from feeder: low-priority decoder + card reader, high-priority feeder that only copies
      from a PCM ring and calls the blocking output
- [ ] Prefer a regular 44.1 kHz channel and CPU resampling; if using `sceAudioSRCChReserve()`, reserve it **once**
      and keep it — re-reserving after release failed repeatedly on hardware in another project
- [ ] Software-mix sounds into one or two channels, leaving hardware channels free for streamed music
- [ ] Stream music: decode ahead on the decoder thread into a ring buffer (~1 s), never open a file from the
      feeder or the miniaudio callback; batch reads with a 64 KB `setvbuf`
- [ ] Respect memory-stick cost: ~17 ms per `sceIoOpen`, so few long-lived handles and large reads
- [ ] `SetSoundPitch` is resampling — profile it rather than assuming it is free
- [ ] Make audio threads persistent; retry failed bring-up on a timer and log every failure
- [ ] Decide the Media Engine route (see below): main-CPU mixer, `sceSas` through `pspsascore`, or an owned ME
      mixer. Record the decision and the measurement behind it
- [ ] If the ME is taken over: gate it behind a config file next to the EBOOT, keep the main-CPU path compiled in,
      and handle suspend/resume
- [ ] Define the exit path: stop and join the audio threads, release channels, then exit — in that order

## What we already know

- 8 hardware channels (0–7), volume 0–`0x8000`, `PSP_AUDIO_FORMAT_STEREO` = 0 / `MONO` = 0x10
- `sceAudioSRCChReserve(samplecount, freq, channels)` + `sceAudioSRCOutputBlocking()` exist for arbitrary rates;
  `sceAudioOutput2*` is the same SRC output fixed at 44.1 kHz
- A blocking audio call in the main loop turns frame time into the audio buffer length
- Audio mixing on the Media Engine measured 5.8 ms/frame saved on another project — a real number, but from
  different firmware and workload, so it is a hypothesis here, not a budget
- The console is a PSP-1000 with 32 MB; the ME's local eDRAM is 2 MB on this model (4 MB on 2000+)
- PPSSPP does not execute Media Engine code, so every ME claim needs a hardware run
- `raudio`'s boot log on the 5.5 port reads `raudio:.... not loaded (optional)`, and miniaudio has no PSP backend

## Audio-service facts

Sources: PSPSDK `pspaudio.h` and `pspaudiolib.h`, Sony's Sound / Simple Audio / SAS overviews in the 6.6 SDK
tree (reading aid only, [§11.4](../memory-map.md#the-sony-sdk-what-it-is-for-and-what-it-is-not)), and the local
psp-developer skill (`references/audio.md`, `references/media-engine.md`). Numbers measured elsewhere carry their
hardware.

| fact | value | tag |
|---|---|---|
| Output channels | 8 regular channels, reserved by number (0–7) or `PSP_AUDIO_NEXT_CHANNEL` | `[public: pspaudio.h]` |
| Frames per block | 64–65,472; the request must satisfy the driver's 64-byte alignment. pspaudiolib uses **1,024** frames; Sony's SAS sample uses **736** (a multiple of 16 stereo frames) | `[public: pspaudio.h]` + `[external: Sony samples]` |
| Volume | 0–`0x8000` per channel, applied by the hardware digital volume | `[public: pspaudio.h]` |
| Formats | `PSP_AUDIO_FORMAT_STEREO` = 0, `PSP_AUDIO_FORMAT_MONO` = 0x10 | `[public: pspaudio.h]` |
| Regular-channel rate | 44.1 kHz by default; the kernel can switch the output between 44100 and 48000 only | `[public: uofw audio.c]` |
| SRC channel | one channel, no channel number: `sceAudioSRCChReserve(frames, freq, 2)`; accepts 8000, 11025, 12000, 16000, 22050, 24000, 32000, 44100, 48000 Hz; other rates return `SCE_AUDIO_ERROR_INVALID_FREQUENCY` | `[public: pspaudio.h]` + `[public: uofw audio.c]` |
| SRC handover | releasing and immediately re-reserving the SRC channel failed on hardware; one failed handover silenced the session | `[measured: PSP-3000, large 3D port, via skill]` |
| Blocking semantics | `sceAudioOutput*Blocking()` returns when the block has been accepted; playback starts after the previous block. Double-buffer and do not overwrite on return | `[external: Sony Simple Audio Overview]` |
| Buffer placement | PCM buffers for DMA must be in 32-bit-accessible main memory, 64-byte aligned, and **not** in eDRAM or scratchpad | `[external: same]` |
| Thread starvation | if the output thread does not run, audio stops and an audible click is produced; interrupt/dispatch-disabled periods must stay at most a few hundred microseconds | `[external: same]` |
| Double-buffer timing | with two 1,024-frame blocks in flight the blocking call is the audio clock, one block of latency (~23 ms at 44.1 kHz) | `[derived]` |
| Music decode cost | Vorbis decode measured at ~5–6 ms of main-CPU time per frame even with clean I/O | `[measured: PSP-3000, large 3D port, via skill]` |
| Mixer cost | a software mixer for an emulated sound chip measured 7.5 ms/frame on the game thread plus ~2 ms feeder time-stretch; moving it to the ME saved 5.8 ms/frame | `[measured: PSP-3000, large 3D port, via skill]` |
| miniaudio hooks | custom backends are selected with `ma_backend_custom`; the vtable supplies `onContextInit`, `onDeviceInit`, `onDeviceStart`, `onDeviceStop`, `onDeviceUninit`, `onContextUninit` | `[public: src/external/miniaudio.h]` |

## What the reference implementations do

| source | structure | frames | notes |
|---|---|---|---|
| PSPSDK `pspaudiolib` | 4 channels, one thread per channel, callback fills a double buffer, `sceAudioOutputPannedBlocking` | 1,024 | the canonical public pattern; PGE and openTRI both copy it |
| `PGE` `pgeWav.c` | same 4×1,024 layout, up to 16 WAV slots, volume clamp | 1,024 | one thread per channel; callback-based |
| `PGE` `pgeMp3.c` | `sceAudiocodec` decode thread + card-read thread; SRC channel reserved inside the decode thread | per frame | read priority 14, decode 15; SRC output blocking in the same thread as decode |
| `openTRI` `triAudioLib.c` | pspaudiolib clone; 4 channels, one thread each; caller supplies the thread priority | 1,024 | priority passed in (the header suggests 0x12) |
| `openTRI` `triAt3.c` | kernel-mode ATRAC3; 64-byte-aligned codec and mix buffers; one thread fills, blocks on output, then decodes the next block | 1,024 | passes an uncached alias of the mix buffer to the output call |
| `openTRI` `pmp_play.c` | separate decode and output threads, semaphores; audio blocking paces video | from file | both threads priority 0x8; channel 0 reserved stereo |
| Sony `at3mix` | ATRAC3+ decode thread + **SAS on the ME** (`sceSasCoreWithMix`) + Simple Audio output thread | 512 | ME does the mixing; "reduces the load on the main CPU" |
| Sony `sas-streaming` | `sceSasCore` then `sceAudioOutput2OutputBlocking` on one sound thread, double buffer | 736 | both buffers 64-byte aligned |
| `PSPRuntime` | — | — | no audio code at all |

The common shape is clear: **one dedicated output thread per channel, a double buffer, and a blocking call**.
The references differ in where decoding and mixing happen, which is the actual design choice for this phase.

## Media Engine: what it means for audio

The hardware facts and the rules for running code on the ME are in
[§10.2](../memory-map.md#102-media-engine-the-second-mips-core). For this phase, five consequences matter:

1. **The firmware already uses the ME for audio.** SAS (up to 32 voices, 44.1 kHz, ADPCM/PCM/ATRAC3) runs
   mainly on the ME, and `sceSasCoreWithMix` exists to let it mix and then hand the result to Simple Audio.
   `[external: Sony SAS Overview]`
2. **Taking the reset vector disables that.** A session that owns the ME loses `sceSas`, `sceAtrac` and
   `sceMpeg` until reboot. If the game needs an ATRAC or MPEG soundtrack, the ME is not available for a custom
   mixer. `[practice; measured: PSP-3000, large 3D port, via skill]`
3. **The ME is not a drop-in fast CPU.** No VFPU, no libc, no syscalls, no GE, a 16 KB non-coherent D-cache and
   an exception handler that must exist before the first bug. `[public: mcidclan/me-tiny-lib]`
4. **Suspend is the expensive part.** With a live ME loop and the firmware's handler gone, a suspend powered the
   console off; the fixes are `scePowerLock(0)` for the session or replacing the `SceMeRpc` sysevent handler
   with one that parks the ME. `[measured: PSP-3000, large 3D port, via skill]`
5. **The payoff is real but workload-specific.** Moving mixer + resampler + time-stretcher to the ME saved
   5.8 ms/frame (85.8 → 80.0 ms) and cut main-CPU audio cost from 135 to 30 ms per second of game time.
   `[measured: PSP-3000, large 3D port, via skill]` Our 2D workload has to earn its own number.

Three routes, in increasing order of risk:

| route | mechanism | main-CPU cost | prerequisites | gives up |
|---|---|---|---|---|
| **A — main-CPU mixer** | mix/resample into 1–2 `sceAudioChReserve` channels; feeder thread does the blocking output | mixer time, measured not assumed | none; works with the miniaudio custom backend | CPU time only |
| **B — `sceSas`** | `__sceSasInit`/`__sceSasSetVoice`/`__sceSasCoreWithMix` from `pspsascore.h`; ME mixes up to 32 voices; feed the result to a regular channel or Simple Audio | voice setup + output only | VAG/ADPCM/PCM assets; raylib `Sound` needs a voice mapping | arbitrary PCM mixing, per-sound pitch semantics raylib expects |
| **C — owned ME mixer** | reset-vector takeover, custom mixer/resampler in ME code | near zero once running | kernel mode (kubridge/ARK or kernel PRX), stub with a stack, exception handler, watchdog, suspend/resume handling, config gate | `sceSas`, `sceAtrac`, `sceMpeg`; PPSSPP testing |

Route A is the smallest path to correct audio and the one that fits a miniaudio backend. Route B is the only
supported way to use the ME without losing the firmware decoders. Route C is an optimisation to consider only
after A or B is measured and found short. The phase decision is which one the profile justifies, not which one
is cleverest.

## Candidate techniques — not recipes yet

Per [§13](../memory-map.md#13-working-method-sprints-features-recipes) a recipe is written only after the
technique worked on hardware. These are the candidates, each with the measurement that would promote it.

| candidate | mechanism | promote when |
|---|---|---|
| Double-buffered channel | reserve one channel at 1,024 frames; two PCM buffers; blocking output on a feeder thread | latency and underrun count are stable over five minutes |
| Decoder/feeder split | low-priority decode+read thread fills a ring; high-priority feeder only blocks on output | card stalls stop appearing as frame spikes (the 186–243 ms spike signature) |
| SPSC PCM ring with generation counter | decoder owns the write index, feeder the read index; track change bumps a generation and snaps the ring | track changes are gapless and no torn buffer is audible |
| Persistent audio threads | create once per session; post requests; retry bring-up on a timer | a failed start recovers without restarting the game |
| Offline resampling | bake assets to 44.1 kHz; for runtime pitch use 16.16 fixed-point linear interpolation | measured cheaper than the SRC channel for the same quality |
| SRC channel, reserved once | `sceAudioSRCChReserve` at start-up and keep it for the session | it survives five minutes and repeated track changes |
| 64-byte-aligned, written-back buffers | align every PCM buffer; write back the range before the blocking call; consider the uncached alias used by `triAt3` | no stale-cache noise; alignment assert never fires |
| `sceSasCoreWithMix` on the ME | map `Sound` voices to SAS voices; ME mixes; main CPU calls `__sceSasCoreWithMix` and outputs | main-CPU mixer cost disappears from the frame profile and assets fit the SAS formats |
| ME takeover, config-gated | reset-vector stub, shared uncached queues, heartbeat then shadow mode, main-CPU fallback | heartbeat, shadow hashes and a live run all pass on device, and suspend/resume is handled |
| Hardware volume | per-channel pan/volume in the blocking call instead of scaling every sample | identical output with less mixer work |

## Exit criteria

- [ ] sounds and one streamed track play simultaneously, at 60 fps, measured
- [ ] audio thread CPU cost in the profile, and the main thread free of `sceAudio`/`sceSas` calls
- [ ] five minutes without dropout or underrun, counters confirming
- [ ] assets loaded from the stick survive a slow-mount case (the firmware's own fallback behaviour is the model)
- [ ] pitch/resample cost measured, not assumed
- [ ] a clean exit stops the threads, releases the channels, and leaves no audio thread running
- [ ] the Media Engine route is recorded as a decision, with the number that justified it (or the reason it was
      deferred)

## Sprints and features

None yet. Use the [sprint template](../templates/sprint.md).
