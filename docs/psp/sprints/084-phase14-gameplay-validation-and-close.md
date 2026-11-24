# Sprint 084 (P14-S3) — Gameplay validation and close

| | |
|---|---|
| Roadmap phase | [Phase 14 — PSP Raylib Games](../phases/14-raylib-games/index.md) |
| Sprint | P14-S3 |
| Branch / base commit | `master` @ `7d495d16`; port at `/home/psp/galaga-psp` from P14-S2 |
| Started / closed | 2026-11-23 (written) / — |
| Build number(s) | `galaga-b3` onward; driver chosen in P14-S1 T1 |
| Tasks | 1/7 done — T2 soak and T3 attribution in progress; T4–T7 open |

## 0. Handover

**Situation.** P14-S1 delivered the backend, renderer, text, input and loop. P14-S2 made the asset set fit and
added audio and EBOOT packaging. This sprint proves the game as a game: a deterministic run from title to game
over, a ten-minute soak, a frame-time attribution, and the phase's close. It measures a build that should no
longer be moving.

**Read before acting**, in this order:

1. [Memory map](../memory-map.md) §4 (where time goes), §6 (measurement protocol), §9 (traps), §13 (task
   tracking and how a sprint closes).
2. [Phase 14](../phases/14-raylib-games/index.md) — the exit criteria this sprint satisfies.
3. P14-S1 §9 and P14-S2 §9 — the display/driver/format decisions and the first numbers; do not re-open them
   unless a measurement contradicts them.
4. The port's own `README`/notes and its build script; `src/entity/GameController.cpp` for the state machine and
   the places where a deterministic input tape can be injected.

**First action.** Freeze the build and record it:

```sh
cd /home/psp/galaga-psp
git -C /home/psp/raylib-psp rev-parse --short HEAD   # library revision
sha256sum galaga.prx EBOOT.PBP                        # the artifacts under test
```

Then add the autoplay switch (T1) and take the baseline run before touching anything else.

**Hard rules.**

- A run is a numbered procedure: what to press, how long, what not to touch, which file to bring back, what
  success looks like. "Closed" is a word about a run with a build number, not about a change.
- Two captures of the same stopped frame must be identical before a frame is trusted.
- No new feature work in this sprint. If a defect needs a code change, fix it, re-run the affected baseline,
  and record the delta; do not broaden scope.
- Device only for numbers. PPSSPP may be used for a quick "does it boot" check, never for timing or memory.
- The port, assets and EBOOT remain outside this repository.

**Commands.**

```sh
export PSPDEV=/home/psp/pspdev
export PATH="$PATH:$PSPDEV/bin"

cd /home/psp/galaga-psp && make
pspsh -e "reset"
( printf './galaga.prx\n'; sleep 120 ) | pspsh
pspsh -e "scrshot host0:/galaga-soak.raw"
pspsh -e "reset"
```

Keep the shell attached; never send `exit`; the app logs `DONE` and sleeps.

**Do not touch.** The display mapping, the texture format and the audio design settled in P14-S1/S2; raylib's
`src/`; another session's `tools/pspgu/` work.

**Done means** §8 with evidence in every task row.

## 1. Objective

A deterministic autoplay build completes title → stage(s) → game over in one run, twice, with identical
captures; a 10-minute soak shows stable heap and VRAM, zero audio dropouts and no crash; the frame-time
attribution says where the work goes at 60 fps; and the phase's exit criteria are checked with numbers rather
than feel.

## 2. Why now

P14-S1 and S2 prove the parts. A game can still fail at the joins: a scene transition that leaks entities, a
sound that exhausts its alias pool after ten minutes, a texture that is evicted and never reloaded, a heap that
fragments. Those failures need time and repetition, and they cannot be measured while the build is still
changing.

## 3. Scope

**In:** the deterministic input tape/autoplay switch; the ten-minute soak; frame-time and submission
attribution; heap/VRAM high-water; input-feel calibration; the port README and packaging check; the phase close
and memory-map updates.

**Out:** new gameplay features; tate mode (follow-up); high-score persistence; any library change not already
proven necessary; asset work beyond a defect fix.

## 4. Preconditions

- [ ] P14-S2 closed: zero texture allocation failures, zero audio dropouts in five minutes, EBOOT launches
- [ ] The build and the library revision are frozen and hashed
- [ ] The device log prints build number, frame time, heap and VRAM high-water
- [ ] `pspsh` answers; `reset` done; partition 2 free reads 25,149,440 B
- [ ] The autoplay tape is committed in the port (not in this repository) and its seed is recorded
- [ ] The user has authorised a long device session

## 5. Tasks

Status vocabulary — `todo` · `doing` · `done` · `blocked` · `dropped`.

| ID | task | status | verification | evidence |
|---|---|---|---|---|
| T1 | **Deterministic run.** Add a `GALAGA_AUTOPLAY` build that replays a fixed left/right/fire tape by frame index; log the tape and a final state hash | `done` | two runs produce identical captures and the same final state hash | `AUTOPLAY=1` tape driven by simulation updates; `RUN_SECONDS` limits by game time; autoplay reaches stage 1, score 310, lives 2 at t=30 s |
| T2 | **Ten-minute soak.** Run the tape for 10 minutes; log frame p50/p95, the declared heap high-water, VRAM, audio dropouts, entity counts | `doing` | no crash; heap and VRAM flat within the recorded band; 0 dropouts | soak running; 0 underruns to t=30 s; 33–37 fps at t=55–70 s |
| T3 | **Frame attribution.** Split work into update, draw submission and GE/vblank; use the backend's logical counters plus pspgu's `pspguVramStats`/`pspguResidencyStats`; identify the largest cost at 60 fps | `doing` | a table that sums to the measured frame work, not a single number | `work` counter in `STATS`; 1.1–1.5 ms quiet title/stage, 6.9–8.5 ms in the stressed stage; no split yet |
| T4 | **Memory.** Record heap high-water and largest free block over the soak; check entity spawn/destroy churn; if growth is found, pool the offending type and record the delta | `todo` | high-water and fragmentation figures; no monotonic growth | heap partition free 4.25 MB stable in the 30 s arm; soak open |
| T5 | **Input feel.** Verify D-pad and nub control, deadzone, repeat-fire (`isKeyPressedOnce` semantics) and the non-gameplay exit; record the nub's raw range on this unit | `todo` | a numbered button checklist on the device; no lost press, no stuck key | mapping implemented; physical test open |
| T6 | **Packaging and README.** EBOOT, asset directory, build and run instructions, the local-only licensing note | `todo` | a clean copy of the EBOOT + assets launches from the Memory Stick; README reproduces the build | — |
| T7 | **Phase close.** Check the phase's exit criteria, write the result, update the phase page and the memory-map facts/decision log | `todo` | every criterion has a number or a recorded reason; links and tables current | — |

### Task log (append-only)

- 2026-11-23 — sprint opened, all tasks `todo`
- 2026-11-24 — T1 `done`; T2/T3 `doing`; T4–T7 `todo`

### Design notes

**T1 — why an input tape.** The game has no scripted mode and no save. A manual run is not repeatable, so it
cannot produce two identical captures or a comparable final state. Add a compile-time tape: a table of
`(frame, key, down/up)` entries consumed instead of the pad. Keep it out of the shipped build. The final state
hash can be as small as `(score, stage, lives, entity count, gameElapsedTime quantised)`; it only has to catch
divergence.

**T2 — what "stable" means.** Heap high-water may rise during the first stage as entities spawn, then must
plateau. Record the plateau, not the peak, against the declared `PSP_HEAP_SIZE_KB` and the `entry free` figure.
VRAM must show 0 allocation failures and no eviction/reload storm in a steady frame; the pspgu log already
reports these. Audio dropouts are counted by the port's own callback gap counter, not by ear.

**T3 — attribute before optimising.** The Phase 9 pattern applies: main-thread audio cost, update cost and
submission cost are separate numbers. Logical draw calls come from the backend's own counters; GPU state and
VRAM come from pspgu's exported `pspguVramStats`/`pspguResidencyStats`. Do not add the `-Wl,--wrap` harness to
the app. If the frame is already inside 16.7 ms with headroom, the correct result is "no optimisation" with
the table attached. Do not chase a change that is under the 0.1 % repeatability floor.

**T4 — allocation churn is the likely finding.** The game uses `std::list<std::shared_ptr<Entity>>`, creates
and destroys bullets and explosions every second, and constructs a `std::map`-keyed resource layer once. If
the heap high-water keeps climbing, pool the bullet/explosion type in the port; do not change the game's
entity API to do it.

**T5 — the shipped defaults.** Phase 8's calibrated nub (centre `(133,112)`, radial deadzone 0.18) is the
starting point; keep the raw range recorded per unit. `FIRE` is edge-triggered by the game, so holding it does
not autofire — that is the upstream behaviour and must not be "fixed". `PAUSE` and `EXIT` are unused by the
game; the backend's exit must be a separate, documented combination (SELECT) that logs `DONE` and sleeps.

**T6 — local only.** The README must state that the upstream has no licence and the assets are Namco material;
the build is for personal/local use and is not redistributable. Keep the upstream commit and the port's
modifications in a short, dated note.

## 6. Budgets

| budget | limit | measured | tag |
|---|---|---|---|
| frame time | p95 ≤ 16.7 ms at 60 fps | | |
| soak | 10 minutes, no crash | | |
| heap | declared heap plateau; no monotonic growth | | |
| VRAM | stable; 0 allocation failures, 0 eviction storms | | |
| audio | 0 dropouts | | |
| determinism | two runs, identical captures and state hash | | |
| repeatability | reportable difference ≥ 0.1 % | | |

## 7. Measurements

| run | build | workload | metric | value | notes |
|---|---|---|---|---|---|
| baseline | galaga-b1 | autoplay title→stage, 30 s | frame / work | **60–58 fps; 1.2–6.0 ms** | t=5–26 s |
| baseline | galaga-b1 | autoplay, 30 s | final state | **stage 1, score 310, lives 2** | deterministic across audio on/off |
| baseline | galaga-b1 | autoplay, 55–70 s | fps / work | **33–37 fps; 6.9–8.5 ms** | heavy stage; soak running |
| baseline | galaga-b1 | autoplay, 10 min | heap / VRAM | **4.25 MB partition free; 1,766,912/330,240 B; 0 failures** | flat to t=70 s |
| determinism | galaga-b1 | two title runs | capture diff | **none (identical)** | full-screen stretch |

## 8. Exit criteria

- [ ] §1 is met, with the determinism and soak numbers in §7
- [ ] the phase's exit criteria are checked line by line, each with a number or a recorded reason
- [ ] no per-frame stall or wait introduced; audio and input do not appear in the frame profile
- [ ] the EBOOT + assets launch from a clean Memory Stick copy
- [ ] the port's source, assets and EBOOT are still outside this repository
- [ ] new facts written back into `memory-map.md` §3/§9, and decisions into §8
- [ ] the phase page's state row and sprint table are current

## 9. Result

**P14-S3 is in progress.** The deterministic autoplay tape runs by simulation update and `RUN_SECONDS` limits
the run by game time, so audio-on and audio-off arms reach the same state (stage 1, score 310, lives 2 at 30 s).
The quiet pspgu driver and the 512-frame audio backend hold 60 fps and **0 underruns** for the first 30 s.

The 10-minute soak is still running. It has reached **GAMEOVER** at t≈75 s (stage 1, score 2320, lives 0) with
0 audio underruns to that point. Under sustained autoplay the game drops to 33–40 fps in the heavy phase and
runs the post-game attract at 44–45 fps; the main-thread work at the drop is 4.7–8.5 ms, so the remaining frame
time is scheduler/audio interaction rather than draw submission. The soak result, p50/p95, EBOOT packaging and
the physical input pass remain open.

## 10. Follow-ups

- Tate mode, packed assets and any performance work below the repeatability floor — after the phase, only if
  the numbers justify them.
- A second game port is a new phase-sized decision, not a continuation of this sprint.
