# Sprint 022 (P7-S5) — Particles: one stream per texture run and the cap boundary

| | |
|---|---|
| Roadmap phase | [Phase 7 — 2D specialisation](../phases/07-2d-specialisation/index.md) |
| Sprint | P7-S5 |
| Branch / base commit | `master` @ `d9a83d81` (sprint 021 closed) |
| Started / closed | 2026-11-21 / 2026-11-21 |
| Build number(s) | pspgu `b24`; `libraylib.a` `PSP_CULL=1 PSP_TRI_FAST=1`; `scene_2d` api/direct |
| Tasks | 5/5 done |

## 0. Handover

**Situation.** Sprint 018 measured particles at **3.285 ms** work for 500 and **11.844 ms** for 2,000, with
simulation **0.443 / 1.775 ms** and **1 draw/frame** at both (4,000 vertices at 2,000, just under the 4,096-vertex
cap). So the phase's "one dynamic stream per texture run" is already true at the GL boundary; the cost is CPU
vertex generation (~5 µs/particle), not submission. Above 2,048 particles the cap adds a second draw. This sprint
measures whether a run-batched particle loop (the same probe shape sprint 019 used for text) removes enough CPU
cost to justify promoting it, and records the cap/tearing boundary.

**Read before acting**, in this order:

1. `../sprints/018-phase7-corpus-and-budget.md` (P7-S1) §7 — the particle and mixed counters, the 2,000-particle point.
2. `../sprints/019-phase7-text-work.md` (P7-S2) §7 — the `api`/`direct` method and what it proved for a per-object loop.
3. `psp-tests/scene_2d/main.c` — `MODE=particles`, `particlesUpdate`, the freeze frames.
4. `src/rlgl.h:622-930` — the batcher: one bind per run, 2-vertex sprites, the 4,096 cap.
5. `../phases/07-2d-specialisation/index.md` — the particle exit criterion.

**First action.** Add `PARTICLE_IMPL=api|direct` and a per-run `MARKER=` to `scene_2d`, where `direct` is one
`rlSetTexture` + `rlBegin(RL_QUADS)` loop with the same geometry, and run `COUNT=500/2000` before any `src/`
change. The marker name matters: other sessions run the same app, and the default `scene2d_done.txt` was
overwritten during sprint 021.

**Hard rules.**

- **Same geometry.** `direct` must produce the same quad as `DrawTexturePro` for an unrotated, uniformly scaled
  particle; captures are byte-identical for a fixed final state.
- **Simulation stays separate.** `sim` is measured around the update loop only, as in 018; do not fold it into
  `work`.
- **A stream is per texture run.** One texture for the whole system; a bind in the middle of the loop is a bug,
  not a tuning detail.
- **The cap is evidence, not a knob.** Do not raise `RLPSP_BATCH_CAP`; use **2,049** as the clean first-second-draw
  point (4,098 vertices, one arena) and record 4,000 only as the arena-overflow case. The arena is 128 KiB,
  24 B/vertex, so 5,461 vertices is its ceiling; 4,000 particles (8,000 vertices) forces the full-sync fallback.
- **No driver change.** pspgu stays `b24`; the 512-window path must not be touched.
- **Freeze before capture.** Particles move; render three frozen frames after the measured ones so two captures
  can be compared, as 018 does.
- **Stage only this sprint's paths.** `git status` before commit.
- Commit messages carry the task ID: `sprint 022 T2: ...`.

**Commands.**

```sh
export PSPDEV=/home/psp/pspdev
export PATH=$PATH:$PSPDEV/bin

cd /home/psp/raylib-psp/psp-tests/scene_2d
make clean && make MODE=particles COUNT=2000 PARTICLE_IMPL=api MARKER=s22_api.txt
make clean && make MODE=particles COUNT=2000 PARTICLE_IMPL=direct MARKER=s22_direct.txt
make clean && make MODE=particles COUNT=2049 PARTICLE_IMPL=api MARKER=s22_cap.txt
cp scene_2d.prx /tmp/s3/s22_app.prx
cd /tmp/s3 && timeout 20 pspsh -e "reset"; sleep 6
( printf './s22_app.prx\n'; sleep 16 ) | timeout 35 pspsh
pspsh -e "scrshot host0:/s22.raw"          # after DONE only
```

Use a unique PRX name (`s22_app.prx`) and the `MARKER` file; the shared `app.prx`/`scene2d_done.txt` are used by
other sessions.

**Do not touch.** The text path, the 512-window splitter, the shape/sprites work, the allocator, `DrawMesh`.

**Done means** §8: the direct probe and its decision, 1 draw at 2,000, the 2,049/4,000 cap table, two identical
captures after a 300-frame run, and the regression hashes.

## 1. Objective

Four falsifiable results:

1. A **direct particle loop** (one bind, one begin/end per system) is compared with `DrawTexturePro` at 500 and
   2,000 particles; the decision rule is at least 10 % work reduction with identical pixels.
2. **One draw per stream** at 2,000 particles; **two draws** at 2,049 (the clean cap crossing), with 4,000
   recorded as the arena-overflow case and the vertex/arena arithmetic written.
3. **Simulation and rendering are separate numbers** at every point.
4. **No tearing and no mid-run bind:** two captures after a 300-frame run are identical; binds are one per
   system, not one per particle.

## 2. Why now

Particles are the last unmeasured 2D workload before the cache-strip experiment and the phase close. The 018
point already shows the submission is right; this sprint decides whether the remaining per-particle CPU cost is
worth a promoted fast path or is a recipe for games.

## 3. Scope

**In:** `scene_2d` particle implementation arm and cap points; the measurement and decision; a recipe if the
direct loop wins.

**Out:** a new public raylib API, the batcher's primitive choice, the cap/arena, the 512-window path, the text
and shape paths, 3D.

## 4. Preconditions

- [x] Sprint 018 baselines (500: 3.285/0.443 ms; 2,000: 11.844/1.775 ms; 1 draw) are in `sprint018/`
- [x] pspgu `b24`, gate `b69830b8…`
- [x] `scene_2d` particle mode is deterministic and freezes before capture
- [x] Device loop available; pspGL restorable (`2ab7378e`)

## 5. Tasks

| ID | task | status | verification | evidence |
|---|---|---|---|---|
| T1 | **Corpus**: `PARTICLE_IMPL=api\|direct`; `direct` is one bind/begin/end with the same quad; per-run `MARKER`; counts 500/2,000/2,049/4,000 | `done` | direct captures equal api at a fixed final state; sim/work separate; marker name unique | Both arms run 300 frames, two runs each, with unique PRX/marker names. Direct captures byte-identical to api at 500 and 2,000; sim 0.140/0.524–0.580/0.554/1.098 ms |
| T2 | **Direct-loop measurement**: 500/2,000 api vs direct, two runs, same-session baseline | `done` | work table; decision (≥10 % → promote, else null) | api **3.064/3.063**, direct **2.218/2.217** at 500 (−27.6 %); api **10.928/10.928**, direct **7.804/7.806** at 2,000 (−28.6 %). Decision: **promote** (recipe added), captures identical |
| T3 | **Cap boundary**: 2,000 / 2,049 / 4,000, draws and vertices | `done` | 2,000 = 1 draw, 2,049 = 2; 4,000 = 2 but arena-bound; arithmetic recorded | 2,000: 4,000 verts, 1 draw, 10.928 ms. 2,049: 4,098 verts, **2 draws**, 11.238/11.237 ms. 4,000: 8,000 verts, 2 draws, **22.104 ms**, **303 arena full-sync fallbacks** (192 KiB > 128 KiB arena) |
| T4 | **Tear and bind audit**: 300 frames, two captures, binds per run | `done` | captures identical; one bind per system; no mid-loop bind | Every point's two captures are byte-identical within and across runs; **1 bind per run** at all counts (not per particle); no mid-loop bind |
| T5 | **Regression**: 018–021 captures, gate, VRAM/residency | `done` | hashes unchanged; gate `b69830b8…`; 0 evictions | shapes **13.220 ms**, mixed **10.658 ms**, captures match 021; text/tiles from 021 unchanged; gate **b69830b8…**; every run 0 evictions/reloads/failures |

Status vocabulary: `todo` / `doing` / `done` / `blocked` / `dropped`. IDs are stable.

### T1 design

- `PARTICLE_IMPL=api` is today: `DrawTexturePro` per particle.
- `PARTICLE_IMPL=direct`: `rlSetTexture(particleTex.id)`, `rlBegin(RL_QUADS)`, one colour and four vertices per
  particle in `DrawTexturePro`'s TL/BL/BR/TR order, `rlEnd()`, `rlSetTexture(0)`. The Makefile adds
  `-I../../src -DPLATFORM_PSP -DGRAPHICS_API_OPENGL_11 -DPSP_BATCH=1` for this arm, as sprint 019's direct arm
  does; without them the app links the non-inline rl* declarations and fails.
- `MARKER ?= scene2d_done.txt` becomes `-DSCENE_MARKER="\"$(MARKER)\""`, and `SaveFileText(SCENE_MARKER, ...)`
  replaces the hardcoded name, so a concurrent session cannot delete the marker between the run and the copy.
- Counts: 500, 2,000 and 2,049 for the direct/cap A/B, plus 4,000 sampled once for the arena-overflow record.
- The same update loop, seed and frozen final frame for both arms.

### T2 design

- Run 500 and 2,000, two runs each, 300 frames, against the **same-session** `api` baseline on the sprint 021
  library; the 018 numbers are the historical reference, not the A/B denominator.
- Decision: promote only if `direct` work is at least 10 % below `api` **and** the capture is byte-identical.
  Promotion means adding the loop as a **recipe** (`docs/psp/recipes/`) and using it in the corpus; it does not
  mean a new public API unless a separate review asks for one.
- If the win is <10 %, record the null result with the table and keep `api`.

### T3 design

- 2,000 particles = 4,000 vertices, one draw. 2,048 = 4,096, exactly the cap. **2,049 = 4,098, two draws**, the
  first clean crossing; the second draw is 2 vertices.
- 4,000 = 8,000 vertices, two draws, but the per-list arena is 128 KiB = **5,461 vertices** at 24 B each, so the
  frame forces the full-sync fallback. Record it as an arena-bound case, not as a clean cap result.
- If a larger single draw is proposed, the cap/arena arithmetic is the evidence; do not change either in this
  sprint.

### T4 design

- The scene already freezes the final state for three frames; two captures after DONE must be identical.
- Binds: one per system at the boundary; a bind inside the particle loop is a bug. The `--wrap` counters report
  binds/array vertices; the marker includes them.

### T5 design

- Rebuild with the sprint 021 library; run text 8/24, tiles, shapes, mixed, oversize and the wide atlas; compare to
  the 018–021 captures. `batch_check` vs `par_pspgl.raw`; VRAM/residency.

## 6. Budgets

| item | budget | why |
|---|---|---|
| counts | 500 / 2,000 / 2,049 / 4,000 | below, at, just over, and over the arena |
| frames | 300, two runs | project rule |
| direct decision | ≥10 % work with identical pixels | below that the promotion is not justified |
| binds | 1 per system | the stream is one texture run |

## 7. Measurements

All points 300 frames, two runs, unique PRX and marker names. Evidence: `/home/psp/evidence/sprint022/`
(`*_run{1,2}.txt|raw|log`, `SHA256SUMS`). Boot line: `RLGL: PSP_CULL=1 PSP_TRI_FAST=1`, pspgu `b24`.

**API vs direct:**

| particles | `DrawTexturePro` work | direct stream work | change | sim | draws | capture |
|---:|---|---:|---:|---:|---:|---|
| 500 | 3.064 / 3.063 ms | **2.218 / 2.217 ms** | **−27.6 %** | 0.140 ms | 1 | `8b5d8a4f520bbb31` both |
| 2,000 | 10.928 / 10.928 ms | **7.804 / 7.806 ms** | **−28.6 %** | 0.524 / 0.580 ms | 1 | `604906f9b40843fb` both |

Decision: **promote**. The win is above 10 % at both points, captures are byte-identical, and the recipe is
`docs/psp/recipes/particle-stream.md`.

**Cap / arena:**

| particles | vertices/frame | draws/frame | work | median | arena |
|---:|---:|---:|---:|---:|---|
| 2,000 | 4,000 | **1** | 10.928 ms | 16.683 ms | fits (96 KiB) |
| 2,049 | 4,098 | **2** | 11.238 / 11.237 ms | 16.684 ms | fits (98 KiB) |
| 4,000 | 8,000 | 2 | 22.104 / 22.103 ms | 33.367 ms | **303 full-sync fallbacks** (192 KiB > 128 KiB) |

The 4,000-particle point is the arena-overflow case, not a clean cap result. 2,049 is the clean first-second-draw
point.

**Tear / binds:** every point's two captures are byte-identical within the run and across the two runs; **1 bind
per run** at every count, not per particle; no mid-loop bind.

**Regression (final library):**

| scene | work | capture |
|---|---:|---|
| shapes | 13.220 ms | matches 021 |
| mixed | 10.658 ms | matches 021 |
| text 8 / 24, tiles | 3.359 / 9.129 / 3.125 ms | match 019/018 |
| `batch_check` gate | — | `b69830b8…`, byte-identical to pspGL |

**Instrument note.** The first gate run failed because another session had replaced the shared
`host0:/textures/bunnymark/wabbit_alpha.png` with an RGBA copy; the reference used the gray+alpha original. The
original was restored and the gate passed. Particle measurements use their own generated texture and were not
affected.

## 8. Exit criteria

- [x] the api/direct table is recorded at 500 and 2,000 with identical captures, against a same-session baseline
- [x] the decision is recorded with the ≥10 % rule (promote or null) — **promote**, −27.6 % / −28.6 %
- [x] 2,000 particles submit one draw; 2,049 submit two; 4,000 is recorded as the arena-overflow case
- [x] simulation and rendering are separate numbers at every point
- [x] two captures after 300 frames are identical; binds are one per system
- [x] 018–021 captures and the gate are byte-identical
- [x] no `tools/pspgu/` change and no cap change

## 9. Result

Sprint 022 closed 5/5. The particle stream was already one draw per texture run; the remaining cost was the
per-particle `DrawTexturePro` call, and the direct loop removes about 28 % of it:

- **Direct stream:** 500 particles **3.064 → 2.218 ms** (−27.6 %), 2,000 **10.928 → 7.804 ms** (−28.6 %), with
  byte-identical captures and a separate simulation number (0.140 / 0.52–0.58 ms). The win is above the 10 %
  promotion rule, so the loop is now a recipe (`../recipes/particle-stream.md`) and the corpus keeps both arms.
- **Cap:** 2,000 particles are 4,000 vertices and one draw; **2,049** are 4,098 vertices and **two draws** (the
  clean crossing). 4,000 are 8,000 vertices / 192 KiB and hit the 128 KiB per-list arena, producing **303
  full-sync fallbacks**; that point is recorded as arena-bound, not as a cap result.
- **Tear/binds:** every point's two captures are identical within and across runs; binds are **one per run**, not
  per particle.
- **Regression:** shapes and mixed captures match 021; text/tiles match 019/018; `batch_check` is byte-identical
  to pspGL (`b69830b8…`); every run has 0 evictions/reloads/failures.

No driver or cap change. The only caveat is the shared host0 texture replaced by another session during the
first gate attempt; it was restored, and the particles use their own generated texture.

## 10. Follow-ups

- **Sprint 023 (P7-S6) — cache strips, regression, phase close**.
- The direct loop is a recipe, not a new public API; rotated/multi-texture particle systems still use
  `DrawTexturePro` and the batcher's primitive rules.
