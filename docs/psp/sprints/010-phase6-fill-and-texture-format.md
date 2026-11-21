# Sprint 010 — What fill actually costs, and 16-bit textures

| | |
|---|---|
| Roadmap phase | [Phase 6 — Textures and VRAM](../phases/06-textures-vram.md) |
| Branch / base commit | `master` @ `628a4611` |
| Started / closed | — / — |
| Build number(s) | `b11` (sprint 009 default, `8d919899…`), `b12` (this sprint) |
| Tasks | 0/5 done |

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
[Phase 5 review](../phases/05-native-gu-backend.md) §0 (why this phase comes first), and
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
| T1 | **the probe**: `TEX_FMT=8888\|5650\|5551` (via `ImageFormat()` before upload) and `BLEND_OFF=1` (`rlDisableColorBlend`), app-side only. Run the 8 cells at N=1,200: 3 formats × blend on, 3 × blend off, plus `SPRITE_SCALE=2` for 8888 and 5551 | `todo` | every cell's `work` stays within 2 % of 7.9 ms (N=1,200 CPU is unchanged by format) while the frame or `swap` moves — i.e. the effect is fill, not CPU | — |
| T2 | **name the mechanism** and write it down before implementing anything: which hypothesis the cells support, with the numbers | `todo` | a sentence of the form "fill is X-bound because Y moved Z % while W did not", backed by ≥6 cells | — |
| T3 | **ship the winner** in `rlgl.h`'s GL11 texture path behind `PSP_TEX_16BIT` (default off until measured), converting at upload rather than per draw | `todo` | `batch_check` renders, and the differing-pixel count against `c4eb44e7…` is recorded; 2× fewer bytes per texel confirmed by the boot-time accounting | — |
| T4 | **VRAM accounting at boot**: framebuffers (two × 512×272×2), textures (sum of upload sizes), free and largest free block, printed once | `todo` | one log line whose texture total matches the sum of what was loaded; free+used ≤ 2 MiB | — |
| T5 | **re-bisect the ceiling** with the winning configuration, and state the new limit | `todo` | last N at 60 fps, two runs, bracketed; compared against 1,600 | — |

Status vocabulary: `todo` / `doing` / `done` / `blocked` / `dropped`. IDs are stable — never renumber.

### Task log (append-only)

- _(not started)_

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

All at N=1,200, `TARGET_FPS=0`, two runs; `work` is `frame − swap`.

| format | blend | scale | frame | work | swap | texels/frame | note |
|---|---|---|---|---|---|---|---|
| RGBA8888 | on | 1 | | | | 1.23 M | the control: today's path |
| RGBA8888 | off | 1 | | | | 1.23 M | blend hypothesis |
| 5650 | on | 1 | | | | 1.23 M | texel-bytes hypothesis, no alpha |
| 5650 | off | 1 | | | | 1.23 M | |
| 5551 | on | 1 | | | | 1.23 M | texel bytes with alpha |
| 5551 | off | 1 | | | | 1.23 M | |
| RGBA8888 | on | 2 | | | | 4.92 M | area hypothesis |
| 5551 | on | 2 | | | | 4.92 M | area × bytes |

Ceiling re-bisection after the change: N = 1,600 / 2,000 / 2,400, two runs.

## 8. Exit criteria

- [ ] the fill mechanism is named with ≥6 cells of evidence, and the naming survives the numbers (no "it depends")
- [ ] if texel bytes: the 16-bit path is shipped behind a switch, pixel diff recorded, ceiling re-bisected above 1,600
- [ ] if area or blend: the finding is written into `memory-map.md` §4 and the roadmap re-priced — phase 5's worth
      depends on it, and so does every later sprite-count target
- [ ] VRAM accounted at boot within the 2 MiB budget
- [ ] `baselines.md` carries the new ceiling, or an explicit statement that the old one stands and why

## 9. Result

(To fill in on close. The most valuable outcome is a *negative* result that is specific: "5650 textures changed the
frame by 0.4 % at equal pixel area, so fill is not texel-bandwidth-bound, and the 4×-area probe's 3× frame says it
is pixel/blend-bound" would redirect the whole roadmap away from texture formats and toward overdraw — which is a
different sprint and a different phase.)

## 10. Follow-ups

- Overdraw reduction (culling, atlas packing, opaque passes drawn without blending) if T2 says area/blend.
- Texture cache and swizzle policy, mipmaps, and eviction under VRAM pressure — the rest of phase 6.
- The 4 B→2 B vertex question is separate and lives in phase 5/7: the vertex format is what `GU_TRANSFORM_2D` and
  16-byte vertices would halve.
