# Sprint 085 (P10-S15) — Hardware optimisation sweep: formats, submission, clear, coherence and GE/CPU attribution

| | |
|---|---|
| Roadmap phase | [Phase 10 — 3D and advanced](../phases/10-3d-advanced/index.md) |
| Sprint | P10-S15 |
| Branch / base commit | `master` @ `27391da3` (pspgu `b37`; P10-S13 T1; docs commits) |
| Started / closed | 2026-11-23 (written) / — |
| Build number(s) | pspgu `b38` for any adopted driver change; installed `libraylib.a` `c2004b26...` (`PSP_BATCH=1 PSP_MERGE_DRAWS=1`, P10-S13 T1); new probes opt-in |
| Tasks | 0/11 done — T2b/T5 blocked on b36-removed code; T1 probe to add |

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
| Framebuffer | 32/64-px clear stripes | `GU_FAST_CLEAR_BIT` measured **null** in 079 T2 (0.265 → 0.268 ms); manual stripe clear untested | T5, low prior |
| Submission | batched display lists | merged clean runs adopted in 078 (N=500 43.22 → 26.67 ms); static `GU_CALL` sub-lists untested | T6 |
| Submission | fewer GE sync points | targeted two-lists-in-flight waits, 13-19 µs, no full wait (069/076); all `sceGeListSync`/`TexSync` sites not audited as a set | T6 |
| Coherence | writeback and buffer lifetime | per-list 64-byte-aligned arenas, writeback before every GE pointer, texture pinning across two frames | T7 verification; no feature unless a bug is found |
| Profiling | GE and CPU time separately | `work = avg - swap`; no GE time; `sceGeDrawSync(1)` is explicitly not a GE timer (see the guide) | **T1 first** |

A second reference, **gLib2D** (LGPLv3), contributes three implementation choices not in the list above: its
2D through-mode 16-bit-UV vertex format (T2b), list-local vertex memory via `sceGuGetMemory` (T6a), and 64-px
sprite slicing (T5b). Its single-list full-sync model, global dither-off and scissor call are explicitly rejected
in §6; only ideas are used, no code.

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
texture; clear-path variants; static `GU_CALL` lists and a sync audit; a coherence stress probe; gates and close.

**Out:** GUM, per-sprite submission, 32-64 px **blit** strips (023 null), compact 5551 colour, removing the CPU clip,
the Media Engine, audio, streaming, and any change to the 2D `batch_check` pixel contract.

## 4. Preconditions

- [x] pspgu `b37` installed; the installed `libraylib.a` is `c2004b26...` (P10-S13 T1); the b37 pair is historical
- [x] main tree clean except `ai-memory.db-shm`; no isolated worktree required
- [x] PSPLink is available for this session; coordinate if another session owns it
- [ ] the fresh control pair on `c2004b26...` is recorded before T1 is compared
- [ ] the T1 probe is produced by T1; it is not a precondition

## 4a. Readiness review (2026-11-24)

Reviewed against `master @ 27391da3`, pspgu `b37` and `libraylib.a c2004b26...` (P10-S13 T1). The sprint is a
measurement plan, not a set of flag flips. Verdict: **T1, T2, T3, T4, T6, T6a and T7 are startable; T2b and T5 are
blocked** until code removed by the b36 revert is restored; the baseline is stale and must be re-recorded.

**Baseline.** The §0 pair (33.677 / 21.578 ms) predates P10-S13 T1. Re-run the pair on `c2004b26...` and use it as
the control. The P10-S13 gates (batch 613, scene_3d 0, shapes 284) are the current pixel control.

**T1 — no probe exists.** `matrix_probe` has api/calls/gum/scene, all inside the driver's direct lists; there is no
GE/SEND mode. A `GU_SEND` list cannot be opened while a `GU_DIRECT` list is open. Add a standalone raw-GU
`psp-tests/ge_probe`: init GU, build the representative frame in a `GU_SEND` list (the 400-cube merged scene and a
2,000-sprite batch), `sceGuSendList` + `sceGuSync(GU_SYNC_FINISH, GU_SYNC_WHAT_DONE)` with the CPU idle, and time
the sync; build the same list CPU-only to subtract. Cut by draw category. Keep its vertex/index buffers separate
from the driver's direct lists; report the GE time beside the direct-list work/swap.

**T2b — code removed.** The b36 revert removed `PSPGU_FAST2D`, `pspguCanUseTransform2D` and
`pspguBoundTexture2D`; there is no through-mode path to toggle. T2b requires reintroducing a scoped
`GU_TRANSFORM_2D` path (a new flag) with the 16-bit-UV vertex, the texel-UV/scale conversion and the
2-vertex/6-vertex split. The prior 079 T4 was slower with 32-bit UV; 061's s16-UV cost 37 px. Restore or drop
before coding.

**T5 — fast-clear code removed.** `PSPGU_FAST_CLEAR`/`GU_FAST_CLEAR_BIT` is not in the b37 driver; only the
`pspgu.h` define remains. The control arm needs a raw-GU probe or the flag restored; the manual 32/64-px stripe
clear is new code. 079 T2's null is the prior. Restore or drop before coding.

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

**Order.** T1 first; T2/T3/T6a next; T4/T6/T7; T2b/T5 only after the restoration decision. The gate table and the
adoption threshold are unchanged.

## 5. Tasks

| ID | task | status | verification | evidence |
|---|---|---|---|---|
| T1 | **GE/CPU attribution.** Add a serial-replay probe: build one frame with `sceGuStart(GU_SEND, list)`, `sceGuSendList` and `sceGuSync(GU_SYNC_FINISH, GU_SYNC_WHAT_DONE)` while the CPU is idle; report GE ms, CPU render ms, swap ms, and the existing per-phase counters (clip, cull, submit). Do not use `sceGeDrawSync(1)` as a GE timer | `todo` | GE + CPU + swap reconstructs the fresh control pair within the harness repeatability; two runs agree; no game change | — |
| T2 | **S8 positions.** Add an opt-in `GU_VERTEX_8BIT` packed path (scale 128 about the mesh centre, same scale-matrix rule as s16). Workload: a dense mesh (≥10k vertices) drawn once per frame, not the 400-cube draw-call-bound scene. A/B S16 vs S8 | `todo` | T1 GE time and work; pixel parity on a dedicated mesh capture; adopt only if it beats the control and 0 diff | — |
| T2b | **2D through-mode 16-bit UV.** gLib2D's 20-byte textured vertex: `GU_TRANSFORM_2D` + `GU_TEXTURE_16BIT`, 32-bit position, 2-vertex `GU_SPRITES` for unrotated and 6-vertex triangles for rotated. Workload: 2,000+ sprites / `psp_2d_showcase`, where vertex copy/fetch is a measurable share. Prior: 079 T4 through mode with 32-bit UV was slower; 061's s16-UV trial cost 37 px; 077 T5 dropped compact 2D. Only the 16-bit-UV variant is tested | `blocked` | code restoration first; then T1 split / 613-colour gate; drop if slower or any pixel moves | — |
| T3 | **Strips and index cache.** (a) Stripify a dense mesh and draw `GU_TRIANGLE_STRIP` via a probe or a `PspGeMesh` strip path; compare vertex/index count, GE time and pixels with `GU_TRIANGLES`. (b) For static `PspGeMesh` indices, copy the index run once per mesh/list (or hand the app pointer after a writeback) instead of per draw; measure the CPU copy | `todo` | T1 split; 0 diff; per-list lifetime proof; no `arenaOverflows` | — |
| T4 | **CLUT4/CLUT8, 3D only.** If T1 shows a texture-fetch-bound 3D scene, quantise a texture offline to 16/256 colours, upload `GU_PSM_T4/T8` with `sceGuClutLoad`, and compare 8888/5650: VRAM, upload, GE time, pixel parity. A palette path that changes pixels is opt-in, never the default | `todo` | VRAM bytes, upload ms, T1 GE time, capture; drop with the number if no fetch/bandwidth win | — |
| T5 | **Clear path.** A/B `sceGuClear` (control), `GU_FAST_CLEAR_BIT` (expected null, 079 T2) and a manual 32/64-px stripe clear on 5650 and an 8888 RTT target; measure with `clear_probe` and T1 | `blocked` | code restoration first; then clear-only work / 0 diff; drop if < 0.05 ms or no GE-time change | — |
| T5b | **64-px sprite slicing.** gLib2D's `SLICE_WIDTH 64` (glib2d.c:45,428): slice large sprites into 64-px vertical strips and measure draw/bind counts and texture-cache/fetch on a large-sprite scene. Prior: 023 measured 32-64 px **blit** strips null; this is the sprite-slicing variant | `todo` | T1 split; 613-colour gate; drop if no >0.05 ms win or if draw/bind counts grow without a GE-time benefit | — |
| T6 | **Static display lists and sync audit.** (a) Build a static `GU_CALL` sub-list for a `PspGeMesh` and batch kicks on a 1,000-mesh scene (cull on/off); (b) inventory every `sceGeListSync`/`sceGuTexSync`/`sceGuSync` (`egl.c:393,473,535,570,642,762`; `gl_texture.c:353,362`) and remove only a sync with a proven non-dependency | `todo` | CPU submission and `listWaits`/`displayWaits`; 0 diff; no full wait; `arenaOverflows=0` | — |
| T6a | **List-local vertex memory.** Public pspsdk maps the list through the uncached alias (`sceGuStart` ORs `0x40000000`), so a `sceGuGetMemory` block needs **no `pspguWriteback`**; the block is 4-byte aligned and dies when the list is refilled. It also advances the stall address for a direct list and performs **no bounds check**. Our list is 240 KiB (`PSP_GU_LIST_BYTES`), our arena is 2×512 KiB (`PSP_GU_ARENA_BYTES`), so the arm must track `listUsedMax` and fall back rather than overrun. Test one block per list vs one per batch (gLib2D's pattern) | `todo` | T1 split; main-RAM ledger; `listUsedMax`/overflow guard; uncached-store vs cached+writeback cost; stall count; 0 diff; no full wait | — |
| T7 | **Coherence stress.** A loader thread writes vertices/indices and a texture while the main thread submits; writeback then publish; compare captures to a single-thread control over N frames. Audit every pointer handed to `sceGuDrawArray` against the per-list arena and texture pinning | `todo` | 0 corrupted frames; if a missing writeback/alias is found, fix and re-run the gates | — |
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
| `glib2d.c:375-440` — 2-vertex `GU_SPRITES`, 6-vertex `GU_TRIANGLES` for rotated; `GU_TRANSFORM_2D`, 32-bit position, `GU_TEXTURE_16BIT`, optional 8888 colour (20-byte textured vertex) | **T2b** — the only new ingredient is the 16-bit UV in through mode; the through-mode and compact-format priors are already negative |
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
- **No re-opened decisions:** S16, swizzle, 5650, targeted waits and merged clean runs are baseline; compact 2D/s8
  only as T2b/T2 on a new workload; CLUT4/8 only as T4; blit strips remain 023's null.
- **No full GE wait:** `arenaOverflows=0`, `listUsedMax` within the list, no gLib2D-style per-frame `sceGuSync`.
- **Forced-clean build:** `rm -f tools/pspgu/*.o tools/pspgu/libGL.a` before every driver build; one tag (`b38`).
- **T2b/T5 are blocked:** the b36 revert removed `PSPGU_FAST2D`/the through-mode helpers and `PSPGU_FAST_CLEAR`;
  restore that code behind a new flag or drop the task. Do not assume the old flag exists.

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
- [ ] every task and sub-task (T2-T7, T2b, T5b, T6a) has a measured adopt/drop with a number and a pixel result
- [ ] T2b and T5 are restored or explicitly dropped before close
- [ ] adopted changes: `b38`, gate table 0, pair re-run, docs updated
- [ ] dropped changes: recorded here and in the Phase 10 table; no code left behind
- [ ] full gates and a strict docs build pass
- [ ] driver state at close is explicit (pspgu or pspGL)

## 9. Result

Pending — no device runs yet.

## 10. Follow-ups

- If T1 works, add `GE ms` to `tools/psp-bench.sh` output so future sprints do not re-derive it.
- If T3's index cache wins, revisit the 066 deferred index-buffer note.
- If T4 finds a texture-fetch wall, an offline texture pipeline (quantise + CLUT) becomes its own sprint.
- If T2/T3/T4 lose, record the bound: the renderer is draw-call/batcher bound, not vertex/texture-fetch bound.
- T2b/T5: restore the b36-removed through-mode/fast-clear code or drop them before coding; T1 is the first task
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
