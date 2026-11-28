# Sprint 098 (P10-S28) — The merged-run copy: hand the vertices to the driver

| | |
|---|---|
| Roadmap phase | [Phase 10 — 3D and advanced](../phases/10-3d-advanced/index.md) |
| Sprint | P10-S28 |
| Branch / base commit | `master` @ `7b1e434e`; source of the lever: 096 §9.4–§9.6 (flush 4.9 ms, of which ~4.3 is the copy) |
| Started / closed | 2026-11-28 (written) |
| Build number(s) | pspgu `b38` only if a driver change is adopted; the arm is `PSP_MERGE_DIRECT=1` |
| Tasks | 0/6 |

## 0. Handover

The merged path builds a run's vertices into `pspMergeVerts` (cached, 256 KiB) and its indices into `pspMergeIdx`
(48 KiB), then calls `pspguDrawMergedArrays`, which allocates `vb + ib` in the driver arena and `memcpy`s both
(`tools/pspgu/gl_draw.c:1717`). 096 measured the flush at **4.9 ms/frame** at 400 cubes and put
the copy at **~4.3 ms** of it: 307 KiB of vertices plus 29 KiB of indices, moved once per frame, from cache into the
uncached arena.

That copy exists only because the driver owns its vertex memory. Take it away and the merged path saves ~4.3 ms — a
quarter of its 15.5 ms — with the expansion (10.6 ms) untouched.

## 1. Objective

Let raylib hand the driver a merge buffer it will draw from directly, safely, and prove it with a ceiling.

## 2. Why this shape, and not the other two

- **Build straight into the arena (uncached).** Removes the copy but replaces a cached build with 336 KiB of uncached
  stores per frame; the per-store bus cost is the reason the build and the copy are separate today. Rejected.
- **Double-buffer and `memcpy` anyway.** Saves nothing: the memcpy is the cost.
- **Cached slots + writeback + draw from them** (this sprint). The build stays cached, the writeback is the hardware
  flush of one range instead of a read-and-write copy loop, and the GE reads the same bytes.

## 3. Scope

**In:** a 2-slot merge buffer in raylib, a driver entry that draws from caller memory without copying, the lifetime
argument and its counter, the pixel gates, the ceiling A/B, the RAM ledger.

**Out:** the expansion loop (096 T4 showed no single line dominates); index-run caching beyond T2a; strips (096 T3,
still deferred); anything on the per-object path.

## 4. The lifetime argument, stated before it is coded

The driver owns two lists (`lists 2 x 240 KiB` in the boot line) and rotates them: starting list *n+2* waits for
list *n* to be done. The merge buffer is written during a frame, i.e. **after** `sceGuStart` for that frame. Therefore
a slot may be reused once every two frames, and two slots are exactly enough — provided the build happens after the
list start and the driver never draws from a slot whose list is still queued.

That "provided" is the whole risk, and it is what T3 instruments: a counter of frames where the slot's owning list was
not yet `PSP_GE_LIST_DONE` when its slot was reused. It must be **0** at 30 fps, 60 fps, `--vsync 0`, and at 1,200
instances, or the arm is dropped.

## 5. Tasks

| # | Task | Status | Acceptance | Result |
|---|---|---|---|---|
| T1 | **Slots.** `pspMergeVerts`/`pspMergeIdx` become `[PSP_MERGE_SLOTS][…]` with `PSP_MERGE_SLOTS=2`; `pspMergeBuild` writes slot `frameCount & 1`; the run records its slot | `todo` | build cost unchanged (±0.2 ms at COUNT=270); slot recorded in the flush | — |
| T2 | **`pspguDrawMergedArraysPinned(mode, count, indices, vertexCount, verts, flags)`** (weak): same contract as `pspguDrawMergedArrays` but no `pspguArenaAlloc` and no `memcpy`; `pspguWriteback(verts, vb+ib)` then `sceGuDrawArray` from the caller's pointers. Returns 0 if the layout is unsupported, and raylib then falls back to the existing entry | `todo` | the copy disappears from the flush; pspGL build unaffected (weak symbol absent) | — |
| T3 | **Hazard counter.** At each slot reuse, ask the driver whether the list that drew from that slot is done; count violations; log `MERGEHZ slots=… hazards=…` once per session | `todo` | **0 hazards** at 270 / 610, `--vsync 0`, and 1,200 instances | — |
| T4 | **Gates.** `batch_check` 613, `basic_shapes` 284, checksum `86b1e3c2 / c4b13eb4 / 670d502c`, `scene_3d SCENE=both` 0 diff | `todo` | all four exact | — |
| T5 | **Ceiling A/B.** The step-free metric: 270/610 today (§9.9 of 096). Measure the last count at 60 fps and at 30 fps with and without the arm, **interleaved in one session**, two runs per point | `todo` | ≥ **+40 cubes at 30 fps** (half the predicted 75) or the arm is dropped with the number | — |
| T6 | **RAM ledger and close.** Second slot bytes against the arena and against the 32 MB budget; adopt, gate, or drop | `todo` | a number per arm; docs strict; lane unlocked | — |

## 6. Design notes

**One slot switch, not a new allocator.** The slot is `frames & 1`, the same parity the driver uses for its lists, so
the mapping from slot to in-flight list is direct and needs no queue.

**The writeback is the point.** `sceKernelDcacheWritebackRange` on 336 KiB is one hardware sweep of a range the CPU
just wrote; the copy it replaces reads 336 KiB out of cache and writes it through the bus in a loop.

**Fallback is mandatory.** A single `libraylib.a` links against pspgu and pspGL; the weak symbol keeps the reference
build working exactly as 092's bake did.

**At 1,200 instances the arena wall is the failure mode.** The pinned path removes the arena from the merge path
entirely for these runs, which is a second, unlooked-for benefit — record whether 1,200 now draws without replays, but
do not adopt on it.

## 7. Gates

| gate | expected |
|---|---|
| `batch_check` / `basic_shapes` | 613 / 284 |
| checksum 60/120/240 | `86b1e3c2` / `c4b13eb4` / `670d502c` |
| `scene_3d SCENE=both` | 0 differing pixels |
| hazards | 0 at 270, 610, vsync 0, 1,200 instances |
| ceilings | 270 / 610 today; the arm's counts stated with two runs each |

## 8. Exit criteria

- [ ] the copy is gone from the flush, or the arm is dropped with its measured cost
- [ ] hazards 0 in every configuration named above
- [ ] pixel gates exact on all four
- [ ] ceiling counts reported per arm, same session, interleaved
- [ ] RAM ledger states the extra bytes
- [ ] lane a unlocked; `psp/lib` and the driver left in the adopted state; docs strict

## 9. Result

*(to be filled)*

## 10. Follow-ups

- If the ceiling moves, 099 re-bases its acceptance on the new counts before it runs.
- The pinned entry is the natural place to later remove the per-frame expansion as well (a persistent world-space
  bake), but only 093's arm can justify that.

## 11. References

- 096 §9.4 (the attribution: build 10.6 ms, flush 4.9 ms), §9.6 (why the copy was prioritised), §9.9 (ceilings 270/610)
- `tools/pspgu/gl_draw.c:1685` — `pspguDrawMergedArrays`, the copy at `:1717` (repo paths, not doc links)
- `src/platforms/rlpsp_mesh.c` — `pspMergeBuild`, `rlpspMergeFlush`, the 256 KiB / 48 KiB buffers at `:372`
- [Benchmarking recipe](../recipes/benchmarking.md) — ceilings, lanes, the lock
