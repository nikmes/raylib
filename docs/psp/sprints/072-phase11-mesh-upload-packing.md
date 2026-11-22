# Sprint 072 (P11-S10) — Mesh upload and packing cost: doubles to float and VFPU conversion

| | |
|---|---|
| Roadmap phase | [Phase 11 — General optimisation outside the renderer](../phases/11-general-optimisation.md) |
| Sprint | P11-S10 |
| Branch / base commit | `master` @ `816a8c25` (pin at start) |
| Started / closed | 2026-11-22 (written) / — |
| Build number(s) | pspgu as installed (`--check` recorded); `libraylib.a` with `PSP_MESH_VFPU=0|1` |
| Tasks | 0/5 done |

## 0. Handover

**Situation.** `src/rmodels.c` carries **349 soft-float `double` relocation sites** and its upload path packs
positions, UVs, normals and colours one vertex at a time. Phase 10 has already chosen the formats (s16
positions, the direct interleaved path); this sprint measures and accelerates the **conversion loop**, not the
draw or clip path. The work is one-time per mesh, so the payoff is load time and large-mesh scenes, not the
steady frame.

**Read before acting**, in this order:

1. `../memory-map.md` §10.1/§14 — VFPU costs, `vtfm4.q`/`vmmul`, accuracy, thread/context; §4 — the draw
   path is the frame cost, not conversion.
2. `../sprints/060-phase10-drawmesh-and-mesh-gate.md` and `061-phase10-camera-relative-and-ge-native-models.md`
   — the s16 packing, direct interleaved path, and the `scene_3d`/`batch_check` parity gates.
3. `src/rmodels.c` — `UploadMesh`, `DrawMesh`, the vertex/UV/normal/colour packing, `GenMesh*`.
4. Phase 13 model samples (`psp-tests/samples/models/`) and `psp-tests/scene_3d`.
5. `../sprints/056-phase11-vfpu-math-and-copy.md` — per-operation adoption, accuracy rows, thread discipline.

**First action.** Capture the **before** rows: `UploadMesh` µs for 1k/10k/100k vertices, `DrawMesh`
conversion µs, the double-site census per function, and parity captures (`scene_3d`, `batch_check`
`MESH_SCENE=1`). No code change before those rows exist.

**Hard rules.**

- **Do not change the packed format or the draw path.** Positions stay s16, UVs/normals keep their chosen
  widths, and pspgu is untouched. This sprint accelerates the loop that produces the same bytes.
- **Vertex bytes are the parity gate.** Any float/VFPU conversion must produce the identical vertex buffer;
  renders must be 0-diff.
- **One-time only.** Do not move per-frame draw transforms into a VFPU loop unless T1 shows the per-object
  matrix build is the measured cost; that is P10-S8's question.
- **VFPU thread discipline**: `PSP_THREAD_ATTR_VFPU`, `pspvfpu_initcontext()`/`pspvfpu_use_matrices()`,
  16-byte alignment; PPSSPP will not catch corruption.
- **No intrinsics.** Use `libpspmath`, `pspgum`, `pspvfpu.h` or inline asm.
- **Default off until proven.** `PSP_MESH_VFPU=1` is opt-in; `=0` is byte-identical.
- Commit messages carry the task ID.

**Commands.**

```sh
export PSPDEV=/home/psp/pspdev
export PATH=$PATH:$PSPDEV/bin

/home/psp/raylib-psp/tools/pspgu-build.sh --check
cd /home/psp/raylib-psp/src && make PLATFORM=PLATFORM_PSP -j8 PSP_MESH_VFPU=0
cp libraylib.a $PSPDEV/psp/lib/libraylib.a

cd ../psp-tests/mesh_probe          # built in T1
make
( printf './mesh_probe.prx\n'; sleep 40 ) | pspsh
pspsh -e "reset"

# parity gates
cd ../scene_3d && make
( printf './scene_3d.prx\n'; sleep 20 ) | pspsh
cd ../batch_check && make MESH_SCENE=1
```

**Do not touch.** `tools/pspgu`, `rlgl.h` draw paths, the s16/UV formats, `rtextures.c` (P11-S9),
`rshapes.c` (P11-S11), audio, input, and the frozen `scene_3d` output.

**Done means** §8, with per-size rows, a byte-parity hash and a written adoption/no-adoption decision.

## 1. Objective

Measure the mesh conversion loop and remove its avoidable `double` work; adopt a VFPU packing path only where
it beats scalar/float at the sizes raylib uses, with identical vertex bytes and 0-diff renders.

## 2. Why now

The mesh path is the second-largest `double` cluster and the Phase 13 model corpus is coming. Phase 10 has
fixed the formats, so the conversion loop is now a stable target; changing it after the samples are ported
would invalidate their baselines.

## 3. Scope

**In:** `src/rmodels.c` upload/conversion; `GenMesh*` transform maths if measured; a mesh probe; the model
sample/parity corpus; `PSP_MESH_VFPU`.

**Out:** pspgu, draw/clip/guard-band, the packed formats, per-object draw transforms (P10-S8), textures
(P11-S9), shapes (P11-S11).

## 4. Preconditions

- [ ] P11-S1 harness (CPU zones/wrap counters) available
- [ ] pspgu installed; `--check` recorded
- [ ] `scene_3d` and `batch_check MESH_SCENE=1` build and capture
- [ ] Vertex-buffer hashes for 1k/10k/100k meshes as parity controls
- [ ] VFPU-capable thread identified and attributed in a separate measured step
- [ ] PSPLink available; the user has approved the run

## 5. Tasks

| ID | task | status | verification | evidence |
|---|---|---|---|---|
| T1 | **Baseline**: `UploadMesh`/`DrawMesh` conversion µs at 1k/10k/100k vertices; double census per function; parity hashes | `todo` | before rows; parity; per-function counts | — |
| T2 | **Doubles to float**: convert only where the vertex bytes are identical; list rejected sites | `todo` | 0-diff vertex hash; µs; no format change | — |
| T3 | **VFPU packing**: float→s16, UV/normal/colour packing, four lanes per operation; `PSP_MESH_VFPU` | `todo` | byte parity; µs at three sizes; VFPU vs scalar | — |
| T4 | **`GenMesh*` transforms**: `vtfm4.q` only if T1 shows the generator cost; parity | `todo` | mesh hash; µs; or `dropped` with the number | — |
| T5 | **Corpus/write-back**: model samples, `scene_3d`, `batch_check`, docs, `mkdocs --strict` | `todo` | 0-diff; gate hash; docs build | — |

### Task log

- 2026-11-22 — sprint written; not started

## 6. Budgets

| budget | limit | measured | tag |
|---|---|---|---|
| vertex parity | identical bytes / 0-diff render | | |
| upload win | ≥ 0.1 % or a stated load-time saving | | |
| frame regression | 0 | | |
| format change | none | | |

## 7. Measurements

| run | build | workload | metric | value | notes |
|---|---|---|---|---|---|
| baseline | `P11-S10 a` | 1k/10k/100k | µs/upload | | |
| doubles→float | `P11-S10 b` | same | µs, vertex hash | | |
| VFPU | `P11-S10 c` | same | µs, vertex hash | | opt-in |
| parity | `P11-S10 c` | `scene_3d`, `MESH_SCENE` | diff pixels | | 0 |

## 8. Exit criteria

- [ ] per-size baseline/change rows and the double census
- [ ] VFPU adoption decision per packing step, with byte parity
- [ ] `scene_3d`/`batch_check` 0-diff; `batch_check` `b69830b8…`
- [ ] no format or draw-path change
- [ ] `PSP_MESH_VFPU=0` byte-identical
- [ ] docs updated; `mkdocs --strict`

## 9. Result

Not started.

## 10. Follow-ups

- If the cost is draw submission, the owner is Phase 10's clip/draw path, not this sprint.
- Skinning/instance transforms are P10-S8's per-object question; do not fold them in without a measurement.
- Keep the UV/normal widths; a VFPU win does not justify a format change.
