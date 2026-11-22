# Sprint 060 (P10-S1) — DrawMesh conversion fix and the 3D mesh gate

| | |
|---|---|
| Roadmap phase | [Phase 10 — 3D and advanced](../phases/10-3d-advanced/index.md) |
| Sprint | P10-S1 |
| Branch / base commit | `master` @ `682207df` (all six P10 sprints written; 060 was first drafted at `f04bac38`) |
| Started / closed | 2026-11-22 (written) / — |
| Build number(s) | pspgu `b25` — T1/T2 change driver behaviour, so `PSPGU_BUILD` must be bumped from `b24` and the boot line must show it; `libraylib.a` default `PSP_BATCH=1` |
| Tasks | 0/5 done |

## 0. Handover

**Situation.** Phase 10 opens with one narrow carry-over from sprint 013: `glDrawElements` reaches the driver and
produces no pixels, while the immediate 3D path and the whole 2D corpus are correct. The converting vertex path in
`tools/pspgu/gl_draw.c` packs a fixed record that does not match the format it advertises to the GE. This sprint
fixes the packing, fixes the chunker that silently degrades large meshes, and turns the existing cube probes into a
deterministic gate with a `scene_3d` corpus skeleton for later sprints.

**Read before acting**, in this order:

1. `../phases/10-3d-advanced/index.md` §Readiness review — blocker 1 (corrected by this sprint) and the exit
   criteria this sprint serves; §Tasks, first bullet.
2. `../sprints/013-phase5-corpus-and-counters.md` §T1 split — the exact failing trace
   (`drawElements count=36 tex2d=0 fmt=0x1e3`), the predicted cube bbox, and the six hypotheses already eliminated.
3. `tools/pspgu/gl_draw.c:1-60` — the fast/slow split and the `PspGuVertex` record; `:422-450` —
   `guVertexFormat()`; `:452-500` — `convertVertex()`; `:520-600` — `glDrawArrays()`; `:604-690` —
   `glDrawElements()` and the chunker; `tools/pspgu/pspgu_internal.h:46` — `PSPGU_BUILD`, which T1 bumps.
4. `src/rmodels.c:1450-1495` — which client arrays `DrawMesh` enables, and why `tex2d=0` for
   `LoadMaterialDefault` (`rlGetTextureIdDefault()` is 0 on GL11, `src/rlgl.h`).
5. `src/rlgl.h:4406-4420` — `rlDrawVertexArrayElements()` passes the index buffer through unchanged;
   `pspgl-pspdev/glDrawElements.c` forwards to its varray path with no chunking, so a pspGL capture is the
   reference for T2's spread-index grid.
6. `psp-tests/triangle_probe/main.c` and `Makefile` — `DRAW_MESH` arm, predicted bbox x 193..287, y 92..189;
   `EXTRA_CFLAGS` is now wired (it used to be dropped silently).
7. `psp-tests/batch_check/main.c:114-138` and `Makefile:32-35` — the `MESH_SCENE` arm: 2D → `DrawCube` + `DrawMesh`
   → 2D in one frame.
8. `../memory-map.md` §4 — list arenas and VRAM; §6 — the measurement protocol.

**First action.** Build the current tree against pspgu and capture the *failing* baseline before editing anything:
`triangle_probe` with `EXTRA_CFLAGS=-DDRAW_MESH` and `batch_check` with `MESH_SCENE=1`, pspgu and pspGL, two captures
each. Record fmt, **allocated stride** (currently 36 B against a 32-B format), draw calls and non-background pixel
count. No fix without a before row. The boot line must say `b24`; after T1 it must say `b25`, or the device is
running a stale archive.

**Hard rules.**

- **The 2D gate is sacred.** After every change, `batch_check` 2D must be 0 differing pixels against pspGL
  (`/tmp/s3/par_pspgl.raw` or a fresh same-build capture). The fast path's 24-byte vertex must not change.
- **Suspect the instrument first.** `triangle_probe`'s Makefile once dropped `EXTRA_CFLAGS`, so `-DDRAW_MESH`
  compiled the default; `batch_check` once read `CFLAGS = -Wall` without `EXTRA_CFLAGS`. Prove the build contains
  the arm: a changed `psp-nm`, a boot log line naming the arm, or a deliberate break.
- **One variable per run.** Control (pspGL) → change (pspgu) → control again. Two identical captures before a
  capture is evidence; a `scrshot` taken while the app renders freezes it, so capture after `DONE`.
- **Pack only what the format advertises.** GU's field order is fixed: texture, colour, normal, position. The stride
  is the sum of the enabled fields; the struct's size is not the format's size. `pspguWriteback()` and the arena
  allocation both take `n*stride` bytes — a forgotten multiplier leaves the tail of the buffer uncached.
- **Bump the build tag.** `PSPGU_BUILD` moves to `b25` in T1 and the boot line is part of the evidence; a stale
  archive otherwise looks like a failed fix.
- **Unsupported input rejects loudly.** Only `GL_FLOAT` vertex/normal/texcoord arrays and `GL_UNSIGNED_BYTE` colours
  are converted today; a different type must log once and skip the draw, not reinterpret bytes as floats. Likewise
  a non-`GL_UNSIGNED_SHORT` index type is not silently dropped (`gl_draw.c:610`).
- **No fast-path change, no VRAM growth, no new GE wait.** This sprint adds no texture and no `sceGuSync`; if a
  change would do either, it belongs to a later sprint.
- **Device for numbers, emulator for yes/no.** PPSSPP may confirm "it draws"; only the device confirms the bbox,
  parity and cost.
- **Stage only this sprint's paths.** Other sessions edit the tree; `git status` before every commit, never
  `git add -A`. Commit messages carry the task ID: `sprint 060 T1: ...`.

**Commands.**

```sh
export PSPDEV=/home/psp/pspdev
export PATH=$PSPDEV/bin:$PATH
set -e

cd /home/psp/raylib-psp
tools/pspgu-build.sh                     # pspgu installed; record the content hash
tools/pspgu-build.sh --check             # which driver is installed
tools/pspgl-build.sh --check             # pspGL reference must report 2ab7378e

build_lib() {                            # $1 = extra make vars
    ( cd src && make clean && make PLATFORM=PLATFORM_PSP -j8 $1 \
        && cp libraylib.a $PSPDEV/psp/lib/libraylib.a )
}

run_app() {                              # $1 = app dir, $2 = tag, $3 = make vars
    ( cd "$1" && make clean && make $3 && cp *.prx /tmp/s3/app.prx )
    ( cd /tmp/s3
      timeout 20 pspsh -e "reset"; sleep 6
      ( printf './app.prx\n'; sleep 40 ) | timeout 70 pspsh
      pspsh -e "scrshot host0:/$2.raw"
      pspsh -e "scrshot host0:/$2-b.raw" )
}

# Failing baseline on pspgu, then the same arms after each task.
run_app psp-tests/triangle_probe p10s1-mesh-pspgu 'EXTRA_CFLAGS=-DDRAW_MESH'
run_app psp-tests/batch_check    p10s1-scene-pspgu 'MESH_SCENE=1'

# Reference captures: restore pspGL, relink and run; then reinstall pspgu and relink.
tools/pspgu-build.sh --restore
run_app psp-tests/triangle_probe p10s1-mesh-pspgl 'EXTRA_CFLAGS=-DDRAW_MESH'
run_app psp-tests/batch_check    p10s1-scene-pspgl 'MESH_SCENE=1'
run_app psp-tests/scene_3d       p10s1-grid-pspgl 'SCENE=grid'
tools/pspgu-build.sh
run_app psp-tests/scene_3d       p10s1-grid 'SCENE=grid'

# Capture comparison: 480x272 RGBA, count differing pixels and the non-background bbox.
python3 - <<'EOF'
import sys
a = open('/tmp/s3/p10s1-mesh-pspgu.raw','rb').read()    # change the two paths per row
b = open('/tmp/s3/p10s1-mesh-pspgl.raw','rb').read()
assert len(a) == len(b) == 480*272*4
diff = sum(1 for i in range(0, len(a), 4) if a[i:i+4] != b[i:i+4])
xs = [ (i//4)%480 for i in range(0, len(a), 4) if a[i:i+4] != b'\0\0\0\xff' ]
ys = [ (i//4)//480 for i in range(0, len(a), 4) if a[i:i+4] != b'\0\0\0\xff' ]
print('diff_px', diff, 'bbox', (min(xs), min(ys), max(xs), max(ys)) if xs else None)
EOF
```

**Do not touch.** `src/rlgl.h`'s batcher, the window splitter or texture upload (`tools/pspgu/gl_texture.c`),
`src/platforms/rcore_psp.c`, `src/Makefile`, and every Phase 8/9/11/13 file with uncommitted work. The slow path in
`gl_draw.c` and the two probe apps are the only code in scope.

**Done means** §8 below, with evidence in each task row.

## 1. Objective

Falsifiable: for every client-array combination `DrawMesh`/`DrawModel` can emit, the packed vertex stride equals the
GU format's advertised size (decoded from `fmt`) — `0x1e3`/32 B for `DrawMesh`, `0x1ff`/36 B with colours,
`0x19c`/16 B colour+vertex, `0x180`/12 B vertex-only. `triangle_probe DRAW_MESH=1` lands on bbox
x 193..287, y 92..189 and is pixel-identical to pspGL; `batch_check MESH_SCENE=1` shows the mesh cube at
x 289..399, y 92..189 (the red immediate cube at x 81..191) with a diff no worse than the 1-px edge recorded in
sprint 013. A mesh whose index range exceeds the chunk ceiling draws all its triangles — parity with pspGL — in one
`drawElements` call, not one index at a time.

## 2. Why now

This is the first task in the Phase 10 task list and the precondition for every later measurement: S2 converts model
data, S3 maps lighting, S4 renders to texture, S5 culls; all of them need a mesh that draws and a deterministic
scene to compare. It also produces the vertex-format/byte-cost table Phase 10 lists as an exit criterion.

## 3. Scope

**In:** the slow (converting) path in `tools/pspgu/gl_draw.c`; the chunker and index rebasing in `glDrawElements`;
probe flags and capture comparison in `triangle_probe` and `batch_check`; a new `psp-tests/scene_3d` skeleton; the
Phase 10 page's blocker text and vertex-format table.

**Out:** textured/lit model parity, camera-relative rendering, matrix precision (S2); lighting and fog (S3);
render-to-texture (S4); culling and tessellation (S5); any pspGL change; any change to the 2D fast path.

## 4. Preconditions

- [ ] `pspsh` answers over USB; `/tmp/s3` is the PSP's `host0:` and has room for four 480×272 captures
- [ ] `tools/pspgu-build.sh --check` reports pspgu `b24` and its content hash; `tools/pspgl-build.sh --check`
      reports pspGL `2ab7378e`
- [ ] `EXTRA_CFLAGS` demonstrably reaches the compiler in both probe Makefiles (deliberate break or build log)
- [ ] Failing baseline captured for both arms on pspgu, and the pspGL reference captured for both, twice identical
- [ ] `libraylib.a` built from the current `src/` with defaults (`PSP_BATCH=1`); the 2D reference capture exists
- [ ] `PSPGU_BUILD` is `b24` before T1 and the plan is to bump it to `b25` in T1, not at the end
- [ ] Reset between runs; no app calls `sceKernelExitGame`, both log `DONE` and sleep

## 5. Tasks

| ID | task | status | verification | evidence |
|---|---|---|---|---|
| T1 | Replace the fixed `PspGuVertex` (36 B measured; colour +8, normal +12, position +24) with one `vertexLayout(fmt, &stride, &offTex, &offCol, &offNrm, &offVtx)` plus `packVertex()`: fields in GU order, stride = sum of enabled fields; allocate and `pspguWriteback()` `n*stride`; remove `sizeof(PspGuVertex)` from both chunk sizes; log once on unsupported array types (`GL_FLOAT`/`GL_UNSIGNED_BYTE` only) and non-`GL_UNSIGNED_SHORT` indices; bump `PSPGU_BUILD` to `b25` | `todo` | trace fmt/stride matches the layout table; boot line `b25`; cube draws | — |
| T2 | Fix the `glDrawElements` chunker: 2,048-index windows; when the chunk's index range does not fit the per-draw arena budget, dedup instead of the `n=1` fallback; 4,096-slot open-addressed table (`u32 key = index+1`, `u16 slot`; 24 KiB) + 2,048-entry slot map (4 KiB) as call-local scratch; counters `dedupChunks`, `maxRange`; add `SCENE=grid` to `scene_3d` (4,096 vertices, each 2,048-index window spanning ≥3,000 vertices) | `todo` | grid is pixel-identical to pspGL; one `drawElements` call; cube unchanged; `dedupChunks > 0` | — |
| T3 | Gate the fix: `triangle_probe DRAW_MESH=1` (identity cube, expected x 193..287, y 92..189) and `batch_check MESH_SCENE=1` (mesh at x=+1.2, expected x 289..399; immediate red cube x 81..191); two captures per arm per driver | `todo` | triangle_probe 0 differing pixels vs pspGL and bbox ±1 px; MESH_SCENE diff no worse than the 013 1-px edge; mesh present | — |
| T4 | Add `psp-tests/scene_3d`: fixed camera (0,1,4)→origin, flat cube and 64×64 checker cube via `DrawMesh`, `FRAMES=120`, `SetTargetFPS(0)`, `STAT`/`SPLIT`, counters, `DONE` + `sceKernelSleepThread()`; Makefile clones `scene_2d`'s counter rule (`psp_counters.o: ../bunnymark/psp_counters.c`) and `batch_check`'s app rules, with `-lpspgu` in `LIBS`; flags `SCENE=flat|tex|both|grid`, default `both`; reserve the later sprint arms but implement only these | `todo` | builds on pspgu and pspGL; two captures identical; `SCENE=both` logs 2 `drawElements` | — |
| T5 | Regression and write-back: 2D `batch_check` without `MESH_SCENE` against `/tmp/s3/par_pspgl.raw` (0 diff, 613 colours); `basic_shapes` default build (284 colours); `bunnymark` `BUNNY_COUNT=1000 FRAMES=200 TARGET_FPS=0 WRAP=1` (work 6.565 ms pspgu ±0.2 ms, two runs); vertex-format/byte-cost table on the Phase 10 page (32-bit converting formats only; S2 extends it); confirm the corrected blocker 1 text is what ships | `todo` | gates pass; table matches measured strides; boot line carries `b25` | — |

### Design notes for T1/T2 (implementation detail)

**Layout.** GU order is texture, colour, normal, position. Widths: `GU_TEXTURE_32BITF` 8 B, `GU_COLOR_8888` 4 B,
`GU_NORMAL_32BITF` 12 B, `GU_VERTEX_32BITF` 12 B. Offsets are cumulative; there is no padding because every field
starts 4-byte aligned. The combinations raylib emits:

| enabled arrays | fmt | stride | where it comes from |
|---|---|---|---|
| tex, normal, vertex | `0x1e3` | 32 B | `DrawMesh` with `GenMeshCube` and default material (the failing case) |
| tex, colour, normal, vertex | `0x1ff` | 36 B | the same with `mesh.colors` |
| normal, vertex | `0x1e0` | 24 B | mesh without UVs |
| tex, colour, vertex | `0x19f` | 24 B | unlit textured mesh with vertex colours |
| tex, vertex | `0x183` | 20 B | unlit textured mesh |
| colour, vertex | `0x19c` | 16 B | coloured mesh without UVs/normals |
| vertex | `0x180` | 12 B | positions only |

`guVertexFormat()` already builds exactly these bits from `pspgu.clientBits`; T1 changes the *packer* to agree with
it, not the format function. A colour array is absent in `DrawMesh`'s default path, and that is the whole bug: the
old 36-B record reserves the colour slot, so GU reads normals at +8 and positions at +20 while the data sits at +12
and +24.

**Packer.** `packVertex(unsigned char *dst, int fmt, int index)` writes only enabled fields at their layout offsets.
`glDrawArrays` and `glDrawElements` allocate `n*stride` from the list arena, pack, write back `n*stride`, and emit
`sceGuDrawArray(prim, fmt, n, ...)`; there is no stride argument — GU derives it from the format bits, so the data
must be packed at exactly that stride. Cap the per-call count at `min(PSPGU_MAX_CONVERT, arenaBytes/stride)` so one
huge draw cannot force an arena reset.

**Chunker.** Choose per chunk: convert the index range when `range*stride + n*2` fits a 96 KiB per-draw budget
(leaving 32 KiB of the 128 KiB arena for the rest of the frame); otherwise dedup. Probe a 4,096-slot table with
`index % 4096`, linear-probe on collision, assign slots in first-seen order, pack unique vertices, and emit rebased
indices `slot[i]`. The table is two parallel arrays — `u32 key[4096]` where `key = index + 1` and 0 means empty
(0xFFFF is a legal vertex index), plus `u16 slot[4096]` — 24 KiB total, and the slot map is 4 KiB. This scratch
lives in main RAM; the GE never reads it. Only the packed vertices (≤72 KiB worst case) and rebased indices (4 KiB)
come from the list arena, so the GE-referenced set is 76 KiB and leaves room for the frame's other draws. Report
`dedupChunks` and `maxRange` in the frame log, and require `arenaOverflows == 0` for the gate runs; a reset is a full
GE wait and invalidates the "no new wait" claim.

**Scene arms.** `SCENE=grid` is a synthetic 64×64-vertex plane whose index buffer makes each 2,048-index window
reference at least 3,000 distinct vertices (for example triangles `(i, i+2048, i+4095)`, clamped); that is what
forces the dedup path and what pspGL renders without chunking. The checker texture is generated at runtime
(`GenImageChecked` + `LoadTextureFromImage`), so no asset or licence is introduced; `SCENE=both` draws one flat and
one textured cube and must log exactly 2 `drawElements` calls.

Rules:

- **IDs are stable.** Never renumber; a task that no longer applies becomes `dropped` with the reason, so the
  history stays readable.
- **`done` requires evidence** in §7 — a trace row, a capture path, a measurement. Code written is `doing`.
- **`blocked` needs a dependency**: "blocked by T1", not "blocked".

### Task log (append-only)

- 2026-11-22 — sprint written, all tasks `todo`

## 6. Budgets

| budget | limit | measured | tag |
|---|---|---|---|
| frame time | no target this sprint; record median/p95 for S2 | | |
| converting cost | record µs for one cube (36 indices); fast path must not move | | |
| list/arena memory | dedup GE-referenced set ≤76 KiB of the 128 KiB arena; scratch 28 KiB in main RAM; `arenaOverflows == 0` | | |
| VRAM | 0 delta for the flat arm; checker texture 16 KiB in the textured arm | | |
| boot tag | `b25` after T1; a `b24` line invalidates the run | | |

## 7. Measurements

Protocol: device only; control (pspGL) → change (pspgu) → control again; two identical captures.

| run | build | workload | metric | value | notes |
|---|---|---|---|---|---|
| control | pspGL `2ab7378e` | `triangle_probe DRAW_MESH=1` | bbox / pixels | | expected x 193..287, y 92..189 |
| failing | pspgu `b24` | `triangle_probe DRAW_MESH=1` | fmt / allocated stride / pixels | `0x1e3` / 36 / 0 | format advertises 32 |
| control | pspGL `2ab7378e` | `batch_check MESH_SCENE=1` | mesh bbox / diff | | mesh x 289..399 |
| failing | pspgu `b24` | `batch_check MESH_SCENE=1` | fmt / stride / mesh pixels | | mesh absent |
| control | pspGL `2ab7378e` | `scene_3d SCENE=grid` | pixels | | 4,096-vertex reference |
| fixed | pspgu `b25` | `triangle_probe` + `MESH_SCENE` | bbox / diff | | gates below |
| fixed | pspgu `b25` | `SCENE=grid` | pixels / `dedupChunks` | | parity vs pspGL |
| fixed | pspgu `b25` | one cube | converting µs | | fast path unchanged |
| waits | pspgu `b25` | gate + grid | `arenaOverflows` / full waits | | target 0 |

## 8. Exit criteria

- [ ] §1 met: every emitted format's stride equals its decoded size; `triangle_probe` is 0 diff from pspGL and
      ±1 px of the analytic bbox; `MESH_SCENE` shows the mesh cube at x 289..399 with no new diff
- [ ] 2D gate: `batch_check` 2D 0 differing pixels and 613 colours; `basic_shapes` 284 colours
- [ ] a mesh with an index range above the chunk ceiling draws all its triangles, pixel-identical to pspGL, in one
      `drawElements` call
- [ ] `scene_3d` builds on both drivers and captures deterministically, 120 frames, `STAT`/`SPLIT` present
- [ ] boot line reports `b25`; no new GE wait (`arenaOverflows == 0`), no fast-path change, no unintended VRAM growth
- [ ] unsupported array/index types log once and skip, instead of reinterpreting bytes or silently returning
- [ ] vertex-format/byte-cost table and the corrected blocker written into the Phase 10 page

## 9. Result

—

## 10. Follow-ups

- Textured and lit model parity, camera-relative rendering, matrix precision →
  [P10-S2 — 061](061-phase10-camera-relative-and-ge-native-models.md)
- Lighting, fog and texture environment mapping → [P10-S3 — 062](062-phase10-lighting-fog-texenv.md)
- Render-to-texture backend → [P10-S4 — 065](065-phase10-render-to-texture.md)
- Culling, guard-band clipping, ME option → [P10-S5 — 066](066-phase10-culling-and-guard-band.md)
- Frozen sample and phase close → [P10-S6 — 067](067-phase10-sample-target-and-close.md)
