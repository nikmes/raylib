# Sprint 083 (P14-S2) — Assets, VRAM and audio

| | |
|---|---|
| Roadmap phase | [Phase 14 — PSP Raylib Games](../phases/14-raylib-games/index.md) |
| Sprint | P14-S2 |
| Branch / base commit | `master` @ `7d495d16`; port at `/home/psp/galaga-psp` from P14-S1 |
| Started / closed | 2026-11-23 (written) / — |
| Build number(s) | `galaga-b2` onward; driver chosen in P14-S1 T1 |
| Tasks | 4/8 done — T5 load timing and T7 soak open; T6/EBOOT and T8 close remain |

## 0. Handover

**Situation.** P14-S1 put the game on the device: renderer, 8-pixel text, input and the fixed-step loop, with
the title scene and one stage frame. This sprint makes the full asset set fit, gives the game its audio, and
packages it so it launches without PSPLink. It starts from the P14-S1 build and a recorded texture-budget line;
it must not change the display decision from P14-S1 T1. **Audio is the second budget, not an afterthought:**
`LoadSound` decodes to f32 stereo, so the 7.0 MB WAV set is 27.5 MiB if every file is a Sound. T4 decides what
streams; T5 declares and measures the heap.

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
- Music is one stream at a time; stop the previous stream before starting the next, and clear `music.looping`
  (raylib sets it true).
- Declare `PSP_HEAP_SIZE_KB(n);` in the port's main source. The auto heap leaves ~768 KB; a Makefile-only
  setting is ignored by `build.mak`.
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
recorded VRAM and heap budgets; 8-pixel text is byte-identical across two captures; audio plays all 17 sounds
and 6 tracks with **zero dropouts** in a 5-minute run, with music and long clips streamed rather than decoded
wholesale; and the packaged EBOOT launches from the Memory Stick without PSPLink and reaches the title screen
in ≤ 3 s.

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
- [ ] `PSP_LARGE_MEMORY=0`; `PSP_HEAP_SIZE_KB(n);` declared in the port's main source; EBOOT title and asset
      root chosen
- [ ] The user has authorised a device session

## 5. Tasks

Status vocabulary — `todo` · `doing` · `done` · `blocked` · `dropped`.

| ID | task | status | verification | evidence |
|---|---|---|---|---|
| T1 | **Budget and format.** Record the current padded texture set, free VRAM and the declared heap; choose 16-bit `--texfmt=auto`, a repack, or both; confirm zero texture allocation failures | `done` | boot line: used/free/largest; 27 textures loaded; 0 failures; declared heap / entry free | 4444/5650 auto; 1,766,912/330,240 B; 30 resident, **0 failures**; heap 16 MiB, partition free 4.25 MB |
| T2 | **Repack and split.** Split `title_shine` (540×36) into ≤512 pieces; optionally pack the sprite sheets into one atlas. Measure draws/binds before and after | `dropped` | every sprite and frame renders; draw/bind delta recorded; adopt only if it wins | Not needed: pspgu's 512-window path renders `title_shine`; auto fits with 330 KB free and 0 failures |
| T3 | **Text parity.** If the runtime 8 px TTF is unstable, bake the atlas offline; keep the fixed 8-pixel advance | `done` | HUD/message checklist; atlas ≤512; no default-font fallback; two identical captures | atlas 256×128 at 8 px; fixed 8 px advance; title captures identical |
| T4 | **Audio backend and memory plan.** Classify the 17 sound clips and 6 music tracks by decoded size; load short SFX as `Sound` with alias pools, stream music and (if needed) long clips as `Music` streams; set `looping = false`, stop-previous, one `UpdateMusicStream` per frame | `done` | every enum plays; repeated shots overlap; a track stops the previous; peak heap and open-file count recorded; no blocking call in the frame | 17 Sounds + 6 Music on 16 MiB; commands queued to a 0x30 control thread; 512-frame blocks; worker 0x20; **0 underruns** in the 30 s stage arm |
| T5 | **Startup and load.** Measure the open count and time to first frame with the chosen load plan; stage or pack assets; no black screen longer than the measured load | `doing` | load time, open count, heap high-water in §7; no missing-file log | assets staged under `host0:/galaga/`; 0 missing-file logs; timing not yet recorded |
| T6 | **EBOOT packaging.** PARAM.SFO, title, asset root under `ms0:/PSP/GAME/<TITLE>/`; verify beside-EBOOT paths | `todo` | EBOOT launches without PSPLink; title appears; same captures as the PRX run | — |
| T7 | **Five-minute run.** Title → stage → game over with audio; log frame time, dropouts, heap and VRAM high-water | `doing` | 0 dropouts; 0 allocation failures; no crash; p95 frame time recorded | 10-minute soak running; 0 underruns to t=30 s; autoplay stress reached 33–37 fps at t=55–70 s |
| T8 | **Close and decide.** Record which format/atlas/packaging arms were adopted; update the phase page and §9 | `todo` | decision table in §9; phase page's state row updated | — |

### Task log (append-only)

- 2026-11-23 — sprint opened, all tasks `todo`
- 2026-11-24 — T1/T3/T4 `done`; T2 `dropped` (windows + auto fit); T5/T7 `doing`; T6/T8 `todo`

### Design notes

**T1 — the arithmetic to confirm.** The phase page's padded total is **908,544 B** at 8888 against
**913,344 B free**, before the font — a set that cannot coexist with an atlas. At 16 bits the same set is
**454,272 B**; pspgu's `auto` chooses 5650 for opaque RGB and 4444 for RGBA whose alpha is all 0/255, otherwise
8888. Check the actual per-texture choice in the boot log; do not assume every sprite is 4444.

**T2 — what an atlas buys.** The 13 animation sheets are 384×16 each, padded to 512×16: 425,984 B and thirteen
distinct texture ids. A single 512×512 4444 atlas (524,288 B) costs about 100 KiB more but turns every sprite
into one bind and one run, and removes the width padding. It requires a 16-bit format (`--texfmt=auto` or an
explicitly converted asset); the same atlas at 8888 is 1,048,576 B and does not fit. Adopt it only if the
draw/bind counters and the frame work show a win; the alternative (16-bit per texture, no atlas) is a valid
close.

**T2 — `title_shine`.** 540 wide, 15 frames of 36×36. pspgu's 512-window path can sample it from a 1024×64
buffer (262,144 B at 8888), but that is 29 % of the whole budget for one title-screen effect. Split it into two
pieces or pack its 15 frames; do not leave a 1024-wide texture in the set.

**T3 — the font.** PressStart2P is SIL OFL and is the font the layout was designed around. `LoadFontEx` at 8 px
produces a small atlas; the library rejects anything over 512 in either dimension (`src/rtext.c:576-581`), so
check the boot log. If the rasterised glyphs are not crisp on the GE, pre-render the same atlas on the host
and load it as a bitmap font; the advance stays 8 px either way.

**T4 — audio memory is the decision.** `LoadSound` decodes every file to the device format, f32 stereo at
44.1 kHz (`src/raudio.c:1016-1024`), i.e. 8 bytes per source frame. The 23 files fall into three groups:

| group | files | decoded as Sound | plan |
|---|---:|---:|---|
| short SFX (≤ ~2 s) | 10 | **2.3 MiB** | load as `Sound`; alias pools for `FIGHTER_SHOT`/explosions |
| long SFX (> ~2 s) | 7 | **9.5 MiB** | load as `Sound` only if the heap allows; otherwise stream as `Music` or trim |
| music | 6 | **15.7 MiB** | always stream; do not decode as `Sound` |
| **total** | **23** | **27.5 MiB** | |

The long group is `fighter_captured`, `formation_scaled`, `fighter_explosion`, `enemy_beam`,
`enemy_beam_fighter_captured`, `fighter_rescued`, `enemy_diving`. Load the short group as `Sound`, stream the six
music tracks, and try the long group as `Sound` on a declared 16 MB heap; if an allocation fails, move the long
group to streams or trim the tails. Streaming long clips costs open file handles: six music streams plus seven
long clips exceeds the ~8-open-file budget, so reuse a small pool of stream slots instead of holding all of them.
`LoadMusicStream` sets `music.looping = true`; clear it for jingles. `LoadSound` peaks at the source WAV plus the
decoded buffer, so load the largest clip first and check the high-water mark.

**T4 — aliases and stop semantics.** `PlaySound` calls `PlayAudioBuffer` on the sound's single buffer and
restarts it (`src/raudio.c:714`). `LoadSoundAlias` allocates only a buffer header (`sizeInFrames == 0`) and shares
the sample data, so a pool of aliases is cheap. `stopSound(ENEMY_BEAM)` must stop every alias/stream mapped to
that enum, not just the first. `playMusic` must `StopMusicStream` the current track first, then
`PlayMusicStream` the new one; pump `UpdateMusicStream` for every active stream once per frame. Phase 9's budget
is 8 sounds + 1 streamed track at 60 fps with 0 dropouts.

**T5 — the open cost.** 27 PNG plus the short SFX is ~37 one-shot opens; streamed music and long clips stay
open, so their count is bounded by the ~8-file limit. At ~17 ms per Memory Stick open the one-shot part is
~0.6 s of pure latency, plus decode. Measure it; if it exceeds 3 s, pack the assets into one archive or move
the load behind a progress screen. Do not add a per-frame allocator to hide it.

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
| audio memory | short SFX as Sounds; music and long clips streamed; peak heap recorded | | |
| load | ≤ 3 s title screen from the Memory Stick; open files ≤ ~8 | | |
| heap | declared heap; stable across the 5-minute run; no allocation failure | | |

## 7. Measurements

| run | build | workload | metric | value | notes |
|---|---|---|---|---|---|
| baseline | galaga-b1 | all textures | used / free / largest / failures | **1,766,912 / 330,240 / 330,240 / 0** | 30 resident |
| T4 | galaga-b1 | 17 Sounds + 6 Music | partition free / open files | **4.25 MB free**; music files streamed | 16 MiB heap |
| T4 | galaga-b1 | 30 s stage | audio dropouts | **0** | 512-frame blocks, worker 0x20 |
| T4 | galaga-b1 | title, 5 min | audio dropouts | 4 at startup (before the queue/priority fixes) | superseded |
| T7 | galaga-b1 | 10-minute autoplay | fps / dropouts | **33–37 fps at t=55–70 s; 0 underruns so far** | soak running |
| T6 | — | EBOOT | launch / capture parity | — | not tested |

## 8. Exit criteria

- [ ] §1 is met: zero allocation failures, zero dropouts, text parity, EBOOT launches
- [ ] the texture set is inside the recorded budget and the audio set is inside the declared heap, with both
      format/streaming decisions written down
- [ ] the 5-minute run has no crash, no heap growth and no audio dropout
- [ ] P14-S1's two captures still match (no regression from asset work)
- [ ] the port's source, assets and EBOOT are still outside this repository
- [ ] new facts written back into `memory-map.md` §3/§9 if the port exposes a limit

## 9. Result

**P14-S2 is mostly implemented; packaging and the long-run performance gate remain.** The texture set fits at
4444/5650 with 330 KB free and 0 allocation failures, so the planned repack was dropped. The 8 px font is
stable. Audio uses 17 decoded Sounds and 6 streamed Music tracks on a 16 MiB heap; playback commands are queued
to a control thread, the DSP block is 512 frames, and the worker runs at the game thread's priority. The 30 s
stage arm recorded **0 underruns**.

The 10-minute autoplay soak is running. It has 0 underruns so far, but the autoplay stress drops to 33–37 fps
after about 55 s; the main-thread work at that point is 6.9–8.5 ms, so the remaining time is scheduling/audio
interaction rather than draw submission. EBOOT packaging and a final five-minute run are still open.

## 10. Follow-ups

- Deterministic input tape, 10-minute soak and attribution — P14-S3.
- Tate mode — optional after the phase closes.
- Asset archive/pack if the load time stays above budget — a port-side optimisation, not a library change.
