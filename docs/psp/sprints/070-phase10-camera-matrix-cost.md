# Sprint 070 (P10-S8) — Camera and matrix CPU cost: measure before GUM

| | |
|---|---|
| Roadmap phase | [Phase 10 — 3D and advanced](../phases/10-3d-advanced/index.md) |
| Sprint | P10-S8 |
| Branch / base commit | `master` @ `9957a5b2` (P10-S7 and P10-S9 written; pin at start) |
| Started / closed | 2026-11-22 (written) / — |
| Build number(s) | arm `a` = measurement only: `libraylib.a` default `PSP_BATCH=1`, pspgu `b30` (`2ea2da7a…`), no renderer change. arm `b` only if GUM lands |
| Tasks | 0/5 done |

## 0. Handover

**Situation.** The question is whether the PSP `pspgum` matrix library can accelerate raylib's
`Camera2D`/`Camera3D`. The code says the camera builders run **once per frame** (`BeginMode2D` /
`BeginMode3D`), while `GetWorldToScreen2D`/`GetWorldToScreenEx` and per-object model transforms run **per
call**. pspgu already keeps the GL matrix stack on the CPU, multiplies a matrix once per mode change, and
uploads only when it changed; the GE does the vertex transform. So the plausible cost is per-object/per-call
math, not the camera. This sprint measures that before any GUM code.

**Read before acting**, in this order:

1. `src/rcore.c:938` `BeginMode2D`, `:959` `BeginMode3D`, `:1487` `GetCameraMatrix2D`, `:1523`
   `GetWorldToScreenEx`, `:1565` `GetWorldToScreen2D`.
2. `src/rcamera.h:419` `GetCameraViewMatrix`, `:425` `GetCameraProjectionMatrix`, `:445` `UpdateCamera`.
3. `tools/pspgu/gl_matrix.c` — the GL stack, `glMultMatrixf` (`:168`), the upload-on-change
   (`upload`, `:58`) and `toGuMatrix` (`:51`, the layout trap: GL column-major is copied straight into
   `ScePspFMatrix4`); the upload is `sceGuSetMatrix`, skipped when the composed matrix is unchanged.
4. `../memory-map.md` §10.1/§14 — GUM default vs VFPU, no VFPU intrinsics, thread attribute and context
   rules. The libraries are `$PSPDEV/psp/sdk/lib/libpspgum.a` and `libpspgum_vfpu.a`, the header
   `$PSPDEV/psp/sdk/include/pspgum.h` (both present, verified 2026-11-23); the API is `gumLookAt`,
   `gumPerspective`, `gumOrtho`, `gumMultMatrix`.
5. `../sprints/061-phase10-camera-relative-and-ge-native-models.md` §9/§T5 — the camera-relative result was
   **null** because pspgu composes view×model on the CPU; do not repeat that experiment.

**First action.** Build `psp-tests/matrix_probe` (it does not exist yet) and capture the **before** numbers:
10,000 calls each of `GetCameraMatrix2D`, `GetWorldToScreen2D`, `GetScreenToWorld2D`, `GetWorldToScreen`,
`GetWorldToScreenEx`, `MatrixLookAt`, `MatrixPerspective`, `MatrixOrtho`, `MatrixMultiply`,
`Vector3Transform`, `QuaternionTransform` and `UpdateCamera(CAMERA_CUSTOM)`, plus the cached-vs-rebuild loops.
No library or renderer change before the rows exist.

**Hard rules.**

- **Measure on the PSP-1000 only**, with `sceKernelGetSystemTimeWide()` (µs). Report p95, not one sample.
- **Do not add GUM until a threshold is crossed.** T4 is conditional: default GUM only if T1–T3 show the
  matrix work above **0.1 ms/frame or 1 % of the frame** at the target scene.
- **Never use `sceGum*` stack APIs.** They would fight pspgu's GL stack. Use only `gumLookAt`,
  `gumPerspective`, `gumOrtho`, `gumMultMatrix` and convert to raylib's `Matrix`.
- **Do not enable `libpspgum_vfpu` yet.** It needs `PSP_THREAD_ATTR_VFPU` plus
  `pspvfpu_initcontext()`/`pspvfpu_use_matrices()` on every calling thread; that is a project-wide change
  and PPSSPP will not catch state corruption. Default GUM must win first.
- **The GE transforms vertices.** Do not attribute draw submission or clip time to matrix maths.
- **Keep the public camera API and desktop behaviour.** Any cache is app-side/test-side; any library branch
  is `#ifdef PLATFORM_PSP` and must produce byte-identical captures.
- **pspgu first.** `tools/pspgu-build.sh` then `--check` (pspGL is the shipped state, so it must be
  reinstalled); record the content hash (`2ea2da7a…` for b30). The probe links `-lraylib` and therefore needs
  the standard GL libs (`-lGL -lpspgu -lpspvfpu`).
- **No API change.** The cache pattern is demonstrated in the probe; it is guidance for apps, not a new
  raylib function.
- **Do not touch another session's files.** `docs/psp/memory-map.md` may be dirty from concurrent work; if T5
  updates §10.1/§14, stage only that file's own hunk set and never `git add -A`.
- Commit messages carry the task ID: `sprint 070 T3: attribute the scene matrix cost`.

**Commands.**

```sh
export PSPDEV=/home/psp/pspdev
export PATH=$PATH:$PSPDEV/bin

/home/psp/raylib-psp/tools/pspgu-build.sh --check
cd /home/psp/raylib-psp/src && make PLATFORM=PLATFORM_PSP -j8
cp libraylib.a $PSPDEV/psp/lib/libraylib.a

cd ../psp-tests/matrix_probe        # built in T1
make
( printf './matrix_probe.prx\n'; sleep 30 ) | pspsh
pspsh -e "reset"

# scene attribution (T3): the probe scales N; scene_3d stays the frozen parity capture
cd ../matrix_probe && make MODE=scene COUNT=100
```

**Do not touch.** `src/rlgl.h`'s public API, the pspgu draw/clip path, the frozen `scene_3d` sample's
output, audio, and the input phases. Measurement code may add counters; it must not change rendering.

**Done means** §8, with a cost table and a written GUM go/no-go.

## 1. Objective

Answer with numbers whether GUM is worth adopting for the camera/matrix path: per-call and per-frame costs
for the camera builders and world-to-screen helpers, the matrix share of a 3D scene at N=1/10/100/500, and
a measured cached-vs-rebuild delta. Adopt GUM only if a threshold is crossed; otherwise close with the
numbers and keep raymath.

## 2. Why now

Phase 10 is closed; the 60 fps ceiling is between N=50 and N=99 (069 §7), and the cost above it is engine-side
per-`DrawMesh` matrix and culling work in software float. Before adding a second matrix library or changing the
camera path, we need to know whether that CPU work is visible, and which component it is. The camera builders run
once per frame; the plausible costs are per-call `GetWorldToScreen*` (which rebuilds P and V on every call) and
per-object model transforms. This sprint prevents a speculative dependency and gives the phase a measured answer;
P10-S9 owns the remaining driver defect and is independent.

## 3. Scope

**In:** `psp-tests/matrix_probe`; per-call/per-frame timings; app-side cached-path measurement; scene matrix
attribution; a conditional default-GUM A/B; documentation.

**Out:** VFPU GUM/thread attribute, `sceGum*` stack, public API changes, renderer/draw-path changes, and any
GUM adoption without a crossed threshold.

## 4. Preconditions

- [x] P10-S7 closed (commit `90f9726a`); Phase 10 recorded done; P10-S9 written for the deferred flicker
- [ ] pspgu `b30` installed and `--check` reports `2ea2da7a…` (pspGL is currently the shipped state)
- [ ] `scene_3d` builds and its frozen parity captures are the reference, not the N-scaling probe
- [ ] `sceKernelGetSystemTimeWide()` timing pattern available (see `psp-tests/math_probe`)
- [x] PSPLink authorised (standing instruction)

## 5. Tasks

Status vocabulary — `todo` · `doing` · `done` · `blocked` · `dropped`.

| ID | task | status | verification | evidence |
|---|---|---|---|---|
| T1 | **Per-call cost probe**: `psp-tests/matrix_probe` (`MODE=api|calls|scene`). Protocol: `InitWindow(480,272)` once (only to give `GetWorldToScreen` real dimensions) then no `BeginDrawing`/`EndDrawing`; 100-call warm-up; 20 blocks x 10,000 calls; per-call µs = median and p95 of the 20 block means; a `volatile` checksum consumes every result; machine-readable line per function (`API <name> n=10000 med_us= p95_us= sum=`). Functions: `GetCameraMatrix2D`, `GetWorldToScreen2D`, `GetScreenToWorld2D`, `GetWorldToScreen`, `GetWorldToScreenEx`, `MatrixLookAt`, `MatrixPerspective`, `MatrixOrtho`, `MatrixMultiply`, `Vector3Transform`, `QuaternionTransform`, `UpdateCamera(CAMERA_CUSTOM)` | `todo` | µs/call median and p95 for each, two runs within repeatability; total per frame at the call count named in §3 | — |
| T2 | **Cached vs rebuild**: for N=100/1000 world positions compare `GetWorldToScreen*` per call with the app-side pattern: build `matProj` and `matView` once, compose `P*V`, then `Vector3Transform` + perspective divide. Report µs total and µs/position for both, plus the break-even N; document the pattern as guidance (no public API change) | `todo` | µs for both at N=100/1000; break-even; no API change | — |
| T3 | **Scene attribution**: N=1/10/100/500 cubes in the probe's `MODE=scene` with the `bunnymark` `STAT`/`SPLIT` harness. Attribute with measurement-only counters under `PSP_MATRIX_PROBE`: (a) per-object model build in `DrawMesh` (Matrix multiply/scale/translate), (b) `rlMultMatrixf` -> `glMultMatrixf`, (c) pspgu matrix upload (`sceGuSetMatrix`), (d) the remainder to the frame. Cross-check the slope against 069's measured sample: `(28.3 - 11.56) / (250 - 50) = 0.084 ms/cube` | `todo` | matrix share in ms and % of frame at N=1/10/100/500; slope agrees with 0.084 ms/cube within noise | — |
| T5 | **Decision and write-back**: adopt cache pattern, adopt GUM, or close "not justified"; update Phase 10 page, memory map §10.1/§14 and baselines | `todo` | decision text with metric/threshold/result; docs build `--strict` | — |

### Design notes

**Probe protocol.** Follow `psp-tests/math_probe` (headless timing, `volatile` sink, 10k iterations,
`sceKernelGetSystemTimeWide`). `matrix_probe` differs in one way: it calls `InitWindow` once so
`GetWorldToScreen` sees 480x272, but it never enters a frame loop, so vsync cannot quantise the timings. Every
line names the function, the iteration count and the median/p95 so T5 can quote it.

**What GUM can and cannot accelerate.** GUM replaces the three matrix builders and 4x4 multiplies. It does not
provide a vector transform, so the per-position `QuaternionTransform` inside `GetWorldToScreenEx` stays raymath;
the real per-call win is T2's cache (build P and V once), not GUM. Keep the two decisions separate.

**Attribution without changing rendering.** The `PSP_MATRIX_PROBE` counters wrap existing calls and add a
`TraceLog` line at `DONE`; they must not alter the draw path, and the arm is built only for T3. `scene_3d`'s
capture must stay byte-identical with the switch off. The counters are a build-time switch: after T3, rebuild
`libraylib.a` without it before the parity gates and T4.

### Task log (append-only)

- 2026-11-22 — sprint written; not started
- 2026-11-23 — implementation review: base commit, preconditions, probe path and protocol, corrected command
  recipe, T4's authorising rows made component-specific, `gl_matrix.c` upload reference fixed, GUM library
  locations verified, matrix-layout trap added. Ready for T1.

## 6. Budgets

| budget | limit | measured | tag |
|---|---|---|---|
| per-call repeatability | two runs within 5 % or 0.5 µs, whichever is larger | | |
| camera matrix build | record (µs/call and per frame) | | |
| per-object matrix share | ≤ 0.1 ms/frame or 1 % | | |
| frame regression (any change) | 0 | | |
| capture parity (any change) | 0 differing pixels | | |

At 16.68 ms the 1 % figure is 0.167 ms, so **0.1 ms/frame is the binding threshold**; state which row authorised
each decision.

## 7. Measurements

| run | build | workload | metric | value | notes |
|---|---|---|---|---|---|
| probe protocol | `P10-S8 a` | 2 runs, 20 x 10k | repeatability | | within 5 % or 0.5 µs |
| camera API | `P10-S8 a` | 10k calls each | µs/call median / p95 | | T1; includes `GetWorldToScreen` and the primitives |
| world-to-screen N=100 | `P10-S8 a` | per call vs cached | µs total | | T2 |
| world-to-screen N=1000 | `P10-S8 a` | per call vs cached | µs total | | T2 |
| scene N=1/10/100/500 | `P10-S8 a` | cubes | matrix ms / frame | | T3 |
| GUM A/B | `P10-S8 b` | same scene | µs, frame p95, captures | | only if T3 authorises |
| gate | `P10-S8 b` | `batch_check` | hash | | `b69830b8…` if a library change lands |

## 8. Exit criteria

- [ ] per-call µs median/p95 table for every camera/matrix helper, two runs within repeatability
- [ ] cached-vs-rebuild delta at N=100/1000, with the app-side pattern documented
- [ ] matrix share of the scene at N=1/10/100/500, in ms and % of frame
- [ ] a written GUM go/no-go using the 0.1 ms / 1 % threshold
- [ ] if GUM is adopted: 0-diff captures, p95 frame not worse, default GUM only
- [ ] if not adopted: the number that decided it, and the cache pattern left as guidance
- [ ] no public API change; `examples/` diff empty; docs build `--strict`

## 9. Result

Not started.

## 10. Follow-ups

- If per-object matrix work is the real cost, the next target is **skinning/instance transforms**, where
  GUM/VFPU can batch many vertices; do not generalise from the camera numbers.
- If the cost is draw submission, the owner is Phase 10's clip/GE path, not a matrix library.
- `libpspgum_vfpu` remains opt-in; any adoption needs the thread attribute, context init and an A/B that
  includes the state-corruption risk.
