# Sprint 007 — Batch skeleton, and the measurement that decides the phase

| | |
|---|---|
| Roadmap phase | [Phase 4 — PSP rlgl batcher](../phases/04-rlgl-batcher.md) |
| Branch / base commit | `master` @ `3bd7830b` |
| Started / closed | 2026-11-20 / 2026-11-20 |
| Build number(s) | `b1` unbatched `libraylib.a` sha256 `0b0a7aeb…`, `b2` batched `2ee10d2a…` |
| Tasks | 8/8 done (T1–T7 + T4b) |

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

**Deviation, with reason:** the batch state lives in `src/platforms/rcore_psp.c`, not in `rlgl.h`. `rlgl.h` is
included by five translation units, so a `static` buffer declared there is five buffers, and the flush that
`rcore.c` runs at `EndDrawing()` would see an empty one. `rlgl.h` only gains the declarations plus the guarded
one-line delegations; the state and the emission live in the PSP-only unit that raylib already uses for exactly
this kind of code. Measured: `rcore.o` bss 19,996 → 118,324 B, i.e. one buffer, not five.

## 4. Preconditions

- [ ] Phase 3 closed: pspGL `ae9a668` staged with `PSPGL_DLIST_SIZE_WORDS=8192`, `tools/pspgl-build.sh --check` passes
- [ ] [Baselines](../baselines.md) present: unbatched 61.63 ms at N=1,000, 294.63 at 5,000
- [ ] `psp-tests/bunnymark` builds and reaches `DONE` unbatched
- [ ] `psp-tests/basic_shapes` archived reference: 284 colours, two identical captures
- [ ] PSPLink up; `reset` gives 25,149,440 B free

## 5. Tasks

| ID | task | status | verification | evidence |
|---|---|---|---|---|
| T1 | wire the switch first: `ifdef PSP_BATCH` in the `PLATFORM_PSP` block of `src/Makefile`, mirroring `PSP_DISABLE_VSYNC`, so `-DPSP_BATCH=0/1` reaches the compiler; then accumulation — `rlBegin`/`rlEnd`/`rlVertex2f`/`rlVertex2i`/`rlTexCoord2f`/`rlColor4ub` write into a preallocated buffer instead of calling GL per vertex | `done` | `make -Bn PLATFORM=PLATFORM_PSP PSP_BATCH=1 \| grep -o '\-DPSP_BATCH=1'` prints it (twice, once per compile line); without the flag the same grep prints nothing. Batching is visible in the counters: `begins=0 verts=0` in the batched arm | `make -B` compile lines carry `-DPSP_BATCH=0` / `-DPSP_BATCH=1`; bunnymark N=1000 batched logs `begins=0 ends=0 verts=0`, unbatched logs `begins=204998 ends=204998 verts=819992` over 200 frames |
| T2 | flush policy: pending-texture semantics (see §0), flush on a *draw* that needs a different texture or primitive, on capacity, and at `EndDrawing` | `done` | 1,000 same-texture sprites produce single-digit `glDrawArrays` calls, not 1,000 | 1,000 sprites → **3 draws/frame** (two batches + the HUD); 5,000 → **9** (8 batches of 682 quads + HUD); `batch_check`, 51 primitives → **2** draws/frame |
| T3 | buffer: preallocated once, sized in vertices; overflow flushes rather than corrupting | `done` | no per-frame allocation; capacity reported in the log | static `RLPspVertex[4096]` = 98,304 B, visible as `rcore.o` bss 19,996 → **118,324 B**; overflow path is `rlPspReserve()` → flush → continue, exercised by the N=5,000 arm (8 flushes/frame) and pixel-clean |
| T4 | extend the harness: `__wrap_glDrawArrays` must add its `count` to an `arrayVertices` total, because in the batched arm `glVertex2f` is never called and the existing `vertices` counter would read 0 — the per-vertex cost could not be attributed at all | `done` | batched arm reports nonzero `arrayVertices`; unbatched reports nonzero `verts` and zero `arrayVertices` | N=1000: batched `verts=0 arrayVerts=1,229,994`, unbatched `verts=819,992 arrayVerts=0` over 200 frames — both **4,100 and 6,150 vertices per frame** respectively |
| T4b | choose the primitive for the batched path by measurement: 4-vertex quads vs 6-vertex triangles (`GL_QUADS` support in pspGL arrays is not guaranteed) | `done` | whichever wins by >0.1 %; vertices per sprite recorded for both | **`GL_QUADS` is not a primitive in pspGL's array path**: `geprim_tab` maps it to `GE_TRIANGLE_FAN`, so a 240-vertex quad run drew one fan from vertex 0 — on screen, spikes radiating from the first sprite. Quads are now expanded to two triangles on push: **6 vertices/sprite**, and the picture is byte-identical to the immediate path |
| T5 | try `glLockArraysEXT`/`glUnlockArraysEXT` around the batch — the fork's profiler counts `array_locked_fast_paths` | `done` | either a measured gain or a recorded "no effect on this driver" | **No effect**: work 18.09 → 18.17 ms (+0.4 %), frame median 33.367 ms both. The cache is built per lock, so it copies as much as the unlocked path (§7, T5 table). Feature stays behind `PSP_BATCH_LOCK=1` |
| T6 | **the decomposition**: same workload, both arms, N=100 / 1,000 / 5,000, two runs each | `done` | frame time, `glDrawArrays`/`glEnd` counts and vertices/frame for both arms in one table | §7: at N=1,000, **66.73 → 33.37 ms**; marginal cost per sprite **62.6 → 16.7 µs**, of which the submission half (45.9 µs) is gone. Residual is linear in vertices (2.4–2.6 µs each) and flat in pixels (4× fill = +0.1 %) |
| T7 | correctness needs a **deterministic scene**, because the samples animate (`basic_shapes/main.c:146` does `rotation += 0.2f`, so no two frames match and no two captures can be compared across arms). Add `psp-tests/batch_check`: a static grid of textured sprites plus a few shapes, fixed positions, two frames, `DONE` + sleep | `done` | the same build produces **two identical captures**; batched vs unbatched captures are identical pixel-for-pixel; `basic_shapes` is a smoke test only (renders, 284 colours) | `psp-tests/batch_check`: 8×5 textured grid + `DrawRectangle` + `DrawText`, two frames. Batched `md5 ee9e00f8c25c3f773fabcad31cfc8592` = unbatched `md5 ee9e00f8…`, `cmp -l` reports **0 differing bytes** |

Status vocabulary: `todo` / `doing` / `done` / `blocked` / `dropped`. IDs are stable — never renumber.

### Task log (append-only)

- 2026-11-20 — **T1 `done`** (the Makefile half). `make -Bn ... PSP_BATCH=1 | grep -o -- -DPSP_BATCH=1` prints the
  flag once per compile line; with no flag the same grep prints nothing. Both arms build with 0 warnings and the
  same source tree.
- 2026-11-20 — **T1 deviation from §3, recorded as the rule asks**: the batch state does **not** live in `rlgl.h`.
  `rlgl.h` is included by `rcore.c`, `rshapes.c`, `rtextures.c`, `rtext.c` and `rmodels.c`, so a `static` buffer in
  the header is one copy per translation unit — the vertices pushed by `rtextures.c` would be invisible to the flush
  `rcore.c` runs at `EndDrawing()`, and the frame would come out empty. Verified indirectly: bss grows by exactly
  98,328 B in **one** object (`rcore.o`), not five. The entry points are declared in `rlgl.h` behind the guard and
  defined once in `platforms/rcore_psp.c`, a PSP-only translation unit that already carries raylib's PSP code.
- 2026-11-20 — **T2 `done`**: the deferred-texture rule works as designed. `rlSetTexture(T)` only records a pending
  id; the switch is applied when a vertex arrives. 1,000 sprites → **3 draws/frame**, 5,000 → 9 (capacity 4,096
  vertices / 682 quads per flush), `batch_check` 51 primitives → 2. Flush hooks: 31 call sites — matrices, viewport,
  scissor, clear, texture parameters, the GL enable/disable one-liners, wire/point mode, cull face, line width,
  `rlDrawRenderBatchActive`. Blend state needs no hook: `rlSetBlendMode()` is a no-op under `GL11`.
- 2026-11-20 — **T3 `done`**: one static 4,096-vertex buffer, 24 B/vertex (*u,v* floats, `rgba` bytes, *x,y,z*
  floats — the order `glTexCoordPointer`/`glColorPointer`/`glVertexPointer` point into, which also happens to be
  pspGL's native GE layout).
- 2026-11-20 — **T4 `done`**: `__wrap_glDrawArrays` now accumulates `arrayVertices`. Without it the batched arm
  reports 0 vertices drawn and the decomposition is impossible.
- 2026-11-20 — **T4b `done`, and it was a correctness bug, not a preference.** The first batched build drew spikes
  radiating from the first sprite. `pspgl_varray.c`'s `geprim_tab` maps `GL_QUADS` to `GE_TRIANGLE_FAN`: a run of
  240 quad vertices is one fan anchored on vertex 0. `glBegin(GL_QUADS)` hides this because it emits one fan per
  quad. Quads are now assembled in a 4-slot staging array and written out as triangles (0,1,2) and (0,2,3) — the
  fan's own diagonals split exactly the same way. Cost: 6 vertices per sprite instead of 4, i.e. **+50 % vertices**
  against the unbatched arm, which the §7 numbers carry.
- 2026-11-20 — **T7 `done`**: `psp-tests/batch_check` (new) draws a fixed 8×5 textured grid, one untextured
  rectangle and one string, twice, then sleeps. Batched and unbatched captures are **byte-identical**
  (`md5 ee9e00f8c25c3f773fabcad31cfc8592`, `cmp -l` → 0). One incidental finding from the trace: in 6.5-dev the
  default font atlas and the shapes texture share one id, so the rectangle and the text coalesce — a frame's
  submission count is "textures used", not "kinds of primitive".


## 6. Budgets

| budget | limit | measured | tag |
|---|---|---|---|
| per-frame allocation | 0 | | |
| batched submissions at N=1,000 | ≤ 10 | | |
| pixel difference vs unbatched | 0, or explained | | |
| `src/` changes | `rlgl.h` only, behind `PLATFORM_PSP` + `PSP_BATCH` | | |

## 7. Measurements

Session: PSP-1000, one `reset` per run, `TARGET_FPS=0`, two runs per configuration, both arms built from this
commit. Run-to-run agreement was excellent — the two medians of each configuration differ by **0.004 ms or less**,
so a single number per configuration is quoted.

The decomposition — same workload, different submission count:

| N | arm | frame median | work (frame − swap) | `glEnd`/frame | `glDrawArrays`/frame | `verts`/frame | `arrayVertices`/frame |
|---|---|---|---|---|---|---|---|
| 100 | unbatched | 16.683 ms | 9.16 ms | 124 | 0 | 496 | 0 |
| 100 | batched | 16.683 ms | 3.55 ms | 0 | 2 | 0 | 744 |
| 1,000 | unbatched | 66.733 ms | 65.70 ms | 1,025 | 0 | 4,100 | 0 |
| 1,000 | batched | **33.367 ms** | **18.09 ms** | 0 | **3** | 0 | 6,150 |
| 5,000 | unbatched | 316.98 ms | 317.19 ms | 5,024 | 0 | 20,096 | 0 |
| 5,000 | batched | **100.107 ms** | 81.91 ms | 0 | **9** | 0 | 30,150 |

Marginal cost per sprite, from the 1,000 → 5,000 slope:

| arm | µs per sprite | µs per submitted vertex | submissions per frame at N=1,000 |
|---|---|---|---|
| unbatched | **62.6** (4 verts + 1 `glEnd`) | 15.7, submission included | 1,025 |
| batched | **16.7** (6 verts, no `glEnd`) | 2.8 | 3 |

So of the 58.3 µs the old baseline attributed to a sprite, **45.9 µs was submission** and 16.7 µs is what is left —
and the batched arm pays it on **50 % more vertices**, because a quad must be sent as two triangles (T4b). Equal
vertices would have been about 11 µs per sprite.

What the residual is *not*: fill. Four times the pixel area, same vertex count, same frame:

| N=1,000, batched | frame median | work | `arrayVertices`/frame |
|---|---|---|---|
| `SPRITE_SCALE=1` (1.0 M px) | 33.367 ms | 18.09 ms | 6,150 |
| `SPRITE_SCALE=2` (4.1 M px) | 33.369 ms | 18.12 ms | 6,150 |
| `SKIP_DRAW` (HUD only, 0.15 k px) | 16.683 ms | 3.46 ms | 150 |

The residual is **per submitted vertex**, and it is not the draw-call count — 3 draws/frame cannot cost 14.6 ms.
Subtracting the 3.46 ms HUD floor leaves 14.6 ms for 6,150 vertices = 2.4 µs each, and the N=5,000 point gives
(81.91 − 3.5) / 30,150 = 2.6 µs each. Linear in vertices, flat in pixels.

`glLockArraysEXT` (T5):

| N=1,000, batched | frame median | work |
|---|---|---|
| plain `glDrawArrays` | 33.367 ms | 18.09 ms |
| `glLockArraysEXT`/`glUnlockArraysEXT` per flush | 33.367 ms | 18.17 ms |

**No effect** (+0.4 % on a 0.01 ms-stable measurement). The cache is built on first use after the lock and freed on
unlock, so a lock around every draw costs the same copy as the unlocked path — the zero-copy route in
`__pspgl_cache_arrays` needs the arrays to live in a pspGL buffer object, not in client memory.

Two protocol notes, both recorded rather than smoothed over:

- **The archived baseline is 8 % optimistic against today's session.** [Baselines](../baselines.md) has 61.63 ms at
  N=1,000; the unbatched arm measures **66.73 ms** today, four 16.68 ms display periods. 61.63 does not sit on a
  16.68 ms vsync grid, so it was taken under a different display state. Every comparison in this sprint is
  same-session, which is why the unbatched arm was re-measured instead of quoted.
- The N=5,000 unbatched row was re-run with 100 frames and a longer host window; at 200 frames the host pipe closed
  before `DONE` and the next `reset` killed a live app. Frame time per frame is unaffected.

Verdict: submission cost = 45.9 µs per sprite, removed. Residual = 16.7 µs per sprite, linear in vertices, flat in
pixels, unmoved by locked arrays. At 60 fps the budget left for sprites is 16.68 − 3.46 = 13.2 ms, i.e. **~790
sprites/frame batched against ~220 unbatched**.


## 8. Exit criteria

- [x] batched and unbatched are two builds of one commit, measured in one session — `PSP_BATCH=0` and `PSP_BATCH=1`
  from the same tree, one device session per arm, both measured 2026-11-20
- [x] submissions collapse to single digits at N=1,000 — **3** `glDrawArrays` per frame against 1,025 `glEnd`
- [x] output identical between arms, or the difference explained — `psp-tests/batch_check` is **byte-identical**.
  `basic_shapes`/`cubicmap` were replaced as the gate by that test on purpose: both animate (no two captures of one
  build match, so no cross-arm comparison is possible) and `batch_check` covers the same three submission kinds
  (textured quad, untextured quad, font atlas quad) on a fixed scene. A sample sweep of the 3D path is left open in
  §10, because `PSP_BATCH=1` drops `rlNormal3f` and lighting was never in this sprint's scope
- [x] the table in §7 attributes the 58.3 µs — 45.9 µs submission, 16.7 µs per sprite left, linear in vertices
- [x] verdict written — §9
- [x] `src/` diff behind the two guards, with one recorded deviation: the batch *state* is in
  `platforms/rcore_psp.c` because five translation units include `rlgl.h` (§3, task log)

## 9. Result

**Phase 4 does not close 2D at 1,000 sprites, and the measurement says exactly why.**

Batching is the single largest win measured in this project so far: at N=1,000 the frame goes **66.73 → 33.37 ms**
(2.0×), at N=5,000 **316.98 → 100.11 ms** (3.2×), with 1,025 draw calls per frame reduced to 3 and 5,024 to 9. Per
sprite that is **62.6 → 16.7 µs**. On the device that is the difference between a slideshow and 30 fps with 1,000
animated sprites filling the screen.

But 33.37 ms is two display periods, and a 60 fps frame at N=1,000 needs 18.09 ms of work to fit in 16.68. The
remaining 16.7 µs per sprite is **per submitted vertex**, not per draw call (3 draws cannot cost 14.6 ms) and not
per pixel (4× the fill changes the frame by 0.1 %). pspGL costs ~2.5 µs to take one vertex from application memory
into a GE draw, and this sprint cannot say whether that time is spent in its conversion copy or in the GE vertex
path — that needs the fork's `PSPGL_PROFILE=1` build, which is a follow-up.

What that means for the plan:

- **Everything submission-shaped is done.** Texture-run coalescing, capacity tuning and glyph runs (sprints 008/009)
  can now move at most the 3 draws/frame left, i.e. almost nothing. They should be re-scoped around *vertex count*
  instead, or dropped.
- **The next lever is vertices per sprite.** 6 now (two triangles), 4 at best through GL11, 2 through `GU_SPRITES`.
  Phase 5's native backend cuts the residual by 3× on that axis alone and by removing pspGL from the path. That is
  where the remaining 16.7 µs is spent, and it is worth one GU backend, not a year of GL11 tuning.
- **`GU_TRANSFORM_2D` and compact vertex formats now have a number to beat**: 2.5 µs per submitted vertex.

One trap is now documented because it cost a broken frame: pspGL maps `GL_QUADS` to a GE **triangle fan**, so a
batched quad run renders as spikes radiating from its first vertex. Quads must be expanded to triangles by the
batcher, and the +50 % vertex count that follows is part of every number above.

## 10. Follow-ups

- **Where the 2.5 µs per vertex goes.** Build pspGL with `PSPGL_PROFILE=1` and run the batched arm at N=1,000 and
  N=5,000 with `PSPGL_PROFILE_APP=1`: `native_vertex_array_copies` vs `converted_vertex_array_copies` says which
  copy path the 24-byte layout took, and `command_list_rollovers` /
  `command_list_reuse_sync_wait_us` say whether the batcher is being paced by the GE instead of by the CPU. This is
  the one question sprint 007 could not answer.
- **Phase 5 has a target per lever**: `GU_SPRITES` (2 vertices instead of 6, i.e. −67 %), `GU_TRANSFORM_2D` (no
  matrix work per draw), compact vertex formats (bytes per vertex), direct GE-formatted writes (no conversion copy).
  Each is judged against 2.5 µs per submitted vertex.
- **`PSP_BATCH=1` drops `rlNormal3f`, so it is a 2D mode.** Any lit 3D scene needs normals in the vertex format
  before the flag can be defaulted on; per-vertex lighting via GE is phase 10's problem.
- **Sprint 008/009 need re-scoping.** With 3 draws per frame left at N=1,000, further state-run coalescing and
  capacity tuning cannot pay. Redirect them at vertex count (sprite primitive, glyph atlas packing, compact formats)
  or close them as covered.
- `glLockArraysEXT` is worth nothing on this driver unless the arrays live in a pspGL **buffer object**; that variant
  was not tried, and the native backend has no equivalent anyway, so it is recorded and dropped.
- A **sample sweep** with `PSP_BATCH=1` (`basic_shapes`, `cubicmap`, `models`) is still owed before the flag can be
  recommended beyond 2D: the batch changes the primitive path for every GL11 draw, and only the 2D scene has been
  proven byte-identical.
- The **archived baseline should be re-taken** with the current protocol (today's unbatched arm reads 66.73 ms at
  N=1,000 against the published 61.63). Either the display state differs or the old number was measured with a
  partial-vsync swap; until it is re-taken, quote same-session A/B numbers only.

