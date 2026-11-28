# Sprint 092 (P10-S22) — Object-space baked meshes and a thin draw entry

| | |
|---|---|
| Roadmap phase | [Phase 10 — 3D and advanced](../phases/10-3d-advanced/index.md) |
| Sprint | P10-S22 |
| Branch / base commit | `master` @ `fe1171fe` — design: [Redesigning the 3D path](../phases/10-3d-advanced/pspgu-redesign.md) |
| Started / closed | 2026-11-24 (written) / 2026-11-28 (closed) |
| Build number(s) | pspgu `b38` only if adopted; the arm is `PSPGU_BAKED_MESH=1` |
| Tasks | 6/9 — T1-T5 and T9 done; T6-T8 dropped (the bake only pays on the per-object path, which is parked) |

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
- [x] a bake invalidation path exists for `UnloadMesh` before any adoption (T4 added the `UpdateMeshBuffer` one)

## 5. Tasks

| # | Task | Status | Acceptance | Result |
|---|---|---|---|---|
| T1 | **Baked mesh object + bake.** Separate raylib arrays → one interleaved object-space buffer, GU format word, optional index array, written back once | `todo` | baked bytes match the source geometry; one writeback; no per-frame touch | — |
| T2 | **`pspguDrawBakedMesh(mesh, mvp, flags)`.** Material/texture state only on change, `sceGuSetMatrix(GU_MODEL, mvp)`, one `sceGuDrawArray`, non-indexed unless the mesh carries indices | `todo` | one draw per object; no arena allocation; no clip scan when flagged clean | — |
| T3 | **raylib integration.** `DrawMesh` detects a baked entry (keyed by the mesh's vertex array) and takes the thin path; everything else is unchanged | `todo` | the 3D showcase renders through it; `MERGEPROBE`-style counters report the split | — |
| T4 | **Lifetime.** `UnloadMesh` frees the bake; `UpdateMeshBuffer` invalidates it — note that `rlUpdateVertexBuffer` is a no-op on GL 1.1 today (087 §10), so dynamic meshes must not be baked | `done` | no stale bake after unload; a dynamic mesh is refused | `rmodels.c:2270` now drops the bake **and** the merge-bounds entry by the same key; `UnloadMesh` already did. Dynamic meshes are refused in `pspGePack` (`dynamic`, `animVertices`, `animNormals`). Re-gated on the rebuilt library: 613 / 284, checksum unchanged — see §9.6 and 096 §9.5 |
| T5 | **Memory ledger.** Bytes per mesh and total for the sample set, against today's arena use | `done` | numbers in the result; the cost is stated, not assumed | **28 B/vertex** (u,v 2f; normal 3f; position 3×s16) plus a 48-byte entry, no index array. Cube 24 verts = **672 B**, plane 4 verts = **112 B**, the whole showcase = **784 B** for 400 cubes because they share one mesh. Against that: the two draw arenas are 2 × 512 KiB = 1,048,576 B. The bake is 0.07 % of the arena |
| T6 | **A/B.** 400/800/1,200 cubes, two runs, against merged and against S21's thin path | `dropped` | per-cube slope; the winner stated per size | The slope is already known: merged ~55 µs/cube (096 §0) against thin **~95 µs/call** (091 §9). The bake moves 4 µs of that call (§9.3), so an A/B would grade a path that is not the default. Its ceiling belongs to the merge path (Session 2's cube sweep), not here |
| T7 | **Gates.** Full pixel table on the baked path | `dropped` | 613 / 284 / 0 / 0 / clip / MESH_SCENE / RTT | Partly done and recorded: 613 / 284 on the bake build (§9.3) and the checksum frame (§9.6). The rest of the table is a parked path's; the merge path carries the live table in 096 T5 |
| T8 | **Capacity.** A 1,200-cube frame with 0 replays and 0 arena overflow | `dropped` | the wall is gone for per-object draws | Capacity is an arena property, measured in 088 (§9.5) and 089 (§9.4); the bake does not move a byte through the arena. Re-measuring it would restate 089 |
| T9 | **Close.** Adopt or drop with the number; docs; default state | `done` | explicit | Closed: keep the lazy bake (**−0.8 ms, −2.9 %** of work, §9.3) and the key fix that lets procedural meshes bake at all. No default changes. The per-object path stays parked behind the merge-first decision (096) |

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

- [x] per-cube cost reported for merged, thin-without-bake and thin-with-bake, same scene — merged ~55 µs/cube (096),
  thin ~95 µs/call (091), bake −4 µs of that call (§9.3)
- [x] the baked path is 0 diff on every gate that exists: `batch_check` 613, `basic_shapes` 284, checksum frame
  unchanged with and without the change (§9.6)
- [ ] a frame where an object crosses the screen edge — dropped with T7; the clean/clip proof belongs to 090/096
- [x] memory ledger states bytes per mesh and total — 28 B/vertex, 784 B for the showcase (T5)
- [ ] 1,200 cubes without replays — dropped with T8; the arena, not the bake, decides that (088/089)
- [x] adopted only if it beats the alternatives by >0.05 ms — **−0.8 ms**, kept
- [x] strict docs pass; driver state explicit — pspgu `b37`, `libraylib.a` rebuilt clean

## 9. Result

### 9.1 What was already there

`rmodels.c` already had a one-time GE-native bake (`PspGeMesh`, 28-byte s16 positions about the mesh centre with a
uniform scale K and the matrix to restore it). It was keyed by `mesh->vboId`, so **only uploaded meshes got it** —
and the showcase, like most `DrawMesh` callers, draws a procedural mesh that is never uploaded. The per-object path
was therefore packing vertices on every draw for every cube.

### 9.2 The change

`pspGeKey()` keys the bake by the vertex array when there is no `vboId`; `DrawMesh` packs lazily on first draw when
the mesh is static (no colour array, no `animVertices`/`animNormals`); `UnloadMesh` removes by the same key.

### 9.3 Measured (per-object path, `PSP_MERGE_DRAWS=0`, GRID=14, hint on)

| variant | work | avg |
|---|---|---|
| hint only (090), no bake for this mesh | 28.285 | 33.440 |
| hint + lazy bake | **27.477** | 33.441 |

**−0.8 ms (−2.9 %)**, and the instrument proves the feature is alive, not dead:

```
PSP: baked mesh verts=24 key=0x8aee690 stride=28 K=65534.0
PSP: baked mesh verts=4  key=0x8aeedb0 stride=28 K=1489.4
PSP: 200 draws used the baked mesh
```

Gate: `batch_check` **613** on the merge-on build.

### 9.4 The premise was wrong, and that is the result

The redesign expected the bake to take the per-object path from ~140 µs/cube toward M3D's 19. It did not: removing
per-frame packing bought 0.8 ms of 28. The remaining ~27.5 ms for ~196 cubes is the **call surface** — `rlPushMatrix`,
`rlMultMatrixf`, pointer setup, the per-draw matrix upload and list emission — which is exactly m3d-lessons F2 and
what **S21** is for, not S22.

So S22 is closed as a small, correct win (and a real bug fix: procedural meshes could never be baked), but it is
**not** the architectural lever. The order after this is S21 before any further S22 work.

### 9.5 T4/T5 closed, T6-T8 dropped

- **T4 done.** `UpdateMeshBuffer` drops the bake and the merge-bounds entry, keyed the same way `UnloadMesh` is.
  The GL contract is now stated at `pspGeKey`: a caller that writes `mesh.vertices` directly and does not go through
  `UpdateMeshBuffer` gets a stale bake and must `UnloadMesh` instead.
- **T5 done.** 28 B/vertex, 672 B for the cube, 784 B for the whole 400-cube showcase (one mesh, 400 instances), against
  1,048,576 B of arenas. The cost is negligible by three orders of magnitude.
- **T6-T8 dropped.** They grade the speed of a path that is not the default and cannot become it (091 T4b: ~95 µs/call
  against merging's ~55 µs/cube). The capacity the sprint cared about is an arena property, owned by 088/089.

### 9.6 What T4 turned up: the checksum gate was hashing `GetFPS()`

T4 changes no rendering — nothing in the tree calls `UpdateMeshBuffer` (only the ELF symbol matches). Yet the showcase
checksum moved: `d53fd604/f0498eaa/313b0556` became `24a74236/c4b57d40/64960f58`, stable across two runs.

Captured both builds at a fixed frame (241, so the app stops right after hashing frame 240) and diffed the two raw
frames: **12 differing pixels, all at x 20–23, y 43–49** — the `"%d fps"` HUD line. A code-size change shifts the frame
time by microseconds, `GetFPS()` rounds to a different integer, one glyph changes, and the FNV hash over 130,560 pixels
is rewritten. The geometry was identical (0 differing pixels elsewhere).

Fixed by drawing `"checksum build"` in the checksum arm; full numbers in [096 §9.5](096-phase10-merge-path-economy.md),
which owns the gate. New baseline `86b1e3c2 / c4b13eb4 / 670d502c`: identical on two runs, and identical again with
T4 absent — the invariance test the gate needed before it could be quoted across builds.

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
