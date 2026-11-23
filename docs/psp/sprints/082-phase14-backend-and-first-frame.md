# Sprint 082 (P14-S1) — Backend and first frame

| | |
|---|---|
| Roadmap phase | [Phase 14 — PSP Raylib Games](../phases/14-raylib-games/index.md) |
| Sprint | P14-S1 |
| Branch / base commit | `master` @ `7d495d16` |
| Started / closed | 2026-11-23 (written) / — |
| Build number(s) | first port build `galaga-b1`; driver from T1 (pspgu `b36` expected) |
| Tasks | 0/8 done |

## 0. Handover

**Situation.** Phase 14 is open and the target is CppGalaga v3 at `/home/psp/ref/CppGalaga/Galaga_v3`
(commit `b1232a6`). 23 of the 24 non-backend `.cpp` files pass `psp-gcc 15.2.0 -std=gnu++14 -fsyntax-only`; the
24th, `src/infra/Game.cpp`, is an obsolete pre-split entry point that includes headers removed from v3 and is
deleted by the first action. The game's platform seam is six Renderer methods, a game entry point, six audio
functions and two keyboard functions. This sprint supplies a raylib implementation of the rendering, input and
loop parts and gets the title scene plus one stage frame onto the device; audio waits for P14-S2.

**Read before acting**, in this order:

1. [Memory map](../memory-map.md) §2 (where the work happens), §3 (VRAM and hardware limits), §5 (this phase),
   §6 (measurement protocol) — especially `reset` before any memory or timing measurement and "never type `exit`
   into pspsh".
2. This phase page: [Phase 14](../phases/14-raylib-games/index.md) — read the target audit and the capability map.
3. The exact source: `/home/psp/ref/CppGalaga/Galaga_v3/include/infra/media_layer/Renderer.hpp`,
   `.../Game.hpp`, `.../AudioPlayer.hpp`, `.../Keyboard.hpp`; then `src/infra/sdl2_impl/*` as the reference
   backend and `src/infra/olc_pge_impl/Game.cpp` for the fixed-step loop.
4. `src/platforms/rcore_psp.c` for the PSP platform and batch boundary; `psp-tests/basic_shapes/` for the smallest
   working Makefile and `PSP_LARGE_MEMORY=0`.

**First action.**

```sh
PORT=/home/psp/galaga-psp
mkdir -p "$PORT"
cp -r /home/psp/ref/CppGalaga/Galaga_v3/include "$PORT/"
cp -r /home/psp/ref/CppGalaga/Galaga_v3/src     "$PORT/"
cp -r /home/psp/ref/CppGalaga/Galaga_v3/res     "$PORT/"
rm -rf "$PORT"/src/infra/{sdl2_impl,sfml_impl,olc_pge_impl}
rm -f  "$PORT"/src/infra/Game.cpp   # pre-split entry point; includes headers that no longer exist
mkdir -p "$PORT"/src/infra/raylib_impl
```

Then write the Makefile and `raylib_impl/{Game,Renderer}.cpp`. Do not edit the upstream clone.

**Hard rules.**

- Port code and assets live only in `/home/psp/galaga-psp`. Nothing from `res/` and no EBOOT may enter this
  repository: the upstream has no licence and the art/audio are Namco material.
- Record the display and driver decisions in §9 before drawing anything; later measurements are meaningless if
  the logical→physical mapping changes underneath them.
- Do not change raylib's public API or `tools/pspgu/` in this sprint. If the port exposes a library defect,
  stop and write a separate sprint; the port then links the fixed library.
- `PSP_LARGE_MEMORY=0` and `BUILD_PRX=1` in every build. The PSPSDK default asks for a 40 MB partition on a
  32 MB console.
- Device only for timing and memory. PPSSPP is for "does it draw" only.
- Keep the upstream clone read-only; if a source edit is genuinely required, copy the file into the port and
  record why in §9.

**Commands.**

```sh
export PSPDEV=/home/psp/pspdev
export PATH="$PATH:$PSPDEV/bin"

# driver state (read-only)
/home/psp/raylib-psp/tools/pspgu-build.sh --check

# build the port (after the Makefile exists)
cd /home/psp/galaga-psp && make

# device run: keep the shell attached, never send exit
pspsh -e "reset"
( printf './galaga.prx\n'; sleep 30 ) | pspsh
pspsh -e "scrshot host0:/galaga-title.raw"
pspsh -e "reset"
```

The app must log `DONE` and call `sceKernelSleepThread()`; it must not call `sceKernelExitGame`.

**Do not touch.** `src/`, `tools/`, `examples/`, `psp-tests/` and `docs/` except for the facts written back in
§9; the upstream clone; any other session's `tools/pspgu/` work.

**Done means** §8 with evidence in every task row.

## 1. Objective

A `galaga.prx` launches on the PSP-1000, loads every asset the title scene needs, draws the title screen and a
stage frame with correct sprite positions and 8-pixel text, accepts LEFT/RIGHT/FIRE from the pad and nub, and
runs 600 frames without a crash. Two captures of the same stopped frame are byte-identical and archived with
the build number. Audio is silent on purpose.

## 2. Why now

This is the phase's first exit criterion and the prerequisite for every later number: assets, audio and
gameplay all get measured through this backend. It also settles the two decisions that would otherwise be
retrofitted across every file: the 224×288 → 480×272 mapping and whether the target driver is pspgu or pspGL.

## 3. Scope

**In:** the port scaffold and Makefile under `/home/psp/galaga-psp`; `raylib_impl/Game.cpp` and
`raylib_impl/Renderer.cpp`; the 8-pixel text path; the pad/nub input map; the fixed-step loop; the `ol::error`
and logging adaptations; the title scene and first stage frame; asset staging under `host0:`.

**Out:** audio (P14-S2), texture repacking/atlas and VRAM fitting (P14-S2), EBOOT packaging (P14-S2), the
10-minute soak and performance attribution (P14-S3), tate mode (follow-up after the phase closes), high-score
persistence (the upstream does not persist it).

## 4. Preconditions

- [ ] `/home/psp/ref/CppGalaga` is at `b1232a6` and clean; the port directory does not yet exist
- [ ] `pspsh` answers, USB device is `054c:01c9`; `reset` done and partition 2 free reads 25,149,440 B
- [ ] `tools/pspgu-build.sh --check` recorded; T1 has chosen the driver
- [ ] `psp-gcc --version` is 15.2.0; the installed `libraylib.a` is the one the port will link
- [ ] `PSP_LARGE_MEMORY=0`; a build number is printed at boot and in every log line
- [ ] Two identical captures of a stopped frame are the acceptance rule (memory map §6)
- [ ] The user has authorised a device session

## 5. Tasks

Status vocabulary — `todo` · `doing` · `done` · `blocked` · `dropped`.

| ID | task | status | verification | evidence |
|---|---|---|---|---|
| T1 | **Decide display and driver.** Choose A (logical height 272, 1:1, centred at x=128) or C (tate); choose pspgu `b36` (install and verify) or pspGL (repack every texture ≤512). Record both with the reason | `todo` | decision table in §9; `pspgu-build.sh --check` output or a written pspGL constraint list | — |
| T2 | **Scaffold and build.** Copy the core, add a C++ rule and Makefile, produce `galaga.elf`/`.prx` | `todo` | `make` completes; `psp-nm`/size logged; no errors from port code | — |
| T3 | **Renderer.** Implement `setTranslation`, `drawPoint`, `drawRectangle`, `drawSprite(Sprite,dst,src,size)`, including the 540-wide `title_shine` path | `todo` | title screen + stage frame show every expected sprite; sprite positions match the checklist in §7 | — |
| T4 | **Text.** Load PressStart2P at 8 px (or a pre-baked atlas) and emit each glyph at `x + 8*i`; never fall back to raylib's 10 px font | `todo` | HUD and message positions match the checklist; no "atlas exceeds 512" log; two identical captures | — |
| T5 | **Input.** Map D-pad and nub to LEFT/RIGHT, CROSS to FIRE, and keep `isKeyPressedOnce` semantics; add a non-gameplay exit (SELECT/START) that logs `DONE` and sleeps | `todo` | button-by-button device test; edge detection fires once per press; no press is lost while held | — |
| T6 | **Loop and runtime.** Fixed 1/60 s accumulator, `EndDrawing` pacing, no 3 ms sleep; replace `getchar(); exit()` in `ol::error`; route logs to `TraceLog` with a build number | `todo` | 600 frames logged, frame counter and `DONE`; a forced asset miss logs an error and exits cleanly, not a hang | — |
| T7 | **Assets.** Load all title/stage PNGs from a staged root; report any missing file; keep the upstream `res/graphic` layout | `todo` | zero load errors; every sprite used by the title and stage is present | — |
| T8 | **Device run and close.** Title → stage, 600 frames, two identical captures, counters (draws, binds, vertices, VRAM), reset; write the result back | `todo` | two identical captures; no crash; §7 row filled; §9 result written | — |

### Design notes

**T1 — the display math.** Physical screen is 480×272. Option A sets `SCREEN_HEIGHT = 272`, leaves
`SCREEN_WIDTH = 224`, calls `InitWindow(480, 272, "Galaga")`, and makes the backend add `(128, 0)` to every
logical coordinate. Fixed positions that encode 288 must move: the lives text at `Hud.cpp:145` (`y = 278`), the
bottom symbol row at `StageSymbols.cpp:71`, `Fighter.cpp:141`, and the `Path.cpp` cell height. Option C rotates
all four primitive types about the screen centre and requires the player to turn the console; it is not in
scope for this sprint beyond recording the decision.

**T2 — the Makefile needs C++ rules.** PSPSDK's `build.mak` defines `CXX = psp-g++` but supplies no `.cpp`
pattern rule and links with `$(LINK.c)` (psp-gcc). Add an explicit `%.o: %.cpp` rule using `$(CXX) $(CXXFLAGS)`
and link with `$(CXX)` or add `-lstdc++` to `LIBS`. Model the rest on `psp-tests/basic_shapes/Makefile`:
`BUILD_PRX = 1`, `PSP_LARGE_MEMORY = 0`, `EXTRA_TARGETS = EBOOT.PBP`, and
`LIBS = -lraylib -lpng -lz -lglut -lGLU -lGL -lpspgu -lpspvfpu`. The upstream also carries
`src/infra/Game.cpp`, an obsolete pre-split entry point that includes `infra/Display.hpp`, `infra/Renderer.hpp`
and `infra/AudioPlayer.hpp` (none exist); delete it from the port or exclude it, or the build fails on headers
that are not part of v3.

**T3 — the sprite call.** `drawSprite(sprite, dest, srcPosition, srcSize)` maps to
`DrawTextureRec(tex, (Rectangle){src.x, src.y, size.x, size.y}, (Vector2){dest.x + offX, dest.y}, WHITE)`.
The game always draws at 1:1, so no rotation or scaling is needed. `drawRectangle` is an outline:
`DrawRectangleLines`. `drawPoint` is a 1×1 quad through the shapes texture and batches with its neighbours.
`title_shine` is 540 wide; if pspgu is installed the 512-window path handles it, otherwise split the sheet
into two textures and select by frame.

**T4 — the 8-pixel grid is contractual.** Every text coordinate in `Hud.cpp`, `Title.cpp` and
`GameController.cpp` assumes 8-pixel cells. Do not call `DrawTextEx` with its natural advance; iterate the
characters and draw each glyph's source rectangle at `x + 8*i`, with the glyph's own width and height. This
also makes `drawText2` a one-line alias, as it is upstream.

**T5 — edge semantics.** The game owns `keysDown` and `keysDownConsumed`; the backend only writes `keysDown`
and clears `keysDownConsumed` on release. Read `IsGamepadButtonDown` and the nub once per frame in
`PollInputEvents` order. Use the calibrated nub from Phase 8 (`PSP_NUB_*`, radial deadzone 0.18) rather than a
raw 128-centre test.

**T6 — fixed step, no sleep.** Accumulate `GetFrameTime()` and run `sceneManager.update()` in 1/60 s steps,
then draw once. `EndDrawing` waits for vblank; the SDL backend's `sleep_for(3 ms)` is removed. A frame that
falls behind must catch up by bounded iterations, not spiral.

**T7 — asset root.** For PSPLink, serve the port directory as `host0:` and load `res/graphic/...`. For an
EBOOT, resolve the same relative paths from `ms0:/PSP/GAME/GALAGA/`. Put the root behind one constant so P14-S2
can change it without touching the game.

## 6. Budgets

| budget | limit | measured | tag |
|---|---|---|---|
| frame time | ≤ 16.7 ms at 60 fps | | |
| frame work | ≤ 8 ms first target, revised after the baseline | | |
| draw submissions | title and stage reported; no per-star or per-glyph submission | | |
| heap | stable across 600 frames; no per-frame allocation growth | | |
| VRAM | must fit the 8888 set or the chosen 16-bit/repacked variant | | |
| build | 0 errors, 0 warnings from port code | | |

## 7. Measurements

Protocol: device only; one variable per run; control → change → control again. Every row carries its build
number. A screenshot without a build number is not evidence.

| run | build | workload | metric | value | notes |
|---|---|---|---|---|---|
| baseline | | title, 600 frames | frame median / p95 | | |
| baseline | | title, 600 frames | draws / binds / vertices | | |
| baseline | | boot | VRAM used / free / largest | | |
| baseline | | title + stage | load time, open count | | |

## 8. Exit criteria

- [ ] §1 is met: title and stage frame render, input works, 600 frames without a crash, two identical captures
- [ ] T1's display and driver decisions are recorded with their reasons
- [ ] no per-frame stall or wait introduced (check the counters, not the feel)
- [ ] the library's own gates still pass on the same install (`basic_shapes`, `batch_check`, `scene_2d`)
- [ ] new facts written back into `memory-map.md` §3/§9 if the port exposes a limit
- [ ] the port's source and assets are **not** in this repository

## 9. Result

Open. This section is written when the sprint closes; it must contain the decision table from T1, the first
frame's numbers, and what failed.

## 10. Follow-ups

- Audio backend, alias pools and music stop-previous semantics — P14-S2.
- Texture budget, repack/atlas, 8 px font bake and EBOOT packaging — P14-S2.
- Deterministic gameplay run, soak and attribution — P14-S3.
- Tate mode — optional, after Phase 14 closes; only if the 1:1 centred layout proves too small on the device.
- Any library defect found here becomes its own sprint; do not patch `src/` inside this port.
