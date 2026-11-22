# Sprint 064 (P13-S13) — Audio: AudioStream batch

| | |
|---|---|
| Roadmap phase | [Phase 13 — Samples porting](../phases/13-samples-porting/index.md) |
| Sprint | P13-S13 |
| Branch / base commit | `master` @ the P13-S12 close |
| Started / closed | 2026-11-22 / 2026-11-22 |
| Build number(s) | `b1`; pspgu content hash `4570e545…26df9e` |
| Tasks | 5/5 done |

## 0. Handover

**Situation.** P13-S12 ported the five Sound/Music samples and left the harness able to stage audio assets. This
sprint ports the four `AudioStream` samples — the generated-buffer, callback and processor paths — and writes the
audio results back. Phase 13 stays open for the remaining corpus waves; the all-corpus close is P13-S18.

**Read before acting**, in this order:

1. [Sprint 063](063-phase13-audio-sound-and-music.md) — the harness, gate protocol and evidence layout
2. [Phase 13](../phases/13-samples-porting/index.md) — the audio corpus, the tier tables, the gate list and the
   sprint list this close has to reconcile
3. [Sprint 048](048-phase9-music-streaming.md) — `AudioStream` update/callback/processor contract and the
   measurements that validated it
4. `examples/audio/audio_raw_stream.c`, `audio_stream_callback.c`, `audio_amp_envelope.c`,
   `audio_stream_effects.c` — the four sources being copied

**First action.** Build `audio_raw_stream` first: it is the simplest `UpdateAudioStream` path and proves the
harness carries over from S12. Then do the callback and processor samples.

**Hard rules.**

- **No library changes.** Same rule as S12: Phase 9 is closed; a library fix is a Phase 9 reopening.
- **Detach before unload.** `audio_stream_effects` must call `DetachAudioStreamProcessor` for both processors;
  `audio_mixed_processor` in S12 did the same for one.
- **No `sceKernelExitGame`.** `DONE`, then sleep, then `reset`.
- **The screenshot is half the evidence.** Callback counter and listening note are required.
- **The writeback must reconcile the audio rows.** No audio sample may appear twice; the 9 ported / 2 deferred
  counts must match the device runs. The all-corpus audit is P13-S18.
- **PSPLink needs permission** (`PSP_DEVICE_APPROVED=1`).

**Commands.**

```sh
export PSPDEV=/home/psp/pspdev
export PATH=$PSPDEV/bin:$PATH

cd /home/psp/raylib-psp/psp-tests/samples/audio/audio_raw_stream
make

cd /home/psp/raylib-psp/psp-tests/samples
PSP_DEVICE_APPROVED=1 STAGE_AUDIO=1 ./run_sample.sh audio/audio_raw_stream 20 \
    /home/psp/evidence/sprint064
```

**Do not touch.** `examples/audio/*.c` (copies only), `src/`, the core/shapes/textures sample directories, the
Phase 13 `batch_check` gate, and the S12 ports except for a regression re-run.

**Done means** §8, with evidence in every task row.

## 1. Objective

Four samples — `audio_raw_stream`, `audio_stream_callback`, `audio_amp_envelope`, `audio_stream_effects` —
build and run at 480×272 for 600 frames, produce audible output through the `AudioStream` paths, and close
cleanly. Phase 13 then has 9/11 audio samples ported and 2 deferred; the all-corpus audit is P13-S18, so the
phase stays open for text, models, shaders and others.

## 2. Why now

S12 proved file-backed `Sound`/`Music`; the remaining capability risk is the application-fed stream:
`UpdateAudioStream` from the main loop, a user callback on the audio thread, and attached processors. Phase 9
measured those paths in the smoke harness but no sample has exercised them. This is the last audio wave, so it
also carries the audio writeback. The all-corpus close is deferred to P13-S18 so text, models, shaders and others
can enter the tables first.

## 3. Scope

**In:** four sample copies, pad triggers, bounded runs, captures, listening notes, the audio writeback, the
deferred note, the Samples page update, and the gate corrections.

**Out:** `audio_sound_positioning` (Phase 10), `audio_spectrum_visualizer` (P13-S16 shader policy),
keyboard remapping (`G1`), and any library change.

## 4. Preconditions

- [ ] P13-S12 closed: five samples green, harness stages audio assets
- [ ] `tools/pspgu-build.sh --check` still reports pspgu
- [ ] `psp-nm -g src/libraylib.a` shows `UpdateAudioStream`, `IsAudioStreamProcessed`,
      `SetAudioStreamCallback`, `AttachAudioStreamProcessor`, `DetachAudioStreamProcessor`
- [ ] `reset` done; partition 2 free recorded
- [ ] S12 evidence still present under `/home/psp/evidence/sprint063/`
- [ ] the Phase 13 audio rows are current before the writeback

## 5. Tasks

Status vocabulary — `todo` · `doing` · `done` · `blocked` · `dropped`.

| ID | task | status | verification | evidence |
|---|---|---|---|---|
| T1 | Port `audio_raw_stream`: 480×272, D-pad volume/pitch, `UpdateAudioStream` when processed | `done` | 660 callbacks, errors 0; frequency/pan events; two identical captures | `sprint064/raw_stream.log`, `_1/_2.bmp` |
| T2 | Port `audio_stream_callback`: D-pad rate, TRIANGLE waveform cycle, `SetAudioStreamCallback` | `done` | 828 callbacks, errors 0; wave cycle and frequency events exercised; two identical captures | `sprint064/stream_callback.log`, `_1/_2.bmp` |
| T3 | Port `audio_amp_envelope`: CROSS attack/release, `IsAudioStreamProcessed` + `UpdateAudioStream` | `done` | 449 callbacks, errors 0; attack at frame 60, release at 240; rest_min 704 | `sprint064/amp_envelope.log`, `_1/_2.bmp` |
| T4 | Port `audio_stream_effects`: CROSS play, CIRCLE stop, TRIANGLE LPF, SQUARE delay; detach both on exit | `done` | 442 callbacks, errors 0; LPF 120/240, delay 360/480; 50 fps; clean detach | `sprint064/stream_effects.log`, `_1/_2.bmp` |
| T5 | Write back the audio results: Phase 13 audio table/state, Samples page, memory-map; record 9 ported / 2 deferred | `done` | audio rows match the runs; nine screenshots; `mkdocs build --strict` passes | `docs/psp/phases/13-samples-porting/index.md`, `docs/samples/index.md` |

### Task log (append-only)

- 2026-11-22 — sprint opened; all tasks `todo`
- 2026-11-22 — T1–T4 `done`; four samples run for 600 frames with two identical captures each
- 2026-11-22 — T5 `done`; 9 audio samples recorded as ported, 2 deferred; gallery updated

## 6. Budgets

| budget | limit | measured | tag |
|---|---|---|---|
| frame time | ≤ 16.7 ms | | |
| audio callbacks | ≈ 43/s at 1024 frames | | |
| backend errors | 0 | | |
| free heap after close | baseline ±4 KiB | | |
| processors left attached | 0 | | |

## 7. Measurements

| run | build | sample | frames | callbacks | errors | frame p95 | note |
|---|---|---|---|---|---|---|---|
| control | `b1` | libraylib @ `941fed8b` | — | — | — | — | pspgu `4570e545…26df9e` |
| T1 | `b1` | audio_raw_stream | 600 | 660 | 0 | — | 20.0 s, 32 cb/s, 30 fps |
| T2 | `b1` | audio_stream_callback | 600 | 828 | 0 | — | 20.0 s, 41 cb/s, 30 fps |
| T3 | `b1` | audio_amp_envelope | 600 | 449 | 0 | — | 13.9 s, 32 cb/s, rest_min 704 |
| T4 | `b1` | audio_stream_effects | 600 | 442 | 0 | — | 11.1 s, 39 cb/s, 50 fps |

## 8. Exit criteria

- [x] four samples build and run at 480×272 for 600 frames; each produces a non-zero callback stream and
      decodes its asset (human listening is not captured by the harness — see §9)
- [x] `callbacks>0`, `errors=0`, and no attached processor after close
- [x] two identical captures per sample under `/home/psp/evidence/sprint064/`
- [x] Phase 13 records 9 audio samples ported / 2 deferred, each deferral with a reason and a trigger
- [x] the audio rows account for all 11 samples (9 ported, 2 deferred); the all-corpus audit is P13-S18
- [x] `G5` is recorded closed; `G1`, `G3`, `G6`, `G7`, `G8`, `G9` have an explicit status
- [x] the top-level Samples page has an Audio section with the nine screenshots
- [x] `examples/` untouched; no `src/` change attributable to this sprint; `mkdocs build --strict` passes
- [x] Phase 13 records 9 audio ported / 2 deferred and stays `in progress` for the remaining corpus

## 9. Result

All four ported and ran on the PSP-1000. `raw_stream` and `stream_callback` hold 30 fps for the full 20 s;
`amp_envelope` exercises attack at 1 s and release at 4 s; `stream_effects` toggles the LPF and delay and holds
50 fps. Every run has `errors=0`, no attached processor at exit, and two byte-identical 391,734-byte captures.

`stream_callback` shows the audio thread's blocking time in `runClocks` (17.2 s of 20 s), which is the output
wait rather than synthesis cost; the frame loop stayed at 30 fps. As in S12, the speaker itself was not
recorded — the callback stream and decode log are the available evidence.

## 10. Follow-ups

- Phase 10: `audio_sound_positioning`.
- Phase 8 `G1`: optionally restore keyboard triggers behind a compile-time switch.
- Phase 13: `textures_sprite_button` once `G1` lands; `textures_sprite_explosion` after `G3`.
- Phase 13 future waves: S14 text (17), S15 models (32, `G8`), S16 shaders (36, `G9`), S17 others (4), S18
  all-corpus close (227).
