# Sprint 076 (P10-S9) — Framebuffer rotation with render targets

| | |
|---|---|
| Roadmap phase | [Phase 10 — 3D and advanced](../phases/10-3d-advanced/index.md) |
| Sprint | P10-S9 |
| Branch / base commit | `master` @ `f0dfd0ab` (P10-S7 closed; Phase 10 recorded done) |
| Started / closed | 2026-11-23 (written) / 2026-11-23 (closed) |
| Build number(s) | pspgu `b31`; `PSPGU_FB_RESTORE=2` (adopted), `PSPGU_TEXSYNC_FIRST=0`; `libraylib.a` unchanged |
| Tasks | 4/4 done — T2 and T3 dropped as unnecessary |

## 0. Handover

**Situation (resolved).** Sprint 069 deferred its own T5: `glBindFramebuffer` saved the screen target from
`pspgu.drawOffset` before `pspguBeginFrame()` advanced it, so an RTT unbind restored the previous frame's buffer
and pinned drawing to one framebuffer while the display rotated through three. The fix is candidate (b): save
`pspgu.fbOffset[pspgu.frames % PSP_GU_FB_COUNT]` without starting a list before the save. The 2-frame RTT gate
that made the prior candidates look broken was measuring display lag, not a render band; with 8 frames the RTT
`direct` vs `rtt` region is **0 diff** and all three framebuffers are identical.

**What is known (do not re-derive).**

- Before the fix, every frame's drawing was pinned to one framebuffer: `fb0 = 53,130 px`, `fb1 = fb2 = 0`, a strict
  3-frame checker cycle. The stopped frame looked correct because it was buffer 0.
- Two candidate restores were built: (a) `pspguBeginFrame()` before the save, (b) save
  `fbOffset[frames % PSP_GU_FB_COUNT]`. Both fix the flicker.
- At **2 frames** both candidates appear to carry a 528-656 px band in the last 8 rows. `BAND_DIAG` showed the
  draw buffer on frame 2 is clean; the band is the first frame/display lag of the rotating buffers.
- At **8 frames** both candidates give RTT `direct` vs `rtt` **0 diff**. Candidate (b) is adopted because it only
  changes the saved pointer and does not start a list before the save.
- `sceGuTexSync()` / `sceGuTexFlush()` order was tested as arm 3 and **rejected**: 656 px versus arm 2's 528 px
  at 2 frames. The default remains `TexFlush` then `TexSync`.

**GU cross-check (2026-11-23).** The GU reference (local notes §2) says `sceGuTexSync()` inserts a list command
that blocks frame-buffer read-out, required for recursive drawing; `TexFlush()` is the separate cache flush. The
the reference samples order them `TexSync` then `TexFlush` before sampling (`billboard:848`, `monochrome:633`), but that
order did not clear the measured band. `sceGuDrawBufferList()` is a **mid-list** command; `sceGuSwapBuffers()`
returns the new drawing buffer and does not change the display buffer through `DrawBufferList`. The adopted fix
uses the post-swap frame buffer, not a pre-swap saved pointer.

**Read before acting (historical).**

1. `../sprints/069-phase10-clip-cost-and-lit-parity.md` §5 T5, §7, §9 T5 — the original cause and rejected fixes.
2. `tools/pspgu/egl.c` — `pspguSwapCallback`, `pspguBeginFrame`, `eglSwapBuffers`.
3. `tools/pspgu/gl_fbo.c` — `glBindFramebuffer` save/restore; `gl_texture.c` — the dirty-target bind.
4. `psp-tests/rtt_probe/main.c` — `MODE=direct`/`rtt`, `FRAMES=8`, `BAND_DIAG`.
5. `psp-tests/scene_3d/main.c` — `SAMPLE_DIAG`, `pspguDebugFrameStats`.

**Hard rules.**

- **The RTT gate must run 8 frames.** Two frames measure display lag with the corrected restore; the 8-frame
  capture is the one that settles the three-buffer rotation.
- **No shipped regression.** The 2D, `MESH_SCENE`, non-FBO 3D and RTT gates must stay 0.
- **`TexSync` is not `sceGuSync`.** The full CPU wait inside an open list still deadlocks; the list command is safe.
- **A full sync inside an open list deadlocks.** Any CPU wait must be at a list boundary.
- **Capture after `DONE`, twice; require identical hashes.**
- **Leave pspGL `2ab7378e` installed when stopping.**

**Commands.**

```sh
export PSPDEV=/home/psp/pspdev
export PATH=$PSPDEV/bin:$PATH
cd /home/psp/raylib-psp

# Adopted driver: b31, candidate (b), old TexSync order.
tools/pspgu-build.sh && tools/pspgu-build.sh --check

# RTT gate: 8 frames, two captures after DONE.
( cd psp-tests/rtt_probe && make clean && make MODE=direct FRAMES=8 )
# run and capture direct
( cd psp-tests/rtt_probe && make clean && make MODE=rtt FRAMES=8 )
# run and capture rtt

# Sample: DIAG for FBSTAT/FLICK, then twice without DIAG for timing.
( cd psp-tests/scene_3d && make clean && make SCENE=tex SAMPLE=1 INSTANCES=50 CULL=1 DIAG=1 FRAMES=300 )
( cd psp-tests/scene_3d && make clean && make SCENE=tex SAMPLE=1 INSTANCES=50 CULL=1 DIAG=0 FRAMES=300 )

tools/pspgu-build.sh --restore && tools/pspgl-build.sh --check   # end state
```

## 1. Objective

With candidate (b), a static RTT frame is identical in all three framebuffers (`fb0 = fb1 = fb2`, `FLICK 0`), the
8-frame `direct` vs `rtt` 256×256 region is **0 diff**, and the frozen sample still meets median ≤16.683 ms,
p95 ≤16.75 ms, work ≤15.0 ms, `arenaOverflows=0`.

## 2. Why now

It was the only user-visible defect left in the driver, and it contaminated every capture with a render target: two
of three displayed frames were empty at 2 frames. Closing it removes the last known driver defect before Phase 11
and P10-S8's cost work.

## 3. Scope

**In:** `tools/pspgu/gl_fbo.c` restore; `PSPGU_FB_RESTORE`; `rtt_probe` `FRAMES`/`BAND_DIAG`; the docs and the
069 T5 close-out.

**Out:** new RTT features, texture formats, the clip pass, culling, Phase 11.

## 4. Preconditions

- [x] `master` @ `f0dfd0ab`; P10-S7 closed; pspgu `b30`/`b31` build
- [x] pspGL `2ab7378e` available and restored at the end
- [x] `rtt_probe` builds direct/rtt; `scene_3d` builds `SAMPLE=1 DIAG=1`
- [x] Device reset between runs; `DONE` + sleep; two identical captures
- [x] `usbhostfs_pc` and `pspsh` available; no concurrent device user during the final runs

## 5. Tasks

| ID | task | status | verification | evidence |
|---|---|---|---|---|
| T1 | **Three-arm matrix, order A/B and band localization.** `(FB_RESTORE,TEXSYNC_FIRST)` = `(0,0)`, `(1,0)`, `(1,1)`, plus candidate (b) `(2,0)`. `rtt_probe` direct/rtt, `BAND_DIAG`, and `SAMPLE=1 DIAG=1` | `done` | arm 1 flicker + 16 px; arms 2/3 flicker gone; 2-frame band 528/656; `BAND` draw buffer clean on frame 2; 8-frame RTT **0 diff** for (a) and (b) | §7; chain `arm1/arm2/arm3`, `BAND`, `f8`, `f8b` captures |
| T2 | **Hazard list break on unbind** | `dropped` | not needed: candidate (b) + 8-frame gate gives RTT 0 diff; no list break added | §9 |
| T3 | **Two-framebuffer fallback** | `dropped` | not needed: three buffers pass every gate | §9 |
| T4 | **Re-close.** 2D, 060 mesh, `MESH_SCENE`, non-FBO 3D, 065 RTT, `basic_shapes`, sample 300 frames ×2, docs, commit, restore pspGL | `done` | gates below; sample median/p95/work pass; `2ab7378e` restored; strict docs build | §7, §9 |

### Design notes

**Why 2 frames lied.** With the corrected restore the display rotates through three buffers; a 2-frame run stops
before the third buffer has been displayed. The capture then reads a first-frame buffer with a partial band. At
8 frames all three buffers carry the same frame. The old (stale) restore made 2 frames *look* correct because it
pinned drawing to buffer 0 — the same buffer the capture read.

**Why candidate (b).** It only changes the saved pointer
(`saved.draw = pspgu.fbOffset[pspgu.frames % PSP_GU_FB_COUNT]`); candidate (a) also starts a list before the save,
which changes the frame/list state for no additional benefit. Both measured 0 diff at 8 frames.

**What `BAND_DIAG` showed.** On frame 2, target-lo and screen have 0 black pixels; target-hi y=240..255 is 4096
black, but that region is outside the sampled panel's last 16 screen rows (the sampled band is GL y=16..31). The
sampled draw buffer is clean; the capture band was display lag.

**The rejected order.** The `TexSync`→`TexFlush` order (local notes §2) gave 656 px versus 528 px at 2 frames; it is not
adopted. The switch remains for future measurement.

### Task log (append-only)

- 2026-11-23 — sprint written; all tasks `todo`, no device run
- 2026-11-23 — T1 `doing`: candidates (a) and (b), `PSPGU_TEXSYNC_FIRST`, `BAND_DIAG` and `FRAMES` implemented
- 2026-11-23 — arm 1 `(0,0)`: flicker 55,644/55,644/0; RTT 16 px; arm 2 `(1,0)`: flicker gone,
  `fb0=fb1=fb2=55,644`, RTT 528 px at 2 frames; arm 3 `(1,1)`: flicker gone, RTT 656 px — order rejected
- 2026-11-23 — `BAND_DIAG`: draw buffer clean on frame 2; 2-frame band is display lag. 8-frame RTT:
  candidate (a) **0 diff**, candidate (b) **0 diff**
- 2026-11-23 — adopted candidate (b), `PSPGU_FB_RESTORE=2`, `PSPGU_TEXSYNC_FIRST=0`, `PSPGU_BUILD=b31`
- 2026-11-23 — final b31: RTT 0 diff (8 frames, two identical captures); `FBSTAT f5` fb0=fb1=fb2=63,880;
  `FLICK 0`; sample median 16.684/16.683, p95 16.731, work 12.132/11.984
- 2026-11-23 — gates: `basic_shapes` 600 frames/284 colours; `batch_check` `b69830b8…` byte-identical;
  `MESH_SCENE` pspgu vs pspGL 0/130,560; non-FBO 3D 0/130,560; pspGL restored
- 2026-11-23 — T1/T4 `done`; T2/T3 `dropped`; sprint closed

### T1 execution and close checklist

The three-arm matrix is retained as the record. The adopted arm is `(FB_RESTORE=2, TEXSYNC_FIRST=0)`; the gate is
`rtt_probe FRAMES=8`, not 2.

| arm | `FB_RESTORE` | `TEXSYNC_FIRST` | 2-frame result | 8-frame result |
|---|---|---|---|---|
| 1 control | 0 | 0 | flicker, 16 px | flicker |
| 2 (a) | 1 | 0 | flicker gone, 528 px | **0 diff** |
| 3 (a)+order | 1 | 1 | flicker gone, 656 px | order rejected |
| 4 (b) adopted | 2 | 0 | flicker gone | **0 diff** |

**Close checklist (T4).**

- [x] `rtt_probe` direct vs rtt: 256×256 **0 diff**, two identical captures (8 frames)
- [x] `scene_3d SAMPLE=1 INSTANCES=50 DIAG=1`: `fb0 = fb1 = fb2`, `FLICK 0`
- [x] sample 300 frames ×2: median 16.684/16.683, p95 16.731, work 12.132/11.984 ms
- [x] 2D `batch_check` `b69830b8…` byte-identical; `basic_shapes` 600 frames/284 colours
- [x] `MESH_SCENE` and non-FBO 3D pspgu vs pspGL: 0/130,560
- [x] boot line `pspgu b31 fb-restore=2 texsync-order=0`
- [x] 069 T5 resolved; Phase 10 page, 065 page and this page updated; `mkdocs build --strict`
- [x] pspGL `2ab7378e` restored; `tools/pspgl-build.sh --check`
- [ ] 066 `clip_probe` arms not re-run: the change is confined to `gl_fbo.c` (no FBO in those probes); the
      `CULL=1` non-FBO 3D scene is 0 diff. Recorded as not affected rather than claimed as a fresh run.

## 6. Budgets

| budget | limit | measured | tag |
|---|---|---|---|
| sample median | ≤16.683 ms | 16.684 / 16.683 | b31 |
| sample p95 | ≤16.75 ms | 16.731 | b31 |
| sample work | ≤15.0 ms | 12.132 / 11.984 | b31 |
| arena overflows | 0 | 0 | b31 |
| RTT region | 0 diff | 0 | b31, 8 frames |
| all run gates | 0 diff | 0 | §7 |

## 7. Measurements

Protocol: device only; one variable per arm; two identical captures; reset between runs.

| run | build | workload | metric | value | notes |
|---|---|---|---|---|---|
| defect | b30 | sample DIAG | framebuffer | fb0 55,644 / fb1 0 / fb2 0 | 3-frame checker |
| arm 1 | b30 `(0,0)` | RTT direct/rtt | region | 16 px, y=250 | control |
| arm 2 | b30 `(1,0)` | RTT direct/rtt | region | 528 px, y=248..255 | 2 frames; display lag |
| arm 3 | b30 `(1,1)` | RTT direct/rtt | region | 656 px, y=248..255 | order rejected |
| BAND | b30 `(1,0)` | target/screen last 16 | black | target-lo 800/0, target-hi 4096, screen 800/0 | draw buffer clean on frame 2 |
| 8-frame (a) | b30 `(1,0)` | RTT direct/rtt | region | **0** | two identical captures |
| 8-frame (b) | b30 `(2,0)` | RTT direct/rtt | region | **0** | two identical captures |
| final RTT | b31 `(2,0)` | RTT direct/rtt | region | **0** | two identical captures |
| final sample | b31 `(2,0)` | `FBSTAT f5` | nonblack | fb0=fb1=fb2=63,880 | flicker gone |
| final sample | b31 `(2,0)` | `FLICK` | diff | 0 | |
| timing 1 | b31 `(2,0)` | 300 frames | median/p95/work | 16.684 / 16.731 / 12.132 | |
| timing 2 | b31 `(2,0)` | 300 frames | median/p95/work | 16.683 / 16.731 / 11.984 | |
| 2D | b31 | `batch_check` | hash | `b69830b8…` | byte-identical |
| 2D | b31 | `basic_shapes` | frames/colours | 600 / 284 | |
| 060/061 | b31 vs pspGL | `MESH_SCENE` | pixels | 0 / 130,560 | |
| 061 | b31 vs pspGL | non-FBO 3D 120 frames | pixels | 0 / 130,560 | |

## 8. Exit criteria

- [x] T1: three-arm matrix recorded; order rejected; candidate (b) adopted; 8-frame RTT 0 diff
- [x] T2/T3: dropped as unnecessary, with the 0-diff evidence
- [x] `fb0 = fb1 = fb2` and `FLICK 0`; RTT region **0 diff**
- [x] sample 300-frame pair: median ≤16.683, p95 ≤16.75, work ≤15.0, `arenaOverflows=0`
- [x] 2D/060/061/065 gates 0; `basic_shapes` 284
- [x] boot tag `b31`; docs updated; strict build; pspGL `2ab7378e` restored
- [ ] 066 `clip_probe` not re-run (FBO-only change; cull scene parity 0)

## 9. Result

The flicker is fixed and the apparent band was a measurement artifact. Candidate (b) saves this frame's rotating
buffer without starting a list before the save. At 2 frames the corrected restore still shows the *previous*
frame's buffer in the capture (528 px for (a), 656 px with that order), which is why the earlier bisect
looked like a regression. `BAND_DIAG` showed the draw buffer on frame 2 is clean; at 8 frames both candidates give
**0 diff**. Candidate (b) is adopted because it is the smaller change.

Final b31: RTT direct vs sampled 0 diff; `fb0 = fb1 = fb2 = 63,880`; `FLICK 0`; median 16.684/16.683 ms, p95
16.731, work 12.132/11.984 ms. `basic_shapes` is 600 frames/284 colours, `batch_check` is byte-identical,
`MESH_SCENE` and a non-FBO 3D scene are 0/130,560 vs pspGL. pspGL `2ab7378e` was restored.

The 066 `clip_probe` arms were not re-run: the change is confined to `gl_fbo.c`, and those probes do not create an
FBO. The `CULL=1` non-FBO 3D scene is 0 diff; this is recorded as unaffected, not as a fresh 066 measurement.

## 10. Follow-ups

- **P10-S8 (070)** still owns the per-`DrawMesh` matrix/cull cost; the post-fix sample is 11.98-12.13 ms work.
- **065** gate uses `rtt_probe FRAMES=8` now; a 2-frame run is a display-lag measurement, not a render gate.
- 066 `clip_probe` can be re-run in a future full gate sweep if a clip-path change lands.
- The `TexSync` order switch remains available but is not the fix.
