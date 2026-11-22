# Sprint 073 (P11-S11) — 2D shape and particle math: attribute before vectorising

| | |
|---|---|
| Roadmap phase | [Phase 11 — General optimisation outside the renderer](../phases/11-general-optimisation.md) |
| Sprint | P11-S11 |
| Branch / base commit | `master` @ `816a8c25` (pin at start) |
| Started / closed | 2026-11-22 (written) / — |
| Build number(s) | pspgu as installed (`--check` recorded); `libraylib.a` default `PSP_BATCH=1` |
| Tasks | 0/4 done |

## 0. Handover

**Situation.** The 2D corpus reports **work** of 12.8–13.1 ms for shapes at 1,000 objects, 3.07 ms for 500
particles and 10.59 ms mixed, but `work` is GL submission time, not `rshapes.c` maths. Before adding VFPU to
circles, arcs, rotated quads or particles, this sprint splits the frame into **vertex-generation maths,
submission, and GE/display wait**. It is measurement-only; if the maths is the cost, a Phase 7 follow-up owns
the implementation, because a change that moves a vertex is not a Phase 11 change.

**Read before acting**, in this order:

1. `../memory-map.md` §4 — per-sprite submission is the measured wall, not fill; §10.1 — `vrot.p`,
   `vsin/vcos`, truncation and thread rules.
2. `../sprints/052-phase11-measurement-harness.md` — CPU zones, `PSP_DISABLE_VSYNC`, and the 0.1 % rule.
3. `../sprints/018-phase7-corpus-and-counters.md` … `023-phase7-cache-strips-and-close.md` — the corpus
   modes, `work`/`swap`/`sim` counters and the gates.
4. `src/rshapes.c` — `DrawCircle*`, `DrawRing`, `DrawRectanglePro`, `DrawTriangle*`, collision helpers;
   `src/rlgl.h` batcher (`rlBegin`/`rlVertex*`) — where vertex generation ends and submission begins.
5. `psp-tests/scene_2d` — the `text`/`tiles`/`particles`/`shapes`/`mixed` modes and `psp_counters`.

**First action.** Add a test-only `ZONES=1` build to `scene_2d` that timestamps (a) shape/particle vertex
generation, (b) batcher submission, (c) `EndDrawing`/swap; run shapes 1000, particles 500/2000 and mixed with
`PSP_DISABLE_VSYNC=1`. No renderer change.

**Hard rules.**

- **Measurement only.** This sprint does not change `rshapes.c`, the batcher or the draw path. The deliverable
  is the split and a decision.
- **Do not attribute submission to maths.** `work` includes the GL boundary; only the zone around the
  vertex-generation loop is maths.
- **CPU zones plus no-vsync.** Frame time alone is vblank-quantised; use the 052 harness and report median/p95.
- **Per operation, with accuracy.** VFPU `sin/cos/atan2/rot` are candidates only with a time row and an error
  row; never for frame-accumulated state.
- **Handoff, not scope creep.** If the maths crosses the threshold, write the Phase 7 sprint with the exact
  functions and sizes; do not implement it here.
- **Keep the gates.** Any later vectorisation must keep the 2D captures byte-identical and `batch_check`
  `b69830b8…`.
- Commit messages carry the task ID.

**Commands.**

```sh
export PSPDEV=/home/psp/pspdev
export PATH=$PATH:$PSPDEV/bin

/home/psp/raylib-psp/tools/pspgu-build.sh --check
cd /home/psp/raylib-psp/src && make PLATFORM=PLATFORM_PSP -j8

cd ../psp-tests/scene_2d
make MODE=shapes COUNT=1000 ZONES=1
( printf './scene_2d.prx\n'; sleep 25 ) | pspsh
pspsh -e "reset"
make MODE=particles COUNT=500 ZONES=1
make MODE=mixed ZONES=1
```

**Do not touch.** `src/rshapes.c`, `src/rlgl.h`, pspgu, the corpus scenes, audio/input, and the existing
`work`/`swap` counters' meaning.

**Done means** §8, with the math/submission/wait split and a written Phase 7 handoff or a no-op decision.

## 1. Objective

Answer with numbers whether 2D shape/particle **maths** is a visible part of the frame. If it is, name the
functions and sizes for a Phase 7 vectorisation sprint; if it is not, close the VFPU-2D idea with the number.

## 2. Why now

The 2D corpus is measured but its `work` is submission, and the VFPU question cannot be answered from it.
This sprint prevents a speculative `rshapes.c` rewrite and gives Phase 7 a precise target if one exists.

## 3. Scope

**In:** test-only CPU zones in `scene_2d`; the shapes/particles/mixed runs; per-op microbenchmarks; the
decision and handoff.

**Out:** any renderer change, VFPU adoption in `rshapes.c`, batcher changes, and the 3D/audio paths.

## 4. Preconditions

- [ ] P11-S1 harness (052) exists; `PSP_DISABLE_VSYNC=1` and CPU zones work
- [ ] pspgu installed; `--check` recorded
- [ ] `scene_2d` builds for shapes/particles/mixed and its captures match the existing gates
- [ ] PSPLink available; the user has approved the run

## 5. Tasks

| ID | task | status | verification | evidence |
|---|---|---|---|---|
| T1 | **Attribution**: `ZONES=1` splits vertex-generation maths, submission and swap for shapes 1000, particles 500/2000, mixed, text, tiles | `todo` | ms and % per zone; median/p95; no-vsync arm | — |
| T2 | **Per-op microbench**: `sin/cos/atan2` (circles/arcs), `vrot` (rotated quads/particles), gradient interpolation and collision helpers; libm vs VFPU, with accuracy | `todo` | µs/op and error rows; input ranges | — |
| T3 | **Decision/handoff**: if maths > 0.1 ms/frame or 1 %, write the Phase 7 sprint naming functions/sizes; otherwise close | `todo` | decision text with metric/threshold; exact target list if positive | — |
| T4 | **Write-back**: Phase 7/11 pages, baselines, memory map if a fact changes; `mkdocs --strict` | `todo` | docs build; no code change | — |

### Task log

- 2026-11-22 — sprint written; not started

## 6. Budgets

| budget | limit | measured | tag |
|---|---|---|---|
| measurement noise | ≤ 1 µs p95 | | |
| maths share threshold | 0.1 ms/frame or 1 % | | |
| capture parity (any later change) | 0 diff | | |
| frame regression | 0 | | |

## 7. Measurements

| run | build | workload | metric | value | notes |
|---|---|---|---|---|---|
| zones shapes | `P11-S11 a` | 1000 shapes | maths/submit/wait ms | | no-vsync |
| zones particles | `P11-S11 a` | 500/2000 | same | | no-vsync |
| zones mixed | `P11-S11 a` | mixed | same | | no-vsync |
| microbench | `P11-S11 a` | per-op | µs and error | | libm vs VFPU |

## 8. Exit criteria

- [ ] the maths/submission/wait split for shapes, particles and mixed
- [ ] per-op time and accuracy rows for the candidate maths
- [ ] a written decision: Phase 7 handoff with exact functions/sizes, or “maths is not the cost”
- [ ] no renderer change in this sprint; `batch_check`/2D captures unchanged
- [ ] docs updated; `mkdocs --strict`

## 9. Result

Not started.

## 10. Follow-ups

- If maths wins: the Phase 7 sprint owns `rshapes.c` and the batcher changes, with byte-identical captures.
- If submission wins: the owner is the batcher/Phase 4–7 path, not VFPU.
- Do not fold text/tiles into a shape sprint; their cost is glyph/tile submission.
