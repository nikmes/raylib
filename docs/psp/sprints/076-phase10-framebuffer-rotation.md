# Sprint 076 (P10-S9) — Framebuffer rotation with render targets

| | |
|---|---|
| Roadmap phase | [Phase 10 — 3D and advanced](../phases/10-3d-advanced/index.md) |
| Sprint | P10-S9 |
| Branch / base commit | `master` @ `f0dfd0ab` (P10-S7 closed; Phase 10 recorded done) |
| Started / closed | 2026-11-23 (written) / — |
| Build number(s) | pspgu `b31` (T2/T3 change the restore path); `libraylib.a` unchanged, default `PSP_BATCH=1` |
| Tasks | 0/4 done |

## 0. Handover

**Situation.** Sprint 069 closed Phase 10 with the sample at 60 fps (16.683 ms median, 11.56 ms work, 0 arena
resets) and all gates 0 — but it **deferred its own T5**: the checker flicker the user reported. The cause is
certain and the fix is blocked by one measured regression.

**What is known (do not re-derive).**

- `glBindFramebuffer` saves the screen target as `saved.draw = pspgu.drawOffset`. At the first bind of a frame
  `pspgu.drawOffset` still holds the *previous* frame's buffer, so the unbind restores a stale pointer. With the
  RTT panel in the sample, every frame's drawing is pinned to one framebuffer while the display rotates through
  three: two of every three displayed frames are empty in the 3D area.
- Evidence: `SAMPLE_DIAG` frame diff was a strict 3-frame cycle of 53,130 px; reading EDRAM gave
  `fb0=53,130, fb1=0, fb2=0`. Two captures taken 8 s apart are identical, so the defect is deterministic.
- Two candidate fixes were built: (a) call `pspguBeginFrame()` before the save, (b) keep the order and save
  `fbOffset[frames % 3]`. Both make `fb0 = fb1 = fb2` (flicker gone) and **both break the 065 RTT gate**:
  `MODE=direct` vs `MODE=rtt` differs by **624-800 px** in the panel's last 16 rows.
- Bisect: the exact 065 driver (b28, commit `6df56cb8`) with the current app and library gives **0 diff**; the
  corrected rotation gives the band. `sceGuTexFlush()` at target unbind changes nothing. `sceGuSync(0,
  GU_SYNC_FINISH)` inside the open list **deadlocks** the app, so the band is not a plain "sync harder" case.
- The diff is confined to the last 16 rows (two 8-row tiles) of the 256×256 panel; the rest of the region matches.
  The old code masks it because the screen pass draws into buffer 0, which is also what the direct arm does.

**Read before acting**, in this order:

1. `../sprints/069-phase10-clip-cost-and-lit-parity.md` §5 T5, §7 (defect row), §9 T5, §10 — the bisect and the
   rejected fixes.
2. `tools/pspgu/egl.c` — `pspguSwapCallback`, `pspguBeginFrame`, `eglSwapBuffers` (`show = (finished-1)%3`,
   `swapRender = (finished+1)%3`, `guSwapBuffersBehaviour(PSP_DISPLAY_SETBUF_NEXTHSYNC)`).
3. `tools/pspgu/gl_fbo.c` — `glBindFramebuffer` save/restore; `tools/pspgu/gl_state.c` — the `#include "gl_fbo.c"`
   that puts the FBO code in one object.
4. `psp-tests/rtt_probe/main.c` — `MODE=direct` draws the pattern to the screen; `MODE=rtt` renders it to a
   256×256 target then draws the sampled texture at (0,0) over a black clear. The gate compares the 256×256 region
   only; a whole-screen compare is meaningless (black vs raywhite outside the panel).
5. `psp-tests/scene_3d/main.c` — the `SAMPLE_DIAG` arm and `pspguDebugFrameStats` (still in the tree).

**First action.** T1: apply the minimal restore candidate behind a build switch and add the probe arm that reads
the target's last 16 rows *before* the sample. Three device runs. Do not change the restore path without the
localization data.

**Hard rules.**

- **No shipped regression.** A rotation fix that moves the RTT gate off 0 does not ship, whatever it does for the
  flicker. The two gates must both be 0.
- **A full sync inside an open list deadlocks.** It was tried in 069 T5; the list was not submitted, so the sync
  could never complete. Any sync must be at a list boundary.
- **The band is deterministic; the capture is not the variable.** Two captures 8 s apart were identical. Do not
  conclude "capture race" without two captures that differ.
- **`MODE=direct` and `MODE=rtt` differ outside the panel.** Compare the 256×256 region only.
- **Captures after `DONE`, two identical.** With the defect present, a capture at a frame count ≡ 2 (mod 3) can be
  an empty buffer — check that the capture has content before diffing.
- **One app per boot.** `pspsh -e "reset"` only resets with `usbhostfs_pc` running; `modstun` does not return the
  app's memory, so a second run needs a reset.
- **`make clean` after every driver change.** The test Makefiles do not depend on `libGL.a`; `strings app.prx` is
  the check that the build contains the change.
- **Bump `PSPGU_BUILD` to `b31` in T2.** Stage only this sprint's paths; never `git add -A`.
- **Leave pspGL installed (`2ab7378e`) when stopping.**

**Commands.**

```sh
export PSPDEV=/home/psp/pspdev
export PATH=$PSPDEV/bin:$PATH
cd /home/psp/raylib-psp
tools/pspgu-build.sh && tools/pspgu-build.sh --check
tools/pspgu-build.sh --restore && tools/pspgl-build.sh --check      # end state

run_rtt() {   # $1 = tag, $2 = MODE value
    ( cd psp-tests/rtt_probe && make clean >/dev/null && make MODE=$2 )
    cp psp-tests/rtt_probe/rtt_probe.prx /tmp/s3/app.prx
    ( cd /tmp/s3
      timeout 20 pspsh -e "reset"; sleep 25
      ( printf './app.prx\n'; sleep 18 ) | timeout 32 pspsh
      pspsh -e "scrshot host0:/$1-a.raw"
      pspsh -e "scrshot host0:/$1-b.raw" )
}
```

**Do not touch.** The 065 FBO implementation beyond the restore path; the clip pass; texture formats (another
session's `gl_texture.c` work); Phase 11 sprints.

**Done means** §8 below.

## 1. Objective

Falsifiable, in one sentence: with the fix, a static frame is identical in all three framebuffers
(`fb0 = fb1 = fb2`, `FLICK 0`), the RTT `direct` vs `rtt` region is **0 diff**, and the sample still meets median
≤16.683 ms, p95 ≤16.75 ms, work ≤15.0 ms with `arenaOverflows=0`.

## 2. Why now

It is the only user-visible defect left in the driver. It also contaminates every future capture: any app with a
render target shows an empty buffer on two of three displayed frames, and a capture taken at the wrong frame count
is silently empty. Phase 10 is closed on its numbers; this sprint exists so the closure is not resting on a known
display bug, and so P10-S8's cost work does not start on a driver whose screen path is still wrong.

## 3. Scope

**In:** `tools/pspgu/gl_fbo.c` restore/unbind; a hazard list-break helper in `egl.c` if T2 needs it; a temporary
build switch for the A/B; a diagnostic arm in `psp-tests/rtt_probe`; the docs and the 069 T5 close-out.

**Out:** new RTT features, texture-format work, the clip pass, culling, Phase 11.

## 4. Preconditions

- [ ] `master` @ `f0dfd0ab`; P10-S7 closed; `tools/pspgu-build.sh --check` reports a pspgu build
- [ ] pspGL `2ab7378e` available for the end state
- [ ] `rtt_probe` builds with `MODE=direct` and `MODE=rtt`; `scene_3d` builds with `SAMPLE=1` and `DIAG=1`
- [ ] `SAMPLE_DIAG` and `pspguDebugFrameStats` still present in the tree
- [ ] Device reset between runs; `DONE` + sleep; two identical captures

## 5. Tasks

| ID | task | status | verification | evidence |
|---|---|---|---|---|
| T1 | **Localize the band.** Build the minimal restore candidate behind `PSPGU_FB_RESTORE=1` (default off) and add a `BAND_DIAG` arm to `rtt_probe`: render the target, `glReadPixels` its last 16 rows *before* sampling (the wait is deliberate and diagnostic-only), log black counts; then draw and read the screen region. Attribute the 624-800 px to the target render or to the sampling | `todo` | target last-16 black count and screen black count on the same frame; direct/rtt reference captures | — |
| T2 | **Hazard list break on unbind.** Instead of `sceGuDrawBufferList(saved.draw)` after an RTT: submit the target list, mark the texture dirty, start a new list (the swap callback's render pointer applies at `sceGuStart`, so it is this frame's framebuffer), set `drawOffset = fbOffset[drawIndex]`. If T1 says the target is clean and the sample is wrong, add the cache flush at the boundary. Bump `b31` | `todo` | flicker gone (`fb0=fb1=fb2`, `FLICK 0`); RTT region **0 diff**; list waits and arena overflows reported; sample thresholds hold | — |
| T3 | **Conditional two-framebuffer fallback.** Only if T2 fails: `PSP_GU_FB_COUNT=2` with the correct restore; measure list/display waits and the RTT region; keep the design that meets §6 with the smaller wait | `todo` | RTT 0 diff; sample thresholds; waits recorded | — |
| T4 | **Re-close.** Gates (2D, 060 mesh, `MESH_SCENE`, `SCENE=both`, 065 RTT, 066 probes/cull, `basic_shapes`), sample 300 frames ×2, docs (069 T5 resolved in place, Phase 10 page), commit, restore pspGL | `todo` | every gate 0; numbers written; `2ab7378e` installed; strict build | — |

### Design notes

**Why a list break can be the fix.** The 624-800 px band appears only when the target render and the screen sample
are in the same display list *and* the screen pass draws into the rotated buffer. A list boundary makes the GE
complete the target write before the sample list starts; and because `sceGuStart` applies the render pointer the
swap callback set, the new list automatically targets this frame's framebuffer — the restore does not have to
guess it. This addresses both the flicker and the band with one mechanism.

**What the list break costs.** One extra submit per RTT frame. `pspguBeginFrame` rotates lists and waits on the
list two frames old (a targeted wait, 13-14 µs in the 069 final runs); the arena is per-list, so the target
render's vertices stay valid while the screen list runs. Watch `arenaOverflows` and `list_used_max`: if a frame
needs two lists per target and the arena is not enough, split at frame boundaries instead.

**What T1 must rule out.** If the target's own last 16 rows are black, the band is a render-side problem (scissor,
viewport, depth) and T2's list break will not fix it — the sprint then pivots to the target's clear/viewport, not
the restore. If the target is clean and the screen is black, the sample side is the problem and T2 is the right
fix. Do not implement T2 before T1 answers this.

**Diagnostics stay.** `SAMPLE_DIAG` (`FLICK`, `FLICKPX`, `FLICKBB`) and `pspguDebugFrameStats` (`FBSTAT`) were
added in 069 and are the cheapest way to see buffer rotation failures. Keep them.

**The rejected variants stay rejected.** Moving `pspguBeginFrame` before the save and saving
`fbOffset[frames % 3]` both fix the flicker and both carry the band. Do not re-try them without a new datum.

## 6. Budgets

| budget | limit | measured | tag |
|---|---|---|---|
| sample median | ≤16.683 ms | | |
| sample p95 | ≤16.75 ms | | |
| sample work | ≤15.0 ms | | |
| arena overflows | 0 | | |
| list wait | targeted (no full GE wait) | | |
| RTT region diff | 0 | | |
| all gates | 0 diff | | |

## 7. Measurements

Protocol: device only; one variable per arm; two identical captures; reset between runs.

| run | build | workload | metric | value | notes |
|---|---|---|---|---|---|
| defect | b30 | `SAMPLE=1 N=50 DIAG=1` | framebuffer content | **fb0 53,130 / fb1 0 / fb2 0 px** | 3-frame checker cycle |
| bisect | b28 driver | RTT `direct` vs `rtt` | region diff | **0** | current app and library |
| bisect | b30 + restore (a) | same | region diff | **800** | last 16 rows |
| bisect | b30 + restore (b) | same | region diff | **624-704** | same band; flush no help |
| T1 | b31 diag | target last rows vs screen | black counts | | target-side or sample-side |
| T2 | b31 | RTT `direct` vs `rtt` | region diff | | target 0 |
| T2 | b31 | sample `DIAG=1` | fb0/fb1/fb2, FLICK | | target identical, 0 |
| T2 | b31 | sample N=50, 300 frames x2 | median / p95 / work / overflows | | thresholds hold |
| gate | b31 | 2D/060/MESH_SCENE/061/066 | diff | | 0 |
| gate | b31 | `basic_shapes` | colours | | 284 |

## 8. Exit criteria

- [ ] T1: the band is attributed to the target render or the sampling, with counts
- [ ] T2 or T3: `fb0 = fb1 = fb2` and `FLICK 0`; RTT region **0 diff**
- [ ] sample N=50 300-frame pair: median ≤16.683, p95 ≤16.75, work ≤15.0, `arenaOverflows=0`, waits reported
- [ ] gates 2D/060/MESH_SCENE/061/065/066 = 0; `basic_shapes` 284
- [ ] boot tag `b31`; docs updated (069 T5 resolved); strict build; pspGL restored

## 9. Result

—

## 10. Follow-ups

- If neither the list break nor two buffers clears the band, record the evidence and hand the target's last-tile
  visibility question to a focused driver investigation; do not ship a rotation change with a nonzero RTT gate.
- P10-S8 (070) still owns the per-`DrawMesh` matrix/cull cost; it should start from the post-fix sample number.
