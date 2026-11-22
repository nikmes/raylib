# Sprint 070 (P10-S8) — Camera and matrix CPU cost: measure before GUM

| | |
|---|---|
| Roadmap phase | [Phase 10 — 3D and advanced](../phases/10-3d-advanced/index.md) |
| Sprint | P10-S8 |
| Branch / base commit | `master` @ `816a8c25` (P10-S7 review; pin at start) |
| Started / closed | 2026-11-22 (written) / — |
| Build number(s) | `libraylib.a` default `PSP_BATCH=1`; pspgu unchanged (record `--check`) |
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
3. `tools/pspgu/gl_matrix.c` — the GL stack, `glMultMatrixf` (`:168`), and the upload-on-change
   (`:55`); the upload is `sceGuSetMatrix`, once per changed matrix.
4. `../memory-map.md` §10.1/§14 — GUM default vs VFPU, no VFPU intrinsics, thread attribute and context
   rules; `pspgum.h`, `libpspgum.a`, `libpspgum_vfpu.a`.
5. `../sprints/061-phase10-camera-relative-and-ge-native-models.md` §9/§T5 — the camera-relative result was
   **null** because pspgu composes view×model on the CPU; do not repeat that experiment.

**First action.** Build `psp-tests/matrix_probe` and capture the **before** numbers: 10,000 calls each of
`GetCameraMatrix2D`, `GetWorldToScreen2D`, `GetScreenToWorld2D`, `MatrixLookAt`, `MatrixPerspective`,
`GetWorldToScreenEx`, and 1,000-object per-call loops. No code change before the rows exist.

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
- **pspgu first.** `tools/pspgu-build.sh --check` before building; record the content hash.
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

Phase 10 is reopened for draw/clip cost. Before adding a second matrix library, we need to know whether the
CPU matrix work is even visible. The camera path is known to be once-per-frame; the only plausible costs are
per-entity `GetWorldToScreen*` and per-object model transforms. This sprint prevents a speculative
dependency and gives the phase a measured answer.

## 3. Scope

**In:** `psp-tests/matrix_probe`; per-call/per-frame timings; app-side cached-path measurement; scene matrix
attribution; a conditional default-GUM A/B; documentation.

**Out:** VFPU GUM/thread attribute, `sceGum*` stack, public API changes, renderer/draw-path changes, and any
GUM adoption without a crossed threshold.

## 4. Preconditions

- [ ] P10-S7 is at least written; this sprint does not alter its clip/lit work
- [ ] pspgu installed (`--check`); content hash recorded
- [ ] `scene_3d` builds and runs with a deterministic capture (frozen parity reference, not the N-scaling probe)
- [ ] `GET_TIME`-equivalent counters available (`sceKernelGetSystemTimeWide`)
- [ ] PSPLink available and the user has approved the run

## 5. Tasks

Status vocabulary — `todo` · `doing` · `done` · `blocked` · `dropped`.

| ID | task | status | verification | evidence |
|---|---|---|---|---|
| T1 | **Per-call cost probe**: `psp-tests/matrix_probe` (`MODE=api|calls|scene`) times 10k iterations of `GetCameraMatrix2D`, `GetWorldToScreen2D`, `GetScreenToWorld2D`, `MatrixLookAt`, `MatrixPerspective`, `MatrixOrtho`, `GetWorldToScreenEx`, `UpdateCamera`; volatile checksum prevents elision | `todo` | µs/call p95 for each; total per frame at 1× | — |
| T2 | **Cached vs rebuild**: for N=100/1000 world positions, compare `GetWorldToScreen*` per call with a precomputed view-projection + transform loop; document the app-side pattern (no API change) | `todo` | µs for both; the cache win at N; no public API change | — |
| T3 | **Scene attribution**: N=1/10/100/500 cubes in the probe's `MODE=scene` (`scene_3d` stays the frozen parity capture); split per-object matrix build, `rlMultMatrixf`/upload, draw submission and total frame | `todo` | matrix share in ms and % of frame; threshold decision (0.1 ms / 1 %) | — |
| T4 | **Conditional GUM A/B**: only if T3 crosses the threshold; `PSP_CAMERA_GUM=1` builds `gumLookAt`/`gumPerspective`/`gumOrtho` into raylib `Matrix`; default GUM, no VFPU; captures identical | `todo` | µs delta vs raymath; p95 frame; 0 diff captures; or `dropped` with the T3 number | — |
| T5 | **Decision and write-back**: adopt cache pattern, adopt GUM, or close "not justified"; update Phase 10 page, memory map §10.1/§14 and baselines | `todo` | decision text with metric/threshold/result; docs build `--strict` | — |

### Task log (append-only)

- 2026-11-22 — sprint written; not started

## 6. Budgets

| budget | limit | measured | tag |
|---|---|---|---|
| per-call measurement noise | ≤ 1 µs p95 | | |
| camera matrix build | record | | |
| per-object matrix share | ≤ 0.1 ms/frame or 1 % | | |
| frame regression (any change) | 0 | | |
| capture parity (any change) | 0 differing pixels | | |

## 7. Measurements

| run | build | workload | metric | value | notes |
|---|---|---|---|---|---|
| camera API | `P10-S8 a` | 10k calls each | µs/call p95 | | T1 |
| world-to-screen N=100 | `P10-S8 a` | per call vs cached | µs total | | T2 |
| world-to-screen N=1000 | `P10-S8 a` | per call vs cached | µs total | | T2 |
| scene N=1/10/100/500 | `P10-S8 a` | cubes | matrix ms / frame | | T3 |
| GUM A/B | `P10-S8 b` | same scene | µs, frame p95, captures | | only if T3 authorises |
| gate | `P10-S8 b` | `batch_check` | hash | | `b69830b8…` if a library change lands |

## 8. Exit criteria

- [ ] per-call µs p95 table for every camera/matrix helper
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
