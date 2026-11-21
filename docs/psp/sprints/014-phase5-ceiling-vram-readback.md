# Sprint 014 — Ceiling, VRAM and readback: closing phase 5

| | |
|---|---|
| Roadmap phase | [Phase 5 — Native GU backend](../phases/05-native-gu-backend.md) |
| Branch / base commit | `master` @ `de61e4cf` |
| Started / closed | — / — |
| Build number(s) | `b15` (pspgu, 2D parity, counters linked) |
| Tasks | 1/8 done |

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
| T1 | **the ceiling on the final configuration**: 1,600 is the floor; probe 1,700, then bisect to the adjacent last-passing/first-failing N up to 3,200, two runs each, `TARGET_FPS=0`, `WRAP=1`. Record `work` and texels/frame at every point. Repeat on the rotated-list build; that is the reported ceiling | `todo` | last passing and first failing N are adjacent; `work` and fill rate recorded; the reported ceiling is from the configuration that ships; a tie at 1,600 is recorded as a phase-level finding, not a pass | — |
| T2 | **VRAM accounting at boot**: draw, display, depth, list, textures allocated, used, free tail and peak — one line, in both depth configurations. The current allocator is a bump allocator: report the free tail, not a "largest free block" the allocator cannot provide. Add an allocation counter | `todo` | totals add up to ≤ 2 MiB in both configurations; the texture total equals the allocation counter; no free-list metric is claimed | — |
| T3 | **`PSP_GU_DEPTH=0`**: boot it, confirm the boot line stops reserving the 272 KiB, the 2D frame is unaffected, and `glClear(GL_DEPTH_BUFFER_BIT)` is a documented no-op | `todo` | both builds boot; 2D gate is 0 differing pixels in both; a depth clear with no depth buffer touches no null pointer | — |
| T4 | **`glReadPixels` + `TakeScreenshot`**: expand the 5650 framebuffer to RGBA8888 and flip. Test on a stopped frame (`DONE` + sleep), after two identical `scrshot` captures; compare the 480×272 visible region at 512 stride, RGB only | `todo` | two `scrshot` captures are identical; the PNG matches the visible region within the 5-6-5 quantisation; alpha is ignored; no torn capture is used as evidence | — |
| T5 | **display lists (mandatory)**: two rotating lists **and per-list vertex arenas**, so frame N never writes memory frame N–1's list still points at; submit frame N and sync frame N–1 with `GU_SYNC_WHAT_QUEUED`; size each list from the measured worst case and assert overflow. Measure against today's one list + full sync; keep the rotation even if the performance delta is null, because no full wait is a Phase 5 exit | `todo` | a stale-vertex test (frame N draws different vertices before syncing N–1; no frame N–1 geometry appears); `SPLIT work/swap` at N=1,000 and N=1,600; list size and overflow assertion recorded; no full GE wait in a steady frame | — |
| T6 | **empty `gl_stub.c`**: every symbol implemented, or deliberately dropped and removed from the call list with a reason; any remaining called stub logs once and asserts in a debug build | `todo` | no symbol resolves to a silent stub; `psp-nm` shows no stub object supplying a live symbol; a deliberately dropped symbol is absent from the list and the reason is recorded | — |
| T7 | **`basic_shapes` re-run** on both drivers (sprint 013's build failed and its capture is invalid) | `done` | 284-colour fingerprint, and the two drivers' captures compared pixel by pixel | **0 differing pixels, 284 colours on both.** The failure was one missing flag: `basic_shapes`' Makefile never got `-lpspgu`, which pspgu needs because it reaches the GE through GU where pspGL used `libpspge` — so the link failed and a stale pspGL binary ran |
| T8 | **3D depth exit probe**: two overlapping cubes with `PSP_GU_DEPTH=1`, depth-correct front/back order; also record whether the `batch_check MESH_SCENE=1` 2D→3D→2D scene is byte-identical. `DrawMesh` parity itself remains phase 10 | `todo` | overlapping cubes are depth-correct; if the mesh scene is not byte-identical, the phase exit is recorded as deferred to phase 10, not marked met | — |

Status vocabulary: `todo` / `doing` / `done` / `blocked` / `dropped`. IDs are stable — never renumber.

### Task log (append-only)

- _(not started)_

## 6. Budgets

| budget | limit | measured | tag |
|---|---|---|---|
| 60 fps ceiling with pspgu, final config | **> 1,600 sprites** | | |
| 2D gate, differing pixels | 0 | | |
| VRAM accounted | ≤ 2 MiB, totals add up from an allocation counter | | |
| `glReadPixels` vs `scrshot` | equal within 5-6-5 quantisation, two identical captures | | |
| full GE wait in a steady frame | 0 | | |
| list rotation + vertex arenas | kept; null performance delta recorded, not reverted | | |
| display-list memory | 2 × measured worst case (current `PSP_GU_LIST_BYTES` is 240 KiB each) | | |
| vertex arenas | 2 × `RLPSP_BATCH_CAP` × 24 B = 192 KiB at 4,096 vertices; record the actual | | |
| stale-vertex artifacts | 0 | | |
| silent stubs remaining | 0 | | |
| overlapping-cubes depth probe | depth-correct with `PSP_GU_DEPTH=1` | | |

## 7. Measurements

Ceiling. T5 is mandatory, so the final row is the one that counts as the reported ceiling.

| N | pspGL (sprint 008) | pspgu | pspgu work | texels/frame | note |
|---|---|---|---|---|---|
| 1,000 | 16.683 ms, work 7.30 | 16.683 ms | 6.565 ms | 1.02 M | 60 fps |
| 1,600 | 16.684 ms (last at 60 fps) | | | 1.64 M | phase-4 floor |
| 1,700 | 33.367 ms | | | 1.74 M | first failure under pspGL |
| 2,000 | 33.367 ms, work 14.97 | | | 2.05 M | |
| 2,400 | — | | | 2.46 M | upper probe; 3,200 if 2,400 passes |
| final config | — | | | | after T5; this is the reported ceiling |

Submission:

| configuration | N=1,000 work | N=1,600 work | stale-vertex test | verdict |
|---|---|---|---|---|
| one list, full sync | 6.565 ms | | n/a | baseline |
| two lists + arenas, queued sync | | | pass/fail | **kept even if the delta is null**; null result recorded |

Depth probe:

| configuration | result |
|---|---|
| `PSP_GU_DEPTH=1`, two overlapping cubes | front/back order correct? |
| `PSP_GU_DEPTH=0`, same scene | depth clear is a no-op; 2D gate still 0 |

## 8. Exit criteria

- [ ] ceiling measured on the **final configuration** by bisection, with adjacent last-passing/first-failing N and
      `work` + fill rate recorded; **> 1,600** is a pass, a tie is a phase-level finding and not a pass
- [ ] the 2D gate is 0 differing pixels at the end of the sprint
- [ ] VRAM accounted at boot in both depth configurations, totals adding up from the allocation counter; the bump
      allocator's free tail is reported, not a fictitious largest free block
- [ ] `glReadPixels` produces a PNG that matches a `scrshot` capture of the same stopped frame within 5-6-5
- [ ] display lists: two lists with per-list vertex arenas and queued sync; no full GE wait in a steady frame; the
      stale-vertex test passes; a null performance delta is recorded, not used to revert the change
- [ ] `gl_stub.c` holds no silent no-ops, and any deliberately dropped symbol is removed from the call list with a
      recorded reason
- [ ] `basic_shapes` at 284 colours on both drivers
- [ ] overlapping cubes are depth-correct with `PSP_GU_DEPTH=1`
- [ ] Phase 5's exit table is reconciled: the 2D→3D→2D scene is either byte-identical or explicitly deferred to
      phase 10 with a recorded decision; if it is deferred, phase 5 is not marked done until that decision is
      accepted

## 9. Result

(To fill in on close. The number that matters is the ceiling on the final configuration, not on the convenient one.
There are three outcomes: **(a)** above 1,600 — Phase 5 meets its exit; **(b)** exactly 1,600 — the driver bought
cost (10 % on the batched frame, 47 % on the immediate path) but not ceiling; that is a real result, but it is a
Phase 5 **finding**, not a pass, unless the phase exit is explicitly amended; **(c)** below 1,600 — a regression,
and Phase 5 fails. Record `work`, texels/frame and the full-wait status in all three cases, and say whether T5
moved them or produced a null result.)

## 10. Follow-ups

- 3D through `DrawMesh` (phase 10, with sprint 013's evidence); the overlapping-cubes depth probe is closed here.
- If the 2D→3D→2D scene is deferred, carry the decision into the Phase 5 page and Phase 10 scope.
- Non-power-of-two textures: pad the height and correct with `sceGuTexScale` (task 6c).
- Mipmaps, 4-bit glyph atlases, texture residency (phase 6).
- `glGetFloatv` for `GL_LINE_WIDTH`/`GL_POINT_SIZE` still answers from constants.
