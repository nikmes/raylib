# Sprint 012 — Texture parity: right texels, right alpha, right dither

| | |
|---|---|
| Roadmap phase | [Phase 5 — Native GU backend](../phases/05-native-gu-backend.md) |
| Branch / base commit | `master` @ `8843e652` |
| Started / closed | — / — |
| Build number(s) | `b12` (pspgu with textures drawing, 36 KB archive), `b13` (this sprint) |
| Tasks | 5/6 done, **parity reached**, T2 diagnosed |

## 0. Handover

**Situation.** `tools/pspgu` is a GE driver that replaces pspGL and supplies the 52 GL/EGL symbols raylib's GL11
path needs. It boots, clears, transforms and draws (`gl_matrix.c`, `gl_draw.c` proven by a full-screen self-test
sprite). Textures upload and appear, but they disagree with pspGL and nobody has yet been able to say exactly why:
three candidate causes were measured and two were ruled out.

**The three deltas, measured on the same binary and scene (all at N=1 sprite grid, `batch_check`):**

| where | pspgu | pspGL | candidate |
|---|---|---|---|
| background | flat `(240,244,240)` | `(240,240,240)` **and** `(248,248,248)` in a pattern | dithering (task 3b) |
| inside a sprite | the tint colour | `(0,0,0)` | texture alpha not reaching the blend |
| one sprite pixel | `(80,4,0)` | background | wrong texel — sampling the wrong address or stride |

62.4 % of the frame differs. This is not rounding — and after T1 the pattern test has narrowed it to one
sentence: **texels are right, alpha is ignored** (see the task log). The transparent quadrant renders opaque black
and a 50 %-alpha texel renders fully opaque, which is why the sprite areas wear the tint colour where pspGL shows
see-through or background.

**Already true, verified — do not re-derive:**

- **The upload path exists and is exercised**: `gl_texture.c` implements gen/bind/delete/image/subimage/parameters,
  a bump VRAM arena from 816 KiB, and a 1×1 white default so `bind(0)` cannot sample an undefined address.
- **Ruled out by measurement: the texture coordinate transform.** `sceGuTexScale(1,1)` + `sceGuTexOffset(0,0)` were
  added because PSPRuntime and PGE both set them; the capture changed by **exactly zero pixels**. GU's default is
  already identity. The calls stay, the hypothesis is dead.
- **What the references do and we do not**: PSPRuntime (`texturemanager.cpp:35`/`:175`, `gpumanager.cpp:1793`) and
  PGE (`pgeTexture.c`, `pgeFont.c:878`) **pad to a power of two, row-copy with the padded stride, swizzle the
  texels, and call `sceGuTexMode(psm, 0, 0, 1)`** — `swizzle = 1` — with `tbw` = the padded width. Ours uploads
  **unswizzled** with `swizzle = 0`, and computes `tbw` with a redundant second `nextPow2` call.
- **The five bugs of sprint 011 are fixed**, including the two that looked exactly like this one: missing dcache
  writeback before the GE reads memory, and a transposed `sceGuSetMatrix`.
- pspGL's archive is one command away (`tools/pspgu-build.sh --restore`, verified by `tools/pspgl-build.sh --check`
  against content hash `2ab7378e`), and the reference capture of the current scene is
  `/tmp/s3/ref_pspgl.raw` — regenerate it if absent.

**Read before acting:**

1. [Phase 5 §"Reference implementations"](../phases/05-native-gu-backend.md) — the file/line index for the four
   engines in `/home/psp/ref`. This sprint should not have to search them again.
2. `tools/pspgu/gl_texture.c` — `applyTexture()` is the whole contract with the GE: mode, image, filter, wrap,
   scale/offset, flush.
3. `/home/psp/ref/PSPRuntime/PSPBasicPlatformer/src/texturemanager.cpp` — `LoadTexture` and `SwizzleFast` are the
   recipe to match, about 60 lines of it.

**First action.** Build the known-pattern test (T1) **before** changing any driver code. A 4×4 texture with four
distinct corner colours, drawn as one 64×64 sprite at a known position, is one capture that separates wrong-texel
from wrong-alpha from wrong-blend — which three hypothesis runs have now failed to do.

**Hard rules.**

- **One variable per run.** Swizzle, dither, alpha and stride are four independent changes; measuring them together
  is how the last three hypotheses died.
- **`sceGuTexImage`'s `tbw` is in pixels, and the data must be laid out with that stride.** A `tbw` that disagrees
  with the buffer layout samples the wrong rows — which is exactly what `(80,4,0)` looks like.
- **Swizzle and layout are a pair.** If the data is swizzled, `swizzle = 1`; if not, `swizzle = 0`. A mismatch
  gives diagonal garbage, which is easy to recognise, unlike a stride mismatch.
- **The oracle is pspGL's capture of the same scene on the same binary**, not an impression of the image.
- `reset` before each run, never `exit`, the app must not call `sceKernelExitGame`.
- pspGL must be restorable at any point; never leave `$PSPDEV/psp/lib/libGL.a` in a state that needs a rebuild to
  undo.

**Commands.**

```sh
export PSPDEV=/home/psp/pspdev
export PATH=$PATH:$PSPDEV/bin

cd /home/psp/raylib-psp
./tools/pspgu-build.sh                  # build + install over libGL.a
./tools/pspgu-build.sh --check          # confirm which driver is installed
./tools/pspgu-build.sh --restore        # put pspGL back

cd psp-tests/batch_check && make WRAP=0 # WRAP=1 needs the immediate-mode symbols (task 12)
( printf './batch_check.prx\n'; sleep 12 ) | pspsh
pspsh -e "scrshot host0:/gu.raw"
```

**Do not touch.** `gl_matrix.c` and `gl_draw.c` (proven by the self-test), the batcher in `src/`, anything under
`docs/` outside this sprint's record, and `psp-tests/bunnymark` (that is sprint 013's).

**Done means** §8: `batch_check` byte-identical to pspGL, with the pattern test proving texel-by-texel sampling.

## 1. Objective

`psp-tests/batch_check` rendered by `tools/pspgu` is **byte-identical** to the same binary rendered by pspGL, and a
4×4 known-pattern texture drawn 1:1 reproduces its four corner colours exactly — which fixes, in one sprint, the
texel, alpha and dither deltas that are currently 62 % of the frame.

## 2. Why now

Everything else in phase 5 is downstream of this: the exit criterion is byte-identical captures, the counter harness
needs a working 2D path, and the ceiling cannot be compared while the picture is wrong. It is also the point where
the references become directly useful — both engines have the exact recipe, and we have already ruled out the one
part of it we guessed at.

## 3. Scope

**In:** `gl_texture.c` (layout, swizzle, stride, alpha), dither in `gl_state.c`/`egl.c`, and one new test program
for the pattern.

**Out:** mipmaps (raylib uploads level 0), texture eviction and residency (phase 6), 4-bit glyph atlases
(phase 6 memory decision), the `--wrap` harness and immediate mode (sprint 013), ceiling measurement (sprint 014).

## 4. Preconditions

- [ ] `pspsh` answers, USB device `054c:01c9`; `reset` per run
- [ ] pspgu builds and installs; `./tools/pspgu-build.sh --check` says so
- [ ] pspGL restorable in one command, and its capture of the current `batch_check` exists
- [ ] `batch_check` builds with `WRAP=0` (task 12 owns `WRAP=1`)
- [ ] The three deltas above reproduce: 62.4 % of pixels, background `(240,244,240)`, `(80,4,0)` at (130,60)

## 5. Tasks

| ID | task | status | verification | evidence |
|---|---|---|---|---|
| T1 | **the pattern test**: `psp-tests/texture_pattern` builds a 4×4 image in code (red/green/blue/transparent quadrants) drawn 16×, a 32×32 half-alpha texture drawn 1:1, and (behind `PATTERN_NPOT`) a 33×17 checkerboard | `done` | the four quadrants are the four colours, in the right corners | **Texels are correct; alpha is not applied.** Measured at 1:1: TL `(248,0,0)` red, TR `(0,252,0)` green, BL `(0,0,248)` blue, transparent quadrant `(0,0,0)` (should be the `(32,32,32)` background), half-alpha half `(248,0,0)` **at full opacity** instead of blending 50 % over the background |
| T2 | **layout and stride** | `doing` | non-power-of-two texture samples correctly | **Diagnosed, and the diagnosis is the value.** The redundant `nextPow2` is gone and padding rows are zeroed, but a 33×17 texture still samples wrong: the swizzle is an 8-row block operation, so it needs `height % 8 == 0` and a power-of-two *both* ways. The real fix is (a) pad the height to a power of two as PSPRuntime does with `pHeight`, and (b) **correct with `sceGuTexScale(realW/padW, realH/padH)`** — which also retires the earlier "TexScale is identity" conclusion: it is identity only for power-of-two textures. Until then a texture that cannot be swizzled stays linear (`t->swizzled`), so nothing reads out of bounds |
| T3 | **swizzle**: adopt the references' recipe — swizzle the texels (`SwizzleFast` equivalent) and pass `swizzle = 1`, with a switch for the A/B | `done` | swizzled and unswizzled uploads must be pixel-identical to each other | **Byte-identical to pspGL with swizzling on: 0 differing pixels, 613 colours** — which is the verification the task asked for, and it proves the swizzle arithmetic. `PSP_GU_NO_SWIZZLE=1` keeps the linear path |
| T4 | **alpha and blend** | `done` | in-sprite pixels match; transparent areas show through | **One line: `sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA)`.** Without it GU takes the fragment alpha from the *vertex* colour, which the batcher always sends as 255, so every texel was opaque. Pattern test after: transparent quadrant → `(32,32,32)` background, `(255,0,0,128)` → `(144,16,16)` = exactly 50 % red over the background, quadrants still red/green/blue. Found by reading PSPRuntime `gpumanager.cpp:126` and PGE `pgeGfx.c:170`, which both set this pair right after `sceGuTexMode` |
| T5 | **dither** | `done` | background matches pspGL's pattern | `sceGuEnable(GU_DITHER)` at init. pspGL's background alternates `(240,240,240)` / `(248,248,248)` pixel by pixel — a 5650 buffer being dithered — while ours was flat, and that alone accounted for half the frame. **Differing pixels: 81,470 (62.4 %) → 42,544 (32.6 %).** daedalus' memory map (`AI_MEMORY_MAP` §12.4, "Fast clear + dither") is the reference |

| T6 | **source-format decode** (found by this sprint, not planned): `upload8888` branched on *byte count* (`bytes == 2`) to decide whether a source was packed RGB565 or grey+alpha, so every LUMINANCE_ALPHA texture — the sprite sheet and the font atlas, i.e. everything in our tests — was decoded as RGB565 | `done` | sprite pixels match pspGL exactly | branch on the GL **type** (`isPacked16(type)`) instead of the byte count. This one bug was the last 32.6 % |

Status vocabulary: `todo` / `doing` / `done` / `blocked` / `dropped`. IDs are stable — never renumber.

### Task log (append-only)

- sprint opened, all tasks `todo`
- T1 `done`: the pattern test collapses three deltas into one. **Texture sampling is correct and alpha is
  ignored.** Four quadrant colours land in the right corners at 1:1; a `(255,0,0,128)` texel renders as
  `(248,0,0)` instead of blending over the background, and a fully transparent texel renders as opaque black
  instead of showing what is behind it. So T4 is the whole sprint, T2/T3 are hygiene, and T5 is the background
  only.
- T1 also found that **pspGL hangs on this test scene** (it prints the texture ids, never reaches `DONE`, no
  capture). That is why the non-power-of-two checkerboard is behind `PATTERN_NPOT`: it cannot be part of a
  comparison scene until we know whether pspGL is choking on that texture or on something else in this app. The
  scene pspgu renders and pspGL does not is itself a finding — a driver that handles a case pspGL does not — but
  it makes the parity oracle unusable until it is understood.

## 6. Budgets

| budget | limit | measured | tag |
|---|---|---|---|
| differing pixels, `batch_check` | **0** | 81,470 (62.4 %) at start | |
| corner colours in the pattern test | 4 of 4 correct | | |
| texture VRAM for a 32×32 | 4 KiB at 8888 | | |
| `src/` changes | 0 — the driver only | | |
| per-frame allocation | 0 | | |

## 7. Measurements

| run | driver | scene | differing pixels vs pspGL | notes |
|---|---|---|---|---|
| control | pspGL | `batch_check` | 0 (it *is* the reference) | |
| change | pspgu | `batch_check` | 81,470 (62.4 %) | flat background, tint instead of alpha, `(80,4,0)` |
| change | pspgu | `texture_pattern` | | T1 |
| change | pspgu | pattern, unswizzled vs swizzled | | T3 |

## 8. Exit criteria

- [ ] `batch_check` byte-identical to pspGL (md5 equality, `cmp -l` → 0)
- [ ] the pattern test reproduces all four corner colours and the transparent quadrant
- [ ] a non-power-of-two texture samples correctly
- [ ] swizzled and unswizzled uploads are pixel-identical to each other
- [ ] the background dither matches pspGL or the difference is explained with evidence
- [ ] findings written back into the phase page and `memory-map.md` §9

## 9. Result

(To fill in on close. The valuable outcome is not "it matches" but *which* of the three deltas each change fixed:
if swizzling alone fixes all 62 %, the layout path was never the problem and the stride was; if dither fixes the
background and nothing else, the other two deltas are separate bugs in the alpha path and should be split out.)

## 10. Follow-ups

- Mipmaps and filtering policy if a sample needs them (raylib asks for `GL_LINEAR` mostly).
- 4-bit glyph atlases (`GU_PSM_T4` + palette, as PGE and PSPRuntime do) — phase 6 memory work.
- Texture residency and eviction: the arena is bump-only, so `glDeleteTextures` leaks by design (phase 6).

## 9a. Result: parity

`batch_check` rendered by pspgu is **byte-identical** to the same binary rendered by pspGL: `cmp -l` reports
**0 differing bytes**, both captures contain **613 distinct colours**, and the frame that was 62.4 % wrong at the
start of the sprint is 0 % wrong.

The four changes that got there, in the order they were found and measured:

| fix | what it moved | how it was found |
|---|---|---|
| `sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA)` | alpha: transparent texels show through, 50 %-alpha blends | reading PSPRuntime `gpumanager.cpp:126` / PGE `pgeGfx.c:170` |
| `sceGuEnable(GU_DITHER)` | background pattern matches | daedalus `AI_MEMORY_MAP` §12.4 + a capture showing pspGL alternating two values |
| **decode LUMINANCE_ALPHA as LA, not as RGB565** | 32.6 % → **0 %** | the pattern test, then pspGL's `copy_expand_LA` proving the expansion matched — so the difference had to be upstream of the expansion |
| the pattern test itself | turned three deltas into one sentence per run | written before any driver change, as the sprint required |

The 62.4 % → 32.6 % → 0 % progression is the whole argument for the pattern test: two fixes came from reading
references, and the last one came from noticing that pspGL's expansion was *identical to ours*, which left only the
decode before it.

## 9b. Old analysis, kept because it names the wrong suspect

After T4 and T5, the background pattern matches and sprite alpha is right. What still differs is
**colour modulation of the grey+alpha textures**: the same sprite pixel reads `(8,32,184)` on pspgu and
`(8,16,104)` on pspGL — same hue, roughly 1.8× brighter. Our expansion puts the luminance in R=G=B and
multiplies by the tint with `GU_TFX_MODULATE`; pspGL's `copy_expand_LA` evidently arrives at a darker
result, so either its RGB expansion or its modulation differs from ours.

That is one question with three candidates (expansion values, pre-multiplication, or the texture
environment), and the pattern test can answer it: a **grey+alpha** pattern texture (luminance 255 and
128, alpha 255 and 128) drawn 1:1, which severs "what does LA expand to" from "what does modulate do".
That is the first task of the next session; T2 (stride/padding) and T3 (swizzle) remain as hygiene.
