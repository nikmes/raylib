# Sprint 082 (P14-S1) — Backend and first frame

| | |
|---|---|
| Roadmap phase | [Phase 14 — PSP Raylib Games](../phases/14-raylib-games/index.md) |
| Sprint | P14-S1 |
| Branch / base commit | `master` @ `7d495d16` |
| Started / closed | 2026-11-23 (written) / — |
| Build number(s) | first port build `galaga-b1`; driver from T1 (current pspgu b37 expected) |
| Tasks | 7/8 done — T5 physical-input confirmation open |

## 0. Handover

**Situation.** Phase 14 is open and the target is CppGalaga v3 at `/home/psp/ref/CppGalaga/Galaga_v3`
(commit `b1232a6`). 23 of the 24 non-backend `.cpp` files pass `psp-gcc 15.2.0 -std=gnu++14 -fsyntax-only`; the
24th, `src/infra/Game.cpp`, is an obsolete pre-split entry point that includes headers removed from v3 and is
deleted by the first action. The game's platform seam is six Renderer methods, a game entry point, six audio
functions and two keyboard functions. This sprint supplies a raylib implementation of the rendering, input and
loop parts and gets the title scene plus one stage frame onto the device; audio waits for P14-S2.

**Build recipe and memory are already verified.** A `/tmp` app using `std::string`, `std::map`, `std::list`,
`std::shared_ptr` and `std::function` built to a 2.64 MB PRX with an explicit `%.o: %.cpp` rule and
`-lstdc++ -lm`; PSPSDK's `build.mak` supplies no C++ rule and links with `psp-gcc`. The same test used
`PSP_HEAP_SIZE_KB(4096);` in the source, which is the macro that works — the Make variable of the same name is
not wired by `build.mak`. The auto heap leaves only ~768 KB, so T2 declares a heap before any asset load.

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
- Build the driver from a **clean worktree** at the chosen commit. The main tree currently carries uncommitted
  `tools/pspgu/` content and local edits to `tools/pspgu-build.sh`; building there measures that content, not
  b37. Record the driver hash from the boot line.
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

A `galaga.prx` launches on the PSP-1000 within a declared heap, loads every asset the title scene needs, draws
the title screen and a stage frame with correct sprite positions and 8-pixel text, accepts LEFT/RIGHT/FIRE
from the pad and nub, and runs 600 frames without a crash. Two captures of the same stopped frame are
byte-identical and archived with the build number. Audio is silent on purpose.

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
- [ ] The C++ recipe is available: explicit `%.o: %.cpp` rule and `-lstdc++` in `LIBS`
- [ ] `PSP_HEAP_SIZE_KB(n);` is declared in the port's main source (not only in the Makefile)
- [ ] `PSP_LARGE_MEMORY=0`; a build number is printed at boot and in every log line
- [ ] Two identical captures of a stopped frame are the acceptance rule (memory map §6)
- [ ] The user has authorised a device session

## 5. Tasks

Status vocabulary — `todo` · `doing` · `done` · `blocked` · `dropped`.

| ID | task | status | verification | evidence |
|---|---|---|---|---|
| T1 | **Decide display and driver.** Choose A (logical height 272, 1:1, centred at x=128) or C (tate); choose the current pspgu (b37 on `master`; build from a clean worktree and verify) or pspGL (repack every texture ≤512). Record both with the reason | `done` | decision table in §9; `pspgu-build.sh --check` output, driver hash, and a clean-worktree note | pspgu b37 + `PSPGU_TEXFMT=auto`; full-screen `STRETCH=1` requested by the user, `STRETCH=0` keeps 224×272 centred; pspgu built quiet for gameplay |
| T2 | **Scaffold and build.** Copy the core, add a C++ rule and Makefile, declare `PSP_HEAP_SIZE_KB(n);` in the port's main source, and produce `galaga.elf`/`.prx` | `done` | `make` completes; PRX size and `psp-nm` logged; boot log shows the declared heap and free bytes; no errors from port code | `galaga.prx` 3,171,722 B; `PSP_HEAP_SIZE_KB(16384)`; explicit `%.o: %.cpp`, `-lstdc++ -lm`, `-G0` |
| T3 | **Renderer.** Implement `setTranslation`, `drawPoint`, `drawRectangle`, `drawSprite(Sprite,dst,src,size)`, including the 540-wide `title_shine` path; convert the game's `0x00BBGGRR` integers to raylib `Color {r,g,b,255}` | `done` | title screen + stage frame show every expected sprite; a colour test (red/blue/green) matches the enum; sprite positions match the checklist in §7 | title and stage render full-screen; 27 textures; `title_shine` through the 512-window path |
| T4 | **Text.** Load PressStart2P at 8 px (or a pre-baked atlas) and emit each glyph at `x + 8*i` using `GetGlyphIndex`/`GetGlyphAtlasRec`; never fall back to raylib's 10 px font | `done` | HUD and message positions match the checklist; no "atlas exceeds 512" log; two identical captures | atlas 256×128, 8 px; HUD/messages aligned; two identical title captures |
| T5 | **Input.** Map D-pad and nub (`IsGamepadButtonDown`, `GetGamepadAxisMovement`) to LEFT/RIGHT, CROSS to FIRE, and keep `isKeyPressedOnce` semantics; add a non-gameplay exit (SELECT/START) that logs `DONE` and sleeps | `doing` | button-by-button device test; edge detection fires once per press; no press is lost while held | mapping implemented and the autoplay FIRE starts the stage; **physical D-pad/nub/CROSS test not yet recorded** |
| T6 | **Loop and runtime.** Fixed 1/60 s accumulator, `EndDrawing` pacing, no 3 ms sleep; replace `getchar(); exit()` in `ol::error`; route logs to `TraceLog` with a build number; count logical draws/text/points and read pspgu's `pspguVramStats`/`pspguResidencyStats` | `done` | 600 frames logged, frame counter and `DONE`; a forced asset miss logs an error and exits cleanly; the stats line prints | `STATS`/`CALLS` lines; `DONE frames=600 fps=60.0`; pspgu VRAM/residency counters |
| T7 | **Assets.** Load all title/stage PNGs from a staged root; report any missing file; keep the upstream `res/graphic` layout | `done` | zero load errors; every sprite used by the title and stage is present | 27 textures + font, 0 load errors, 0 VRAM failures |
| T8 | **Device run and close.** Title → stage, 600 frames, two identical captures, the T6 counters (logical draw/text/point calls, `pspguVramStats`/`pspguResidencyStats`, frame time), reset; write the result back | `done` | two identical captures; no crash; §7 rows filled; §9 result written | title 600 frames/60 fps, two byte-identical captures; stage 600 frames/60 fps; VRAM 1,766,912/330,240; 30 resident, 0 failures |

### Task log (append-only)

- 2026-11-23 — sprint opened, all tasks `todo`
- 2026-11-24 — T1–T4, T6–T8 `done`; T5 mapping implemented, physical test open

### Design notes

**T1 — the display math.** Physical screen is 480×272. Option A sets `SCREEN_HEIGHT = 272`, leaves
`SCREEN_WIDTH = 224`, calls `InitWindow(480, 272, "Galaga")`, and makes the backend add `(128, 0)` to every
logical coordinate. The constant change is not the whole change: the HUD's scale centre is hardcoded as
`(112.0f, 144.0f)` in `Hud.cpp:getScaledPosition` and must become `(112.0f, 136.0f)`; the lives text at
`Hud.cpp:145` (`y = 278`) must become `SCREEN_HEIGHT - 10`; the bottom symbol row at `StageSymbols.cpp:71` and
`Fighter.cpp:141` follow `SCREEN_HEIGHT`; `Path.cpp:156-157` recomputes the cell height. `Title.cpp`'s credits
end at y = 272 exactly (264 + 8), so the title screen has no bottom margin. Option C rotates all four primitive
types about the screen centre and requires the player to turn the console; it is not in scope for this sprint
beyond recording the decision.

**T1 — the driver build trap.** The installed `libGL.a` is currently pspGL, and the main tree has uncommitted
`tools/pspgu/` content plus local edits to `tools/pspgu-build.sh` (b33-era versus b37). Running the script in
that tree would build the local content, not the commit under test. Use a clean worktree at the chosen revision
(for example the existing `b37-gates` worktree or `git worktree add /tmp/pspgu-clean origin/master`), run
`tools/pspgu-build.sh --check` and the build there, and record the driver hash. This is the sprint-081 lesson:
a baseline on someone else's half-finished driver measures their build.

**T2 — the Makefile needs C++ rules, and the tested recipe is specific.** PSPSDK's `build.mak` defines
`CXX = psp-g++` but supplies no `.cpp` pattern rule and links with `$(LINK.c)` (psp-gcc). The verified recipe
adds an explicit `%.o: %.cpp` rule using `$(CXX) $(CXXFLAGS)` and appends `-lstdc++ -lm` to `LIBS`; linking
with `$(CXX)` instead is not wired into `build.mak`'s PRX rules. Add `-G0` to match the library's build (the
PSP crt0 sets gp for its own) and `-O2 -std=gnu++14`. `-fno-exceptions -fno-rtti` linked in the `/tmp` test
(the game uses neither), but confirm on the device before adopting them. Model the rest on
`psp-tests/basic_shapes/Makefile`: `BUILD_PRX = 1`, `PSP_LARGE_MEMORY = 0`, `EXTRA_TARGETS = EBOOT.PBP`, and
`LIBS = -lraylib -lpng -lz -lglut -lGLU -lGL -lpspgu -lpspvfpu -lstdc++ -lm`. The upstream also carries
`src/infra/Game.cpp`, an obsolete pre-split entry point that includes `infra/Display.hpp`, `infra/Renderer.hpp`
and `infra/AudioPlayer.hpp` (none exist); delete it from the port or exclude it, or the build fails on headers
that are not part of v3.

**T2 — the heap is a source macro.** Add `PSP_HEAP_SIZE_KB(n);` next to `PSP_MODULE_INFO` in the port's main
source, and include `<pspmoduleinfo.h>` (the `/tmp` test did both). The PSPSDK Make variable of the same name is
not consumed by `build.mak`; a Makefile-only setting is silently ignored. Start at 8192 KB for a port without
audio and raise it in P14-S2 from the measured audio budget. Log `entry free` after `main` starts and confirm it
matches the declaration.

**T3 — sprite calls and colour order.** `drawSprite(sprite, dest, srcPosition, srcSize)` maps to
`DrawTextureRec(tex, (Rectangle){src.x, src.y, size.x, size.y}, (Vector2){dest.x + offX, dest.y}, WHITE)`.
The game always draws at 1:1, so no rotation or scaling is needed. `drawRectangle` is an outline:
`DrawRectangleLines`. `drawPoint` is a 1×1 quad through the shapes texture and batches with its neighbours.
`title_shine` is 540 wide; if pspgu is installed the 512-window path handles it, otherwise split the sheet
into two textures and select by frame. The game stores colours as `0x00BBGGRR` (red in the low byte:
`RED = 255`, `BLUE = 16711680`); convert with `r = c & 255`, `g = (c >> 8) & 255`, `b = (c >> 16) & 255`,
alpha 255, matching `sdl2_impl/Renderer.cpp`. Do not feed the integer to raylib's `Color` or swap red/blue.

**T4 — the fixed advance, not the font's advance.** `GetGlyphIndex(font, c)` and `GetGlyphAtlasRec(font, c)`
are public; draw each glyph's source rectangle at `x + 8*i` with its own width/height. Every text coordinate in
`Hud.cpp`, `Title.cpp` and `GameController.cpp` assumes 8-pixel cells, so this is the layout contract, not a
style choice. `LoadFontEx` already sets `TEXTURE_FILTER_POINT`, so no filter change is needed. If the runtime
atlas is not crisp, pre-render the same 8 px atlas on the host. Do not call `DrawTextEx` with its natural
advance; `drawText2` is a one-line alias, as it is upstream.

**T5 — edge semantics.** The game owns `keysDown` and `keysDownConsumed`; the backend only writes `keysDown`
and clears `keysDownConsumed` on release. Read `IsGamepadButtonDown` and the nub once per frame in
`PollInputEvents` order. Use the calibrated nub from Phase 8 (`PSP_NUB_*`, radial deadzone 0.18) rather than a
raw 128-centre test.

**T6 — fixed step, no sleep.** Accumulate `GetFrameTime()` and run `sceneManager.update()` in 1/60 s steps,
then draw once. `EndDrawing` waits for vblank; the SDL backend's `sleep_for(3 ms)` is removed. A frame that
falls behind must catch up by bounded iterations, not spiral. Counters are split: the backend counts logical
`drawSprite`/`drawText`/`drawPoint`/`drawRectangle` calls; GPU state and VRAM come from pspgu's exported
`pspguVramStats`/`pspguResidencyStats` (declare them locally; a pspGL install lacks the symbols, so guard the
calls with the T1 driver choice). Do not add the `-Wl,--wrap` counter harness to a shipped app; that belongs
to `psp-tests`.

**T7 — asset root.** For PSPLink, serve the port directory as `host0:` and load `res/graphic/...`. For an
EBOOT, resolve the same relative paths from `ms0:/PSP/GAME/GALAGA/`. Put the root behind one constant so P14-S2
can change it without touching the game.

## 6. Budgets

| budget | limit | measured | tag |
|---|---|---|---|
| frame time | ≤ 16.7 ms at 60 fps | | |
| frame work | ≤ 8 ms first target, revised after the baseline | | |
| draw submissions | title and stage reported; no per-star or per-glyph submission | | |
| heap | declared, not auto; 8192 KB first target; stable across 600 frames | | |
| VRAM | must fit the 8888 set or the chosen 16-bit/repacked variant | | |
| build | 0 errors, 0 warnings from port code | | |

## 7. Measurements

Protocol: device only; one variable per run; control → change → control again. Every row carries its build
number. A screenshot without a build number is not evidence.

| run | build | workload | metric | value | notes |
|---|---|---|---|---|---|
| title | galaga-b1 | 600 frames | frame / fps | **60 fps**, work 1.1–1.5 ms | `DONE frames=600 fps=60.0` |
| title | galaga-b1 | 600 frames | logical calls | 2,397 sprites / 4,430 texts / 44,895 points | cumulative |
| stage | galaga-b1 | 600 frames | frame / fps | **60 fps**, work 1.2–1.5 ms | `START_SCENE=2` |
| boot | galaga-b1 | all assets | VRAM used / free / largest | **1,766,912 / 330,240 / 330,240 B** | 30 resident, 0 failures |
| boot | galaga-b1 | declared heap | partition free / max | **4,247,552 / 4,202,496 B** | 16 MiB heap |
| title | galaga-b1 | two captures | raw sha256 / diff | **identical / none** | full-screen stretch |

## 8. Exit criteria

- [ ] §1 is met: title and stage frame render, input works, 600 frames without a crash, two identical captures
- [ ] T1's display and driver decisions are recorded with their reasons
- [ ] no per-frame stall or wait introduced (check the counters, not the feel)
- [ ] the library's own gates still pass on the same install (`basic_shapes`, `batch_check`, `scene_2d`)
- [ ] new facts written back into `memory-map.md` §3/§9 if the port exposes a limit
- [ ] the port's source and assets are **not** in this repository

## 9. Result

**P14-S1 is implemented; one verification remains open.** The port builds as `galaga.prx`, loads 27 textures
plus the 8 px font, and runs the title and stage at 60 fps with the full-screen stretch. The title produces two
byte-identical captures. VRAM is 1,766,912 B used / 330,240 B free with 30 textures resident and 0 allocation
failures; the 16 MiB heap leaves 4.2 MB of partition free.

Two game defects were exposed and fixed in the port: `Hud::set1UpBlinking` (missing `return` in a `bool`
function) fell through into the next function and crashed the stage, and `Entity::commitZOrderChange` had the
same defect. The raylib colour macros also collide with the game's `ol::Color` enum, so the backend `#undef`s
them after `raylib.h`. The stale `src/infra/Game.cpp` was deleted before the first build.

**Open:** T5's physical D-pad/nub/CROSS confirmation. The mapping is implemented and the autoplay FIRE path
starts the stage, but no pass has recorded a human press of the three controls.

## 10. Follow-ups

- Audio backend, alias pools and music stop-previous semantics — P14-S2.
- Texture budget, repack/atlas, 8 px font bake and EBOOT packaging — P14-S2.
- Deterministic gameplay run, soak and attribution — P14-S3.
- Tate mode — optional, after Phase 14 closes; only if the 1:1 centred layout proves too small on the device.
- Any library defect found here becomes its own sprint; do not patch `src/` inside this port.
