# Sprint 071 (P11-S9) — Image and texture data conversion: doubles to float and VFPU packed pixels

| | |
|---|---|
| Roadmap phase | [Phase 11 — General optimisation outside the renderer](../phases/11-general-optimisation.md) |
| Sprint | P11-S9 |
| Branch / base commit | `master` @ `816a8c25` (pin at start) |
| Started / closed | 2026-11-22 (written) / — |
| Build number(s) | pspgu as installed (`--check` content hash recorded); `libraylib.a` with `PSP_IMAGE_VFPU=0|1` |
| Tasks | 0/5 done |

## 0. Handover

**Situation.** `src/rtextures.c` carries **443 soft-float `double` relocation sites** (Phase 11 finding 3),
and its image functions convert pixels one at a time. The VFPU converts **four RGBA8 pixels per cycle**
(`vt5551.q`/`vt5650.q`) and does packed row maths. This sprint removes the avoidable doubles and adopts VFPU
pixel paths only where measured. It does **not** change texture upload/VRAM policy — the pspgu swizzle and
upload belong to Phase 6.

**Read before acting**, in this order:

1. `../memory-map.md` §10.1 — VFPU costs (`vt5551.q`/`vt5650.q` 1 cycle per four pixels), accuracy and
   thread/context rules; §14 — `pspgum`/`pspmath`/`pspvfpu` availability; §4 — what is not the frame
   bottleneck.
2. `../phases/11-general-optimisation.md` §0 findings 3, 6 and 8 — the double-site census, the `calloc`
   load path, and the missing counters.
3. `../sprints/056-phase11-vfpu-math-and-copy.md` — the per-operation rule, accuracy rows, VFPU thread
   discipline and the 64-byte copy.
4. `src/rtextures.c` — `ImageFormat`, `ImageResize`, `ImageRotate`, `ImageAlphaMask/Clear/Premultiply`,
   `ImageMipmaps`, `ImageDraw*`, `ImageBlend`, `ImageColor*`, `GenImage*`; `rlLoadTexture`'s CPU-side
   conversion.
5. Phase 13 image samples: `psp-tests/samples/` `image_processing`, `image_kernel`, `image_rotate`,
   `image_drawing`, `textures_*` — the practical workloads.

**First action.** Capture the **before** rows: per-function µs for the image functions at 64²/256²/1024²,
the image-sample load/processing times, and a double-relocation count per function. Export a fixed image from
each path and hash it as the parity control. No code change before those rows exist.

**Hard rules.**

- **Data path only.** No VRAM allocation, texture upload policy, swizzle, bind or draw change; those stay in
  Phase 6. If a change would move a texture to the GE, it is not this sprint.
- **Pixel parity is the gate.** Every adopted path must reproduce the current image bytes exactly; if a
  float/VFPU path cannot, state the tolerance and the reason before adopting. Hash the exported control.
- **VFPU thread discipline.** Any thread calling VFPU helpers needs `PSP_THREAD_ATTR_VFPU`; call
  `pspvfpu_initcontext()`/`pspvfpu_use_matrices()` and keep 16-byte alignment. PPSSPP will not catch
  corruption.
- **No accumulated state.** Image maths is one-shot per pixel; VFPU truncation is acceptable only with the
  parity row. Never move a value that accumulates across frames into VFPU.
- **No intrinsics.** `__builtin_vfpu_*` does not exist in psp-gcc 15.2.0; use `libpspmath`, `pspvfpu.h` or
  inline asm.
- **Per function, measured.** Adopt a VFPU path only where it beats the scalar/float version at the sizes
  raylib uses; a 0.1 % frame/load delta is the reportable threshold.
- **Default off until proven.** `PSP_IMAGE_VFPU=1` is opt-in; `=0` must be byte-identical to today.
- Commit messages carry the task ID.

**Commands.**

```sh
export PSPDEV=/home/psp/pspdev
export PATH=$PATH:$PSPDEV/bin

/home/psp/raylib-psp/tools/pspgu-build.sh --check
cd /home/psp/raylib-psp/src && make PLATFORM=PLATFORM_PSP -j8 PSP_IMAGE_VFPU=0
cp libraylib.a $PSPDEV/psp/lib/libraylib.a

cd ../psp-tests/image_probe        # built in T1
make
( printf './image_probe.prx\n'; sleep 60 ) | pspsh
pspsh -e "reset"

# practical workload (Phase 13 image corpus)
cd ../samples/textures/image_processing && make
( printf './image_processing.prx\n'; sleep 30 ) | pspsh
pspsh -e "reset"
# repeat for image_kernel, image_rotate, image_loading
```

**Do not touch.** `tools/pspgu` upload/swizzle, VRAM allocation, `rlgl.h` draw paths, `rmodels.c` (P11-S10),
`rshapes.c` (P11-S11), audio, input.

**Done means** §8, with per-function rows, parity hashes and a written adoption/no-adoption decision.

## 1. Objective

Remove the avoidable soft-float `double` work from the image data path and adopt VFPU packed pixel conversion
only where the device shows a win, with identical output bytes (or a stated tolerance) and no change to
texture upload or draw behaviour.

## 2. Why now

The image paths are the largest `double` cluster in raylib and are exactly the bulk, one-shot data VFPU is
good at. Phase 13's image samples are the practical driver, and the VFPU facts are already verified. Doing this
before Phase 10/11 close would mix it with the draw work; this sprint keeps it a clean data-path A/B.

## 3. Scope

**In:** `src/rtextures.c` image-data functions; the CPU-side conversion before upload; a test probe and the
image sample corpus; `PSP_IMAGE_VFPU` switch.

**Out:** pspgu texture swizzle/upload, VRAM allocation, draw paths, mesh packing (P11-S10), 2D shapes
(P11-S11), and any public API change.

## 4. Preconditions

- [ ] P11-S1 measurement harness (052) exists; CPU zones and wrap counters available
- [ ] pspgu installed and `--check` recorded
- [ ] An image probe and the image sample corpus build
- [ ] Parity control: exported image hashes for each path
- [ ] VFPU-capable thread identified and its attribute added in a separate, measured step
- [ ] PSPLink available; the user has approved the run

## 5. Tasks

| ID | task | status | verification | evidence |
|---|---|---|---|---|
| T1 | **Baseline**: per-function µs at 64²/256²/1024² for Format/Resize/Rotate/Alpha/Mipmaps/Draw/Blend; sample load times; double-site census | `todo` | before rows; parity hashes; per-function double counts | — |
| T2 | **Doubles to float**: convert only where the pixel result is identical; list rejected sites and why | `todo` | 0 differing bytes on the control set; µs per function; no size/regression | — |
| T3 | **VFPU format conversion**: RGBA8↔RGB565/5551/4444 with `vt5551.q`/`vt5650.q`; four pixels/cycle; `PSP_IMAGE_VFPU` | `todo` | parity on all formats; µs/row; VFPU vs scalar at three sizes | — |
| T4 | **Resize/rotate/alpha**: packed row maths only if T1 shows the cost; per-op accuracy/parity | `todo` | parity; µs; or `dropped` with the T1 number | — |
| T5 | **Corpus and write-back**: image samples, `batch_check`, memory-map/Phase 11 update, `mkdocs --strict` | `todo` | sample times; gate unchanged; docs build | — |

### Task log

- 2026-11-22 — sprint written; not started

## 6. Budgets

| budget | limit | measured | tag |
|---|---|---|---|
| image parity | 0 differing bytes (or stated tolerance) | | |
| per-function win | ≥ 0.1 % or a stated load-time saving | | |
| frame/load regression | 0 | | |
| VFPU state corruption | 0 over a switch stress run | | |

## 7. Measurements

| run | build | workload | metric | value | notes |
|---|---|---|---|---|---|
| baseline | `P11-S9 a` | probe 64²/256²/1024² | µs/function | | |
| baseline | `P11-S9 a` | image samples | load/process ms | | |
| doubles→float | `P11-S9 b` | same | µs, parity | | |
| VFPU | `P11-S9 c` | same | µs, parity | | opt-in |
| gate | `P11-S9 c` | `batch_check` | hash | | `b69830b8…` |

## 8. Exit criteria

- [ ] per-function baseline and change rows at three sizes
- [ ] double-site census before/after, with rejected sites named
- [ ] VFPU adoption decision per function, with parity and accuracy
- [ ] image samples no slower; `batch_check` unchanged
- [ ] `PSP_IMAGE_VFPU=0` byte-identical; `=1` parity or stated tolerance
- [ ] docs updated; `mkdocs --strict`

## 9. Result

Not started.

## 10. Follow-ups

- Mesh packing (P11-S10) uses the same measured-adoption rule; do not copy a win from pixels to vertices.
- If the cost is the pspgu swizzle/upload rather than pixel conversion, the owner is Phase 6, not this sprint.
- If VFPU wins only at 1024², record the size threshold and keep the scalar path below it.
