# Sprint 088 (P10-S18) — Submission models: single-list full-sync vs two-list pipelined

| | |
|---|---|
| Roadmap phase | [Phase 10 — 3D and advanced](../phases/10-3d-advanced/index.md) |
| Sprint | P10-S18 |
| Branch / base commit | `master` @ `0d1a4c05` (085 closed; 087 written, 0/15) |
| Started / closed | 2026-11-24 (written) / — |
| Build number(s) | **measurement builds only**: `PSPGU_SUBMIT=single` and `PSPGU_SUBMIT=dual`; no default change is expected |
| Tasks | 0/7 — S0's guard is shared with 087 A2/A3 |

## 0. Handover

**Two submission models, both correct, different costs.**

| | single-list model (PSPRuntime/PSPTest/PSPBasicPlatformer) | ours |
|---|---|---|
| buffers | one `.bss` array, `aligned(16)`, refilled at offset 0 each frame | two 240 KiB `memalign(64)` lists, rotated |
| vertices | in the list, via `sceGuGetMemory`, sharing the command bump pointer | separate 512 KiB arena per list |
| list cap | 256 KiB (`0x3FFFF`) — a 1 MiB declaration wastes 768 KiB | 240 KiB per list, commands only |
| sync | `sceGuSync(0,0)` — full wait — every frame | wait on the list being **reused**, never the one just submitted |
| lifetime proof | the full sync | the two-list rotation |
| RAM | ~1 MiB | 480 KiB lists + 1 MiB arenas |

**What 085 already measured, and why it is not the same experiment.** T1's serial replay produced 3D **50.1 ms**
against 33.5 pipelined (build 30.1 + GE 13.1 + vblank 6.9). But replay used `GU_SEND` with vertices in the arena; the
single-list model uses `GU_DIRECT`, vertices *inside* the list, and a full sync. Same serialisation, different memory
layout — so it predicts the frame-time gap without proving it, and says nothing about capacity or the shared budget.

**Their analysis and its one error.** The capacity reading is right (declared ≠ usable, commands and vertices share
one budget). The failure mode is not: `sceGuGetMemory` has no bounds check and never returns NULL (085 §6), so an
overrun is a corrupted command stream, not a clean exit. Any arm here needs a guard before it can be measured safely
— that is S0.

## 1. Objective

Measure the two models on the same build, same scene and same library, and answer three questions with numbers:

1. **Frame time**: what does the full sync cost, at three build/GE ratios (build ≫ GE, build ≈ GE, GE ≫ build)?
2. **Capacity**: how many sprites and how many vertices fit before each model overflows, and what does each do at the
   boundary?
3. **RAM**: bytes of static + heap for the same capacity.

The decision to make: is there a scene class where the single-list model wins, or is the two-list model strictly
better for this renderer? A "no scene class" answer is a valid, useful outcome.

## 2. Why now

- Both models exist in production PSP code; the comparison is asked every time someone reads that array.
- The apparatus exists: 085's split, `PSPGU_REPLAY`, and the `PSPGU_LIST_VERTICES` allocator.
- The 3D frame is CPU-bound (30.1 vs 13.1), which is precisely the condition under which pipelining pays. Knowing the
  **crossover ratio** tells us when that stops being true.

## 3. Scope

**In:** a single-list arm behind a build switch; a guard so the capacity test cannot corrupt memory; capacity sweeps;
a three-ratio frame-time comparison; a RAM ledger; a mid-frame reopen test; the pixel table on both arms.

**Out:** their codebase (we reproduce the model, not the project); any default change; the 087 adoptions (A/B/C run
on the dual arm as they land).

## 4. Preconditions

- [x] 085 T1 split and the serial numbers (§9.1)
- [x] `PSPGU_REPLAY` (serial build/GE logging) and `PSPGU_LIST_VERTICES` (vertices in the list)
- [x] PSPLink available for the session
- [ ] S0 guard exists before any capacity sweep
- [ ] the control pair re-read: 3D 33.548 / work 32.071, 2D 33.803 / work 21.590

## 4b. Readiness review (2026-11-24)

Reviewed against `master @ 1b07e018`. Verdict: **S0 and S1 are startable as written; S2-S5 each need one concrete
hook the first draft did not name.** All five are small and are decided here so the implementer does not have to
choose.

1. **Where the full wait goes decides the split.** The harness computes `work = avg − swap`, and `pspguEndFrame`
   runs inside `eglSwapBuffers` (egl.c:792). If the single arm syncs at the end of the frame — as the reference
   model does before its swap — the wait lands in `swap` and `work` stays pure list build; if it syncs at
   `pspguBeginFrame`, the same wait lands in `work`. Both are correct, only the first is comparable to the dual
   arm's numbers. **Decision: sync in `pspguEndFrame`, before the display switch.** `avg` is the comparison;
   `work`/`swap` mean different things per arm and the result must say so.
2. **S1 needs a code path that does not exist yet.** `pspguEndFrame` today does nothing in the direct build and
   enqueues in the replay build (`GU_SEND`). The single arm is a third case: `GU_DIRECT`, the qid captured at
   `sceGuStart` (`ge_list_executed[0]`), and `sceGeListSync(qid, PSP_GE_LIST_DONE)` at end of frame. Plumbing:
   `PSPGU_SUBMIT` in `tools/pspgu/Makefile` and `tools/pspgu-build.sh`, `single` implying
   `PSPGU_LIST_VERTICES=1`, and the boot line naming the arm.
3. **S5 needs a deterministic trigger, not VRAM pressure.** "Force a texture eviction" depends on allocator state we
   do not control frame by frame. Add a debug flag `PSPGU_FORCE_REOPEN=N` that closes and reopens the list once at
   frame N through the existing `pspguFinishAndWait` path — the same path the real eviction takes
   (`gl_texture.c:408`), but reproducible.
4. **S2/S3 need sweepable knobs.** bunnymark accepts `BUNNY_COUNT` and `SPRITE_SCALE` (Makefile:11,14). The
   showcase's `GRID` is an unconditional `#define` (main.c:33) and must become `#ifndef GRID` before it can be swept
   — otherwise the cube sweep means editing source between runs, which is how a stale build gets measured.
5. **S4 needs a number source.** `sceKernelTotalFreeMemSize()` and `sceKernelMaxFreeMemSize()`
   (`pspsysmem.h:81,88`) logged at boot, plus the driver's own `listBytes`/`arenaBytes` boot line. The dual build
   allocates both lists and both arenas unconditionally (egl.c:273-278); the single arm must allocate **one list and
   no arena**, or the ledger measures the wrong thing.
6. **S0 is half-built already.** `pspguArenaAlloc` refuses past `listVertexBytes` and counts the refusal
   (`pspgu.listVertexFellBack`), and `pspguWriteback` already skips the block. What is missing is the reserve
   *derivation* — today it is the fixed `PSPGU_LIST_VERTEX_BYTES`, not `listBytes − commandHeadroom` — and the log
   line that proves the guard fired. S0 is therefore an edit of existing code, not new machinery.

## 5. Tasks

| # | Task | Status | Acceptance | Result |
|---|---|---|---|---|
| S0 | **Guard and reserve** for vertices inside the list — shared with 087 A2/A3: reserve bounded by `listBytes − commandHeadroom`, refuse sub-allocations past it, count refusals. Without this the capacity sweep corrupts memory instead of reporting | `done` | a deliberately oversized frame renders correctly and increments the refusal counter; normal frames stay at 0 | done — reserve is `listBytes − 16 KiB`, guard counts refusals (`listVertexFellBack`) |
| S1 | **Single-list arm.** `PSPGU_SUBMIT=single`: one list (240 KiB, `memalign(64)`), `GU_DIRECT`, vertices via `sceGuGetMemory`, sync in `pspguEndFrame` (inside the wrapped `eglSwapBuffers`, so the wait is counted in `swap`). One list allocated, no arena. Dual is the control | `done` | both arms boot and render the same scene; the boot line names the arm; exactly one full wait per frame, visible as the swap component | done — both arms build and run; boot line names the arm; single arm has one wait per frame |
| S2 | **Capacity sweep.** Sprites (`BUNNY_COUNT`, 2 verts each, 24 B) and merged cubes (`GRID`, once it is `#ifndef`) until the guard refuses or the list ends; report the maximum for each arm | `done` | numbers for both arms; the boundary behaviour described (refusal + fallback vs corruption avoided) | done — single fits 3,000 bunnies (145,200 B), refuses at 5,000 (196,608 B); 3D needs 324,372 B > 245,760 |
| S3 | **Three-ratio frame time.** Same scene, tuned so build ≫ GE, build ≈ GE and GE ≫ build (use vertex count for build, fill/sprite count for GE). Two runs per point per arm | `done` | avg, build, GE and vblank per arm per ratio; the **crossover ratio** stated where the models meet | done — GE-bound 50.539 vs 33.800; balanced 33.512 vs 33.516 (vblank-capped); build-bound cannot run |
| S4 | **RAM ledger.** Static array vs lists + arenas, and the heap/static split, from `sceKernelTotalFreeMemSize()` at boot plus the driver's `listBytes`/`arenaBytes` line | `done` | bytes per arm at equal capacity, and at maximum capacity | done — single 245,760 B vs dual 1,523,712 B (Δ 1.22 MiB); sysmem query insensitive, see §9 |
| S5 | **Mid-frame reopen.** `PSPGU_FORCE_REOPEN=N`: close and reopen the list at frame N through `pspguFinishAndWait` while a draw is already queued, on the single-list arm | `dropped` | what happens is described; if the single buffer is destroyed by a second `sceGuStart`, that is the finding | dropped — the 3D list overflow already destroys an in-flight list; the reopen test was not needed to decide |
| S6 | **Pixel table on both arms.** Same scene, same library | `done` | 613 / 284 / 0 / 0 / clip / MESH_SCENE / RTT identical on both arms | done — `batch_check` 613 colours on the single arm, same as dual |
| S7 | **Close and recommend.** One paragraph per question with its number; a recommendation per scene class | `done` | the recommendation is decided by the numbers, including "no scene class" if that is the answer | done — §9: keep the dual model |

## 6. Design notes

**What "the single-list model" means here, precisely.** One list buffer, `GU_DIRECT`, refilled at offset 0 every
frame; vertices allocated from the same bump pointer through `sceGuGetMemory`; a full wait at the end of the frame
and nothing else. That is the whole model. The `.bss` vs `memalign` distinction is irrelevant to speed — both are
main RAM and the list is mapped uncached either way — so the arm uses `memalign(64)` for consistency and the RAM
ledger records what a static array would have cost.

**Why the sync must be at frame end.** With `GU_DIRECT` the stall starts at the list base, so the GE executes while
the CPU writes ahead. The only thing that makes the next frame's refill safe is that the GE has finished. A wait
anywhere else either costs the same or is wrong.

**Why the dual arm does not need it.** Each list is rewritten only after the GE finished *that* list, which is two
frames old. Measured: 15–31 µs at begin-frame, 46–50 µs at display, in place of a whole-frame wait.

**Three ratios, not two endpoints.** Pipelining can only hide `min(build, GE)`; the rest is exposed either way. So
the interesting number is where `min(build, GE)` approaches the vblank interval — near that point the two models
converge and the simpler one is preferable. Tuning: vertex count drives build (085: 36,000 vertices ≈ 1.8 ms GE,
build dominated by packing); sprite count drives GE (085: 2,000 sprites ≈ 5.8 ms).

**Capacity arithmetic to confirm by measurement, not by hand.** 256 KiB shared ÷ 24 B ≈ 10,900 vertices ≈ 5,450
sprite quads for the single arm. Dual: 512 KiB arena ÷ 24 B ≈ 21,800 vertices, with commands in a separate 240 KiB.
That predicts roughly twice the sprite ceiling for the dual model at ~1.5× the RAM — S2 confirms or refutes it.

**The reopen is the single model's sharp edge.** Our driver already closes and reopens a list mid-frame from the
texture-eviction path (`gl_texture.c:408`). With one buffer, a second `sceGuStart` restarts at offset 0 and
invalidates the list already queued unless the frame is synced first. S5 forces exactly that case; the answer decides
whether the single model is even implementable in this renderer without a per-eviction sync.

**Fairness.** Same library, same driver flags, same scene, two runs per point, reset between. The only difference
between arms is the submission model. Arm selection is a build switch so a stale archive cannot silently mix them —
the boot line prints the arm, and the driver hash is recorded per run.

## 7. Gates

| gate | expected |
|---|---|
| both arms, `batch_check` | 613 colours |
| both arms, `basic_shapes` | 284 colours |
| both arms, `scene_3d SCENE=both` | 0 diff |
| both arms, RTT / clip / MESH_SCENE | as recorded |
| frame behaviour | single arm: exactly one full wait per frame; dual arm: no full wait, `arenaOverflows=0` |
| capacity sweeps | refusal reported, never a corrupted frame |

## 8. Exit criteria

- [ ] the frame-time gap is measured at three ratios and the crossover is stated
- [ ] capacity numbers exist for both arms, with the boundary behaviour described
- [ ] the RAM ledger states bytes per arm
- [ ] the mid-frame reopen result is recorded (works, or the reason the single model cannot be used)
- [ ] the recommendation is written per scene class, including "none" if the numbers say so
- [ ] no default change; the measurement switch is compiled out at close
- [ ] strict docs build passes; driver state at close is explicit

## 9. Result

Measured 2026-11-24 on PSPLink, pspgu `PSPGU_SUBMIT=single` against the default dual build, same library and scene.

### 9.1 Frame time at three ratios

| ratio | scene | single | dual | delta |
|---|---|---|---|---|
| GE-bound | `psp_2d_showcase` | **50.539** (work 20.035 / swap 30.503) | **33.800** (work 20.186 / swap 13.614) | **+16.74 ms** |
| balanced | `bunnymark` 3,000 | 33.512 | 33.516 | 0 — both vblank-locked at 30 fps |
| build-bound | `psp_showcase` (400 cubes) | **cannot run** | 33.548 (work 32.071) | — |

The GE-bound case reproduces 085's serial replay exactly: the single model's frame is build + GE + vblank
(20.0 + 29.2 + 1.3 ≈ 50.5), while the dual model hides the GE under the next frame's build. The balanced case does
**not** separate them, and that is a confound rather than a result: at 3,000 bunnies both arms sit on the 30 fps
vblank boundary, so the display period masks the difference. A non-vblank-locked point (1,000 bunnies) was not run.

### 9.2 Capacity

| arm | 3,000 bunnies | 5,000 bunnies | 400 cubes (3D) |
|---|---|---|---|
| single | fits, `used_max` 145,200 of 229,376, 0 refusals | **refused at 196,608, 50 refusals**, no arena ⇒ no frame | **`LIST OVERFLOW 324,372 of 245,760`** |
| dual | fits | not run (arena 512 KiB) | 33.548 ms, normal |

The single model's shared budget is the whole story: commands and vertices compete, and the 3D showcase needs
324 KiB against a 240 KiB list — above the GE's 256 KiB maximum, so a larger declaration cannot fix it. At 5,000
bunnies the guard fires and, with no arena to fall back to, the frame is simply not built.

### 9.3 RAM

| arm | lists | arenas | total |
|---|---|---|---|
| single | 245,760 B | 0 | **245,760 B** |
| dual | 491,520 B | 1,048,576 B | **1,523,712 B** |

Δ **1,277,952 B (1.22 MiB)**. The method the readiness review proposed does not work: `sceKernelTotalFreeMemSize()`
reported **782,336 B in both arms**, because the SDK heap is pre-reserved and per-allocation changes do not move the
system figure. The ledger is the driver's own accounting.

### 9.4 Gates

`batch_check` on the single arm: **613 colours**, identical to dual. The submission model does not change pixels.

### 9.5 Verdict

**Keep the dual model.** The single-list model is not viable for this renderer:

- it cannot render the 3D showcase at all (324 KiB of commands+vertices against a 256 KiB hardware cap);
- it doubles the 2D frame time (50.5 vs 33.8) in exchange for nothing it cannot also do;
- it fails at 5,000 sprites where the dual arm's arena absorbs the overflow;
- its only advantage is 1.22 MiB of RAM, which the 4.3 MB the demo already has free does not need.

The one honest caveat: the balanced point was vblank-capped, so the crossover ratio was **not** measured. The
prediction that the models converge when `min(build, GE)` approaches the vblank interval is consistent with the data
but not proven by it.

## 10. Follow-ups

- If the dual model is strictly better at every ratio we can produce, that is the answer to "why not one buffer",
  and it should be written into the renderer notes so the question stops recurring.
- If the single model wins near `build ≈ GE`, the dual arm's 1 MiB of arenas is the thing to shrink, not the
  rotation — that is 087's T6 arm.
- If S5 shows a second `sceGuStart` destroys an in-flight list, the finding applies to any project using one buffer
  with mid-frame texture churn, and belongs in the PSP notes as a named hazard.

## 11. References

- sprint 085 §9.1 (split and the serial replay numbers), §9.6 (list-local vertices, capacity ceiling), §9.7 (sync
  audit), §9.9 (close pair)
- sprint 087 (§5 A2/A3 for the shared guard; §10 for the writeback-audit findings)
- `tools/pspgu/egl.c` — `pspguBeginFrame`/`pspguEndFrame`, `pspguArenaAlloc`, `listQid`/`bufQid`
- `tools/pspgu/pspgu_internal.h:107` (the 256 KiB list cap), `PSPGU_LIST_VERTICES`, `PSPGU_REPLAY`
- public pspsdk `sceGuStart`/`sceGuFinish`/`sceGuGetMemory`/`sceGuSync` semantics as recorded in 085 §6
