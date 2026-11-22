# Sprint 063 (P13-S12) — Audio: Sound and Music batch

| | |
|---|---|
| Roadmap phase | [Phase 13 — Samples porting](../phases/13-samples-porting/index.md) |
| Sprint | P13-S12 |
| Branch / base commit | `master` @ `0f79bfd1` (2026-11-22) |
| Started / closed | — / — |
| Build number(s) | — (pspgu content hash `4570e545…26df9e` at open) |
| Tasks | 0/8 done |

## 0. Handover

**Situation.** Phase 9 is closed on route A: playback, `Sound`, `Music`, `AudioStream`, callbacks and stream
processors work on the device, and gate `G5` is closed. Phase 13 now includes the `examples/audio` corpus. This
sprint adds the audio harness and ports the five Sound/Music samples; it changes no library code.

**Read before acting**, in this order:

1. [Phase 13](../phases/13-samples-porting/index.md) — the audio corpus table, trigger mapping, `G5` and the
   porting recipe
2. [Phase 9](../phases/09-audio/index.md) — the route-A contract and the clean-close evidence this sprint relies on
3. [Sprint 025](025-phase13-texture-paths.md) — the harness, capture wait and two-identical-copies rule
4. `psp-tests/samples/run_sample.sh` and `psp-tests/samples/Makefile.inc` — the code this sprint extends

**First action.**

```sh
cd /home/psp/raylib-psp
tools/pspgu-build.sh --check            # must say "installed libGL.a is a pspgu build"
psp-nm -g src/libraylib.a | grep -E ' T (PlaySound|LoadMusicStream|UpdateMusicStream)$'
```

Then extend `run_sample.sh` with audio asset staging (`STAGE_AUDIO=1`) before touching any sample.

**Hard rules.**

- **No library changes.** Do not edit `src/raudio.c`, `src/platforms/raudio_psp.c`, `src/Makefile` or anything
  that changes `libraylib.a`. Phase 9 is closed; a sample that needs a library change is a Phase 9 reopening,
  not a sample fix.
- **No upstream edits.** `examples/` stays untouched. Each port is a copy under
  `psp-tests/samples/audio/<name>/main.c`, following Phase 13's porting recipe.
- **The screenshot is half the evidence.** Every `done` row also needs the backend's
  `callbacks=… errors=0` line and a listening note.
- **Never call `sceKernelExitGame`.** Log `DONE`, then `sceKernelSleepThread()`; `reset` from the host.
- **`reset` between runs.** `kill` leaks the module heap and shrinks the next run's partition.
- **PSPLink needs permission.** Only run with `PSP_DEVICE_APPROVED=1` after the user approves the session.

**Commands.**

```sh
export PSPDEV=/home/psp/pspdev
export PATH=$PSPDEV/bin:$PATH

# build one port
cd /home/psp/raylib-psp/psp-tests/samples/audio/audio_sound_loading
make

# asset staging + two captures + DONE log (after approval)
cd /home/psp/raylib-psp/psp-tests/samples
PSP_DEVICE_APPROVED=1 STAGE_AUDIO=1 ./run_sample.sh audio/audio_sound_loading 20 \
    /home/psp/evidence/sprint063
```

**Do not touch.** `examples/audio/*.c` (copies only), `src/` (library and backend), the core/shapes/textures
sample directories, and the Phase 13 `batch_check` gate.

**Done means** §8, with evidence in every task row.

## 1. Objective

Five samples — `audio_sound_loading`, `audio_sound_multi`, `audio_music_stream`, `audio_mixed_processor`,
`audio_module_playing` — build against the current `libraylib.a`, run for 600 frames at 480×272, produce
audible output, and close cleanly. Each has a build marker, a `DONE` line, two identical 391,734-byte captures,
a non-zero audio callback count and zero backend errors.

## 2. Why now

`G5` is closed and the audio corpus is in Phase 13's scope, so this is the next wave that needs no new renderer
capability. It proves the `Sound`/`Music` half of the corpus and the asset path; P13-S13 proves the `AudioStream`
half; the all-corpus close is P13-S18.

## 3. Scope

**In:** the audio harness extension, five sample copies, their 480×272 layouts, pad triggers, bounded runs,
captures, listening notes and doc writeback.

**Out:** the four `AudioStream` samples (P13-S13), `audio_sound_positioning` (Phase 10),
`audio_spectrum_visualizer` (P13-S16 shader policy), keyboard remapping (`G1`, Phase 8), and any library or
backend change.

## 4. Preconditions

- [ ] `pspsh` answers; `usbhostfs_pc` is running; the USB device enumerates
- [ ] `reset` done; partition 2 free recorded at open
- [ ] `tools/pspgu-build.sh --check` reports a pspgu build
- [ ] `psp-nm -g src/libraylib.a` shows `PlaySound`, `LoadMusicStream`, `UpdateMusicStream`,
      `SetSoundPan`, `LoadSoundAlias`, `AttachAudioStreamProcessor`
- [ ] Audio assets staged: `coin.wav`, `country.mp3`, `mini1111.xm`, `sound.wav`, `target.ogg` under
      `/tmp/s3/resources` (`spring.wav` and `weird.wav` are not used by these five samples)
- [ ] `examples/audio/resources/target.flac` is **not** used; FLAC is compiled out
- [ ] Reference build archived: `libraylib.a` sha256 recorded in §7 before the first run

## 5. Tasks

Status vocabulary — use exactly these words, in backticks: `todo` · `doing` · `done` · `blocked` · `dropped`.

| ID | task | status | verification | evidence |
|---|---|---|---|---|
| T1 | Extend `psp-tests/samples/run_sample.sh` with audio staging (`STAGE_AUDIO=1`) and create `psp-tests/samples/audio/`; keep the texture path unchanged | `todo` | a dry run stages the five used assets and prints their names; `audio_sound_loading` builds with the shared `Makefile.inc` | — |
| T2 | Port `audio_sound_loading`: 480×272, CROSS → WAV, SQUARE → OGG, 600 frames, `DONE` + sleep | `todo` | both sounds audible in separate runs; `callbacks>0`, `errors=0`; two identical captures | — |
| T3 | Port `audio_sound_multi`: CROSS fires `MAX_SOUNDS` aliases; alias count and unload checked | `todo` | 8 aliases start together, no truncation or click; heap returns to baseline after unload | — |
| T4 | Port `audio_music_stream`: MP3, CROSS pause, CIRCLE stop, D-pad seek/volume | `todo` | `GetMusicTimePlayed` advances, pauses and resets on stop; decoder thread closes; two captures | — |
| T5 | Port `audio_mixed_processor`: music + `coin.wav`, D-pad exponent, CROSS coin, processor detached on exit | `todo` | music and coin mix; LPF changes audibly with the exponent; no processor survives close | — |
| T6 | Port `audio_module_playing`: XM, CROSS play/pause, CIRCLE stop, D-pad pitch | `todo` | XM length reads 61.95 s; 5 s of playback advances time; stop leaves no stale audio | — |
| T7 | Run the `G5` gate over all five: two runs each, captures compared byte-for-byte, counters and heap recorded | `todo` | 5/5 produce audible output; 10/10 captures identical pairwise; free-heap delta ≤4 KiB; `errors=0` | — |
| T8 | Write back: Phase 13 audio section/state, top-level Samples page (new Audio section), memory-map §5, evidence paths | `todo` | `mkdocs build --strict` passes; every `done` sample has a screenshot and a listening note | — |

### Task log (append-only)

- 2026-11-22 — sprint written; all tasks `todo`

## 6. Budgets

| budget | limit | measured | tag |
|---|---|---|---|
| frame time | ≤ 16.7 ms | | |
| audio callbacks | ≈ 43/s at 1024 frames | | |
| backend errors | 0 | | |
| free heap after close | baseline ±4 KiB | | |
| VRAM | unchanged from the pre-sprint sample set | | |

## 7. Measurements

Protocol: device only; `reset` before each run; two runs per sample. A capture without a build marker and a
listening note is not acceptance.

| run | build | sample | frames | callbacks | errors | frame p95 | note |
|---|---|---|---|---|---|---|---|
| control | | libraylib sha256 | — | — | — | — | archive before first run |
| T2 | | audio_sound_loading | 600 | | | | |
| T3 | | audio_sound_multi | 600 | | | | |
| T4 | | audio_music_stream | 600 | | | | |
| T5 | | audio_mixed_processor | 600 | | | | |
| T6 | | audio_module_playing | 600 | | | | |

## 8. Exit criteria

- [ ] five samples build and run at 480×272 for 600 frames
- [ ] each is audible; the listening note says what was heard and when
- [ ] `callbacks>0` and `errors=0` for every run
- [ ] two identical captures per sample, stored under `/home/psp/evidence/sprint063/`
- [ ] free heap after close returns to baseline ±4 KiB; no decoder thread or channel left behind
- [ ] `examples/` untouched; no `src/` change in the diff
- [ ] Phase 13, the Samples page and memory-map §5 updated
- [ ] P13-S13 unblocked with the five green samples recorded

## 9. Result

Filled at close. Record what failed, what was abandoned, and any sample that needed a deviation from the
upstream structure.

## 10. Follow-ups

- P13-S13: the four `AudioStream` samples and the phase close.
- Phase 10: `audio_sound_positioning`.
- P13-S16: decide `audio_spectrum_visualizer` (CPU-FFT rewrite or drop).
