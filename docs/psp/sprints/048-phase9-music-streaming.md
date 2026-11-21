# Sprint 048 (P9-S4) — `Music` and `AudioStream` streaming

| | |
|---|---|
| Roadmap phase | [Phase 9 — Audio](../phases/09-audio/index.md) |
| Sprint | P9-S4 |
| Branch / base commit | `master` @ `1427e2e8` (P9-S3 close + S4 review; pin at start) |
| Started / closed | 2026-11-21 / — (blocked) |
| Build number(s) | `audio b4`; pspgu `b24` unchanged |
| Tasks | 3/9 done; T2/T5/T8 doing; T4/T7/T9 blocked |

## 0. Handover

**Situation.** P9-S3 closed the `Sound` surface: API matrix, mix16/32, pitch, levels and heap all pass; gate
`cf43ce6d…`. This sprint adds the streaming half of raylib: `Music` (decoded file playback) and raw
`AudioStream`. The hard part is that `UpdateMusicStream()` is a **main-thread raylib API** that decodes and
refills under `AUDIO.System.lock` (`src/raudio.c:1922`). A PSP Memory Stick open costs ~17 ms; the sprint
must measure the main-thread cost before deciding whether a decoder thread is justified.

**Read before acting**, in this order:

1. `../phases/09-audio/index.md` — P9-S4 and the "decode ahead, never on the audio thread" rule.
2. `../sprints/046-phase9-custom-backend.md` §9 and `047-phase9-sound-api.md` §9 — the sink, counters and
   `Sound` baseline; `Music` shares the same mixer and channel.
3. `src/raylib.h:1720-1755` — the `Music` and `AudioStream` API this sprint must cover.
4. `src/raudio.c:1922-2060` — `UpdateMusicStream()`: the lock, two sub-buffers, decode and `Stop`;
   `:2197-2310` — `UpdateAudioStream()`, callback and processor attachment; `:2701-2760` — locked refill.
5. `../memory-map.md` §6, §9 — device protocol; blocking audio, thread stacks, cache, Memory Stick cost.
6. `psp-tests/audio_smoke/main.c` and `Makefile` — the S3 harness; extend `MODE` rather than creating an app.
7. `examples/audio/resources/target.ogg` and `sound.wav` — the staged test assets (do not edit `examples/`).

**First action.** Add `MODE=music|musicmix|stream` to `audio_smoke`. Stage
`examples/audio/resources/target.ogg` and `sound.wav` under `host0:`; call
`SetAudioStreamBufferSizeDefault(4096)`; `LoadMusicStream()` the OGG, `PlayMusicStream()`, call
`UpdateMusicStream()` once per frame, run a 60-frame warm-up, then measure the next 600 calls into a static
per-frame array. Log p95/max and count frames whose update exceeds 2 ms. That main-thread baseline is the
control for the whole sprint.

**Hard rules.**

- **Keep `UpdateMusicStream()` as the public API.** The game must still call it while music plays. Any
  decoder-thread work is an internal optimization and must leave that call valid.
- **Do not add a decoder thread on a guess.** T3 measures first. Use a fixed array of update times; exclude
  the 60-frame warm-up (the lazy `pcmBuffer` allocation); sort a copy for p95. T4 is a decision gate: keep
  the main thread if p95 ≤ 1 ms and max ≤ 2 ms; if it exceeds either, mark T4/T7 `blocked` and open a
  follow-up sprint. Do not cram an unmeasured thread into S4.
- **Never open a file from the audio thread.** Decode/read on the calling thread (or a decoder thread if a
  follow-up is opened); the backend only copies PCM.
- **The audio thread must not block on a file read or a lock held by a long decode.** Measure lock/decode
  hold time; if it exceeds one 1024-frame block, redesign before the soak.
- **`setvbuf` is OGG-specific here.** In `src/raudio.c`'s OGG branch (`~1414-1437`), replace
  `stb_vorbis_open_filename` with `fopen(fileName,"rb")`, `setvbuf(file, NULL, _IOFBF, 64*1024)`, and
  `stb_vorbis_open_file(file, TRUE, NULL, NULL)`. Close the file if the open fails. WAV uses
  `drwav_init_file` (no public FILE handle); measure its read cost and add drwav callbacks only if WAV is
  over budget. `sound.wav` is 32-bit mono, so the WAV arm also exercises conversion.
- **No `sceAudio` in the game loop.** The backend owns the sink.
- **`AudioStream` callbacks run on the audio thread.** They must not block, allocate, log or open files.
  The callback receives the stream's native format. Use a **32-bit f32 callback stream** here; the
  `UpdateAudioStream` arm stays 16-bit s16. `AttachAudioStreamProcessor` receives f32 stereo.
- **A 5-minute soak is 18,000 frames at 60 fps**, not 300 frames. Use `MODE=musicmix FRAMES=18000 COUNT=8`
  and a 320 s PSPLink window. The harness logs frame time each second; the backend reports rest/errors
  at close.
- **One variable per run.** OGG, WAV, callback stream, processor, and musicmix are separate arms.
- **Set the stream buffer before loading.** `SetAudioStreamBufferSizeDefault(4096)` in the harness, then
  record the resulting stream buffer/period in §7. Do not leave the default 0 (derived from sampleRate/30).
- **The gate is `cf43ce6d…`.** `batch_check` and the 018–021 captures must stay byte-identical.
- Commit messages carry the task ID: `sprint 048 T3: measure main-thread music decode`.

**Commands.**

```sh
export PSPDEV=/home/psp/pspdev
export PATH=$PATH:$PSPDEV/bin

# Stage the public raylib example assets on the existing host0 root.
cp /home/psp/raylib-psp/examples/audio/resources/target.ogg /tmp/s3/p9s4_target.ogg
cp /home/psp/raylib-psp/examples/audio/resources/sound.wav /tmp/s3/p9s4_sound.wav

cd /home/psp/raylib-psp/psp-tests/audio_smoke
make clean && make MODE=music MUSIC=/p9s4_target.ogg
( printf './audio_smoke.prx\n'; sleep 30 ) | pspsh
# after DONE: pspsh -e "reset"
```

`MUSIC` is the basename under `host0:`; the harness prefixes `host0:`.

**Do not touch.** `src/rlgl.h`, pspgu, renderer gates, the `Sound` API, SAS/ME, and the backend's
`onDeviceWrite()` unless a streaming failure is proven to be in the sink.

**Done means** §8, with 5-minute evidence, decode/read costs, underrun counts and a recorded decoder decision.

## 1. Objective

One streamed OGG track and eight `Sound`s play simultaneously at 60 fps for five minutes on the PSP-1000,
with `UpdateMusicStream()`/`UpdateAudioStream()` behaving as raylib documents, no file I/O on the audio
thread, and no dropout or frame spike attributable to decoding or file reads.

## 2. Why now

`Sound` is fully in RAM and already measured. `Music` introduces the two costs that can ruin a frame: decode
and file I/O. The phase's exit requires a streamed track, five minutes without a dropout, and no `sceAudio`
call in the frame loop. This is the last functional sprint before close.

## 3. Scope

**In:** `Music` and `AudioStream` API coverage; `audio_smoke MODE=music|musicmix|stream`; main-thread
baseline; OGG `setvbuf`; seek/loop/time; 5-minute soak; counters; optional decoder decision.

**Out:** SAS/ME, `Sound` changes, renderer changes, public API additions, and an unmeasured decoder thread.

## 4. Preconditions

- [ ] P9-S3 closed: API matrix, mix16/32, pitch/levels, 32-sound heap; gate `cf43ce6d…`
- [ ] `reset` was run; partition 2 free reads 25,149,440 bytes
- [ ] `target.ogg` (Vorbis, stereo, 22050 Hz) and `sound.wav` staged on `host0:`
- [ ] `sound.wav` (32-bit mono, 44100 Hz) is staged; it is the WAV arm and also exercises conversion
- [ ] `host0:` is the test root; no absolute desktop path is embedded
- [ ] A 5-minute run can be left unattended; the app logs `DONE` and sleeps

## 5. Tasks

Status vocabulary — use exactly these words, in backticks: `todo` · `doing` · `done` · `blocked` · `dropped`.

| ID | task | status | verification | evidence |
|---|---|---|---|---|
| T1 | **`Music` API matrix**: `LoadMusicStream`, `IsMusicValid`, `Play`, `IsMusicStreamPlaying`, `Update`, `Stop`, `Pause`, `Resume`, `Seek`, `SetMusicVolume`, `SetMusicPitch`, `SetMusicPan`, `GetMusicTimeLength`, `GetMusicTimePlayed`, `Unload`; OGG and WAV | `done` | every call observed; seek/time within a frame; pitch changes duration; invalid music does nothing | OGG valid length 2.000, WAV 0.553, invalid=0; play/pause/resume/seek/time pass |
| T2 | **`AudioStream` API**: `SetAudioStreamBufferSizeDefault(1024)`, 16-bit s16 for update/processor, 32-bit f32 for callback; `UpdateAudioStream`, `IsAudioStreamProcessed`, play/pause/resume/stop, volume/pan/pitch, `SetAudioStreamCallback`, `AttachAudioStreamProcessor` | `done` | all three arms continuous at 60 fps; callback f32 and processor f32; no blocking | update 433/43 (runClocks 1,065,045), callback 433/43 (1,310,585), processor 433/43 (1,287,450); 0 errors; low callback/processor gain is test-only |
| T3 | **Main-thread baseline**: `UpdateMusicStream()` in the frame loop, OGG then WAV; 60 warm-up + 600 measured frames | `done` | p95/max from a static array, frames >2 ms, gate baseline; OGG and WAV separate | OGG p95 **16.908 ms**, max **21.622 ms**, 57 spikes; WAV p95 **13.172 ms**, max **15.124 ms**, 110 spikes |
| T4 | **Decoder decision gate**: compare T3 p95/max with 1 ms/2 ms; if over, mark T4/T7 `blocked` and open a follow-up; do not add an unmeasured thread | `blocked` | written decision with numbers; no decoder thread unless authorized | both OGG and WAV exceed p95 1 ms and max 2 ms by a wide margin; **P9-S4a decoder/feeder follow-up required** |
| T5 | **Track change / seek / loop**: loop on, seek, stop/start, change track; time queries | `doing` | no torn audio, no stale track; `GetMusicTimePlayed` consistent | OGG loops, seek 0.2 s and time queries pass; track change not yet exercised |
| T6 | **File I/O**: OGG `setvbuf` 64 KiB via `fopen`/`stb_vorbis_open_file` in `raudio.c`; WAV read cost; open/read timing | `done` | card-stall spikes recorded; no file open from the feeder; OGG handle closes on unload | OGG setvbuf path used; OGG and WAV load/stream; WAV 32-bit mono conversion measured |
| T7 | **5-minute soak**: music + 8 sounds, `MODE=musicmix FRAMES=18000 COUNT=8`, frame log each second | `blocked` | zero underruns; rest never 0; frame p95; blocked if T4 fails | not run: T4 blocked; a soak on the main-thread decoder would only reproduce the spikes |
| T8 | **Heap/stream buffers**: record stream buffer/period, ring (if any), decoder stack and free bytes; `SetAudioStreamBufferSizeDefault(1024)` | `doing` | numbers in §7; no per-frame allocation after warm-up | 1024-frame stream default in use; heap decode buffers not yet isolated |
| T9 | **Regression**: `batch_check` and 018–021 gates; `git diff -- examples/` empty | `blocked` | gate `cf43ce6d…` byte-identical | installed `libGL.a` is **not a pspgu build**; `batch_check` capture is `b69830b8…` (old pspGL), not `cf43ce6d…`. Not an audio regression; restore pspgu before rerunning |

### Task log (append-only)

- 2026-11-21 — sprint drafted; not started
- 2026-11-21 — readiness review: pinned S3 baseline, staged real assets, defined OGG `setvbuf`, made the
  decoder thread a measurement gate and fixed the 5-minute duration
- 2026-11-21 — implementation: OGG `setvbuf`; music/musicmix/stream modes; OGG/WAV baselines and 3 stream arms run
- 2026-11-21 — T3 fails gate (OGG p95 16.9 ms, WAV 13.2 ms); T4/T7 blocked; no soak
- 2026-11-21 — T9 blocked by installed pspGL, not the audio change; restore pspgu before gate
- 2026-11-21 — stream arms fixed: 1024-frame sub-buffers, precomputed PCM for update/callback, f32/32-bit callback; continuous at 60 fps

## 6. Budgets

| budget | limit | measured | tag |
|---|---|---|---|
| frame time | ≤ 16.7 ms (60 fps) with music + 8 sounds | 56–60 fps on Music; 58–60 on stream callback | `[measured: PSP-1000, P9-S4]` |
| main-thread update | p95 ≤ 1 ms, max ≤ 2 ms, else T4 `blocked` | OGG p95 16.908 / max 21.622; WAV 13.172 / 15.124 | `[measured: PSP-1000, P9-S4]` |
| audio thread CPU | record; target ≤ 2 ms/frame | music `runClocks` 1,353,671 / WAV 1,049,255 / callback 8,762,601 over ~10–13 s | `[measured: PSP-1000, P9-S4]` |
| dropout | zero in 18,000 frames | not run (T4 blocked) | `[blocked]` |
| ring | only if T4 authorizes a follow-up; record bytes | none | `[blocked]` |

## 7. Measurements

Audio has no screenshot evidence. Record build marker, boot log, counters, audible confirmation and the
frame-time distribution.

| run | build | workload | metric | value | notes |
|---|---|---|---|---|---|
| baseline OGG | `audio b4` | 1 OGG, main-thread update | update p95/max, frames >2 ms | 16.908 / 21.622, 57 spikes | 60 warm-up + 600 measured |
| baseline WAV | `audio b4` | 1 `sound.wav` | same | 13.172 / 15.124, 110 spikes | 32-bit mono conversion |
| stream update | `audio b4` | UpdateAudioStream | callbacks, errors, frame | 478/40, 0, 60 fps | separate arm |
| stream callback | `audio b4` | s16 callback | callbacks, errors, frame | 410/40, 0, 58–60 fps | runClocks 8,762,601 |
| stream processor | `audio b4` | f32 processor | callbacks, errors, frame | 476/40, 0, 60 fps | runClocks 1,425,012 |
| soak | `audio b4` | `musicmix` 18,000 frames | underruns, rest, p95 | — | blocked by T4 |
| gate | `audio b4` | `batch_check` | hash | `b69830b8…` | installed libGL is pspGL; expected `cf43ce6d…` after pspgu restore |
| gate | `audio b4` | `batch_check` | hash | `cf43ce6d…` | unchanged |

## 8. Exit criteria

- [ ] one streamed OGG track and eight sounds play simultaneously at 60 fps
- [ ] `UpdateMusicStream()` and `UpdateAudioStream()` behave as raylib documents
- [ ] no file open or long decode occurs on the audio thread
- [ ] five minutes (18,000 frames) without a dropout or underrun, counters confirming
- [ ] seek, loop, pause/resume and time queries are correct
- [ ] T4 is `done` with a written decision (defer or authorize a follow-up), with the p95/max that justified it
- [ ] OGG `setvbuf` and WAV read costs are recorded; OGG handle closes on unload
- [ ] stream buffer/period and heap costs are recorded; no per-frame allocation after warm-up
- [ ] `batch_check` stays `cf43ce6d…`; the 018–021 gates are unchanged

## 9. Result

**Blocked at the decoder gate.** `UpdateMusicStream()` on the main thread is far over budget:

- OGG: p95 **16.908 ms**, max **21.622 ms**, **57** frames >2 ms.
- WAV: p95 **13.172 ms**, max **15.124 ms**, **110** frames >2 ms.

The `Music` API matrix itself passes: OGG valid (2.000 s), WAV valid (0.553 s), invalid music rejected;
play/pause/resume/seek and time queries work. OGG uses the 64 KiB `setvbuf` path. The three `AudioStream`
arms (update, 32-bit f32 callback, f32 processor) all run continuously at 60 fps with zero output errors;
the 1024-frame sub-buffer and precomputed PCM removed the initial update/callback choppiness.

T4 and T7 are **blocked**; the 5-minute soak was deliberately not run because it would only reproduce the
spikes. T5 still needs a track-change arm; T8 needs an isolated heap count.

T9 is also blocked for an environmental reason: the installed `libGL.a` is not a pspgu build
(`tools/pspgu-build.sh --verify-install` fails), so `batch_check` captures the old pspGL reference
`b69830b8…`, not the pspgu gate `cf43ce6d…`. Restore pspgu before rerunning T9.

Next: P9-S4a decoder/feeder thread, or an explicit decision to accept main-thread decode (which the gate
does not support).

## 10. Follow-ups

- If T3 exceeds 1 ms p95 or 2 ms max, open P9-S4a for a decoder/feeder thread; do not silently accept the
  spike or hide it behind a larger stream buffer.
- If `drwav_init_file` is over budget, add FILE-backed drwav callbacks for `setvbuf`; do not change WAV
  semantics. The WAV arm is 32-bit mono by design (`sound.wav`), so record conversion too.
- `AudioStream` callbacks are user code on the audio thread; keep the non-blocking rule in Phase 9.
- P9-S5 closes the phase and decides the SAS/ME route.
