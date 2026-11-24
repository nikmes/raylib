# Sprint 089 (P10-S19) — Vertex memory: list-local vs arena, measured as a 2×2

| | |
|---|---|
| Roadmap phase | [Phase 10 — 3D and advanced](../phases/10-3d-advanced/index.md) |
| Sprint | P10-S19 |
| Branch / base commit | `master` @ `2400a097` (088 closed; 087 open, 0/15) |
| Started / closed | 2026-11-24 (written) / — |
| Build number(s) | measurement arms only: `PSPGU_LIST_VERTICES=0|1|2` × `PSPGU_SUBMIT=dual|single` |
| Tasks | 0/7 — no default change until V6 |

## 0. Handover

**What 088 settled and what it left open.** 088 measured the submission model and rejected the single list:

| scene | single (1 list, full wait) | dual (2 lists, queued) |
|---|---|---|
| 2D showcase | 50.539 ms | 33.800 ms |
| 3D N=100 | 33.398 ms | 16.759 ms |
| 3D N=400 | overflow 324,372 of 245,760 | 33.548 ms |

But its single arm also moved the vertices into the list, so **two variables changed together**. The 2×2 was never
run:

|  | vertices in the arena | vertices in the list |
|---|---|---|
| **dual lists, queued** | today's default (measured) | **never measured** |
| **single list, full wait** | **never measured** | 088's S arm (measured) |

**What the references do.** All three put vertices in the display list; none uses a separate arena:

- SDK samples: 5 of 13 use `sceGuGetMemory`, the rest hand static `.bss` arrays written back once.
- openTRI: `sceGuGetMemory` at every primitive (one allocation per line, circle, quad), 256 KiB list default.
- PGE: `sceGuGetMemory` at every primitive, two 256 KiB `GU_DIRECT` lists, **full wait every frame**.

And one thing 088 got wrong about the shape: it reserved **228 KiB once** and used 4 % of it. The references
allocate **per draw, exactly what the draw needs**. That is the variant that has to be measured, not the fixed
reserve.

**A note in openTRI's own source** (`triGraphics.c:400-408`) states the rule this sprint exists to test: a vertex
buffer rewritten while the GE reads it gives unpredictable results, and *"this can be avoided for vertices if they
are stored in the list (`sceGuGetMemory`)"*. They chose the list; we chose the arena. 088 measured the list only in
its worst configuration.

## 1. Objective

Settle the vertex-memory axis with a 2×2 matrix and a per-draw allocation variant, then adopt the winner on the pair
and the pixel table — or record why the arena stays.

Four cells, one scene, one library:

1. **D-A** dual + arena — today's default.
2. **D-Lf** dual + list, fixed reserve — 088's shape, kept as the control for "list done badly".
3. **D-Lp** dual + list, **per-draw allocation** — the references' shape.
4. **S-A** single + arena — isolates submission with vertex storage held constant.

## 2. Why now

- It is the last unmeasured axis, and every reference differs from us on exactly it.
- 088's list arm was distorted by a 228 KiB reserve that the frame barely touched, so "list-local loses" is not yet
  a fair statement.
- The result decides 087 workstream A, which is blocked on the same question.

## 3. Scope

**In:** a per-draw allocation mode; the 2×2 at N=100; a capacity sweep per cell; a list-bytes ledger (commands vs
vertices, headroom needed); the pixel table; the pair for the winner.

**Out:** changing the submission model again (088 closed it); the index cache (087 B); offline baking (087 C);
CALL lists.

## 4. Preconditions

- [x] `PSPGU_LIST_VERTICES` (reserve + guard) and `PSPGU_SUBMIT=single` exist, both defaulting off
- [x] `PSPGU_SINGLE_ARENA_BYTES` (default 0) exists for the S-A cell
- [x] `GRID` is a Makefile passthrough on `psp_showcase`
- [x] control numbers: N=100 dual 16.759, single 33.398; N=400 dual 33.548; 2D 33.800
- [ ] a per-draw allocation mode exists (`PSPGU_LIST_VERTICES=2`) before any D-Lp run

## 4a. Readiness review (2026-11-24)

Reviewed against the code 088 left. **V1 is the only new machinery**; everything else is flags and runs.

- `pspguArenaAlloc` today has two paths: the fixed block (4-byte bump inside one reserve) and the arena. The
  per-draw mode is a third: **no reserve**, `sceGuGetMemory(bytes)` per allocation, with the block remembered only
  for the writeback skip.
- The guard must still refuse when the list is exhausted. In per-draw mode the failure mode is the reserve call
  itself, so the size must be checked against the bytes remaining — which the driver cannot query. The practical
  guard is a soft budget (`listBytes − commandHeadroom − used`) tracked by the driver, and a fallback to the arena.
- The stall count is the cost of the references' shape: one `sceGeListUpdateStallAddr` per allocation in
  `GU_DIRECT`. It must be counted, not assumed; a 24-sprite HUD and a 400-cube merge differ by two orders of
  magnitude.
- `sceGuGetMemory` rounds to 4 bytes and has no bounds check (085 §6), so an over-allocation is a corrupted stream,
  not an error return.

## 5. Tasks

| # | Task | Status | Acceptance | Result |
|---|---|---|---|---|
| V1 | **Per-draw allocation mode.** `PSPGU_LIST_VERTICES=2`: `pspguArenaAlloc` calls `sceGuGetMemory(bytes)` directly, no reserve; track a soft budget and the allocations/bytes used; count stall updates. Keep the arena as fallback | `todo` | a frame renders with 0 fallbacks; the counter reports allocations and bytes; the guard refuses without corrupting | — |
| V2 | **The 2×2 at N=100.** D-A, D-Lf, D-Lp, S-A on the same `GRID=10` scene, two runs each | `todo` | avg/work/swap per cell; allocation count and bytes per cell; no `LIST OVERFLOW` | — |
| V3 | **Capacity sweep per cell.** Raise `GRID` until each cell refuses or overflows | `todo` | maximum cubes per cell, with the failure mode named (refusal vs overflow) | — |
| V4 | **List-bytes ledger.** Commands vs vertices per frame, and the headroom each cell needs | `todo` | bytes per category; the reserve that would have been needed; 088's 4 %-used reserve quantified | — |
| V5 | **Pixel table on the adopted variant** | `todo` | 613 / 284 / 0 / 0 / clip / MESH_SCENE / RTT; no full GE wait | — |
| V6 | **Pair A/B.** N=400 and the 2D showcase, winner vs today's default | `todo` | adopt only if `avg` or capacity improves without a pixel change; otherwise record the number | — |
| V7 | **Close.** Recommendation, docs, driver hash, restore the default | `todo` | one paragraph per cell; explicit default state | — |

## 6. Design notes

**Why the 2×2 and not another single arm.** 088's mistake was moving submission and vertex storage together. Each
row and column here differs by exactly one variable, so a win or a loss is attributable. D-A is the control for both
axes.

**Per-draw is the references' shape, and it is not the same experiment as the reserve.** With a reserve, the frame
pays for the worst case in command headroom on every frame (088: 228 KiB reserved, 9,552 B used, and the 26.7 KiB
command stream then overran a 240 KiB list). Per-draw pays one stall syscall per allocation and risks fragmentation,
but never wastes the list. The two must be reported separately; averaging them would hide both effects.

**Where vertices should go, given what we know.** Merged runs are large and few (2 draws, ~200 KiB at N=400); the
tiny HUD is many and small. Per-draw allocation suits the second and fragments the first. A plausible outcome is a
**hybrid**: the merge path into the arena, small dynamic draws into the list. The sprint should be able to report
that, so V4 must break the ledger down by draw class, not just totals.

**Capacity is the second half of the answer.** The list is capped at `0x3FFFF`; the arena is 512 KiB per list. Any
list-local scheme trades RAM for a hard ceiling, and the ceiling is what stopped 088's 400-cube frame. V3 states the
ceiling in instances for each cell, which is the number a renderer decision actually needs.

**Fairness.** Same library, same driver flags apart from the cell, two runs per point, reset between. The boot line
already names the submission arm; V1 must add the vertex mode to it so a stale archive cannot be measured as the
wrong cell.

## 7. Gates

| gate | expected |
|---|---|
| `batch_check` 2D | 613 colours |
| `basic_shapes` | 284 colours |
| `scene_3d SCENE=both` | 0 diff |
| MESH_SCENE / RTT / clip | as recorded |
| frame behaviour | no full GE wait outside the S cells; fallback counters 0 outside a forced-overflow test |
| capacity sweeps | refusal reported, never a corrupted frame |

## 8. Exit criteria

- [ ] all four cells measured at N=100 with the allocation/stall counters
- [ ] the per-draw variant is measured separately from the fixed reserve
- [ ] the capacity ceiling is stated per cell, in instances
- [ ] the list-bytes ledger splits commands, merged vertices and small dynamic draws
- [ ] the adopted variant passes the pixel table and improves the pair or capacity, or the arena stays with a number
- [ ] strict docs build passes; driver state at close is explicit

## 9. Result

Pending — no device runs yet.

## 10. Follow-ups

- If per-draw wins, the stall syscall count becomes a budget: one per allocation is fine for a HUD and wrong for a
  merge. That split may deserve its own constant rather than one global mode.
- If the hybrid is the answer, 087 A's reserve disappears entirely and the list becomes a bump arena with a soft
  budget — which is a smaller change than the reserve scheme 088 tested.
- If the arena stays, the reason is the 256 KiB cap, and the asset-side answer (087 C) is the only way to make
  static geometry fit it.

## 11. References

- sprint 088 §9.1-§9.6 (submission model, capacity, the 4 %-used reserve, the 26.7 KiB command stream)
- sprint 085 §9.6 (first list-local measurement), §6 (`sceGuGetMemory` mechanics)
- sprint 087 §5 A2/A3 (shared guard), §10 (writeback audit)
- `/home/psp/ref/openTRI/triGraphics.c:400-408` (the lifetime rule and the list-local rationale), `triGraphics.c:503-950`
  (per-primitive `sceGuGetMemory`), `:52` (256 KiB default list)
- `/home/psp/ref/PGE/pgeGfx.c:64,394-406` (two 256 KiB `GU_DIRECT` lists, full wait), `:503-950`
  (`sceGuGetMemory` per primitive)
- public pspsdk `samples/gu/` — 9 of 13 with a 1 MiB list, 5 with `sceGuGetMemory`
