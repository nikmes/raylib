# Sprint 060 (P10-S1) — DrawMesh conversion fix and the 3D mesh gate

| | |
|---|---|
| Roadmap phase | [Phase 10 — 3D and advanced](../phases/10-3d-advanced/index.md) |
| Sprint | P10-S1 |
| Branch / base commit | `master` @ `f04bac38` (phase 10 readiness review and RTT references) |
| Started / closed | 2026-11-22 (written) / — |
| Build number(s) | pspgu `b24` + the T1/T2 change (content hash recorded by `--check`); `libraylib.a` default `PSP_BATCH=1` |
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
   `glDrawElements()` and the chunker.
4. `src/rmodels.c:1450-1495` — which client arrays `DrawMesh` enables, and why `tex2d=0` for
   `LoadMaterialDefault` (`rlGetTextureIdDefault()` is 0 on GL11, `src/rlgl.h`).
5. `src/rlgl.h:4406-4420` — `rlDrawVertexArrayElements()` passes the index buffer through unchanged.
6. `psp-tests/triangle_probe/main.c` and `Makefile` — `DRAW_MESH` arm, predicted bbox x 193..287, y 92..189;
   `EXTRA_CFLAGS` is now wired (it used to be dropped silently).
7. `psp-tests/batch_check/main.c:114-138` and `Makefile:32-35` — the `MESH_SCENE` arm: 2D → `DrawCube` + `DrawMesh`
   → 2D in one frame.
8. `../memory-map.md` §4 — list arenas and VRAM; §6 — the measurement protocol.

**First action.** Build the current tree against pspgu and capture the *failing* baseline before editing anything:
`triangle_probe` with `EXTRA_CFLAGS=-DDRAW_MESH` and `batch_check` with `MESH_SCENE=1`, pspgu and pspGL, two captures
each. Record fmt, stride, draw calls and non-black pixel count. No fix without a before row.

**Hard rules.**

- **The 2D gate is sacred.** After every change, `batch_check` 2D must be 0 differing pixels against pspGL
  (`/tmp/s3/par_pspgl.raw` or a fresh same-build capture). The fast path's 24-byte vertex must not change.
- **Suspect the instrument first.** `triangle_probe`'s Makefile once dropped `EXTRA_CFLAGS`, so `-DDRAW_MESH`
  compiled the default; `batch_check` once read `CFLAGS = -Wall` without `EXTRA_CFLAGS`. Prove the build contains
  the arm: a changed `psp-nm`, a boot log line naming the arm, or a deliberate break.
- **One variable per run.** Control (pspGL) → change (pspgu) → control again. Two identical captures before a
  capture is evidence; a `scrshot` taken while the app renders freezes it, so capture after `DONE`.
- **Pack only what the format advertises.** GU's field order is fixed: texture, colour, normal, position. The stride
  is the sum of the enabled fields; the struct's size is not the format's size.
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

# Failing baseline, then the same two arms after each task.
run_app psp-tests/triangle_probe p10s1-mesh-pspgu 'EXTRA_CFLAGS=-DDRAW_MESH'
run_app psp-tests/batch_check    p10s1-scene-pspgu 'MESH_SCENE=1'
```

**Do not touch.** `src/rlgl.h`'s batcher, the window splitter or texture upload (`tools/pspgu/gl_texture.c`),
`src/platforms/rcore_psp.c`, `src/Makefile`, and every Phase 8/9/11/13 file with uncommitted work. The slow path in
`gl_draw.c` and the two probe apps are the only code in scope.

**Done means** §8 below, with evidence in each task row.

## 1. Objective

Falsifiable: for every client-array combination `DrawMesh`/`DrawModel` can emit, the packed vertex stride equals the
GU format's advertised size (decoded from `fmt`), and the cube drawn through `glDrawElements` lands on the pspGL
reference — predicted bbox x 193..287, y 92..189 for the identity transform — with 0 differing pixels for the
untextured cube. A mesh whose index range exceeds the chunk ceiling must draw all its triangles, not one index at a
time.

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
- [ ] `tools/pspgu-build.sh --check` reports a pspgu build and its content hash; `tools/pspgl-build.sh --check`
      reports pspGL `2ab7378e`
- [ ] `EXTRA_CFLAGS` demonstrably reaches the compiler in both probe Makefiles (deliberate break or build log)
- [ ] Failing baseline captured for both arms on pspgu, and the pspGL reference captured for both, twice identical
- [ ] `libraylib.a` built with defaults (`PSP_BATCH=1`); the 2D reference capture exists
- [ ] Reset between runs; no app calls `sceKernelExitGame`, both log `DONE` and sleep

## 5. Tasks

| ID | task | status | verification | evidence |
|---|---|---|---|---|
| T1 | Replace the fixed 36-B `PspGuVertex` packer with per-format packing: offsets computed once per draw in GU order (texture, colour, normal, position), stride = sum of enabled fields; trace `fmt`, stride and vertex count | `todo` | trace stride equals the format's decoded size for each arm; cube draws | — |
| T2 | Fix the `glDrawElements` chunker: chunk by index window, and replace the `n=1` fallback for `range > chunk` with a bounded dedup pack so every triangle is submitted; counter for the dedup path | `todo` | 4,096-vertex spread-index grid draws all triangles; `drawElements` count unchanged; cube unchanged | — |
| T3 | Gate the fix: `triangle_probe DRAW_MESH=1` and `batch_check MESH_SCENE=1` on pspGL and pspgu; bbox extraction from the raw captures | `todo` | bbox within 1 px of x 193..287, y 92..189; 0 differing pixels for the untextured cube | — |
| T4 | Add `psp-tests/scene_3d`: fixed camera, flat cube via `DrawMesh` and textured cube (64×64 checker) via `DrawMesh`, `STAT`/`SPLIT`, `DONE` + sleep, deterministic capture; borrow `psp_counters.o` as `scene_2d` does | `todo` | builds on both drivers; two captures identical; counters show 2 `drawElements` | — |
| T5 | Regression and write-back: 2D gate, `basic_shapes` 284 colours, `bunnymark` N=1,000 canary; vertex-format/byte-cost table on the Phase 10 page; correct blocker 1 | `todo` | 0 diff 2D; 284 colours; work within noise; table matches measured strides | — |

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
| list memory | record high-water for the 3D frame | | |
| VRAM | 0 delta for the flat arm; checker texture 16 KiB in the textured arm | | |

## 7. Measurements

Protocol: device only; control (pspGL) → change (pspgu) → control again; two identical captures.

| run | build | workload | metric | value | notes |
|---|---|---|---|---|---|
| control | pspGL `2ab7378e` | `triangle_probe DRAW_MESH=1` | bbox / pixels | | |
| failing | pspgu `b24` | `triangle_probe DRAW_MESH=1` | fmt / stride / pixels | `0x1e3` / 36 / 0 expected | |
| control | pspGL `2ab7378e` | `batch_check MESH_SCENE=1` | mesh bbox / diff | | |
| failing | pspgu `b24` | `batch_check MESH_SCENE=1` | fmt / stride / pixels | | |
| fixed | pspgu T1 | both arms | bbox / diff | | |
| fixed | pspgu T2 | 4,096-vertex grid | triangles submitted | | |

## 8. Exit criteria

- [ ] §1 met: every emitted format's stride equals its decoded size; the cube matches the pspGL reference
- [ ] 2D gate: `batch_check` 2D 0 differing pixels; `basic_shapes` 284 colours
- [ ] a mesh with an index range above the chunk ceiling draws all its triangles
- [ ] `scene_3d` builds on both drivers and captures deterministically
- [ ] no new GE wait, no fast-path change, no unintended VRAM growth
- [ ] vertex-format/byte-cost table and the corrected blocker written into the Phase 10 page

## 9. Result

—

## 10. Follow-ups

- Textured and lit model parity, camera-relative rendering, matrix precision → **P10-S2**
- Lighting, fog and texture environment mapping → **P10-S3**
- Render-to-texture backend → **P10-S4**
- Culling, tessellation, ME option → **P10-S5**
