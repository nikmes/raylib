# Sprint 075 (P13-S19) — Corpus ledger and status sweep

| | |
|---|---|
| Roadmap phase | [Phase 13 — Samples porting](../phases/13-samples-porting/index.md) |
| Sprint | P13-S19 |
| Branch / base commit | `master` @ `816a8c25` (2026-11-22) |
| Started / closed | 2026-11-22 / — (continuous) |
| Build number(s) | — (documentation) |
| Tasks | 4/5 — T1–T4 done; T5 standing |

## 0. Handover

**Situation.** Phase 13 now spans 227 samples in eight raylib categories. Before this sprint the state was spread
across tier prose, sprint pages and the phase header. This sprint owns one page, the
[sample ledger](../phases/13-samples-porting/ledger.md), where every sample appears once with its category,
`ported`/`pending` status, and a `possible`/`blocked`/`drop` portability verdict. It is continuous: it does not
wait for the wave sprints and it never closes while Phase 13 is open.

**Read before acting**, in this order:

1. [Sample ledger](../phases/13-samples-porting/ledger.md) — the table this sprint maintains
2. [Phase 13](../phases/13-samples-porting/index.md) — the tier definitions and gates the verdicts use
3. [Phase 13 porting recipe](../phases/13-samples-porting/index.md#porting-recipe) — step 8 is the ledger update

**First action.** Open the ledger, pick a sample whose state changed, and update its row and the summary counts.
Then check that the category total still matches the upstream file count.

**Hard rules.**

- **One row per sample.** No sample may appear twice or disappear; a dropped sample stays with `drop` and a reason.
- **The category totals must sum to the upstream counts** (core 49, shapes 45, textures 33, text 17, models 32,
  shaders 36, audio 11, others 4 = 227).
- **`possible` means no missing backend capability.** Input remapping (`G1`) is allowed and named in the note;
  a missing render target, shader path, 3D mesh path, clipboard, readback or oversized asset is `blocked`.
- **The ledger changes in the same commit as the sprint that changes a sample's state.** A sprint that ports or
  drops a sample and leaves the ledger stale is not done.

**Do not touch.** Sample code and the library; this sprint is bookkeeping only.

**Done means** §8, for the life of Phase 13.

## 1. Objective

One page lists all 227 upstream samples grouped by raylib category — core, shapes, textures, text, models,
shaders, audio, others — with `ported`/`pending` status and a `possible`/`blocked`/`drop` verdict plus the gate
or reason. The counts sum to 227, and every porting sprint updates the affected rows.

## 2. Why now

The phase crossed four completed sprints (42 samples ported) and added five future waves. Without one ledger the
same question — "what is left, and is it blocked?" — has to be answered by reading eight sections and a sprint
list. This makes the answer one page and one grep.

## 3. Scope

**In:** the ledger page, its nav entry, the Phase 13 link and update rule, and the counts.

**Out:** any sample port, any library change, and any reclassification that is not backed by the gates already
recorded in Phase 13.

## 4. Preconditions

- [x] Phase 13's tier tables and gate list are current
- [x] The upstream category counts are known (227)
- [x] The ledger page exists and is linked from Phase 13

## 5. Tasks

Status vocabulary — `todo` · `doing` · `done` · `blocked` · `dropped`.

| ID | task | status | verification | evidence |
|---|---|---|---|---|
| T1 | Create the ledger grouped by raylib category, one row per sample, with status and portability | `done` | 227 sample rows, eight category sections | `docs/psp/phases/13-samples-porting/ledger.md` |
| T2 | Verify coverage and totals: every `.c` sample appears once; counts sum to 227 | `done` | 42 ported / 64 possible / 108 blocked / 13 drop = 227 | ledger summary |
| T3 | Wire the ledger into Phase 13 and the nav | `done` | Phase 13 links it; nav has the page and this sprint | `mkdocs.yml`, Phase 13 page |
| T4 | Add the update rule to the Phase 13 porting recipe and sprint exit criteria | `done` | recipe step 8 names the ledger; this sprint's rule is explicit | Phase 13 page |
| T5 | Standing: every porting sprint updates its rows and the summary in the same commit | `doing` | the next porting sprint's diff contains a ledger change | — |

### Task log (append-only)

- 2026-11-22 — sprint opened; ledger generated from the filesystem and Phase 13's classification
- 2026-11-22 — T1–T4 `done`; counts 42/64/108/13 sum to 227
- 2026-11-22 — T5 `doing` (continuous)

## 6. Budgets

Not applicable — documentation only.

## 7. Measurements

| run | build | workload | metric | value | notes |
|---|---|---|---|---|---|
| ledger | — | all categories | sample rows | 227 | one per `.c` sample, template excluded |
| ledger | — | all categories | ported | 42 | 33 core/shapes/textures + 9 audio |
| ledger | — | all categories | possible | 64 | 59 + 5 pointer/G1 |
| ledger | — | all categories | blocked | 108 | named gate per row |
| ledger | — | all categories | drop | 13 | 4 touch-only, 8 desktop-only, 1 interop |

## 8. Exit criteria

- [x] the ledger lists all 227 samples by raylib category, one row each
- [x] the summary counts sum to 227 and match the category sections
- [x] Phase 13 links the ledger and the porting recipe requires an update
- [x] `mkdocs build --strict` passes
- [ ] standing: the next porting sprint updates the ledger in the same commit (T5, continuous)

## 9. Result

Generated on 2026-11-22 from the eight `examples/` directories and the Phase 13 tier/gate classification:

- **42 ported** — 5 core, 8 shapes, 20 textures, 9 audio.
- **64 possible** — 19 core, 33 shapes, 1 texture (`sprite_button`), 10 text, 1 other.
- **108 blocked** — 13 core, 4 shapes, 12 textures, 7 text, 32 models, 36 shaders, 2 audio, 2 others.
- **13 drop** — 12 core (touch/desktop), 1 other (`raylib_opengl_interop`).

The five `possible` rows that name `G1` are pointer-driven; the rest need only pad triggers or no input.

## 10. Follow-ups

- The next porting sprint (P13-S3 or the text batch) must update its rows and the summary.
- When a gate closes, move its rows from `blocked` to `possible` in the ledger before writing the sprint.
- When the upstream tree grows, add rows and re-check the 227 total.
