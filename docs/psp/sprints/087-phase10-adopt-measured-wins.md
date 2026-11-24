# Sprint 087 (P10-S17) — Adopting the measured wins: list-local vertices, index cache, baked geometry

| | |
|---|---|
| Roadmap phase | [Phase 10 — 3D and advanced](../phases/10-3d-advanced/index.md) |
| Sprint | P10-S17 |
| Branch / base commit | `master` @ `07c19f31` (085 closed; 086 folded in here; pspgu `9798af84e81b269e`, `libraylib.a c2004b266a6c3291`) |
| Started / closed | 2026-11-24 (written) / — |
| Build number(s) | pspgu `b38` (A), `b39` (A+B), `b40` (A+B+C); one pair per build so each workstream is attributable |
| Tasks | 0/15 — three workstreams, each adopted on its own gate, plus a shared pixel table and close |

## 0. Handover

**Where the phase stands.** 085 measured nine levers and adopted none: the 3D frame is CPU-bound (**30.1 ms build
against 13.1 ms GE**), the 2D frame is GE-bound (**19.5 against 29.2**), and every GE-side idea measured at
microseconds or failed parity. Installed state is the gated control (`9798af84e81b269e`, `c2004b266a6c3291`); the
closing pair was 3D **33.548 / work 32.071**, 2D **33.803 / work 21.590**.

**What is left, and why one sprint.** Three measured candidates survived 085, all unimplemented:

| # | candidate | measured | side that binds | expected effect on the pair |
|---|---|---|---|---|
| A | list-local vertices (`sceGuGetMemory`) | −1.49 ms CPU on 2D | CPU in 3D | **may move `avg`** — the only one that can |
| B | index-run cache | 31 µs per 1,000 indices; 14,400/frame ≈ 0.45 ms | CPU in 3D | may move `avg` by ≈1 % |
| C | baked static geometry (strips, optional s8) | strips −1.073 ms GE at 0 px; s8 −0.171 ms at 1.8 px | GE | **expected not to move the pair**; lowers GE for asset-heavy scenes |

They belong together because they share a budget (list and vertex memory), share the gate table, and interact: C
changes vertex counts, which changes A's list need and B's index run sizes. Doing them as one sprint with one pair
per build is the only way to attribute the result.

**Sprint 086 is folded into this one** as workstream A. Its §5 task list and §6 design notes stand as the reference
for the reserve geometry; its diagnosis task is A1 here.

## 1. Objective

Adopt all three into the default path, each on its own measured gate, and prove the combination with the pixel table
and the benchmark pair. A workstream that fails its gate is dropped with its number rather than carried.

Adoption is the aim, not a foregone conclusion: A has an unresolved capacity question, B has an unproven cost on the
real demo, and C does not touch the side of the frame that binds.

## 2. Why now

085 ended with a map and no code. The phase's own verdict says the next value is in the CPU build and in asset
preparation, not in more GE probing. All three candidates have a number attached; none has been tried in the default
path.

## 3. Scope

**In:** the reserve diagnosis, sizing, guard and fallback (A); an index cache with a key, an invalidation rule and a
miss path (B); a load-time bake for static meshes with strips and optional s8 (C); the pair after each workstream;
the full pixel table; the RAM ledger.

**Out:** re-opening any 085 drop (s8 as a runtime path, CLUT as a speed lever, sprite slicing, CALL lists, fast
clear); the merge path's own design; text/2D batching.

## 4. Preconditions

- [x] 085 closed; driver and library hashes verified
- [x] the list-local flag, allocator and writeback no-op exist and compile out (`PSPGU_LIST_VERTICES=0`)
- [x] PSPLink available for the session
- [ ] the §0 control pair re-read before A's A/B
- [ ] A1's diagnosis exists before any reserve budget changes

## 4a. Readiness and dependencies

```
A1 diagnose ──> A2 reserve/guard ──> A3 forced fallback ──> A4 pair (b38)
                                                            │
B1 instrument ──> B2 cache ──> B3 miss path ──> B4 pair (b39) ┘  (B is independent of A, but the pair is measured on A+B)
                                                            │
C1 bake helper ──> C2 gate scene ──> C3 integrate ──> C4 pair (b40) ┘
                                                            │
                                              X1 pixel table ──> X2 close
```

A and B both allocate vertex/index memory, so B2 must respect A's reserve: if A is adopted, indices come from the
same block when they fit, and the cache pool is separate and persistent. C runs last because it changes the vertex
counts that A and B were tuned against.

## 5. Tasks

### A — list-local vertices (build `b38`)

| # | Task | Status | Acceptance | Result |
|---|---|---|---|---|
| A1 | **Diagnose the 161,768 B ceiling.** Instrument only: per fill, log the reserve size and address, the count and sizes of sub-allocations, the size of the one that fails, and the number of `pspguBeginFrame` calls per buffer per frame. Separate chunky-allocation from reopen | `todo` | the cause is stated with evidence; no behaviour change | — |
| A2 | **Reserve sized from the list.** `PSP_GU_LIST_BYTES` to 252 KiB, reserve = `listBytes − commandHeadroom`, new reserve on a fresh `sceGuStart` after a reopen | `todo` | 3D: 0 fallbacks, `list_used_max` + reserve under the list, no `LIST OVERFLOW`, `arenaOverflows=0` | — |
| A3 | **Guard and forced fallback.** Refuse a sub-allocation that would pass the block, count it, fall back to the arena; a synthetic frame that exceeds the block must render correctly | `todo` | synthetic frame: correct pixels, fallback counter > 0; normal frames: 0 | — |
| A4 | **Pair A/B.** Two runs per point against the §0 control | `todo` | 3D `work` **and** `avg` improve by >0.05 ms; 2D `work` improves, `avg` unchanged; drop A with the required bytes if it cannot fit | — |

### B — index-run cache (build `b39`)

| # | Task | Status | Acceptance | Result |
|---|---|---|---|---|
| B1 | **Instrument the real cost.** Count per frame: index bytes copied, runs, distinct run keys, copies avoided by a cache. The 085 estimate (31 µs/1,000) came from a probe; this is the demo | `todo` | the measured per-frame cost is stated; B is dropped now if it is <0.05 ms | — |
| B2 | **Cache.** A persistent main-RAM pool (candidate 64 KiB, not the rotating arena) keyed by `(index pointer, count, first)`, written back once on creation; a hit hands the block straight to `sceGuDrawArray` | `todo` | hits > 0 and copies/frame fall; key changes ⇒ miss, never stale bytes; `arenaOverflows=0` | — |
| B3 | **Miss path and lifetime.** A run whose key changes every frame is copied every frame, uncached; the pool is overwritten LRU and never while a list that references it is in flight | `todo` | the animated scene (the showcase's cubes) keeps correct pixels; two-lists-in-flight proof; misses counted | — |
| B4 | **Pair A/B.** A+B against A alone | `todo` | 3D `work` improves by >0.05 ms over A; drop B with the number if not | — |

### C — baked static geometry (build `b40`)

| # | Task | Status | Acceptance | Result |
|---|---|---|---|---|
| C1 | **Load-time bake helper.** `rlpspBakeStaticMesh(Mesh*, options)`: triangle-strip indices (0-diff path) and optional s8 positions for meshes under a stated pixel-size threshold (s8 quantises to ≈1.8 px on screen-spanning geometry) | `todo` | a baked mesh renders from the same draw path; no per-frame cost; s8 is opt-in per mesh | — |
| C2 | **Gate scene.** Baked vs unbaked on a dedicated dense mesh | `todo` | strips: **0 diff**; s8: diff counted and bounded by the threshold rule, recorded as opt-in | — |
| C3 | **Integrate and measure.** Use a baked mesh in one sample (ground plane and/or cube) and record GE time on that scene | `todo` | GE time before/after; no `avg` regression on the pair | — |
| C4 | **Pair check.** A+B+C against A+B | `todo` | no regression (>0.05 ms); the pair is expected to be flat here, and that is recorded, not hidden | — |

### X — shared

| # | Task | Status | Acceptance | Result |
|---|---|---|---|---|
| X1 | **Pixel table** on the combined build | `todo` | 613 / 284 / 0 / 0 / clip / MESH_SCENE / RTT all as recorded; no full GE wait; fallback counters 0 outside A3 | — |
| X2 | **Close.** Pair re-run, RAM ledger, docs, driver hash, restore the chosen driver | `todo` | §9 criteria; one paragraph per workstream with its number | — |

## 6. Design notes

**A — reserve geometry.** The list is the hard bound: 252 KiB (258,048 B) against the GE maximum `0x3FFFF`. The 3D
frame's commands measured 4,664 B, so a 224 KiB reserve leaves ~28 KiB — checked at A2, not assumed. A new
`sceGuStart` is a new fill and legitimately gets a new reserve; what must not happen is a second reserve on a buffer
that was not restarted, which A1 determines. The reserve itself must be bounded by `listBytes − commandHeadroom`, or
the overrun happens before any guard can help. Detail: [086 §6](086-phase10-list-local-vertices.md).

**B — what may be cached.** Only the *index* bytes are cached, never vertices: vertices change with the merge's
instance transforms every frame, indices do not. The key is `(pointer, count, first)`; a changed key is a miss. The
pool is persistent because the list rotates and the arena is reused — a cache in either would be invalidated every
frame. The lifetime proof is the same shape as A's: a block handed to `sceGuDrawArray` must outlive the list that
references it, and the pool only overwrites a block whose last referencing list has completed. That is counted, not
assumed.

**B — the honest ceiling.** 0.45 ms of a 30.1 ms build is 1.5 %. If B1 measures materially less on the demo (the
probe used 36,000 indices, the demo 14,400 in two runs), the cache is not worth its lifetime risk and B is dropped
with the number.

**C — "offline" means load time, not a separate tool.** The demos build their meshes in code at startup. A bake step
that runs once in `InitWindow`/load and never in the frame loop is the realistic form here; a separate asset
converter is out of scope until there is an asset format to convert. Strips are the 0-diff win and the default;
s8 is opt-in per mesh because it moves pixels by ≈1.8 px on screen-spanning geometry.

**C — why it may not move the pair.** The 3D GE is 13.1 ms against a 30.1 ms build; lowering GE cannot lower the
frame. C is adopted for scenes where the GE *is* the wall and for the future asset path, and C4 records a flat pair
as the expected outcome rather than a failure.

**Interaction.** If A is adopted, B's index blocks may come from the list block when they fit; the cache pool exists
for runs that must persist across frames. If A is dropped, B still stands alone. C changes vertex counts, so it runs
last and its pair is measured against A+B.

## 7. Gates

| gate | expected |
|---|---|
| `batch_check` 2D | 613 colours |
| `basic_shapes` | 284 colours |
| `scene_3d SCENE=both` | 0 diff |
| `MESH_SCENE=1` | 0 diff |
| RTT | 0 diff |
| `clip_probe` | 237/720 as recorded |
| C2 baked mesh | strips 0 diff; s8 diff recorded and bounded |
| frame behaviour | no full GE wait; `arenaOverflows=0`; A3/C2 counters only in their own tests |
| benchmark pair | one pair per build (b38/b39/b40), two runs per point, >0.05 ms adoption bar |

## 8. Exit criteria

- [ ] A1's cause is stated with evidence and A2 follows from it
- [ ] each workstream is adopted or dropped **with its number**, recorded in §10 and the Phase 10 table
- [ ] the combined build passes the full pixel table with 0 diff
- [ ] the 3D pair shows the CPU workstreams' savings in `avg`, or the flat/modest result is stated with numbers
- [ ] the RAM ledger states bytes freed (arenas) and bytes used (cache pool)
- [ ] the guard is exercised by a frame that would otherwise overrun
- [ ] strict docs build passes; driver state at close is explicit (pspgu or pspGL)

## 9. Result

Pending — no device runs yet.

## 10. Follow-ups

- If A cannot fit the 3D frame, the measured per-fill vertex need is the number to carry: it bounds every list-local
  scheme.
- If B is adopted, the next question is whether the merge path can emit runs whose indices never change at all
  (whole-mesh draws), which would remove the cache entirely.
- If C is adopted, an asset converter becomes worthwhile only once the repo has an asset format; until then the bake
  helper is the whole pipeline.

**Audit findings from the writeback review (2026-11-24).** Every path that hands a pointer to the GE was checked
against the rule "the range must be visible, by writeback or by an uncached alias". The shipped build satisfies it
everywhere — copied runs go through `arenaCopy` (memcpy + writeback, gl_draw.c:936-937), the converting layout
packs and writes back at each site, static meshes are written back at load (rmodels.c:1945), and the list block is
never written back because `sceGuStart` maps it uncached. Two gaps are outside the shipped path and are recorded
here rather than fixed:

- **The direct-layout path has an unasserted visibility invariant.** `gl_draw.c:1429` passes the client's pointer to
  `sceGuDrawArray` with no writeback. It is correct today because the batcher's layout is caught by
  `isBatchedLayout()` first and copied, and because `Mesh` vertices are written back at load. Nothing states or
  checks that "whoever produced this pointer wrote it back", so a future caller with its own interleaved array reads
  stale vertices — the documented failure mode is degenerate, invisible primitives, not distorted ones. Follow-up:
  assert or log the invariant in debug builds, and copy when the provenance is not known.
- **`UpdateMeshBuffer` is a silent no-op on the GL 1.1 path.** `rmodels.c:2131` calls `rlUpdateVertexBuffer`, whose
  body is compiled only for `GRAPHICS_API_OPENGL_33` / `_ES2`. Dynamic mesh updates therefore change nothing and
  report no error. Follow-up: either implement it (memcpy into the mesh's array plus `sceKernelDcacheWritebackRange`,
  keeping `mesh.vertices` as the live source) or fail loudly. This is a functional gap, not a coherence hazard.

Neither is a reason to change the writeback model: the model is right, and 085 T6a measured the alternative
(21.593 → 20.106 ms with zero writebacks on a 2D frame). They are recorded so the next person does not re-derive
them.

## 11. References

- sprint 085 §9.1 (split), §9.3 (strips, index copy), §9.6 (T6a numbers), §9.7 (sync audit), §9.9 (close pair)
- [sprint 086](086-phase10-list-local-vertices.md) — workstream A's design note, folded in
- public pspsdk `src/gu/sceGuGetMemory.c` (4-byte block, two command words, stall update, no bounds check),
  `sceGuStart.c` (uncached alias)
- `tools/pspgu/egl.c` (`pspguBeginFrame`/`pspguEndFrame`, `pspguArenaAlloc`), `gl_draw.c:749-793` (index copy
  sites), `gl_texture.c:408` (the eviction path that can reopen a list)
- `tools/pspgu/pspgu_internal.h` — `PSPGU_LIST_VERTICES`, `PSPGU_LIST_VERTEX_BYTES`, the writeback no-op
