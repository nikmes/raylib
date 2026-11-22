# Sprint 064 (P13-S13) — Audio: AudioStream batch

| | |
|---|---|
| Roadmap phase | [Phase 13 — Samples porting](../phases/13-samples-porting/index.md) |
| Sprint | P13-S13 |
| Branch / base commit | `master` @ the P13-S12 close |
| Started / closed | — / — |
| Build number(s) | — |
| Tasks | 0/5 done |

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
| T1 | Port `audio_raw_stream`: 480×272, D-pad volume/pitch, `UpdateAudioStream` when processed | `todo` | tone changes with the controls; `IsAudioStreamProcessed` paces updates; `callbacks>0`, `errors=0`; two captures | — |
| T2 | Port `audio_stream_callback`: D-pad rate, TRIANGLE waveform cycle, `SetAudioStreamCallback` | `todo` | each waveform is audible; switching does not click or deadlock; callback count advances | — |
| T3 | Port `audio_amp_envelope`: CROSS attack/release, `IsAudioStreamProcessed` + `UpdateAudioStream` | `todo` | envelope attack and release are audible; no buffer underrun in the log | — |
| T4 | Port `audio_stream_effects`: CROSS play, CIRCLE stop, TRIANGLE LPF, SQUARE delay; detach both on exit | `todo` | both effects audible and removable; detach-before-unload leaves no processor; `errors=0` | — |
| T5 | Write back the audio results: Phase 13 audio table/state, Samples page, memory-map; record 9 ported / 2 deferred | `todo` | the audio rows match the device runs; `mkdocs build --strict` passes; the all-corpus audit stays open for P13-S18 | — |

### Task log (append-only)

- 2026-11-22 — sprint written; all tasks `todo`

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
| control | | libraylib sha256 | — | — | — | — | archive before first run |
| T1 | | audio_raw_stream | 600 | | | | |
| T2 | | audio_stream_callback | 600 | | | | |
| T3 | | audio_amp_envelope | 600 | | | | |
| T4 | | audio_stream_effects | 600 | | | | |

## 8. Exit criteria

- [ ] four samples build and run at 480×272 for 600 frames; each is audible
- [ ] `callbacks>0`, `errors=0`, and no attached processor after close
- [ ] two identical captures per sample under `/home/psp/evidence/sprint064/`
- [ ] Phase 13 records 9 audio samples ported / 2 deferred, each deferral with a reason and a trigger
- [ ] the audio rows account for all 11 samples (9 ported, 2 deferred); the all-corpus audit is P13-S18
- [ ] `G5` is recorded closed; `G1`, `G3`, `G6`, `G7`, `G8`, `G9` have an explicit status
- [ ] the top-level Samples page has an Audio section with the nine screenshots
- [ ] `examples/` untouched; no `src/` change in the diff; `mkdocs build --strict` passes
- [ ] Phase 13 records 9 audio ported / 2 deferred and stays `in progress` for the remaining corpus

## 9. Result

Filled at close. Record the audio results. The `spectrum_visualizer` decision belongs to P13-S16 (shader policy),
so do not pre-empt it here.

## 10. Follow-ups

- Phase 10: `audio_sound_positioning`.
- Phase 8 `G1`: optionally restore keyboard triggers behind a compile-time switch.
- Phase 13: `textures_sprite_button` once `G1` lands; `textures_sprite_explosion` after `G3`.
- Phase 13 future waves: S14 text (17), S15 models (32, `G8`), S16 shaders (36, `G9`), S17 others (4), S18
  all-corpus close (227).
