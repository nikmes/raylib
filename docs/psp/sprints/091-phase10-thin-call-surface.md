# Sprint 091 (P10-S21) — Thin call surface: matrix epoch and cached MVP

| | |
|---|---|
| Roadmap phase | [Phase 10 — 3D and advanced](../phases/10-3d-advanced/index.md) |
| Sprint | P10-S21 |
| Branch / base commit | `master` @ `6a03bce9` — design: [Redesigning the 3D path](../phases/10-3d-advanced/pspgu-redesign.md) |
| Started / closed | 2026-11-24 (written) / — |
| Build number(s) | driver `b38` only if adopted; the arms are `PSPGU_MATRIX_EPOCH=1`, `PSPGU_STATE_SKIP=1` |
| Tasks | 0/6 — T3 reframed and T4 promoted by the readiness review (§4b) |

## 0. Handover

S20 removes the clip scan from clean per-object draws. What remains of the 180 µs per cube is the call surface: a
`DrawMesh` today does `rlPushMatrix` + `rlMultMatrixf` + three `glEnableClientState` + three `gl*Pointer` +
`rlColor4ub` + `glDrawElements`, and the driver then **recomputes and re-uploads** both matrices through
`pspguUploadMatrices` (`gl_matrix.c:112`). The cull has already composed the MVP to classify the mesh, and that
result is thrown away.

M3D's surface, per cube, is one `sceGuSetMatrix(GU_MODEL, m)` and one `sceGuDrawArray` (`AMG_VFPU.cpp:109`,
`AMG_Model.cpp:1148`).

## 1. Objective

Remove the duplicated work between classification and draw: cache the MVP, add a matrix epoch so the copy is not
recomputed, and skip state that did not change. Adopt only with 0 pixel diff.

## 2. Why now

After S20 this is the second-largest block, and it is cheap to remove because the data already exists. It is also the
prerequisite for S22's thin entry: a baked-mesh draw needs an MVP it can trust without recomposing.

## 3. Scope

**In:** per-line instrumentation of the surface; a PSP-guarded matrix epoch in rlgl; a cached MVP carried from cull to
draw; skipping unchanged texture/pointer/colour setup.

**Out:** vertex production (S22), submission, 2D, desktop paths (the code must not change behaviour when
`PLATFORM_PSP` is absent).

## 4b. Readiness review (2026-11-28)

Reviewed against `master @ 6a03bce9`, after 090 and 092 measured the per-object path at **~139 µs/cube** (GRID=14,
`PSP_MERGE_DRAWS=0`) and showed that removing per-frame vertex packing bought only 0.8 ms. The surface is confirmed
as the target; three details change how to attack it.

1. **T3's premise is wrong twice.** "Reuse the cull's MVP for the draw" fails because (a) the GE lights with the
   **model** matrix — its inverse-transpose — so a projection-inclusive MVP cannot stand in for `GU_MODEL`; and
   (b) GL 1.1 exposes only `GL_MODELVIEW`, so the driver never has `V` and `M` apart and cannot hand the GE a view
   and a model to compose. The cull's `MVP` is usable for the cull and nothing else. T3 is reframed to "skip a
   redundant matrix upload when the composed modelview is unchanged", which only helps identical transforms and
   may be dropped at T1.
2. **T2 is smaller than it reads.** The cull already caches `P*V` by value (`pspCullPVValid`, two 64-byte `memcmp`s,
   `rmodels.c:1554-1559`). An epoch would replace two memcmps with one integer compare — real, but micro. Keep it
   only if T1's profile names it.
3. **T4 is the item with weight.** The showcase draws **one** cube mesh 196 times per frame, and every draw re-issues
   three `glEnableClientState` + three `gl*Pointer` + colour + texture enable. Skipping the pointer/state block when
   the mesh and colour are unchanged is the largest removable piece the profile is likely to find. T4 is promoted
   to the first implementation task after T1.

**Base numbers.** The sprint's "−10–20 µs/cube" was written against a 180 µs/cube estimate. The measured figure is
139 µs, so the top of that range is ~14 %; T1 sets the target from the profile rather than the estimate, and T5's
adoption bar stays "beats the control by >0.05 ms with 0 diff".

**Benchmark discipline** carries over from 090: run below a vblank step, report `work` beside `avg`, and keep the
`clean_skips`-style counter so a skipped block is visible as skipped rather than assumed.

## 4. Preconditions

- [x] `PSP_MATRIX_PROBE` and `PSP_CALL_PROBE` counters exist (078)
- [x] S20's numbers exist, so a win here is attributable
- [ ] the epoch has a single place where matrices change (verify before writing)

## 5. Tasks

| # | Task | Status | Acceptance | Result |
|---|---|---|---|---|
| T1 | **Instrument the surface.** Per-line cost at N=500: matrix push/mult, pointer setup, colour, texture enable, driver upload | `todo` | the lines sum to the measured delta ±10 % | — |
| T2 | **Matrix epoch.** rlgl keeps a revision counter bumped on every matrix change; the cull and the draw both read it | `todo` | no stale matrix in any gate; `PSP_MATRIX_PROBE` cull ≤8 µs | — |
| T3 | **Matrix upload skip.** Skip `pspguUploadMatrices` when the composed modelview is unchanged since the last draw (identical transforms only; see §4b 1 — the cull's MVP cannot feed the draw) | `todo` | no stale matrix in any gate; the counter reports how often it fires | — |
| T4 | **State skip (promoted, §4b 3).** Skip `glEnableClientState`/`gl*Pointer`/`rlEnableTexture`/colour when the mesh and colour are unchanged since the previous draw | `todo` | no missed state change in the gates; counters report skips; measured on the 196-cube scene where one mesh repeats | — |
| T5 | **A/B + gates.** Below a vblank step and at GRID=20 for the step distance, two runs; report `work`; full pixel table | `todo` | slope falls by the measured surface cost; 0 diff | — |
| T6 | **Close.** Adopt or drop with the number | `todo` | default state explicit | — |

## 6. Design notes

**Why an epoch and not a dirty flag.** A dirty flag per matrix is what the driver already has
(`gl_matrix.c:57-63`), but it answers "did the bits change", not "is my composed value still valid". The epoch is one
integer compared once per draw, and it is the only way to reuse the cull's compose without risking a stale product.

**Where the epoch must be bumped.** Every `glLoadIdentity`, `glLoadMatrix`, `glMultMatrix`, `glRotate`, `glTranslate`,
`glScale`, `glMatrixMode` switch and GUM helper that writes a matrix. Missing one is a stale frame, which the pixel
gates catch — but only on scenes that exercise that path, so T2's acceptance includes the 3D scene, RTT and
`MESH_SCENE`.

**The risk is shared code.** Everything here sits in `src/rlgl.h`, used by desktop builds too. All of it goes inside
`PLATFORM_PSP` guards, and the desktop build must be unchanged — the design's §6 names this as the sprint most
likely to grow.

**No GUM.** The upload is already `sceGuSetMatrix` with a dirty check, and M3D's hot path does the same; reintroducing
GUM would add a CPU stack in front of a product we already have (redesign §3.6).

## 7. Gates

| gate | expected |
|---|---|
| `batch_check` / `basic_shapes` | 613 / 284 |
| `scene_3d SCENE=both`, `MESH_SCENE`, RTT | 0 diff |
| `clip_probe` | 237/720 |
| camera/matrix | no stale matrix across a mode switch or an RTT pass |

## 8. Exit criteria

- [ ] per-line costs exist before and after, on the same scene
- [ ] the epoch covers every matrix write, proven by the gates
- [ ] one compose per draw, stated by the counters
- [ ] adopted only with 0 diff; otherwise dropped with the number
- [ ] desktop build unaffected; strict docs pass

## 9. Result

### 9.1 T1 — the split (`done`, 2026-11-28)

The counters existed since 078 but nothing printed them; `rlpspProbeReport()` now emits one line per 2,000 calls
(`CALLPROBE calls=2000 drawmesh=… cull=… driver=… self=…`) and resets. Run on console `0x1C9`, GRID=14, merge off,
**default driver (clip hint off)**, `PSP_CALL_PROBE=1 PSP_MATRIX_PROBE=1`:

| component | µs/call | share |
|---|---|---|
| `DrawMesh` total | 155-158 | — |
| cull | 23.4 | 15 % |
| **driver** (`rlDrawVertexArrayElements`) | **65.0** | **42 %** |
| residual (rlgl/raylib surface) | 67.5 | 43 % |

Ten consecutive windows agree within 2 %. The probe costs ~4 µs per timing pair and the call carries five or six of
them, so ~17 µs of the total is instrument; the shares hold.

**Correction to this sprint's premise.** 091 assumed the remainder was the call surface *above* the driver. It is
roughly half and half: the driver is 42 %, not a footnote.

**The driver's 65 µs is not all driver work.** This run used the default driver, so it still runs the clip scan that
090 removes — measured on its own at **~41 µs/call**. With `PSPGU_CLEAN_HINT=1` the driver component should fall to
~24 µs, and the per-call total to ~115 µs. That re-run is the next measurement and it decides whether T2/T3 (matrix
upload) or T4 (state skip) is worth writing.

### 9.2 The same run with the hint on (`PSPGU_CLEAN_HINT=1`)

| component | hint off | hint on | Δ |
|---|---|---|---|
| `DrawMesh` total | 155-158 | 123.0 | **−33** |
| cull | 23.4 | 24.3 | — |
| **driver** | **65.0** | **27.7** | **−37** |
| residual (surface) | 67.5 | 70.9 | — |

`clean_skips=9800` again (24.5/frame). The driver's drop matches the clip pass plus the per-triangle expansion a
dirty mesh triggers — that expansion, not the scan, is why the hint was worth 8 ms.

**T1 is answered.** With the hint on, the per-call split is **cull 24 (20 %), driver 28 (22 %), surface 71 (58 %)**.
Subtracting the instrument (the surface carries the two matrix probe pairs, ~8 µs), the surface is still the largest
single block at ~60 µs.

### 9.3 What that predicts

- The prediction held: driver ~24 predicted, 27.7 measured; the surface is the largest block.
- **T4 is the right next task**, but it will not close the gap on its own: pointer/enable skips are worth a few µs,
  while the surface's bulk is per-draw matrix work (`rlPushMatrix`, a 4×4 `MatrixMultiply` for the bake, and
  `rlMultMatrixf` through rlgl on a scalar path). That is the "thin entry" the redesign described, and it is larger
  than T4 as written.
- The driver's remaining 28 µs is matrix upload (two 64-byte converts + two `sceGuSetMatrix`) plus emission —
  T3's territory, and the cull already composes `P*V`, which is why T3 is not free.

### 9.4 Open

- Re-run with the hint driver to confirm the 41 µs drop (needs the console).
- The probe build inflates every figure above; compare arms of the same build, never across.

## 10. Follow-ups

- If the surface is already thin, S22 can skip T3 and go straight to baking.
- If the epoch proves fragile, a per-frame "matrix generation" on the camera is the fallback: bump once when the
  camera or model changes, not per matrix call.

## 11. References

- [Redesign](../phases/10-3d-advanced/pspgu-redesign.md) — §2, §3.1, §3.6
- `tools/pspgu/gl_matrix.c:55-116` — upload and its dirty check
- `src/rlgl.h` — matrix stack, `rlMultMatrixf`, pointer setup
- sprint 070 (camera/matrix cost, GUM dropped), 077 (per-call attribution)
