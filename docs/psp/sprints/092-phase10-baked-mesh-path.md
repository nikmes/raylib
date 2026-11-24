# Sprint 092 (P10-S22) — Object-space baked meshes and a thin draw entry

| | |
|---|---|
| Roadmap phase | [Phase 10 — 3D and advanced](../phases/10-3d-advanced/index.md) |
| Sprint | P10-S22 |
| Branch / base commit | `master` @ `fe1171fe` — design: [Redesigning the 3D path](../phases/10-3d-advanced/pspgu-redesign.md) |
| Started / closed | 2026-11-24 (written) / — |
| Build number(s) | pspgu `b38` only if adopted; the arm is `PSPGU_BAKED_MESH=1` |
| Tasks | 0/9 |

## 0. Handover

S20 and S21 thin the existing path. This sprint removes the remaining per-frame work entirely: geometry is baked once
into a GU-native buffer and drawn with one call, the way M3D does it — `sceGuSetMatrix(GU_MODEL, m)` then
`sceGuDrawArray(..., nfaces*3, 0, modelData)` (`AMG_VFPU.cpp:109`, `AMG_Model.cpp:1148`).

Our driver already uploads through `sceGuSetMatrix` (`gl_matrix.c:112-116`), so the draw entry has no new mechanism to
invent. What it removes is the per-draw packing into the arena, the range writeback, the clip scan, and the arena
wall (~680 cubes per frame today, 154 replays at 1,200 — M3D-lessons F5).

Target: **180 → ~40 µs per cube**, at which point per-object at 400 cubes lands near 14–18 ms of work against the
31.92 ms merged path.

## 1. Objective

Bake static meshes once, draw them thin, and prove it on the same content M3D was measured on. Adopt only with 0
pixel diff and a stated memory ledger.

## 2. Why now

It is the architectural change the redesign exists for. S20 makes the clip skip available; S21 makes the MVP
trustworthy; with both in place the bake is the only remaining piece. Every alternative has been measured and lost:
merging (31.92 ms), per-object with the old path (67.68 ms), list-local vertices (089), single-list submission (088).

## 3. Scope

**In:** the baked-mesh object and its bake function; `pspguDrawBakedMesh`; raylib integration and lifetime; the memory
ledger; the A/B; the capacity check at 1,200 cubes; the gates.

**Out:** the world-space bake and the merge re-decision (S23); s16 positions (a later arm — start with the f32 format
we already pack); 2D; submission.

## 4. Preconditions

- [x] S20's clean hint exists (the thin draw skips the scan on the same proof)
- [x] S21's cached MVP exists, or T2 composes one per draw and states it
- [x] the 32-byte interleaved format is what the driver already packs (`gl_draw.c` layout, stride 32)
- [ ] a bake invalidation path exists for `UnloadMesh` before any adoption

## 5. Tasks

| # | Task | Status | Acceptance | Result |
|---|---|---|---|---|
| T1 | **Baked mesh object + bake.** Separate raylib arrays → one interleaved object-space buffer, GU format word, optional index array, written back once | `todo` | baked bytes match the source geometry; one writeback; no per-frame touch | — |
| T2 | **`pspguDrawBakedMesh(mesh, mvp, flags)`.** Material/texture state only on change, `sceGuSetMatrix(GU_MODEL, mvp)`, one `sceGuDrawArray`, non-indexed unless the mesh carries indices | `todo` | one draw per object; no arena allocation; no clip scan when flagged clean | — |
| T3 | **raylib integration.** `DrawMesh` detects a baked entry (keyed by the mesh's vertex array) and takes the thin path; everything else is unchanged | `todo` | the 3D showcase renders through it; `MERGEPROBE`-style counters report the split | — |
| T4 | **Lifetime.** `UnloadMesh` frees the bake; `UpdateMeshBuffer` invalidates it — note that `rlUpdateVertexBuffer` is a no-op on GL 1.1 today (087 §10), so dynamic meshes must not be baked | `todo` | no stale bake after unload; a dynamic mesh is refused | — |
| T5 | **Memory ledger.** Bytes per mesh and total for the sample set, against today's arena use | `todo` | numbers in the result; the cost is stated, not assumed | — |
| T6 | **A/B.** 400/800/1,200 cubes, two runs, against merged and against S21's thin path | `todo` | per-cube slope; the winner stated per size | — |
| T7 | **Gates.** Full pixel table on the baked path | `todo` | 613 / 284 / 0 / 0 / clip / MESH_SCENE / RTT | — |
| T8 | **Capacity.** A 1,200-cube frame with 0 replays and 0 arena overflow | `todo` | the wall is gone for per-object draws | — |
| T9 | **Close.** Adopt or drop with the number; docs; default state | `todo` | explicit | — |

## 6. Design notes

**Object space, not world space.** One buffer serves every instance; the transform stays a matrix. Baking world space
would need one buffer per instance (400 × 24 × 32 B ≈ 307 KiB) and is S23's arm, valid only for sets that never move.

**Format.** Start with the 32-byte f32 layout the driver already produces (position 3f, colour 8888, uv 2f): it makes
the bake a copy, not a conversion, and it is what M3D uses. s16 positions are a later arm with a scale matrix, and
they move pixels at screen-spanning sizes (085 T2), so they are not the default.

**Non-indexed like M3D.** A cube's 36 vertices are cheaper to bake than 24 vertices plus 36 indices, and the GE takes
the array directly. Meshes that arrive indexed may keep their index array; the bake records which.

**The clip skip is inherited, not assumed.** The thin draw takes S20's clean hint. An object that is not proven clean
falls back to the scanning path, so the GE's guard-band rule keeps its proof.

**The GL contract.** `Mesh.vertices` stays for gameplay (collision, raycasts). The bake is additive; releasing the
source is a separate decision with its own memory measurement.

## 7. Gates

| gate | expected |
|---|---|
| `batch_check` / `basic_shapes` | 613 / 284 |
| `scene_3d SCENE=both`, `MESH_SCENE`, RTT | 0 diff |
| `clip_probe` | 237/720 |
| 1,200 cubes | 0 replays, `arenaOverflows=0`, no `LIST OVERFLOW` |
| frame behaviour | no full GE wait |

## 8. Exit criteria

- [ ] per-cube cost reported for merged, thin-without-bake and thin-with-bake, same scene
- [ ] the baked path is 0 diff, including a frame where an object crosses the screen edge
- [ ] memory ledger states bytes per mesh and total
- [ ] 1,200 cubes runs without replays, or the limit is stated
- [ ] adopted only if it beats the alternatives by >0.05 ms; otherwise dropped with the number
- [ ] strict docs pass; driver state explicit

## 9. Result

Pending — no device runs yet.

## 10. Follow-ups

- If the bake reaches ~40 µs/cube, S23 decides merging's fate and tries the world-space arm.
- If it stays >60 µs, the remaining cost is raylib's scene layer, and the next redesign belongs there (redesign §5).
- An s16 bake is worth a separate measurement only after the f32 path is adopted.

## 11. References

- [Redesign](../phases/10-3d-advanced/pspgu-redesign.md) — §3.2, §3.3, §3.6
- [M3D compared](../phases/10-3d-advanced/m3d-lessons.md) — F1, F2, F5, R3
- `tools/pspgu/gl_draw.c` — the current packing path and `pspguArenaAlloc`
- `src/rmodels.c` — `DrawMesh`, `UnloadMesh`, `UpdateMeshBuffer`
- sprint 089 (§9.4: the arena stays), 088 (§9.5: dual lists stay)
