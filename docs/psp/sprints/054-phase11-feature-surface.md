# Sprint 054 (P11-S3) — Feature surface and limits

| | |
|---|---|
| Roadmap phase | [Phase 11 — General optimisation outside the renderer](../phases/11-general-optimisation.md) |
| Sprint | P11-S3 |
| Branch / base commit | `master` @ `e307fe05` (docs; no Phase 11 code yet) |
| Started / closed | 2026-11-21 (written) / — |
| Build number(s) | pspgu `b24`; full and stripped `libraylib.a`; numbers assigned when the sprint opens |
| Tasks | 0/7 done |

## 0. Handover

**Situation.** Every raylib module and format is enabled by default, while the PSP build links all of it into
the PRX and fetches only a fraction. The feature list in the phase page is a candidate list, not a per-sample
requirement. This sprint builds the manifest, adds a safe per-format strip switch, and measures what each
removal buys in `.text` and load time without breaking the corpus.

**Read before acting**, in this order:

1. `../memory-map.md` §6 — the measurement protocol; §9 — the "stripping a format can silently break a sample"
   trap.
2. `../phases/11-general-optimisation.md` §0 findings 4 and 7 — the config mechanism and the ownership overlap
   with phases 9 and 13.
3. `../phases/13-samples-porting/index.md` — the tier lists and the per-sample capability audit; the sprint 024
   harness and its sample manifest.
4. `src/config.h:33-125` and the module flags after it — the `#ifndef` defaults and the
   `EXTERNAL_CONFIG_FLAGS` boundary; **read the boundary before adding a switch**.
5. `src/Makefile:379-400` and `:549-601` — `CFLAGS`, `CUSTOM_CFLAGS`, `RAYLIB_CONFIG_FLAGS` and the PSP block.
6. `src/rcore.c:750-850` — the boot module list; `src/rtextures.c`, `src/rtext.c`, `src/rmodels.c` and
   `src/raudio.c` — where `SUPPORT_FILEFORMAT_*` is consumed.

**First action.** Build the per-sample requirement manifest before changing a flag. Start from the five phase 1
samples and the phase 13 tier-A set; for each, record the modules and formats it actually calls (`LoadImage`,
`LoadTexture`, `LoadFont`, `LoadModel`, `PlaySound`, export or capture). A format that no sample uses is a
candidate; a format that one sample uses is not.

**Hard rules.**

- **The full build stays.** The stripped build is a measurement build; correctness runs against the full build.
  Keep both in the tree, named in the boot line.
- **Use `PSP_CONFIG_FLAGS`, not `CUSTOM_CFLAGS` or `RAYLIB_CONFIG_FLAGS`.** A command-line `CUSTOM_CFLAGS`
  overrides the PSP block's own `-O2`, `-DPSP_BATCH` and `-DSUPPORT_MODULE_RAUDIO=0`; `RAYLIB_CONFIG_FLAGS`
  implies `EXTERNAL_CONFIG_FLAGS`, which skips every `config.h` default, so a partial set silently disables
  modules. Add `PSP_CONFIG_FLAGS ?=` inside the PSP block and append it to `CUSTOM_CFLAGS` there.
- **One group at a time.** Strip modules, then image formats, then model formats, then audio formats, then the
  export/generation/compression/capture group. A combined arm cannot attribute a size change or a break.
- **A missing symbol is a reject, not a link trick.** Do not add a stub to make a stripped build link; if a
  removed format leaves an unresolved symbol, the format is still required by a path you missed.
- **Audio follows phase 9.** `SUPPORT_MODULE_RAUDIO=0` is already forced by the current Makefile; when phase 9
  lands, its decision wins. Do not strip audio in this sprint before that decision.
- **Formats follow phase 13.** The manifest is the source of truth for what a sample needs; do not infer from
  the file extension in the sample name.
- **MAX limits need a reachability argument.** Reduce a limit only when no supported sample can reach it; record
  the arithmetic of the memory saved.
- **Stage only this sprint's paths.** `git status` before every commit; commit messages carry the task ID.

**Commands.**

```sh
export PSPDEV=/home/psp/pspdev
export PATH=$PSPDEV/bin:$PATH
set -e

cd /home/psp/raylib-psp
mkdir -p /home/psp/evidence/sprint054

# The PSP block must keep its own flags while the new ones are added.
make -Bn -C src PLATFORM=PLATFORM_PSP \
    PSP_CONFIG_FLAGS="-DSUPPORT_FILEFORMAT_BMP=0" 2>/dev/null \
  | grep -m1 psp-gcc | tr ' ' '\n' \
  | grep -E 'SUPPORT_FILEFORMAT_BMP|-O2|PSP_BATCH|SUPPORT_MODULE_RAUDIO' | sort -u
# expected: the BMP flag, -O2, -DPSP_BATCH=1, -DSUPPORT_MODULE_RAUDIO=0

# Full build (correctness) and stripped build (measurement).
( cd src && make clean && make PLATFORM=PLATFORM_PSP -j8 \
    && cp libraylib.a /home/psp/evidence/sprint054/libraylib-full.a )
( cd src && make clean && make PLATFORM=PLATFORM_PSP -j8 \
    PSP_CONFIG_FLAGS="-DSUPPORT_FILEFORMAT_BMP=0 ..." \
    && cp libraylib.a /home/psp/evidence/sprint054/libraylib-strip-bmp.a )

psp-size /home/psp/evidence/sprint054/libraylib-*.a > /home/psp/evidence/sprint054/sizes.txt
```

**Do not touch.** `src/config.h` (the defaults are upstream; use the make variable), `tools/pspgu/`, the
renderer, `src/raudio.c` before phase 9's decision, and any sample's source to remove a format it actually uses.

**Done means** §8, with the manifest, the per-group size table, the full-build capture gate and the chosen
default.

## 1. Objective

Five falsifiable results:

1. A per-sample manifest lists the raylib modules and file formats each sample in the current correctness set
   needs, with a file/command reference for each claim.
2. A `PSP_CONFIG_FLAGS` make variable adds individual `-DSUPPORT_*` overrides while preserving the PSP block's
   `-O2`, `PSP_BATCH` and audio defaults, proved by `make -Bn`.
3. Each stripped group has an archive and PRX size row; the full build and the stripped measurement build are
   both archived and named at boot.
4. The full build still passes the five-sample/phase 13 tier-A captures; the stripped build passes every sample
   in the manifest that does not require a removed format.
5. The effective `MAX_*` values are listed with their memory cost and a reachability argument; the ones reduced
   have a size row and no sample failure.

## 2. Why now

The phase's compile-time surface is the largest source of code that is never fetched: every unused decoder
costs I-cache and boot time even when no sample calls it. The review found the existing mechanism cannot do
partial overrides safely, so this sprint first makes the switch correct and then measures it. It advances the
config-flag half of the phase exit criteria and produces the size baseline for 053's chosen flags.

## 3. Scope

**In:** `src/Makefile` (`PSP_CONFIG_FLAGS`), the sprint 052 boot line (report the config), the sample manifest
under `docs/` or `psp-tests/`, and the measurement scripts/captures.

**Out:** phase 9 audio decisions, phase 13 sample rewrites, phase 10 model features, any source change to a
decoder. Stripping a format is a build configuration change, not a rewrite.

## 4. Preconditions

- [ ] Sprints 052 and 053 are in place; the boot line reports config and the chosen flags
- [ ] The full-feature build exists and passes the current capture gate
- [ ] The phase 13 tier-A manifest is available, or the five phase 1 samples are the documented fallback
- [ ] Phase 9's audio decision is recorded, or audio is left exactly as the current Makefile forces it
- [ ] `make -Bn -C src PLATFORM=PLATFORM_PSP` is captured before the switch is added
- [ ] `git status` understood; only this sprint's paths will be staged

## 5. Tasks

Status vocabulary: `todo` · `doing` · `done` · `blocked` · `dropped`.

| ID | task | status | verification | evidence |
|---|---|---|---|---|
| T1 | **Manifest**: per sample, record the modules and formats actually called, with the source line or load path that proves it; include the fallback five if tier-A is not ready | `todo` | every `SUPPORT_*` candidate has at least one "required by" row or none; no claim from a file extension alone | — |
| T2 | **`PSP_CONFIG_FLAGS` switch**: add it inside the PSP block; append to `CUSTOM_CFLAGS`; log it in the boot line; prove `-O2`, `PSP_BATCH`, audio and `-G0` survive | `todo` | `make -Bn` shows all four plus the new flag; `CUSTOM_CFLAGS` on the command line is documented as unsafe | — |
| T3 | **Module strip**: `SUPPORT_MODULE_RMODELS`; audio only if phase 9 says unused; keep rcore/rlgl/rshapes/rtextures/rtext as the 2D path requires | `todo` | stripped build links and boots; 2D samples unchanged; size row | — |
| T4 | **Image formats**: BMP/GIF/QOI/DDS/TTF/FNT, one group at a time; keep PNG/JPG if the corpus loads them | `todo` | each group links; a sample needing it still works in the full build; size row | — |
| T5 | **Model and audio formats**: OBJ/MTL/IQM/GLTF/VOX/M3D and WAV/OGG/MP3/QOA, grouped; audio only behind phase 9 | `todo` | each group links or is rejected with the missing symbol named; size row | — |
| T6 | **Feature group**: `SUPPORT_IMAGE_EXPORT`, `SUPPORT_IMAGE_GENERATION`, `SUPPORT_COMPRESSION_API`, `SUPPORT_SCREEN_CAPTURE`, `SUPPORT_AUTOMATION_EVENTS`; strip only what no sample uses | `todo` | full build unchanged; stripped build has no unresolved symbol; size row | — |
| T7 | **`MAX_*` audit**: effective `MAX_GAMEPADS`, `MAX_GAMEPAD_AXES`, `MAX_GAMEPAD_BUTTONS`, `MAX_TOUCH_POINTS`, `MAX_FILEPATH_*`, `MAX_TRACELOG_MSG_LENGTH`, `RL_DEFAULT_BATCH_*`, `RL_MAX_*`; list bytes and reachability; reduce only unreachable | `todo` | each value has a source line and a byte cost; reduced values pass the corpus | — |

### T1 design

- The manifest is a table, not prose: `sample | module | format/feature | source line | status`.
- Check the binary as well as the source: `psp-nm` for a decoder symbol is evidence that the path linked, but
  not that the sample calls it. The source line is the stronger claim.
- Treat `LoadImage`/`LoadImageRaw`/`LoadTexture` and `LoadFont`/`LoadFontEx` as the image/font entry points;
  `LoadModel`/`LoadModelFromMesh` for models; `PlaySound`/`LoadSound`/`LoadMusicStream` for audio.
- The fallback set is the five samples recorded in sprint 002: `basic_shapes`, `collision_area`,
  `logo_raylib_anim`, `cubicmap` and `bunnymark`. If a fallback sample is not in the tree, say which sample
  replaces it.

### T2 design

- In the PSP block:
  ```make
  PSP_CONFIG_FLAGS ?=
  CUSTOM_CFLAGS += $(PSP_CONFIG_FLAGS)
  ```
  Place it after the existing `PSP_OPT`/`PSP_BATCH` additions so the command-line variable cannot override the
  block. The command-line `CUSTOM_CFLAGS` caveat is a hard rule: it replaces the whole variable in make.
- Do **not** use `RAYLIB_CONFIG_FLAGS` for a partial set. It adds `-DEXTERNAL_CONFIG_FLAGS`, and every
  `#ifndef` default in `config.h` disappears; undefined module flags then evaluate false in `#if`, which can
  silently disable rshapes/rtextures/rtext.
- Log the effective config in the boot line: `config=default` or the exact `PSP_CONFIG_FLAGS` string. A size
  number without it is not evidence.

### T3 design

- `SUPPORT_MODULE_RMODELS=0` is the first module candidate; the 2D corpus does not need it, but the manifest
  decides whether the correctness set includes a 3D sample. Verify that `rlgl`/`rshapes`/`rtextures`/`rtext`
  still link and that the default font/shape texture path is intact.
- Audio is not a free strip in this sprint: `src/Makefile` already forces `SUPPORT_MODULE_RAUDIO=0` because
  miniaudio has no PSP backend. Phase 9 owns the enable decision; do not change it here.
- Keep a full build with every module on for correctness; the stripped build is named and archived separately.

### T4/T5 design

- Group by decoder family, not by one flag at a time, so the number of device runs stays bounded. Record the
  group's flags in the evidence file.
- `SUPPORT_FILEFORMAT_TTF` and `SUPPORT_FILEFORMAT_FNT` are not just image decoders: `LoadFontDefault` uses the
  built-in font data, and `LoadFontEx` uses stb_truetype. A sample that only calls `DrawText` may still need
  the default font path; check before stripping TTF.
- `SUPPORT_FILEFORMAT_PNG` is required by the current texture samples and screenshots; do not strip it without
  changing the capture path.
- A stripped build that links but fails at runtime is worse than one that fails to link. Run the manifest's
  samples, not just the linker.

### T6 design

- The export/generation/compression/capture group is used by tooling and by samples that call
  `ExportImage`, `GenImage*`, `CompressData` or `TakeScreenshot`. The five-sample set may not use them; the
  phase 13 corpus might. The manifest decides.
- `SUPPORT_AUTOMATION_EVENTS` is small but adds per-frame code in `EndDrawing`; if no sample records automation,
  strip it and record the size.
- `SUPPORT_SCREEN_CAPTURE` is the F12 path; the capture harness uses `scrshot`, not this flag, so it is a
  candidate only if no sample relies on the in-app key.

### T7 design

- The `MAX_*` values that matter on this device are the ones multiplied by a large element: gamepad axis
  arrays, touch points, file paths, the batch buffer, the matrix stack and trace-log length. List the element
  size and count for each.
- `MAX_TOUCH_POINTS` is 8 in `rgestures.h`; `MAX_GAMEPADS`/`MAX_GAMEPAD_*` are in `rcore.c`. The config.h block
  is commented documentation, so a grep of `config.h` alone is not an audit.
- Reduce only when the hardware or the API contract makes the value unreachable; a 4-pad array on a one-pad
  console is a candidate, a 16-entry key queue is not obviously so.

### Task log (append-only)

- 2026-11-21 — sprint written; all tasks `todo`, no device run

## 6. Budgets

| budget | limit | measured | tag |
|---|---|---|---|
| `.text` | each stripped group records archive and PRX delta | | |
| frame | no regression on the 2D corpus | | |
| load | no regression on the load sample | | |
| correctness | full build passes the capture gate; stripped build passes its manifest | | |
| heap | `MAX_*` reductions record bytes saved | | |

## 7. Measurements

Protocol: device only; one variable per group; control → change → control. Every row carries the boot id and
the exact `PSP_CONFIG_FLAGS`.

| arm | config flags | archive `.text` | PRX `.text` | frame | load | manifest samples | notes |
|---|---|---|---|---|---|---|---|
| full | default | | | | | all | |
| modules | `-DSUPPORT_MODULE_RMODELS=0` | | | | | | |
| image group | `...` | | | | | | |
| model group | `...` | | | | | | |
| audio group | phase 9 decision | | | | | | |
| features | `...` | | | | | | |
| limits | `...` | | | | | | |

**Manifest:**

| sample | module | format/feature | source evidence | required? |
|---|---|---|---|---|
| | | | | |

## 8. Exit criteria

- [ ] the manifest covers every candidate flag and every correctness sample
- [ ] `PSP_CONFIG_FLAGS` preserves the PSP block's flags, proved by `make -Bn`
- [ ] `RAYLIB_CONFIG_FLAGS`/`EXTERNAL_CONFIG_FLAGS` and command-line `CUSTOM_CFLAGS` are documented as unsafe
- [ ] each stripped group has archive and PRX size rows
- [ ] the full build passes the capture gate; the stripped build passes its manifest
- [ ] the chosen default is explicit and reported at boot
- [ ] the `MAX_*` audit lists values, bytes and reachability; reductions are measured
- [ ] the archive has `SHA256SUMS`

## 9. Result

Not started. Record the manifest, the groups that were safely removed, the groups that broke a sample, the size
deltas and the chosen default. A stripped build that was abandoned is a result; write it down.

## 10. Follow-ups

- **Sprint 055 (P11-S4)** uses the stripped measurement build for the frame-CPU work and the full build for
  correctness.
- **Phase 9** owns the audio module and format decision; this sprint only records its current forced state.
- **Phase 13** owns the per-sample capability manifest; if this sprint's manifest grows, merge it there rather
  than maintaining two.
