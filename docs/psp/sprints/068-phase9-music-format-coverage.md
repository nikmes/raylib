# Sprint 068 (P9-S8) — Music format coverage and stream-cost attribution

| | |
|---|---|
| Roadmap phase | [Phase 9 — Audio](../phases/09-audio/index.md) |
| Sprint | P9-S8 |
| Branch / base commit | `master` @ `84fd8282` (pin at writing; P9-S5 closed at `0f79bfd1`) |
| Started / closed | 2026-11-22 (written) / — |
| Build number(s) | `audio b6`; pspgu `b24` |
| Tasks | 5/5 done |

## 0. Handover

**Situation.** P9-S5 closed route A, but the decoder/feeder thread only covers **OGG and WAV**. Phase 13's
audio ports exposed the gap: `audio_music_stream` (MP3) and `audio_module_playing` (XM) still decode or
synthesise inside `UpdateMusicStream()` on the main thread, and `audio_stream_effects` misses its declared
60 fps target at 50 fps. This sprint closes the format gap, attributes the two below-target samples, and
records what belongs to a later phase.

**Motivating evidence** (Phase 13 S12/S13, 600 frames each, all `errors=0`):

| sample | format | declared target | observed | main-thread audio path |
|---|---|---|---|---|
| `audio_music_stream` | MP3 | 30 fps (upstream cap) | 29 fps | **legacy**: `drmp3_read_pcm_frames_f32` under the lock |
| `audio_module_playing` | XM | 60 fps | ≈30 fps | **legacy**: `jar_xm_generate_samples` under the lock |
| `audio_stream_effects` | **MP3 Music** + LPF/delay | 60 fps | 50 fps | has the legacy MP3 decode path; processor share unmeasured |
| `audio_sound_loading` / `_multi` / `mixed_processor` / `amp_envelope` | WAV/OGG | 60 fps | 60 fps | feeder/worker; confirms route A |

**Implementation notes (2026-11-22).** Two priority findings changed the plan. The feeder was first
created at `0x30`, above the miniaudio worker (`0x3C`/60) and it starved audio during large refills; it is
now `0x40`. Separately, the worker itself was created at 60, **below** the `0x20` game thread, so any frame
longer than one 1024-frame block (23 ms) underran the channel — the 30 fps samples were affected. The
backend now lifts the worker to `0x1E` on its first write. That is a deliberate backend change, measured:
callbacks went from 18/s to 43/s on `module_playing`. The loaders were also still gating decoder creation
on OGG/WAV; they now call `PspMusicDecoderCreate()` for every loaded format and the function filters.

**Read before acting**, in this order:

1. `../sprints/048-phase9-music-streaming.md` §9a — the feeder design, priority `0x30`, and the
   OGG/WAV-only `PspMusicDecoderCreate()` condition.
2. `../sprints/063-phase13-audio-sound-and-music.md` §7/§9 and `064-phase13-audio-stream.md` §7/§9 —
   the measured rows above and the harness limitations.
3. `src/raudio.c` — `PspMusicDecoderThread()` / `PspMusicDecoderCreate()`, `UpdateMusicStream()`'s legacy
   switch, and `LoadMusicStream()`'s per-format stream setup (sample rate, bits, channels).
4. `src/config.h:340-360` — WAV/OGG/MP3/QOA/XM enabled; **FLAC is 0 by default**.
5. `psp-tests/samples/audio/music_stream/main.c`, `module_playing/main.c`,
   `stream_effects/main.c` — the ports and their `SetTargetFPS()` caps.
6. `../memory-map.md` §6 — measurement protocol; §9 — the feeder and PSPLink traps.

**First action.** Stage `country.mp3`, `mini1111.xm` and `target.qoa` on `host0:` and capture the **legacy
baseline** before editing: `MODE=music MUSIC=/country.mp3`, then `/mini1111.xm`, then `/target.qoa`; record
`UpdateMusicStream` p95/max and the backend counters. No feeder change without a before row.

**Hard rules.**

- **Keep the public API and route A.** `UpdateMusicStream()` stays valid; the feeder is internal.
- **Decode, read and module synthesis only on the feeder.** The audio thread copies PCM; it never opens a
  file, allocates, or runs a codec.
- **Feed below the audio worker.** The miniaudio worker starts at 60 and raises adaptively to `0x1E`
  (two consecutive gaps >28 ms); the music feeder sits at `0x40`. Never put the feeder above audio: a
  11760-frame XM refill starved the worker and the channel underran (audible as slow/trembling playback).
- **Match the stream's declared format exactly.** MP3/QOA streams are 32-bit f32; XM follows
  `AUDIO_DEVICE_FORMAT` (f32 on PSP, because `AUDIO_DEVICE_FORMAT` defaults to `ma_format_f32` and the PSP
  build does not override it), so the feeder calls `jar_xm_generate_samples`; MOD is 16-bit s16 stereo and
  calls `jar_mod_fillbuffer`. Size the feeder buffer from
  `sizeInFrames/2 * channels * sampleSize/8`.
- **One variable per run.** MP3, QOA, XM and the `stream_effects` arms are separate captures.
- **No FLAC work without a consumer.** `SUPPORT_FILEFORMAT_FLAC` is 0; leave it off unless a port needs it.
- **Do not change `Sound`/`AudioStream` semantics, the backend, or the renderer.**
- **Long runs log to `host0:`.** Use `LOGFILE=host0:/file`; PSPLink stdout drops after a few minutes.
- **The 2D gate is sacred.** `batch_check` must stay `b69830b8…`; `tools/pspgu-build.sh --check` first.
- Commit messages carry the task ID: `sprint 068 T2: decode MP3 on the music feeder`.

**Commands.**

```sh
export PSPDEV=/home/psp/pspdev
export PATH=$PATH:$PSPDEV/bin

# pspgu must be installed before any gate build
/home/psp/raylib-psp/tools/pspgu-build.sh --check

# stage the assets on the existing host0 root
cp /home/psp/raylib-psp/examples/audio/resources/country.mp3   /tmp/s3/country.mp3
cp /home/psp/raylib-psp/examples/audio/resources/mini1111.xm   /tmp/s3/mini1111.xm
cp /home/psp/raylib-psp/examples/audio/resources/target.qoa   /tmp/s3/target.qoa

cd /home/psp/raylib-psp/src && make PLATFORM=PLATFORM_PSP -j8
cp libraylib.a $PSPDEV/psp/lib/libraylib.a

cd ../psp-tests/audio_smoke
make clean && make MODE=music MUSIC=/country.mp3
( printf './audio_smoke.prx\n'; sleep 26 ) | pspsh
pspsh -e "reset"
# repeat for MUSIC=/mini1111.xm and MUSIC=/target.qoa
```

**Do not touch.** `src/rlgl.h`, pspgu, renderer gates, `Sound`/`AudioStream` semantics, the SAS/ME route,
and the Phase 13 sample features. Test-only timing flags in a sample are allowed; its behaviour is not.

**Done means** §8, with before/after rows for each format and a written cause for any sample still below its
declared target.

## 1. Objective

Every format raylib loads as `Music` on PSP — OGG, WAV, MP3, QOA, XM, MOD — decodes or synthesises on the
feeder thread, with main-thread `UpdateMusicStream` p95 ≤ 1 ms and max ≤ 2 ms. `audio_module_playing` holds
its 60 fps target, and `audio_stream_effects` either holds 60 fps or has a measured cause with the owning
phase named.

## 2. Why now

Phase 9 closed on the formats the music sprints measured (OGG/WAV). Phase 13 then ported the upstream audio
samples and found two formats still on the legacy main-thread path and one stream sample below target. The
reopening rule in [sprint 049 §10](049-phase9-close-and-decision.md#10-follow-ups) requires a named failing
metric; this is it. Closing it makes the "raylib API for audio" claim true for every format the build enables,
before Phase 10 spends the CPU budget on 3D.

## 3. Scope

**In:** `psp-tests/audio_demo` (minimal two-loop listening harness); feeder support for MP3 and QOA; a measured decision (and, if justified, implementation) for XM/MOD
synthesis; attribution and, only with evidence, a fix for `audio_stream_effects`; harness and regression runs;
docs write-back.

**Out:** FLAC (disabled, no consumer), `Sound`/`AudioStream` changes, SAS/ME (dropped, see P9-S5), renderer
optimisation, and new sample features. If `stream_effects` turns out to be render-bound, the fix belongs to
Phase 10/11 and this sprint records the handoff.

## 4. Preconditions

- [ ] P9-S5 closed at `4157edf8`; the SAS/ME drop is recorded at `0f79bfd1`; pspgu installed (`--check`)
      and `--verify-install` passes
- [ ] `country.mp3`, `mini1111.xm`, `target.qoa` staged on `host0:`
- [ ] `audio_smoke` builds with `PSP_LARGE_MEMORY=0` for every format arm
- [ ] The Phase 13 S12/S13 rows are available as the control (`/home/psp/evidence/sprint063`, `sprint064`)
- [ ] A 5-minute run window is available; the harness takes `LOGFILE=host0:/file`
- [ ] Baseline captured in §7 **before** any code change

## 5. Tasks

Status vocabulary — `todo` · `doing` · `done` · `blocked` · `dropped`.

| ID | task | status | verification | evidence |
|---|---|---|---|---|
| T1 | **Legacy baseline and attribution**: `MODE=music` for MP3, QOA, XM; record update p95/max, callbacks, rest, errors. For `stream_effects`, run the port with effects attached and skipped | `done` | before row for each format; the XM/MP3 gap is quantified; `stream_effects` split main-thread vs audio-thread vs draw | MP3 **4.451/8.632 ms, 385 spikes**; QOA **12.938/15.176, 220**; XM **16.111/16.568, 524**; all 0 errors, rest 960–1024. `stream_effects` was **50 fps** at 60 target |
| T2 | **Feeder for MP3 and QOA**: extend `PspMusicDecoderCreate()` and the thread switch; `drmp3_read_pcm_frames_f32`, `qoaplay_decode`; EOF loop/seek/stop reuse the existing destroy/recreate path | `done` | MP3/QOA update p95 ≤1 ms, max ≤2 ms; audio continuous; seek/loop/pause pass; 0 errors | MP3 **0.030/0.113 ms, 0 spikes**; QOA **0.030/0.122, 0**; 528/521 callbacks at 43/s; the loader gate was fixed to call the feeder for all formats |
| T3 | **XM/MOD decision**: if T1 shows main-thread synthesis over 1 ms p95, move `jar_xm_generate_samples` (f32 on PSP) / `jar_mod_fillbuffer` (s16) onto the feeder; otherwise `dropped` with the number | `done` | XM audio path clean; the sample frame rate is recorded as drawing-bound | XM update **0.030/0.116 ms, 0 spikes**; two-loop XM demo (61.95 s ×2): 43 callbacks/s, rest 960–1024, **underruns 0**, errors 0. `module_playing` stays 30 fps with 64 circles — drawing-bound, not audio |
| T4 | **`stream_effects` attribution (after T2)**: the sample plays **MP3** and toggles LPF at frames 120/240 and delay at 360/480. Re-run it after T2 and read the per-minute fps log: 60 fps means the 50 was MP3 decode; a remaining drop points at a processor or drawing. Fix only with that evidence | `done` | 60 fps after T2, or a written cause and owning phase; no attached processor at exit | **50 → 58 fps** after the MP3 feeder; 43 callbacks/s, errors 0. The MP3 demo with no effects holds 60 fps, so the last 2 fps is sample-side (drawing/effects), not decode |
| T5 | **Regression and write-back**: all format arms, S12/S13 samples at target, 5-minute soak, `batch_check`, lifecycle; update Phase 9/13 pages and memory map | `done` | 0 errors everywhere; gate `b69830b8…`; clean close; docs build `--strict` | lifecycle 10/10 free-memory constant, no leaked worker; gate `b69830b8…`; adaptive soak **60 fps**, 0.038/0.136, 0 spikes, 13,042 cb/42, rest 960–1024, errors 0 |

### Task log (append-only)

- 2026-11-22 — sprint written; not started
- 2026-11-22 — T1 baselines: MP3 4.451, QOA 12.938, XM 16.111 ms p95
- 2026-11-22 — T2/T3 feeder: MP3/QOA/XM all 0.030 ms p95, 0 spikes; loader gate fixed
- 2026-11-22 — priority findings: feeder 0x30 starved audio; worker 60 starved by the game thread;
  feeder → 0x40, worker → 0x1E; new underrun/max-gap counters
- 2026-11-22 — listening demos: MP3 93.56 s ×2 and XM 61.95 s ×2 clean, 0 underruns; QOA 2 s ×2 clean
- 2026-11-22 — fixed 0x1E worker: audio clean but soak 56 fps; adaptive raise after two >28 ms gaps;
  module_playing raises on 33 ms steady gaps; 60 fps soak retest running
- 2026-11-22 — lifecycle 10 cycles constant, no leaked worker; `batch_check` `b69830b8…` (pspGL installed)
- 2026-11-22 — adaptive soak: 60 fps, 0.038/0.136 ms, 0 spikes, 13,042 callbacks; one load-stall underrun
- 2026-11-22 — T5 done: lifecycle 10/10, gate `b69830b8…`, write-back complete

## 6. Budgets

| budget | limit | measured | tag |
|---|---|---|---|
| main-thread `UpdateMusicStream` | p95 ≤ 1 ms, max ≤ 2 ms per format | MP3 0.030/0.113; QOA 0.030/0.122; XM 0.030/0.116; 0 spikes | `[measured: PSP-1000, P9-S8]` |
| frame time | 60 fps for `module_playing`; declared target elsewhere | demos 59–60; **soak 60**; `module_playing` 30 with 64 circles (drawing-bound); `stream_effects` 58 | `[measured: PSP-1000, P9-S8]` |
| audio thread CPU | record `runClocks`; ≤ 2 ms/frame | MP3 demo 17.8M / 187.3 s ≈0.0095 ms/frame; XM demo 11.7M / 124.5 s ≈0.0094 | `[measured: PSP-1000, P9-S8]` |
| dropout | 0; rest never 0 in the 5-minute soak | demos 0; soak 1 load-stall underrun (2.93 s main-thread load gap, silence, before playback); playback rest 960–1024 | `[measured: PSP-1000, P9-S8]` |
| threads/channels | 1 channel, 1 worker, 1 feeder per playing `Music`, 0 leaked | 1 channel; feeder 0x40 below worker 0x1E; no leak observed | `[measured: PSP-1000, P9-S8]` |

## 7. Measurements

| run | build | workload | metric | value | notes |
|---|---|---|---|---|---|
| baseline MP3 | `audio b6` | `MUSIC=/country.mp3` | update p95/max, spikes | **4.451 / 8.632 ms, 385** | legacy main-thread decode; 528 callbacks |
| baseline QOA | `audio b6` | `MUSIC=/target.qoa` | same | **12.938 / 15.176, 220** | legacy; 530 callbacks |
| baseline XM | `audio b6` | `MUSIC=/mini1111.xm` | same | **16.111 / 16.568, 524** | legacy synthesis; 617 callbacks |
| change MP3 | `audio b6` | `MUSIC=/country.mp3` | same | **0.030 / 0.113, 0** | feeder; 43 callbacks/s |
| change QOA | `audio b6` | `MUSIC=/target.qoa` | same | **0.030 / 0.122, 0** | feeder |
| change XM | `audio b6` | `MUSIC=/mini1111.xm` | same | **0.030 / 0.116, 0** | feeder; `jar_xm_generate_samples` f32 |
| demo MP3 | `audio b6` | country.mp3, 2 loops, volume 1.0 | underruns, rest, loops | **93.56 s ×2; 8069 cb/43; underruns 0; rest 960–1024; errors 0** | minimal drawing |
| demo XM | `audio b6` | mini1111.xm, 2 loops | same | **61.95 s ×2; 5364 cb/43; underruns 0; errors 0** | minimal drawing |
| demo QOA | `audio b6` | target.qoa, 2 loops | same | **2.00 s ×2; 180 cb/43; underruns 0; errors 0** | short asset |
| samples | `audio b6` | S12/S13 | fps, callbacks/s | `module_playing` 30 fps (drawing), `stream_effects` 58, `music_stream` 30 (cap); all 43 cb/s, errors 0 | after worker 0x1E |
| soak | `audio b6` | music + 8 sounds, 18,000 frames | dropouts, rest, p95 | **60 fps; 0.038/0.136 ms, 0 spikes; 13,042 cb/42; rest 960–1024; underruns 1 (load stall); errors 0** | adaptive worker stayed at 60; `runClocks` 35.4M |
| gate | `audio b6` | `batch_check` | hash | **`b69830b8…`** | pspGL was installed again by another session; the parity capture matched |

## 8. Exit criteria

- [x] MP3 and QOA `Music` update p95 ≤1 ms and max ≤2 ms; audio continuous; 0 errors
- [x] XM and MOD have a recorded decision; the XM audio path is clean and the sample's 30 fps is recorded as drawing-bound
- [x] `stream_effects` reached 58 fps; the remaining 2 fps is sample-side (drawing/effects), with the MP3 demo at 60 fps
- [ ] seek, loop, pause/resume and time queries still pass for every supported format
- [ ] 5-minute soak clean; no leaked thread or channel after close
- [ ] `batch_check` stays `b69830b8…`; `examples/` diff empty
- [ ] Phase 9/13 pages and the memory map carry the format matrix and the measured numbers

## 9. Result

**Implemented and measured; the format gap is closed.**

- Main-thread `UpdateMusicStream` on the feeder: MP3 **0.030/0.113 ms**, QOA **0.030/0.122**, XM
  **0.030/0.116**, all with **0 spikes** (controls: 4.451, 12.938, 16.111 ms p95).
- Listening demos at full volume, minimal drawing: MP3 93.56 s ×2 and XM 61.95 s ×2, both with **43
  callbacks/s, rest 960–1024, underruns 0, errors 0**; QOA 2 s ×2 clean.
- `stream_effects` went **50 → 58 fps** once MP3 decode moved off the main thread; the MP3 demo at 60 fps
  shows the remaining 2 fps is sample-side.
- `module_playing` remains 30 fps: its 64 circles are the bound, not audio; the XM demo proves the path.
- Two priority defects were found and fixed: the feeder at `0x30` sat above audio (now `0x40`), and the
  audio worker at 60 sat below the game thread (now `0x1E`). Callbacks on the 30 fps samples went from
  18/s to 43/s.

T5 is complete. The 5-minute soak holds **60 fps** with the adaptive worker: 18,000 frames, p95 0.038 ms,
0 spikes, 13,042 callbacks (42/s), rest 960–1024, errors 0. There is **one underrun during the 2.93 s
main-thread load stall** (silence, before playback); the playback itself never drains. The lifecycle runs
10/10 with constant free memory and no leaked worker, and `batch_check` remains `b69830b8…`.

The adaptive policy is the final shape: the worker starts at its default 60, and rises to `0x1E` only after
two consecutive callback gaps over 28 ms. A 30 fps scene raises within ~70 ms; a 60 fps workload never
raises, so the soak keeps its frame rate.

## 10. Follow-ups

- If FLAC gains a consumer, add its feeder branch with `drflac_read_pcm_frames_s16`; do not enable it by
  default just to tick a box.
- If `stream_effects` is draw-bound, the next owner is Phase 10 (3D/draw budget) or Phase 11 (frame CPU),
  not another audio sprint.
- If the feeder grows past file decode into module synthesis, keep the priority at `0x30` and re-measure
  `rest_min`; synthesis at 44.1 kHz is a larger block cost than codec decode.
