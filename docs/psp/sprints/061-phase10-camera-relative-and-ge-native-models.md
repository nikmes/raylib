# Sprint 061 (P10-S2) — Camera-relative rendering and GE-native model data

| | |
|---|---|
| Roadmap phase | [Phase 10 — 3D and advanced](../phases/10-3d-advanced/index.md) |
| Sprint | P10-S2 |
| Branch / base commit | `master` @ `acd92374` (sprint 060 closed at `5e5a4d7b`) |
| Started / closed | 2026-11-22 (written) / 2026-11-22 |
| Build number(s) | pspgu `b26` (060 shipped `b25`; T3/T4 change driver behaviour, so bump and show it in the boot line); `libraylib.a` default `PSP_BATCH=1` |
| Tasks | 5/5 done — T2 and the s16-UV experiment `dropped` with reasons |

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
| T1 | Measure the float24 trap at `FAR=0,300,1000,3000 CAMREL=0` | `done` | four jitter rows; decision before T2 | edge sequence identical at every offset → **null** |
| T2 | Camera-relative convention | `dropped` | — | T1 is a null: pspgu composes view×model on the CPU, so the GE never sees the large separate translation. No no-op was implemented |
| T3 | GE-native positions: pack s16 about the mesh centre in `UploadMesh`, exact `K`, `S·T(centre)·transform`; pspgu reads `GL_SHORT` as `GU_VERTEX_16BIT`; `b26` | `done` | cube parity 0; 26-B fields / 28-B stride; trace shows s16; `converted=0` | `fmt=0x163 draw=0x1163 stride=28`; 0 diff near **and** FAR=3000 |
| T4 | Direct interleaved path, no copy; `direct`/`converted` log | `done` | `direct=1 converted=0`; textured parity 0; large draw direct | `drawElements direct=1 n=36` and `n=2046`; textured/both 0 diff |
| T5 | Textured-model measurement, s16 UV experiment, gates and table | `done` | work table; 2D 0; 284 colours; UV decision recorded | 0.556/1.032/5.696 ms at 1/10/100; UV kept f32 (dropped) |

### Design notes (as shipped)

**Driver gate.** Packing runs only when `glGetString(GL_VENDOR)` contains `pspgu`. pspGL rendered **zero pixels**
from the interleaved `GL_SHORT` layout in the A/B, and the same `libraylib.a` must keep the reference build working;
pspGL falls back to the 060 float path and matches it exactly. The check is one-time and not cached before GL init.

**Quantisation and matrix.** `half = max` half-extent; `K = 32767/half`; `s = lrintf((v - centre)*K)`;
`S = 32768/K`. Raymath composes **row-vector** (`A·B` applies A then B), so the model matrix is
`S · T(centre) · transform` — using `transform · T · S` scales the translation and shrinks the world, which the
first run did and the gate caught. Stride is 28: tex f32 +0, normal f32 +8, position s16 +20, pad +26.

**Storage and draw.** `UploadMesh` allocates a `PspGeMesh` keyed by `mesh->vboId`, writes back the D-cache once;
`UnloadMesh` frees it. `DrawMesh` binds the three pointers into one buffer and calls `glDrawElements`; pspgu detects
the layout (same stride, pointers at the layout offsets) and hands the base to `sceGuDrawArray`, copying only the
index chunk. Meshes without texcoords/normals, dynamic and skinned meshes stay on the float path.
`UpdateMeshBuffer` on a packed mesh is not supported and remains a follow-up (no scene uses it).

**UV decision.** s16 texture coordinates were not adopted: f32 UVs already fit the 28-B stride, the GE's 16-bit
coordinates are unsigned with wrap semantics that need their own gate, and no measured scene is texture-bandwidth
bound. Kept f32; revisit if a scene shows it.

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
| control | b25 | `FAR=0/300/1000/3000 CAMREL=0` | edge sequence | identical at all four | null; diagnostic `glReadPixels` |
| change | b26 | cube near / FAR=3000 | diff vs 060 gate | **0 / 0** | pspGL FAR=3000 also 0 diff |
| change | b26 | one cube | fmt / stride | `0x163` (draw `0x1163`) / 28 B | direct=1 |
| change | b26 | `SCENE=grid` | diff vs 060 | 37 px | one-row top-edge shift, `y 87..94`, max Δ252: s16 quantisation |
| change | b26 | `SCENE=both` | capture identity | two captures identical | direct=1, 19,608 px |
| change | b26 | `SCENE=tex` 1/10/100 | work (ms) | **0.556 / 1.032 / 5.696** | 120 frames, median 16.683 |
| gate | b26 | `batch_check` 2D | diff | **0** | |
| gate | b26 | `basic_shapes` | colours | **284** | |
| gate | b26 | pspGL near/far | diff vs 060 | **0 / 0** | float fallback after driver gate |

## 8. Exit criteria

- [x] §1 met: the four jitter rows are identical and the null is recorded; T2 `dropped`
- [x] if T2 dropped: the null is recorded with its rows (no camera-relative code shipped)
- [x] no per-frame vertex conversion for static pspgu meshes; dynamic/skinned meshes stay on the float path
- [x] pspgu accepts `GL_SHORT` positions and the same-stride interleaved layout without copying
- [x] vertex table updated with 26/28; boot line `b26`
- [x] 2D gate 0 diff; `basic_shapes` 284; no full GE wait in measured scenes
- [x] pspGL reference build still renders and matches 060 (driver gate), same `libraylib.a`

## 9. Result

Sprint 061 closed 5/5 with two documented drops. The useful half shipped; the suspected half did not exist.

- **T1 is a null.** The edge sequence is identical frame-for-frame at `FAR=0,300,1000,3000`. pspgu composes
  view×model on the CPU and sends one `GU_MODEL` matrix, so the blueprint's separate large translation never reaches
  the GE. T2 was `dropped` rather than implemented as a no-op.
- **GE-native packing works on pspgu.** `UploadMesh` packs texcoord f32, normal f32 and s16 positions about the
  centre into a 28-B stride; `DrawMesh` binds it and pspgu passes the base to `sceGuDrawArray` (`direct=1`,
  `converted=0`). Near and FAR=3000 cubes are 0 diff from the 060 gate; textured cubes are 0 diff. Work at
  1/10/100 cubes is 0.556/1.032/5.696 ms.
- **The first matrix order was wrong.** `transform · T · S` scaled the translation (cubes shrank toward the centre).
  Raymath is row-vector, so `S · T(centre) · transform` is correct. The gate caught it; this is the second
  matrix-convention bug in phase 10.
- **Driver gate.** pspGL renders **no pixels** from the interleaved `GL_SHORT` layout. Packing now checks
  `GL_VENDOR` once and only runs on pspgu; pspGL falls back to the float path and matches 060 exactly (near and
  FAR=3000). Without this the A/B build would have regressed.
- **Grid quantisation.** The packed grid differs from the 060 f32 grid by 37 pixels in a one-row top-edge band
  (`y 87..94`, max Δ252) — sub-pixel s16 rounding, not a topology error. The cube gates remain exact.
- **s16 UVs `dropped`:** f32 UVs fit the stride, and unsigned 16-bit wrap semantics need their own gate; no scene is
  bandwidth-bound.

## 10. Follow-ups

- `UpdateMeshBuffer` on a packed mesh is unsupported; rebuild or re-upload. No current scene does it.
- pspGL cannot consume the packed `GL_SHORT` layout; the vendor gate keeps it on the float path. Do not remove the
  gate without a pspGL fix.
- s16 UVs, if a scene becomes texture-bandwidth bound: unsigned 16-bit wrap semantics need a dedicated 0-diff gate.
- s8 normals and their lighting error → **P10-S3**.
- Skinned/morph meshes remain on the float path; GE skinning is a later decision.
- Camera-relative may return if a future scene sends model and view to the GE separately; today it does not.

