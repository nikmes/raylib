# Sprint 061 (P10-S2) — Camera-relative rendering and GE-native model data

| | |
|---|---|
| Roadmap phase | [Phase 10 — 3D and advanced](../phases/10-3d-advanced/index.md) |
| Sprint | P10-S2 |
| Branch / base commit | `master` @ `acd92374` (sprint 060 closed at `5e5a4d7b`) |
| Started / closed | 2026-11-22 (written) / — |
| Build number(s) | pspgu `b26` (060 shipped `b25`; T3/T4 change driver behaviour, so bump and show it in the boot line); `libraylib.a` default `PSP_BATCH=1` |
| Tasks | 0/5 done |

## 0. Handover

**Situation.** Sprint 060 makes `DrawMesh` draw and leaves a deterministic 3D gate. This sprint removes the per-frame
vertex conversion (a one-time GE-native pack at upload) and measures whether large world offsets actually lose
precision in our pipeline before adding a camera-relative convention. The conversion result is proven by counters;
the precision result may be a null, which is recorded rather than worked around.

**Read before acting**, in this order:

1. `../sprints/060-phase10-drawmesh-and-mesh-gate.md` §9 — the packing fix, the index/colour format fields, the
   whole-primitive chunker, and the `scene_3d` skeleton. 060 is **closed**; its gates are the reference.
2. `../phases/10-3d-advanced/index.md` §Guidelines from the renderer blueprint — float24 matrices, s16/s8
   normalisation, the compact vertex format; §What we already know.
3. `../memory-map.md` §4 — list arenas, VRAM and the 512 stride; §6 — the measurement protocol; §9 — the traps
   (float24, s16 normalisation, VFPU).
4. `src/rcore.c:959-996` — `BeginMode3D` applies `MatrixLookAt` through `rlMultMatrixf`; `:998-1022` —
   `EndMode3D` restores the 2D modelview.
5. `src/rlgl.h:630-700` — `RLPspBatch`, `rlPspPushVertex` (the immediate path used by `DrawCube` when
   `PSP_BATCH=1`); `:1720` — the non-batch `rlVertex3f`.
6. `src/rmodels.c:1278-1330` — `UploadMesh` allocates `vboId` on every backend; `:1450-1495` — `DrawMesh`'s GL11
   arm; `:2686-2700` — `GenMesh*` calls `UploadMesh` before returning; `UnloadMesh` — what must be freed.
7. `tools/pspgu/gl_draw.c` — `guVertexFormat`, `vertexLayout`/`packVertex`, `isBatchedLayout`,
   `drawElements`' range/dedup chunker; `gl_matrix.c` — the CPU matrix stack that composes view × model before one
   `sceGuSetMatrix`. Note the lesson from 060: `Matrix` fields are declared `m0,m4,m8,m12,…`, so use
   `MatrixTranslate`/`MatrixScale`/`MatrixMultiply`, never positional literals.
8. `../sprints/014-phase5-ceiling-vram-readback.md` §T7 — `glReadPixels` reads the current draw buffer and is correct
   on a **stopped** frame; the diagnostic arm below is the only place allowed to take that wait.

**First action.** Add `FAR=<units> CAMREL=0|1` to `scene_3d`: camera and cube are both offset by `FAR`, the camera
orbits 0.1 units per frame for 16 frames, and the app logs the left edge x of a high-contrast face, read with
`glReadPixels` (diagnostic sync allowed). Record `FAR=0,300,1000,3000` **before** changing matrix code.

**This may be a null.** pspgu composes view × model on the CPU and sends one `GU_MODEL` matrix, so the close
translation the blueprint describes may never reach the GE as a large value. If the control jitter at 3000 is ≤1 px,
record the null with the four rows, mark T2 `dropped`, and continue with T3–T5. Do not implement a no-op.

**Hard rules.**

- **The 2D gate and the 060 cube gate are sacred.** `batch_check` 2D 0 differing pixels; the 060 cube parity stays
  0. If T2 runs, camera-relative must be a no-op when the camera origin is zero — prove it byte-identically.
- **No per-frame conversion.** The whole point of the GE-native path is that a static mesh is packed once, at
  `UploadMesh`. A counter must show `converted=0` for those draws; if it does not, the fast path did not match.
- **The quantisation formula is exact.** s16 positions are normalised by the GE as `value/32768`; pack with
  `s = round((p - centre) * K)` where `K = 32767/halfExtent`, upload `transform * T(centre) * S(32768/K)`, and never
  re-quantise per frame. Build those with `MatrixTranslate`/`MatrixScale`/`MatrixMultiply`. Stride must be a multiple
  of 4 (26 B of fields pads to 28) or the next vertex's f32 texcoord is unaligned.
- **Bump the build tag to `b26` in T3.** A `b25` boot line means a stale archive and invalidates every packed-path
  claim.
- **The fast path's 24-byte vertex does not change.** No new field, no reordering, no extra work in the batcher.
- **A diagnostic wait is not a performance arm.** The `FAR` arm may sync and read back per frame; it is excluded
  from every timing table and must be labelled as such. Measured scenes keep the phase 5 "no full GE wait" rule.
- **Mesh copies share `vboId`.** The packed buffer is keyed by the `vboId` allocation (unique per mesh and copied
  with it by value); `UnloadMesh` frees the record before `RL_FREE(mesh.vboId)`. Document that unloading a copy frees
  the shared buffer.
- **Packed meshes are static.** If the arrays are modified after upload the copy is stale; `UpdateMeshBuffer` logs
  once for a packed mesh, and dynamic/skinned meshes skip packing entirely.
- **Suspect the instrument.** Prove the arm compiled (`EXTRA_CFLAGS`, a boot line, a deliberate break) and that the
  unpacked and packed builds differ where expected.
- **Stage only this sprint's paths.** Other sessions edit the tree; `git status` before every commit, never
  `git add -A`. Commit messages carry the task ID: `sprint 061 T2: ...`.

**Commands.**

```sh
export PSPDEV=/home/psp/pspdev
export PATH=$PSPDEV/bin:$PATH
set -e

cd /home/psp/raylib-psp
tools/pspgu-build.sh && tools/pspgu-build.sh --check       # record the content hash
tools/pspgl-build.sh --check                              # reference must report 2ab7378e

build_lib() { ( cd src && make clean && make PLATFORM=PLATFORM_PSP -j8 $1 \
                && cp libraylib.a $PSPDEV/psp/lib/libraylib.a ); }

run_app() {   # $1 = app dir, $2 = tag, $3 = make vars
    ( cd "$1" && make clean && make $3 && cp *.prx /tmp/s3/app.prx )
    ( cd /tmp/s3
      timeout 20 pspsh -e "reset"; sleep 6
      ( printf './app.prx\n'; sleep 40 ) | timeout 70 pspsh
      pspsh -e "scrshot host0:/$2.raw"; pspsh -e "scrshot host0:/$2-b.raw" )
}

# Control: world-space, far from the origin. Then the same binary with camera-relative.
run_app psp-tests/scene_3d p10s2-far-world 'FAR=3000 CAMREL=0'
run_app psp-tests/scene_3d p10s2-far-camrel 'FAR=3000 CAMREL=1'
run_app psp-tests/scene_3d p10s2-near 'FAR=0 CAMREL=1'

# Reference captures need the pspGL archive AND a relink: the .prx embeds libGL.
tools/pspgu-build.sh --restore
run_app psp-tests/scene_3d p10s2-near-pspgl 'FAR=0 CAMREL=0'
tools/pspgu-build.sh            # back to pspgu, then relink the next arm
```

**Do not touch.** Lighting, fog, render-to-texture, culling and tessellation (S3–S5); the 2D texture/window paths;
`src/platforms/rcore_psp.c`'s timing hooks; any Phase 8/9/11/13 file with uncommitted work.

**Done means** §8 below, with evidence in each task row.

## 1. Objective

Falsifiable: at world offset 3000, the projected left edge of a fixed cube varies by **≤1 px** across 16 frames of a
0.1-unit camera orbit — or the world-space control already meets that and the null is recorded with T2 dropped;
if T2 runs, offset 0 is **0 differing pixels**; and a static `DrawMesh` mesh is packed **once** —
`converted=0` vertex conversions per frame — with a tex+normal+position layout of 26 B padded to a 28-B stride, down
from the 060 format's 32 B.

## 2. Why now

Phase 10's exit criteria include vertex formats and their byte costs, and every later sprint (lighting, RTT, culling)
will build on this vertex layout. Doing camera-relative after model conversion would force a retrofit through every
model path, which is exactly the trap the phase page names. The 060 gate is the reference this sprint must preserve.

## 3. Scope

**In:** `BeginMode3D`/`EndMode3D` and the modelview composition for camera-relative; immediate-vertex subtraction in
`rlgl.h`; the `DrawMesh` transform translation; a one-time packer in `UploadMesh`, its keyed storage and
`UnloadMesh`; pspgu support for `GL_SHORT` positions and a direct interleaved layout; `scene_3d` arms and counters;
the Phase 10 byte-cost table.

**Out:** s8 normals and their lighting error (S3, where lighting makes the error observable); lighting/fog state
(S3); render-to-texture (S4); culling/tessellation (S5); skinned/morph meshes, which are dynamic and keep the
converting path; any change to model file formats or assets.

## 4. Preconditions

- [x] Sprint 060 closed: cube parity 0, chunker dedup and `scene_3d` on `b25`
- [ ] pspgu `b25`/`b26` content hashes recorded; pspGL `2ab7378e` available for A/B
- [ ] `scene_3d` from 060 builds and captures deterministically on both drivers
- [ ] `EXTRA_CFLAGS` reaches the compiler in `scene_3d` (prove with a deliberate arm)
- [ ] `glReadPixels` works on a stopped frame (014 T8); the diagnostic arm syncs explicitly
- [ ] Device reset between runs; the app logs `DONE` and sleeps; two identical captures per point

## 5. Tasks

| ID | task | status | verification | evidence |
|---|---|---|---|---|
| T1 | Quantify the float24 trap: `FAR=0,300,1000,3000 CAMREL=0`; camera and cube both offset; 16-frame 0.1-unit orbit; per-frame left-edge x via a diagnostic `glReadPixels`. **If the control jitter is ≤1 px, record the null and mark T2 `dropped`** | `todo` | four jitter rows; decision recorded before T2 | — |
| T2 | Camera-relative (conditional on T1): zero `matView.m12..m14` in `BeginMode3D`, publish the camera origin, subtract it from `DrawMesh`'s transform fields and from `rlPspPushVertex` / the non-batch `rlVertex3f`; clear on `EndMode3D` | `todo` | offset 0: 0 diff vs the 060 gate; offset 3000: jitter ≤1 px; 2D captures unchanged | — |
| T3 | GE-native positions: pack s16 about the mesh centre in `UploadMesh` (skip `dynamic`/skinned), exact `K` and matrix order; pspgu converts `GL_SHORT` positions as `GU_VERTEX_16BIT`; bump `PSPGU_BUILD` to `b26` | `todo` | cube parity 0 at both offsets; 26-B fields / 28-B stride; trace shows the s16 format; `converted=0` | — |
| T4 | Direct interleaved path: one buffer in GU order, same stride/offsets; `DrawMesh` PSP branch binds it; pspgu recognises the layout and hands it to `sceGuDrawArray` without a copy; `direct`/`converted` log | `todo` | `scene_3d` logs `direct=1 converted=0`; textured cube parity 0; CPU work drops | — |
| T5 | Textured-model measurement and write-back: `INSTANCES=1,10,100` textured cubes; s16 UV experiment adopted only on 0-diff parity; 2D gate, 060 cube gate, `basic_shapes` 284; byte table | `todo` | work table; 2D 0 diff; 284 colours; table matches measured strides | — |

### Design notes for T3/T4 (implementation detail)

**Quantisation.** `halfExtent = max(|v - centre|)` over the mesh bbox; `K = (halfExtent > 0)? 32767.0f/halfExtent : 1.0f`;
`s = (short)lrintf((v - centre) * K)`. The GE applies `s/32768`, so the model matrix is
`transform * T(centre) * S(32768/K)`, composed with the matrix constructors. `S(32768/K)` must be a single uniform
scale on xyz; the position field is signed 16-bit.

**Layout (28-B stride).** texcoord f32 at +0 (8), normal f32 at +8 (12), position s16 at +20 (6), pad +26..27.
The fields sum to 26; the stride rounds to 28 so the next vertex's f32 starts 4-byte aligned. If s16 UVs are adopted
in T5: texcoord s16 at +0 (4), normal f32 at +4 (12), position s16 at +16 (6), pad to 24.

**Storage.** `UploadMesh` computes the bbox in one pass, then allocates a
`PspGeMesh { unsigned char *verts; unsigned int stride; Vector3 centre; float scale; }` in main RAM, keyed by the
`mesh.vboId` allocation. `UnloadMesh` frees it before `RL_FREE(mesh.vboId)`. Skip when
`dynamic` or `animVertices`/`animNormals` is non-NULL. Meshes are static after upload; `UpdateMeshBuffer` logs once
for a packed mesh.

**Format from array types.** `guVertexFormat()` currently derives bits from enabled arrays only. T3 makes it inspect
types too: `GL_FLOAT` → `GU_TEXTURE/NORMAL/VERTEX_32BITF`, `GL_SHORT` → `GU_VERTEX_16BIT`, `GL_UNSIGNED_BYTE` →
`GU_COLOR_8888`; any other combination is rejected once. The default stride in `glVertexPointer`/`glTexCoordPointer`
must use the type's element size, not `sizeof(float)` (a `GL_SHORT` 3-component array defaults to 6, not 12).

**Draw and detection.** `DrawMesh`'s PSP branch looks up the record and sets the client pointers into the one buffer
with the record's stride; without a record it uses the existing separate-array path. pspgu accepts the direct path
only when every enabled array shares `stride` and `ptr - base` equals the layout offsets for the derived format;
otherwise it packs. Index buffers are still copied per chunk — the zero-conversion counter is about vertices.

**Camera-relative order.** If T2 runs, subtract the camera origin from the composed model matrix's `m12..m14` fields
after `transform * T(centre) * S(scale)` and before `rlMultMatrixf`; the immediate paths subtract from the vertex
before it enters the 24-B batcher (or `glVertex3f` in a `PSP_BATCH=0` build). The origin is set in `BeginMode3D` and
cleared in `EndMode3D`, so 2D calls see zero.

**Readback placement.** The T1 diagnostic calls `glReadPixels` on the current frame **before** `EndDrawing`, with an
explicit sync; reading after the swap reads the next buffer and the wrong frame. It is excluded from every timing
row.

**Counters.** pspgu logs `drawElements direct=1` for a handed-through buffer and `converted=<n>` when it packs.
`scene_3d` asserts `direct=1 converted=0` for static meshes. Bump the boot tag to `b26`; a `b25` line invalidates the
run.

Rules:

- **IDs are stable.** Never renumber; a task that no longer applies becomes `dropped` with the reason.
- **`done` requires evidence** in §7. Code written is `doing`.
- **`blocked` names its dependency** — `blocked by 060 T1`, not "blocked".

### Task log (append-only)

- 2026-11-22 — sprint written, all tasks `todo`

## 6. Budgets

| budget | limit | measured | tag |
|---|---|---|---|
| frame time | 0 diff at offset 0; no regression in the 060 cube arm | | |
| vertex layout | tex f32 + normal f32 + position s16 = 26 B fields, 28-B stride | | |
| per-frame conversion | 0 vertices for static meshes | | |
| VRAM | 0 delta (packed buffers live in main RAM; no new textures) | | |
| heap | packed copy ≤ original mesh size; record per mesh | | |

## 7. Measurements

Protocol: device only; control → change → control again; two identical captures.

| run | build | workload | metric | value | notes |
|---|---|---|---|---|---|
| control | 060 build | `FAR=0` | pixels vs 060 gate | | must be 0 |
| control | 060 build | `FAR=0/300/1000/3000 CAMREL=0` | edge jitter (px/16 frames) | | diagnostic wait; decide T2 here |
| change | T2 | `FAR=3000 CAMREL=1` | edge jitter (px/16 frames) | | target ≤1 |
| change | T3 | cube, both offsets | pixels vs 060 gate | | target 0 |
| change | T3 | one cube | fields/stride (B) | | 26 / 28 |
| change | T4 | `scene_3d` mesh draws | `direct` / `converted` | | target 1/0 |
| change | T5 | textured cube 1/10/100 draws | work (ms) / p95 | | no target; baseline |

## 8. Exit criteria

- [ ] §1 met: four jitter rows (or the recorded null and `dropped` T2) and the conversion counters in §7
- [ ] if T2 ran: camera-relative is byte-identical at offset 0 and jitter ≤1 px at 3000; if dropped: the null is recorded with its four rows
- [ ] no per-frame vertex conversion for static meshes; dynamic/skinned meshes documented as remaining on the slow path
- [ ] pspgu accepts `GL_SHORT` positions and a same-stride interleaved layout without copying
- [ ] vertex table updated with 26/28 (and 22/24 if s16 UV is adopted); boot line `b26`
- [ ] 2D gate 0 diff; `basic_shapes` 284 colours; no full GE wait in any measured scene

## 9. Result

—

## 10. Follow-ups

- s8 normals and their lighting error → **P10-S3** (adopt only after a lit capture)
- Skinned/morph meshes on the GE-native path → after S3, when the skinning policy is chosen
- s16 texture coordinates: adopt only on 0-diff parity; record the failure and keep f32 otherwise
- Model culling with camera-relative bounds → **P10-S5**
