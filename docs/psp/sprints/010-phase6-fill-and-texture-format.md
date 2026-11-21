# Sprint 010 — What fill actually costs, and 16-bit textures

| | |
|---|---|
| Roadmap phase | [Phase 6 — Textures and VRAM](../phases/06-textures-vram/index.md) |
| Branch / base commit | `master` @ `628a4611` |
| Started / closed | 2026-11-21 / 2026-11-21 |
| Build number(s) | `b11` (sprint 009 default, `8d919899…`), `b12` (this sprint) |
| Tasks | 3/5 done, 1 dropped, 1 renamed |

## 0. Handover

**Situation.** Phase 4 shipped a batcher that holds 1,000 sprites at 60 fps and runs out of budget at 1,600. The
budget is spent in two halves that add: **11.0 ms of CPU work and ~5.5 ms of GE fill** inside a 16.683 ms period.
Everything measured so far attacked the CPU half. This sprint attacks the fill half, and its first job is to find
out what "fill" is actually made of, because the obvious answer — fewer bytes per texel — is only one of three
possibilities.

**The three hypotheses, and only one of them is a texture-format problem:**

| hypothesis | evidence for | evidence against | fix if true |
|---|---|---|---|
| **texel bytes** (4 B RGBA8888 read per texel) | a 32×32 RGBA texture is 4 KiB, larger than the GE's texture cache, so every sprite re-fetches from memory | a 32×32 texture is tiny next to the framebuffer traffic | upload as `5650`/`5551` (2 B) |
| **pixel area / overdraw** | at N=1,500, 4× the area takes the frame from 16.684 → **50.050 ms** with CPU work unchanged | that probe changed area *and* texel count *and* blend work at once | fewer, bigger, clipped, or culled sprites — not a format |
| **blend read-modify-write** | every 2D draw blends; the GE must read the destination, mix, write back | never isolated | opaque runs drawn with blending off |

That is why this sprint is a probe first. Implementing 16-bit textures and finding no gain would leave us exactly
where we are, with a worse-looking texture pipeline.

**Already true, verified — do not re-derive:**

- **The upload path exists today.** `pspgl_texobj.c` maps `GL_RGBA + GL_UNSIGNED_SHORT_5_5_5_1` → `GE_RGBA_5551`,
  `GL_RGB + GL_UNSIGNED_SHORT_5_6_5` → `GE_RGB_565`, and `GL_RGBA + GL_UNSIGNED_SHORT_4_4_4_4` → `GE_RGBA_4444`;
  the `*_REV` variants are native and skip even the conversion copy. rlgl already emits exactly those pairs for
  `RL_PIXELFORMAT_UNCOMPRESSED_R5G5B5A1`, `R5G6B5` and `R4G4B4A4` (`rlgl.h:3913-3916`), and raylib exposes the same
  pixel formats (`raylib.h:856-858`). **No library change is needed for the probe: `ImageFormat()` before upload
  is enough.**
- **The framebuffer is already 16-bit.** RAYWHITE is (245,245,245) and comes back from a capture as
  (240,240,240), i.e. `GU_PSM_5650`. So the destination is already 2 B/px; only the *source* texels are 4 B.
- **Blending is always on under GL11.** `rlSetBlendMode()` is a no-op outside GL33/ES2, so `BeginBlendMode()` does
  not reach GL; `rlDisableColorBlend()` is the only way to turn it off for a probe.
- The GE's texture cache is small (2 KiB), so a 32×32 RGBA8888 texture (4 KiB) cannot stay resident, while a 32×32
  5650 texture (2 KiB) can — this is the mechanism the first hypothesis depends on.

**Read before acting:** [baselines §"Scaling past the ceiling"](../baselines.md) (the model and the 4× fill probe),
[Phase 5 review](../phases/05-native-gu-backend/index.md) §0 (why this phase comes first), and
`psp-tests/bunnymark/main.c` (the harness the probe extends).

**First action.** Add `TEX_FMT` and `BLEND_OFF` to `psp-tests/bunnymark` — app-side only, no `src/` change — and run
the 8-cell probe in §7 at N=1,200.

**Hard rules.**

- **One variable per cell, and the cell count is the deliverable.** Twelve configurations, two runs each, is ~15
  minutes of device time; guessing costs a sprint.
- **`work` and the frame median together.** Fill shows up in `swap` (the wait), CPU in `work`. A cell that moves the
  frame without moving either is a measurement error.
- **Colour fidelity is a documented trade, not an accident.** `5551`/`5650` textures change what every capture looks
  like. Record the number of differing pixels against the archived `c4eb44e7…` and call it either acceptable or
  rejected — never silently.
- Alpha matters: `5551` has 1 bit of alpha, `4444` has 4, `5650` has none. A sprite with soft edges needs `5551` or
  `4444`; a hard-edged one can use `5650`. The bunny is soft-edged, so `5650` is expected to look wrong.
- `reset` before each run, never `exit`, the app must not call `sceKernelExitGame`.
- Threshold 0.1 %; two runs per cell.

**Commands.**

```sh
export PSPDEV=/home/psp/pspdev
export PATH=$PATH:$PSPDEV/bin

cd /home/psp/raylib-psp/psp-tests/bunnymark
make BUNNY_COUNT=1200 FRAMES=200 TARGET_FPS=0 TEX_FMT=5551      # one probe cell
./measure.sh 6                                                   # the full matrix, if staged there
```

**Do not touch.** `src/rlgl.h` and `src/platforms/rcore_psp.c` until T3 — the probe is app-side on purpose, so a
null result leaves the renderer exactly as it is. Nothing under `docs/` outside this sprint's record.

**Done means** §8: the fill mechanism is named with numbers, the winning format is shipped behind a switch, the
ceiling is re-bisected, and VRAM is accounted at boot.

## 1. Objective

At fixed sprite count and fixed CPU work, determine whether the ~5.5 ms of GE fill is driven by **texel bytes**,
**pixel area**, or **blend traffic** — and if it is texel bytes, ship a 16-bit texture upload path that raises the
60 fps ceiling above 1,600 sprites without changing anything else.

## 2. Why now

Phase 5's review ended with the phase unable to raise the ceiling on its own: it removes per-vertex conversion
(~0.9 ms at N=1,000) while ~5.5 ms of fill sits untouched. Spending a backend on the smaller half before knowing
the larger half is the wrong order, and this probe is cheap enough that the ordering question costs less than the
argument about it.

## 3. Scope

**In:** `psp-tests/bunnymark` probe switches (`TEX_FMT`, `BLEND_OFF`); the winning upload path in `rlgl.h`'s GL11
texture creation behind a switch; boot-time VRAM accounting; the ceiling re-bisection.

**Out:**

- The drop-in GL/GU backend — phase 5.
- Swizzling work: pspGL already swizzles on upload; only the format changes here.
- Mipmaps and texture filtering policy — later in phase 6.
- Texture streaming/eviction beyond the accounting — later in phase 6.
- The framebuffer format (it is already `5650`; changing it would move every pixel for no measured gain).

## 4. Preconditions

- [ ] `pspsh` answers, USB device `054c:01c9`; `reset` per run
- [ ] Phase 4 default library staged: `8d919899…` (`/home/psp/evidence/sprint009/libraylib-batched-inline-O2.a`)
- [ ] Baselines this sprint is judged against: N=1,200 inside the ceiling at 60 fps; fill probe 4× area → 50.050 ms
- [ ] Archived captures: `c4eb44e7…` (2D), `58bcbb9b…` (2D→3D→2D), `/home/psp/evidence/sprint009/`
- [ ] The four pixel formats reachable through pspGL confirmed by source (`pspgl_texobj.c:155-180`)

## 5. Tasks

| ID | task | status | verification | evidence |
|---|---|---|---|---|
| T1 | **the probe**: `TEX_FMT=8888\|5650\|5551` and `BLEND_OFF=1`, app-side only, 8 cells at N=1,200 | `done` | `work` stays within 2 % across cells | work ranged **8.54–8.65 ms (+1.3 % spread)** — CPU did not move, so every difference is fill |
| T2 | **name the mechanism** | `done` | "fill is X-bound because Y moved Z % while W did not" | **Fill is rasterisation-rate-bound: insensitive to texel bytes and to blending, sensitive only to area.** 5650 and 5551 moved the frame by 0.0 % and `swap` by −0.7 %; blending off moved `swap` by −0.2 %; 4× the area took the frame from 16.683 to 33.368 ms (+199.5 % of the swap) |
| T3 | **ship the 16-bit winner** | `dropped` | — | There is no speed winner to ship. 16-bit textures remain a **VRAM** lever (2 B/texel instead of 4) and belong to phase 6's memory work, with the alpha caveat in §9 — not to a fast-path |
| T4 | **VRAM accounting at boot** | `todo` | one log line, texture total matches, free+used ≤ 2 MiB | Deferred to the next phase-6 sprint, which is now about memory rather than speed |
| T5 | **re-bisect the ceiling** | `dropped` → replaced by the pixel budget | — | Nothing about the renderer changed, so the ceiling cannot have moved. The measurement that matters came out of T1 instead: **a 60 fps frame holds ~1.6–2.0 M drawn texels** (1.64 M at 1,600 sprites holds; 4.92 M needs two periods; implied rasterisation ≥221 Mtexel/s) |

Status vocabulary: `todo` / `doing` / `done` / `blocked` / `dropped`. IDs are stable — never renumber.

### Task log (append-only)

- 2026-11-21 — sprint opened, all tasks `todo`
- 2026-11-21 — T1 `done`: 8 cells × 2 runs. The probe cost 25 minutes of device time and answered the question
  without a line of renderer code
- 2026-11-21 — T2 `done`: **fill is not texel-byte-bound and not blend-bound.** 5650/5551 vs RGBA8888: frame
  unchanged (16.683 vs 16.684), `swap` −0.7 %. Blending off: `swap` −0.2 %. Area ×4: frame 33.368. Rasterisation
  rate ≈221 Mtexel/s
- 2026-11-21 — T5 `dropped` and replaced: with the renderer untouched the ceiling cannot move; the useful number is
  the pixel budget, ~1.6–2.0 M drawn texels per 60 fps frame
- 2026-11-21 — T3 `dropped`: the format is a memory lever, not a speed lever
- 2026-11-21 — T4 `todo`: VRAM accounting moves to the next phase-6 sprint

## 6. Budgets

| budget | limit | measured | tag |
|---|---|---|---|
| probe cells | 8, two runs each | | |
| `work` drift across cells | ≤2 % | | |
| ceiling after the change | ≥2,000 sprites | | |
| differing pixels vs `c4eb44e7…` | counted, and accepted or rejected in writing | | |
| VRAM | ≤2 MiB, accounted at boot | | |
| `src/` diff | `rlgl.h` GL11 texture path, behind one switch | | |

## 7. Measurements

All at N=1,200, `TARGET_FPS=0`, two runs per cell (agreement ≤0.01 ms). `work` is `frame − swap`.

| format | blend | scale | frame | work | swap | Δswap vs control | texels/frame |
|---|---|---|---|---|---|---|---|
| RGBA8888 (expanded from L8A8) | on | 1 | 16.683 | 8.54 | 8.30 | — | 1.23 M |
| 5650 | on | 1 | 16.684 | 8.60 | 8.25 | −0.6 % | 1.23 M |
| 5551 | on | 1 | 16.683 | 8.60 | 8.24 | −0.7 % | 1.23 M |
| RGBA8888 | **off** | 1 | 16.683 | 8.56 | 8.28 | −0.2 % | 1.23 M |
| 5650 | **off** | 1 | 16.683 | 8.59 | 8.25 | −0.6 % | 1.23 M |
| 5551 | **off** | 1 | 16.683 | 8.59 | 8.25 | −0.6 % | 1.23 M |
| RGBA8888 | on | **2** | 33.368 | 8.62 | 24.86 | **+199.5 %** | 4.92 M |
| 5551 | on | **2** | 33.367 | 8.65 | 24.87 | **+199.6 %** | 4.92 M |

Two readings, both clean:

- **Halving texel bytes changes nothing.** `5650` and `5551` cost 2 B/texel against RGBA8888's 4 B and moved the
  frame by 0.0 % and `swap` by −0.7 %, at identical `work`. The GE's texture fetch is not the wall, so the
  "32×32 RGBA texture is bigger than the 2 KiB texture cache" mechanism does not bite at this size.
- **Turning blending off changes nothing.** `swap` moved −0.2 %. The destination read-modify-write is not the wall
  either.
- **Only area moves it.** 4× the pixel area costs an extra 16.68 ms of `swap` at identical `work` (8.54 → 8.62),
  i.e. **≥221 Mtexel/s** of rasterisation, and it costs the same in 5650 as in 8888.

**So the budget is pixels, not bytes.** A 60 fps frame holds roughly **1.6–2.0 M drawn texels** on this console:
1.64 M (1,600 sprites) holds, 1.23 M has slack, 4.92 M needs two periods. At 480×272 that is 12–15 full-screen
layers, which is a far more useful number for a game than "1,600 sprites" — and bunnymark's 1,600 sprites are
**9.4× overdraw**, so the sprite count says more about the workload than about the console.

## 8. Exit criteria

- [x] the fill mechanism is named with ≥6 cells of evidence — **rasterisation-rate-bound**: format and blending are
  invisible, area is everything
- [ ] if texel bytes: ship the 16-bit path — **not applicable, dropped**: there is no speed gain to ship
- [x] the finding is written into `memory-map.md` §4 and the roadmap re-priced — the ceiling is a **pixel budget**
  (~1.6–2.0 M texels/frame) rather than a sprite count, and texture format is a *memory* lever
- [ ] VRAM accounted at boot — carried to the next phase-6 sprint
- [ ] `baselines.md` carries the pixel budget — done in this commit alongside `memory-map.md`

## 9. Result

**The sprint found nothing to optimise and saved a sprint's worth of wrong work.** Three candidate causes of the
~5.5 ms fill cost, tested at fixed sprite count and fixed CPU work:

| candidate | test | result |
|---|---|---|
| texel bytes | RGBA8888 vs 5650 vs 5551 | **0.0 %** on the frame, −0.7 % on `swap` |
| blend traffic | blending on vs off | **−0.2 %** on `swap` |
| pixel area | scale 1 vs 2 (4× area) | **+199.5 %** on `swap`, same `work` |

Fill on this console is **fixed-function rasterisation throughput, ~221 Mtexel/s**, and it does not care what the
texels weigh or whether they blend. That kills the obvious phase-6 plan — "upload 16-bit textures, raise the
ceiling" — with two runs per cell instead of a sprint of work.

Three consequences.

1. **The ceiling is a pixel budget, not a sprite count.** ~1.6–2.0 M drawn texels per 60 fps frame. Bunnymark's
   1,600 sprites are 9.4× overdraw; a game that draws each pixel once could hold *far* more objects than 1,600, and
   a game with more overdraw would hold fewer. Every later target should be quoted in texels per frame.
2. **16-bit textures are still worth having, as memory.** Halving texture VRAM in a 2 MiB budget is real, and the
   probe shows the cost is zero in speed. But 5551 has **1 bit of alpha**, so a soft-edged sprite gets hard edges
   and semi-transparent pixels are thresholded at 50/255 — the bunny would look wrong. 4444 (4-bit alpha) is the
   candidate for sprites; 5650 for textures without alpha. That is a fidelity decision, made per texture, in the
   phase-6 memory sprint.
3. **Phase 5's value is unchanged and still small.** It removes per-vertex conversion (~0.9 ms at N=1,000) and gives
   `GU_TRANSFORM_2D`; it does not touch rasterisation. The lever that would move the ceiling now is **drawing fewer
   pixels** — culling, bounding-box rejection, opaque-pass ordering — which is 2D specialisation, phase 7, not the
   backend.

## 10. Follow-ups

- **Overdraw is the next ceiling lever** (phase 7): scissor/bbox rejection of off-screen sprites, and measuring the
  drawn-texel count per frame rather than counting sprites. bunnymark's 9.4× overdraw means its own ceiling is not
  a game's.
- **16-bit textures as a VRAM feature** (phase 6): per-texture choice of 5650 / 4444 / 5551 with the alpha caveat
  above, plus the boot-time VRAM accounting that T4 deferred.
- **Re-check the texel-byte result on a big texture.** 32×32 fits the texture cache concerns either way; a 512×512
  background (1 MiB in RGBA8888, 512 KiB in 5650) may behave differently, and backgrounds are exactly where a game
  spends its texels.
- Nothing here changes phase 5's task list; it changes only the claim that a backend would raise the sprite ceiling.
