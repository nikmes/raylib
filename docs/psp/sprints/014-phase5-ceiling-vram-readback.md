# Sprint 014 — Ceiling, VRAM and readback: closing phase 5

| | |
|---|---|
| Roadmap phase | [Phase 5 — Native GU backend](../phases/05-native-gu-backend.md) |
| Branch / base commit | `master` @ `de61e4cf` |
| Started / closed | — / — |
| Build number(s) | `b15` (pspgu, 2D parity, counters linked) |
| Tasks | 0/7 done |

## 0. Handover

**Situation.** `tools/pspgu` replaces pspGL and is **byte-identical to it on the 2D corpus**: `batch_check` 2D
renders to the same bytes (0 differing pixels, 613 colours), the immediate path is ~1.9× cheaper than pspGL's, and
`bunnymark` links and runs with `WRAP=1` again. Sprint 013 split its 3D task: `DrawMesh`/`glDrawElements` emits a
draw and produces no pixels, handed to phase 10 with the suspect named. This sprint finishes what remains before
phase 5 can be called done — and **the ceiling has never been measured with this driver in place**, which is the
phase's actual exit criterion.

**What is left, in the phase's own task numbering:** 7 (`glReadPixels`), 8 (rotating display lists with queued
sync), 10 (VRAM accounting at boot), 11 (`PSP_GU_DEPTH=0`), 13 (remove the stubs), 6c (non-power-of-two textures),
plus the ceiling measurement and the phase's exit report.

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
- Sprint 013 left one broken item: the `basic_shapes` build on pspgu **failed** while the stale pspGL binary ran, so
  its capture is invalid. T7 below re-runs it.

**Read before acting:**

1. `tools/pspgu/egl.c` — `pspguBeginFrame`/`pspguEndFrame` are the whole display-list story today: one list,
  full sync per frame.
2. `tools/pspgu/pspgu_internal.h` — the context struct, the VRAM bump allocator, the compile switches.
3. [Baselines](../baselines.md) §"Scaling past the ceiling" — the model this sprint's ceiling number updates:
  `work = 0.87 ms + 6.35 µs × N`, and 60 fps held to 1,600 sprites under pspGL.

**First action.** Measure the ceiling with pspgu installed — `bunnymark` at N=1,600 and 2,000, two runs each, compare
against pspGL's 1,600/1,700 bracket. That single number decides whether this phase delivered what it was for.

**Hard rules.**

- **The 2D gate stays at 0 differing pixels** across every change here. It is the only regression gate for a driver
  that is otherwise identical to its predecessor.
- **A ceiling claim needs a bisection**, not an extrapolation: last N that holds 16.683 ms, first that does not.
- **VRAM accounting must add up**: framebuffers + depth + textures ≤ 2 MiB, and the texture total must equal the sum
  of what was uploaded.
- **Rotating lists are only worth it if the counters show it**: record `work`/`swap` before and after; a "queued
  sync" that does not move either is a change to revert, not to keep.
- **Removing a stub must not remove a call**: `gl_stub.c` empties only when its symbols are implemented elsewhere;
  any remaining symbol logs once and, in a debug build, asserts.
- pspGL restorable in one command, verified by `tools/pspgl-build.sh --check` (content hash `2ab7378e`).

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
```

**Do not touch.** The batcher in `src/`, `gl_texture.c`'s upload path (parity depends on it), and anything under
`docs/` outside this sprint's record.

**Done means** §8: a measured ceiling above 1,600, VRAM accounted at boot, both depth configurations booting,
`glReadPixels` producing a correct screenshot, no stubs left, and phase 5's exit table filled in.

## 1. Objective

Phase 5 can be declared done: **the 60 fps ceiling with pspgu is measured and beats 1,600 sprites**, VRAM is accounted
at boot in both depth configurations, `glReadPixels`/`TakeScreenshot` works, the display-list submission is either
improved with numbers or left alone with a reason, `gl_stub.c` is empty, and the phase's exit criteria are answered
one by one.

## 2. Why now

Every previous sprint in this phase built toward a claim that has not been tested: that replacing pspGL pays for
itself. So far the evidence is a **2D-identical** picture and a **10 % cheaper** batched frame — good, but the
ceiling, which is what the phase page named as its exit, has never been re-measured with this driver. If the ceiling
did not move, that is a phase-level finding and it changes what phase 6 and 7 are worth.

## 3. Scope

**In:** the ceiling bisection; VRAM accounting; the `PSP_GU_DEPTH=0` build; `glReadPixels` and `TakeScreenshot`;
display-list rotation with queued sync (measured, keep or revert); emptying `gl_stub.c`; the `basic_shapes` re-run;
the phase exit report.

**Out:** 3D through `DrawMesh` (phase 10, sprint 013's T1b); non-power-of-two textures (task 6c, can slip to phase 6);
mipmaps and texture residency (phase 6); the counter harness (done in 013).

## 4. Preconditions

- [ ] `pspsh` answers, USB device `054c:01c9`; `reset` per run
- [ ] pspgu installed and `--check` agrees; the 2D gate re-confirmed at **0 differing pixels** this session
- [ ] pspGL restorable, `pspgl-build.sh --check` at `2ab7378e`
- [ ] Baselines in hand: pspGL ceiling **1,500 holds / 1,700 fails**; `work = 0.87 + 6.35 µs·N` at N=1,000 →
      7.30 ms; pspgu at N=1,000 → **6.565 ms**
- [ ] `bunnymark` links with `WRAP=1` (sprint 013 T3)

## 5. Tasks

| ID | task | status | verification | evidence |
|---|---|---|---|---|
| T1 | **the ceiling** with pspgu: bisect N=1,600 / 2,000 / 2,400, two runs each, `TARGET_FPS=0`, `WRAP=1` | `todo` | last N holding 16.683 ms and the first failing | — |
| T2 | **VRAM accounting at boot**: framebuffers, depth, textures used, free, largest free block — one line, in both depth configurations | `todo` | the totals add up to ≤ 2 MiB and the texture figure equals what was uploaded | — |
| T3 | **`PSP_GU_DEPTH=0`**: boot it, confirm the boot line stops reserving the 272 KiB and that a 2D frame is unaffected | `todo` | both builds boot; the 2D gate is 0 differing pixels in both | — |
| T4 | **`glReadPixels` + `TakeScreenshot`**: expand the 5650 framebuffer to RGBA8888 and flip | `todo` | the PNG matches a `scrshot` capture of the same frame within the 5-6-5 quantisation — `scrshot` bypasses GL, so it is a genuine cross-check | — |
| T5 | **display lists**: rotate two lists, submit frame N and sync frame N-1 (`GU_SYNC_WHAT_QUEUED`), measured against today's single list and full sync | `todo` | `SPLIT work/swap` at N=1,000 and N=1,600; **keep the change only if it moves a number**, otherwise revert with the measurement recorded | — |
| T6 | **empty `gl_stub.c`**: every symbol implemented or deliberately dropped with a reason; a remaining stub logs once and asserts in a debug build | `todo` | the file holds no silent no-ops; `psp-nm` shows the 52 symbols all defined by a real file | — |
| T7 | **`basic_shapes` re-run** on both drivers (sprint 013's build failed and its capture is invalid) | `todo` | 284-colour fingerprint, and the two drivers' captures compared | — |

Status vocabulary: `todo` / `doing` / `done` / `blocked` / `dropped`. IDs are stable — never renumber.

### Task log (append-only)

- _(not started)_

## 6. Budgets

| budget | limit | measured | tag |
|---|---|---|---|
| 60 fps ceiling with pspgu | **> 1,600 sprites** | | |
| 2D gate, differing pixels | 0 | | |
| VRAM accounted | ≤ 2 MiB, totals add up | | |
| `glReadPixels` vs `scrshot` | equal within 5-6-5 quantisation | | |
| list rotation | must move `work`/`swap`, or be reverted | | |
| silent stubs remaining | 0 | | |

## 7. Measurements

Ceiling:

| N | pspGL (sprint 008) | pspgu | note |
|---|---|---|---|
| 1,000 | 16.683 ms, work 7.30 | 16.683 ms, work **6.565** | 60 fps |
| 1,600 | 16.684 ms (last at 60 fps) | | |
| 1,700 | 33.367 ms | | |
| 2,000 | 33.367 ms | | |
| 2,400 | — | | |

Submission, if T5 lands:

| configuration | N=1,000 work | N=1,600 work | verdict |
|---|---|---|---|
| one list, full sync | 6.565 ms | | baseline |
| two lists, queued sync | | | keep or revert |

## 8. Exit criteria

- [ ] ceiling measured by bisection with pspgu, and either **> 1,600** or the reason it did not move recorded
- [ ] the 2D gate is 0 differing pixels at the end of the sprint
- [ ] VRAM accounted at boot in both depth configurations, totals adding up
- [ ] `glReadPixels` produces a PNG that matches a `scrshot` capture
- [ ] display lists: improved with numbers, or reverted with the measurement recorded
- [ ] `gl_stub.c` holds no silent no-ops
- [ ] `basic_shapes` at 284 colours on both drivers
- [ ] phase 5's exit criteria answered in the phase page, including the 3D split from sprint 013

## 9. Result

(To fill in on close. The number that matters is the ceiling. If pspgu holds 1,600 as pspGL did, then replacing the
driver bought *cost* — 10 % on the batched frame, 47 % on the immediate path — but not *ceiling*, and the phase's
justification is "cheaper at the same ceiling" rather than "higher ceiling". That distinction decides whether phase 6
is about fill, phase 7 about overdraw, or phase 5's remaining levers — `GU_TRANSFORM_2D` and 16-byte vertices — are
worth another sprint.)

## 10. Follow-ups

- 3D through `DrawMesh` (phase 10, with sprint 013's evidence).
- Non-power-of-two textures: pad the height and correct with `sceGuTexScale` (task 6c).
- Mipmaps, 4-bit glyph atlases, texture residency (phase 6).
- `glGetFloatv` for `GL_LINE_WIDTH`/`GL_POINT_SIZE` still answers from constants.
