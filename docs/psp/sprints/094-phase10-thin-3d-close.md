# Sprint 094 (P10-S24) — Close: matched ceiling, cleanup and the phase record

| | |
|---|---|
| Roadmap phase | [Phase 10 — 3D and advanced](../phases/10-3d-advanced/index.md) |
| Sprint | P10-S24 |
| Branch / base commit | `master` @ `fe1171fe` — design: [Redesigning the 3D path](../phases/10-3d-advanced/pspgu-redesign.md) |
| Started / closed | 2026-11-24 (written) / — |
| Build number(s) | final `b38` if the redesign is adopted; otherwise the driver is unchanged |
| Tasks | 0/4 |

## 0. Handover

S20-S23 can leave the renderer on a new 3D architecture, on the old one, or on a mechanical split. This sprint makes
the result legible and removes what the programme retired.

Open questions it inherits:

- The **60 fps ceiling** was 270 cubes on a layout that is not M3D's 20×60; the comparison needs one layout
  (M3D-lessons E6).
- The **arena and replays**: a thin per-object path has no per-frame expansion, so the ~680-cube wall may no longer
  exist for the adopted path — but the merged path's wall must still be stated if merging survives.
- **087 B (index-run cache)** was postponed until S22's numbers existed; this is where it is re-estimated or closed.
- The **driver default** and its hash must be explicit, because every number in the phase is tied to them.

## 1. Objective

Close the redesign with one ceiling number, one gate table, one default, and a phase record that says what was
adopted, what was dropped, and what is still worth doing.

## 2. Why now

The phase has accumulated sprints 078-094. Without a close, the numbers scatter across nine documents and the next
session re-derives them — the failure this phase has paid for repeatedly.

## 3. Scope

**In:** the matched ceiling sweep; the arena/replay ledger; retirement or gating of the losing path; the full gate
table; the benchmark pair; 087 B's disposition; docs.

**Out:** new optimisations. This sprint measures and records; anything new becomes its own sprint.

## 4. Preconditions

- [x] S23 has chosen a path (or a mechanical rule)
- [x] the pixel gates are known and their expected values recorded
- [ ] the matched layout exists on both sides (S20 T1)

## 5. Tasks

| # | Task | Status | Acceptance | Result |
|---|---|---|---|---|
| T1 | **Matched ceiling.** Last cube count at 60 fps on one layout, ours and M3D's 760, two runs per point | `todo` | one number per engine on the same layout | — |
| T2 | **Arena / replay ledger.** Replays and `arenaOverflows` on the adopted path at 1,200; the losing path's wall if it survives | `todo` | numbers; dead paths removed or compiled out | — |
| T3 | **Gates + pair.** Full table and the benchmark pair on the default build | `todo` | 613 / 284 / 0 / 0 / clip / MESH_SCENE / RTT; pair recorded | — |
| T4 | **Docs and default.** Phase index, redesign page, 087 disposition, driver hash, install state | `todo` | one paragraph per adopted/dropped item; strict docs pass | — |

## 6. Design notes

**One layout or no number.** The 270-cube ceiling and M3D's 760 were measured on different grids, which M3D-lessons
E6 already flags. A ceiling quoted on the wrong layout is worse than no ceiling.

**Retire, do not comment out.** If merging loses, its code path should be removed with the numbers kept in the phase
record; a compiled-out branch still taxes every future reader. If it wins, the thin path's pieces that survive are
named explicitly.

**087 B is re-estimated here, not assumed.** Its 0.45 ms/frame applies to a path that copies indices per draw; after
S22 the static path may copy none. The estimate must come from the adopted path's counters.

## 7. Gates

| gate | expected |
|---|---|
| `batch_check` / `basic_shapes` | 613 / 284 |
| `scene_3d SCENE=both`, `MESH_SCENE`, RTT | 0 diff |
| `clip_probe` | 237/720 |
| frame behaviour | no full GE wait; fallback counters 0 |
| benchmark pair | two runs per point; hashes recorded |

## 8. Exit criteria

- [ ] one ceiling number per engine, same layout, two runs
- [ ] the adopted architecture is stated, with the dropped one recorded and its code removed or compiled out
- [ ] the gate table is green on the default build
- [ ] 087 is closed or re-scoped with a number
- [ ] driver hash and install state explicit; strict docs build passes

## 9. Result

Pending — no device runs yet.

## 10. Follow-ups

- Whatever 087 B becomes, if anything.
- An s16 bake arm, only if the f32 path was adopted and memory is tight.
- The desktop/emulator surface, if the PSP-guarded epoch ever needs sharing.

## 11. References

- [Redesign](../phases/10-3d-advanced/pspgu-redesign.md) — §4, §5
- [M3D compared](../phases/10-3d-advanced/m3d-lessons.md) — E6, R7
- sprints 078, 085 (§9.9 close), 088 (§9.5), 089 (§9.4), 090-093
- `docs/psp/recipes/backend-ab-pspgu-vs-pspgl.md` — the harness this close uses
