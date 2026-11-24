# Sprint 085 (P10-S15) — Hardware optimisation sweep: formats, submission, clear, coherence and GE/CPU attribution

| | |
|---|---|
| Roadmap phase | [Phase 10 — 3D and advanced](../phases/10-3d-advanced/index.md) |
| Sprint | P10-S15 |
| Branch / base commit | `master` @ `27391da3` (pspgu `b37`; P10-S13 T1; docs commits) |
| Started / closed | 2026-11-23 (written) / — |
| Build number(s) | pspgu `b38` for any adopted driver change; installed `libraylib.a` `c2004b26...` (`PSP_BATCH=1 PSP_MERGE_DRAWS=1`, P10-S13 T1); new probes opt-in |
| Tasks | 8/9 active — T1 done (§9.1), T2 dropped (§9.2), T3 done (§9.3), T4 dropped (§9.4), T5b dropped (§9.5), T6a dropped (§9.6), T6 dropped (§9.7), T7 done (§9.8); T2b/T5 dropped on prior evidence |

## 0. Handover

**Situation.** Phase 10 closed at P10-S12 and the two follow-up sprints (P10-S13/P10-S14) added state and
reference-idea checks. The b37 driver and the P10-S13 T1 library `c2004b26...` are the current baseline. The
historical b37 pair (3D **33.677** / 2D **21.578 ms**, driver `9798af84...`) predates the lighting/fog scoping
(4 GE state commands/frame), so it is not the control. The 2D/3D gates are 0 diff and the launcher embeds both.

A new list of PSP optimisations has arrived from a general hardware guide. Most of it is already adopted here, or
was measured and dropped for a specific workload. This sprint is therefore **not** a re-run of the list. It isolates
the few untested paths, gives each a workload where the claimed bottleneck could actually exist, and adopts only a
pixel-clean, repeatable win. Every speed claim needs T1's GE/CPU split; today the harness has `work` and `swap` but
not GE time.

**The current T1 pair is the control.** Re-run `tools/psp-bench.sh` on `c2004b26...` before any task comparison;
the b37 pair is historical. One tag per sprint (`b38`), forced object removal before every driver build, and two
runs per point.

### What the list asks for, and what is already true here

| area | technique | our state | decision |
|---|---|---|---|
| Geometry | S16 vertices | adopted for packed meshes: `PspGeMesh`, 28-byte stride, `direct=1` (061) | baseline; no task |
| Geometry | S8 vertices | dropped in 077 T5 **without a device run** because the measured scenes are draw-call bound and parity-bound | re-test only on a new **vertex-bound** workload (T2) |
| Geometry | triangle strips | `gl_draw.c:1292-1293` maps `GL_TRIANGLE_STRIP/FAN`, but the raylib batcher emits only lines/triangles/quads; no 3D strip workload exists | T3 |
| Geometry | reduced indexing | indices are copied/rebased per chunk; 066 deferred a direct index path; `PspGeMesh` indices still travel per draw | T3 |
| Textures | CLUT4 | measured in 017: 8,256 B vs 32,768 B, upload 0.080 vs 0.532 ms; **dropped for 2D** (not VRAM-bound), 3D revisit deferred | T4, conditional on a texture-fetch-bound 3D scene |
| Textures | CLUT8 | never measured | T4 |
| Textures | swizzled data | adopted in 012; upload is swizzled, pixel parity 0 | baseline; no task |
| Framebuffer | 16-bit colour | 5650, three buffers + depth, adopted | baseline; no task |
| Framebuffer | 32/64-px clear stripes | `GU_FAST_CLEAR_BIT` measured **null** in 079 T2 (0.265 → 0.268 ms); b36 then removed the flag | **T5 dropped** — no code re-opened |
| Submission | batched display lists | merged clean runs adopted in 078 (N=500 43.22 → 26.67 ms); static `GU_CALL` sub-lists untested | T6 |
| Submission | fewer GE sync points | targeted two-lists-in-flight waits, 13-19 µs, no full wait (069/076); all `sceGeListSync`/`TexSync` sites not audited as a set | T6 |
| Coherence | writeback and buffer lifetime | per-list 64-byte-aligned arenas, writeback before every GE pointer, texture pinning across two frames | T7 verification; no feature unless a bug is found |
| Profiling | GE and CPU time separately | `work = avg - swap`; no GE time; `sceGeDrawSync(1)` is explicitly not a GE timer (see the guide) | **T1 first** |

A second reference, **gLib2D** (LGPLv3), contributed three implementation choices: its 2D through-mode
16-bit-UV vertex format (T2b, **dropped after review**), list-local vertex memory via `sceGuGetMemory` (T6a), and
64-px sprite slicing (T5b). Its single-list full-sync model, global dither-off and scissor call are explicitly
rejected in §6; only ideas are used, no code.

## 1. Objective

Falsifiable, per candidate:

1. T1 produces a stable split of one frame into **GE time**, **CPU render work** and **swap wait** on the device,
   reproducible against the b37 control.
2. Each later task runs a control and a control-again (or interleaved variants) on a workload where its claimed
   bottleneck exists, and reports the pixel result.
3. A task is **adopted** only if the measured GE or work delta exceeds the harness repeatability (0.05 ms work;
   GE time as measured by T1) and every pixel gate stays 0. Otherwise it is **dropped with the number**.
4. No task changes the default driver without the gate table in §7.

## 2. Why now

The current pair is stable and fast enough to serve as a control, and the remaining list items are second-order
features: they do not help a draw-call-bound 3D scene or a batcher-bound 2D scene by construction. Before spending
code on them, the project needs the missing measurement split (T1) and a workload per claim (dense mesh, CLUT
texture, clear-only frame, 1,000-mesh submission). This is the last systematic sweep of the hardware levers; it is
measurement-only unless a task wins.

## 3. Scope

**In:** T1 GE/CPU attribution; S8 on a vertex-bound mesh; triangle strips and a static index path; CLUT4/8 on a 3D
texture; 64-px sprite slicing (T5b); static `GU_CALL` lists and a sync audit; a coherence stress probe; gates and
close.

**Out:** GUM, per-sprite submission, 32-64 px **blit** strips (023 null), compact 5551 colour, removing the CPU clip,
the Media Engine, audio, streaming, and any change to the 2D `batch_check` pixel contract.

## 4. Preconditions

- [x] pspgu `b37` installed; the installed `libraylib.a` is `c2004b26...` (P10-S13 T1); the b37 pair is historical
- [x] main tree clean except `ai-memory.db-shm`; no isolated worktree required
- [x] PSPLink is available for this session; coordinate if another session owns it
- [x] the fresh control pair on `c2004b26...` is recorded before T1 is compared — 3D **33.569** (work 32.059,
      swap 1.51), 2D **33.803** (work 21.593, swap 12.21); `psp-tests/bench/results/p10s15-control.json`
- [x] the T1 probe is produced by T1; it is not a precondition — `psp-tests/ge_probe`, §9.1

## 4a. Readiness review (2026-11-24)

Reviewed against `master @ 27391da3`, pspgu `b37` and `libraylib.a c2004b26...` (P10-S13 T1). The sprint is a
measurement plan, not a set of flag flips. Verdict: **T1, T2, T3, T4, T5b, T6, T6a and T7 are startable; T2b and
T5 are dropped** on their prior numbers; the baseline is stale and must be re-recorded.

**Baseline.** The §0 pair (33.677 / 21.578 ms) predates P10-S13 T1. Re-run the pair on `c2004b26...` and use it as
the control. The P10-S13 gates (batch 613, scene_3d 0, shapes 284) are the current pixel control.

**T1 — no probe exists.** `matrix_probe` has api/calls/gum/scene, all inside the driver's direct lists; there is no
GE/SEND mode. A `GU_SEND` list cannot be opened while a `GU_DIRECT` list is open. Add a standalone raw-GU
`psp-tests/ge_probe`: init GU, build the representative frame in a `GU_SEND` list (the 400-cube merged scene and a
2,000-sprite batch), `sceGuSendList` + `sceGuSync(GU_SYNC_FINISH, GU_SYNC_WHAT_DONE)` with the CPU idle, and time
the sync; build the same list CPU-only to subtract. Cut by draw category. Keep its vertex/index buffers separate
from the driver's direct lists; report the GE time beside the direct-list work/swap.

**T2b — dropped.** The b36 revert removed `PSPGU_FAST2D`, `pspguCanUseTransform2D` and
`pspguBoundTexture2D`, so there is no through-mode path to toggle. More importantly, the prior evidence is
negative: 079 T4 through mode was **0.942 → 0.972 ms** for one quad and **8.583 → 9.531 ms** at 1,600 sprites (0
diff but slower); 077 T5 dropped the compact 2D formats because the scene is parity-bound and CPU-bound; 061's
s16-UV trial cost 37 px. Do not restore the code; re-open only with a new vertex-bound workload that reverses
those numbers.

**T5 — dropped.** `GU_FAST_CLEAR_BIT` measured **null** in 079 T2: clear 0.265 → 0.268 ms (+1.1 %) and
bunnymark 4.706 → 4.702 ms with 0 diff; 023's 32-64 px blit strips were also null. The b36 revert then removed
`PSPGU_FAST_CLEAR`, so there is no flag to toggle. Do not restore it; re-open only with a new clear-bound
workload.

**T2 — S8.** Add `PSPGU_VERTEX_S8`; `guVertexFormat`/`pspGePack` emit only 32/16-bit positions; the 28-byte s16
path is the baseline. Workload: a dense single-draw mesh; the scale-128 rule; the pixel gate.

**T3 — strips.** `guPrimitive` maps `GL_TRIANGLE_STRIP/FAN`; the raylib batcher does not emit them. Use a probe with
`glDrawArrays(GL_TRIANGLE_STRIP)` and client arrays (or a raw GU list); the CPU clip only touches `GU_TRIANGLES`.
The static index cache lives in `pspGePack`/`glDrawElements`; the app indices are static; the list lifetime applies.

**T4 — CLUT.** `gl_texture.c` has no CLUT; add `GU_PSM_T4/T8`, `sceGuClutLoad`/`ClutMode` and an offline quantiser.
Only run it if T1 shows a texture-fetch-bound 3D scene; otherwise drop with that number.

**T6 — CALL lists.** The driver has no `sceGuCallList`; a static `PspGeMesh` sub-list, matrices in the parent, the
1,000-mesh scene and the sync-site audit are the work. No full wait.

**T7 — coherence.** A loader thread and a probe over the existing writeback/arena/pinning; no code unless a frame
fails.

**T6a — ready.** The flags, 160 KiB reserve, bump allocator, writeback skip, arena/no-arena arms, `listUsedMax`
guard and workloads are specified above.

**Order.** T1 first; T2/T3/T6a next; T4/T5b/T6/T7; T8. T2b/T5 are dropped and are not on the path. The gate table
and the adoption threshold are unchanged.

## 5. Tasks

| ID | task | status | verification | evidence |
|---|---|---|---|---|
| T1 | **GE/CPU attribution.** Add a serial-replay probe: build one frame with `sceGuStart(GU_SEND, list)`, `sceGuSendList` and `sceGuSync(GU_SYNC_FINISH, GU_SYNC_WHAT_DONE)` while the CPU is idle; report GE ms, CPU render ms, swap ms, and the existing per-phase counters (clip, cull, submit). Do not use `sceGeDrawSync(1)` as a GE timer | `done` | GE + CPU + swap reconstructs the fresh control pair within the harness repeatability; two runs agree; no game change | §9.1 — probe repeats within 1 µs; 3D CPU-bound 30.1 vs 13.1 ms, 2D GE-bound 19.5 vs 29.2 ms |
| T2 | **S8 positions.** Add an opt-in `GU_VERTEX_8BIT` packed path (scale 128 about the mesh centre, same scale-matrix rule as s16). Workload: a dense mesh (≥10k vertices) drawn once per frame, not the 400-cube draw-call-bound scene. A/B S16 vs S8 | `dropped` | T1 GE time and work; pixel parity on a dedicated mesh capture; adopt only if it beats the control and 0 diff | §9.2 — GE 1,852→1,681 µs (−0.171 ms, 9.2 %) on 36,000 verts, but 59,538/130,560 pixels differ: s8 quantises a screen-wide mesh to ~1.8 px |
| T2b | **2D through-mode 16-bit UV.** gLib2D's 20-byte textured vertex: `GU_TRANSFORM_2D` + `GU_TEXTURE_16BIT`, 32-bit position, 2-vertex `GU_SPRITES` for unrotated and 6-vertex triangles for rotated. Workload: 2,000+ sprites / `psp_2d_showcase`, where vertex copy/fetch is a measurable share. Prior: 079 T4 through mode with 32-bit UV was slower; 061's s16-UV trial cost 37 px; 077 T5 dropped compact 2D. Only the 16-bit-UV variant is tested | `dropped` | 079 T4 through mode slower; 077 T5 compact 2D; 061 s16 UV 37 px; b36 removed the path | prior numbers |
| T3 | **Strips and index cache.** (a) Stripify a dense mesh and draw `GU_TRIANGLE_STRIP` via a probe or a `PspGeMesh` strip path; compare vertex/index count, GE time and pixels with `GU_TRIANGLES`. (b) For static `PspGeMesh` indices, copy the index run once per mesh/list (or hand the app pointer after a writeback) instead of per draw; measure the CPU copy | `done` | T1 split; 0 diff; per-list lifetime proof; no `arenaOverflows` | §9.3 — strips −1.073 ms GE (−58 %) at 0/130,560 px for +46 µs CPU build; index copy 31 µs per 1,000 (≈0.45 ms/frame in 3D) → follow-up |
| T4 | **CLUT4/CLUT8, 3D only.** If T1 shows a texture-fetch-bound 3D scene, quantise a texture offline to 16/256 colours, upload `GU_PSM_T4/T8` with `sceGuClutLoad`, and compare 8888/5650: VRAM, upload, GE time, pixel parity. A palette path that changes pixels is opt-in, never the default | `dropped` | VRAM bytes, upload ms, T1 GE time, capture; drop with the number if no fetch/bandwidth win | §9.4 — T8 5,825 vs 5650 5,847 µs (no GE gain), 0 px diff, VRAM 5 KB vs 8 KB: saves VRAM, not time; unswizzled 8888 151.9 ms shows swizzle is mandatory (driver does it) |
| T5 | **Clear path.** A/B `sceGuClear` (control), `GU_FAST_CLEAR_BIT` (expected null, 079 T2) and a manual 32/64-px stripe clear on 5650 and an 8888 RTT target; measure with `clear_probe` and T1 | `dropped` | 079 T2 0.265→0.268 ms; bunnymark 4.706→4.702; 0 diff; b36 removed the flag | prior numbers |
| T5b | **64-px sprite slicing.** gLib2D's `SLICE_WIDTH 64` (glib2d.c:45,428): slice large sprites into 64-px vertical strips and measure draw/bind counts and texture-cache/fetch on a large-sprite scene. Prior: 023 measured 32-64 px **blit** strips null; this is the sprite-slicing variant | `dropped` | T1 split; 613-colour gate; drop if no >0.05 ms win or if draw/bind counts grow without a GE-time benefit | §9.5 — 32 vs 16 quads at 0 px diff, GE +36 µs (no saving); T4 already ruled out the texture-cache mechanism, and this arm's absolute times are below the GE fill rate (8× draws gave 3.2× time), so direction only |
| T6 | **Static display lists and sync audit.** (a) Build a static `GU_CALL` sub-list for a `PspGeMesh` and batch kicks on a 1,000-mesh scene (cull on/off); (b) inventory every `sceGeListSync`/`sceGuTexSync`/`sceGuSync` (`egl.c:393,473,535,570,642,762`; `gl_texture.c:353,362`) and remove only a sync with a proven non-dependency | `dropped` | CPU submission and `listWaits`/`displayWaits`; 0 diff; no full wait; `arenaOverflows=0` | §9.7 — 1,000 draws vs 1 over the same vertices: CPU 283→7 µs, GE 969→461 µs ⇒ CALL lists could save ≤0.27 ms on 1,000 draws and ≈1 µs on the 4-draw showcase; sync audit clean (no `sceGuSync`, targeted waits 15–50 µs, `arenaOverflows=0`) |
| T6a | **List-local vertex memory.** Public pspsdk maps the list through the uncached alias (`sceGuStart` ORs `0x40000000`), so a `sceGuGetMemory` block needs **no `pspguWriteback`**; the block is 4-byte aligned and dies when the list is refilled. It also advances the stall address for a direct list and performs **no bounds check**. Our list is 240 KiB (`PSP_GU_LIST_BYTES`), our arena is 2×512 KiB (`PSP_GU_ARENA_BYTES`), so the arm must track `listUsedMax` and fall back rather than overrun. Test one block per list vs one per batch (gLib2D's pattern) | `dropped` | T1 split; main-RAM ledger; `listUsedMax`/overflow guard; uncached-store vs cached+writeback cost; stall count; 0 diff; no full wait | §9.6 — 2D: work 21.593→20.106 ms (−1.49 ms, 0 fallbacks) but avg unchanged (GE-bound); 3D: `used_max` pinned at 161,768 B with 50 fallbacks at 160/200/208 KiB and work +0.88 ms, because the frame reopens its list mid-frame |
| T7 | **Coherence stress.** A loader thread writes vertices/indices and a texture while the main thread submits; writeback then publish; compare captures to a single-thread control over N frames. Audit every pointer handed to `sceGuDrawArray` against the per-list arena and texture pinning | `done` | 0 corrupted frames; if a missing writeback/alias is found, fix and re-run the gates | §9.8 — 600 frames × 1,000 moving bunnies, `arenaOverflows=0`, `list_used_max=1,516`; two runs byte-identical (`5e390310…`); no code change |
| T8 | **Gates and close.** Full gate table, the pair twice, docs, commit, restore the chosen driver | `todo` | §7 all green; two runs; strict docs build | — |

Status vocabulary: `todo` / `doing` / `done` / `dropped`. IDs are stable.

## 6. Design notes

**T1 — GE time is a serial replay, not a poll.** `sceGeDrawSync(1)` reports "drawing" while a direct list is open,
so it measures the CPU's render phase. Build the same frame with `GU_SEND`, send it, then `sceGuSync` with the CPU
idle. Subtract a CPU-only build to separate list construction from GE execution. The existing `PspCountersSwap*`
pair stays the swap measure; `work = avg - swap` remains the CPU-side number until T1 lands.

**T2 — S8 is a new workload, not a re-open of 077 T5.** 077 dropped s8 because the scenes were draw-call bound and
the parity gate is exact. S8 is only meaningful when vertex fetch matters: a dense, single-draw mesh. Keep the
s16 path and the 28-byte `PspGeMesh` default; the s8 arm is opt-in and gated.

**T3 — the driver can express strips, the batcher cannot.** `gl_draw.c:1292-1293` maps the primitives, and the CPU
clip pass only touches `GU_TRIANGLES`. A strip probe must therefore supply its own indexed vertex buffer and prove
winding and edge parity. For the index cache, the constraint is the list lifetime: an app index array may be
reused before the GE runs it; only a static `PspGeMesh` may hand the pointer directly, after writeback.

**T4 — CLUT is a pipeline decision.** The runtime upload path has no CLUT (`gl_texture.c`); a palette path needs an
offline quantiser and a VRAM CLUT entry. The 017 result was a 2D glyph atlas; the 3D question is texture-fetch
traffic on the shared bus. Do not adopt a palette that changes the pixel gate; record it as an opt-in quality trade.

**T5 — fast clear was already null.** `GU_FAST_CLEAR_BIT` measured 0.265 → 0.268 ms in 079 T2. The only new arm is a
manual stripe clear, and only if the clear is a measurable share of a real frame; a clear-only probe alone is not
enough to adopt.

**T6 — CALL lists need a lifetime and a kick count.** A static sub-list is legal only when its vertices and indices
are static; matrices stay in the parent list. Measure `sceGuCallList`/stall updates on a 1,000-mesh scene, not on the
4-draw showcase. The sync audit starts from the existing targeted waits: no full GE wait, no `arenaOverflows`.

**T7 — coherence is verified, not assumed.** The driver already writes back vertices/indices/textures and keeps one
arena per rotating list. The stress probe is the evidence; only a failing frame justifies code.

**T6a — list-local vertices are uncached and capped by the list.** Public pspsdk `sceGuStart` maps the list with
`| 0x40000000` (uncached); `sceGuGetMemory` returns a 4-byte-aligned block from that alias, writes a pair of command
words before it, and advances the stall address for a direct list. Writes through the returned pointer therefore
need no `pspguWriteback`, and the block is invalid as soon as the same list is refilled — the two-list rotation
already provides that lifetime. The costs are the **240 KiB list cap** (the GE maximum is 0x3FFFF), no bounds check
in the public function, 4-byte rather than 64-byte alignment, uncached stores instead of cached stores plus a
range writeback, and one stall syscall per allocation. Measure one block per list (one stall) against one block per
batch (gLib2D's pattern, one stall per batch), and fall back to the arena when the block would not fit. The vendor
6.6.0 graphics samples use the same API widely (30 samples); its headers and code are not copied.

### T6a reference findings: `sceGuGetMemory` (pspsdk and the 6.6.0 tree)

The mechanics below come from public pspsdk source; the 6.6.0 tree was checked only for usage, not quoted.

| source | finding |
|---|---|
| `$PSPDEV` pspsdk `src/gu/sceGuGetMemory.c` | returns a **4-byte-aligned** block; rounds `size+3` down to 4; writes two command words before it; advances `gu_list->current`; in `GU_DIRECT` updates the stall with `sceGeListUpdateStallAddr`; performs **no bounds check** |
| pspsdk `src/gu/sceGuStart.c` | maps the list as `list \| 0x40000000`, the **uncached alias**, so writes to the block need **no data-cache writeback** |
| pspsdk `src/gu/sceGuFinishId.c` | FINISH/END plus a stall update; no separate cache flush for the list |
| 6.6.0 reference tree | **30 graphics samples** call the API; usage observed only, no vendor code or documentation copied |

**Why it might be a good idea.**

- It removes the 2 × 512 KiB cached vertex arenas (`PSP_GU_ARENA_BYTES`) — **1 MiB of main RAM** — when the frame
  fits in the 240 KiB list.
- No `pspguWriteback` for the vertex block: it is in the uncached list alias.
- Commands and vertices share one contiguous buffer, and the block's lifetime is the list's lifetime, which the
  two-list rotation already guarantees.

**What it costs.** The 240 KiB list cap (`PSP_GU_LIST_BYTES`; GE maximum `0x3FFFF`), no bounds check, 4-byte
alignment, uncached stores instead of cached stores plus a range writeback, and one stall syscall per allocation.
A per-batch block (gLib2D's pattern) multiplies the stall count; one reserve per list avoids that.

**T6a implementation outline (ready to code).** Reviewed against the current driver; this is what the task
needs before a device run:

- **Flags.** `PSPGU_LIST_VERTICES ?= 0` in `tools/pspgu/Makefile` and the `tools/pspgu-build.sh` passthrough;
  `PSPGU_LIST_VERTEX_BYTES ?= 163840` (160 KiB) in `pspgu_internal.h`. Default off; never a release default until
  the gates.
- **`egl.c`.** In `pspguBeginFrame`, after `sceGuStart`, reserve one block with
  `sceGuGetMemory(PSPGU_LIST_VERTEX_BYTES)` and keep `listVertexBase`/`listVertexUsed` per list. That is one skip
  and one stall per list. `pspguArenaAlloc` becomes a 4-byte bump allocator over that block; when
  `used + bytes > budget` it returns NULL and the existing arena is the fallback. Add a second arm that removes
  the 2×512 KiB arena (or shrinks it to a small fallback) to measure the 1 MiB saving.
- **Writeback.** `pspguWriteback` must no-op for pointers in `[listVertexBase, listVertexBase + budget)`; the
  cached-arena ranges keep their current writeback. The list-local block is uncached, so this is a correctness
  requirement, not an optimisation.
- **Guards.** `sceGuGetMemory` has no bounds check. The 160 KiB reserve leaves ~80 KiB for the list commands in the
  240 KiB list; `pspguEndFrame` must log `listUsedMax`, the reserve used and the remaining bytes, and a frame where
  the reserve plus commands would exceed the list must fall back before the overrun. `pspguArenaReset` remains the
  full-sync fallback and `arenaOverflows` must stay 0.
- **Arms.** (a) One reserve block per list, sub-allocated; (b) one `sceGuGetMemory` per draw (gLib2D's pattern),
  counting the extra stall updates. Both with the arena present and with the no-arena/small-fallback build.
- **Workloads.** 2D showcase 3,645 vertices (~87 KiB), bunnymark 2,375 sprites (~57 KiB), `scene_3d SAMPLE=1`, and a
  1,000-mesh scene; a synthetic frame that exceeds the 160 KiB budget must exercise the fallback.
- **Measure and gate.** T1 GE/CPU split; main-RAM ledger; `listUsedMax`; stall count; 0-diff gates (2D 613,
  scene_3d, MESH_SCENE, RTT, clip 237/720, shapes 284); no full wait; no list overflow; drop if there is no win or
  the fallback is reached on a normal frame.

### gLib2D reference (LGPLv3) — what to test and what not to copy

Source: <https://github.com/libcg/gLib2D>, `glib2d.c`. Ideas only; do not copy code, comments or inline assembly.

| gLib2D choice | disposition |
|---|---|
| `glib2d.c:375-440` — 2-vertex `GU_SPRITES`, 6-vertex `GU_TRIANGLES` for rotated; `GU_TRANSFORM_2D`, 32-bit position, `GU_TEXTURE_16BIT`, optional 8888 colour (20-byte textured vertex) | **T2b — dropped after review** — 079 T4 through mode slower, 077 T5 compact 2D, 061 s16 UV 37 px; b36 removed the path |
| `glib2d.c:406` — `sceGuGetMemory` for the batch inside the direct list | **T6a** — the strongest new idea: list-local vertices are uncached (no writeback), but bounded by the 240 KiB list with no bounds check; the 2×512 KiB arena saves 1 MiB if the frame fits |
| `glib2d.c:39-45,428` — 512 KiB single direct list, 64-px sprite slicing | **T5b** for the slicing; the single list is rejected |
| `glib2d.c:633-644` — `sceGuFinish` + `sceGuSync(0,0)` every frame | **Rejected** — our two rotating lists and targeted waits are measured better (069/076) |
| `glib2d.c:264-266` — `GU_DITHER` globally off | **Rejected** — 5650 dither parity (012) |
| `glib2d.c:1384` — `sceGuScissor(x, y, x+w, y+h)` | **Rejected, do not copy** — passes edges where width/height are required; our `gl_state.c:249` is correct |
| `glib2d.c:588-589` — `GU_PSM_8888` always, optional swizzle | **No task** — ours already supports swizzle and per-texture 16-bit formats |

### Hard rules

- **LGPLv3 reference:** ideas only; no code, comments, macros or inline assembly copied.
- **Instrument first:** T1 before any speed claim; a control and a control-again per point; one variable per run.
- **Adoption threshold:** >0.05 ms work or measured GE time; 0 pixel diff on the gate table; otherwise drop with the
  number.
- **No re-opened decisions:** S16, swizzle, 5650, targeted waits and merged clean runs are baseline; compact 2D
  and fast clear remain dropped (079/077/061/023); s8 only as T2; CLUT4/8 only as T4; blit strips 023 null.
- **No full GE wait:** `arenaOverflows=0`, `listUsedMax` within the list, no gLib2D-style per-frame `sceGuSync`.
- **Forced-clean build:** `rm -f tools/pspgu/*.o tools/pspgu/libGL.a` before every driver build; one tag (`b38`).
- **T2b/T5 are dropped:** 079 T4/T2, 077 T5 and 061 are the prior numbers; the b36 revert removed the code. Do
  not restore it without a new workload that reverses the evidence.

## 7. Gates

Every task keeps these green; a speed claim with a pixel change is a drop.

| gate | expected |
|---|---|
| `batch_check` 2D | 0 diff / 613 colours |
| `scene_3d SCENE=both` | 0 diff |
| `MESH_SCENE=1` | 0 diff |
| RTT | 0 diff |
| `clip_probe` | 237/720 pixels as recorded |
| `basic_shapes` | 284 colours |
| frame behaviour | no full GE wait; `arenaOverflows=0`; list waits reported |
| benchmark pair | two runs per point; `work` and T1 GE time; no adoption under 0.05 ms |

## 8. Exit criteria

- [ ] T1's split is reproducible and used by every adoption decision
- [ ] every active task (T1-T4, T5b, T6, T6a, T7, T8) has a measured adopt/drop with a number and a pixel result
- [x] T2b and T5 are dropped with their prior numbers; no code is restored
- [ ] adopted changes: `b38`, gate table 0, pair re-run, docs updated
- [ ] dropped changes: recorded here and in the Phase 10 table; no code left behind
- [ ] full gates and a strict docs build pass
- [ ] driver state at close is explicit (pspgu or pspGL)

## 9. Result

### 9.1 T1 — GE/CPU attribution (`done`, 2026-11-24)

**Control pair** (`p10s15-control`, pspgu `9798af84...`, `libraylib.a c2004b26...`, git `34e057b4`, two runs):

| sample | avg ms | fps | work ms | swap ms | draws |
|---|---|---|---|---|---|
| `psp_showcase` (3D) | 33.569 | 29.8 | 32.059 | 1.51 | 4 |
| `psp_2d_showcase` (2D) | 33.803 | 29.6 | 21.593 | 12.21 | 6 |

**Probe.** `psp-tests/ge_probe` (raw GU, no raylib/pspgu): 2,000 32×32 blended sprites and 400 lit indexed cubes
built in a `GU_SEND` list, enqueued with `sceGeListEnQueue` and waited with `sceGeListSync(id, PSP_GE_LIST_DONE)`
while the CPU is idle. Three defects had to be fixed before the numbers were real, and they are recorded because
each one produced a believable lie:

- the list is built with cached CPU stores; the GE reads it from RAM, so `sceGuFinish()` must be followed by
  `sceKernelDcacheWritebackRange(list, used)` before the enqueue (pspsdk `samples/gu/doublelist` does exactly this);
- `sceGeListEnQueue(list, list, ...)` sets the stall address to byte 0 and holds the GE at the start of the list;
  the second argument must be `NULL`;
- a stale `main.o`: the patched source was 22 s newer than the object, so the first "measurements" (15-17 µs) were
  the old binary. `strings main.o` is the cheap check.

| category | CPU build | GE | list bytes |
|---|---|---|---|
| clear | 8 µs | 465 µs | 64 |
| 2,000 sprites | 14 µs | 5,838 µs | 104 |
| 400 cubes (14,400 verts, one indexed draw) | 16 µs | 5,071 µs | 140 |
| frame (clear + sprites + cubes) | 36 µs | 11,215 µs | 292 |

The categories are additive: 465 + 5,838 + 5,071 = 11,374 µs against 11,215 µs for the combined frame (1.4 %). A
second run reproduced every value within 1 µs (464 / 5,838 / 5,068 / 11,214). `sceGeListDeQueue` after a completed
list returns `0x80008061`; the entry is already retired, so the return is ignored.

**Driver split.** A compile-time `PSPGU_REPLAY=1` mode (not the default, `tools/pspgu-build.sh` with
`PSPGU_REPLAY=1`) queues the demo's own frame as a `GU_SEND` list, writes it back, enqueues and waits, and logs
`REPLAY n=50 build=… ge=…` per 50-frame window. Same binaries, same scene, 400/300 frames:

| sample | CPU build | GE | vblank | avg | control work |
|---|---|---|---|---|---|
| `psp_showcase` (3D) | 30.1 ms | 13.1 ms | 6.9 ms | 50.1 | 32.06 |
| `psp_2d_showcase` (2D) | 19.5 ms | 29.2 ms | 1.3 ms | 50.5 | 21.59 |

Reading:

- **3D is CPU-bound.** The CPU spends 30.1 ms building the frame and the GE 13.1 ms drawing it. In the control the
  GE work hides under the next frame's CPU build, which is why the pipelined pair is 33.6 ms and not 43 ms:
  `max(build, GE) + swap` = 31.6 ms against 33.6 ms measured.
- **2D is GE-bound.** 29.2 ms of GE against 19.5 ms of CPU; the control average 33.8 ms is close to the GE time
  alone. A 2D CPU-side change can only recover the part of 19.5 ms that does not overlap.
- `swap` absorbs the GE wait in replay mode because `pspguEndFrame()` runs inside the wrapped `eglSwapBuffers`
  (egl.c:792): 3D reports 19.8 ms = 13.1 GE + 6.7 vblank, 2D 30.5 = 29.2 + 1.3. `work = avg - swap` therefore lands
  on the CPU build, which is the independent check that the two measurements agree.

Consequence for the sprint: T2/T3/T6a act on the 3D CPU side (the 30 ms), T4/T5b on the 2D GE side (the 29 ms).
A lever on the short side of a 13/30 (3D) or 19/29 (2D) split cannot reach the 0.05 ms adoption bar.

### 9.2 T2 — S8 positions (`dropped`, 2026-11-24)

Workload: 12,000 triangles (36,000 vertices) tiling the screen once, unlit, no depth test, no culling, one draw, in
the same `GU_SEND` replay as T1. Both arms use one model matrix (scale 7×4, translate z=−10) and store the same
geometry in [−1,1]: `x*32767` as s16 and `x*127` as s8, which the GE normalises back to that range.

| arm | GE | CPU build | vertex |
|---|---|---|---|
| s16 (`GU_VERTEX_16BIT`) | 1,852 µs | 12 µs | 12 B |
| s8 (`GU_VERTEX_8BIT`) | 1,681 µs | 12 µs | 8 B |

The vertex-bound GE time drops **0.171 ms (9.2 %)**, the first measured GE-side win in the sprint. It is dropped on
the parity gate: **59,538 of 130,560 pixels differ** (58,968 non-black in the s16 frame). The step is about
`screen_width/128` ≈ 1.8 px for a mesh that spans the screen, and no scale matrix changes that — 8 bits over the
visible extent is what sets it. A control draw of the 400-cube scene produced 22,644 non-black pixels through the
same readback, so the capture path was proven before the mesh numbers were believed; the first attempt measured a
black frame and was discarded.

Two earlier versions of the arms were wrong and are worth recording:

- `sceGumLoadMatrix` only updates pspgum's CPU-side stack; the matrices reach the GE through `sceGumDrawArray`.
  Used with `sceGuDrawArray`, both arms drew nothing (`nonblack=0`) and still reported a plausible 1.8 ms.
- `ScePspFMatrix4` has no `.m[]` in pspsdk; the basis columns are `.x/.y/.z/.w` and the translation lives in `.w`.

Why it is also the wrong trade here: the app holds float vertices, so packing to s8 costs CPU per vertex every frame,
and T1 puts the 3D frame's CPU at 30.1 ms against 13.1 ms of GE — the saving lands on the short side. A content
pipeline could bake s8 once offline for small props (a 100-px-wide model quantises to ≈0.78 px), but that is an
asset decision with its own pixel gate, not a renderer default.

### 9.3 T3 — Strips and index copy (`done`, 2026-11-24)

**(a) Strips.** One quad grid (100×60 cells) drawn two ways from the same vertex values: a triangle list (6 vertices
per quad, 36,000 vertices, one draw) and per-row `GU_TRIANGLE_STRIP` (2*(COLS+1) per row, 12,120 vertices, 60
draws). Both arms use the T2 model matrix.

| arm | vertices | draws | GE | CPU build | list bytes |
|---|---|---|---|---|---|
| `GU_TRIANGLES` | 36,000 | 1 | 1,850 µs | 14 µs | 100 |
| `GU_TRIANGLE_STRIP` | 12,120 | 60 | 777 µs | 60 µs | 1,044 |

**GE −1.073 ms (−58 %) at 0 differing pixels of 130,560** (120,520 non-black in both). The strip costs 46 µs of
extra CPU build for the 59 additional draws, so the trade is strongly positive on a vertex-bound mesh.

Not adopted as a default: the driver receives triangle lists from rlgl, so a runtime strip path would have to
stripify per frame and pay CPU on the 30.1 ms side to save GE on the 13.1 ms side. The measured win belongs to
static asset meshes, baked offline — the same place T2's s8 could be baked. Recorded as such, not handed to the
default renderer.

**(b) Index copy.** The driver copies each run of indices before a draw (gl_draw.c:749-760), because the list may
outlive the caller's array. Measured directly on the device:

| copy | 36,000 indices |
|---|---|
| element loop (`rb[i] = idx[i]`) | 1,128 µs |
| `memcpy` | 1,129 µs |

≈**31 µs per 1,000 indices**, i.e. 1.13 ms for a 36k run. The 3D showcase submits 14,400 indices per frame in two
merged runs, so the copy is ≈**0.45 ms/frame** — 1.5 % of the 30.1 ms CPU, above the 0.05 ms bar and on the side
that binds. `memcpy` being no faster than the loop says the cost is the memory traffic, not the loop shape, so the
only real fix is to stop copying: cache an index run while the merged run is unchanged.

That cache must survive two lists in flight and invalidate when the merge changes, which is a driver change with
its own lifetime proof, not a measurement. It goes to §10 as a follow-up sprint rather than into this one.

### 9.4 T4 — CLUT8 (`dropped`, 2026-11-24)

T1 puts the GE-bound path in 2D (29.2 ms GE against 19.5 ms CPU), so the texture format was tested on the sprite
scene rather than on a 3D mesh: 2,000 blended 32×32 sprites from one 64×64 image uploaded three ways. Every colour
is exactly representable in 5650, 8888 and a 256-entry 8888 palette, so any pixel difference is a decode error.

| texture | bytes/texel | VRAM | GE |
|---|---|---|---|
| 5650 | 2 | 8 KB | 5,847 µs |
| CLUT8 (`GU_PSM_T8` + `sceGuClutLoad(8, …)`) | 1 | 4 KB + 1 KB palette | 5,825 µs |
| 8888, **unswizzled** | 4 | 16 KB | 151,903 µs |

**No GE-time win.** Halving the texel size moved the sprite scene by 22 µs, inside run-to-run noise: the 2D GE time
is fill and blend bound, not texture-bandwidth bound. That is the same conclusion T2 reached from the other side.
Dropped on the sprint's rule (no fetch/bandwidth win), with exact parity — 0 differing pixels of 130,560 for both
comparisons, 124,380 non-black.

**CLUT8 is still a VRAM win**: 5 KB against 8 KB (5650) and 16 KB (8888) at 0 px diff. It belongs with T2/T3a as an
offline asset option, never a default.

**The 8888 arm is the useful accident.** Unswizzled 8888 is **26× slower** (151.9 ms): the texture cache reads a
line per texel. The pspgu driver swizzles by default (gl_texture.c:282, 4 bytes per texel counted in `bpp`), so this
is not a renderer defect — but any new upload path that forgets `GU_TRUE` in `sceGuTexMode` will look like a hang
rather than a slow frame.

Consequence for T5b: sprite slicing targets the texture cache, and this workload shows no texture-bandwidth
sensitivity at all. Expect it to be null; measure it cheaply rather than building it.

### 9.5 T5b — 64-px sprite slicing (`dropped`, 2026-11-24)

16 large sprites (120×68, tiling the screen) drawn whole and sliced into 64-px vertical strips with a matching UV
split, so the pixels are the same by construction:

| arm | quads | GE | parity |
|---|---|---|---|
| whole | 16 | 47 µs | — |
| sliced | 32 | 83 µs | 0 differing pixels of 130,560 (126,848 non-black) |

Slicing **doubles the primitive count and adds 36 µs**; it never wins here. The sprint's drop condition is met twice
over: no >0.05 ms win, and the draw count grows without a GE-time benefit.

**The absolute numbers are not trustworthy and the record says so.** The instrument check — the same whole-sprite
draw 8× in one list — returned 150 µs, only 3.2× the single draw, and 47 µs for 130,560 blended pixels is above the
GE's plausible fill rate (2 pixels/cycle at 222 MHz puts 130k pixels near 290 µs). So the arm measures
per-primitive processing more than fill, and it is used for direction only.

The decisive evidence is T4: halving the texel size (8888 → CLUT8) moved the sprite scene by 22 µs. The texture
cache is not the constraint, so a change whose only mechanism is texture-cache locality has nothing to recover;
this arm's +36 µs is consistent with that. Recorded as a drop; no code changed.

**Instrument lesson for the next probe:** a list whose GE time does not scale with the work in it is not a timing of
that work. Repeat the workload in the same list and require the ratio before recording a magnitude.

### 9.6 T6a — list-local vertices (`dropped`, 2026-11-24)

`PSPGU_LIST_VERTICES=1` reserves one `sceGuGetMemory` block per list and sub-allocates 4-byte aligned from it;
`pspguArenaAlloc` falls back to the arena when the block is full, and `pspguWriteback` no-ops inside the block
(the block is in the list's uncached alias, so a writeback is both redundant and wrong). Two workloads, two answers.

**2D showcase (fits, no reopen): a real CPU win.**

| | work | avg | swap | block used | fallbacks |
|---|---|---|---|---|---|
| control | 21.593 ms | 33.803 | 12.21 | — | — |
| T6a, 160 KiB | **20.106 ms** | 33.800 | 13.70 | 87,456 B | 0 |

**−1.49 ms of list-build CPU (−6.9 %)**, `arenaOverflows=0`, no list overflow. The frame time does not move because
2D is GE-bound at 29.2 ms (T1) — the CPU simply waits longer at the swap. That also settles the open question in the
design note: **uncached list-local stores are faster than cached stores plus a range writeback**, by about the cost
of the writeback itself.

**3D showcase (the CPU-bound workload): cannot use it.** `used_max` stopped at exactly **161,768 bytes with 50
fallbacks** at 160, 200 and 208 KiB budgets, and work rose to 32.94 ms against the 32.06 control (+0.88 ms). A
budget that does not change the ceiling is not the limiter: the 3D frame **closes and reopens its list mid-frame**
(`pspguFinishAndWait`, gl_texture.c:408, reached from the texture-eviction path), so a fixed per-list reserve is
asked for twice on the same buffer. The public `sceGuGetMemory` has no bounds check, so this is a design limit, not
a tuning value.

Dropped as a default, for three reasons and with no code changed:

- it cannot serve the only workload where the CPU is the wall;
- the workload it does help is GE-bound, so it buys nothing measurable in the pair;
- a correct version needs a reserve strategy that survives a mid-frame list reopen and a per-list budget that
  adapts to the frame, which is a separate design, not this measurement.

What survives is the useful fact for any future attempt: the uncached block is worth ~1.5 ms per frame on a 2D-class
frame, and it removes the writeback but not the arena fallback. The flag, the allocator and the no-op writeback stay
in the driver compiled out (`PSPGU_LIST_VERTICES=0`); the installed archive is byte-identical to the control
(`9798af84e81b269e`).

### 9.7 T6 — static CALL lists and the sync audit (`dropped`, 2026-11-24)

**(a) CALL lists — measured as the cost they could remove.** pspsdk exports `sceGuCallList` but no `sceGuReturn`, and
no sample in `$PSPDEV/psp/sdk/samples/` builds a callable sub-list, so a sub-list would need a hand-written RET
opcode. Rather than record a number from an unverified construct, the probe measures the thing a CALL list replaces:
emitting N draw commands over static vertices.

| arm | draws | CPU build | GE | list bytes |
|---|---|---|---|---|
| 1,000 separate draws | 1,000 | 283 µs | 969 µs | 16,032 |
| one draw, same 6,000 vertices | 1 | 7 µs | 461 µs | 48 |

- **Command emission: 276 µs per 999 draws ≈ 0.28 µs/draw** — the absolute upper bound on what CALL lists can save,
  since a CALL still has to be emitted.
- **GE time also grows 508 µs** for the same vertices. A CALL list does not remove that: the GE still processes
  1,000 primitives inside the sub-lists. So even a perfect CALL implementation cannot recover it.
- The 3D showcase submits **4 draws per frame** after the P10-S11 merge. At 0.28 µs/draw the ceiling is ≈1 µs.

Dropped: the merge path has already collapsed 400 cubes into 2 draws, so there are no per-mesh command streams left
to cache. A renderer that submits 1,000 separate meshes could save ≈0.27 ms, and that is the number to remember if
the batcher is ever changed to stop merging.

**(b) Sync audit — clean, and the evidence is the counters.** No `sceGuSync` or `sceGeDrawSync` exists anywhere in the
driver; every wait is `sceGeListSync` on a specific queue id:

| wait | when | measured |
|---|---|---|
| begin-frame, list two frames old | steady state | 15–31 µs avg, 140 µs max |
| display, the frame being shown | at swap | 46–50 µs avg |
| `pspguFinishAndWait` | boot, readback, texture eviction only | not on the steady path |

`arenaOverflows=0` in every run of this sprint, `list_used_max` 2.4–4.7 KiB against a 240 KiB list, and no list
overflow line. The audit passes: targeted waits only, nothing waits on the frame it just submitted.

### 9.8 T7 — coherence stress (`done`, 2026-11-24)

The hazard is the CPU running ahead of the GE: a vertex buffer rewritten while the GE still reads it, or a writeback
that publishes the pointer before the data. The driver's answer is four rules; the audit below cites each, and the
stress run is the evidence that they hold under a moving workload.

| rule | where |
|---|---|
| write back the arena run before the draw that references it | `gl_draw.c:738,758,792-793,843,938,1460,1586,1615-1616` |
| one vertex arena per rotating list; reset only that list's cursor | `egl.c:502-503` |
| reuse a list only after the GE finished the frame that last used it | `egl.c:491-494` (wait on `listQid[next]`) |
| texture churn: finish and wait before evicting VRAM the GE may read | `gl_texture.c:408` |

**Stress run.** `bunnymark` with `FIXED_SEED`, 1,000 bunnies, 600 frames — geometry rebuilt and re-written every
frame, so every rotation is exercised:

| measure | value |
|---|---|
| frames | 600, median 16.710 ms (60 fps), one 46.8 ms outlier at startup |
| `arenaOverflows` | 0 |
| `list_used_max` | 1,516 B |
| list waits | 22 µs avg, 24 µs max |

**Determinism check.** The demo freezes its last two frames under `FIXED_SEED`, so a capture at DONE is comparable.
Two independent runs (reset between) produced **byte-identical 391,734-byte captures**, md5 `5e390310d911b3cd…`.
A missing writeback or a list reused too early would show as a run-to-run difference, because the rotation timing is
not deterministic even when the frame content is.

Verdict: verified, **no code change**. The only anomaly is the single 46.8 ms first-frame outlier, which is texture
upload at first use, not coherence — it appears once and never returns in 600 frames.

## 10. Follow-ups

- If T1 works, add `GE ms` to `tools/psp-bench.sh` output so future sprints do not re-derive it.
- **Index-run cache** (from T3b): 31 µs per 1,000 indices, ≈0.45 ms/frame in the 3D showcase. A per-mesh cache that
  invalidates when the merged run changes is the only way to remove it; needs a two-lists-in-flight lifetime proof.
- Offline asset baking (from T2/T3a): s8 positions for small props and strips for static meshes both win on the GE
  with no format change to the runtime path. One pipeline, two wins, its own pixel gate.
- If T3's index cache wins, revisit the 066 deferred index-buffer note.
- If T4 finds a texture-fetch wall, an offline texture pipeline (quantise + CLUT) becomes its own sprint.
- If T2/T3/T4 lose, record the bound: the renderer is draw-call/batcher bound, not vertex/texture-fetch bound.
- T2b/T5 are dropped; re-open only if a new workload reverses 079 T4/T2, 077 T5 and 061. T1 is the first task
  and needs the new raw-GU probe.

## 11. References

- `docs/psp/recipes/benchmarking.md` and `tools/psp-bench.sh` — the pair, work/swap, JSON fields
- sprints 017 (CLUT4), 023 (cache strips), 061 (s16), 066 (index deferral), 069/076 (targeted waits),
  077 (compact formats), 078 (merged clean runs), 079 (PGE/fast clear), 080/081 (state and reference checks)
- public pspsdk `src/gu/sceGuGetMemory.c`, `sceGuStart.c`, `sceGuFinishId.c` — list-local memory, the uncached
  list alias, the stall update and the missing bounds check
- `tools/pspgu/gl_draw.c` (`guPrimitive`, `guVertexFormat`, index paths), `gl_texture.c` (no CLUT),
  `gl_state.c` (clear), `egl.c` (list waits/arena), `gl_matrix.c` (s16/s8 scale rule)
- gLib2D — <https://github.com/libcg/gLib2D>, `glib2d.c` lines 39-45, 153, 241, 264-266, 375-440, 588-589, 633-644,
  1384. LGPLv3; ideas only, no code copied.
- public: PPSSPP GE documentation (vertex pipeline, display lists, raster rules), pspsdk `samples/gu` and
  `pspgu.h`. No vendor SDK documentation or code is used.
