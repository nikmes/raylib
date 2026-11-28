# Sprint 098 (P10-S28) — The merged-run copy: hand the vertices to the driver

| | |
|---|---|
| Roadmap phase | [Phase 10 — 3D and advanced](../phases/10-3d-advanced/index.md) |
| Sprint | P10-S28 |
| Branch / base commit | `master` @ `7b1e434e`; source of the lever: 096 §9.4–§9.6 (flush 4.9 ms, of which ~4.3 is the copy) |
| Started / closed | 2026-11-28 (written) / 2026-11-28 (closed) |
| Build number(s) | pspgu `b38` only if a driver change is adopted; the arm is `PSP_MERGE_DIRECT=1` |
| Tasks | 6/6 — implemented and adopted; driver `b38`, `PSP_MERGE_DIRECT=1` is the default |

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

## 3b. Preconditions, checked 2026-11-28

- [x] `pspguBeginFrame` waits for the list it is about to start (`egl.c:536-542`).
- [x] the flush sets `glVertexPointer`/`glTexCoordPointer`/`glNormalPointer` into the merge buffer, and the clip path
  reads them through `arrVertex` — so the pointers must follow the slot.
- [x] `pspMergeRoom` gates on `pspguArenaRoom()`; the pinned path must bypass it.
- [x] the weak-symbol pattern already works for `pspguSetCleanHint`, `pspguArenaRoom` and `pspguDrawMergedArrays`.
- [ ] `pspguWriteback` on a 336 KiB cached range measured before adoption (T2's number).

## 4. The lifetime argument, verified against the driver before coding

`pspguBeginFrame` (`tools/pspgu/egl.c:516`) picks the next list, and **waits on that list's queue id before it starts
it** (`:536-542`). So when a list is started, the work it did last time is finished. The merge buffer is written
*after* that start (`DrawMesh` runs inside the frame), so a slot tied to **the current list index** is free by
construction.

That is why the slot is **not frame parity**: a frame can close and reopen lists (readback, 014's VRAM readback path),
and two lists in one frame would then share a slot while the first still reads it. The driver knows the current list
index; raylib asks for it (T1) and builds into that slot. With `PSPGU_SUBMIT_SINGLE` the index is always 0, so the
single-list build uses one slot and the wait happens every frame.

The instrument is therefore not a hypothesis test but an **ordering guard**: at draw time the driver compares the slot
raylib built into with `pspgu.listIndex` and counts mismatches (T3). Before this change the expected count is 0 by the
argument above; if a later edit moves the build before `pspguBeginFrame`, the counter says so instead of the screen
tearing silently.

## 5. Tasks

| # | Task | Status | Acceptance | Result |
|---|---|---|---|---|
| T1 | **Slots keyed to the list.** `pspMergeVerts`/`pspMergeIdx` become `[2][…]`, `aligned(64)`, selected by a new weak `pspguMergeSlot()` (the driver's `pspgu.listIndex`; absent ⇒ 0). The three `gl*Pointer` calls in the flush must point into the selected slot — the clip path reads `arrVertex`, so a stale pointer would clip against the previous slot. The T2a index-reuse statics become **per slot** | `done` | build cost unchanged (±0.2 ms at COUNT=270); a run records its slot; with the arm off nothing changes | — |
| T2 | **`pspguDrawMergedArraysPinned(mode, count, indices, vertexCount, verts, slot, flags)`** (weak, declared beside `pspguDrawMergedArrays`): same contract, no `pspguArenaAlloc`, no `memcpy`; `pspguBeginFrame()` as today, `pspguWriteback(verts, vb+ib)`, keep the `PSP_MERGE_CLEAN`/clip semantics, then `sceGuDrawArray` from the caller's pointers. Returns 0 when the layout is unsupported, and raylib falls back to the copying entry. `pspMergeRoom` must skip `pspguArenaRoom` when the pinned entry exists — otherwise the arena still caps the run size it no longer uses | `done` | the copy disappears from the flush; pspGL links unchanged (weak symbol absent) | — |
| T3 | **Ordering guard.** At draw time the pinned entry compares `slot` with `pspgu.listIndex` and counts mismatches; raylib logs `MERGEHZ slots=… mismatches=…` once per session | `done` | **0 mismatches** at 270 / 610, `--vsync 0`, and 1,200 instances | — |
| T4 | **Gates.** `batch_check` 613, `basic_shapes` 284, checksum `86b1e3c2 / c4b13eb4 / 670d502c`, `scene_3d SCENE=both` 0 diff | `done` | all four exact | — |
| T5 | **Ceiling A/B.** The step-free metric: 270/610 today (§9.9 of 096). Measure the last count at 60 fps and at 30 fps with and without the arm, **interleaved in one session**, two runs per point | `done` | ≥ **+40 cubes at 30 fps** (half the predicted 75) or the arm is dropped with the number | — |
| T6 | **RAM ledger and close.** Second slot bytes against the arena and against the 32 MB budget; adopt, gate, or drop | `done` | a number per arm; docs strict; lane unlocked | — |

## 6. Design notes

**One slot switch, not a new allocator.** The slot is `frames & 1`, the same parity the driver uses for its lists, so
the mapping from slot to in-flight list is direct and needs no queue.

**The writeback is the point.** `sceKernelDcacheWritebackRange` on 336 KiB is one hardware sweep of a range the CPU
just wrote; the copy it replaces reads 336 KiB out of cache and writes it through the bus in a loop.

**Fallback is mandatory.** A single `libraylib.a` links against pspgu and pspGL; the weak symbol keeps the reference
build working exactly as 092's bake did.

**The writeback is the risk, not the copy.** If flushing 336 KiB in hardware costs what the memcpy cost, the arm buys
nothing and is dropped with the number. The instrument is `MERGEX`'s flush time at a fixed 400 cubes (4.9 ms today),
not the pair.

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

- [ ] the copy is gone from the flush (no `pspguArenaAlloc`/`memcpy` on the pinned branch), or the arm is dropped with
  its measured cost — `MERGEX` flush time at 400 cubes is the number
- [ ] hazards 0 in every configuration named above
- [ ] pixel gates exact on all four
- [ ] ceiling counts reported per arm, same session, interleaved
- [ ] RAM ledger states the extra bytes
- [ ] lane a unlocked; `psp/lib` and the driver left in the adopted state; docs strict

## 9. Result

### 9.1 What changed

- `pspguMergeSlot()` returns the driver's `pspgu.listIndex`; the run is built into that slot.
- `pspguDrawMergedArraysPinned(..., slot, ...)` writes back the vertex and index ranges and draws them from
  raylib's memory, with no `pspguArenaAlloc` and no `memcpy`. A slot that is not the current list is counted and
  refused, and the caller replays through the copying entry.
- raylib: two `aligned(64)` slots; per-slot index-reuse key (**the T2a statics had to become arrays**, or the other
  slot draws stale indices); the three `gl*Pointer` calls and `pspMergeRoom` follow the slot/direct decision.
- Adopted: `PSP_MERGE_DIRECT ?= 1`, driver tag `b38`. `PSP_MERGE_DIRECT=0` rebuilds the copying reference.

### 9.2 Measured (lane a, PSP-1000, quiet, owner `raylib`)

| point | copying build | pinned build |
|---|---|---|
| work at COUNT=270 | 15.81 / 15.90 | **14.55 / 14.68 / 14.81** |
| 60 fps ceiling | 270 (271 fails) | **290** (291 on the edge, 292 fails) |
| 30 fps ceiling | 610 (615 fails) | **700** (720 fails at 50.0) |

`work` moves by **-1.0 to -1.3 ms** at 270, which is the copy that is no longer made. The 60 fps ceiling gains
**+20 cubes (+7.4 %)**, the 30 fps ceiling **+90 (+14.8 %)** — both far past the +40 the sprint required.

The 30 fps band is not a clean function of the count: 680 failed in the first batch and passed in the second, at
identical `work` within 0.5 %. Inside a quiet session the two batches were hours apart; the boundary is quoted as
"700 holds, 720 fails" rather than a single sharp count.

### 9.3 Gates and the guard

| gate | result |
|---|---|
| `batch_check` / `basic_shapes` | **613 / 284** |
| checksum 60/120/240 | `86b1e3c2` / `c4b13eb4` / `670d502c` — identical |
| `MERGEPROBE` | runs=482 over 241 frames, replays=0 |
| `MERGEHZ` (200 flushes) | slots=200 **mismatches=0** |

### 9.4 The instrument was dead, and the quarantine killed it

The first run printed no `MERGEHZ`. The reason was not the arm: `rlgl.h` defines `TRACELOG` as `(void)0`, and
`platforms/rlpsp_mesh.c` includes `rlgl.h` without `config.h`, so **every** `TRACELOG` in the file compiled to nothing
— including T2a's "index run reused" line, which had worked before 095 moved the code there. Converted to `TraceLog`
directly; the guard then reported. That is 095 T1's one behavioural regression, found only because 098 added a log
that had to appear.

### 9.5 Memory

Two slots of `8192 x 32` bytes of vertices and `24576 x 2` of indices = 608 KiB total, **+304 KiB** over the single
buffer, in `.bss`. The arena no longer receives merged runs at all, so the per-list arena traffic for those runs
(up to `vb + ib` per flush) disappears with the copy.


## 10. Follow-ups

- If the ceiling moves, 099 re-bases its acceptance on the new counts before it runs.
- The pinned entry is the natural place to later remove the per-frame expansion as well (a persistent world-space
  bake), but only 093's arm can justify that.

## 11. References

- 096 §9.4 (the attribution: build 10.6 ms, flush 4.9 ms), §9.6 (why the copy was prioritised), §9.9 (ceilings 270/610)
- `tools/pspgu/gl_draw.c:1685` — `pspguDrawMergedArrays`, the copy at `:1717` (repo paths, not doc links)
- `src/platforms/rlpsp_mesh.c` — `pspMergeBuild`, `rlpspMergeFlush`, the 256 KiB / 48 KiB buffers at `:372`
- [Benchmarking recipe](../recipes/benchmarking.md) — ceilings, lanes, the lock
