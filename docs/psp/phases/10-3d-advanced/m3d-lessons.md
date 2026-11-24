# M3D compared with pspgu — what a direct-to-GE engine does differently

| | |
|---|---|
| Written | 2026-11-24 |
| Phase | [Phase 10 — 3D and advanced](index.md) |
| Subject | `/home/psp/ref/M3D-for-PlayStation-Portable` (mills32, commit `d170fd3`) measured against `raylib` + `tools/pspgu` |
| Harness | `tools/psp-bench.sh`, `psp-tests/samples/psp/psp_showcase`, and two benches added to the M3D tree (`M3D_SAMPLES/20_BENCH_3D`, `21_BENCH_2D`) |
| Status | measurement only — no port code depends on M3D; every claim here has a number or a source line |

M3D is a different architecture on the same hardware: no GL layer, no CPU clipper, no per-object CPU transform — it
bakes geometry once, composes a GUM matrix per object and lets the GE transform. It is therefore a control for the
question this phase keeps asking: **is our cost in the GE or in the path we build around it?** The answer, measured,
is the path.

## 1. What was compared

Same content on both sides, 480×272, frame-indexed animation so frame *N* is identical on every run:

- **3D** — a grid of lit cubes on a sine wave, orbit camera, fog, 2D HUD. Ours: `psp_showcase` (merged clean runs).
  M3D: `20_BENCH_3D`, one `M3D_ModelRender` per cube. Counts 400 (20×20), 800 (20×40), 1,200 (20×60).
- **2D** — glow nebula (1,300 particles) + 250 stars + 160 core + 16 rings + HUD. Ours: `psp_2d_showcase`
  (batcher, 6 draws). M3D: `21_BENCH_2D`, arm 0 = `M3D_DrawSpriteScaleRot` per sprite, arm 1 = OSLib
  `oslDrawImage` per sprite, arm 2 = OSLib with one fixed size (control).

One porting trap, recorded because it invalidated the first 3D capture: `plasmacube.ply` is **3 units across**, so at
our per-instance scale it overlapped into a solid block. Scaling it `s/3` matched the scenes (coverage 36.3 % ours
vs 41.9 % M3D) and *improved* M3D's number (9.54 → 8.48 ms), i.e. the corrected comparison is the harder one.

## 2. The numbers

### 3D, identical content

| cubes | M3D work | M3D avg / fps | ours work (merged) | ours avg / fps |
|---|---|---|---|---|
| 400 | **8.48 ms** | 16.67 / **60.0** | 31.92 ms | 33.44 / 29.8 |
| 720 | 14.34 ms | 16.69 / 59.9 | — | — |
| 760 | 15.13 ms | 16.69 / **59.9** | — | — |
| 780 | 15.53 ms | 16.95 / 59.0 | — | — |
| 800 | 15.93 ms | 17.45 / 57.3 | 54.05 ms | 66.80 / 15.0 |
| 1,200 | **24.07 ms** | 33.34 / **30.0** | 96.11 ms | 107.42 / 9.3 |

- **M3D's 60 fps ceiling is 760 cubes**; first drop at 780.
- Our merged path does not hold 60 fps at 400 in this layout. Its measured ceiling is **270 cubes**, but on the
  `matrix_probe` layout (10 wide, 0.8 spacing) — a different cull profile, so it is *not* the same scene. A matched
  re-sweep is open work (§8 E6).
- Per-cube slope: **M3D ~19 µs**, **ours merged ~55 µs**, ours per-object **~180 µs** (400 cubes: 67.68 ms).
- M3D draws everything — **no CPU cull**. Ours culled 24/400, 236/800, 489/1,200 per frame.

### 2D, identical content

| path | sprites | **draws** | work | avg / fps |
|---|---|---|---|---|
| ours (batcher) | 1,566 | **6** | **20.51 ms** | 33.80 / 29.6 |
| OSLib `oslDrawImage` | 1,726 | 1,726 | 36.87 ms | 99.78 / 10.0 |
| OSLib, fixed 9 px (control) | 1,726 | 1,726 | 34.92 ms | 148.49 / 6.7 |
| `M3D_DrawSpriteScaleRot` | 1,726 | 1,726 | 63.79 ms | 89.08 / 11.2 |

The control matters: holding the sprite state constant did **not** reduce work (34.9 vs 36.9 ms). OSLib's image path
emits one draw per sprite unconditionally (`oslDrawImageSimple.c:42-74`), so the cost is per draw, not per state
change.

### The hard wall we hit on our side

`MERGEPROBE` at 1,200 cubes: **154 replays**. 1,200 × 24 verts × 32 B = 921 KiB of merged vertices against a
512 KiB per-list arena, so the run that does not fit is replayed per object — work jumped 54.05 → 96.11 ms instead
of scaling linearly. The merged path tops out around **~680 cubes per frame** (arena minus the dirty draw and 2D).

## 3. Where the time goes, on our side

From the sprint record (077/078/085), for a per-object `DrawMesh` on the cube scene:

| line | cost |
|---|---|
| per-`DrawMesh` baseline, clip off | ~51 µs/call |
| clip pass per submitted draw | ~47 µs |
| CPU cull | ~13 µs/call |
| compose + `rlMultMatrixf` | ~1.7 + 3.5 µs (1.1 with VFPU) |
| merged clean run | ~55 µs per cube (CPU expansion dominates) |

M3D pays ~19 µs per cube in total, GE transform included. Its per-draw surface is: compose a GUM matrix, set the
texture, `sceGumDrawArray` (`AMG_Model.cpp:1146`). Ours is: `rlPushMatrix` + `rlMultMatrixf` + three
`glEnableClientState` + three `gl*Pointer` + `rlColor4ub` + `glDrawElements` — and then the driver classifies, scans
for clipping, and only then emits.

## 4. What M3D does, with source references

All paths relative to `/home/psp/ref/M3D-for-PlayStation-Portable`.

| technique | where |
|---|---|
| geometry baked once, grouped per material; draw straight from the model buffer | `M3D_LIBS/AMGLib/AMG_Model.cpp:1146-1164` (`sceGuDrawArray(Render_Style, model->Flags, nfaces*3, 0, …)`) |
| vertex format: float32 position + 8888 colour + float32 UV + `GU_TRANSFORM_3D` | `AMG_Model.cpp:161`, `:227` |
| GUM float matrices per object | `AMG_User.cpp:65+`, `M3D_CameraSet` |
| one static display list, refilled every frame | `AMG_User.cpp:44` (`unsigned int list[262144]`), `:86` (`sceGuStart`), `:91-92` (`sceGuFinish(); sceGuSync(0,0);`) |
| frame boundary = finish + full sync + clear | `AMG_3D.cpp:547-555` (`M3D_updateScreen`) |
| **no CPU cull, no CPU clipper** — the GE clips | the draw call has no classification step |
| vertices live in the list's uncached alias via `sceGuGetMemory` | `AMG_Texture.cpp:1152` (sprite vertex block) |
| sprites: one draw per image | `AMG_Texture.cpp:1116-1163`; OSLib `image/oslDrawImageSimple.c:42-74` |
| 8888 framebuffers at fixed VRAM offsets | `AMG_User.cpp:48-50` |
| compressed depth range 50000..10000, `GU_GEQUAL` | `AMG_User.cpp:53-56` |
| linear + mipmap sampling by default | `AMG_Texture.cpp:150` |

**Licence:** that tree is GPLv2. Nothing here is copied; the references are for study only. Any implementation of the
ideas below is written from scratch against our own interfaces.

## 5. Findings

**F1 — GE-side transform beats CPU expansion by ~3× on this GPU.** M3D transforms 24 vertices per cube on the GE for
~19 µs total. Our merged path transforms them on the CPU and costs ~55 µs per cube, even though it removes 99.5 % of
the draws. The GE is not the bottleneck; our use of the CPU is.

**F2 — the per-call surface, not the GE, is our wall.** Our per-object path costs ~180 µs per cube; M3D's costs
~19 µs with the same 12-triangle mesh and the same `sceGuDrawArray`. The difference is rlgl state, the clip scan, and
matrix work — not rasterisation.

**F3 — the clip scan is the largest removable block for clean objects.** ~47 µs per submitted draw. M3D never runs
one: it lets the GE drop primitives at its guard band. Our classification already proves when a whole object is
inside the band — we act on it only when *merging*. Extending it to per-object draws removes most of that 47 µs from
~99 % of draws.

**F4 — the cull earns its 13 µs, but could cost less.** 218/500 culled at N=500, 489/1,200 at the 20×60 layout. Two
`glGetFloatv` plus eight corner transforms per call dominate. A matrix epoch in rlgl and a cached clip-space AABB
would cut it without changing the decision.

**F5 — our merged path has a capacity wall at ~680 cubes/frame.** 512 KiB arena ÷ (24 × 32 B) ≈ 682. Past it the
extension returns 0 and the run is replayed per object (154 replays at 1,200; work 54 → 96 ms). M3D has the same
class of limit — vertices inside a 1 MiB list — but its per-object path never needs the space.

**F6 — our 2D batcher is the right architecture; M3D is the counter-example.** 20.5 ms / 6 draws against 34.9–63.8 ms
/ 1,726 draws, and the fixed-size control proves the cost is per draw, not per state change. Do not model 2D on M3D.

**F7 — their submission model is not why they are fast.** Single list + full `sceGuSync` per frame is the thing this
port removed in Phase 5; their 2D numbers show the GE tail it exposes (avg 99.8 ms with 36.9 ms of work). The 3D win
is the call surface, not the sync.

**F8 — vertex visibility has two correct answers.** They put vertices in the list's uncached alias and need no
writeback; our arenas are cached main RAM, so we write back. Both are correct; ours is bounded by arena size, theirs
by list size. (Our `pspguWriteback` already skips list-alias blocks — `tools/pspgu/pspgu_internal.h:333-351`.)

**F9 — merging is bounded by its own key.** Uniform scale, same mesh/material/texture, no vertex colours, no
skinning. M3D sidesteps this because each object is its own draw. Any "merge everything" design will keep losing to
a thin per-object path on varied content.

**F10 — instrumentation lessons, paid for here.** The capture raw is a 54-byte header then `B,G,R`, bottom-up,
5-6-5 (sprint 080 T3); `make clean` in `tools/pspgu` does not remove objects, so builds must be forced clean and
verified with `psp-nm` or the boot tag; the asset scale must be checked before comparing images.

## 6. Recommendations, ordered by expected effect

**R1 — give per-object draws the "known clean" fast path.** The classification already runs for every mesh; when it
returns *clean*, draw the packed s16 mesh directly (skipping `clipNeeds`), exactly as the merged path does for its
instances. Expected: removes most of the ~47 µs clip cost from ~99 % of submitted draws — the largest single win
available. Verify: `matrix_probe MODE=scene` N=500 work, all gates 0 diff, `PSPGU_NO_CLIP=1` A/B as a control.

**R2 — thin the call surface.** Cache the composed MVP from the cull and reuse it for the draw (no second compose);
add a matrix epoch to rlgl so the two `glGetFloatv` copies disappear; skip `rlEnableTexture` and the pointer setup
when the mesh/texture/colour did not change. Expected: the remainder of F2; ~10–20 µs per call. Verify with
`PSP_MATRIX_PROBE` + `PSP_CALL_PROBE` counters and the pixel gates.

**R3 — do not CPU-expand what the GE can transform.** Keep merging for genuinely clean runs, but treat it as a
bonus, not the architecture: the measurements say a per-object GE-transform draw at ~19 µs beats a CPU-expanded
merged vertex at ~55 µs per cube. Where a set is static, bake world-space geometry once (M3D's model) instead of
re-expanding it every frame.

**R4 — lift or bypass the arena wall.** Either raise `PSP_GU_ARENA_BYTES` (main RAM is plentiful; the ledger is
documented) or adopt the T6a list-local vertex block (`PSPGU_LIST_VERTICES`) so merged vertices come out of the
list's uncached alias. Measure both against the 1,200-cube point where replays appear today.

**R5 — cheapen the cull.** Matrix epoch + a per-mesh clip-space AABB cached per frame; keep the eight-corner test
only when the mesh or the camera actually changed.

**R6 — keep the 2D batcher, and measure 2D changes with it.** It is 1.8–3× faster than both M3D paths on the same
content; the per-sprite draw is the anti-pattern to avoid.

**R7 — keep the harness honest.** `tools/psp-bench.sh` with two runs per point, forced-clean builds, the boot tag
checked, and captures transformed with the documented layout. Every number in this page came from that discipline;
two wrong conclusions earlier in the same session came from skipping one part of it.

## 7. Experiment backlog

| id | change | metric | pass condition | risk |
|---|---|---|---|---|
| E1 | R1 clean fast path for per-object draws | N=500 work | ≥30 % cut; all gates 0 diff | classification must be exact (proof in 078 §0) |
| E2 | R2 matrix epoch + cached MVP | `PSP_MATRIX_PROBE` cull/mult | cull ≤5 µs, mult unchanged | stale matrices if the epoch misses a path |
| E3 | R3 baked geometry for a static set | static N cubes work | ≥40 % cut vs re-expansion | memory; needs a bake step |
| E4 | R4 arena 512 → 768 KiB, and T6a block | 1,200-cube run | replays 0; work scales linearly | list size vs vertices trade-off |
| E5 | R5 cached clip-space AABB | cull µs/call | ≤8 µs | conservative correctness |
| E6 | matched ceiling sweep on one layout | last N at 60 fps | report ours vs 760 | needs both harnesses on 20×60 |
| E7 | sprite count sweep on our batcher | sprites at 60 fps | report | none |
| E8 | 5551 framebuffer re-test at 1,200 | work + colour delta | recorded | parity cost, already measured at 400 |

## 8. Reproducing

M3D side (outside this repo; assets and libraries are in that tree):

```sh
cd /home/psp/ref/M3D-for-PlayStation-Portable/M3D_SAMPLES/20_BENCH_3D
make COLS=20 ROWS=40 \
  INCDIR="../../M3D_LIBS/AMGLib ../../M3D_LIBS/release/include $PSPDEV/psp/sdk/include" \
  LIBDIR="../../M3D_LIBS/release/lib $PSPDEV/psp/sdk/lib" \
  OBJS="main.o /tmp/mpegbase_stub.o"
# the stub only satisfies a missing firmware import; see sprint notes
```

Our side:

```sh
cd psp-tests/samples/psp/psp_showcase && make FRAMES=200 COLS=20 ROWS=40
# or the whole pair through the runner:
PSP_DEVICE_APPROVED=1 ./tools/psp-bench.sh --label m3d-compare
```

The 2D numbers come from `psp-tests/samples/psp/psp_2d_showcase` and `M3D_SAMPLES/21_BENCH_2D` (arms selected with
`MODE=0|1|2`).

## 9. References

- M3D: `M3D_LIBS/AMGLib/AMG_Model.cpp:161,227,1146-1164`; `AMG_User.cpp:44,48-58,86,91-92`; `AMG_3D.cpp:547-555`;
  `AMG_Texture.cpp:150,1116-1163`; `M3D_LIBS/oslibmodv2-master/src/image/oslDrawImageSimple.c:42-74`.
- Ours: `tools/pspgu/pspgu_internal.h` (arena, list cap, T6a, writeback); `tools/pspgu/gl_draw.c` (clip, direct
  path); `src/rmodels.c` (cull, merge); `tools/psp-bench.sh`; `docs/psp/recipes/benchmarking.md`.
- Sprint record: 077 (per-call attribution), 078 (merged clean runs, arena 512 KiB, capture layout),
  080 (2D state scoping, colour gates), 085 (hardware sweep, T6a), 086/087 (list-local vertices, baked geometry
  candidates), 088 (submission models), 089 (vertex memory 2×2).
