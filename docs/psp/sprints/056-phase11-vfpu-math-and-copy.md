# Sprint 056 (P11-S5) — VFPU math and bulk copy

| | |
|---|---|
| Roadmap phase | [Phase 11 — General optimisation outside the renderer](../phases/11-general-optimisation.md) |
| Sprint | P11-S5 |
| Branch / base commit | `master` @ `e307fe05` (docs; no Phase 11 code yet) |
| Started / closed | 2026-11-21 (written) / — |
| Build number(s) | pspgu `b24`; `libraylib.a` with `PSP_VFPU_MATH`/`PSP_VFPU_COPY`; numbers assigned when the sprint opens |
| Tasks | 0/6 done |

## 0. Handover

**Situation.** The PSP has a vector coprocessor with fast `sin`/`cos`/`atan2`/`pow` and a slow `sqrt`/`fabs`
relative to the FPU. `libpspmath` is installed (`$PSPDEV/psp/sdk/lib/libpspmath.a`, `pspmath.h`), but raylib
uses libm for everything. The VFPU's special functions are truncated, not rounded, so they are usable per
operation and dangerous for frame-accumulated state. This sprint measures each operation and the 64-byte
`lv.q`/`sv.q` copy at loader sizes, then adopts only the winners behind explicit switches.

**Read before acting**, in this order:

1. `../memory-map.md` §10.1 — the VFPU register file, accuracy table, quarter-turn angles, cycle costs, build
   traps and the accumulated-state rule; §9 — the `double` trap.
2. `../phases/11-general-optimisation.md` §0 finding 3 and the Daedalus cross-check — the cycle table and what
   does/does not transfer.
3. `../sprints/053-phase11-compiler-and-linker.md` — the chosen flags; VFPU results are only comparable at the
   same optimisation level.
4. `/home/psp/ref/daedalus/Source/SysPSP/Math/Math.h:19-38` — the source-authored cycle table;
   `Source/SysPSP/Utility/FastMemcpyPSP.cpp:25-140` — the 64-byte copy and its alignment/tail rules.
5. `$PSPDEV/psp/sdk/include/pspmath.h` and `pspvfpu.h` — the available `vfpu_*` entry points and the context
   API; `psp-nm $PSPDEV/psp/sdk/lib/libpspmath.a` — the symbols actually shipped.
6. `src/rcore.c` — `LoadFileData`, the font/image buffers and any per-frame `sin`/`cos`/`sqrt`/`atan2` calls;
   `src/rshapes.c` — the 2D rotation and collision paths.

**First action.** Build a microbenchmark that calls `sinf`, `cosf`, `atan2f`, `sqrtf`, `fabsf` and `powf`
against `vfpu_sinf`, `vfpu_cosf`, `vfpu_atan2f`, `vfpu_sqrtf`, `vfpu_fabsf` and `vfpu_powf` over fixed inputs,
with a host-generated golden table for accuracy. Run it before changing a library call; the device decides.

**Hard rules.**

- **Per operation, not per library.** Adopt `vfpu_sinf` only if it wins for the sizes/angles raylib uses; keep
  libm where the error or the call overhead loses. `sqrtf`/`fabsf` stay on the FPU unless the measurement
  contradicts the Daedalus comment.
- **No VFPU for frame-accumulated state.** Camera yaw, timers, physics and interpolation accumulate truncation
  bias; use integer/fixed or FPU `float` there. VFPU is for one-shot values (spawn, rotation, colour, fog).
- **Every VFPU thread carries the attribute.** A thread without `PSP_THREAD_ATTR_VFPU` corrupts its VFPU state
  across switches, and PPSSPP will not tell you. The main thread and any worker that calls these helpers need it.
- **Reserve matrices away from libgum.** `pspgum` uses matrices 0–3; helpers take 6–7. Call
  `pspvfpu_initcontext()`/`pspvfpu_use_matrices()` before issuing VFPU instructions.
- **No `__builtin_vfpu_*` or `__builtin_allegrex_*`.** psp-gcc 15.2.0 does not provide them; use `libpspmath`,
  `pspvfpu.h` or inline asm.
- **Accuracy is part of the result.** Record absolute/relative error and the input range where it holds. A speed
  win with an unreported error is not adopted.
- **The copy is loader-only.** Do not replace renderer vertex copies or texture upload with this helper; those
  are phases 4–6. The target is `LoadFileData`, font/image scratch buffers and the asset loader.
- **Stage only this sprint's paths.** `git status` before every commit; commit messages carry the task ID.

**Commands.**

```sh
export PSPDEV=/home/psp/pspdev
export PATH=$PSPDEV/bin:$PATH
set -e

cd /home/psp/raylib-psp
mkdir -p /home/psp/evidence/sprint056

# Microbench: one PRX, N iterations per function, golden table from the host.
( cd psp-tests/math_vfpu && make clean && make \
    && cp math_vfpu.prx /tmp/p11s6/app.prx )
( cd /tmp/p11s6
  timeout 20 pspsh -e "reset"; sleep 6
  ( printf './app.prx\n'; sleep 90 ) | timeout 110 pspsh )

# Copy bench: sizes 16 B .. 2 MiB, aligned and unaligned source; one row per size.
( cd psp-tests/copy_vfpu && make clean && make \
    && cp copy_vfpu.prx /tmp/p11s6/app.prx )
( cd /tmp/p11s6
  timeout 20 pspsh -e "reset"; sleep 6
  ( printf './app.prx\n'; sleep 90 ) | timeout 110 pspsh )

# Library arms: VFPU math/copy off and on, same optimisation level.
( cd src && make clean && make PLATFORM=PLATFORM_PSP -j8 PSP_VFPU_MATH=1 PSP_VFPU_COPY=1 \
    && cp libraylib.a $PSPDEV/psp/lib/libraylib.a )
```

**Do not touch.** `src/rlgl.h`, the batcher, `src/rtextures.c` upload paths, `tools/pspgu/`, `src/raudio.c` and
`src/rshapes.c`'s vertex generation. This sprint changes math/copy helpers, not draw paths.

**Done means** §8, with a per-operation table, an accuracy table, the copy table and the capture gate unchanged.

## 1. Objective

Five falsifiable results:

1. A per-operation table compares libm and `libpspmath` for `sin`, `cos`, `atan2`, `sqrt`, `fabs` and `pow` on
   fixed inputs: time per call and absolute/relative error. The table names the winner for raylib's use.
2. `sqrt`/`fabs` stay on the FPU unless the device says otherwise; the decision is recorded, not assumed from
   the Daedalus comment.
3. The 64-byte `lv.q`/`sv.q` copy is compared with libc at 16 B, 64 B, 256 B, 4 KiB, 64 KiB, 512 KiB and 2 MiB,
   with aligned and 4-byte-phase sources; the chosen size threshold is explicit.
4. Every thread that executes VFPU has the VFPU attribute, and `pspgum`'s matrices are not clobbered; a
   stress run switches between the helper and a non-VFPU thread without corruption.
5. The capture gate and the 2D corpus are unchanged; a numerical smoke test covers camera and collision inputs.

## 2. Why now

The compiler flags (053) and the frame CPU (055) are settled; the remaining CPU lever is the vector unit. The
phase's exit criteria explicitly require per-operation VFPU decisions with accuracy, not a blanket "use VFPU".
This sprint produces those decisions and the bulk-copy threshold the loader (058) will use.

## 3. Scope

**In:** a new `psp-tests/math_vfpu/` microbenchmark and `psp-tests/copy_vfpu/` copy benchmark; a small
`psp_math`/`psp_copy` helper in `src/platforms/` behind `PSP_VFPU_MATH`/`PSP_VFPU_COPY`; the call sites in
`src/rcore.c`/`src/rshapes.c` that the measurement selects; the PRX thread attribute in the test boilerplate.

**Out:** renderer math (`BeginMode3D`, `rlOrtho`, `rlFrustum`, pspgu `gl_matrix.c`), texture-upload conversion
(phase 6/5), skinning (phase 10), and any change to `libm` itself.

## 4. Preconditions

- [ ] Sprints 052/053 are in place; the zone harness and the chosen flags are the baseline
- [ ] `libpspmath.a`, `libpspvfpu.a` and `pspmath.h` exist in `$PSPDEV/psp/sdk`
- [ ] The main-thread attribute in the test PRX is known; `THREAD_ATTR_USER` is the current default
- [ ] A host golden table can be generated for the accuracy test (no `double` math on the device)
- [ ] The full-feature build and the capture gate are available
- [ ] `git status` understood; only this sprint's paths will be staged

## 5. Tasks

Status vocabulary: `todo` · `doing` · `done` · `blocked` · `dropped`.

| ID | task | status | verification | evidence |
|---|---|---|---|---|
| T1 | **Math microbench**: libm vs `vfpu_*` for `sin`, `cos`, `atan2`, `sqrt`, `fabs`, `pow`; N iterations; microseconds/call and a host golden accuracy table | `todo` | each function has a time and an error row; the input range is stated | — |
| T2 | **Adopt per operation**: add `PSP_VFPU_MATH=0\|1`; route only the winning calls; keep `sqrt`/`fabs` on the FPU unless measured otherwise; no accumulated-state call | `todo` | the switch changes only the selected calls; the frame and capture gate are unchanged | — |
| T3 | **Bulk copy**: 64-byte `lv.q`/`sv.q` helper with `ulv.q`/tail/fallback rules; compare to libc at the sizes above; choose the threshold | `todo` | aligned and 4-byte-phase rows; the threshold is explicit; no renderer use | — |
| T4 | **VFPU discipline**: VFPU thread attribute on every caller; `pspvfpu_initcontext`/`use_matrices` 6–7; stress switch between VFPU and non-VFPU work | `todo` | no state corruption over a stress run; PPSSPP is not the evidence | — |
| T5 | **Div/mod audit**: static and dynamic integer division/modulo in hot loops; replace only with provably identical shifts/reciprocals | `todo` | each replacement has a proof and a timing row; rejected cases recorded | — |
| T6 | **FP exceptions**: `pspFpuSetEnable(0)` A/B; verify raylib does not rely on traps; numerical smoke and capture parity | `todo` | both arms run; no trap-dependent path; time row | — |

### T1 design

- Inputs: a fixed table of angles covering 0, ±π/2, ±π, small values, and the camera/collision ranges; for
  `pow`, a fixed base/exponent grid. Generate the golden values on the host at 32-bit precision and store them
  as `float` literals or a binary blob; do not compute a `double` reference on the PSP.
- Time: run each function N times (start at 100,000), measure with `sceKernelGetSystemTimeWide()`, report
  microseconds/call; if the loop is optimised away, use a volatile accumulator and record the code.
- Error: absolute and relative; report the maximum and the input where it occurs. The §10.1 table is the
  expectation, not the result; the device decides.
- Include the call overhead: a wrapper that calls `vfpu_sinf` is not the same as libm's `sinf` if the wrapper
  saves/restores VFPU state; measure the wrapper, not the leaf.

### T2 design

- Add a `psp_math.h` with `pspSin`, `pspCos`, `pspAtan2`, `pspSqrt`, `pspFabs`, `pspPow`; the switch selects
  the implementation. Keep the names distinct from libm so a grep can audit call sites.
- Route only the calls the table selects. A blanket `-Dsinf=vfpu_sinf` is forbidden: it changes the meaning of
  every call and makes the A/B unreadable.
- Do not use the VFPU for `CORE.Time`, camera yaw accumulation, particle velocity integration or collision
  state; the integer/frame path from 055 and the FPU `float` path stay.
- `pspSqrt` and `pspFabs` default to the FPU; record the measured reason even if the VFPU is faster on one
  input range, because the Daedalus comment says it is slower.

### T3 design

- Port the reference copy's rules, not its code: below 16 B or mismatched 4-byte phase, call `memcpy`; align
  the destination to 4 then 16; use four `lv.q`/`sv.q` per 64 B; use `ulv.q` when only the source is 4-byte
  aligned; finish with 16-byte and byte tails.
- Test sizes around the thresholds (15/16/17, 63/64/65, 4 KiB ± 1, 64 KiB ± 1, 2 MiB ± 1) and both alignments.
  The threshold is the size where the helper stops winning, not a guess.
- Keep the helper in the loader path (`LoadFileData`, font/image scratch), not in `rlgl` vertex copies or
  texture upload. Those have their own owners.

### T4 design

- The test PRX uses `PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU)` (or the `PSP_` aliases the SDK
  provides). A worker thread that calls a VFPU helper gets the same attribute at creation.
- `pspvfpu_initcontext()` once per thread; `pspvfpu_use_matrices()` with 6–7, leaving 0–3 to pspgum. A stress
  loop alternates a VFPU calculation with a non-VFPU function and checks the result against the golden table.
- The main thread currently has `THREAD_ATTR_USER` only in the samples; adding VFPU is a deliberate change. If
  a sample does not call VFPU, it does not need the attribute; document which ones do.

### T5 design

- Find division/modulo with `psp-objdump` (`div`, `divu`, `rem`, `remu`) and the S1 counters if they exist; the
  PSP has no hardware integer divide, so a hot loop can pay a call. Replace only where the divisor is a power of
  two and the operands are provably non-negative (or the semantics are preserved for negatives).
- Record the rejected cases: a `%` on a signed value is not a shift, and a change that alters rounding is a bug.

### T6 design

- `pspFpuSetEnable(0)` is a bitmask of exceptions to enable; zero disables them. Put it behind
  `PSP_FPU_EXCEPTIONS=0|1` in `InitPlatform()` or the PRX entry, and A/B the frame.
- Verify raylib does not rely on FP traps: the numerical smoke test from 053 plus a deliberate NaN/Inf path
  must not hang or fault differently. If a trap is load-bearing, record it and reject the arm.

### Task log (append-only)

- 2026-11-21 — sprint written; all tasks `todo`, no device run

## 6. Budgets

| budget | limit | measured | tag |
|---|---|---|---|
| math | adopt only a measured win; record error | | |
| copy | helper wins above the chosen threshold; below it, libc | | |
| frame | no regression; capture gate unchanged | | |
| correctness | camera/collision smoke and tier-A captures pass | | |
| VFPU state | no corruption across a stress run | | |

## 7. Measurements

Protocol: device only; one function/size per row; control → change → control. Same clock and flags as 053.

**Math:**

| function | implementation | input range | µs/call | max abs error | max rel error | adopted |
|---|---|---|---|---|---|---|
| `sin` | libm | | | | | |
| `sin` | `vfpu_sinf` | | | | | |
| `cos` | libm / VFPU | | | | | |
| `atan2` | libm / VFPU | | | | | |
| `sqrt` | libm / VFPU | | | | | |
| `fabs` | libm / VFPU | | | | | |
| `pow` | libm / VFPU | | | | | |

**Copy:**

| size | alignment | libc µs | VFPU µs | winner |
|---|---|---|---|---|
| 16 B | 16/16 | | | |
| 64 B | 16/16 | | | |
| 4 KiB | 16/16 | | | |
| 2 MiB | 16/16 | | | |
| 64 KiB | 4-byte src | | | |

**Frame and correctness:**

| run | build | workload | frame median | capture | notes |
|---|---|---|---|---|---|
| control | `PSP_VFPU_MATH=0` | bunnymark / mixed | | | |
| change | `=1` | same | | | |

## 8. Exit criteria

- [ ] every candidate function has a time and an accuracy row
- [ ] the adopted set is explicit and the rejected calls are recorded with the reason
- [ ] `sqrt`/`fabs` are on the FPU unless the measurement contradicts the reference, and the decision is written
- [ ] the copy threshold and the alignment/tail rules are explicit, and the loader uses them
- [ ] every VFPU thread has the attribute and the matrices are reserved away from pspgum
- [ ] the div/mod audit has proofs for the accepted changes and reasons for the rejected ones
- [ ] the FP-exception arm runs and the numerical smoke test passes
- [ ] the capture gate and the 2D corpus are unchanged
- [ ] the archive has `SHA256SUMS`

## 9. Result

Not started. Record the per-operation winners, the copy threshold, the VFPU discipline evidence, the div/mod
decisions and the FP-exception result. A function that loses to libm is a result; keep it in the table.

## 10. Follow-ups

- **Sprint 057** uses the copy helper for asset buffers and the heap work.
- **Sprint 058** uses the copy threshold in `LoadFileData` and the loader.
- Any VFPU matrix decision that belongs to pspgu is a phase 5/6 follow-up, not this sprint.
