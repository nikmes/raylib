# Sprint 068 (P9-S8) — Music format coverage and stream-cost attribution

| | |
|---|---|
| Roadmap phase | [Phase 9 — Audio](../phases/09-audio/index.md) |
| Sprint | P9-S8 |
| Branch / base commit | `master` @ `84fd8282` (pin at writing; P9-S5 closed at `0f79bfd1`) |
| Started / closed | 2026-11-22 (written) / — |
| Build number(s) | `audio b6`; pspgu `b24` |
| Tasks | 0/5 done |

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

**In:** feeder support for MP3 and QOA; a measured decision (and, if justified, implementation) for XM/MOD
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
| T1 | **Legacy baseline and attribution**: `MODE=music` for MP3, QOA, XM; record update p95/max, callbacks, rest, errors. For `stream_effects`, run the port with effects attached and skipped | `todo` | before row for each format; the XM/MP3 gap is quantified; `stream_effects` split main-thread vs audio-thread vs draw | — |
| T2 | **Feeder for MP3 and QOA**: extend `PspMusicDecoderCreate()` and the thread switch; `drmp3_read_pcm_frames_f32`, `qoaplay_decode`; EOF loop/seek/stop reuse the existing destroy/recreate path | `todo` | MP3/QOA update p95 ≤1 ms, max ≤2 ms; audio continuous; seek/loop/pause pass; 0 errors | — |
| T3 | **XM/MOD decision**: if T1 shows main-thread synthesis over 1 ms p95, move `jar_xm_generate_samples` (f32 on PSP) / `jar_mod_fillbuffer` (s16) onto the feeder; otherwise `dropped` with the number | `todo` | `module_playing` 60 fps if implemented, or a recorded no-op decision; update p95 within budget | — |
| T4 | **`stream_effects` attribution (after T2)**: the sample plays **MP3** and toggles LPF at frames 120/240 and delay at 360/480. Re-run it after T2 and read the per-minute fps log: 60 fps means the 50 was MP3 decode; a remaining drop points at a processor or drawing. Fix only with that evidence | `todo` | 60 fps after T2, or a written cause and owning phase; no attached processor at exit | — |
| T5 | **Regression and write-back**: all format arms, S12/S13 samples at target, 5-minute soak, `batch_check`, lifecycle; update Phase 9/13 pages and memory map | `todo` | 0 errors everywhere; gate `b69830b8…`; clean close; docs build `--strict` | — |

### Task log (append-only)

- 2026-11-22 — sprint written; not started

## 6. Budgets

| budget | limit | measured | tag |
|---|---|---|---|
| main-thread `UpdateMusicStream` | p95 ≤ 1 ms, max ≤ 2 ms per format | | |
| frame time | 60 fps for `module_playing`; declared target elsewhere | | |
| audio thread CPU | record `runClocks`; ≤ 2 ms/frame | | |
| dropout | 0; rest never 0 in the 5-minute soak | | |
| threads/channels | 1 channel, 1 worker, 1 feeder per playing `Music`, 0 leaked | | |

## 7. Measurements

| run | build | workload | metric | value | notes |
|---|---|---|---|---|---|
| baseline MP3 | `audio b6` | `MUSIC=/country.mp3` | update p95/max, callbacks, errors | | legacy path |
| baseline QOA | `audio b6` | `MUSIC=/target.qoa` | same | | legacy path |
| baseline XM | `audio b6` | `MUSIC=/mini1111.xm` | same | | legacy synthesis |
| baseline effects | `audio b6` | `stream_effects` (MP3) | fps per 60-frame segment | | frames 60/120 none, 180/240 LPF, 300/360 none, 420/480 delay |
| change MP3/QOA | `audio b6` | same | same | | feeder |
| change XM | `audio b6` | same | same | | only if T3 authorises |
| change effects | `audio b6` | `stream_effects` (MP3) | fps per segment | | after T2; separates decode from processors |
| soak | `audio b6` | music + 8 sounds, 18,000 frames | dropouts, rest, p95 | | `LOGFILE=host0:` |
| gate | `audio b6` | `batch_check` | hash | | must be `b69830b8…` |

## 8. Exit criteria

- [ ] MP3 and QOA `Music` update p95 ≤1 ms and max ≤2 ms; audio continuous; 0 errors
- [ ] XM and MOD have a recorded decision; if moved to the feeder, `module_playing` holds 60 fps
- [ ] `stream_effects` holds 60 fps or has a measured cause with the owning phase named
- [ ] seek, loop, pause/resume and time queries still pass for every supported format
- [ ] 5-minute soak clean; no leaked thread or channel after close
- [ ] `batch_check` stays `b69830b8…`; `examples/` diff empty
- [ ] Phase 9/13 pages and the memory map carry the format matrix and the measured numbers

## 9. Result

Not started.

## 10. Follow-ups

- If FLAC gains a consumer, add its feeder branch with `drflac_read_pcm_frames_s16`; do not enable it by
  default just to tick a box.
- If `stream_effects` is draw-bound, the next owner is Phase 10 (3D/draw budget) or Phase 11 (frame CPU),
  not another audio sprint.
- If the feeder grows past file decode into module synthesis, keep the priority at `0x30` and re-measure
  `rest_min`; synthesis at 44.1 kHz is a larger block cost than codec decode.
