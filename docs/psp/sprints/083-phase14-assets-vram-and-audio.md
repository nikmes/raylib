# Sprint 083 (P14-S2) — Assets, VRAM and audio

| | |
|---|---|
| Roadmap phase | [Phase 14 — PSP Raylib Games](../phases/14-raylib-games/index.md) |
| Sprint | P14-S2 |
| Branch / base commit | `master` @ `7d495d16`; port at `/home/psp/galaga-psp` from P14-S1 |
| Started / closed | 2026-11-23 (written) / — |
| Build number(s) | `galaga-b2` onward; driver chosen in P14-S1 T1 |
| Tasks | 0/8 done |

## 0. Handover

**Situation.** P14-S1 put the game on the device: renderer, 8-pixel text, input and the fixed-step loop, with
the title scene and one stage frame. This sprint makes the full asset set fit, gives the game its audio, and
packages it so it launches without PSPLink. It starts from the P14-S1 build and a recorded texture-budget line;
it must not change the display decision from P14-S1 T1.

**Read before acting**, in this order:

1. [Memory map](../memory-map.md) §3 (VRAM/framebuffer budget), §4 (submission cost), §6 (measurement
   protocol), §9 (traps: allocation at draw time, texture residency, D-cache).
2. [Phase 14](../phases/14-raylib-games/index.md) — the texture-budget table, the capability map and the open
   decisions.
3. P14-S1's §9 decision table and §7 first-frame numbers.
4. `src/config.h` and `src/rtext.c` for the font-atlas limit; `src/raudio.c` (`PlaySound`, `PlaySoundStream`,
   `LoadSoundAlias`) and `src/platforms/raudio_psp.c` for audio semantics; `psp-tests/scene_2d/main.c` for the
   text and texture-staging patterns.

**First action.** Reproduce the current budget from the device log, not from the phase page:

```sh
cd /home/psp/galaga-psp && make
pspsh -e "reset"
( printf './galaga.prx\n'; sleep 10 ) | pspsh
```

Record the `VRAM`/`RESIDENCY` line and the boot free/largest figures in §7. If the line does not exist, the
first task is to make the port print it before doing anything else.

**Hard rules.**

- No allocation at draw time; the texture set is loaded once, before the first frame, and reported.
- The port and its assets stay outside this repository. No `res/` file, no EBOOT and no generated atlas may be
  committed here.
- `PlaySound` restarts the same buffer. Do not assume SDL_mixer's per-call voice allocation; use aliases.
- Music is one stream at a time; stop the previous stream before starting the next.
- All timing and dropout numbers are device-only. PPSSPP does not emulate the Media Engine and does not answer
  audio cost.
- Keep the upstream clone read-only; all changes are in the port.

**Commands.**

```sh
export PSPDEV=/home/psp/pspdev
export PATH="$PATH:$PSPDEV/bin"

# pspgu format arm, only if P14-S1 chose it (rebuilds the installed libGL.a)
/home/psp/raylib-psp/tools/pspgu-build.sh --texfmt=auto
/home/psp/raylib-psp/tools/pspgu-build.sh --check

# build and run
cd /home/psp/galaga-psp && make
pspsh -e "reset"
( printf './galaga.prx\n'; sleep 60 ) | pspsh
pspsh -e "scrshot host0:/galaga-stage.raw"
pspsh -e "reset"

# EBOOT: pack, copy the asset directory beside it, verify both sides
```

**Do not touch.** `src/` and `examples/` in this repository; the display mapping; the game's entity logic; any
other session's `tools/pspgu/` changes.

**Done means** §8 with evidence in every task row.

## 1. Objective

The complete title + stage asset set (27 PNG, 23 WAV) loads with **zero allocation failures** and fits the
recorded VRAM budget; 8-pixel text is byte-identical across two captures; audio plays all 17 sounds and 6
tracks with **zero dropouts** in a 5-minute run; and the packaged EBOOT launches from the Memory Stick without
PSPLink and reaches the title screen in ≤ 3 s.

## 2. Why now

P14-S1 proved the seam; this sprint proves the port is viable as a whole. VRAM and audio are the two places
where "it draws" can still be false: a texture that silently fails to allocate renders uninitialised VRAM, and
a music stream that blocks or a sound pool that exhausts itself is audible only on hardware.

## 3. Scope

**In:** texture repacking/atlas and 16-bit format choice; `title_shine` split; font loading/baking; the
`raylib_impl/AudioPlayer.cpp`; asset staging and load-time accounting; EBOOT packaging; a 5-minute
title→stage→game-over run.

**Out:** deterministic input tape and the 10-minute soak (P14-S3); performance attribution (P14-S3); tate mode
(follow-up); high-score persistence (not in upstream); any change to raylib's public API.

## 4. Preconditions

- [ ] P14-S1 T1's display and driver decisions are recorded and unchanged
- [ ] P14-S1's title and stage frame render; two identical captures exist
- [ ] The device log prints the `VRAM`/`RESIDENCY` counters and a build number
- [ ] `pspsh` answers; `reset` done; partition 2 free reads 25,149,440 B
- [ ] The asset set is staged under one root; PSPLink serves it as `host0:`
- [ ] `PSP_LARGE_MEMORY=0`; EBOOT title and asset root chosen
- [ ] The user has authorised a device session

## 5. Tasks

Status vocabulary — `todo` · `doing` · `done` · `blocked` · `dropped`.

| ID | task | status | verification | evidence |
|---|---|---|---|---|
| T1 | **Budget and format.** Record the current padded set and free VRAM; choose 16-bit `--texfmt=auto`, a repack, or both; confirm zero allocation failures | `todo` | boot line: used/free/largest; 27 textures loaded; 0 failures | — |
| T2 | **Repack and split.** Split `title_shine` (540×36) into ≤512 pieces; optionally pack the sprite sheets into one atlas. Measure draws/binds before and after | `todo` | every sprite and frame renders; draw/bind delta recorded; adopt only if it wins | — |
| T3 | **Text parity.** If the runtime 8 px TTF is unstable, bake the atlas offline; keep the fixed 8-pixel advance | `todo` | HUD/message checklist; atlas ≤512; no default-font fallback; two identical captures | — |
| T4 | **Audio backend.** Load 17 sounds and 6 music tracks; implement `initAudio`/`closeAudio`, play/stop, alias pools for repeated SFX, stop-previous for music, and one `UpdateMusicStream` per frame | `todo` | every enum plays; repeated shots overlap; starting a track stops the previous; no blocking call in the frame | — |
| T5 | **Startup and load.** Measure the open count and time to first frame; stage or pack assets; no black screen longer than the measured load | `todo` | load time and open count in §7; no missing-file log | — |
| T6 | **EBOOT packaging.** PARAM.SFO, title, asset root under `ms0:/PSP/GAME/<TITLE>/`; verify beside-EBOOT paths | `todo` | EBOOT launches without PSPLink; title appears; same captures as the PRX run | — |
| T7 | **Five-minute run.** Title → stage → game over with audio; log frame time, dropouts, heap and VRAM high-water | `todo` | 0 dropouts; 0 allocation failures; no crash; p95 frame time recorded | — |
| T8 | **Close and decide.** Record which format/atlas/packaging arms were adopted; update the phase page and §9 | `todo` | decision table in §9; phase page's state row updated | — |

### Design notes

**T1 — the arithmetic to confirm.** The phase page's padded total is **908,544 B** at 8888 against
**913,344 B free**, before the font — a set that cannot coexist with an atlas. At 16 bits the same set is
**454,272 B**; pspgu's `auto` chooses 5650 for opaque RGB and 4444 for RGBA whose alpha is all 0/255, otherwise
8888. Check the actual per-texture choice in the boot log; do not assume every sprite is 4444.

**T2 — what an atlas buys.** The 13 animation sheets are 384×16 each, padded to 512×16: 425,984 B and thirteen
distinct texture ids. A single 512×512 4444 atlas (524,288 B) costs about 100 KiB more but turns every sprite
into one bind and one run, and removes the width padding. Adopt it only if the draw/bind counters and the frame
work show a win; the alternative (16-bit per texture, no atlas) is a valid close.

**T2 — `title_shine`.** 540 wide, 15 frames of 36×36. pspgu's 512-window path can sample it from a 1024×64
buffer (262,144 B at 8888), but that is 29 % of the whole budget for one title-screen effect. Split it into two
pieces or pack its 15 frames; do not leave a 1024-wide texture in the set.

**T3 — the font.** PressStart2P is SIL OFL and is the font the layout was designed around. `LoadFontEx` at 8 px
produces a small atlas; the library rejects anything over 512 in either dimension (`src/rtext.c:576-581`), so
check the boot log. If the rasterised glyphs are not crisp on the GE, pre-render the same atlas on the host
and load it as a bitmap font; the advance stays 8 px either way.

**T4 — audio semantics.** `PlaySound` calls `PlayAudioBuffer` on the sound's single buffer and restarts it
(`src/raudio.c:714`). Keep a small pool of `LoadSoundAlias` copies for `FIGHTER_SHOT` and the explosion sounds
and rotate through them. `playMusic` must `StopMusicStream` the current track first, then `PlayMusicStream` the
new one; pump `UpdateMusicStream` once per frame. Phase 9's budget is 8 sounds + 1 streamed track at 60 fps with
0 dropouts; this game uses fewer simultaneous voices.

**T5 — the open cost.** 27 PNG + 23 WAV = 50 opens. At ~17 ms per Memory Stick open that is ~0.85 s of pure
latency, plus decode. Measure it; if it exceeds 3 s, pack the assets into one archive or move the load behind a
progress screen. Do not add a per-frame allocator to hide it.

**T6 — paths.** The game's relative `res/...` strings work if the process cwd is the EBOOT directory. Set one
`GALAGA_ASSET_ROOT` constant (`host0:` for PSPLink, `ms0:/PSP/GAME/GALAGA/` for EBOOT) and prefix every load;
do not scatter `#ifdef PLATFORM_PSP` through gameplay code.

## 6. Budgets

| budget | limit | measured | tag |
|---|---|---|---|
| VRAM | ≤ 913,344 B free tail (or 1,191,872 without depth); 0 allocation failures | | |
| texture format | 16-bit where the source permits; 8888 only when required | | |
| frame time | ≤ 16.7 ms at 60 fps | | |
| frame work | ≤ 8 ms first target, revised from P14-S1 | | |
| audio | 0 dropouts in 5 minutes; 8 simultaneous sounds + 1 music track | | |
| load | ≤ 3 s title screen from the Memory Stick | | |
| heap | stable across the 5-minute run | | |

## 7. Measurements

| run | build | workload | metric | value | notes |
|---|---|---|---|---|---|
| baseline | | all textures | used / free / largest / failures | | |
| baseline | | all textures | draws / binds per frame | | |
| T1 | | all textures | used / free / largest / failures | | |
| T2 | | all textures | draws / binds, frame work | | |
| T3 | | title + HUD | capture diff | | |
| T4 | | 5-minute run | dropouts / audio memory | | |
| T5 | | cold start | load time, open count | | |
| T6 | | EBOOT | launch / capture parity | | |

## 8. Exit criteria

- [ ] §1 is met: zero allocation failures, zero dropouts, text parity, EBOOT launches
- [ ] the texture set is inside the recorded budget with its format/atlas decision written down
- [ ] the 5-minute run has no crash, no heap growth and no audio dropout
- [ ] P14-S1's two captures still match (no regression from asset work)
- [ ] the port's source, assets and EBOOT are still outside this repository
- [ ] new facts written back into `memory-map.md` §3/§9 if the port exposes a limit

## 9. Result

Open. This section is written when the sprint closes; it must contain the format/atlas table, the audio
numbers and the load-time row.

## 10. Follow-ups

- Deterministic input tape, 10-minute soak and attribution — P14-S3.
- Tate mode — optional after the phase closes.
- Asset archive/pack if the load time stays above budget — a port-side optimisation, not a library change.
