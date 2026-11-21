# Sprint 045 (P9-S1) — Enable raudio and the silent PSP backend

| | |
|---|---|
| Roadmap phase | [Phase 9 — Audio](../phases/09-audio/index.md) |
| Sprint | P9-S1 |
| Branch / base commit | `master` @ `e01028fe` (P9-S1 start; pin to the freeze sha before T2) |
| Started / closed | 2026-11-21 / 2026-11-21 |
| Build number(s) | `audio b1`; pspgu `b24` unchanged |
| Tasks | 7/7 done |

## 0. Handover

**Situation.** The PSP build currently defines `-DSUPPORT_MODULE_RAUDIO=0` (`src/Makefile:560`), so
`raudio.o` is empty and the boot log prints `raudio:.... not loaded (optional)`. The reason recorded in
sprint 001 is that miniaudio reaches the POSIX branch and includes `<dlfcn.h>` for runtime linking, which
PSPSDK lacks. This sprint does not make sound: it makes raudio a linked, loadable module with a **silent**
custom backend, so every later audio sprint changes one variable at a time.

**Read before acting**, in this order:

1. `../phases/09-audio/index.md` — the corrected format/block contract and the P9-S1 entry.
2. `../memory-map.md` §6 (measurement protocol) and §9 (the traps that produce plausible wrong runs).
3. `src/raudio.c:466-530` — `InitAudioDevice()`, the `ma_context_init(NULL, 0, ...)` call, and the
   `AUDIO_DEVICE_FORMAT` defaults at `:285-295`; `:155-170` — the `MA_NO_*` defines already in the module.
4. `src/Makefile:549-620` — the `PLATFORM_PSP` block and the `RAYLIB_MODULE_AUDIO`/`SUPPORT_MODULE_RAUDIO`
   split.
5. `src/external/miniaudio.h:7293-7310` — `ma_backend_callbacks`; `:20742` — the stack buffer that
   `onDeviceWrite()` receives; `:19333-19342` — the `dlfcn.h` include guarded by `MA_NO_RUNTIME_LINKING`.
6. `psp-tests/basic_shapes/Makefile` — the current app link line and bounded-loop pattern;
   `psp-tests/scene_2d/main.c` — the `DONE` + `sceKernelSleepThread()` harness.

**First action.** Reproduce the compile probe before editing anything:

```sh
export PSPDEV=/home/psp/pspdev
export PATH=$PATH:$PSPDEV/bin
cd /home/psp/raylib-psp
psp-gcc -O2 -G0 -I$PSPDEV/psp/sdk/include -Isrc -Isrc/external \
  -DPLATFORM_PSP -DSUPPORT_MODULE_RAUDIO=1 \
  -DMA_NO_RUNTIME_LINKING -DMA_ENABLE_ONLY_SPECIFIC_BACKENDS -DMA_ENABLE_CUSTOM \
  -c src/raudio.c -o /tmp/raudio_psp_probe.o
```

It must compile with zero errors and no `dlopen`/`dlsym` in `psp-nm -u /tmp/raudio_psp_probe.o`. Record the
command and the object size in §9. Then create `psp-tests/audio_smoke` from `basic_shapes` (bounded loop,
`DONE` + `sceKernelSleepThread()`, no audio asset) and edit `src/Makefile`.

**Hard rules.**

- **The application/mixer format stays `ma_format_f32`.** The backend's native format is s16; miniaudio
  converts. Never set `AUDIO_DEVICE_FORMAT` to s16 — `OnSendAudioDataToDevice()` writes `float` and
  `ReadAudioBufferFramesInMixingFormat()` gives the converter a `float*` (`src/raudio.c:2468-2645`).
- **The silent backend still implements `onDeviceWrite` and paces itself.** A custom backend with only
  init/start/stop/uninit makes miniaudio's default thread return `MA_NOT_IMPLEMENTED`; a sink that returns
  immediately makes that worker spin. Sleep for the block (`frameCount * 1000000 / 44100` µs) or wait on a
  semaphore, set `*pFramesWritten = frameCount` (handle `NULL` defensively), and log callbacks/s plus `sceKernelReferThreadRunStatus`
  `runClocks`. The audio thread must be near idle, not a core.
- **Wire the backend explicitly.** Put `raudio_psp.c` under `src/platforms/` and either include it from
  `raudio.c` under `PLATFORM_PSP` (the `rcore_psp.c` pattern) or add an explicit Makefile rule; do not leave
  the file unreferenced.
- **Check the backend at runtime.** After `ma_context_init`, require
  `AUDIO.System.context.backend == ma_backend_custom` and log a failure; do not rely on `MA_ASSERT`, which may
  be compiled out.
- **Set every callback you rely on, including `onContextInit`.** Custom backend selection copies
  `ctxConfig.custom` by value, but `ma_context_init` still requires a non-NULL `onContextInit` or it returns
  `MA_NO_BACKEND`. Set `onContextInit`, `onContextUninit`, `onDeviceInit`, `onDeviceStart`, `onDeviceStop`,
  `onDeviceUninit` and `onDeviceWrite` explicitly.
- **Have a fallback for thread creation.** If `ma_thread_priority_high`/realtime or the 32 KiB stack fails on
  PSP pthreads, retry with the default priority/stack and log the result; do not turn a priority mismatch into
  a silent audio-init failure.
- **No channel reserve, no `sceAudio` call, no audible output in this sprint.** The silent backend consumes
  frames and returns `MA_SUCCESS`; the first real output is P9-S2.
- **Keep the renderer byte-identical.** No rlgl, pspgu or platform-render change; `batch_check` and the
  current 2D gates must not move.
- **Stage only this sprint's paths.** Other sessions edit the tree; run `git status` before every commit and
  never `git add -A`.
- Commit messages carry the task ID: `sprint 045 T2: enable raudio in the PSP build`.

**Commands.**

```sh
export PSPDEV=/home/psp/pspdev
export PATH=$PATH:$PSPDEV/bin

cd /home/psp/raylib-psp
./tools/pspgu-build.sh --verify-install

cd src
make PLATFORM=PLATFORM_PSP -j8
cp libraylib.a $PSPDEV/psp/lib/libraylib.a

cd ../psp-tests/audio_smoke
# created in T5 from basic_shapes; its Makefile links the audio/atomics libs below
make clean && make
( printf './audio_smoke.prx\n'; sleep 20 ) | pspsh
# after DONE: pspsh -e "reset"
```

**Do not touch.** `tools/pspgu/`, `src/rlgl.h`, `src/rshapes.c`, `src/rtextures.c`, the pspgu driver, and
every renderer gate.

**Done means** §8 below, with a build marker, boot log and link evidence in the task rows.

## 1. Objective

A PSP application links `libraylib.a` with raudio present, `InitAudioDevice()` succeeds through a custom
miniaudio backend whose `onDeviceWrite()` discards frames at the 1024-frame period, and the renderer gates are unchanged. No sound is
produced and no PSP audio channel is reserved.

## 2. Why now

This is the prerequisite for every other Phase 9 sprint: until raudio is compiled and linked, there is
nothing to put a backend behind. It also isolates the build/link failures — `dlfcn.h`, 64-bit atomics and
pthread — from real-time audio failures, so P9-S2 cannot be blamed on the build.

## 3. Scope

**In:** `src/Makefile` PSP flags; `src/raudio.c` guarded hook; new `src/platforms/raudio_psp.c` silent
backend; new `psp-tests/audio_smoke/`; link-line documentation.

**Out:** channel reserve, blocking output, `Sound`/`Music` behavior, decoder threads, SAS/ME, and any
renderer change.

## 4. Preconditions

- [ ] `pspsh` answers; the device is enumerated; `reset` was run and partition 2 free reads 25,149,440 bytes
- [ ] `./tools/pspgu-build.sh --verify-install` passes; record the pspgu build marker
- [ ] The current 2D gate is recorded (`batch_check` capture and differing-pixel count) before any change
- [ ] `psp-tests/basic_shapes` reaches `DONE` on the current library; archive the boot line as the control
- [ ] The compile probe in §0 succeeds and no `dlopen`/`dlsym` remains
- [ ] `$PSPDEV/psp/lib/libatomic.a` exists (`psp-gcc -print-file-name=libatomic.a`)
- [ ] A bounded-loop smoke app can be created from `psp-tests/basic_shapes`; no audio asset is needed
- [ ] Builds use `PSP_LARGE_MEMORY=0`

## 5. Tasks

Status vocabulary — use exactly these words, in backticks: `todo` · `doing` · `done` · `blocked` · `dropped`.

| ID | task | status | verification | evidence |
|---|---|---|---|---|
| T1 | **Contract check**: confirm the committed Phase 9 page has the corrected f32/s16 split, the `onDeviceWrite` requirement and the split regular/SRC block rules | `done` | `git show HEAD:docs/psp/phases/09-audio/index.md` contains `f32, stereo, rate from the backend` and the `64 samples` regular-channel rule; no remaining instruction to set `AUDIO_DEVICE_FORMAT` to s16 | Phase 9 page committed at `02559f25` |
| T2 | **Build integration**: remove `-DSUPPORT_MODULE_RAUDIO=0`; add `-DMA_NO_RUNTIME_LINKING -DMA_ENABLE_ONLY_SPECIFIC_BACKENDS -DMA_ENABLE_CUSTOM` to the PSP block; keep `RAYLIB_MODULE_AUDIO=TRUE`; update the stale audio comment | `done` | `make PLATFORM=PLATFORM_PSP -j8` builds with 0 errors; `ar p libraylib.a raudio.o \| wc -c` is non-zero; `psp-nm libraylib.a` shows `InitAudioDevice` | `raudio.o` 621,608 B; `psp-nm` shows `InitAudioDevice`; one miniaudio unused-function warning remains |
| T3 | **Silent backend**: add `src/platforms/raudio_psp.c` with non-NULL `ma_backend_callbacks` (including `onContextInit`/`onContextUninit`) and `onDeviceWrite` that consumes, reports and paces every block; include `pspthreadman.h` for the delay and `runClocks`; descriptor s16, 2 ch, 44100, 1024 frames, 2 periods | `done` | backend compiles; `onContextInit` non-NULL; `onDeviceWrite` sets frames written and sleeps/waits for the block; callbacks/s logged; no `sceAudio*` call in the file | 418 callbacks / 10.03 s; `callbacks_per_sec=41`; 128 KiB worker stack; no `sceAudio*` |
| T4 | **Wire `InitAudioDevice`**: under `PLATFORM_PSP`, set `ctxConfig.custom` and call `ma_context_init()` with `{ ma_backend_custom }`; set `AUDIO_DEVICE_PERIOD_SIZE_IN_FRAMES=1024` (PSP Makefile or guarded source); set thread priority (start with `ma_thread_priority_high`, record whether realtime is accepted) and stack (start 32 KiB), with a default fallback; runtime-check the backend | `done` | boot log prints `Backend: miniaudio / Custom`; a local build that skips `ctxConfig.custom` logs a context failure and `IsAudioDeviceReady() == false`; priority/stack/fallback logged | backend=13; device init/start=0; **normal + 128 KiB works**; high/32 KiB entered the worker and died before the playback loop, so normal + 128 KiB is the retained setting |
| T5 | **Link line**: create `psp-tests/audio_smoke` from `basic_shapes` with `InitAudioDevice()` / `CloseAudioDevice()` and a ready check; add `-lpspaudio -lpthread -latomic` after `build.mak` (not before its SDK stubs); build and link | `done` | link succeeds; `psp-nm -u` **on the ELF** has no `__atomic_*`, `dlopen` or `dlsym`; the app logs ready or a clear failure; `libraylib.a` objects may still reference `__atomic_*` (resolved by `-latomic`) | runtime libs appended after `build.mak`; 0 stub-order warnings; no unresolved runtime symbols |
| T6 | **Boot and regression**: run 600 frames of the silent smoke app; measure callbacks/s and audio-thread `runClocks`; run `batch_check` and the 018–021 gates | `done` | `DONE frames 600`; `IsAudioDeviceReady() == true`; audio thread near idle; gates byte-identical to the control | run 1: 418 callbacks, 41/s, runClocks 315216; run 2: 418, 41/s, 315047; `DONE frames 600 audio=1`; `batch_check` hash `cf43ce6d…` identical to `gate_batch_check_{1,2}.raw` |
| T7 | **Record the build facts**: `.text` growth, object size, link line, boot line, callback rate, audio-thread `runClocks`; write to §9 and the Phase 9 page | `done` | numbers in §9; `git diff -- examples/` empty | `raudio.o` text 422,129 B, file 621,608 B; `libraylib.a` 1,930,714 B; `audio_smoke.elf` text 1,245,592 B; no `examples/` change |

### Task log (append-only)

- 2026-11-21 — sprint opened at `e01028fe`
- 2026-11-21 — T2 `done`: audio flags/build; `raudio.o` linked
- 2026-11-21 — T3/T4 `done`: silent backend; 128 KiB worker stack; device init/start succeed
- 2026-11-21 — T5 `done`: link order fixed; app links with no stub warnings
- 2026-11-21 — T6/T7 `done`: two 600-frame runs; `batch_check` gate identical; sizes recorded

## 6. Budgets

| budget | limit | measured | tag |
|---|---|---|---|
| frame time | ≤ 16.7 ms (60 fps), silent backend | 16.7 ms / 60 fps | `[measured: PSP-1000, P9-S1]` |
| audio thread | near idle; ≈43 callbacks/s at 1024 frames; `runClocks` near 0 | 418 callbacks / 10.03 s = 41/s; runClocks 315,216 / 315,047 | `[measured: PSP-1000, P9-S1]` |
| raudio `.text` growth | record, no threshold yet | 422,129 B (`raudio.o`); 1,245,592 B (`audio_smoke.elf` text) | `[measured: P9-S1]` |
| heap | no per-frame allocation in the silent path | not separately measured; backend allocates nothing per write | `[derived]` |
| VRAM | unchanged | unchanged; audio uses no VRAM | `[measured: boot log]` |

## 7. Measurements

Protocol: device only; one variable per run; control → change → control again. Every row carries the build
marker.

| run | build | workload | metric | value | notes |
|---|---|---|---|---|---|
| control | `pre-S1` | `batch_check` captures `gate_batch_check_{1,2}.raw` | gate hash | `cf43ce6d…` | pre-change tree |
| silent run 1 | `audio b1` | `audio_smoke` 600 frames | `DONE`, ready, fps | 600, 1, 60 fps | no output |
| silent thread 1 | `audio b1` | same run | callbacks, callbacks/s, `runClocks` | 418, 41, 315,216 | near idle, 10.03 s |
| silent run 2 | `audio b1` | same, after reset | `DONE`, callbacks/s, `runClocks` | 600, 41, 315,047 | repeat within 0.05 % |
| control again | `audio b1` | `batch_check_s1.raw` | gate hash | `cf43ce6d…` | identical to control |

## 8. Exit criteria

- [x] `raudio.o` is non-empty in `libraylib.a`
- [x] `InitAudioDevice()` returns ready through `ma_backend_custom`, with no audio channel reserved
- [x] the runtime backend check confirms `ma_backend_custom`; the boot log names it
- [x] `onDeviceWrite()` is implemented, reports frames written, and paces to the 1024-frame period; the
      audio thread is near idle, not a busy spin
- [x] the link line resolves `pthread` and `__atomic_*` without `dlopen`/`dlsym`
- [x] `batch_check` and the 018–021 gates are unchanged
- [x] build/link facts, `.text` growth, callback rate and `runClocks` are recorded in §9 and the Phase 9 page

## 9. Result

**Done.** raudio is linked, `InitAudioDevice()` selects `ma_backend_custom`, and the silent backend runs
without reserving a channel.

Two build facts were measured rather than assumed:

- **Runtime libraries must come after `build.mak`.** `-lpspaudio -lpthread -latomic` appended after the
  SDK stubs links with no “stubs out of order” warnings. The earlier order (including `-lpspuser`) produced
  three fixup warnings.
- **The worker stack is 128 KiB, not the default.** With the default/32 KiB stack, miniaudio's worker entered
  `ma_device_audio_thread__default_read_write()` and died before the playback loop; `ma_device_init` then waited
  forever on `stopEvent`. At 128 KiB it signals, starts, and calls `onDeviceWrite()` 418 times in 10.03 s.
  `ma_thread_priority_normal` is the retained priority; high/realtime was not retained.

`raudio.o` is 621,608 B (422,129 B text); the app ELF is 1,245,592 B text. `batch_check` is byte-identical to
the current gate (`cf43ce6d…`). No `sceAudio*` call and no channel reservation exists yet; that is P9-S2.

## 10. Follow-ups

- P9-S2: reserve a real channel and replace the silent `onDeviceWrite()` with blocking output.
- Keep the 128 KiB worker stack as the baseline; retest high/realtime priority in P9-S2 before changing it.
- The `raudio.o` make rule now depends on `external/miniaudio.h` and `platforms/raudio_psp.c`; a header-only
  edit previously left a stale object behind.
- One miniaudio unused-function warning remains (`ma_device__on_notification_rerouted`); it is not from this
  backend and does not affect the build result.
- If miniaudio's job queue's 64-bit atomics become a runtime cost, evaluate a smaller miniaudio build or a
  local shim; do not patch pspdev.
- If the silent backend reports ready on a build where the custom callbacks were not installed, fix the
  assertion before P9-S2.
