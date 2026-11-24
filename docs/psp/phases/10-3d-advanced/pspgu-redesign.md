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

## 4. Phases and gates

Each phase is its own sprint; each can stop the programme.

| phase | change | expected | gate | kill rule |
|---|---|---|---|---|
| P10-S20 | **clean hint**: `DrawMesh` passes the classifier's answer; the driver skips `clipNeeds` for a clean per-object draw | per cube 180 → ~130 µs; 400 cubes 67.7 → ~52 ms work | 0 diff on `scene_3d`, `MESH_SCENE`, `batch_check`; `PSPGU_NO_CLIP=1` as the control arm | slope falls <20 % ⇒ the decomposition is wrong, stop |
| P10-S21 | **matrix epoch**: cache the MVP from the cull, reuse it for the draw, skip unchanged pointer/texture setup | −10–20 µs/cube | gates 0 diff; `PSP_MATRIX_PROBE` cull ≤8 µs | no measurable change ⇒ the surface was already thin |
| P10-S22 | **baked mesh path**: load-time bake, `pspguDrawBakedMesh`, no packing/arena/writeback | per cube ~130 → ~40 µs | 0 diff; memory ledger; 1,200-cube frame with 0 replays | >60 µs/cube ⇒ raylib's surface dominates and the path belongs lower |
| P10-S23 | **merge re-decision**: A/B thin per-object against merged at 400/800/1,200 | one of them wins clearly | pair + gates | if merged still wins, keep it and record why |

Every phase uses the same harness: `psp-bench.sh`, two runs per point, forced-clean builds, boot tag checked, and
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

## 7. Relationship to the open sprints

- **087 A (list-local vertices)** — superseded by 089. Keep only its diagnosis; the arena stays.
- **087 B (index-run cache)** — unchanged and still worth ≈0.45 ms/frame. It applies to the current path and to the
  merged path; the thin path has no per-frame index copy at all, which is another argument for it.
- **087 C (baked geometry)** — this design is the concrete form of it; P10-S22 and 087 C should be merged into one
  sprint rather than run twice.

## 8. References

- [M3D compared](m3d-lessons.md) — the comparison, findings F1–F10, recommendations R1–R7
- `src/rmodels.c:1447-1620` — the classifier, its guard-band margins, `clean` vs `dirty`
- `tools/pspgu/gl_draw.c:1417` — the clip scan on every per-object triangle draw
- sprints 077 (per-call attribution), 078 (merge, arena wall, capture layout), 085 (T1/T6a), 088 (submission),
  089 (vertex memory)
- `/home/psp/ref/M3D-for-PlayStation-Portable/M3D_LIBS/AMGLib/AMG_Model.cpp:1146-1164` (study only, GPLv2, no code
  copied)
