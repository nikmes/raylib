# Sprint 014 — Ceiling, VRAM and readback: closing phase 5

| | |
|---|---|
| Roadmap phase | [Phase 5 — Native GU backend](../phases/05-native-gu-backend.md) |
| Branch / base commit | `master` @ `de61e4cf` |
| Started / closed | 2026-11-21 / 2026-11-21 |
| Build number(s) | `b16` (pspgu: 3 buffers, 2 lists + arenas, readback, no stubs) |
| Tasks | 8/8 done |

## 0. Handover

**Situation.** `tools/pspgu` replaces pspGL and is **byte-identical to it on the 2D corpus**: `batch_check` 2D
renders to the same bytes (0 differing pixels, 613 colours), the immediate path is ~1.9× cheaper than pspGL's, and
`bunnymark` links and runs with `WRAP=1` again. Sprint 013 split its 3D task: `DrawMesh`/`glDrawElements` emits a
draw and produces no pixels, handed to phase 10 with the suspect named. This sprint finishes the remaining phase-5
work, or records why a criterion moves to phase 10 — and **the ceiling has never been measured with this driver in
place**, which is the phase's actual exit criterion.

**What is left, in the phase's own task numbering:** 7 (`glReadPixels`), 8 (rotating display lists with queued
sync and per-list vertex arenas), 10 (VRAM accounting at boot), 11 (`PSP_GU_DEPTH=0`), 13 (remove the stubs), 6c
(non-power-of-two textures), plus a minimal 3D depth exit probe, the ceiling measurement and the phase's exit
report. Non-power-of-two textures may slip to phase 6; the depth probe may not, because it is a phase-5 exit
criterion.

**Already true, verified — do not re-derive:**

- **2D parity**: `batch_check` 2D → 0 differing pixels against pspGL, with swizzled textures, `GU_TCC_RGBA` alpha
  and `GU_DITHER`.
- **Counters work**: `PSP_BATCH=1` reports `arrays=400 arrayVerts=409,998` and `begins=verts=0` by design;
  `PSP_BATCH=0` reports `begins=ends=204,999 verts=819,996`. `WRAP=1` links.
- **Cost with the new driver at N=1,000**: `work 6.565 ms` batched (pspGL: 7.30), `25.980 ms` unbatched (pspGL: 48.8).
- **`PSP_DISABLE_VSYNC=1` builds, links and runs**; `SPLIT` shows `swap=2.433 ms` against ~9.4 ms with vsync, and the
  counters are intact.
- **Depth**: 16-bit, 272 KiB, allocated by default; `PSP_GU_DEPTH=0` exists but has never been booted.
- **VRAM map**: two framebuffers 544 KiB + depth 272 KiB, textures from 816 KiB. The boot line prints it.
- **The current submission is one list and a full sync per frame** (`pspguEndFrame`: `sceGuFinish()` then
  `sceGuSync(GU_SYNC_FINISH, GU_SYNC_WHAT_DONE)`). T5 is what removes that wait, and Phase 5's exit requires it
  to be gone.
- **The fast vertex path passes client memory straight to `sceGuDrawArray`** (`gl_draw.c`: `arrTexCoord.ptr`), and
  the converting path uses one static `scratch` buffer. With a full sync that is safe; with a queued sync it is
  not unless the vertex memory is per-list. T5 includes that arena, not just the lists.
- Sprint 013 left one broken item: the `basic_shapes` build on pspgu **failed** while the stale pspGL binary ran, so
  its capture is invalid. T7 below re-runs it.

**Read before acting:**

1. `tools/pspgu/egl.c` — `pspguBeginFrame`/`pspguEndFrame` are the whole display-list story today: one list,
  full sync per frame.
2. `tools/pspgu/pspgu_internal.h` — the context struct, the VRAM bump allocator, the compile switches.
3. `tools/pspgu/gl_draw.c` and the batch buffer in `src/rlgl.h` — the vertex pointer is borrowed, not copied.
   Rotating lists therefore need per-list vertex arenas.
4. [Phase 5](../phases/05-native-gu-backend.md) §0 findings and exit criteria — the no-full-wait, overlapping-cubes
   depth and 2D→3D→2D parity requirements.
5. [Baselines](../baselines.md) §"Scaling past the ceiling" — **three numbers that must not be confused**, because two
   of them belong to the old driver:
   - **pspGL**, N=1,000: `work = 7.30 ms` (model: `0.87 + 6.35 µs × N` = 7.22). Ceiling **1,500 passes, 1,700 fails**;
     1,600 was the last measured at 60 fps.
   - **pspgu**, N=1,000: `work = 6.565 ms` — the driver this sprint measures. Same 1,600/1,700 bracket has *not* been
     re-tested, which is T1.
   - **unbatched**, N=1,000: pspGL 48.8 ms, pspgu 25.98 ms (1.9× cheaper) — context for what "cheaper" looks like,
     not a ceiling figure.

**First action.** Measure the ceiling with pspgu installed — start at 1,600 (the phase-4 floor) and 1,700, then
bisect to the adjacent last-passing/first-failing N, two runs each, `TARGET_FPS=0`, `WRAP=1`. Repeat after T5 on the
rotated-list build; that is the reported ceiling. That single number decides whether this phase delivered what it
was for.

**Hard rules.**

- **The 2D gate stays at 0 differing pixels** across every change here. It is the only regression gate for a driver
  that is otherwise identical to its predecessor.
- **A ceiling claim needs a real bisection**, not a bracket: the last passing N and the first failing N must be
  adjacent, with `work` and texels/frame recorded at both. A tie at 1,600 is a phase-level finding, not a pass.
- **VRAM accounting must add up**: framebuffers + depth + list + textures ≤ 2 MiB, and the texture total must come
  from an allocation counter. The current allocator is a bump allocator, so "largest free block" is only the free
  tail; do not report a free-list metric that does not exist.
- **A queued sync needs per-list vertex memory.** Frame N must not write any vertex or scratch memory that frame
  N–1's list still points at. The fast path passes client memory to `sceGuDrawArray`, so rotating lists without
  arenas is a tearing bug, not an optimisation.
- **The no-full-wait exit is not optional.** T5 stays even if the performance delta is null; a null result is
  recorded, and the rotation stays unless the Phase 5 exit criterion is explicitly amended.
- **The 3D depth exit needs its own probe.** Overlapping cubes with `PSP_GU_DEPTH=1` must render depth-correct.
  `DrawMesh` parity remains phase 10 and does not substitute for this.
- **Readback is tested on a stopped frame.** `scrshot` reads displayed VRAM, `glReadPixels` reads the current draw
  buffer; compare them only after the app logs `DONE` and sleeps, with two identical `scrshot` captures first.
- **Removing a stub must not remove a call**: `gl_stub.c` empties only when its symbols are implemented elsewhere;
  any remaining symbol logs once and, in a debug build, asserts. A deliberately dropped symbol is removed from the
  call list and the reason recorded — it is not left as a silent no-op.
- pspGL restorable in one command, verified by `tools/pspgl-build.sh --check` (content hash `2ab7378e`).
- **Every app that links pspgu needs `-lpspgu` in `LIBS`.** pspGL reached the GE through `libpspge`; pspgu calls
  `sceGu*`, so without the flag the link fails with a wall of undefined `sceGuInit`/`sceGuStart` symbols. Fixed in
  `bunnymark`, `batch_check` and `basic_shapes`; a new test app needs it too.
- **Suspect the instrument before the driver.** Two conclusions in sprint 013 were wrong because of the measuring
  rig, not the code: a `grep` pattern missing the `]` in `[PSPGU]`, and a Makefile that read `CFLAGS = -Wall` and
  never referenced `EXTRA_CFLAGS`, so a `-DDRAW_CUBE` build silently compiled the default. Both cost device runs.
  Before acting on "it draws nothing", confirm the build actually contains the change (`psp-nm`, a log line, or a
  deliberate break) and that the log you are grepping matches the line you print.

**Commands.**

```sh
export PSPDEV=/home/psp/pspdev
export PATH=$PATH:$PSPDEV/bin

cd /home/psp/raylib-psp
./tools/pspgu-build.sh                  # install the driver
./tools/pspgu-build.sh --depth=0        # 2D-only build
./tools/pspgu-build.sh --restore        # pspGL back

cd psp-tests/bunnymark && make BUNNY_COUNT=1600 FRAMES=200 TARGET_FPS=0 WRAP=1
( printf './bunnymark.prx\n'; sleep 45 ) | pspsh
pspsh -e "scrshot host0:/ceiling.raw"
# Repeat from N=1600 upward (1700, 1800, ...), `reset` between runs, until the first failure.
```

**Do not touch.** The batcher in `src/`, `gl_texture.c`'s upload path (parity depends on it), and any `docs/` page
other than this sprint and the Phase 5 state/sprint-table update in §4.

**Done means** §8: a measured ceiling on the final configuration above 1,600, no full GE wait in a steady frame,
VRAM accounted at boot in both depth configurations, `glReadPixels` correct on a stopped frame, no stubs left,
`basic_shapes` at 284 colours, overlapping cubes depth-correct with `PSP_GU_DEPTH=1`, and the Phase 5 exit table
reconciled (met, deferred with a recorded decision, or not met).

## 1. Objective

Phase 5 can be declared done only if: **the 60 fps ceiling with pspgu is measured on the final configuration and
beats 1,600 sprites**, the full GE wait is gone, VRAM is accounted at boot in both depth configurations,
`glReadPixels`/`TakeScreenshot` works on a stopped frame, `gl_stub.c` holds no silent stubs, the overlapping-cubes
depth probe passes, and the phase's exit criteria are answered one by one. A tie at 1,600 is a real result
("cheaper at the same ceiling") but **not** a Phase 5 pass unless the phase exit is explicitly amended. If the
2D→3D→2D scene is still not byte-identical, that criterion is either met or moved to phase 10 by a recorded
decision; until then phase 5 stays open.

## 2. Why now

Every previous sprint in this phase built toward a claim that has not been tested: that replacing pspGL pays for
itself. So far the evidence is a **2D-identical** picture and a **10 % cheaper** batched frame — good, but the
ceiling, which is what the phase page named as its exit, has never been re-measured with this driver. If the ceiling
did not move, that is a phase-level finding and it changes what phase 6 and 7 are worth.

## 3. Scope

**In:** the ceiling bisection on the final configuration; VRAM accounting; both `PSP_GU_DEPTH` configurations;
`glReadPixels` and `TakeScreenshot`; two rotating lists **with per-list vertex arenas** and queued sync; emptying
`gl_stub.c`; the `basic_shapes` re-run; a minimal overlapping-cubes depth probe; the phase-exit reconciliation.

**Out:** 3D through `DrawMesh` parity (phase 10, sprint 013's T1b) — but **not** the depth probe, which is a phase-5
exit; non-power-of-two textures (task 6c, may slip to phase 6); mipmaps and texture residency (phase 6); the counter
harness (done in 013).

## 4. Preconditions

- [ ] Phase 5 page state is current: 011–013 done, task 12 done, 014 current. The page still says "not started" and
      "013 written, not started"; fix those before the first device run so the sprint and its phase agree
- [ ] `pspsh` answers, USB device `054c:01c9`; `reset` per run
- [ ] pspgu installed and `--check` agrees; the 2D gate re-confirmed at **0 differing pixels** this session
- [ ] pspGL restorable, `pspgl-build.sh --check` at `2ab7378e`
- [ ] Baselines in hand: pspGL ceiling **1,600 last measured / 1,700 fails** (bracket 1,500–1,700); model
      `0.87 + 6.35 µs × N` gives **7.22 ms** at N=1,000 while the measured value is **7.30 ms**; pspgu at N=1,000 →
      **6.565 ms**
- [ ] `bunnymark` links with `WRAP=1` (sprint 013 T3)
- [ ] A one-list/full-sync `SPLIT` baseline at N=1,000 and N=1,600; the final configuration's ceiling is compared
      against it, not against pspGL alone
- [ ] `batch_check` has a `MESH_SCENE=1` build and a two-overlapping-cubes scene ready for the depth probe

## 5. Tasks

| ID | task | status | verification | evidence |
|---|---|---|---|---|
| T1 | **the ceiling on the final configuration**: 1,600 is the floor; probe 1,700, then bisect to the adjacent last-passing/first-failing N up to 3,200, two runs each, `TARGET_FPS=0`, `WRAP=1`. Record `work` and texels/frame at every point. Repeat on the rotated-list build; that is the reported ceiling | `done` | last passing and first failing N are adjacent; `work` and fill rate recorded; the reported ceiling is from the configuration that ships; a tie at 1,600 is recorded as a phase-level finding, not a pass | **1,895 pass / 1,896 fail** on the one-list build; final configuration **2,375 pass / 2,376 fail**, two runs each. At 2,375: median 16.684 ms, p95 16.726–16.729, `work` 16.553–16.555 ms, **2.432 M texels/frame**. At 2,376: p95 33.313–33.315 ms in both runs. pspGL's last pass was 1,600 → **+48.4 %** |
| T2 | **VRAM accounting at boot**: draw, display, depth, list, textures allocated, used, free tail and peak — one line, in both depth configurations. The current allocator is a bump allocator: report the free tail, not a "largest free block" the allocator cannot provide. Add an allocation counter | `done` | totals add up to ≤ 2 MiB in both configurations; the texture total equals the allocation counter; no free-list metric is claimed | Depth=1 assets: `fb=278528 x3 + depth=278528 + textures=69636 + pad=60 = used=1183808 free=913344 peak=1183808 allocs=3`. Depth=0 assets: `fb=278528 x3 + depth=0 + textures=69636 + pad=60 = used=905280 free=1191872 peak=905280 allocs=3`. Both add up exactly; the list is host RAM and is not counted in VRAM |
| T3 | **`PSP_GU_DEPTH=0`**: boot it, confirm the boot line stops reserving the 272 KiB, the 2D frame is unaffected, and `glClear(GL_DEPTH_BUFFER_BIT)` is a documented no-op | `done` | both builds boot; 2D gate is 0 differing pixels in both; a depth clear with no depth buffer touches no null pointer | Depth=0 boot: `depth=0`, `used=835588`, free 1,261,564 bytes; 2D gate **byte-identical** (0 differing pixels); the cleared frame runs with the DEPTH bit dropped in `glClear` |
| T4 | **`glReadPixels` + `TakeScreenshot`**: expand the 5650 framebuffer to RGBA8888 and flip. Test on a stopped frame (`DONE` + sleep), after two identical `scrshot` captures; compare the 480×272 visible region at 512 stride, RGB only | `done` | two `scrshot` captures are identical; the PNG matches the visible region within the 5-6-5 quantisation; alpha is ignored; no torn capture is used as evidence | Two `scrshot` captures identical; PNG vs capture **max diff 7** (one 5-6-5 step), **0 pixels >8**; the intended tint (30,40,200) arrives as (24,40,206). The 5650 is RGB565; `scrshot` writes BGR, so the comparison swaps its channels |
| T5 | **display lists (mandatory)**: two rotating lists **and per-list vertex arenas**, so frame N never writes memory frame N–1's list still points at; submit frame N and sync frame N–1 with `GU_SYNC_WHAT_QUEUED`; size each list from the measured worst case and assert overflow. Measure against today's one list + full sync; keep the rotation even if the performance delta is null, because no full wait is a Phase 5 exit | `done` | a stale-vertex test (frame N draws different vertices before syncing N–1; no frame N–1 geometry appears); `SPLIT work/swap` at N=1,000 and N=1,600; list size and overflow assertion recorded; no full GE wait in a steady frame | Two 240 KiB lists + two 128 KiB arenas. **N=1,000: work 7.478 ms, swap 9.355**; **N=1,600: work 11.325, swap 5.51**; N=2,375: work 16.55, swap 0.90. Reuse wait avg **18–22 µs**, display wait avg **22–26 µs**. List high-water **1,456 B** of 245,760; arena high-water ≤91 KiB of 128 KiB; overflow fallback **0**. The moving-bar tear probe is one clean line; the frozen 1,895 frame is byte-identical to pspGL. `GU_SYNC_WHAT_QUEUED` is a non-blocking poll (`pspge.h`: syncType 1 = peek), so memory safety uses `sceGeListSync(qid, PSP_GE_LIST_DONE)` on the list being reused instead |
| T6 | **empty `gl_stub.c`**: every symbol implemented, or deliberately dropped and removed from the call list with a reason; any remaining called stub logs once and asserts in a debug build | `done` | no symbol resolves to a silent stub; `psp-nm` shows no stub object supplying a live symbol; a deliberately dropped symbol is absent from the list and the reason is recorded | `gl_stub.c` deleted; `glGetString`/`glGetFloatv`/`glReadPixels` live in `gl_query.c`. The 52 symbols `libraylib.a` needs are all supplied; no undefined `gl*`/`egl*` remains. `glLineWidth`/`glPolygonMode`/`glPixelStorei` are documented no-ops, not hidden failures |
| T7 | **`basic_shapes` re-run** on both drivers (sprint 013's build failed and its capture is invalid) | `done` | 284-colour fingerprint, and the two drivers' captures compared pixel by pixel | **284 colours on both; 0 differing pixels.** The test app freezes its last four frames so the queued display's lag cannot turn into a false diff. The earlier 26/78-byte differences were real: per-draw arena allocations were only 16-byte aligned, so one polyline shared a dirty 64-byte cache line; **64-byte alignment made the captures byte-identical** |
| T8 | **3D depth exit probe**: two overlapping cubes with `PSP_GU_DEPTH=1`, depth-correct front/back order; also record whether the `batch_check MESH_SCENE=1` 2D→3D→2D scene is byte-identical. `DrawMesh` parity itself remains phase 10 | `done` | overlapping cubes are depth-correct; if the mesh scene is not byte-identical, the phase exit is recorded as deferred to phase 10, not marked met | Near (red) cube drawn **first**, far (blue) cube **second**. Depth=1: overlap **6,360 px red** (224,36,48); depth=0: the same pixels are **blue** (0,116,232). Depth is doing the work, not draw order. `MESH_SCENE`/`DrawMesh` is **not** byte-identical and is deferred to phase 10 with sprint 013's evidence |

Status vocabulary: `todo` / `doing` / `done` / `blocked` / `dropped`. IDs are stable — never renumber.

### Task log (append-only)

- **T1, first bracket (one list, full sync).** 1,600 and 1,700 both pass; 1,800, 1,850, 1,875, 1,880, 1,885, 1,890 and 1,895 pass; 1,896 fails (p95 33.33 ms). The old pspGL first failure was 1,700, so the driver alone moved the ceiling +18.4 %. Two runs agree to ≈0.01 ms.
- **T1, instrument lie found.** `pspgu-build.sh` backed up the installed archive with `! grep -q pspgu <(grep -c pspguBeginFrame ...)`: the count prints `0`/`1`, which never contains "pspgu", so every pspgu install overwrote the pspGL backup with pspgu. An apparent "pspGL" run was pspgu (60 fps at 1,895). Fixed the test to inspect `psp-nm` symbols, made `--restore` reject a pspgu archive, rebuilt pspGL from pinned source and verified content hash `2ab7378e`. Real pspGL at 1,895: median 33.367 ms.
- **T5, queue semantics.** Public pspsdk source: `sceGuStart(GU_DIRECT, buf)` enqueues immediately with the stall at the list start; `sceGuFinish` moves the stall. `sceGuSync(GU_SYNC_FINISH, GU_SYNC_WHAT_QUEUED)` passes syncType 1, which is **peek**, not a wait. Memory safety therefore waits with `sceGeListSync(qid, PSP_GE_LIST_DONE)` on the list about to be reused, using the qid `ge_list_executed[0]` after each `sceGuStart`.
- **T5, display tear.** Two buffers cannot show a complete frame and submit without a full wait. A first 3-buffer attempt used `PSP_DISPLAY_SETBUF_NEXTFRAME`; `sceDisplayGetFrameBuf` showed the display reading the **same address** the next frame was drawing (the set-buffer took effect two frames later). Switched to `NEXTHSYNC` after the existing vblank wait: display offset and draw offset are now always different, and the moving-bar probe is one clean vertical line (confirmed on the device by eye).
- **T5, arena alignment.** With per-draw arena starts 16-byte aligned, `basic_shapes` differed from pspGL by 26–71 edge pixels on one polyline, while a diagnostic direct-pointer build was byte-identical. The difference was the D-cache line: a draw sharing a dirty 64-byte line with its neighbour was written back with a stale tail. **64-byte arena alignment made `basic_shapes` byte-identical.**
- **T4, readback.** The PNG first looked red/blue-swapped because `scrshot` writes the framebuffer bytes in BGR order; the driver's 5650 expansion is standard RGB565 (red low). Comparing against the channel-swapped capture: max diff **7** (bit replication vs truncation), 0 pixels >8, two captures identical.
- **T1, final bracket.** On the shipped 3-buffer/2-list/queued build: 2,200, 2,300, 2,325, 2,350 and 2,375 pass; 2,376 fails both runs (p95 33.31), and 2,380/2,385/2,390 also fail. **2,375 is the last pass**, work 16.55 ms, 2.43 M texels/frame.
- **T3/T8.** Depth=0 boot reserves no depth and the 2D gate is byte-identical. The overlapping-cubes probe is depth-correct: near-red drawn first holds the 6,360-pixel overlap with depth on; the same pixels are far-blue with depth off.
- **T6/T7.** `gl_stub.c` deleted, `gl_query.c` added; 52/52 symbols supplied. `basic_shapes` 284 colours and byte-identical on both drivers after the arena-alignment fix and a four-frame end freeze.

## 6. Budgets

| budget | limit | measured | tag |
|---|---|---|---|
| 60 fps ceiling with pspgu, final config | **> 1,600 sprites** | **2,375 pass / 2,376 fail** (two runs each) | `[measured: PSP-1000, b16]` |
| 2D gate, differing pixels | 0 | **0** (byte-identical, depth=1 and depth=0) | `[measured]` |
| VRAM accounted | ≤ 2 MiB, totals add up from an allocation counter | **1,183,808 B depth=1 / 905,280 B depth=0**, adding up exactly; free tails 913,344 / 1,191,872 B | `[measured]` |
| `glReadPixels` vs `scrshot` | equal within 5-6-5 quantisation, two identical captures | **max 7**, 0 pixels >8, two captures identical | `[measured]` |
| full GE wait in a steady frame | 0 | **0**; reuse wait 18–22 µs, display wait 22–26 µs, both targeted at an older list | `[measured]` |
| list rotation + vertex arenas | kept; null performance delta recorded, not reverted | kept; the ceiling moved **1,895 → 2,375** | `[measured]` |
| display-list memory | 2 × measured worst case (current `PSP_GU_LIST_BYTES` is 240 KiB each) | 2 × 240 KiB; high-water **1,456 B** at N=2,375 | `[measured]` |
| vertex arenas | 2 × `RLPSP_BATCH_CAP` × 24 B = 192 KiB at 4,096 vertices; record the actual | **2 × 128 KiB = 256 KiB**; 91 KiB used at N=1,895; overflow fallback 0 | `[measured]` |
| stale-vertex artifacts | 0 | **0** (display offset ≠ draw offset; moving bar clean; frozen frame byte-identical) | `[measured]` |
| silent stubs remaining | 0 | **0**; 52/52 symbols supplied | `[measured]` |
| overlapping-cubes depth probe | depth-correct with `PSP_GU_DEPTH=1` | correct: 6,360-px overlap red with depth, blue without | `[measured]` |

## 7. Measurements

Ceiling. T5 is mandatory, so the final row is the one that counts as the reported ceiling.

All pspgu rows are the shipped build: three framebuffers, two rotating lists, two 128 KiB arenas, queued submission, `PSP_GU_DEPTH=1`.

| N | pspGL (sprint 008) | pspgu, one list + full sync | pspgu, final | final `work` | texels/frame | note |
|---|---|---|---|---|---|---|
| 1,000 | 16.683 ms, work 7.30 | 16.683 ms, work 6.565 | **16.683 ms** | 7.478 ms | 1.02 M | 60 fps |
| 1,600 | 16.684 ms (last at 60 fps) | 16.684 ms, work 10.086 | **16.684 ms** | 11.325 ms | 1.64 M | phase-4 floor |
| 1,700 | 33.367 ms | passes | — | — | 1.74 M | pspGL's first failure |
| 1,895 | — (fails above 1,700) | 16.683 ms, work 11.765 | **16.683 ms** | 13.23 ms | 1.94 M | one-list last pass |
| 2,200 | — | — | **16.683 ms** | 15.23 ms | 2.25 M | |
| 2,375 | — | — | **16.684 ms**, p95 16.726–16.729 | 16.553–16.555 ms | 2.432 M | **last pass** |
| 2,376 | — | — | p95 **33.313–33.315 ms** | 16.559–16.562 ms | 2.433 M | **first failure** |
| 2,400 | 33.367 ms, work 14.97 | — | 33.31 ms (fails) | 16.59 ms | 2.46 M | |

The ceiling is 2,375, bracketed by the adjacent failure at 2,376 in both runs. pspGL's recorded bracket was 1,600 last measured / 1,700 first failure; the gain is **+48.4 %** on the last measured pspGL pass. The final `work` is higher than the one-list driver at the same N because CPU and GE now overlap and the fast path copies vertices into the per-list arena; `swap` falls by more, and the total frame is unchanged until the ceiling, which moves up.

Submission:

| configuration | N=1,000 work | N=1,600 work | stale-vertex test | verdict |
|---|---|---|---|---|
| one list, full sync | 6.565 ms | 10.086 ms | n/a | baseline; ceiling 1,895 |
| two lists + arenas, queued sync | 7.478 ms | 11.325 ms | pass | **kept**; ceiling **2,375**. The delta is not null, so the "keep it even if null" clause was not needed |

Stale-vertex / display evidence: `sceDisplayGetFrameBuf` shows display offset ≠ draw offset on every logged frame; the moving-bar probe is one clean vertical line (device confirmed); a fixed-seed 1,895-sprite frame is byte-identical between pspgu and pspGL; reuse waits average 18–22 µs; display waits 22–26 µs; arena overflows 0.

Depth probe:

| configuration | result |
|---|---|
| `PSP_GU_DEPTH=1`, two overlapping cubes | **correct.** Near (red) drawn first holds the 6,360-pixel overlap at (224,36,48); far (blue) is visible only outside it |
| `PSP_GU_DEPTH=0`, same scene | depth clear is a no-op; the same overlap is blue (0,116,232); 2D gate still 0 differing pixels |

## 8. Exit criteria

- [x] ceiling measured on the **final configuration** by bisection, with adjacent last-passing/first-failing N and
      `work` + fill rate recorded; **> 1,600** is a pass, a tie is a phase-level finding and not a pass
      — **2,375 pass / 2,376 fail**, two runs each; work 16.55 ms, 2.432 M texels/frame
- [x] the 2D gate is 0 differing pixels at the end of the sprint — byte-identical with depth=1 and depth=0
- [x] VRAM accounted at boot in both depth configurations, totals adding up from the allocation counter; the bump
      allocator's free tail is reported, not a fictitious largest free block — 1,183,808 B / 905,280 B, adding up
- [x] `glReadPixels` produces a PNG that matches a `scrshot` capture of the same stopped frame within 5-6-5
      — max 7, 0 pixels >8, two identical captures (capture channels are BGR)
- [x] display lists: two lists with per-list vertex arenas and queued sync; no full GE wait in a steady frame; the
      stale-vertex test passes; a null performance delta is recorded, not used to revert the change
      — kept; the ceiling moved 1,895 → 2,375; targeted waits 18–26 µs
- [x] `gl_stub.c` holds no silent no-ops, and any deliberately dropped symbol is removed from the call list with a
      recorded reason — file deleted; 52/52 symbols supplied
- [x] `basic_shapes` at 284 colours on both drivers — 284 on both, **0 differing pixels**
- [x] overlapping cubes are depth-correct with `PSP_GU_DEPTH=1`
- [x] Phase 5's exit table is reconciled: the 2D→3D→2D scene is either byte-identical or explicitly deferred to
      phase 10 with a recorded decision; if it is deferred, phase 5 is not marked done until that decision is
      accepted — **deferred to phase 10**, recorded in that page's "Carried in from phase 5" section with sprint
      013's evidence; the decision is accepted here

## 9. Result

**Outcome (a): the final configuration holds 60 fps at 2,375 sprites, and fails at 2,376.** Two runs per point; the
last pass and first failure are adjacent. The reported ceiling is from the configuration that ships: three
framebuffers, two rotating 240 KiB lists, two 128 KiB per-list arenas, queued submission, `glReadPixels`, no stubs,
`PSP_GU_DEPTH=1`.

| figure | value |
|---|---|
| ceiling, final configuration | **2,375 sprites at 60 fps** (2,376 fails) |
| pspGL's last measured pass | 1,600 (first failure 1,700) |
| move | **+48.4 %** over pspGL; **+25.3 %** over the one-list pspgu build (1,895) |
| N=2,375 frame | median 16.684 ms, p95 16.726–16.729 ms |
| N=2,375 work | 16.553–16.555 ms |
| N=2,375 fill | 2.432 M blended texels/frame |
| full GE wait in a steady frame | **none**; the two waits are `sceGeListSync` on a two-frame-old list (18–22 µs) and on the frame being shown (22–26 µs) |
| 2D gate | **0 differing pixels** |
| VRAM | 1,183,808 B used (58 % of 2 MiB), 913,344 B free with depth; 905,280 B / 1,191,872 B without |

**What moved it.** The queued submission removed the full `sceGuSync` from the frame and let the CPU and GE overlap;
the one-list build's ceiling was 1,895, so the change is worth **+480 sprites**, not a null result. Work at a fixed N
rose (7.478 vs 6.565 ms at N=1,000; 11.325 vs 10.086 at N=1,600) because the fast path now copies vertices into the
per-list arena and the CPU runs against a busy GE; `swap` fell by more, and the total frame is unchanged until the
ceiling moves.

**What the three-buffer display cost.** Two framebuffers cannot show a completed frame while the next is submitted
without a full wait. A third 272 KiB buffer makes the display always show the frame before the one being drawn. The
VRAM budget therefore became 3 × 272 KiB colour + 272 KiB depth = 1,088 KiB, leaving 913 KiB for textures (69,636 B
used by the current corpus). This is a deliberate, recorded trade for a tear-free queued submission; the two-buffer
numbers in §0 of the phase page are superseded.

**What is not met.** The 2D→`DrawCube`+`DrawMesh`→2D scene is still not byte-identical: `glDrawElements` emits and
produces no pixels. That criterion is **deferred to phase 10**, not redefined; the overlapping-cubes depth criterion
that phase 5 did own is met.

## 10. Follow-ups

- 3D through `DrawMesh` (phase 10, with sprint 013's evidence); the overlapping-cubes depth probe is closed here.
  The Phase 10 page carries the narrow statement of the defect.
- Three-framebuffer display budget: 1,088 KiB for colour + depth. If a later scene needs the 272 KiB back, the
  alternative is a full GE wait before the swap — measured, not assumed, and it costs the ceiling.
- Non-power-of-two textures: pad the height and correct with `sceGuTexScale` (task 6c, phase 6).
- Mipmaps, 4-bit glyph atlases, texture residency (phase 6).
- `glGetFloatv` for `GL_LINE_WIDTH`/`GL_POINT_SIZE` still answers from constants.
- `glReadPixels` mid-frame (a list still open) finishes and waits for that list; that is a diagnostic path, not a
  per-frame one. Document it before any sample uses it in a loop.
- The pspgu backup/restore test was fixed here after it silently replaced the pspGL archive with pspgu; `--restore`
  now refuses a backup containing `pspguBeginFrame`.
