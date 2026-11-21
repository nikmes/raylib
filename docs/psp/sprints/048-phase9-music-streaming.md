# Sprint 048 (P9-S4) — `Music` and `AudioStream` streaming

| | |
|---|---|
| Roadmap phase | [Phase 9 — Audio](../phases/09-audio/index.md) |
| Sprint | P9-S4 |
| Branch / base commit | P9-S3 close sha (pin at start) |
| Started / closed | — / — |
| Build number(s) | `audio b4`; pspgu `b24` unchanged |
| Tasks | 0/9 done |

## 0. Handover

**Situation.** P9-S3 closed the `Sound` surface. This sprint adds the streaming half of raylib: `Music`
(decoded file playback) and raw `AudioStream`. The hard part is not the API; it is that
`UpdateMusicStream()` is a **main-thread raylib API** that decodes and refills under
`AUDIO.System.lock` (`src/raudio.c:1882`). A PSP Memory Stick open costs ~17 ms and a read can spike; the
sprint must measure the main-thread cost first and only then decide whether a decoder thread is justified.

**Read before acting**, in this order:

1. `../phases/09-audio/index.md` — P9-S4 and the "decode ahead, never on the audio thread" rule.
2. `../sprints/046-phase9-custom-backend.md` §9 and `047-phase9-sound-api.md` §9 — the working sink and the
   `Sound` counters; `Music` shares the same mixer and channel.
3. `src/raylib.h:1720-1755` — the `Music` and `AudioStream` API this sprint must cover.
4. `src/raudio.c:1882-2050` — `UpdateMusicStream()`: the lock, the two sub-buffers, the decode path and
   `StopMusicStream()`; `:2157-2165` — `UpdateAudioStream()`; `:2701-2760` — the locked refill.
5. `../memory-map.md` §6, §9 — device protocol; "blocking audio on the main thread", "thread stack comes
   from the user partition", "Memory Stick opens cost ~17 ms".
6. The psp-developer skill `references/audio.md` §2 and §5 — decoder/feeder split, SPSC ring, Vorbis cost,
   `setvbuf`; treat the numbers as another project's until re-measured here.

**First action.** Add `MODE=music` to `audio_smoke`: load one short OGG and one WAV from `host0:` (or
generate a WAV), `PlayMusicStream()`, call `UpdateMusicStream()` in the frame loop, run 600 frames, log
`DONE`. Record the main-thread decode time and every frame spike **before** adding any thread. That baseline
is the control for the whole sprint.

**Hard rules.**

- **Keep `UpdateMusicStream()` as the public API.** The game calls it while music plays; that is raylib's
  contract. If an internal decoder thread is added, the public call must remain valid and cheap; do not make
  the game call a PSP-only function.
- **Never open a file from the audio thread.** Decode/read on the caller or a low-priority decoder thread;
  the feeder only copies from the ring and blocks on output.
- **The audio thread must not block on a Memory Stick read or a mutex held by a long decode.** Measure lock
  hold time; if it exceeds a block, redesign.
- **No `sceAudio` in the game loop.** The backend owns the sink; the game only calls raylib.
- **`AudioStream` callbacks run on the audio thread.** They must be non-blocking; document that in the test.
- **One variable per run.** Main-thread baseline, decoder-thread arm and ring-size arm are separate.
- Commit messages carry the task ID: `sprint 048 T3: move music decode to a reader thread`.

**Commands.**

```sh
export PSPDEV=/home/psp/pspdev
export PATH=$PATH:$PSPDEV/bin

# host0: must be the directory containing the test assets
cd /home/psp/raylib-psp/psp-tests/audio_smoke
usbhostfs_pc &
make clean && make MODE=music
( printf './audio_smoke.prx\n'; sleep 30 ) | pspsh
# after DONE: pspsh -e "reset"
```

**Do not touch.** `src/rlgl.h`, pspgu, renderer gates, the `Sound` API, SAS/ME, and the backend's
`onDeviceWrite()` unless a streaming failure is proven to be in the sink.

**Done means** §8, with a 5-minute run, decode/read costs, underrun counts and a recorded decision on the
decoder thread.

## 1. Objective

One streamed track and several `Sound`s play simultaneously at 60 fps for five minutes on the PSP-1000,
with `UpdateMusicStream()`/`UpdateAudioStream()` behaving as raylib documents, no file I/O on the audio
thread, and no dropout or frame spike attributable to decoding or Memory Stick reads.

## 2. Why now

`Sound` is fully in RAM and already measured. `Music` introduces the two costs that can ruin a frame:
decode and file I/O. The phase's exit criterion explicitly requires a streamed track, five minutes without
a dropout, and no `sceAudio` call on the main thread. This is the last functional sprint before close.

## 3. Scope

**In:** `Music` and `AudioStream` API coverage; `audio_smoke MODE=music`/`MODE=stream`; main-thread baseline;
optional decoder/reader thread; SPSC ring; `setvbuf`/read sizing; seek/loop/time; 5-minute soak; counters.

**Out:** compressed-format additions, SAS/ME, `Sound` changes, renderer changes, and any public raylib API
addition.

## 4. Preconditions

- [ ] P9-S3 closed: 16 sounds at 60 fps; backend counters and thread ID in the log
- [ ] `reset` was run; partition 2 free reads 25,149,440 bytes
- [ ] A short OGG (one Vorbis stream) and a WAV are staged; verify the OGG has no cover-art video stream
- [ ] `host0:` is rooted at the asset directory; no absolute desktop path is embedded
- [ ] The 018–021 gate hashes and a `batch_check` capture are archived
- [ ] A 5-minute run can be left unattended; the app logs `DONE` and sleeps, never calls `sceKernelExitGame`

## 5. Tasks

Status vocabulary — use exactly these words, in backticks: `todo` · `doing` · `done` · `blocked` · `dropped`.

| ID | task | status | verification | evidence |
|---|---|---|---|---|
| T1 | **`Music` API matrix**: `LoadMusicStream`, `IsMusicValid`, `Play`, `IsMusicStreamPlaying`, `Update`, `Stop`, `Pause`, `Resume`, `Seek`, `SetMusicVolume`, `SetMusicPitch`, `SetMusicPan`, `GetMusicTimeLength`, `GetMusicTimePlayed`, `Unload` | `todo` | every call observed; seek/time within a frame; pitch changes duration; invalid music does nothing | — |
| T2 | **`AudioStream` API**: `LoadAudioStream(44100,16,2)`, `UpdateAudioStream`, `IsAudioStreamProcessed`, play/pause/resume/stop, volume/pan/pitch, `SetAudioStreamCallback`, `AttachAudioStreamProcessor` | `todo` | a generated PCM stream plays; callback runs on the audio thread and is non-blocking; processor output is audible | — |
| T3 | **Main-thread baseline**: `UpdateMusicStream()` in the frame loop with one OGG and one WAV | `todo` | decode/read time per call and frame-spike histogram in §7 | — |
| T4 | **Decoder/reader decision**: if the baseline exceeds the frame budget, add a low-priority decoder/reader thread and an SPSC PCM ring; keep `UpdateMusicStream()` as the public call | `todo` | a written decision with the baseline number; no file open on the audio thread | — |
| T5 | **Track change / seek / loop**: loop on, seek, stop/start, change track; bump a generation counter and snap the ring if a track change is implemented | `todo` | no torn audio, no stale track; `GetMusicTimePlayed` consistent | — |
| T6 | **File I/O**: `setvbuf` 64 KB, large reads, few long-lived handles; measure open/read cost | `todo` | card-stall spikes recorded; no open from the feeder | — |
| T7 | **5-minute soak**: music + 8 sounds, 300 frames or 5 minutes, counters every second | `todo` | zero underruns; rest length never 0 at the output call; frame percentiles | — |
| T8 | **Heap/stream buffers**: record stream buffer, ring, decoder stack and free bytes | `todo` | numbers in §7; no per-frame allocation after warm-up | — |
| T9 | **Regression**: `batch_check` and 018–021 gates; `git diff -- examples/` empty | `todo` | gates byte-identical | — |

### Task log (append-only)

- 2026-11-21 — sprint drafted; not started

## 6. Budgets

| budget | limit | measured | tag |
|---|---|---|---|
| frame time | ≤ 16.7 ms (60 fps) with music + 8 sounds | | |
| main-thread update | ≤ 1 ms/frame if kept on the main thread | | |
| audio thread CPU | record; target ≤ 2 ms/frame | | |
| dropout | zero in 5 minutes | | |
| ring | ~1 s PCM; record bytes | | |

## 7. Measurements

Audio has no screenshot evidence. Record build marker, boot log, counters, audible confirmation and the
frame-time distribution.

| run | build | workload | metric | value | notes |
|---|---|---|---|---|---|
| baseline | `audio b4` | 1 OGG, main-thread update | decode ms, frame max, spikes | | control |
| WAV | `audio b4` | 1 WAV | same | | decoder vs PCM read |
| thread arm | `audio b4` | 1 OGG + ring | ring low-water, frame max | | only if T4 |
| soak | `audio b4` | music + 8 sounds, 5 min | underruns, rest min, frame p95 | | exit |

## 8. Exit criteria

- [ ] one streamed track and several sounds play simultaneously at 60 fps
- [ ] `UpdateMusicStream()` and `UpdateAudioStream()` behave as raylib documents
- [ ] no Memory Stick file open or long decode occurs on the audio thread
- [ ] five minutes without a dropout or underrun, with counters confirming
- [ ] seek, loop, pause/resume and time queries are correct
- [ ] the decoder-thread decision is recorded with the baseline that justified it (or the reason it was deferred)
- [ ] stream/ring/heap costs are recorded in §7
- [ ] `batch_check` and 018–021 gates are unchanged

## 9. Result

Not started.

## 10. Follow-ups

- P9-S5: close the phase, set budgets, and decide the SAS/ME route.
- If the OGG decoder is too slow, consider decoding at load time when the track fits, or a lower-complexity
  format; do not change `Music` semantics.
- If `UpdateMusicStream()` must stay on the main thread, record the worst-case decode time as the phase's
  accepted cost rather than hiding it.
- `AudioStream` callbacks are user code on the audio thread; document the real-time rule in Phase 9.
