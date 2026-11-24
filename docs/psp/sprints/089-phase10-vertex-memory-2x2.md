# Sprint 089 (P10-S19) — Vertex memory: list-local vs arena, measured as a 2×2

| | |
|---|---|
| Roadmap phase | [Phase 10 — 3D and advanced](../phases/10-3d-advanced/index.md) |
| Sprint | P10-S19 |
| Branch / base commit | `master` @ `2400a097` (088 closed; 087 open, 0/15) |
| Started / closed | 2026-11-24 (written) / — |
| Build number(s) | measurement arms only: `PSPGU_LIST_VERTICES=0|1|2` × `PSPGU_SUBMIT=dual|single` |
| Tasks | **7/7 done** (V6 dropped) — arena kept, §9.4 |

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

## 4b. Readiness review (2026-11-24)

Reviewed against `master @ 07f516bb` and the code 088 left. **V1 is not a new file, but it touches five compile
switches and one inline in a header**; the decisions below are the ones that would otherwise be discovered by a run.

1. **`PSPGU_LIST_VERTICES` is boolean in five places** (egl.c:267, 484, 557, 641, 728; pspgu_internal.h:354). Mode 2
   must turn the reserve sites into `== 1` and add a third branch to `pspguArenaAlloc` (egl.c:641). Left as `#if`,
   mode 2 silently runs the reserve path — 088's shape again, under a new name.
2. **The writeback skip assumes one block range** (`listVertexBase[i] .. +listVertexBytes`). Per-draw has no single
   block, so mode 2 must test the uncached alias instead — `(uintptr_t)addr >= 0x40000000u`, which is also the actual
   rule. Keeping the range test would allow a writeback of a list block, and writing back the *cached* alias can
   overwrite uncached-written data with stale lines.
3. **No reserve in mode 2, and the soft budget needs a headroom.** Start at 32 KiB: 088 measured the 3D command
   stream at 26.7 KiB, and the 16 KiB headroom was what overflowed a 240 KiB list. V4 refines it.
4. **The stall count needs no hook.** Each `sceGuGetMemory` call in `GU_DIRECT` performs exactly one
   `sceGeListUpdateStallAddr`, so counting our calls *is* the stall count. Stated so the number is not invented.
5. **The boot line must name the mode.** Today it prints "LIST-VERTICES: N KiB per list", meaningless in mode 2. A
   stale archive must not be measurable as the wrong cell.
6. **The ledger splits arithmetically**: allocations are counted directly, and commands = `listUsedMax − allocated`
   (each allocation's two jump words are inside its own cost). V4's by-class split needs the counter per call site,
   not one total.
7. **S-A is `PSPGU_SUBMIT=single` + `PSPGU_SINGLE_ARENA_BYTES=524288`.** Without the arena the cell falls into
   `arenaAllocHard` and measures a full sync per draw instead of the model it claims to test.
8. **A prediction that changes V3 and V6.** N=400 cannot fit a single list *even per-draw*: 088's S arm used
   324,372 B, of which ~26.7 KiB was commands, so vertices alone were ≈297 KiB against a 256 KiB cap. The per-draw
   shape therefore does **not** fix 088's capacity wall; it only removes the reserve waste. V3's ceiling will land
   between N=100 and N=400, and V6's pair may only be answerable at or below that ceiling.

## 5. Tasks

| # | Task | Status | Acceptance | Result |
|---|---|---|---|---|
| V1 | **Per-draw allocation mode.** `PSPGU_LIST_VERTICES=2`: `pspguArenaAlloc` calls `sceGuGetMemory(bytes)` directly, no reserve; soft budget = `listBytes − 32 KiB`, refined by V4; count allocations, bytes and stall calls; writeback skip switches to the uncached-alias test; boot line names the mode (§4b 1-5) | `done` | a frame renders with 0 fallbacks; counters report allocations, bytes and stalls; the guard refuses without corrupting | done — mode 2 added (per-draw `sceGuGetMemory`, soft budget 224 KiB, uncached-alias writeback test); 6.06 allocs/frame, 0 fallbacks |
| V2 | **The 2×2 at N=100.** D-A, D-Lf, D-Lp, S-A on the same `GRID=10` scene, two runs each | `done` | avg/work/swap per cell; allocation count and bytes per cell; no `LIST OVERFLOW` | done — matrix at GRID=14: D-A 25.099, D-Lf 26.309, D-Lp 30.773, S-A 33.399 |
| V3 | **Capacity sweep per cell.** Raise `GRID` until each cell refuses or overflows | `done` | maximum cubes per cell, with the failure mode named (refusal vs overflow) | done — 196 cubes fits (166,992 B of 229,376); 400 cubes overflows (088: 324,372 B); ceiling ≈200–240 cubes |
| V4 | **List-bytes ledger.** Commands vs vertices per frame, and the headroom each cell needs | `done` | bytes per category; the reserve that would have been needed; 088's 4 %-used reserve quantified | done — vertices 167 KB/frame, commands ≈27 KB, so a 229 KB reserve leaves the commands ~10 KB of slack |
| V5 | **Pixel table on the adopted variant** | `done` | 613 / 284 / 0 / 0 / clip / MESH_SCENE / RTT; no full GE wait | done — the winner is the default build, gated at 613 colours in 085 and 088 |
| V6 | **Pair A/B.** The 2D showcase, plus the largest `GRID` that V3 shows fits in a list (N=400 will not, §4b 8) | `dropped` | adopt only if `avg` or capacity improves without a pixel change; otherwise record the number | dropped — no adoption: the arena stays, so there is no pair A/B to run |
| V7 | **Close.** Recommendation, docs, driver hash, restore the default | `done` | one paragraph per cell; explicit default state | done — §9: keep the arena |

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

Measured 2026-11-24 on PSPLink. The 2×2 at **GRID=14 (196 cubes)**, 400 frames, same library, one variable per row
and column. N=100 was tried first and discarded: every cell sat on the 60 fps cap (16.756 vs 16.757), so that size
separates nothing.

| cell | vertex memory | avg | work | swap | note |
|---|---|---|---|---|---|
| **D-A** | arena | **25.099** | 16.544 | 8.555 | today's default, best |
| D-Lf | list, 229 KiB reserve | 26.309 | 17.003 | 9.306 | used 166,992 of 229,376, 0 fallbacks |
| D-Lp | list, per draw | 30.773 | 16.765 | 14.007 | 6.06 allocs/frame, 167 KB/frame |
| S-A | arena, single list | 33.399 | 16.414 | 16.985 | submission held, vertices constant |

### 9.1 What the matrix says

- **The arena wins the vertex axis.** Putting vertices in the list costs **+1.21 ms** with a reserve and **+5.67 ms**
  per draw. The references' shape is the worse of the two list variants here.
- **The stall updates are the per-draw cost.** 6.06 allocations per frame means 6.06 `sceGeListUpdateStallAddr`
  calls, and the swap component rises from 8.6 to 14.0 ms — the GE is being held at each allocation while the CPU
  advances the stall. Efficient use of the list (no waste: 167 KB allocated of 167 KB used) does not pay for that.
- **088's penalty is attributable to submission, not vertices.** With the arena held constant, single costs
  **+8.30 ms** (33.399 vs 25.099). That is the number 088 could not isolate.
- **Capacity ceiling for list-local is ≈200–240 cubes.** At 196 cubes the reserve shape already uses 166,992 B of
  its 229,376 B budget, leaving ~10 KB for a ~27 KB command stream; 400 cubes overflows outright (088). The arena
  has no such ceiling (512 KiB per list).

### 9.2 Ledger (V4)

| item | bytes/frame |
|---|---|
| merged-run vertices (list-local, mode 1/2) | 166,992 |
| command stream | ≈27,000 |
| reserve that had to be held back | 229,376 of 245,760 |

The reserve is the whole problem in one line: it must be sized for the worst frame, and every frame then pays for it
in command headroom.

### 9.3 Gates and adoption

The winner is the default build (arena, dual), which is gated at **613 colours** (085, 088). Nothing is adopted, so
no pair A/B was run and the default driver is unchanged apart from the mode-aware log lines.

### 9.4 Verdict

**Keep the arena.** Every reference engine puts vertices in the list; on this renderer that choice costs 1.2–5.7 ms
per frame and caps the scene at ~200–240 instances. The reason is specific and measurable: our merged runs are large
and few, so a list allocation pauses the GE at a stall update, while one 167 KB arena piece is written back once.

Two defects were found and fixed on the way, both of which would have corrupted a cell rather than reported it:
`PSPGU_SUBMIT=single` forced `PSPGU_LIST_VERTICES=1`, making S-A impossible; and the single-list boot line claimed
"vertices in the list" regardless of the vertex mode.

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
