# Sprint 050 (P9-S6) — `sceSas` / Media Engine prototype and measurement

| | |
|---|---|
| Roadmap phase | [Phase 9 — Audio](../phases/09-audio/index.md) |
| Sprint | P9-S6 |
| Branch / base commit | P9-S5 close sha (pin at start) |
| Started / closed | — / — |
| Build number(s) | `audio b6`; pspgu `b24` unchanged |
| Tasks | 0/8 done |

## 0. Handover

**Situation.** P9-S5 closed route A (miniaudio custom backend + raylib mixer) and recorded its budget. This
sprint does **not** ship a feature: it builds a throwaway route-B prototype on the firmware `sceSas` mixer,
measures it against route A, and gates route C (owned Media Engine mixer). P9-S7 only exists if this sprint's
numbers justify it.

`sceSas` is attractive because the firmware mixer already runs outside the user thread and supports 32
voices at 44.1 kHz. It is not free: it has its own voice model, a fixed 44.1 kHz rate, a grain size, and a
firmware module boundary. This sprint measures that, rather than assuming the phase page's optimism.

**Read before acting**, in this order:

1. `../phases/09-audio/index.md` — the three routes and the “measure before adopting” rule.
2. `../sprints/049-phase9-close-and-decision.md` §7/§8 — route A's frame, audio-thread and heap budgets;
   this sprint compares against those, not against a guess.
3. `pspdev/psp/sdk/include/pspsascore.h` — `__sceSasInit`, `__sceSasSetVoice`, `__sceSasSetVoicePCM`,
   `__sceSasSetPitch`, `__sceSasSetVolume`, `__sceSasCore`, `__sceSasCoreWithMix`, the 32-voice limit,
   44.1 kHz, grain 64–2048 and volume `0x1000`.
4. `pspdev/psp/sdk/lib/libpspsascore.a` — `psp-nm` shows the symbols are firmware stubs importing the
   `sceSasCore` module. The mix runs outside the user thread; the call itself still costs time.
5. Sony `sample/sound/at3mix` and `sas-streaming` — call order, 512/736 output frames, 64-byte buffers, and
   the buffer-size discrepancy below. Reading aid only; do not copy Sony code.
6. `../memory-map.md` §10.2 and §11.5 — ME rules and the repos to clone on demand (`me-tiny-lib`, etc.).
7. `../sprints/046-phase9-custom-backend.md` — the working route-A sink; reuse its channel/output idiom.

**First action.** Build `psp-tests/audio_sas_probe/` with `__sceSasInit(core, 256, 32,
PSP_SAS_OUTPUTMODE_STEREO, 44100)` and one PCM voice. Before trusting the output buffer size, allocate the
**larger** of the two documented sizes and fill it with a sentinel; one `__sceSasCore()` call reveals whether
the module writes 4 or 8 bytes per frame. Record that before adding voices.

**Hard rules.**

- **Prototype only.** No production `raudio.c` path, no default build switch, no change to route A. The
  probe lives in `psp-tests/` and is deleted or left behind a build flag.
- **Do not trust the SAS buffer-size formula.** `pspsascore.h:481,495` says
  `grainSize * 4 * sizeof(int16_t)` (8 B/frame); Sony's `at3mix` allocates
  `short[frames * STEREO]` (4 B/frame) and passes `&outputBuf[playPosition*STEREO]`. Resolve with the
  sentinel test before allocating a production buffer.
- **SAS is 44.1 kHz only, 32 voices, VAG/ADPCM or raw PCM.** ATRAC3 is not a SAS voice format; `sceAtrac`
  decodes it, then SAS mixes. Do not claim otherwise.
- **SAS volume is `0x1000`, not `0x8000`; pitch base is `0x1000` (range 1–`0x4000`).** Map deliberately.
- **No ME reset-vector takeover in this sprint.** Route C gets a go/no-go gate, not an implementation.
  An ME exception is a silent hang and needs a handler, watchdog and suspend handling; that is S7 work.
- **Measure on hardware only.** PPSSPP does not execute ME code and cannot answer the SAS/ME question.
- Commit messages carry the task ID: `sprint 050 T2: measure SAS voice mixing`.

**Commands.**

```sh
export PSPDEV=/home/psp/pspdev
export PATH=$PATH:$PSPDEV/bin

cd /home/psp/raylib-psp/psp-tests/audio_sas_probe
make clean && make
( printf './audio_sas_probe.prx\n'; sleep 30 ) | pspsh
# after DONE: pspsh -e "reset"
```

**Do not touch.** `src/raudio.c`, `src/platforms/raudio_psp.c`, the route-A library, renderer files, pspgu,
and the `psp_sdk_660` tree except as read-only reference.

**Done means** §8: measured numbers, a semantics table, and an explicit S7 go/no-go.

## 1. Objective

A minimal `sceSas` prototype plays PCM voices at 44.1 kHz through a PSP output channel, and the sprint
produces a measured comparison against route A: audio-thread CPU, frame impact at 1/8/16/32 voices, output
buffer size, voice-count behavior, and the API semantics that map or do not map to raylib. The result is a
written go/no-go for S7 and, separately, for a route-C (owned ME) prototype.

## 2. Why now

Route A is closed and measured. The phase page says to record the ME route as a decision, not to adopt it by
default. This sprint is the cheapest way to find out whether `sceSas` is a real improvement for this
workload before committing to a fork or an ME takeover.

## 3. Scope

**In:** `psp-tests/audio_sas_probe`; `libpspsascore`; PCM voice mapping; pitch/volume/pan; grain and buffer
experiments; comparison with route A; route-C go/no-go estimate.

**Out:** production integration, default build switch, ME reset vector, ATRAC3 decoding, and any change to
route A or the renderer.

## 4. Preconditions

- [ ] P9-S5 closed with route A's audio-thread, frame and heap budgets in §7
- [ ] `reset` was run; partition 2 free reads 25,149,440 bytes
- [ ] `libpspsascore.a` present in `$PSPDEV/psp/sdk/lib`; `psp-nm` shows the firmware stubs
- [ ] A 64-byte-aligned grain buffer and 64-byte-aligned output buffer exist in the probe
- [ ] `PSP_HEAP_SIZE_KB` is declared; builds use `PSP_LARGE_MEMORY=0`
- [ ] The operator can hear the output; audio is not captured by `scrshot`
- [ ] The ME reference repos are cloned on demand only; none is required for the SAS arm

## 5. Tasks

Status vocabulary — use exactly these words, in backticks: `todo` · `doing` · `done` · `blocked` · `dropped`.

| ID | task | status | verification | evidence |
|---|---|---|---|---|
| T1 | **SAS init**: `SceSasCore` 64-byte aligned; `__sceSasInit(core, 256, 32, PSP_SAS_OUTPUTMODE_STEREO, 44100)`; report return code | `todo` | init succeeds; no error log; sentinel untouched before first mix | — |
| T2 | **Buffer-size sentinel**: allocate the larger documented size, fill with `0xA5`, call `__sceSasCore()` once with one voice; determine bytes written | `todo` | written size recorded; production allocation uses the verified size | — |
| T3 | **PCM voice + output**: `__sceSasSetVoicePCM`, pitch `0x1000`, volume `0x1000`, `__sceSasCoreWithMix`, output via a regular channel or `sceAudioOutput2OutputBlocking` on the audio thread | `todo` | one voice audible; buffer 64-byte aligned; output call returns | — |
| T4 | **Voice scaling**: 1/8/16/32 voices; measure `__sceSasCoreWithMix` call time and audio-thread CPU against route A | `todo` | table of voices vs CPU/frame; route A comparison in §7 | — |
| T5 | **Semantics table**: pitch/volume/pan, loop, key-on/off, voice stealing, streaming PCM update, 44.1 kHz-only; mark each raylib `Sound`/`Music` semantic that maps or does not | `todo` | table complete; no “probably” entries | — |
| T6 | **Route C gate**: estimate an owned ME mixer from `me-tiny-lib`/samples: VFPU none, no libc, cache protocol, exception/watchdog, suspend/resume; write a go/no-go against route A | `todo` | gate text with the costs; no ME code written | — |
| T7 | **S7 decision**: apply the pre-declared threshold (see §6); recommend route A, B or C and name the exact work for S7 | `todo` | decision with numbers; if no route wins, S7 is `dropped` | — |
| T8 | **Cleanup**: probe is self-contained; `git diff -- src/ examples/` empty; route A build unchanged | `todo` | diff clean; probe build line documented | — |

### Task log (append-only)

- 2026-11-21 — sprint drafted; not started

## 6. Budgets and decision threshold

The threshold is declared **before** measurement so it cannot move afterwards.

| budget | limit | measured | tag |
|---|---|---|---|
| route B audio-thread CPU | must beat route A by ≥ 1 ms/frame at the game's maximum voice count, or remove a measured frame spike | | |
| frame time | no worse than route A at 60 fps | | |
| API parity | no raylib `Sound`/`Music` semantic that the game uses may be lost | | |
| memory | record grain, voice and output buffers | | |
| route C | only if route B is insufficient and the ME gate shows a credible path with watchdog/suspend | | |

If route A is already within budget, route B must show a **measured** spike or CPU headroom win, not a
theoretical one. “The ME is idle” is not a reason.

## 7. Measurements

Audio has no screenshot evidence. Record build marker, boot log, counters, audible confirmation and the
route-A control side by side.

| run | build | workload | metric | value | notes |
|---|---|---|---|---|---|
| route A control | `audio b5` | 16 sounds | audio CPU, frame | | from P9-S5 |
| SAS 1 voice | `audio b6` | PCM sine | SAS call ms, frame | | |
| SAS 8 voices | `audio b6` | PCM | SAS call ms, frame | | |
| SAS 16 voices | `audio b6` | PCM | SAS call ms, frame | | |
| SAS 32 voices | `audio b6` | PCM | SAS call ms, frame, dropouts | | limit |
| buffer sentinel | `audio b6` | one grain | bytes written/frame | | 4 vs 8 |

## 8. Exit criteria

- [ ] SAS initializes, mixes PCM voices and outputs through a PSP channel on hardware
- [ ] the actual SAS output-buffer bytes per frame are measured, and the header/sample discrepancy is resolved
- [ ] audio-thread CPU at 1/8/16/32 voices is measured against route A
- [ ] the raylib semantics that SAS can and cannot express are tabulated
- [ ] route C has a written go/no-go with the ME costs, not a guess
- [ ] S7 is explicitly authorized or dropped, with the number that decided it
- [ ] no production source, renderer file or route-A build changed

## 9. Result

Not started.

## 10. Follow-ups

- If S7 is authorized for SAS: plan voice allocation, `Music` streaming through a SAS voice, fallback to
  route A, and a config gate before touching the default build.
- If S7 is authorized for the ME: plan the reset-vector stub, exception handler, watchdog, uncached queues
  and suspend/resume as separate tasks; expect S7 to split.
- If neither: mark S7 `dropped`; write the decision into memory map §8 and close Phase 9.
