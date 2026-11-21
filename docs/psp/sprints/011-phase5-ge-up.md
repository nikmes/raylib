# Sprint 011 — GE up and the vertex path

| | |
|---|---|
| Roadmap phase | [Phase 5 — Native GU backend](../phases/05-native-gu-backend.md) |
| Branch / base commit | `master` @ `ffabe753` |
| Started / closed | 2026-11-21 / 2026-11-21 |
| Build number(s) | `b11` (`tools/pspgu`, 36 KB archive) |
| Tasks | 4/4 done |

Written after the fact, because this work was done before the phase had sprint pages. It is the record of what the
first slice cost, and it is the reason the later sprints are shaped the way they are.

## 0. Handover

**Situation.** Phase 5 builds a driver, not a backend: `tools/pspgu` supplies the 52 GL/EGL symbols raylib's GL11
path actually calls, over `sceGu`, and is installed in place of pspGL by `tools/pspgu-build.sh`. That decision came
from the phase review — 52 symbols is the whole specification, and keeping `rlgl.h` untouched keeps the sprint
007–009 batcher and every existing capture meaningful.

**What this sprint proved.** The driver boots, clears the screen to the same value pspGL produces, transforms
correctly (a raw full-screen `GU_SPRITES` quad fills the display), and draws raylib's geometry through the batcher's
interleaved 24-byte vertices with no copy.

**The five bugs, because four of them looked identical from outside** — "nothing is drawn" while the clear works:

1. **Framebuffer size computed before the struct was filled** → both buffers at offset 0. A single-buffered driver
   still clears correctly, so this would have shipped. The boot log line caught it.
2. **`glEnableClientState` recorded bits the draw path never read** → the fast path never engaged and the vertex
   format lost its colour and texture-coordinate bits.
3. **No data-cache writeback before the GE reads vertices.** `sceGuClear` carries its colour in the command list and
   works; vertices come from main RAM, so the GE read stale memory. This was in the task list as an *optimisation*
   (task 9) and is in fact **blocking**.
4. **GU's view matrix is undefined after `sceGuInit`** and must be set to identity. Samples always set all three
   matrices; skipping one collapses every vertex to a point.
5. **`sceGuSetMatrix` wants the matrix in column-major memory order — no transpose.** Sending the GL matrix
   transposed put the ortho's translation in a `w` row, so every vertex was divided by it: geometry collapsed to a
   few pixels around the screen centre. It looks exactly like a projection bug and is not.

**Left behind, deliberately:** textures (`gl_texture.c` landed in sprint 012's run-up), the `--wrap` counter
harness (needs the immediate-mode symbols, phase-5 task 12), and `psp-tests/*` building with `WRAP=0`.

## 1. Objective

The 52 symbols exist, the GE comes up with two framebuffers and an optional depth buffer, a raw sprite covers the
display, and raylib's own 2D geometry reaches the GE through the batcher's fast path.

## 5. Tasks

| ID | task | status | verification | evidence |
|---|---|---|---|---|
| T1 | vertical slice: 52 stubs + real egl init, framebuffers, display setup, `glClear` | `done` | `psp-nm batch_check.elf \| grep -c pspgl_` = 0; capture uniform `(240,244,240)` | boot log `GU up: 480x272 stride 512, fb 272 KiB x2, depth yes (272 KiB), textures from 816 KiB` |
| T2 | matrices: the GL stack in CPU memory, ortho/frustum/push/pop/mult/rotate/scale/translate, pushed with `sceGuSetMatrix`; `glGetFloatv` answers both queries | `done` | a raw full-screen self-test sprite fills the display | `gl_matrix.c`, self-test behind `PSP_GU_SELFTEST` |
| T3 | vertex arrays: fast path for the interleaved 24-byte layout, converting path for separate arrays, `glDrawElements` with min/max rebasing, modes 10/4/1 | `done` | the trace shows `fast=1` and the batcher's own positions | `gl_draw.c` |
| T4 | cache discipline before the GE reads vertices | `done` | vertices appear at all; without it they were stale and every primitive was degenerate | `pspguWriteback` |

## 9. Result

The slice works and the phase's riskiest unknowns are answered: GU can be driven from a drop-in archive, the
batcher's vertex layout **is** GU's layout (no copy needed), and the GL matrix semantics raylib relies on can be
reproduced exactly. The five bugs above are the expensive part, and they are all in the phase page now.

What it did not answer: whether the *picture* matches pspGL once textures are involved. It does not — that is
sprint 012, and it is 62 % of the frame.

## 10. Follow-ups

- Textures (sprint 012), counter harness and immediate mode (013), lists/VRAM/readback (014).
- `glGetFloatv` still answers `GL_LINE_WIDTH`/`GL_POINT_SIZE` from constants; fine until something reads them.
