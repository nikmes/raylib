# Phase 9 — Audio

| | |
|---|---|
| State | **done** — route A (P9-S1…S5); SAS/ME (P9-S6/S7) deferred with numbers (2026-11-22); **P9-S8 written** for MP3/QOA/XM feeder coverage |
| Depends on | Phase 1 (a running build) — independent of the renderer phases |
| Exit criterion | several sounds plus one streamed track on the device, 60 fps held, audio thread cost measured, five minutes without a dropout, no `sceAudio` call in the steady-state frame loop |

## What this phase is about

`raudio` is built on miniaudio, and **miniaudio has no PSP backend** — zero references in the whole library.
The PSP build currently compiles raudio out (`src/Makefile:560`, `-DSUPPORT_MODULE_RAUDIO=0`) because miniaudio
includes `<dlfcn.h>` for runtime linking; the 5.5 port's boot log reads `raudio:.... not loaded (optional)`.
Audio has to be built and enabled first, then given a device route: the phase does not start from a working
audio module.

## What has to be true before starting

- [x] A build that runs for minutes without a crash
- [x] A way to measure the audio thread separately from the frame loop (`sceKernelReferThreadRunStatus().runClocks`)
- [x] raudio compiles and links on PSP with a custom miniaudio backend (P9-S1)
- [x] The asset question settled: Memory Stick (`ms0:`) or `host0:` under PSPLink

## Tasks

- [x] Choose the integration: route A is miniaudio's custom backend (`MA_ENABLE_ONLY_SPECIFIC_BACKENDS` +
      `MA_ENABLE_CUSTOM` + `MA_NO_RUNTIME_LINKING`; `ma_backend_callbacks`' init/start/stop/uninit **and
      `onDeviceWrite`/`onDeviceDataLoop`**) versus driving `sceAudio` straight from `raudio.c`. Routes B/C are
      P9-S6/S7 and do not replace route A until measured — [open question 6](../../memory-map.md#12-open-questions)
- [x] Keep the application/mixer contract **f32, stereo, rate from the backend**; the PSP backend reports
      **s16, 44100, stereo** as its internal format and a **1024-frame** period. Do not set
      `AUDIO_DEVICE_FORMAT` to s16 — raylib's mixer writes `float` (`src/raudio.c:2468-2645`); measure the
      f32→s16 conversion miniaudio performs on the audio thread
- [x] Reserve a regular channel with `sceAudioChReserve()`: 64–65,472 samples, a multiple of **64 samples**
      (`PSP_AUDIO_SAMPLE_ALIGN`), 64-byte-aligned buffer memory; a stereo s16 buffer is `samplecount*4` bytes.
      `sceAudioOutput2*`/`sceAudioSRCChReserve()` take 17–4,111 samples and are the same single output channel
- [x] Put `sceAudioOutputPannedBlocking()` on the **audio thread**, never in the frame loop — it paces to playback,
      and the thread must not be starved for more than a few hundred microseconds. In route A that thread is
      miniaudio's; `onDeviceWrite()` is where the blocking call belongs
- [x] Use at least a double buffer; the blocking call returns when the block is accepted, not when it has finished
      playing
- [x] Copy miniaudio's frames into a static 64-byte-aligned PCM buffer before the output call: the pointer
      `onDeviceWrite()` receives is a stack buffer (`src/external/miniaudio.h:20742`)
- [x] Split decoder/reader from feeder **only if the measurement needs it**: low-priority decoder + card reader,
      high-priority feeder that only copies from a PCM ring and calls the blocking output. Route A's miniaudio
      audio thread is already the feeder; do not add a second one without evidence
- [x] Keep `UpdateMusicStream()` as the public API; measure its main-thread decode first, then move decode/read
      behind an internal decoder thread while preserving the public call
- [x] Prefer a regular 44.1 kHz channel and CPU resampling; if using `sceAudioSRCChReserve()`, reserve it **once**
      and keep it — re-reserving after release failed repeatedly on hardware in another project
- [x] Software-mix sounds into one or two channels, leaving hardware channels free for streamed music
- [x] Stream music: decode ahead on the decoder thread into a ring buffer (~1 s), never open a file from the
      feeder or the miniaudio callback; batch reads with a 64 KB `setvbuf`
- [x] Respect memory-stick cost: ~17 ms per `sceIoOpen`, so few long-lived handles and large reads
- [x] `SetSoundPitch` is resampling — profile it rather than assuming it is free
- [ ] Make audio threads persistent; retry failed bring-up on a timer and log every failure
- [x] Decide the Media Engine route (see below): route A (miniaudio custom backend), `sceSas` through
      `pspsascore`, or an owned ME mixer. Record the decision and the measurement behind it
- [x] If the ME is taken over: gate it behind a config file next to the EBOOT, keep route A compiled in,
      and handle suspend/resume — **not applicable**: the ME route is deferred (T7)
- [x] Define the exit path: stop and join the audio threads, release channels, then exit — in that order

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
tree (reading aid only, [§11.4](../../memory-map.md#the-sony-sdk-what-it-is-for-and-what-it-is-not)), and the local
psp-developer skill (`references/audio.md`, `references/media-engine.md`). Numbers measured elsewhere carry their
hardware.

| fact | value | tag |
|---|---|---|
| Output channels | 8 regular channels, reserved by number (0–7) or `PSP_AUDIO_NEXT_CHANNEL` | `[public: pspaudio.h]` |
| Regular-channel block | 64–65,472 samples, a multiple of **64 samples** (`PSP_AUDIO_SAMPLE_ALIGN`); pspaudiolib uses **1,024** | `[public: pspaudio.h]` |
| Output2/SRC block | 17–4,111 samples; Sony `at3mix` uses **512**, `sas-streaming` **736** | `[public: pspaudio.h]` + `[external: Sony samples]` |
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
| miniaudio hooks | custom backends use `ma_backend_callbacks`: context/device init/start/stop/uninit **and `onDeviceWrite`/`onDeviceDataLoop`**; select with `MA_ENABLE_ONLY_SPECIFIC_BACKENDS` + `MA_ENABLE_CUSTOM` + `MA_NO_RUNTIME_LINKING`, pass `{ ma_backend_custom }` and `ctxConfig.custom` | `[public: src/external/miniaudio.h]` |
| PSP link line | `libraylib.a` is static: append `-lpspaudio -lpthread -latomic` **after** `build.mak`; do not add `-lpspuser`; `libatomic.a` supplies miniaudio's 64-bit atomics; no `dlfcn` with `MA_NO_RUNTIME_LINKING` | `[measured: P9-S1, 2026-11-21]` |
| PSP silent backend (P9-S1) | custom backend; client f32 -> internal s16; 44100; 1024-frame period x2; 128 KiB worker stack; 418 callbacks in 10.03 s (41/s); `raudio.o` text 422,129 B; app ELF text 1,245,592 B | `[measured: PSP-1000, P9-S1, 2026-11-21]` |
| PSP worker thread | default/32 KiB stack entered the audio loop and died before playback; `ma_device_init` waited on `stopEvent` forever; 128 KiB fixed it. Priority retained: `ma_thread_priority_normal` | `[measured: PSP-1000, P9-S1, 2026-11-21]` |
| PSP output backend (P9-S2) | one regular channel (unit: **7**), 1024-frame blocks, two alternating 64-byte-aligned buffers with cache writeback; 433 callbacks/10.03 s (43/s); rest 960–1024; errors 0; `runClocks` 410,966/411,441; 1 kHz tone audible | `[measured: PSP-1000, P9-S2, 2026-11-21]` |
| PSP `Sound` API (P9-S3) | file+memory loads, aliases, Wave copy/crop/format/samples, `UpdateSound`, master/volume/pan/pitch all pass; levels loud-left/quiet-right; pitch 0.5/2.0 stops at frames 119/30; mix16/32 at 60 fps, rest 960–1024, errors 0; 32/32 sounds valid with 30,440 B free in the 4 MiB test heap | `[measured: PSP-1000, P9-S3, 2026-11-21]` |
| PSP close and route decision (P9-S5) | 5 runs × 10 init/close all ready; free 782,336 / max free 524,288 constant (zero drift); `thlist` after close has no `pthread*` worker; 5-minute soak 18,000 frames, update p95 0.028 / max 0.152, 0 spikes, 13,033 callbacks (42/s), rest 960–1024, errors 0; seven renderer captures byte-identical; route A adopted, SAS/ME dropped | `[measured: PSP-1000, P9-S5, 2026-11-22]` |
| PSP `Music`/`AudioStream` (P9-S4/S4a) | OGG/WAV API matrix passes; OGG `setvbuf` 64 KiB; AudioStream update/s16-callback/f32-processor all 58–60 fps, errors 0. Main-thread decode failed the gate (OGG p95 16.908 / max 21.622, 57 spikes; WAV 13.172 / 15.124, 110 spikes), so a per-track decoder thread now decodes outside `AUDIO.System.lock`. Final main-thread update: OGG 0.027 / 0.112 ms, WAV 0.027 / 0.099 ms, 0 spikes. 5-minute soak: 18,000 frames, 0.027 / 0.165 ms, 0 spikes, 13,032 callbacks, rest 960–1024, errors 0 | `[measured: PSP-1000, P9-S4a, 2026-11-21]` |

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
[§10.2](../../memory-map.md#102-media-engine-the-second-mips-core). For this phase, five consequences matter:

1. **The firmware already uses the ME for audio.** SAS (up to 32 voices, 44.1 kHz, VAG/ADPCM + raw PCM) runs
   mainly on the ME, and `__sceSasCoreWithMix` exists to let it mix and then hand the result to Simple Audio.
   ATRAC3 is decoded by `sceAtrac`, not by SAS. `[external: Sony SAS Overview; public: pspsascore.h]`
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
| **A — miniaudio custom backend** | mix/resample on the main CPU into 1–2 `sceAudioChReserve` channels; miniaudio's audio thread does the blocking output | mixer time, measured not assumed | none; this is the route-A build | CPU time only |
| **B — `sceSas`** | `__sceSasInit`/`__sceSasSetVoice`/`__sceSasCoreWithMix` from `pspsascore.h`; the firmware mixes up to 32 voices; feed the result to a regular channel or Simple Audio | voice setup + output only | VAG/ADPCM/PCM assets; raylib `Sound` needs a voice mapping | voice-model differences; 32-voice cap; fixed 44.1 kHz |
| **C — owned ME mixer** | reset-vector takeover, custom mixer/resampler in ME code | near zero once running | kernel mode (kubridge/ARK or kernel PRX), stub with a stack, exception handler, watchdog, suspend/resume handling, config gate | `sceSas`, `sceAtrac`, `sceMpeg`; PPSSPP testing |

Route A is the smallest path to correct audio and the one that fits a miniaudio backend. Route B is the only
supported way to use the ME without losing the firmware decoders. Route C is an optimisation to consider only
after A or B is measured and found short. The phase decision is which one the profile justifies, not which one
is cleverest.

## Candidate techniques — not recipes yet

Per [§13](../../memory-map.md#13-working-method-sprints-features-recipes) a recipe is written only after the
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
| `sceSasCoreWithMix` on the ME | map `Sound` voices to SAS voices; the firmware mixes; the audio thread calls `__sceSasCoreWithMix` and outputs | main-CPU mixer cost disappears from the frame profile and assets fit the SAS formats |
| ME takeover, config-gated | reset-vector stub, shared uncached queues, heartbeat then shadow mode, main-CPU fallback | heartbeat, shadow hashes and a live run all pass on device, and suspend/resume is handled |
| Hardware volume | per-channel pan/volume in the blocking call instead of scaling every sample | identical output with less mixer work |

## Exit criteria

- [ ] sounds and one streamed track play simultaneously, at 60 fps, measured
- [ ] audio thread CPU cost in the profile, and the **steady-state frame loop** free of `sceAudio`/`sceSas` calls
- [ ] five minutes without dropout or underrun, counters confirming
- [ ] assets loaded from the stick survive a slow-mount case (the firmware's own fallback behaviour is the model)
- [ ] pitch/resample cost measured, not assumed
- [ ] a clean exit stops the threads, releases the channels, and leaves no audio thread running
- [ ] `UpdateMusicStream()` remains the public API and its decode cost is measured
- [ ] the Media Engine route is recorded as a decision, with the number that justified it (or the reason it was
      deferred)

## Sprints and features

Route A is delivered by P9-S1…S5. P9-S6 measures `sceSas`/ME against it; P9-S7 is **conditional** and is
`dropped` if S6 authorizes no route.

| sprint | file | scope | state |
|---|---|---|---|
| P9-S1 — Enable raudio and the silent backend | [045](../../sprints/045-phase9-enable-raudio.md) | build flags, `dlfcn`/atomics/pthread, silent custom backend, link line | **done** — 418 callbacks/41 per s; gate `cf43ce6d…` |
| P9-S2 — Custom backend and first output | [046](../../sprints/046-phase9-custom-backend.md) | channel reserve, two aligned buffers, `onDeviceWrite`, one sound | **done** — 433 callbacks/43 per s; tone audible; gate `cf43ce6d…` |
| P9-S3 — `Sound` API and mixing | [047](../../sprints/047-phase9-sound-api.md) | play/stop/pause, volume/pan/pitch, wave/alias/update, 16 sounds | **done** — API matrix, mix16/32, pitch, levels, heap; gate `cf43ce6d…` |
| P9-S4 — `Music` and `AudioStream` streaming | [048](../../sprints/048-phase9-music-streaming.md) | `UpdateMusicStream`, decoder thread, 5-minute soak | **done (S4a)** — OGG 0.027/0.112 ms, WAV 0.027/0.099 ms, 0 spikes; soak 18,000 frames, 0 errors; gate `b69830b8…` |
| P9-S5 — Close, budgets and route decision | [049](../../sprints/049-phase9-close-and-decision.md) | lifecycle, full API matrix, budgets, B/C go/no-go | **done** — route A adopted; SAS/ME deferred; Phase 9 closed |
| P9-S6 — `sceSas`/ME prototype | [050](../../sprints/050-phase9-sas-me-prototype.md) | SAS buffer/voice/CPU measurement, route-C gate | **dropped** — route A meets every budget |
| P9-S7 — Adopt the chosen route | [051](../../sprints/051-phase9-adopt-route.md) | conditional production adoption, route switch, fallback | **dropped** — no route switch needed |
| P9-S8 — Music format coverage and stream-cost attribution | [068](../../sprints/068-phase9-music-format-coverage.md) | feeder for MP3/QOA, XM/MOD decision, `stream_effects` attribution | **written** — Phase 13 exposed the gap |

Use the [sprint template](../../templates/sprint.md) for any added work.

**Close (2026-11-22).** Route A is the shipped audio path. The exit criterion is met on the PSP-1000:
8 sounds plus one streamed OGG/WAV track at 60 fps, audio work measured on its own thread, five minutes
without a dropout, and no `sceAudio` call in the frame loop. The SAS/ME alternative is deferred because
route A has no failing budget; see [sprint 049 §9](../../sprints/049-phase9-close-and-decision.md#9-result).
Reopen P9-S6 only when a named metric fails — a CPU-bound 3D scene where decode/mix is a measurable share
of frame time, more simultaneous `Sound`s than the f32 mixer holds at 60 fps, or an ME/AVC workload — not
because the phase page lists a route.

**P9-S8 (follow-up).** The feeder covers OGG/WAV only. Phase 13's audio ports showed MP3 (`music_stream`) and
XM (`module_playing`) still decode on the main thread and `stream_effects` at 50 fps against a 60 fps target.
[P9-S8](../../sprints/068-phase9-music-format-coverage.md) closes the format gap and attributes the two
below-target samples; it does not reopen the SAS/ME decision.

**Gate-hash correction (P9-S4a).** The canonical `batch_check` gate is `b69830b8…` — the pspgu `b24`
capture archived byte-identical from sprint 015 through sprint 041 and equal to the `par_pspgl` parity
file. The `cf43ce6d…` value quoted in the S1–S3 rows above is not present in any archived capture; it was
a bad control. After pspgu was restored, three fresh captures all produced `b69830b8…`.
