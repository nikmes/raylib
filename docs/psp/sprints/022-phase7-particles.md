# Sprint 022 (P7-S5) — Particles: one stream per texture run and the cap boundary

| | |
|---|---|
| Roadmap phase | [Phase 7 — 2D specialisation](../phases/07-2d-specialisation/index.md) |
| Sprint | P7-S5 |
| Branch / base commit | `master` @ the sprint 021 close (this doc is written before 021 runs) |
| Started / closed | — / — |
| Build number(s) | pspgu `b24` (unchanged); `scene_2d` builds |
| Tasks | 0/5 done |

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

**First action.** Add `PARTICLE_IMPL=api|direct` to `scene_2d`, where `direct` is one `rlSetTexture` +
`rlBegin(RL_QUADS)` loop with the same geometry, and run `COUNT=500/2000` before any `src/` change.

**Hard rules.**

- **Same geometry.** `direct` must produce the same quad as `DrawTexturePro` for an unrotated, uniformly scaled
  particle; captures are byte-identical for a fixed final state.
- **Simulation stays separate.** `sim` is measured around the update loop only, as in 018; do not fold it into
  `work`.
- **A stream is per texture run.** One texture for the whole system; a bind in the middle of the loop is a bug,
  not a tuning detail.
- **The cap is evidence, not a knob.** Do not raise `RLPSP_BATCH_CAP`; record 2 draws at 4,000 and the arena
  arithmetic if a change is proposed.
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
make clean && make MODE=particles COUNT=2000 PARTICLE_IMPL=api
make clean && make MODE=particles COUNT=2000 PARTICLE_IMPL=direct
make clean && make MODE=particles COUNT=4000 PARTICLE_IMPL=api
cp scene_2d.prx /tmp/s3/app.prx
cd /tmp/s3 && timeout 20 pspsh -e "reset"; sleep 6
( printf './app.prx\n'; sleep 16 ) | timeout 35 pspsh
pspsh -e "scrshot host0:/s22.raw"          # after DONE only
```

**Do not touch.** The text path, the 512-window splitter, the shape/sprites work, the allocator, `DrawMesh`.

**Done means** §8: the direct probe and its decision, 1 draw at 2,000, the 2,048/4,000 cap table, two identical
captures after a 300-frame run, and the regression hashes.

## 1. Objective

Four falsifiable results:

1. A **direct particle loop** (one bind, one begin/end per system) is compared with `DrawTexturePro` at 500 and
   2,000 particles; the decision rule is at least 10 % work reduction with identical pixels.
2. **One draw per stream** at 2,000 particles; **two draws** at 4,000, with the vertex counts and the cap
   boundary recorded.
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

- [ ] Sprint 018 baselines (500: 3.285/0.443 ms; 2,000: 11.844/1.775 ms; 1 draw) are in `sprint018/`
- [ ] pspgu `b24`, gate `b69830b8…`
- [ ] `scene_2d` particle mode is deterministic and freezes before capture
- [ ] Device loop available; pspGL restorable (`2ab7378e`)

## 5. Tasks

| ID | task | status | verification | evidence |
|---|---|---|---|---|
| T1 | **Corpus**: `PARTICLE_IMPL=api\|direct`; `direct` is one bind/begin/end with the same quad; counts 500/2,000/4,000 | `todo` | direct captures equal api at a fixed final state; sim/work separate | |
| T2 | **Direct-loop measurement**: 500/2,000 api vs direct, two runs | `todo` | work table; decision (≥10 % → promote, else null) | |
| T3 | **Cap boundary**: 2,000 / 2,048 / 4,000, draws and vertices | `todo` | 2,000 = 1 draw, 4,000 = 2; arena arithmetic recorded | |
| T4 | **Tear and bind audit**: 300 frames, two captures, binds per run | `todo` | captures identical; one bind per system; no mid-loop bind | |
| T5 | **Regression**: 018–021 captures, gate, VRAM/residency | `todo` | hashes unchanged; gate `b69830b8…`; 0 evictions | |

Status vocabulary: `todo` / `doing` / `done` / `blocked` / `dropped`. IDs are stable.

### T1 design

- `PARTICLE_IMPL=api` is today: `DrawTexturePro` per particle.
- `PARTICLE_IMPL=direct`: `rlSetTexture(particleTex.id)`, `rlBegin(RL_QUADS)`, one colour and four vertices per
  particle in `DrawTexturePro`'s TL/BL/BR/TR order, `rlEnd()`, `rlSetTexture(0)`. The app must compile against
  the header with the PSP batch section (`-I../../src` and `-DPLATFORM_PSP -DGRAPHICS_API_OPENGL_11 -DPSP_BATCH=1`,
  as sprint 019's direct arm does).
- The same update loop, seed and frozen final frame for both arms.

### T2 design

- Run 500 and 2,000, two runs each, 300 frames. Decision: promote only if `direct` work is at least 10 % below
  `api` **and** the capture is byte-identical. Promotion means adding the loop as a **recipe**
  (`docs/psp/recipes/`) and using it in the corpus; it does not mean a new public API unless a separate review
  asks for one.
- If the win is <10 %, record the null result with the table and keep `api`.

### T3 design

- 2,000 particles = 4,000 vertices, one draw. 2,048 = 4,096, exactly the cap. 4,000 = 8,000, two draws.
- If a larger single draw is proposed, the arithmetic is 24 B/vertex and the per-list arena is 128 KiB (5,461
  vertices); record it, do not change the cap in this sprint.

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
| counts | 500 / 2,000 / 4,000 | below, at, and above the sprite count of the 4,096 cap |
| frames | 300, two runs | project rule |
| direct decision | ≥10 % work with identical pixels | below that the promotion is not justified |
| binds | 1 per system | the stream is one texture run |

## 7. Measurements

Filled by T1–T5.

**API vs direct (work/sim ms):** 500 and 2,000, run 1/run 2.

**Cap:** count / vertices / draws / work / median.

**Tear/binds:** capture SHA-256 pair, binds per run, array vertices.

**Regression:** 018–021 hashes, gate, residency.

## 8. Exit criteria

- [ ] the api/direct table is recorded at 500 and 2,000 with identical captures
- [ ] the decision is recorded with the ≥10 % rule (promote or null)
- [ ] 2,000 particles submit one draw; 4,000 submit two; the cap arithmetic is written
- [ ] simulation and rendering are separate numbers at every point
- [ ] two captures after 300 frames are identical; binds are one per system
- [ ] 018–021 captures and the gate are byte-identical
- [ ] no `tools/pspgu/` change and no cap change

## 9. Result

Not run yet.

## 10. Follow-ups

- **Sprint 023 — cache strips, regression, phase close**.
- If direct is promoted, the recipe records the loop, the texture state and the frozen-frame capture method; no
  public API is added in this phase.
