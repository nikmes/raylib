# Sprint 061 (P10-S2) — Camera-relative rendering and GE-native model data

| | |
|---|---|
| Roadmap phase | [Phase 10 — 3D and advanced](../phases/10-3d-advanced/index.md) |
| Sprint | P10-S2 |
| Branch / base commit | `master` @ `f638d4da` (sprint 060 written) |
| Started / closed | 2026-11-22 (written) / — |
| Build number(s) | pspgu after the 060 change + this sprint's format support (content hash recorded); `libraylib.a` default `PSP_BATCH=1` |
| Tasks | 0/5 done |

## 0. Handover

**Situation.** Sprint 060 makes `DrawMesh` draw and leaves a deterministic 3D gate. This sprint fixes the two
things every later 3D measurement would otherwise bake in: large world offsets lose precision on the GE, and model
vertices are converted on the CPU every frame. The output is a camera-relative convention that is pixel-identical at
the origin, and a one-time GE-native packing step at mesh upload, proven by counters — not by feel.

**Read before acting**, in this order:

1. `../sprints/060-phase10-drawmesh-and-mesh-gate.md` — the packing fix, the chunker, and the `scene_3d` skeleton this
   sprint extends. **Do not start until 060 T1/T2 are `done` and its gate is green.**
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
7. `tools/pspgu/gl_draw.c` — `guVertexFormat`, `convertVertex`, `isBatchedLayout`; `gl_matrix.c` — the CPU matrix
   stack that composes view × model before one `sceGuSetMatrix`.
8. `../sprints/014-phase5-ceiling-vram-readback.md` §T7 — `glReadPixels` reads the current draw buffer and is correct
   on a **stopped** frame; the diagnostic arm below is the only place allowed to take that wait.

**First action.** Build `scene_3d` with a `FAR=3000` arm that renders the same cube at world offset 3000 and logs
the left edge x of a high-contrast face for 16 frames while the camera orbits by 0.1 units per frame. Record the
jitter **before** changing any matrix code; that is the control this sprint must beat.

**Hard rules.**

- **The 2D gate and the 060 cube gate are sacred.** `batch_check` 2D 0 differing pixels; the 060 cube parity stays
  0. Camera-relative must be a no-op when the camera origin is zero — prove it byte-identically, not by inspection.
- **No per-frame conversion.** The whole point of the GE-native path is that a static mesh is packed once, at
  `UploadMesh`. A counter must show `converted=0` for those draws; if it does not, the fast path did not match.
- **The quantisation formula is exact.** s16 positions are normalised by the GE as `value/32768`; pack with
  `s = round((p - centre) * K)`, upload the model matrix `T(centre) * S(32768/K)`, and never re-quantise per frame.
  An off-by-one in `K` is invisible at first and wrong in every capture.
- **The fast path's 24-byte vertex does not change.** No new field, no reordering, no extra work in the batcher.
- **A diagnostic wait is not a performance arm.** The `FAR` arm may sync and read back per frame; it is excluded
  from every timing table and must be labelled as such. Measured scenes keep the phase 5 "no full GE wait" rule.
- **Mesh copies share `vboId`.** The packed buffer is keyed by the `vboId` allocation; unloading a copy frees the
  shared buffer. Document it and do not unload copies.
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
```

**Do not touch.** Lighting, fog, render-to-texture, culling and tessellation (S3–S5); the 2D texture/window paths;
`src/platforms/rcore_psp.c`'s timing hooks; any Phase 8/9/11/13 file with uncommitted work.

**Done means** §8 below, with evidence in each task row.

## 1. Objective

Falsifiable: at world offset 3000, the projected left edge of a fixed cube varies by **≤1 px** across 16 frames of a
0.1-unit camera orbit (against the measured world-space jitter), while at offset 0 the change is **0 differing
pixels**; and a static `DrawMesh` mesh is packed **once** — `converted=0` vertex conversions per frame — with a
tex+normal+position stride of at most 28 B, down from the 060 format's 32 B.

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

- [ ] Sprint 060 T1/T2 `done`: cube parity 0 and the chunker fix in the build
- [ ] pspgu content hash recorded; pspGL `2ab7378e` available for A/B
- [ ] `scene_3d` from 060 builds and captures deterministically on both drivers
- [ ] `EXTRA_CFLAGS` reaches the compiler in `scene_3d` (prove with a deliberate arm)
- [ ] `glReadPixels` works on a stopped frame (014 T8); the diagnostic arm syncs explicitly
- [ ] Device reset between runs; the app logs `DONE` and sleeps; two identical captures per point

## 5. Tasks

| ID | task | status | verification | evidence |
|---|---|---|---|---|
| T1 | Quantify the float24 trap: add `FAR=0,300,1000,3000 CAMREL=0` to `scene_3d` (world offset applied to camera *and* object; 16-frame 0.1-unit orbit; per-frame left-edge x) | `todo` | jitter (max−min edge x) grows with offset; record all four rows | — |
| T2 | Camera-relative convention: zero the view translation in `BeginMode3D`, publish the camera origin, subtract it in `rlPspPushVertex`, the non-batch `rlVertex3f` and `DrawMesh`'s transform; clear on `EndMode3D` | `todo` | offset 0: 0 differing pixels vs T1; offset 3000: jitter ≤1 px; 2D captures unchanged | — |
| T3 | GE-native positions: pack s16 about the mesh centre once in `UploadMesh` (skip `dynamic`/skinned), exact `K` scale in the model matrix; pspgu reads `GL_SHORT` positions as `GU_VERTEX_16BIT` | `todo` | cube parity 0 at both offsets; stride ≤28 B; trace shows the s16 format; conversion counter 0 | — |
| T4 | Direct interleaved path: pack in GU order (texture, colour, normal, position) with one stride; pspgu recognises same-stride offsets and hands the buffer to `sceGuDrawArray` without a copy; `direct`/`converted` counters | `todo` | `scene_3d` logs `direct=N converted=0`; textured cube parity 0; CPU work drops | — |
| T5 | Textured-model measurement and write-back: 1/10/100 `DrawMesh` draws of a textured cube; s16 UV experiment adopted only on 0-diff parity; 2D gate, 060 cube gate, `basic_shapes` 284; byte-cost table on the Phase 10 page | `todo` | work table; 2D 0 diff; 284 colours; table matches measured strides | — |

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
| vertex stride | tex f32 + normal f32 + position s16 ≤ 28 B | | |
| per-frame conversion | 0 vertices for static meshes | | |
| VRAM | 0 delta (packed buffers live in main RAM; no new textures) | | |
| heap | packed copy ≤ original mesh size; record per mesh | | |

## 7. Measurements

Protocol: device only; control → change → control again; two identical captures.

| run | build | workload | metric | value | notes |
|---|---|---|---|---|---|
| control | 060 build | `FAR=0` | pixels vs 060 gate | | must be 0 |
| control | 060 build | `FAR=3000 CAMREL=0` | edge jitter (px/16 frames) | | diagnostic wait |
| change | T2 | `FAR=3000 CAMREL=1` | edge jitter (px/16 frames) | | target ≤1 |
| change | T3 | cube, both offsets | pixels vs 060 gate | | target 0 |
| change | T3 | one cube | packed stride (B) | | target ≤28 |
| change | T4 | `scene_3d` mesh draws | `direct` / `converted` | | target 1/0 |
| change | T5 | textured cube 1/10/100 draws | work (ms) / p95 | | no target; baseline |

## 8. Exit criteria

- [ ] §1 met, with the four jitter rows and the conversion counters in §7
- [ ] camera-relative is byte-identical to the 060 gate at offset 0, for both 2D and 3D
- [ ] no per-frame conversion for static meshes; dynamic/skinned meshes documented as remaining on the slow path
- [ ] pspgu accepts `GL_SHORT` positions and a same-stride interleaved layout without copying
- [ ] vertex-format/byte-cost table updated on the Phase 10 page
- [ ] 2D gate 0 diff; `basic_shapes` 284 colours; no full GE wait in any measured scene

## 9. Result

—

## 10. Follow-ups

- s8 normals and their lighting error → **P10-S3** (adopt only after a lit capture)
- Skinned/morph meshes on the GE-native path → after S3, when the skinning policy is chosen
- s16 texture coordinates if T5's parity fails → record the failure and keep f32
- Model culling with camera-relative bounds → **P10-S5**
