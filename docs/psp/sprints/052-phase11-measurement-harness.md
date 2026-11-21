# Sprint 052 (P11-S1) — Measurement harness and re-baseline

| | |
|---|---|
| Roadmap phase | [Phase 11 — General optimisation outside the renderer](../phases/11-general-optimisation.md) |
| Sprint | P11-S1 |
| Branch / base commit | `master` @ `e307fe05` (docs; no Phase 11 code yet) |
| Started / closed | 2026-11-21 (written) / — |
| Build number(s) | pspgu `b24`; `libraylib.a` builds with the new profile/regression switches; app numbers assigned when the sprint opens |
| Tasks | 0/7 done |

## 0. Handover

**Situation.** Phase 11 has no harness that can attribute a change. The phase 2/3 counters count GL calls, and
`scene_2d`'s `SPLIT` line separates total work from swap, but neither separates load, update, draw and list build,
and the boot line does not identify the build or its flags. This sprint adds the instrumentation every later
sprint depends on and changes no optimisation.

**Read before acting**, in this order:

1. `../memory-map.md` §6 — the measurement protocol; §3 — the heap and partition numbers; §9 — the traps;
   §10.1 — why VFPU and `double` are separate problems.
2. `../phases/11-general-optimisation.md` §0 — the review findings this sprint closes first, and the exit
   criteria it will eventually serve.
3. `../sprints/013-phase5-corpus-and-counters.md` §5 — the existing `SPLIT work/swap` counter and the
   `PSP_DISABLE_VSYNC` arm.
4. `psp-tests/bunnymark/main.c:130-235` — the frame loop and `STAT`/`SPLIT` logging; `psp-tests/bunnymark/psp_counters.c`
   and `psp_counters.h` — the shared `--wrap` harness and swap timer.
5. `psp-tests/scene_2d/main.c:980-1045` — the corpus timing block; `psp-tests/scene_2d/Makefile:120` — the rule
   that borrows `psp_counters.o` from `bunnymark`.
6. `src/platforms/rcore_psp.c:341-352` — `GetTime()`; `:563-583` — `CustomLog()`; `:740-765` — the end of
   `InitPlatform()`; `src/rcore.c:881-930` and `:1587-1705` — `BeginDrawing`/`EndDrawing`, `GetTime`, `GetFPS`
   and `WaitTime`.
7. `src/Makefile:549-600` — the PSP build block (`PSP_OPT`, `PSP_DISABLE_VSYNC`, `PSP_BATCH`, `PSP_CULL`);
   `tools/pspgu/egl.c:629` — the swap path.

**First action.** Add `psp-tests/bunnymark/psp_profile.c` and `psp_profile.h` (borrowed by `scene_2d` the same way
`psp_counters.o` is), with `pspNow()` over `sceKernelGetSystemTimeWide()` and a fixed 1,024-sample median/p95
report. Instrument `bunnymark`'s update, `BeginDrawing`/`EndDrawing` and the load block, build against the
**current** library, and capture the first frame table before touching `src/`.

**Hard rules.**

- **This sprint optimises nothing.** A `src/` change is allowed only for the boot line and for a counter hook.
  If a task would change a draw path, a texture, a heap call or a flag that affects code generation, it belongs
  to a later sprint.
- **Device only.** PPSSPP answers "does it render", never "how fast" or "how much". `reset` before every memory
  or timing run; `kill` leaks the module heap.
- **One variable per run.** For every timing claim use control → change → control, or interleaved arms in one
  session. One run per arm separates nothing when variance is 10–30 %.
- **Zone time is not `GetTime()`.** `GetTime()` returns `double` and is one of the things this phase will remove;
  zones use `sceKernelGetSystemTimeWide()` (microseconds) and integer arithmetic.
- **`--wrap` must call `__real_*`.** A counter that shadows a libc/libgcc symbol without forwarding it changes the
  program under measurement.
- **The boot line is the one line that survives a quiet build.** Print it with `printf`, not `TRACELOG`, so
  `SUPPORT_TRACELOG=0` cannot remove the evidence.
- **No pspgu change.** The driver is `b24`; `tools/pspgu/` is read-only for this sprint.
- **Stage only this sprint's paths.** Other sessions edit the tree; `git status` before every commit, never
  `git add -A`. Commit messages carry the task ID: `sprint 052 T2: ...`.

**Commands.**

```sh
export PSPDEV=/home/psp/pspdev
export PATH=$PSPDEV/bin:$PATH
set -e

cd /home/psp/raylib-psp
tools/pspgu-build.sh --check                    # b24, content hash recorded

# Build one library arm, stage it, relink the app, reset, run, capture.
build_lib() {                                   # $1 = extra make vars
    ( cd src && make clean && make PLATFORM=PLATFORM_PSP -j8 $1 \
        && cp libraylib.a $PSPDEV/psp/lib/libraylib.a )
}
run_app() {                                     # $1 = app dir, $2 = tag, $3 = make vars
    ( cd "$1" && make clean && make $3 && cp *.prx /tmp/p11s1/app.prx )
    ( cd /tmp/p11s1
      timeout 20 pspsh -e "reset"; sleep 6
      ( printf './app.prx\n'; sleep 90 ) | timeout 110 pspsh
      pspsh -e "scrshot host0:/$2.raw" )        # after DONE only
}

mkdir -p /tmp/p11s1 /home/psp/evidence/sprint052

# Baseline: vsync on, current library, bunnymark and the 2D corpus.
build_lib ""
run_app /home/psp/raylib-psp/psp-tests/bunnymark s052-bunny-vsync \
        "PSP_LARGE_MEMORY=0 BUNNY_COUNT=1000 TARGET_FPS=0"

# No-vsync arm: the library, not the app, carries PSP_DISABLE_VSYNC.
build_lib "PSP_DISABLE_VSYNC=1"
run_app /home/psp/raylib-psp/psp-tests/bunnymark s052-bunny-novsync \
        "PSP_LARGE_MEMORY=0 BUNNY_COUNT=1000 TARGET_FPS=0"

# Load path: the phase 13 texture sample, or a minimal load_path harness.
run_app /home/psp/raylib-psp/psp-tests/samples/textures/image_loading \
        s052-load "PSP_LARGE_MEMORY=0"
```

**Do not touch.** `tools/pspgu/`, `src/rlgl.h`, the batcher, `rshapes.c`, `rtextures.c`, `rtext.c`, `rmodels.c`,
`raudio.c`, and every compiled default flag. This sprint only observes.

**Done means** §8, with the boot line, the zone table, the counter output and the archived hashes.

## 1. Objective

Six falsifiable results:

1. Every run prints one boot line identifying the raylib version, git id, `PSP_OPT`, config flags, pspgu build and
   the PSP switches (`PSP_BATCH`, `PSP_CULL`, `PSP_TRI_FAST`, `PSP_DISABLE_VSYNC`).
2. Update, load, draw and swap/GE-wait are timed as separate zones, each reported with median and p95 over the
   measured frames; the residual between their sum and the frame total is stated.
3. The regression counters report double-helper calls, `malloc`/`free`/`calloc`/`realloc`, file opens/reads and
   log writes per steady frame, with zero being a value the harness can assert.
4. The same workload runs with and without `PSP_DISABLE_VSYNC`; the frame total is no longer quantised to
   16.683 ms steps in the no-vsync arm.
5. A load workload decodes at least one image or font and opens at least one file; its load time is reported
   separately from the steady frame.
6. The baseline tables and the archive hashes are in §7 and `/home/psp/evidence/sprint052/SHA256SUMS`.

## 2. Why now

The phase's first rule is "measured, or it is not an optimisation". The review found that every existing frame
number is a vblank multiple and that the boot line cannot identify the build, so the phase cannot begin with a
flag sweep. This sprint produces the metric that sprints 053–059 will use and the baseline they will be compared
against. It advances the phase's observability exit criteria and unblocks every other sprint.

It can be written and run before phases 8–10 close, but the §7 baseline must be re-taken after those phases land;
until then it is a harness baseline, not the final one.

## 3. Scope

**In:** `src/Makefile` (profile/regression switches and the generated build header), `src/platforms/rcore_psp.c`
(the one boot line and the zone hook), `src/rcore.c` (zone hooks only, no timing rewrite), `psp-tests/bunnymark/`
(`psp_profile.*`, `psp_regress.*` and the frame loop), `psp-tests/scene_2d/` (the borrowed profile object and
the load/zone reporting), and a minimal `psp-tests/load_path/` if no phase 13 sample is ready.

**Out:** any optimisation of flags, doubles, allocation, I/O, startup or feature stripping. Those are 053–059.
No renderer, pspgu, public API or default behaviour change.

## 4. Preconditions

- [ ] Phase 11 page reviewed; §0 findings read
- [ ] `pspsh` answers; USB device is `054c:01c9`; `usbhostfs_pc` serves `/tmp/p11s1` as `host0`
- [ ] `reset` done; partition 2 free reads 25,149,440 bytes before the first run
- [ ] `src/libraylib.a` and `pspgu` build from the current tree; `tools/pspgu-build.sh --check` reports `b24`
- [ ] `bunnymark` and `scene_2d` run 600 frames (the regression control)
- [ ] `batch_check` capture is archived (`b69830b8…` for the current gate) so a later change is attributable
- [ ] `PSP_LARGE_MEMORY=0` in every sample build
- [ ] `git status` is understood; only this sprint's paths will be staged

## 5. Tasks

Status vocabulary: `todo` · `doing` · `done` · `blocked` · `dropped`.

| ID | task | status | verification | evidence |
|---|---|---|---|---|
| T1 | **Build identity**: generate `src/psp_build.h` from `src/Makefile` with `PSP_BUILD_ID` (`git describe --always --dirty`), `PSP_BUILD_OPT`, `PSP_BUILD_FLAGS`, `PSP_BUILD_CONFIG`, `PSP_BUILD_PSPGU`; include it in `rcore_psp.c`; print one `BUILD ...` line with `printf` | `todo` | change `PSP_OPT` and the line changes; `SUPPORT_TRACELOG=0` still prints it | — |
| T2 | **CPU zones**: `psp_profile.*` over `sceKernelGetSystemTimeWide()`; zones for load, update, draw, swap/GE wait and list build; median/p95 at `DONE`; `PSP_PROFILE=0\|1`; overhead measured | `todo` | zones sum to the frame within the residual; `=0` costs no calls; overhead < 0.05 ms/frame | — |
| T3 | **Regression counters**: `psp_regress.*` with `--wrap` for the `__*df*` libgcc helpers, `malloc`/`free`/`calloc`/`realloc`, `fopen`/`fread`/`fclose`, `sceIoOpen`/`sceIoRead`/`sceIoClose`, `printf`/`vprintf`; per-frame counters and a `DONE` line | `todo` | a known allocation and a `TextFormat` call are counted; forwarding works (`__real_*`) | — |
| T4 | **No-vsync arm**: build the library with and without `PSP_DISABLE_VSYNC=1`; run the same workload; record the frame distribution | `todo` | vsync arm is a multiple of 16.683 ms; no-vsync arm is not; swap zone falls (sprint 013: 9.4 → 2.433 ms) | — |
| T5 | **Load workload**: use a phase 13 texture sample or add `psp-tests/load_path` that opens a file, decodes a PNG and loads a TTF; report the load zone separately | `todo` | the log shows file opens, decode time and the first presented frame | — |
| T6 | **Baseline profile**: bunnymark N=1,000/5,000 and `scene_2d` text/tiles/particles/shapes/mixed, with and without vsync; budget table with update/draw/swap/residual | `todo` | every row carries the boot id and build; control/change/control variance recorded | — |
| T7 | **Archive and write-back**: `SHA256SUMS` for libraries, PRX files, logs and captures; update memory-map §3/§9 only with facts this sprint measured | `todo` | `sha256sum -c SHA256SUMS` passes; the memory-map diff is factual | — |

### T1 design

- Generate the header, do not hand-edit it. `src/psp_build.h` is gitignored and rebuilt when the make variables
  change. It contains string literals only; no logic.
- The boot line is printed once from `InitPlatform()` after `InitTimer()`:
  `BUILD id=<git> opt=<PSP_OPT> config=<flags|default> pspgu=<content hash> psp=<switches>`.
- Use `printf`, not `TRACELOG`; the quiet timing build must still identify itself. Keep it to one line.
- The pspgu hash is read from `tools/pspgu-build.sh --check` at build time and baked into the header; if the
  script is unavailable, print `unknown` rather than a wrong value.

### T2 design

- `pspNow()` returns `unsigned long long` microseconds from `sceKernelGetSystemTimeWide()`; no `double` on the
  measurement path.
- A zone stores count, min, max, sum and a fixed 1,024-sample ring; report median and p95, never the mean alone.
  At `DONE`, call `PspProfileDump()` alongside `PspCountersDump()` and the existing `STAT`/`SPLIT` lines.
- Zone boundaries: before/after the load block, the game update, `BeginDrawing()..EndDrawing()` and
  `SwapScreenBuffer()`. "List build" is measured in the driver's existing split counters or around the batch
  flush; do not change pspgu to add it.
- Measure the instrumentation cost by running the same build with `PSP_PROFILE=0` and `=1`; report the delta.
  A harness that costs more than 0.05 ms/frame is rejected.

### T3 design

- Start from `psp-tests/bunnymark/psp_counters.c`; add a second object, `psp_regress.c`, rather than growing the
  GL counter file.
- Wrap every entry in this list: `__adddf3`, `__subdf3`, `__muldf3`, `__divdf3`, `__negdf2`, `__fixdfsi`,
  `__fixunsdfsi`, `__truncdfsf2`, `__extendsfdf2`, `__cmpdf2`, `__nedf2`, `__ltdf2`, `__gtdf2`; `malloc`,
  `free`, `calloc`, `realloc`; `fopen`, `fclose`, `fread`, `fwrite`; `sceIoOpen`, `sceIoRead`, `sceIoClose`;
  `printf`, `vprintf`, `fputs`. Add a wrapped symbol only if the link reports it used.
- Count calls and bytes where meaningful. Reset at the start of the measured frames, not at process start, so
  the load path and the steady frame are separate rows.
- The sample Makefiles add the `-Wl,--wrap=...` flags once, next to the existing GL wraps. A missing `__real_*`
  is a link error, not a silent zero.

### T4 design

- `PSP_DISABLE_VSYNC` is compiled into `libraylib.a`/`libGL.a`, not the app. Build two libraries and relink the
  same app source; record both. Do not compare a no-vsync library against a vsync app or the reverse.
- `TARGET_FPS=0` in the app. The vsync arm should reproduce the 16.683/33.367/50.051 ms steps; the no-vsync arm
  should expose the work distribution, with swap still around 2.4 ms.
- Keep the same clock and the same workload between arms. If the no-vsync arm changes the frame rate enough to
  change the workload (for example particle counts), say so and use a fixed-work workload.

### T5 design

- Prefer a phase 13 tier-A texture sample: it already loads a PNG from the host and exercises `LoadImage` /
  `LoadTextureFromImage`. If none is ready, add `psp-tests/load_path` with `LoadImage`, `LoadTextureFromImage`,
  `LoadFontEx` and one `LoadFileData`, all timed before the first `BeginDrawing()`.
- The load row is one-shot: file open, read, decode, upload. The steady row is the frame after it. Do not put a
  decode inside the frame loop.

### T6 design

- Use `PSP_PROFILE=1` and the `psp_regress` counters in the same build. The frame table has one row per workload
  and one per vsync arm; the load table has one row per asset class.
- Report median, p95, min, max and count. A budget row without a residual is incomplete: `residual = frame total
  - (update + draw + swap)`.
- The baseline is not the phase's final baseline until phases 7–10 are closed; mark it `harness baseline` in
  §7 and re-run it at the start of 053.

### Task log (append-only)

- 2026-11-21 — sprint written; all tasks `todo`, no device run

## 6. Budgets

| budget | limit | measured | tag |
|---|---|---|---|
| zone instrumentation overhead | ≤ 0.05 ms/frame at `PSP_PROFILE=1` | | |
| frame table | median + p95 for every workload | | |
| load table | file open, read, decode and upload separated | | |
| boot line | exactly one, survives `SUPPORT_TRACELOG=0` | | |
| archive | `SHA256SUMS` for every library and PRX row | | |

## 7. Measurements

Protocol: device only; one variable per run; control → change → control. Every row carries the boot id.

| run | build | workload | zone | median | p95 | residual | notes |
|---|---|---|---|---|---|---|---|
| control | | bunnymark N=1,000, vsync | | | | | |
| change | | bunnymark N=1,000, no-vsync | | | | | |
| control again | | bunnymark N=1,000, vsync | | | | | |
| load | | texture sample | open/read/decode/upload | | | | |

**Harness baseline (to be filled):**

| workload | build | frames | update | draw | swap | residual | counters |
|---|---|---|---|---|---|---|---|
| | | | | | | | |

## 8. Exit criteria

- [ ] the boot line identifies build, flags, config and pspgu on every run
- [ ] update, load, draw and swap/GE wait are separate zones with median and p95
- [ ] the residual is reported for every frame row
- [ ] the counters count doubles, allocations, file opens/reads and log writes; a known test call is visible
- [ ] the no-vsync arm is not quantised to 16.683 ms steps
- [ ] the load workload exists and its phases are separated
- [ ] the harness baseline and `SHA256SUMS` are archived
- [ ] `batch_check` and `basic_shapes` are byte-identical to their controls; no renderer file changed
- [ ] `PSP_PROFILE=0` and `=1` differ only in instrumentation

## 9. Result

Not started. Fill this in with the boot line, the first frame/load tables, the instrumentation cost and the
archive path. Record what failed, including any workload that could not be separated.

## 10. Follow-ups

- **Sprint 053 (P11-S2)** consumes this harness for compiler and linker A/B.
- **Sprint 055 (P11-S4)** replaces the `GetTime()` sites that the zone work deliberately did not touch.
- If a phase 13 sample is used for the load path, record its sample name and the phase 13 sprint that provided it.
- If `--wrap` cannot intercept a libgcc helper on this toolchain, record the alternative (static `objdump`
  count) here and carry it to 055.
