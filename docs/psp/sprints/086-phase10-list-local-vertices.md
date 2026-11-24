# Sprint 086 (P10-S16) — List-local vertices: a reserve that survives a reopen

| | |
|---|---|
| Roadmap phase | [Phase 10 — 3D and advanced](../phases/10-3d-advanced/index.md) |
| Sprint | P10-S16 |
| Branch / base commit | `master` @ `beff3574` (085 closed; pspgu `9798af84e81b269e`, `libraylib.a c2004b266a6c3291`) |
| Started / closed | 2026-11-24 (written) / — |
| Build number(s) | pspgu `b38` **only if adopted**; the arm stays `PSPGU_LIST_VERTICES=1`, default 0 |
| Tasks | 0/7 — T1 is a diagnosis; adoption is gated on the pair and the pixel table |

## 0. Handover

**Where 085 left it.** Sprint 085 closed 9/9 with **no default change**: five levers dropped with numbers, T1's
GE/CPU split done, coherence verified. Installed state is the gated control — pspgu `9798af84e81b269e`,
`libraylib.a c2004b266a6c3291` — and the closing pair was 3D **33.548 ms / work 32.071**, 2D **33.803 / work
21.590**.

**The one lever that failed for an engineering reason.** T6a (085 §9.6) moved the frame's vertex memory into the
display list through `sceGuGetMemory`:

| workload | work | block used | fallbacks |
|---|---|---|---|
| 2D showcase | 21.593 → **20.106 ms** (−1.49 ms, −6.9 %) | 87,456 B of 160 KiB | 0 |
| 3D showcase | 32.059 → 32.934 ms (+0.88 ms) | 161,768 B, pinned | 50 |

T1's split says why this matters more than anything else 085 measured: the 3D frame is **CPU-bound** (30.1 ms build
against 13.1 ms GE), and this is a CPU-side saving. On 2D it is real but invisible, because 2D is GE-bound (19.5
against 29.2).

**What 085 did not establish.** The 3D ceiling of exactly 161,768 bytes with one fallback per submit was explained
in 085 by a hypothesis — that the frame reopens its list mid-frame — and a second explanation fits the same data:
chunky allocations exhausting the budget, where the last chunk fails at every budget tried (160 / 200 / 208 KiB)
because 208 KiB still leaves only 43 KiB for a ~45 KiB chunk. **T1 separates them.** Nothing here assumes which is
true.

## 1. Objective

Make list-local vertices usable on the CPU-bound 3D frame, with a guard that cannot overrun the list, and adopt only
if the benchmark pair moves and every pixel gate stays 0 diff.

The claim under test: the 2D result (−1.49 ms of list-build CPU) transfers to 3D at a similar or larger size, and
because 3D is CPU-bound the saving appears in `avg`, not only in `work`.

## 2. Why now

- It is the only measured CPU-side saving in 085 (everything else was GE-side or submission-side and measured at
  microseconds).
- It also removes up to **1 MiB of main RAM** (the 2 × 512 KiB arenas) if a small fallback replaces them.
- The alternative — accepting 30.1 ms as opaque — leaves the frame's dominant cost unexamined, which is the state
  085's own verdict complains about.

## 3. Scope

**In:** the diagnostic that explains 161,768; a reserve strategy sized from the actual list; reopen handling; a
bounds guard with a clean fallback; an overflow test that forces the fallback; the pair A/B on both workloads; the
full pixel table; an arena-removal arm that measures the RAM freed.

**Out:** the index-run cache (085 §10 follow-up, its own sprint); offline asset baking (085 §10); CALL lists
(dropped, 085 §9.7); any change to the merge path or the batcher.

## 4. Preconditions

- [x] 085 closed; driver `9798af84e81b269e`, library `c2004b266a6c3291` installed and verified
- [x] `PSPGU_LIST_VERTICES` flag, 4-byte bump allocator and the no-op writeback already in the tree, compiled out
      (`tools/pspgu/pspgu_internal.h`, `egl.c`)
- [x] PSPLink available for the session
- [ ] the control pair re-read before any A/B: 3D 33.548 / work 32.071, 2D 33.803 / work 21.590
- [ ] the diagnostic from T1 exists before any budget is changed

## 4a. Readiness review (2026-11-24)

Reviewed against `master @ beff3574` and the code 085 left behind. The arm is **not** ready to A/B: it has no
diagnosis, no guard against a reserve larger than the list, and no pixel evidence. Verdict: **T1 startable now;
T2-T4 need T1's answer; T5-T6 are mechanical once T4 has something to gate.** The gear is right — the flag, the
allocator and the writeback no-op all exist and compile to nothing when off — but the reserve geometry is the open
question, not the plumbing.

## 5. Tasks

| # | Task | Status | Acceptance | Result |
|---|---|---|---|---|
| T1 | **Diagnose the 161,768 ceiling.** Instrument only: log per fill the reserve size and address, the number and sizes of sub-allocations, the size of the one that fails, and the number of `pspguBeginFrame` calls per buffer per frame. Separate (a) chunked allocation from (b) reopen. No behaviour change | `todo` | the cause is stated with evidence, and the change that would fix it is named; no default behaviour changes | — |
| T2 | **Reserve strategy.** Raise `PSP_GU_LIST_BYTES` to 252 KiB (GE maximum `0x3FFFF`), size the reserve from the actual list minus a command headroom, and re-reserve on a fresh `sceGuStart` after a reopen while clearing the old block. Log `listVertexUsedMax` and the remaining command room | `todo` | 3D showcase: **0 fallbacks**, `list_used_max` + reserve under the list, no `LIST OVERFLOW`, `arenaOverflows=0` | — |
| T3 | **Guard and fallback.** Refuse a sub-allocation that would pass the block, count it, fall back to the arena; a synthetic frame that exceeds the block must still render correctly | `todo` | synthetic overflow frame: correct pixels, fallback counter > 0, no corruption; normal frames: counter 0 | — |
| T4 | **Pair A/B.** Both workloads, two runs per point, against the §4 control | `todo` | 3D `work` and `avg` both improve by >0.05 ms; 2D `work` improves with `avg` unchanged (GE-bound, expected); drop the arm if 3D does not move | — |
| T5 | **Pixel table.** Full gate run on the adopted arm | `todo` | 613 / 284 / 0 / 0 / clip / MESH_SCENE / RTT all as recorded; no full GE wait | — |
| T6 | **Arena-removal arm.** Replace 2 × 512 KiB with a small fallback (candidate 64 KiB) and measure the RAM ledger | `todo` | freed RAM stated in bytes; gates still 0; fallback still exercised by T3's frame | — |
| T7 | **Close.** Pair re-run, docs, driver hash, restore the chosen driver, exit criteria | `todo` | as 085 §8, plus the RAM ledger and the guard's counter at close | — |

## 6. Design notes

**Reserve geometry.** The list is the hard bound: 240 KiB today, `0x3FFFF` (262,143 B) is the GE maximum, and 252 KiB
(258,048 B) leaves a little margin. Commands for the 3D frame measured 4,664 B (`list_used_max` 168,504 − 163,840),
so a 224 KiB reserve leaves ~28 KiB of command room — comfortably more than 4.7 KiB, but this must be checked at
T2 rather than assumed.

**Reopen.** `pspguFinishAndWait` (egl.c:544) closes the list, and a later emitter calls `pspguBeginFrame` again,
which restarts `sceGuStart` on the same buffer. Any block from the previous fill is gone with it, so the correct
handling is: a new `sceGuStart` means a **new fill**, and a new fill gets a new reserve. What must not happen is a
second reserve on a buffer that was *not* restarted; T1 says whether that occurs.

**Guard.** `sceGuGetMemory` has no bounds check, so the driver must own the bound. The block is exactly what was
reserved; `pspguArenaAlloc` refuses past it and counts the refusal. The reserve itself must also be bounded by
`listBytes - commandHeadroom`, or the overrun happens at reserve time, before any guard can help.

**Alignment.** `sceGuGetMemory` returns 4-byte-aligned memory. The arena's 64-byte rule exists because a writeback
covers whole cache lines; the list block is uncached and never written back, so the rule does not apply. The pixel
table is the evidence, not the argument.

**Fallback.** The arena stays until T6 proves it can be shrunk. `arenaOverflows` must stay 0 on normal frames, and
the new fallback counter must stay 0 outside T3's synthetic frame.

**RAM ledger.** 2 × 512 KiB arenas today. T6 states the bytes actually returned to the user partition, measured with
the driver's own VRAM/RAM reporting, not by arithmetic on the defines.

## 7. Gates

| gate | expected |
|---|---|
| `batch_check` 2D | 613 colours |
| `basic_shapes` | 284 colours |
| `scene_3d SCENE=both` | 0 diff |
| `MESH_SCENE=1` | 0 diff |
| RTT | 0 diff |
| `clip_probe` | 237/720 as recorded |
| frame behaviour | no full GE wait; `arenaOverflows=0`; fallback counter 0 outside T3 |
| benchmark pair | two runs per point; adopt only on a >0.05 ms 3D improvement |

## 8. Exit criteria

- [ ] T1's cause is stated with evidence and the fix follows from it
- [ ] the 3D showcase runs with 0 fallbacks, or the arm is dropped with the number that prevents it
- [ ] the guard is exercised by a frame that would otherwise overrun, with correct pixels
- [ ] the pixel table is 0 diff on the adopted arm
- [ ] the pair shows the 3D saving in `avg`, or the arm is dropped with the pair as evidence
- [ ] the RAM ledger states the bytes freed if the arena shrinks
- [ ] adopted: `b38`, pair re-run, docs updated; dropped: recorded here and in the Phase 10 table, code compiled out
- [ ] strict docs build passes; driver state at close is explicit

## 9. Result

Pending — no device runs yet.

## 10. Follow-ups

- If the reserve cannot fit the 3D frame, the frame's vertex need is the number to carry: it bounds every
  list-local scheme and says how much the merge path must shrink before this becomes possible.
- If T6 frees the arenas, the index-run cache (085 §10) becomes easier: it needs a home that is not a rotating
  arena.
- If T4 shows the CPU saving does not reach `avg` even when 3D is CPU-bound, the build time is not where the frame
  waits, and T1's split needs revisiting before any further CPU work.

## 11. References

- sprint 085 §9.1 (T1 split), §9.6 (T6a numbers), §9.7 (sync audit), §9.9 (close pair)
- public pspsdk `src/gu/sceGuGetMemory.c` (4-byte block, two command words, stall update, no bounds check),
  `sceGuStart.c` (uncached alias), `sceGuFinishId.c`
- `tools/pspgu/egl.c` — `pspguBeginFrame`/`pspguEndFrame`, `pspguArenaAlloc`; `gl_texture.c:408` (the eviction
  path that can reopen a list); `gl_draw.c:738-793` (arena callers and their writebacks)
- `tools/pspgu/pspgu_internal.h` — `PSPGU_LIST_VERTICES`, `PSPGU_LIST_VERTEX_BYTES`, the writeback no-op
- 6.6.0 reference tree — `sceGuGetMemory` usage observed in 30 graphics samples; no vendor code or documentation
  copied
