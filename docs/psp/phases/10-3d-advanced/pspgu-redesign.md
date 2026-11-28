# Redesigning the 3D path: thin GE transforms instead of CPU expansion

| | |
|---|---|
| Written | 2026-11-24 |
| Phase | [Phase 10 — 3D and advanced](index.md) |
| Basis | [M3D compared](m3d-lessons.md) (same hardware, same content), sprints 078, 085, 088, 089 |
| Status | design proposal — no code; each phase is gated on a measurement |

## 0. The case for change

On identical content, one engine does the same work in a quarter of the time:

| 400 lit cubes, 480×272 | work | fps |
|---|---|---|
| M3D (one `sceGuDrawArray` per cube, GE transform) | **8.48 ms** | 60.0 |
| ours, merged clean runs (2 draws) | 31.92 ms | 29.8 |
| ours, per-object (`PSP_MERGE_DRAWS=0`) | 67.68 ms | — |

Per cube: **19 µs** against **55 µs** merged and **180 µs** per object. M3D runs no CPU cull and no CPU
clipper — the GE clips — and its geometry is baked once. We run a clip scan, a cull, a matrix push and a
per-frame vertex expansion, and we still lose by 3×.

Our own decomposition of the per-object path (077/078) says where it goes:

| line | cost |
|---|---|
| per-`DrawMesh` baseline | ~51 µs |
| clip scan | ~47 µs |
| CPU cull | ~13 µs |
| matrix compose + multiply | ~5 µs |
| remainder: packing into the arena, writeback, GL surface | ~64 µs |

The GE is not the wall. The path we build around it is.

## 1. What is not broken

Three decisions were measured in this phase and must survive the redesign:

- **Submission: two rotating lists, targeted waits.** 088 rejected the single-list full-sync model by
  **+16.7 ms on 2D** and **+8.3 ms at N=196**. M3D's own 2D numbers show the same tail (36.9 ms work, 99.8 ms
  average).
- **Vertex memory: a separate arena.** 089 measured list-local vertices at **+1.2 ms** (reserve) and **+5.7 ms**
  (per draw) against the arena at N=196. This overrides M3D-lessons R4's suggestion to adopt `PSPGU_LIST_VERTICES`
  for merged runs: raise the arena instead; do not move vertices into the list.
- **2D: the batcher.** 6 draws at 20.5 ms against 1,726 draws at 34.9–63.8 ms, with a fixed-state control proving
  the cost is per draw. The batcher is the architecture; per-sprite draws are the anti-pattern.

So the redesign is **3D only**, and it touches the layer above the driver more than the driver itself.

## 2. The three costs that are ours

1. **The clip scan runs even when the object is already proven clean.** `pspGeClassifyMesh` (rmodels.c:1501) returns
   *clean* when every corner is inside 7.9 of the GE's 8.0 guard band. The merged path uses that answer; the
   per-object path does not — `clipNeeds` still runs at `gl_draw.c:1417` for every `GU_TRIANGLES` draw. That is
   ~47 µs spent to re-derive a fact the classifier already established.
2. **The call surface re-does work per draw.** Two `glGetFloatv` copies feed the cull, the composed MVP is thrown
   away, then the matrix is pushed again; pointer setup and texture enable run whether or not anything changed.
3. **Vertices are expanded on the CPU every frame.** The merge path rewrites world-space vertices per instance; the
   non-merged path packs each draw into the arena. M3D keeps object-space vertices and lets the GE transform them.

## 3. The redesign

### 3.1 Layering

```
raylib DrawMesh
   ├── baked mesh?  → pspguDrawBakedMesh(mesh, matrix, material)     new, thin
   └── otherwise    → current rlgl path (unchanged, with the clean hint plumbed)
```

The thin path exists for **static meshes** — the majority of 3D content. Dynamic geometry keeps the current path,
which already works and is gated.

### 3.2 The baked mesh

Built once, at load or on first draw, into main RAM and written back once:

```c
typedef struct {
    const void    *verts;      // interleaved object-space, GU-native format (f32 or s16 + scale)
    const unsigned short *idx; // optional; NULL for a plain triangle list
    int            count;      // indices or vertices
    unsigned int   fmt;        // GU_* format word
    int            baked;
} PspGuBakedMesh;
```

- Object-space, **not** world-space: the per-object transform stays a matrix, so one buffer serves every instance.
- Written back once (`sceKernelDcacheWritebackRange`) and never touched again — the load-time discipline the SDK
  samples use, and the invariant 087 §10 records as unasserted today.
- 32 B per vertex in the f32 form we already produce (position 3f, colour 8888, uv 2f), so a cube is 24 × 32 B.

### 3.3 The draw entry

```c
void pspguDrawBakedMesh(const PspGuBakedMesh *m, const float *mvp, unsigned int flags);
```

Sequence: apply material/texture state **only if it changed**, `sceGumLoadMatrix(mvp)`, one
`sceGuDrawArray(GU_TRIANGLES, m->fmt, m->count, m->idx, m->verts)`. No classification, no clip scan, no packing, no
arena, no writeback.

Two rules make it safe:

- **The clean hint is the license to skip the clip scan.** Callers pass the classifier's answer. An object that is
  not proven clean keeps the current path. This is not "remove clipping"; it is "believe the proof we already made".
- **The guard band is much larger than the screen.** The GE drops a primitive only when it falls outside the 4096-px
  window; a cube at the screen edge rasterises and the scissor handles it. The classifier's 7.9/8.0 margin is what
  makes that true, which is why the hint, not a blanket removal, is the mechanism.

### 3.4 What happens to merging

Merging stays compiled and gated, but stops being the architecture. It is re-measured against the thin path at the
end (P10-S23) and kept only where it still wins. The expectation, stated so it can be refuted: once a per-cube draw
costs ~35–45 µs, per-object at 400 cubes lands near 14–18 ms of work, beating the 31.92 ms merged path — and it
has no arena wall, no merge key, and no replays (154 at 1,200 cubes today).

### 3.5 What does not change

The GE setup, the framebuffer rotation, the submission model, the arena for dynamic draws, the 2D batcher, the
pixel gates, and every number in 088/089.

### 3.6 Matrix upload: not GUM

The obvious reading of "M3D uses GUM matrices" is wrong for the hot path. Its per-object transform is
`sceGuSetMatrix` from its own VFPU matrix system (`AMG_VFPU.cpp:109`), followed by a non-indexed
`sceGuDrawArray` straight out of the model buffer (`AMG_Model.cpp:1148`). `sceGumDrawArray` appears only in a few
paths (a preview at `:161`, a shadow pass at `:2180`, sprites and particles).

Our driver already uploads the same way: `pspguUploadMatrices` (`gl_matrix.c:112-116`) sends `GU_PROJECTION` and
`GU_MODEL` through `sceGuSetMatrix`, with a dirty check that skips an unchanged matrix (`gl_matrix.c:57-63`). So the
mechanism is already the cheap one, and P10-S8 had already measured GUM's stack and dropped it (camera cache 13×,
GUM not used).

Reintroducing GUM for the thin path would add its CPU-side stack — push, multiply, load — to compute a product we
already have from the cull (P10-S21). There is nothing for it to do.

**The question that does survive** is whether the per-object upload is worth avoiding at all:

| option | cost | valid when |
|---|---|---|
| `sceGuSetMatrix(GU_MODEL, mv)` per object (ours, M3D's) | 64 B + a few commands per object, ≈1 µs/cube at 400 | always |
| world-space bake, no matrix at all | 0 upload, but one buffer per instance (400 × 24 × 32 B = 307 KiB) | the set is truly static |
| GUM stack | CPU compose + upload | never, at this layer |

So P10-S22 carries a second arm: **bake world-space for a static set** and measure it against the per-object
matrix. That is "merging done once at load" rather than per frame, and it is the only variant that removes the
upload entirely. It cannot serve the animated cube grid — those matrices change every frame — which is why it is an
arm, not the design.

## 4. Sprints, and how many

**Five sprints, about thirty tasks.** Each is one change → device measurement → pixel gate cycle, and each has a
kill rule, because the design only works if every step is attributable. The count is deliberately not one sprint:
this phase has already paid twice for measuring two variables at once (088's single arm moved submission *and*
vertex storage; 089 exists because of it).

| sprint | scope | tasks (est.) | needs | killed by |
|---|---|---|---|---|
| [P10-S20](../../sprints/090-phase10-clean-draw-fast-path.md) | matched baseline on one layout (M3D-lessons E6) + **clean hint**: `DrawMesh` passes the classifier's answer, the driver skips `clipNeeds` for a clean per-object draw | 5 | — | clip is <20 % of the slope |
| [P10-S21](../../sprints/091-phase10-thin-call-surface.md) | **thin call surface**: matrix epoch, cached MVP reused for the draw, skip pointer/texture setup when unchanged | 6 | S20 (for the baseline) | no measurable change |
| [P10-S22](../../sprints/092-phase10-baked-mesh-path.md) | **object-space baked mesh** + `pspguDrawBakedMesh` (§3.6 `sceGuSetMatrix`), loader/bake integration, memory ledger | 9 | S21 | >60 µs/cube |
| [P10-S23](../../sprints/093-phase10-static-bake-and-merge-decision.md) | **world-space bake arm** for static sets + **merge re-decision** at 400/800/1,200 | 6 | S22 | neither path wins by >0.05 ms |
| [P10-S24](../../sprints/094-phase10-thin-3d-close.md) | **close**: matched ceiling sweep, arena/replay check, full gates, docs, default state | 4 | S23 | any gate not 0 diff |

Total ≈ **30 tasks**. Three of the five are conditional: S21-S24 happen only if the previous phase's number holds, so
a failed assumption costs one sprint, not five.

### 4.1 The shortest path that still tests the design

S21 is an optimisation, not a prerequisite: the baked path can be built on top of the current surface. So if the
question is "does the thin architecture work at all", the minimum is **three sprints** — S20 (know the slope and
remove the clip scan), S22 (bake and draw thin), S23 (decide against merging). S21 and S24 are what turn a working
prototype into an adopted default.

| if | sprints | expected outcome |
|---|---|---|
| everything holds | 5 (~30 tasks) | thin per-object is the 3D architecture; merge is measured and kept only where it wins |
| S21 shows nothing | 4 (~24) | skip the call-surface work, keep the baked path |
| S22 fails (>60 µs/cube) | 2 (~14) | the gap to M3D is raylib's scene layer; stop and record |
| S20 fails (<20 % from the clip) | 1 (~5) | the decomposition was wrong; re-attribute before any redesign |

### 4.2 What is not in that count

- **087 B (index-run cache)** — postponed. The thin path has no per-frame index copy for static meshes, so the
  0.45 ms it targets shrinks to dynamic draws; re-estimate after S22.
- **087 C (baked geometry)** — merged into S22/S23, not run twice.
- **2D** — untouched. M3D-lessons F6 says the batcher is already the right architecture.
- **Arena sizing / replays** — handled in S24, because a thin per-object path has no per-frame expansion to run out
  of room.

Every sprint uses the same harness: `psp-bench.sh`, two runs per point, forced-clean builds, boot tag checked, and
the pixel table on the arm it proposes to adopt.

## 5. Expected end state, and how it would be falsified

- 400 cubes at 60 fps, work ≈16–20 ms against today's 31.92 ms merged and 8.48 ms for M3D.
- The 60 fps ceiling moves from ~270 (merged, arena-bound) to a draw-surface-bound number to be measured.
- 1,200 cubes no longer replays per object, because there is no per-frame expansion to run out of room.

Falsified if: the clip scan is not the dominant term (P10-S20), or a baked mesh still costs >60 µs/cube
(P10-S22). Either result is worth recording — it would say the difference to M3D is raylib's scene layer, not our
driver, and the next redesign belongs there.

## 6. Risks

- **Two vertex copies per static mesh** (raylib's arrays plus the baked buffer) unless the source is released. The
  memory ledger decides; 32 B/vertex in main RAM is affordable for the sample set and must be stated.
- **The GL contract**: `Mesh.vertices` is read by gameplay code (collision), so baking must not move it. The baked
  buffer is additive.
- **Shared rlgl code**: the epoch and the thin path must stay inside `PLATFORM_PSP` guards so desktop builds are
  untouched.
- **The clean hint must be exact.** 078 §0 has the proof obligation; a wrong hint drops primitives silently, and
  the pixel gates are the check.

## 7. What happens to the open sprint (087)

087 was written before 088-089 ran; two of its three workstreams are now settled. The disposition:

| 087 workstream | disposition |
|---|---|
| **A — list-local vertices** | **closed.** 089 measured it at +1.2 ms (reserve) and +5.7 ms (per draw) against the arena; the arena stays. Keep only the diagnosis. |
| **B — index-run cache** | **postponed to after P10-S22.** Still ≈0.45 ms/frame on the current path, but the thin path has no per-frame index copy for static meshes, so its value shrinks to dynamic draws. Re-estimate with S22's numbers rather than building it now. |
| **C — baked geometry** | **absorbed.** S22 (object-space bake) and S23's world-space arm are its concrete form; do not write it twice. |

087 should be rewritten as a short closure note (A closed, B deferred with its number, C absorbed) rather than
implemented as written.

## 8. Decision (2026-11-28): merge-first, per-object opt-in

> **Outcome, 2026-11-28 (098).** The decision stands and the merged path got stronger: handing the merge buffer to the
> driver instead of copying it into the arena moved the 60 fps ceiling from **270 to 290** cubes and the 30 fps ceiling
> from **610 to 700**, with work at 270 instances falling from 15.81 to 14.55 ms (gates 613 / 284 / checksum, driver
> `b38`). 093 is closed unexecuted and 094 as a decision — the thin entry stays compiled and opt-in behind
> `PSP_MERGE_DRAWS=0`, at ~95 µs/call, for a scene that is call-bound rather than instance-bound.

The redesign was written on M3D's evidence that a thin per-object GE-transform draw beats CPU expansion. Three
sprints later that has not happened, and the arithmetic is now explicit:

| path | cost |
|---|---|
| merged, shipped default | ~55 µs/cube (32.6 ms work at 400 cubes) |
| per-object after 090 + 092 + 091 T4b | ~95 µs/call (111 under the probe) |
| per-object if *all* remaining surface were removed | ~63 µs/call |

M3D does the same content at 19 µs/cube, so the model is right and our call surface is the defect — but closing that
gap needs a rewrite of `DrawMesh`'s per-call path, not another micro-optimisation, and its ceiling is "competitive
with merging", not "clearly better".

**Decision:** merging remains the 3D architecture. The per-object work of 090/092/T4b stays in the tree as an
**opt-in** path (default-off), because it is the only path that can draw content the merge key rejects (varied
material, no uniform scale, vertex colours).

**Reversal rule.** One measurement remains: a breakdown of the ~48 µs that no counter explains. If **≥30 µs of it is
removable raylib surface**, a thin-entry sprint could land per-object near 50–60 µs/cube and the decision reverses.
If not, the redesign is closed with that number.

The next sprint is therefore not S21's continuation but **sprint 096**, on the merge path: the index-run cache
(0.45 ms/frame, 085 T3b), strips for static meshes (−58 % GE at 0 px, 085 T3), and the merge expansion cost itself.

## 9. References

- [M3D compared](m3d-lessons.md) — the comparison, findings F1–F10, recommendations R1–R7
- `src/rmodels.c:1447-1620` — the classifier, its guard-band margins, `clean` vs `dirty`
- `tools/pspgu/gl_draw.c:1417` — the clip scan on every per-object triangle draw
- sprints 077 (per-call attribution), 078 (merge, arena wall, capture layout), 085 (T1/T6a), 088 (submission),
  089 (vertex memory)
- `/home/psp/ref/M3D-for-PlayStation-Portable/M3D_LIBS/AMGLib/AMG_Model.cpp:1146-1164` (study only, GPLv2, no code
  copied)
