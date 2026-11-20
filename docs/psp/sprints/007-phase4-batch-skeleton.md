# Sprint 007 — Batch skeleton, and the measurement that decides the phase

| | |
|---|---|
| Roadmap phase | [Phase 4 — PSP rlgl batcher](../phases/04-rlgl-batcher.md) |
| Branch / base commit | `master` @ `3bd7830b` |
| Started / closed | — / — |
| Build number(s) | `b1` (unbatched arm), `b2` (batched arm) |
| Tasks | 0/7 done |

## 0. Handover

**Situation.** At 1,000 sprites we spend **58.3 µs per sprite**, with **1,025 submissions** (`glEnd`) *and*
**4,124 vertices** per frame. Those two costs have never been separated: no experiment has ever run the same
vertex count with a different submission count. This sprint builds the smallest batcher that separates them, and
that is its real deliverable — the frame-time number is secondary.

**The one number to produce:** at 1,000 sprites, batched, what is the frame time? If it lands near 16.7 ms the
cost was submission and phase 4 largely closes 2D; if it lands near 30 ms the residual is per-vertex command
emission through pspGL and only phase 5's GU levers can remove it. Either answer is a success; a vague "it got
faster" is not.

**Read before acting:**

1. [Phase 4 page](../phases/04-rlgl-batcher.md) — the nine ranked levers from four reference engines, and the
   note on what GL11 *cannot* reach (through mode, `GU_SPRITES`, compact formats: all phase 5).
2. [Baselines](../baselines.md) — the unbatched numbers this sprint is measured against (61.63 ms at 1,000).
3. [Memory map](../memory-map.md) §4B, §6, §14.

**Already true, verified:**

- pspGL exports the array path this sprint needs: `glVertexPointer`, `glTexCoordPointer`, `glColorPointer`,
  `glDrawArrays`, and `glLockArraysEXT`/`glUnlockArraysEXT`.
- Our `--wrap` harness counts `glBegin/glEnd/glVertex2f/glBindTexture/glDrawArrays/glDrawElements`.
- The adopted library is pspGL `ae9a668` with `PSPGL_DLIST_SIZE_WORDS=8192`. **A 4,000-vertex batch will not fit
  one list** at ~4 words per vertex, so expect `command_list_rollovers` in the batched arm. That is normal — record
  the count with the `PSPGL_PROFILE=1` build rather than treating a rollover as a bug.

**The crux, stated up front — why naive batching does nothing.** `DrawTexturePro` emits, per sprite:
`rlSetTexture(T)` → `rlBegin(QUADS)` → 4 vertices → `rlEnd` → `rlSetTexture(0)`. If a batcher flushes on every
texture change, it flushes twice per sprite and you have built nothing. The fix is that **`rlSetTexture(0)` must
not flush**: it records a *pending* texture, and the flush happens only when a draw actually needs a different
texture than the batch is building with. Because the next sprite sets `T` again before any draw, the pending 0 is
overwritten and no flush occurs. Get this wrong and the sprint measures zero.

**Hard rules.**

- **Both arms in one binary.** Guard the batcher with a build flag (`PSP_BATCH=0` / `1`) so the same commit
  measures batched and unbatched. Two branches compared across builds is a confounded experiment.
- Batched output must be **pixel-identical** to unbatched on the samples, or the difference explained.
- Nothing outside `src/rlgl.h` and the test harness. If the batcher appears to need a platform-layer change, stop
  and record why.
- `reset` before each run; never send `exit`; the app must not call `sceKernelExitGame`.
- Threshold stays **0.1 %**; two runs per arm.

**Commands.**

```sh
export PSPDEV=/home/psp/pspdev
export PATH=$PATH:$PSPDEV/bin

cd /home/psp/raylib-psp/src
make clean && make PLATFORM=PLATFORM_PSP PSP_BATCH=1     # batched library
cp libraylib.a $PSPDEV/psp/lib/libraylib.a

cd ../psp-tests/bunnymark
make clean && make PSP_LARGE_MEMORY=0 BUNNY_COUNT=1000 FRAMES=100 TARGET_FPS=0
( printf './bunnymark.prx\n'; sleep 20 ) | pspsh
pspsh -e "reset"
```

**Done means** §8: submissions collapse, output matches, and the submission-vs-vertex decomposition is on paper.

## 1. Objective

A batcher behind `PSP_BATCH` that accumulates vertices and submits once per compatible state run, with pixels
identical to the unbatched path — plus the measurement that says what fraction of the 58.3 µs per sprite was
submission and what fraction is per-vertex cost.

## 2. Why now

Every later phase 4 sprint assumes batching helps. If the residual after batching is still ~30 ms at 1,000
sprites, that assumption is wrong and the plan changes. One sprint, one measurement, before more code.

## 3. Scope

**In:** accumulation in `rlgl.h`'s GL11 path under `PLATFORM_PSP`, a flush policy, a preallocated buffer, the
A/B flag, the decomposition measurement, pixel comparison on two samples.

**Out:** state-run optimisation beyond texture/mode (008), capacity tuning and glyph runs (009), the native GU
backend (phase 5), anything that needs `GU_TRANSFORM_2D` or `GU_SPRITES` — those are unreachable from GL11 and
belong to phase 5.

## 4. Preconditions

- [ ] Phase 3 closed: pspGL `ae9a668` staged with `PSPGL_DLIST_SIZE_WORDS=8192`, `tools/pspgl-build.sh --check` passes
- [ ] [Baselines](../baselines.md) present: unbatched 61.63 ms at N=1,000, 294.63 at 5,000
- [ ] `psp-tests/bunnymark` builds and reaches `DONE` unbatched
- [ ] `psp-tests/basic_shapes` archived reference: 284 colours, two identical captures
- [ ] PSPLink up; `reset` gives 25,149,440 B free

## 5. Tasks

| ID | task | status | verification | evidence |
|---|---|---|---|---|
| T1 | wire the switch first: `ifdef PSP_BATCH` in the `PLATFORM_PSP` block of `src/Makefile`, mirroring `PSP_DISABLE_VSYNC`, so `-DPSP_BATCH=0/1` reaches the compiler; then accumulation — `rlBegin`/`rlEnd`/`rlVertex2f`/`rlVertex2i`/`rlTexCoord2f`/`rlColor4ub` write into a preallocated buffer instead of calling GL per vertex | `todo` | **`make -n PLATFORM=PLATFORM_PSP PSP_BATCH=1 \| grep -o '\-DPSP_BATCH=1'` prints it** — without this the flag is silently ignored and both arms build identically, which is the failure this sprint exists to avoid; then `glVertex2f` count ≈ 0 under `PSP_BATCH=1` while the screen still draws | — |
| T2 | flush policy: pending-texture semantics (see §0), flush on a *draw* that needs a different texture or primitive, on capacity, and at `EndDrawing` | `todo` | 1,000 same-texture sprites produce single-digit `glDrawArrays` calls, not 1,000 | — |
| T3 | buffer: preallocated once, sized in vertices; overflow flushes rather than corrupting | `todo` | no per-frame allocation; capacity reported in the log | — |
| T4 | extend the harness: `__wrap_glDrawArrays` must add its `count` to an `arrayVertices` total, because in the batched arm `glVertex2f` is never called and the existing `vertices` counter would read 0 — the per-vertex cost could not be attributed at all | `todo` | batched arm reports nonzero `arrayVertices`; unbatched reports nonzero `verts` and zero `arrayVertices` | — |
| T4b | choose the primitive for the batched path by measurement: 4-vertex quads vs 6-vertex triangles (`GL_QUADS` support in pspGL arrays is not guaranteed) | `todo` | whichever wins by >0.1 %; vertices per sprite recorded for both | — |
| T5 | try `glLockArraysEXT`/`glUnlockArraysEXT` around the batch — the fork's profiler counts `array_locked_fast_paths` | `todo` | either a measured gain or a recorded "no effect on this driver" | — |
| T6 | **the decomposition**: same workload, both arms, N=100 / 1,000 / 5,000, two runs each | `todo` | frame time, `glDrawArrays`/`glEnd` counts and vertices/frame for both arms in one table | — |
| T7 | correctness needs a **deterministic scene**, because the samples animate (`basic_shapes/main.c:146` does `rotation += 0.2f`, so no two frames match and no two captures can be compared across arms). Add `psp-tests/batch_check`: a static grid of textured sprites plus a few shapes, fixed positions, two frames, `DONE` + sleep | `todo` | the same build produces **two identical captures**; batched vs unbatched captures are identical pixel-for-pixel; `basic_shapes` is a smoke test only (renders, 284 colours) | — |

Status vocabulary: `todo` / `doing` / `done` / `blocked` / `dropped`. IDs are stable — never renumber.

### Task log (append-only)

- _(not started)_

## 6. Budgets

| budget | limit | measured | tag |
|---|---|---|---|
| per-frame allocation | 0 | | |
| batched submissions at N=1,000 | ≤ 10 | | |
| pixel difference vs unbatched | 0, or explained | | |
| `src/` changes | `rlgl.h` only, behind `PLATFORM_PSP` + `PSP_BATCH` | | |

## 7. Measurements

The decomposition — same vertex count, different submission count:

| N | arm | frame median | `glEnd`/frame | `glDrawArrays`/frame | `verts` (immediate) | `arrayVertices` |
|---|---|---|---|---|---|---|
| 100 | unbatched | | | 0 | | 0 |
| 100 | batched | | 0 | | 0 | |
| 1,000 | unbatched | | | 0 | | 0 |
| 1,000 | batched | | 0 | | 0 | |
| 5,000 | unbatched | | | 0 | | 0 |
| 5,000 | batched | | 0 | | 0 | |

The two vertex columns are the point: the unbatched arm spends its vertices through `glVertex2f`, the batched arm
through an array, and comparing frame times at **equal total vertices** is what separates submission cost from
per-vertex cost.

Primitive comparison at N=1,000 (`PSP_BATCH=1`):

| primitive | vertices/sprite | frame median | note |
|---|---|---|---|
| quads (4 v) | 4 | | if pspGL arrays accept `GL_QUADS` |
| triangles (6 v) | 6 | | fallback |

Verdict: submission cost = (unbatched − batched) at equal vertex count; residual = batched frame time.

## 8. Exit criteria

- [ ] batched and unbatched are two builds of one commit, measured in one session
- [ ] submissions collapse to single digits at N=1,000
- [ ] `basic_shapes` and `cubicmap` are pixel-identical between arms, or the difference is explained
- [ ] the table in §7 is complete enough to attribute the 58.3 µs: how much was submission, how much is per-vertex
- [ ] a written verdict: does phase 4 close 2D, or does it hand the job to phase 5's GU levers?
- [ ] `src/` diff limited to `rlgl.h` behind the two guards

## 9. Result

(To fill in on close. The most valuable outcome is a batched frame time of ~30 ms, not ~5 ms: it would say the
per-vertex path through pspGL is the wall and save phase 4 from tuning a lever that cannot move the number.)

## 10. Follow-ups

- If the residual is per-vertex, record which of phase 5's levers removes it (`GU_TRANSFORM_2D` removes the
  matrix work per draw; 2-vertex `GU_SPRITES` removes half the vertices; compact formats remove bytes) and quote
  the measured per-vertex cost as the target those must beat.
- If `GL_QUADS` is unsupported in pspGL's array path, that is worth adding to the memory map as a driver
  limitation — it silently changes vertex counts for every 2D batch.
- Any gain from `glLockArraysEXT` is worth knowing before phase 5, because the native backend has no equivalent.
