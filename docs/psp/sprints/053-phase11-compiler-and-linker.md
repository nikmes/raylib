# Sprint 053 (P11-S2) — Compiler and linker flags

| | |
|---|---|
| Roadmap phase | [Phase 11 — General optimisation outside the renderer](../phases/11-general-optimisation.md) |
| Sprint | P11-S2 |
| Branch / base commit | `master` @ `e307fe05` (docs; no Phase 11 code yet) |
| Started / closed | 2026-11-21 (written) / — |
| Build number(s) | pspgu `b24`; `libraylib.a` per flag arm; numbers assigned when the sprint opens |
| Tasks | 0/6 done |

## 0. Handover

**Situation.** `-O2` is already the PSP default (`src/Makefile:597`) and the `-O2` pair is archived under
`/home/psp/evidence/sprint008`, but `-O3`, `-Os`, LTO, section GC, `-ffast-math` and `-G0` versus `-G8` are
unmeasured. The frame-time baselines are vblank multiples, so this sprint uses the zone metrics and the
no-vsync arm from sprint 052, not `GetFrameTime()`.

**Read before acting**, in this order:

1. `../memory-map.md` §6 — the measurement protocol; §10.1 — the `double`/FPU facts; §9 — the
   `-ffast-math` and `-O3` traps.
2. `../phases/11-general-optimisation.md` §0 findings 2 and 4 — the timing metric and the build paths.
3. `../sprints/052-phase11-measurement-harness.md` §6–§8 — the harness, the boot line and the baseline that
   this sprint re-takes first.
4. `src/Makefile:549-600` — the PSP block, `PSP_OPT`, `-G0`, `PSP_DISABLE_VSYNC`; `src/Makefile:379-430` —
   the common `CFLAGS` and `RELEASE` block.
5. `psp-tests/bunnymark/main.c` and `psp-tests/scene_2d/main.c` — the workloads; `psp-tests/bunnymark/psp_counters.c`
   — the shared counter harness.
6. `src/rcore.c:1587-1705` — `GetTime`, `GetFPS`, `GetFrameTime`, `WaitTime`; the double cost this sprint is
   **not** allowed to rewrite (that is 055).

**First action.** Re-run the sprint 052 baseline against the current tree and put its boot id and zone table in
§7. Then build the `-O3` library and run the same workloads; do not start with the flags you expect to win.

**Hard rules.**

- **One variable per arm.** Change one flag family at a time: optimisation level, then LTO/GC, then fast-math,
  then `-G0`. A combined arm cannot attribute anything.
- **Measure with the sprint 052 harness.** Frame time alone is a 16.683 ms quantum; use the update/draw/swap
  zones and the no-vsync arm for CPU effects.
- **Record `.text` twice.** The archive's `.text` is not the code the I-cache sees; record the linked PRX's
  `.text` as well. A smaller archive that links larger is not a win.
- **Correctness before speed.** `-ffast-math` changes NaN/Inf and reassociation; run the numerical smoke test
  before quoting a number. Every arm must pass `batch_check` and the five-sample/phase 13 tier-A captures.
- **Do not change the source to make a flag win.** This sprint changes flags, not algorithms. Algorithmic
  changes belong to 055–058.
- **Keep the full-feature build.** The flag sweep uses the timing build, but the correctness arm uses the
  full-feature build from 054/phase 9.
- **No pspgu change.** pspgu's `-Os` is its own build; this sprint only measures what the app links.
- **Stage only this sprint's paths.** `git status` before every commit; commit messages carry the task ID.

**Commands.**

```sh
export PSPDEV=/home/psp/pspdev
export PATH=$PSPDEV/bin:$PATH
set -e

cd /home/psp/raylib-psp
mkdir -p /tmp/p11s2 /home/psp/evidence/sprint053

# Each arm: build library with one variable, stage, relink the app, run with the
# sprint 052 zone/regression build, capture only after DONE.
arm() {                                          # $1 tag, $2 make vars
    ( cd src && make clean && make PLATFORM=PLATFORM_PSP -j8 $2 \
        && cp libraylib.a $PSPDEV/psp/lib/libraylib.a )
    ( cd psp-tests/bunnymark && make clean \
        && make PSP_LARGE_MEMORY=0 BUNNY_COUNT=5000 TARGET_FPS=0 PSP_PROFILE=1 \
        && cp bunnymark.prx /tmp/p11s2/app.prx )
    ( cd /tmp/p11s2
      timeout 20 pspsh -e "reset"; sleep 6
      ( printf './app.prx\n'; sleep 120 ) | timeout 140 pspsh
      pspsh -e "scrshot host0:/$1.raw" )
    psp-size "$PSPDEV/psp/lib/libraylib.a" > "/home/psp/evidence/sprint053/$1.lib.size"
    psp-size /tmp/p11s2/app.prx                     > "/home/psp/evidence/sprint053/$1.prx.size"
}

arm 053-O2   ""
arm 053-O3   "PSP_OPT=-O3"
arm 053-Os   "PSP_OPT=-Os"
arm 053-O2-G8 "PSP_OPT=-O2 PSP_G=8"             # the unconditional -G0 is PSP_G=0 after T5

# Rebuild the chosen default last and leave the tree on it.
```

**Do not touch.** `tools/pspgu/`, the renderer, any source file for speed, the public API, the heap or the file
I/O. This sprint changes build flags and records numbers.

**Done means** §8, with the winner in `src/Makefile`, the boot line reporting it, the size/time table and the
correctness captures archived.

## 1. Objective

Four falsifiable results:

1. `-O2`, `-O3` and `-Os` each have a zone-time and `.text` row for the CPU-bound and load-bound workloads;
   the winner is chosen from those rows, not from intuition about the I-cache.
2. LTO and `-ffunction-sections -fdata-sections -Wl,--gc-sections` each have a size/frame row; an arm that does
   not link or boot is recorded as rejected, not silently dropped.
3. `-ffast-math -fno-math-errno` either passes the numerical smoke test and the capture gate, or is rejected
   with the failing case named.
4. The current `-G0` and `-G8` each boot and have a size/time row; the winner is explicit in `src/Makefile` and
   in the boot line.

## 2. Why now

The phase's biggest measured lever so far is the compiler: `-O0` → `-O2` moved bunnymark N=1,000 from 33.367 to
16.683 ms. The remaining flag space is unmeasured, and the review found that the frame metric cannot resolve it.
This sprint is the first optimisation sprint and consumes the harness from 052. It advances the compiler and
linker half of the phase's exit criteria.

## 3. Scope

**In:** `src/Makefile` (flag switches and defaults), the sprint 052 boot line (to report the chosen flags),
`psp-tests/bunnymark/` and `psp-tests/scene_2d/` (arms and captures), and a numerical smoke test under
`psp-tests/`.

**Out:** source-level algorithm changes (055–058), feature stripping (054), VFPU math (056), heap/I/O work
(057–058). No renderer or driver change.

## 4. Preconditions

- [ ] Sprint 052 harness is in place and its baseline is re-taken against this tree
- [ ] The boot line reports `PSP_OPT`, config and switches
- [ ] `-O2` archives and their hashes are recorded; `SHA256SUMS` exists for sprint 052
- [ ] `PSP_DISABLE_VSYNC=1` library arm builds and runs
- [ ] `batch_check` and `basic_shapes` captures are archived as the correctness gate
- [ ] The full-feature build is available for correctness (phase 9/13 state recorded)
- [ ] `git status` understood; only this sprint's paths will be staged

## 5. Tasks

Status vocabulary: `todo` · `doing` · `done` · `blocked` · `dropped`.

| ID | task | status | verification | evidence |
|---|---|---|---|---|
| T1 | **`-O2` vs `-O3` vs `-Os`**: build three libraries; run bunnymark N=5,000/10,000 and `scene_2d` mixed on the CPU-bound workload with `PSP_DISABLE_VSYNC=1`; record zones and both `.text` sizes | `todo` | three interleaved arms; zone median/p95; PRX `.text`; no capture difference | — |
| T2 | **Load-bound arm**: repeat T1 on the load workload; record load/decode/upload zones, not only the steady frame | `todo` | the load zone moves independently of the frame zone | — |
| T3 | **LTO and section GC**: `-flto`, then `-ffunction-sections -fdata-sections -Wl,--gc-sections`; link, boot, run; record size/time | `todo` | each arm links and reaches the first frame; rejected arms named | — |
| T4 | **Fast-math smoke test**: `-ffast-math -fno-math-errno`; test NaN/Inf, `x != x`, comparisons, camera math, collision and random; then capture parity | `todo` | a written pass/fail per case; a failing case rejects the arm | — |
| T5 | **`-G0` vs `-G8`**: add an explicit small-data switch; build both; verify gp/crt0 and boot; record size/time | `todo` | both boot; `%gp_rel` appears only in the `-G8` objects; size/time rows | — |
| T6 | **Choose and write back**: update `src/Makefile` defaults, the boot line and the phase page; rebuild the chosen arm; archive; re-run the capture gate | `todo` | `make -Bn` shows the chosen flags; boot line agrees; gate unchanged | — |

### T1 design

- The three levels are separate libraries, staged one at a time. Use the same app source, the same
  `PSP_LARGE_MEMORY=0`, the same `TARGET_FPS=0` and the same clock.
- The CPU-bound workload is bunnymark N=5,000 (about 66 ms at `-O2`) and N=10,000 if it fits; this is above the
  60 fps quantum, so even a vsync arm shows work, but the no-vsync arm is still the primary.
- Record median, p95, min, max and count; the decision uses median and p95, not the mean. A 0.1 % threshold is
  a floor for accepting a difference, not a claim that two runs can resolve it.
- `.text` is recorded from `psp-size` for the archive and for the linked PRX. Record `data`/`bss` too, because a
  flag can move bytes between sections.

### T2 design

- Use the sprint 052 load workload: open, read, decode, upload, then a steady frame. The load-bound arm is where
  `-Os` can win by shrinking the decoder; the frame arm is where `-O3` can win by vectorising or unrolling.
- Do not mix the load and frame numbers in one total. The decision may differ by workload; if it does, choose
  the default for the shipped build and record the per-workload recommendation.

### T3 design

- LTO is a link-time change; add `-flto` to compile and link. If pspdev's `psp-prxgen` or the linker rejects the
  objects, record the error and drop the arm rather than disabling the toolchain's hardening silently.
- Section GC needs both the compile flags and the link flag; a compile-only arm is not the test. Check that the
  default font, default shader and pspgu symbols survive; a dead-stripped symbol that only shows at runtime is a
  reject.
- Compare `.text` and the PRX size; GC often shrinks the archive without changing the fetched hot path.

### T4 design

- The smoke test is a separate `psp-tests/math_smoke` with fixed inputs: `NAN`, `INFINITY`, `-0.0`, a comparison
  chain (`<`, `<=`, `==`, `!=`), a camera projection/unprojection round trip, a circle/rectangle collision case,
  and a random sequence with a fixed seed.
- Run it once with the normal flags and once with fast-math; the outputs must be identical or the difference
  must be understood and accepted in writing. NaN/Inf behaviour is the usual failure.
- Then run the capture gate. Fast-math is accepted only if both the smoke test and the pixels pass.

### T5 design

- `psp-gcc`'s default is gp-relative small data (`%gp_rel`); the tree currently passes `-G0`. Confirm with a
  two-line compile probe and with the linked objects, not by reading the Makefile comment.
- Replace the unconditional `CUSTOM_CFLAGS += -G0` with `PSP_G ?= 0` and `CUSTOM_CFLAGS += -G$(PSP_G)`, so the
  arm is one variable. Record whether the PSP crt0's gp is valid for library code; if `-G8` faults, the comment
  is confirmed and the arm is rejected.
- The size/time row decides; the current `-G0` is not assumed to be optimal.

### T6 design

- Update `PSP_OPT` and the other flags in `src/Makefile`; do not scatter `-O3` into app Makefiles. The boot line
  must show the chosen values.
- Rebuild the chosen arm from clean, stage it, and re-run `batch_check`, `basic_shapes` and the tier-A captures.
- Archive the chosen library, the rejected arms and the table under `/home/psp/evidence/sprint053/`.

### Task log (append-only)

- 2026-11-21 — sprint written; all tasks `todo`, no device run

## 6. Budgets

| budget | limit | measured | tag |
|---|---|---|---|
| frame | no regression against the sprint 052 baseline | | |
| load | no regression; decode/upload separated | | |
| `.text` | record archive and PRX; no unreported growth | | |
| correctness | `batch_check`, `basic_shapes`, tier-A captures unchanged | | |
| clock | same for every arm; 333 MHz only if 059 chose it | | |

## 7. Measurements

Protocol: device only; one variable per arm; interleaved control/change/control. Every row carries the boot id.

| arm | `PSP_OPT` / flags | workload | zone median | zone p95 | archive `.text` | PRX `.text` | capture |
|---|---|---|---|---|---|---|---|
| control | `-O2` | bunnymark 5,000 | | | | | |
| change | `-O3` | bunnymark 5,000 | | | | | |
| change | `-Os` | bunnymark 5,000 | | | | | |
| control again | `-O2` | bunnymark 5,000 | | | | | |
| load control | `-O2` | load path | | | | | |
| load change | `-Os` | load path | | | | | |
| LTO | `-flto` | bunnymark 5,000 | | | | | |
| GC | sections + gc | bunnymark 5,000 | | | | | |
| fast-math | `-ffast-math` | smoke + bunnymark | | | | | |
| small data | `-G8` | bunnymark 5,000 | | | | | |

**Decision row:**

| chosen default | reason | rejected arms | evidence path |
|---|---|---|---|
| | | | |

## 8. Exit criteria

- [ ] the `-O2`/`-O3`/`-Os` decision has zone and `.text` rows for both workloads
- [ ] LTO and section GC are each measured or rejected with the link/boot error recorded
- [ ] fast-math has a written numerical result and a capture result; it is accepted only if both pass
- [ ] `-G0` and `-G8` are both measured and the winner is explicit in `src/Makefile`
- [ ] the boot line reports the chosen flags
- [ ] the correctness gate is unchanged
- [ ] the chosen and rejected arms are archived with `SHA256SUMS`
- [ ] the phase page §0 findings are updated if a flag contradicts them

## 9. Result

Not started. Record the winning arm, the losing arms and why, the numerical smoke result for fast-math, and any
flag that did not link or boot. If the winner differs by workload, say so and state the shipped default.

## 10. Follow-ups

- **Sprint 054** strips features; its size baseline is the chosen flag arm from this sprint.
- **Sprint 055** removes the `double` timing sites; if `-O3` changes their cost, re-run the relevant rows then.
- Any flag that is rejected for correctness belongs in the phase traps, not only in this page.
