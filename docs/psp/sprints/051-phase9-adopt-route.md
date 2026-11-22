# Sprint 051 (P9-S7) — Adopt the chosen route (conditional)

| | |
|---|---|
| Roadmap phase | [Phase 9 — Audio](../phases/09-audio/index.md) |
| Sprint | P9-S7 |
| Branch / base commit | P9-S6 close sha (pin at start) |
| Started / closed | — / 2026-11-22 (`dropped` by P9-S5 T7) |
| Build number(s) | `audio b7`; pspgu `b24` unchanged |
| Tasks | 0/8 done; sprint `dropped` |

## 0. Handover

**Situation.** P9-S6 measured `sceSas` (route B) against route A and wrote a go/no-go for an owned Media
Engine mixer (route C). This sprint is **conditional**: it starts only if S6 authorized a route. If S6 says
route A is sufficient, mark every task here `dropped`, write the decision into the memory map, and close
Phase 9 — do not implement a route because the phase page lists it.

**Dropped (2026-11-22).** S6 was dropped by P9-S5 T7 because route A met every budget, so there is no
authorized route to adopt. The decision and the reopening triggers are in
[sprint 049 §9/§10](049-phase9-close-and-decision.md#9-result).

If S6 authorized route B, this sprint makes `sceSas` the implementation behind the existing raylib API. If
S6 authorized route C, this sprint is the ME takeover gate and will almost certainly split into more than
one time box; the task table below is the minimum, not a promise that one sprint is enough.

**Read before acting**, in this order:

1. `../sprints/050-phase9-sas-me-prototype.md` §7/§8 — the measured route-A vs route-B table, the voice
   semantics table, and the route-C gate.
2. `../sprints/049-phase9-close-and-decision.md` §7/§8 — route A's accepted budgets and the API matrix.
3. `../phases/09-audio/index.md` — the three routes and the “fallback” requirement.
4. `pspdev/psp/sdk/include/pspsascore.h` — voice limit 32, 44.1 kHz, grain 64–2048, volume `0x1000`,
   pitch base `0x1000`; `__sceSasCoreWithMix` is the mix call.
5. `../memory-map.md` §10.2 — ME rules: no VFPU, no libc, no GE, no cache coherence, exception handler,
   watchdog, suspend/resume, `scePowerLock(0)`.
6. `../memory-map.md` §11.5 — the ME repos to clone on demand; `me-tiny-lib`, `me-minimal-handler`,
   `me-spinlock`, `psp-me-bench`.

**First action.** Open with the S6 decision text. If it is “route A stays”, mark T1–T8 `dropped` and go to
§9; do not build anything. If it authorizes route B or C, copy the S6 threshold and the exact deficit into
§1 and start the branch named by the decision.

**Hard rules.**

- **No route change without S6 numbers.** “Cleaner” or “the ME is idle” is not evidence.
- **The public API does not change.** The game still includes only `raylib.h` and calls the same
  `Sound`/`Music` functions; no `sceSas`/`sceAudio` call appears in game code or the steady-state frame loop.
- **Route A remains the fallback.** A config switch (`PSP_AUDIO_ROUTE=miniaudio|sas|me`) selects the route;
  the default is route A until S7 closes. If the chosen route fails to init, fall back and log, never hang
  or report ready with silence.
- **`sceSas` is 44.1 kHz, 32 voices, VAG/PCM.** Do not claim ATRAC3; `sceAtrac` is separate. Map pitch,
  volume and pan explicitly (`0x1000` bases).
- **No ME code without an exception handler, watchdog and suspend/resume plan.** An ME exception is a silent
  hang; PPSSPP will not execute the code.
- **The renderer stays byte-identical.** No rlgl, pspgu or platform-render change.
- Commit messages carry the task ID: `sprint 051 T3: map Sound voices to SAS`.

**Commands.**

```sh
export PSPDEV=/home/psp/pspdev
export PATH=$PATH:$PSPDEV/bin

cd /home/psp/raylib-psp/src
make PLATFORM=PLATFORM_PSP -j8 PSP_AUDIO_ROUTE=sas   # or me; default miniaudio
cp libraylib.a $PSPDEV/psp/lib/libraylib.a

cd ../psp-tests/audio_smoke
make clean && make MODE=soak
( printf './audio_smoke.prx\n'; sleep 320 ) | pspsh
# after DONE: pspsh -e "reset"
```

**Do not touch.** `src/rlgl.h`, pspgu, renderer gates, the public raylib audio API, and route A's backend
except for the fallback hook.

**Done means** §8, or a clean `dropped` with the S6 decision if no route was authorized.

## 1. Objective

The route authorized by P9-S6 is the build's selected implementation for the raylib audio API, it beats or
matches route A on the measured budget that justified it, every supported API call behaves identically, and
a failed initialization falls back to route A without silence or a hang.

## 2. Why now

This is the only sprint allowed to change the default audio implementation. It exists so the decision is
implemented once, behind a switch, with the same API and the same tests — not as an unmeasured rewrite.

## 3. Scope

**In (route B):** `sceSas` voice allocation and mapping, `Sound` voices, `Music` through a SAS voice,
pitch/volume/pan, 32-voice limit, output call on the audio thread, config switch, fallback, conformance.

**In (route C):** reset-vector stub, mixer/resampler on the ME, uncached SPSC queues, watchdog, suspend/resume,
config switch, fallback; expected to split into C1/C2 rather than fit one sprint.

**Out:** renderer changes, public API additions, ATRAC3 decoding, and any route not authorized by S6.

## 4. Preconditions

- [ ] P9-S6 closed with a written go/no-go and the exact deficit or win
- [ ] If no route was authorized: every task here is `dropped`; skip to §9
- [ ] Route A build archived with its boot line, API matrix and gate hashes
- [ ] `reset` was run; partition 2 free reads 25,149,440 bytes
- [ ] `PSP_HEAP_SIZE_KB` declared; builds use `PSP_LARGE_MEMORY=0`
- [ ] The 5-minute soak and full API matrix from P9-S5 are reusable unchanged

## 5. Tasks

Status vocabulary — use exactly these words, in backticks: `todo` · `doing` · `done` · `blocked` · `dropped`.

| ID | task | status | verification | evidence |
|---|---|---|---|---|
| T0 | **Gate**: read S6's decision; if route A stays, mark T1–T8 `dropped` with the number | `todo` | decision text in §9; no build change | — |
| T1 | **Route switch**: `PSP_AUDIO_ROUTE=miniaudio|sas|me`; default stays `miniaudio` until exit; route A compiled in every build | `todo` | each arm builds; default boot line names the route | — |
| T2 | **Route B — voice mapping**: allocate/free SAS voices; map `PlaySound`/`Stop`/`Pause`/pitch/volume/pan; handle 32-voice exhaustion and stealing | `todo` | API matrix passes; 32 voices; no stuck voice | — |
| T3 | **Route B — `Music`**: stream decoded PCM through a SAS voice per grain, or document and implement the chosen alternative; no file I/O on the audio thread | `todo` | 5-minute stream + sounds; no dropout; seek/loop correct | — |
| T4 | **Route C — ME takeover** (only if authorized): reset-vector stub with a stack, exception handler, watchdog, uncached SPSC queues, mixer/resampler, suspend/resume; split if needed | `todo` | heartbeat → shadow mode → live with deadline watchdog; suspend/resume on hardware | — |
| T5 | **API conformance**: run the P9-S5 matrix unchanged against the selected route | `todo` | every supported call passes; unsupported ones documented | — |
| T6 | **Budget**: 5-minute soak at the game's maximum voice/sound count; frame, audio CPU, heap, `.text` vs route A | `todo` | meets or beats the S6 threshold; numbers in §7 | — |
| T7 | **Fallback**: force a route init failure; verify route A starts, logs the failure and `IsAudioDeviceReady()` is truthful | `todo` | audible fallback; no hang; no silent ready | — |
| T8 | **Close**: docs, memory map §8 decision, Phase 9 page; remove or gate the prototype; `git diff -- examples/` empty | `todo` | docs updated; renderer gates unchanged | — |

### Task log (append-only)

- 2026-11-21 — sprint drafted; conditional on P9-S6

## 6. Budgets

Use P9-S6's pre-declared threshold, not a new one.

| budget | limit | measured | tag |
|---|---|---|---|
| audio CPU | beat route A by the S6 threshold | | |
| frame time | no worse than route A at 60 fps | | |
| API parity | 100 % of the P9-S5 matrix the game uses | | |
| dropout | 0 in 5 minutes | | |
| fallback | route A starts on forced failure | | |

## 7. Measurements

| run | build | workload | metric | value | notes |
|---|---|---|---|---|---|
| route A control | `audio b5` | max game load | frame, audio CPU | | P9-S5 |
| selected route | `audio b7` | same | frame, audio CPU, heap, `.text` | | must meet threshold |
| soak | `audio b7` | 5 min | underruns, rest min, frame p95 | | |
| fallback | `audio b7` | forced init failure | ready, audible, log | | route A |

## 8. Exit criteria

- [ ] one route is selected by the S6 decision; the default build names it
- [ ] the selected route meets or beats the S6 threshold on the measured budget
- [ ] the full P9-S5 API matrix passes unchanged
- [ ] five minutes without dropout, counters confirming
- [ ] forced init failure falls back to route A, audibly and truthfully
- [ ] the steady-state frame loop has no `sceAudio`/`sceSas` call
- [ ] renderer gates are unchanged
- [ ] the decision, numbers and route switch are written to memory map §8 and the Phase 9 page
- [ ] if no route was authorized, T1–T8 are `dropped` with the S6 number, and Phase 9 is closed

## 9. Result

Not started. If P9-S6 authorized no route, record that here and mark the sprint `dropped`.

## 10. Follow-ups

- If route B was adopted, remove the route-A-only assumptions from the docs but keep the fallback build.
- If route C was adopted, keep the ME watchdog and suspend/resume tests in the release gate; a single
  successful run is not enough.
- If the selected route loses API parity, revert to route A; do not change the public API to fit the route.
- Any renderer or platform interaction discovered here belongs to Phase 10/12, not this sprint.
