# Sprint 055 (P11-S4) — Frame CPU: doubles, formatting and logging

| | |
|---|---|
| Roadmap phase | [Phase 11 — General optimisation outside the renderer](../phases/11-general-optimisation.md) |
| Sprint | P11-S4 |
| Branch / base commit | `master` @ `e307fe05` (docs; no Phase 11 code yet) |
| Started / closed | 2026-11-21 (written) / — |
| Build number(s) | pspgu `b24`; `libraylib.a` with the integer frame clock; numbers assigned when the sprint opens |
| Tasks | 0/7 done |

## 0. Handover

**Situation.** The current 2D draw path is already `double`-free (`rshapes` and `rlgl` have zero soft-float
relocation sites), but every frame still executes `rcore`'s `double` timing: `BeginDrawing` 2 sites,
`EndDrawing` 7, `WaitTime` 9, `GetFPS` 6 and `GetFrameTime` 1. `GetTime()` itself converts `clock_gettime()` to
`double` on every call. `DrawFPS` and the samples also format text every frame. This sprint moves the PSP frame
clock to integer nanoseconds, keeps the public `double` API as a boundary, removes formatting and log writes
from the steady frame, and leaves input and matrix doubles to their owning phases.

**Read before acting**, in this order:

1. `../memory-map.md` §10.1 — why `double` is a libgcc call and why frame-accumulated state must not move to
   VFPU; §6 — the measurement protocol.
2. `../phases/11-general-optimisation.md` §0 finding 3 — the 949-site breakdown and the cross-phase handoff.
3. `../sprints/052-phase11-measurement-harness.md` §5 T3 — the `--wrap` counter that this sprint turns into a
   guard.
4. `src/rcore.c:380-400` — `CoreData.Time`; `:881-930` — `BeginDrawing`/`EndDrawing`; `:1587-1705` —
   `SetTargetFPS`, `GetFPS`, `GetFrameTime`, `WaitTime`.
5. `src/platforms/rcore_psp.c:341-352` — `GetTime()`; `:563-583` — `CustomLog()`; `:740-765` — the boot line.
6. `src/rtext.c` — `TextFormat`; `src/rcore.c` — `DrawFPS`; `src/config.h:378-382` — the `TRACELOG` macro.
7. `psp-tests/bunnymark/main.c` and `psp-tests/scene_2d/main.c` — the `TextFormat`/`DrawFPS` calls in the frame
   loop.

**First action.** Build the static map before changing a line: run `psp-objdump -dr src/libraylib.a`, group the
`__*df*` relocations by object and function, and put the `rcore` rows in §7. Then add the S1 counter to the same
run and confirm the dynamic count per steady frame matches the static map's hot subset.

**Hard rules.**

- **Integer time only on the PSP frame path; the public API stays.** `GetTime()` still returns `double`; it is
  the boundary conversion, not a reason to keep doubles in `BeginDrawing`/`EndDrawing`.
- **Do not move frame-accumulated state to VFPU.** The truncation bias in §10.1 applies; integer nanoseconds are
  the answer, not VFPU.
- **Do not "fix" input or matrix doubles.** `ProcessGestureEvent` (26 sites) belongs to phase 8;
  `BeginMode3D`/`rlOrtho`/`rlFrustum` and pspgu `gl_matrix.c` belong to phases 10/5. Record them in the handoff
  list; the guard uses an explicit allowlist, not a silent exclusion.
- **A log line is evidence or it is not there.** Keep warnings/errors in the timing build; remove INFO/DEBUG with
  a compile-time level. Keep exactly one boot line, printed outside `TRACELOG`.
- **No renderer change.** `rlgl.h`, `rshapes.c`, `rtextures.c`, `rtext.c` and `tools/pspgu/` are untouched
  except for `rtext.c`'s `TextFormat` implementation if T4 requires it.
- **The guard must be able to fail.** A counter that cannot report a non-zero value is not a guard. Prove it by
  injecting one `malloc` and one `TraceLog` in a test build and seeing the guard fire.
- **Stage only this sprint's paths.** `git status` before every commit; commit messages carry the task ID.

**Commands.**

```sh
export PSPDEV=/home/psp/pspdev
export PATH=$PSPDEV/bin:$PATH
set -e

cd /home/psp/raylib-psp
mkdir -p /home/psp/evidence/sprint055

# Static map: per-object and per-function double-helper sites.
psp-objdump -dr src/libraylib.a 2>/dev/null \
  | awk '/^[a-z_]+\.o: +file format/{obj=$1}
         /^[0-9a-f]+ <[^>]+>:/{fn=$0; sub(/^[0-9a-f]+ </,"",fn); sub(/>:.*/,"",fn)}
         /R_MIPS_(26|J26)/ && /__(add|sub|mul|div|fix|cmp|trunc|extend)[a-z]*df/{n[obj" "fn]++}
         END{for(k in n) print n[k], k}' | sort -rn > /home/psp/evidence/sprint055/static-doubles.txt

# Dynamic: S1 counters, PSP_PROFILE=1, no-vsync arm.
( cd src && make clean && make PLATFORM=PLATFORM_PSP -j8 PSP_DISABLE_VSYNC=1 \
    && cp libraylib.a $PSPDEV/psp/lib/libraylib.a )
( cd psp-tests/bunnymark && make clean \
    && make PSP_LARGE_MEMORY=0 BUNNY_COUNT=1000 TARGET_FPS=0 PSP_PROFILE=1 \
    && cp bunnymark.prx /tmp/p11s4/app.prx )
( cd /tmp/p11s4
  timeout 20 pspsh -e "reset"; sleep 6
  ( printf './app.prx\n'; sleep 90 ) | timeout 110 pspsh
  pspsh -e "scrshot host0:/s055-before.raw" )

# After the change, the same command; the guard must report zero owned sites.
```

**Do not touch.** `src/rlgl.h`, `src/rshapes.c`, `src/rtextures.c`, `tools/pspgu/`, `src/raudio.c`, the public
API signatures, and the input/matrix code listed in the handoff.

**Done means** §8, with the before/after counter table, zero owned sites in the guard, one boot line and the
capture gate unchanged.

## 1. Objective

Five falsifiable results:

1. The PSP frame path uses integer nanoseconds: `BeginDrawing`, `EndDrawing`, `GetFPS`, `GetFrameTime` and
   `WaitTime` execute no `__*df*` helper in a steady frame; `GetTime()` is the only boundary conversion.
2. The S1 regression counter reports zero `double`-helper calls attributable to Phase 11-owned frame code;
   non-owned sites (phase 8 input, phase 10/5 matrix) are in a written allowlist.
3. Per-frame `TextFormat`/`sprintf` is removed where the result is cacheable; `DrawFPS` formats only when the
   value changes.
4. A timing build with `PSP_TRACELOG_MAX=LOG_WARNING` still prints the one boot line and no per-frame INFO/DEBUG;
   the cost of one `TraceLog` line over PSPLink is measured.
5. The tiny-copy audit names the per-frame `memcpy`/`memset`/`strlen`/`strcmp` calls and removes only the ones
   that pay; the capture gate is unchanged.

## 2. Why now

The compiler sprint (053) chose the code generator; this sprint removes work the generator cannot remove. The
timing doubles are the largest Phase 11-owned block in an otherwise double-free 2D frame, and they are executed
every frame. The formatting and logging costs are the same kind of invisible per-frame work the phase exists to
remove. It advances the "no `double`, allocation, file open or log write in Phase 11-owned code" exit criterion.

## 3. Scope

**In:** `src/rcore.c` (`CoreData.Time`, `BeginDrawing`, `EndDrawing`, `SetTargetFPS`, `GetFPS`,
`GetFrameTime`, `WaitTime`, `DrawFPS`), `src/platforms/rcore_psp.c` (`GetTime`, boot line), `src/config.h`
(the compile-time trace level), `src/rtext.c` only if `TextFormat` itself needs a cache, and the test samples'
per-frame formatting.

**Out:** phase 8 input/gesture code, phase 10 `BeginMode3D`/`rlOrtho`/`rlFrustum`, phase 5/pspgu matrix code,
the `double` load-path decoders (the load side is 057), and any renderer path.

## 4. Preconditions

- [ ] Sprint 052's counters and boot line are in place; sprint 053's chosen flags are the build default
- [ ] The static `double` map is captured in §7 before any change
- [ ] The no-vsync arm builds and runs
- [ ] The full-feature build and the capture gate are available
- [ ] `DrawFPS` and the samples' `TextFormat` calls are listed with their frame-loop line numbers
- [ ] A desktop/other-platform build check is possible, or the PSP-only branches are isolated and stated
- [ ] `git status` understood; only this sprint's paths will be staged

## 5. Tasks

Status vocabulary: `todo` · `doing` · `done` · `blocked` · `dropped`.

| ID | task | status | verification | evidence |
|---|---|---|---|---|
| T1 | **Static and dynamic map**: per-object/per-function `__*df*` table; classify hot/load/cold; match the S1 per-frame counters | `todo` | the 949 sites are accounted for; the hot subset is named; no site is silently omitted | — |
| T2 | **Integer frame clock**: add `nowNs`/`previousNs`/`updateNs`/`drawNs`/`frameNs`/`targetNs`; PSP branches of `BeginDrawing`, `EndDrawing`, `SetTargetFPS`, `GetFPS`, `GetFrameTime`, `WaitTime`; `GetTime` boundary only | `todo` | no `__*df*` in the owned frame path; frame time and target/fps behaviour unchanged within one frame; boot and run | — |
| T3 | **Handoff list**: phase 8 gestures, phase 10 matrix and pspgu matrix sites; add them to the guard's allowlist and the phase page | `todo` | the guard reports zero owned sites and a non-zero allowlisted count; the list is in the doc | — |
| T4 | **Formatting**: remove per-frame `TextFormat`/`sprintf`; cache `DrawFPS` output; update the samples where the string is constant | `todo` | the guard reports zero format calls in the steady frame; the pixels are unchanged | — |
| T5 | **Trace level**: compile-time `PSP_TRACELOG_MAX`; timing build at `LOG_WARNING`; one boot line outside; measure one line over PSPLink | `todo` | `SUPPORT_TRACELOG=0` still builds; the boot line prints; no per-frame INFO/DEBUG; cost row | — |
| T6 | **Tiny libc audit**: list per-frame `memcpy`/`memset`/`strlen`/`strcmp`; remove calls whose overhead exceeds the copy; record rejected cases | `todo` | each removal has a size/cost row; no semantic change | — |
| T7 | **Guards**: `psp_regress` asserts zero owned doubles/allocations/opens/logs in a steady frame; prove it fires with an injected call | `todo` | injected `malloc`/`TraceLog` makes it fail; clean run makes it pass | — |

### T1 design

- Use the same `psp-objdump` command as §0. The object split is already known: `rtextures` 443, `rmodels` 349,
  `rcore` 92, `rtext` 65; `rshapes` and `rlgl` 0.
- Inside `rcore`, the frame hot subset is `EndDrawing` 7, `WaitTime` 9, `GetFPS` 6, `BeginDrawing` 2,
  `GetTime`/`GetFrameTime` 1 each. `ProcessGestureEvent` 26 is phase 8; `BeginMode3D` 12 and `rlOrtho`/
  `rlFrustum` 6 + 6 are phase 10.
- The dynamic counter cannot attribute a call to a function without a return-address map; either use the static
  classification for ownership and the dynamic total for the guard, or add a debug-only caller map. Do not
  claim a per-function dynamic split that the harness cannot produce.

### T2 design

- Add integer fields to `CoreData.Time`:
  `unsigned long long nowNs, previousNs, updateNs, drawNs, frameNs, targetNs;`
  Keep the existing `double` fields for other platforms; the PSP branches write the integer fields and derive
  the public values.
- `pspNowNs()` uses `sceKernelGetSystemTimeWide()` (microseconds) × 1,000, or `clock_gettime` with integer
  nanoseconds. Prefer the syscall that was cheaper in the S1 zone overhead test; record which.
- `BeginDrawing`: `nowNs = pspNowNs(); updateNs = nowNs - previousNs; previousNs = nowNs;`.
- `EndDrawing`: compute `drawNs` and `frameNs` in integers; compare `frameNs < targetNs`; call a static
  `pspWaitNs(remainingNs)` helper for the wait. The public `WaitTime(double seconds)` keeps its signature and
  converts once at the boundary; the frame loop does not call it.
- `GetFrameTime` returns `frameNs * 1e-9f`; `GetFPS` uses integer deltas; `GetTime` returns
  `(double)nowNs * 1e-9` only when the public function is called.
- `SetTargetFPS`: `targetNs = (fps < 1)? 0 : 1000000000ULL/fps;` and keep the existing `target` double for
  other platforms.
- The public signatures do not change. A desktop build must still compile; if a shared branch cannot be
  isolated, use `#if defined(PLATFORM_PSP)` and state the desktop path is untouched.

### T3 design

- The handoff is a table in the phase page and in the guard source:
  `owner | function | sites | reason`.
  - Phase 8: `ProcessGestureEvent` 26.
  - Phase 10: `BeginMode3D` 12, `rlOrtho` 6, `rlFrustum` 6.
  - Phase 5/pspgu: `glOrtho` 12, `glFrustum` 14, `upload` 16 in `gl_matrix.c`.
- The guard counts all `__*df*` calls and subtracts the allowlisted totals only in the report; it must not
  silently drop them. A non-zero allowlisted count is visible every run.

### T4 design

- `DrawFPS` currently formats every frame. Cache the last `int fps` and rebuild the string only when it changes;
  the display still updates when the value changes.
- In the samples, hoist constant `TextFormat` results out of the loop; keep the bunnymark count string only if
  it actually changes, and cache it on count changes.
- `TextFormat` itself uses a static buffer and `vsnprintf`; do not replace it globally unless the measurement
  shows the format call is the cost. The target is per-frame calls, not the function.

### T5 design

- Add a compile-time level around `TRACELOG`:
  ```c
  #if !SUPPORT_TRACELOG
      #define TRACELOG(level, ...) (void)0
  #elif defined(PSP_TRACELOG_MAX)
      #define TRACELOG(level, ...) do { if ((level) <= (PSP_TRACELOG_MAX)) TraceLog(level, __VA_ARGS__); } while (0)
  #else
      #define TRACELOG(level, ...) TraceLog(level, __VA_ARGS__)
  #endif
  ```
  The `!SUPPORT_TRACELOG` branch comes first so a quiet build stays quiet even when the level is defined. Keep
  the warning/error levels in the timing build.
- The boot line is `printf`, not `TRACELOG`; it must survive both the level and `SUPPORT_TRACELOG=0`.
- Measure one line: a fixed `TraceLog(LOG_INFO, "S055 one line")` in a test build, timed with the S1 zone or a
  host-side timestamp; record the cost per line and the line rate.

### T6 design

- Search per-frame code for `memcpy`, `memset`, `strlen`, `strcmp` with small constant sizes; the PLT call plus
  the loop can exceed the copy. Replace only with a local inline copy/compare whose semantics are identical.
- Do not change the decoder/load-path copies here; they are large and belong to 057/058.

### T7 design

- The guard is a build of the S1 counter with a check at the end of the measured frames:
  `ownedDoubles == 0`, `allocations == 0`, `fileOpens == 0`, `logLines == 0` for the steady window.
- Prove it can fail: a `PSP_GUARD_INJECT=1` build adds one `malloc` and one `TraceLog`; the guard must report
  both and fail. Remove the hook from the default build.
- The guard reports the allowlisted non-owned counts separately; a zero total is not required.

### Task log (append-only)

- 2026-11-21 — sprint written; all tasks `todo`, no device run

## 6. Budgets

| budget | limit | measured | tag |
|---|---|---|---|
| steady frame | no regression; the owned double/format/log work is zero | | |
| frame/load | zone median/p95 against the 052 baseline | | |
| `.text` | the integer path does not grow the PRX materially; record it | | |
| correctness | `batch_check`, `basic_shapes`, tier-A captures unchanged | | |
| log cost | one line over PSPLink measured; timing build keeps warnings | | |

## 7. Measurements

Protocol: device only; one variable per run; control → change → control. Every row carries the boot id.

**Static map (before):**

| object | sites | hot subset | owner |
|---|---|---|---|
| `rtextures` | 443 | load | 058 |
| `rmodels` | 349 | load | 058 |
| `rcore` | 92 | timing, gestures, matrix | 055 / 058 / 010 |
| `rtext` | 65 | load | 058 |
| `rshapes` / `rlgl` | 0 | — | — |

**Dynamic frame:**

| run | build | workload | double calls | allocations | opens | log lines | frame median | capture |
|---|---|---|---|---|---|---|---|---|
| before | | bunnymark 1,000 | | | | | | |
| after | | bunnymark 1,000 | | | | | | |
| before | | `scene_2d` mixed | | | | | | |
| after | | `scene_2d` mixed | | | | | | |

**Trace cost:**

| build | line | cost | notes |
|---|---|---|---|
| `PSP_TRACELOG_MAX=LOG_WARNING` | one boot line | | |

## 8. Exit criteria

- [ ] the static map and the per-frame counters agree on the hot subset
- [ ] the PSP frame path has no `__*df*` helper outside the allowlist
- [ ] `GetTime()` is the only boundary conversion and is not called by the frame loop
- [ ] per-frame `TextFormat`/`sprintf` is gone from `DrawFPS` and the measured samples
- [ ] a timing build with INFO/DEBUG compiled out still prints the boot line and warnings/errors
- [ ] the guard can fail on an injected allocation/log and passes clean
- [ ] the capture gate and the 2D corpus are unchanged
- [ ] the handoff list is in the phase page and in the guard source
- [ ] the archive has `SHA256SUMS`

## 9. Result

Not started. Record the before/after counters, the integer clock's frame parity, the formatting and log savings,
the tiny-copy decisions and the injected-guard proof.

## 10. Follow-ups

- **Sprint 056** handles VFPU math and bulk copy; the integer time path is a precondition for its accuracy
  measurements.
- **Sprint 057** handles the load-path doubles (`rtextures`/`rmodels`/`rtext`) and the heap.
- **Sprint 058** handles the file/open side of the guard.
- Phase 8 owns `ProcessGestureEvent`; phase 10/5 owns the matrix sites. If either lands before this sprint,
  re-run the static map.
