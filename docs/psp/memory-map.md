# PSP port — memory map and roadmap

Working document for this fork. It exists so that a fresh session (human or AI agent) can pick up
without re-deriving anything: what the code looks like, what is measured, what is decided, and what
comes next.

**How to use it**

1. Read §1–§4 before touching code. They are facts, not plans.
2. Work one phase from §5 at a time. Do not start a phase before the previous one's exit criteria are
   met and recorded here.
3. Every number added to this file carries a tag: `[measured: device]`, `[derived]`, `[public: source]`
   or `[practice]`. An untagged number is a guess and does not belong here.
4. When a decision changes, append to §8 with the date and the evidence. Never rewrite history here.
5. Planning a sprint: start at §11 for where to read, then §10 for what the other compute units can
   take off the main CPU. Do not design a phase from memory when the source is on this disk.
6. Use the templates in §13 for every sprint, feature and recipe. The method is not paperwork — it is what
   makes a change attributable and a measurement repeatable.

Last verified: 2026-11-20. Branch `psp-6.2` (tag `6.2`, commit `02566453`).

---

## 1. Status

| item | state |
|---|---|
| Base | raylib **6.2**, tag `02566453`, full history, branch `psp-6.2` |
| `master` | **merged** (`2fe3e5fb`): `psp-6.2` merged into 6.5-dev. **Validated on device** — 600 frames, 30 fps, 284 colours, no exception |
| Remotes | `origin` = `nikmes/raylib` (fork), `upstream` = `raysan5/raylib`, `consoles` = `mcidclan/raylib4Consoles` |
| PSP platform code | **imported** — sprint 001 closed: 824 lines across 5 files |
| Build on PSP | works: `libraylib.a` 1,718,210 B (6.2), 0 warnings, 0 stub warnings |
| Device validation | **6.2 only** — 600 frames, 30 fps, 284 colours; the `master`/6.5-dev build is unvalidated |
| Hardware | PSP-1000, 32 MB, firmware 6.60 with ARK CFW — see [Environment](environment.md) |
| Build / deploy | `make PLATFORM=PLATFORM_PSP`, stage archive **and headers** into pspdev, run over PSPLink — procedures in [Environment](environment.md) |
| Toolchain | pspdev v20261101, `psp-gcc 15.2.0`, PSPSDK at `~/pspdev/psp/sdk` |

---

## 2. Memory map: where the work happens

Line numbers are 6.2 unless stated. `src/` is 13 files; these six matter.

| file | size | why it matters |
|---|---|---|
| `src/platforms/rcore_template.c` | 542 lines, 49 functions | **the exact interface a platform must implement.** Start here, not from another platform |
| `src/rcore.c` | 4,704 lines | calls `InitPlatform()`/`ClosePlatform()` (declared ~488), includes the platform `.c` behind `#elif defined(PLATFORM_*)` (~516+). Rest is platform-agnostic |
| `src/rlgl.h` | 5,432 lines, 147 distinct GL entry points | the renderer. Backends selected by `GRAPHICS_API_*`; **immediate mode under GL11** |
| `src/rtextures.c` | — | `DrawTexturePro` = the per-sprite draw path (bind, 1 quad, unbind) |
| `src/rtext.c` | — | every glyph goes through `DrawTexturePro` |
| `src/rshapes.c` | — | shapes quads path; the 5.5 port forces PSP off it |
| `src/raudio.c` + `src/external/miniaudio.h` | — | audio. miniaudio has **no PSP backend** — zero references to PSP in 100k+ lines — but it does support a *custom* backend (`MA_ENABLE_CUSTOM` / `ma_backend_custom`) |
| `src/Makefile` | — | where `PLATFORM_PSP` must be added (the 5.5 port has 29 references) |

Input is deliberately not a separate file. `IsKeyDown`, `IsGamepadButtonDown` and
`GetGamepadAxisMovement` (rcore.c ~3903/4033/4089) read `CORE.Input` and are platform-agnostic; a
platform only has to fill that struct inside `PollInputEvents()`. Whatever we do with buttons and the
nub lands there, in the platform file — not in rcore.c.

Facts about `rlgl.h` that shape every decision:

- `GRAPHICS_API_OPENGL_11` maps `rlBegin`->`glBegin`, `rlEnd`->`glEnd`, `rlVertex2i`->`glVertex2i`
  (lines ~1429–1440). The batched implementation is the `#else` branch (~1452) and is compiled out.
  **There is no CPU/GPU render batch on the GL11 path.** `[public: src/rlgl.h]`
- `rlSetTexture(0)` under GL11 becomes `rlDisableTexture()` (~1638–1643), i.e. an unbind. `[public]`
- `GRAPHICS_API_OPENGL_SOFTWARE` is not a rasterizer: it is `#define GRAPHICS_API_OPENGL_11`
  (~178). raylib's model is "rlgl speaks GL 1.1; whatever library supplies those symbols is the
  backend". This is the license raylib itself gives us to add a GU backend. `[public]`

What the 5.5 port (in `consoles/raylib4Consoles`) touches — the whole delta, measured:

- `src/platforms/rcore_psp.c` — new, 730 lines (window, EGL surface, input, timer).
- `src/rcore.c` — additive only: the include, three log strings, the `InitPlatform()` call site, PSP in
  the `clock_gettime` platform list (~15 lines).
- `src/rlgl.h` — two one-line guards: skip `glHint(GL_PERSPECTIVE_CORRECTION_HINT)`, skip
  `glGetTexImage` (so `rlReadScreenPixels` returns a blank buffer instead of calling a missing
  symbol).
- `src/rshapes.c` — `SUPPORT_QUADS_DRAW_MODE && !defined(PLATFORM_PSP)` (two sites): PSP takes the
  per-triangle path.
- `src/Makefile` — the `PLATFORM=PLATFORM_PSP` block.

Interface gap: the 5.5 `rcore_psp.c` implements **46 of 6.2's 49** platform functions. Missing:
`GetClipboardImage`, `GetKeyName`, `SetGamepadVibration`. `[measured: symbol diff, 2026-11-20]`

---

## 3. Hardware facts that bound the design

| fact | value | tag |
|---|---|---|
| RAM | 32 MB total on a 1000; user partition 2 is 25,165,824 B, 25,149,440 free idle | `[measured]` |
| Free memory a program sees with a declared 1 MB heap | 23,128 KB (1 MB heap + 256 KB main stack + 4 KB) | `[measured]` |
| EDRAM for GE | 2 MiB, at `0x04000000` | `[public: pspgu/pspge]` |
| Framebuffers (pspgu: 5650, 512x272 stride **x3**) + 16-bit depth | **1,088 KiB**, of which 272 KiB is depth | `[measured: sprint 014]` |
| Textures left after the above | **913,344 B** with depth; **1,191,872 B** without | `[measured: sprint 014]` |
| VRAM layout, pspgu b16 | colour `0x00000`, `0x44000`, `0x88000`; depth `0xCC000`; textures from `0x110000` (1,114,112) — 1,183,808 B used with the corpus' textures, **913,344 B free** | `[measured: sprint 014]` |
| Two-buffer layout (superseded) | two 512x272 buffers + depth = 816 KiB, textures from `0xCC000` | `[derived, before sprint 014]` |
| VRAM layout, 32-bit | three buffers at 557,056 + 557,056 + 278,528 = **1,392,640 B used**, only 704,512 B free | `[external: blueprint §16.3]` |
| GE texture cache | **8 KiB**, coherent with nothing; only `sceGuTexFlush` empties it. A T8 128x128 texture (16 KiB) already misses within one triangle | `[external: blueprint §16.7]` |
| Max texture the GE samples | **512 x 512**; larger images must be drawn as 512-wide windows | `[external: blueprint §16.7]` |
| Vertex colour order | `0xAABBGGRR` | `[measured]` |
| 16-bit texture coordinates | unsigned; zero-centred UVs wrap the long way round | `[measured]` |
| s16 vertex positions | normalised (value/32768) in 3D transform mode | `[public: PPSSPP GE docs]` |
| GE matrices | float24, ~16 significant bits: render camera-relative | `[public + measured]` |
| X/Y clipping | none; a primitive crossing the 4096 window is dropped whole | `[public: PPSSPP GE docs]` |
| Depth buffer on a 2D-on-rails renderer | allocatable but useless; the reference engine allocates and never enables it | `[measured: PSPRuntime]` |
| Hardware audio channels | 8 (indices 0–7); per-channel buffer 64–65,472 samples, a multiple of 64; volume 0–`0x8000` | `[public: pspdev/psp/sdk/include/pspaudio.h]` |
| Audio sample-rate conversion | `sceAudioSRCChReserve(samplecount, freq, channels)` + `sceAudioSRCOutputBlocking()` — hardware SRC for arbitrary rates instead of resampling on the CPU | `[public: same header]` |
| Input hardware | one nub (2 axes, raw 0–255), D-pad, 4 face buttons, 2 shoulder buttons, SELECT/START/HOME/HOLD/NOTE. **No analog triggers, no rumble** | `[public: pspctrl.h]` |

**What the toolchain actually gives us** — GU primitives, vertex formats, GUM, VFPU contexts, VRAM limits — is
verified against the headers in §14 rather than assumed from documentation.

---

## 4. Where the time goes today

Two independent costs, and they must be attacked separately or the numbers lie.

**A. Per-object submission.** `DrawTexturePro` binds a texture, issues one immediate-mode quad,
unbinds. Under GL11 that is `glBegin`/4 vertices/`glEnd` per sprite, and `glEnd` submits. Text repeats
it per glyph. `[public: 6.2 sources]` On PSPGL the immediate buffer is small and each `glEnd` flushes
`[reference: TheMrIron2/pspgl glVertex.c, glEnd.c]`. Structural target: `N` sprites sharing texture
and state become `ceil(N / capacity)` submissions.

**B. What the submission costs.** PSPGL as shipped is 2022 code (`de4260ad`, 2022-03-23 — the
`psp-packages` pin). `TheMrIron2/pspgl` carries 11 commits on top of that exact commit: aggregate
profiling, configurable command-list capacity, corrected profiling accounting, cached texture-matrix
adjustment, raw depth range, presented framebuffer. `[measured: git merge-base, 2026-11-20]`

**Measured: those 11 commits are worth 10 %.** Three arms at N=1000 / N=5000, two runs each:
shipped 90.29 / 436.42 ms; `de4260ad` built here 90.30 / 436.43 (control, within 0.03 % of shipped, so the
build recipe is neutral); fork HEAD **81.24 / 392.06 (−10.03 % / −10.17 %)**. Marginal cost per sprite falls
from 86.5 to **77.7 µs** — about 215 sprites per 16.7 ms frame instead of 181. The symbol set is a strict
superset: 287 identical plus 6 added, nothing removed. `[measured: PSP-1000, sprint 005]`

**And the command-list size is a bigger lever than the 11 commits were.** `PSPGL_DLIST_SIZE_WORDS`, measured at
N=1000 (medians of two runs): 512 → 81.24 ms, 1024 → 72.37, 4096 → 63.60, **8192 → 61.63**, 16384 → 62.73. The
curve turns over after 8192, which is what we adopt. Combined with the fork that is **90.29 → 61.63 ms, −31.8 %**,
or 86.5 → **58.3 µs per sprite** and ~287 sprites in a 16.7 ms frame. Cost: 16 command lists × 8,192 words × 4 B
= **512 KiB of main RAM**, up from 32 KiB at the default `[measured: PSP-1000, sprint 006]`

**What the fork's profiler showed** (the first instrument that sees inside pspGL): at N=1000 there are
**257 command-list submissions per frame** for 1,025 primitives — ~4 primitives each at the default 512-word
list — with **2,725 µs/frame of command-list reuse wait**. That is the wait sprint 004 could not locate: it is
in list reuse, not in `eglSwapBuffers`. Texture binds are 2,050/frame with **zero redundant**, so there is
nothing for a duplicate-bind optimisation to save yet. `draw_calls` reads 0 — the fork's draw counters cover
the array paths, not `glBegin`; submissions is the counter that sees the immediate path. A profiling build is
**18.6 % slower**, so it is never an A/B arm. `[measured: PSP-1000, sprint 005]`

A proven upper bound for both, from a working GU renderer (`PSPRuntime`, see §7): 2 vertices per sprite
with `GU_SPRITES` gives **1024 sprites per draw call**, quads (4 v + 6 i) give 512, and its bunnymark
therefore issues `ceil(N/1024)` calls per frame. `[measured: code scan]` That is the structure our
batcher has to reach, and the number to beat when the native backend lands.

**What it costs today**, measured on 6.5-dev through pspGL with the GL-boundary counters (sprint 003):

| bunnies | `glEnd`/frame | vertices/frame | frame | fps |
|---|---|---|---|---|
| 100 | 130.0 | 520.0 | 16.9 ms | 59 (vsync-capped, an upper bound) |
| 1,000 | 1,030.9 | 4,123.8 | 100.4 ms | 10 |
| 5,000 | 5,030.0 | 20,120.0 | 470.4 ms | 2.1 |

- **Chrome is 30 primitives/frame** at every count (header rect, two text runs, FPS). Everything else scales
  with sprites.
- **4.000 vertices per `glEnd`** — one quad per primitive, exactly as the GL11 path reads.
- **86.5 µs of marginal cost per sprite** (sprint 004 sweep; 92.5 µs measured earlier on a slightly different
  app build), i.e. about **181 sprites** fit in a 16.7 ms frame. Phase 4's premise is that number, not an
  assumption.
- **Fill is not the wall.** Same 1,000 sprites and same submission count drawn at 4x size — 16x the pixels —
  cost **+5.7 %** (90.279 → 95.438 ms). Five times the sprites cost five times the time. The bottleneck is
  per-sprite submission.
- **The display paces the swap and pspGL hides where.** At 100 sprites the frame is 16.68 ms with
  `min = 12.2 ms`; `eglSwapInterval(display, 0)` does not change the median, and the swap call itself measures
  0.15 ms. pspGL returns from the swap and blocks later, on the next buffer use — so separating CPU cost from
  display wait needs pspGL-level timing (phase 3). Until then the low-count row is an upper bound.
- Repeatability floor is **0.02 %** worst case; adopt **0.1 %** as the threshold for a reportable difference.
`[measured: PSP-1000, sprint 004, full table in `baselines.md`]`
- Every primitive costs one bind and one unbind (`binds == unbinds == ends`), so texture state churn scales with
  sprite count too.
`[measured: PSP-1000, master @ 252acee7, sprint 003]`

**What a batch changes**, same app, same session, both arms from one commit (sprint 007):

| bunnies | arm | draws/frame | vertices/frame | frame | work |
|---|---|---|---|---|---|
| 100 | unbatched | 0 (`glEnd` 124) | 496 | 16.683 ms | 9.16 ms |
| 100 | batched | 2 | 744 | 16.683 ms | 3.55 ms |
| 1,000 | unbatched | 0 (`glEnd` 1,025) | 4,100 | 66.733 ms | 65.70 ms |
| 1,000 | batched | **3** | 6,150 | **33.367 ms** | 18.09 ms |
| 5,000 | unbatched | 0 (`glEnd` 5,024) | 20,096 | 316.98 ms | 317.19 ms |
| 5,000 | batched | **9** | 30,150 | **100.107 ms** | 81.91 ms |

- Marginal cost per sprite drops **62.6 → 16.7 µs**; the 46 µs that went away was submission, exactly as phase 4
  predicted.
- pspGL maps `GL_QUADS` to a GE **triangle fan**, so batched quads must be expanded to triangles: 6 vertices per
  sprite instead of 4.
- **Fill is rasterisation-rate-bound, ~221 Mtexel/s, and it does not care about texel bytes or blending.**
  Measured at N=1,200 with CPU work held constant: RGBA8888 → 5650 → 5551 moved the frame by 0.0 %, blending off
  by −0.2 %, 4× the pixel area by **+199.5 %**. So the 60 fps budget is **~1.6–2.0 M drawn texels per frame**, not
  a sprite count — and bunnymark's 1,600 sprites are 9.4× overdraw. Quote texels/frame, not sprites
  `[measured: sprint 010, 8 cells]`.
- **16-bit textures are a memory lever, not a speed lever**: 2 B/texel instead of 4 in a 2 MiB budget, at zero cost
  in speed. `5551` has 1 bit of alpha (hard-edged sprites), `4444` has 4 — a per-texture fidelity choice.
- `glLockArraysEXT` around each draw: **no effect** (+0.4 %), because the cache is rebuilt per lock.
`[measured: PSP-1000, 2026-11-20, sprint 007, full table in `baselines.md`]`

**The compiler, not the vertex path** (sprint 008, and it corrects the row above). Sprint 007 read the remaining
16.7 µs per sprite as per-vertex cost. Sprint 008 cut vertices per sprite 6 → 2 with `GL_SPRITES_PSP` and the frame
did not move: **33.369 ms**, the same to 0.002 ms. The residual was never per-vertex — it was **unoptimised code**.
`src/Makefile`'s `RELEASE` block covers desktop, web and android only, so every PSP library built until then came
out of `psp-gcc` at **`-O0`**, while pspGL itself is built `-Os`. `PSP_OPT` now defaults to `-O2`:

| bunnies | unbatched `-O0` | unbatched `-O2` | batched `-O0` | batched `-O2` | at `-O2`, sprites (2 v) |
|---|---|---|---|---|---|
| 1,000 | 66.733 ms | 50.051 ms | 33.367 ms | **16.683 ms** | 16.683 ms |
| 5,000 | 316.98 ms | — | 100.107 ms | 66.733 ms | 66.733 ms |

- **1,000 sprites at 60 fps**, byte-identical picture (`batch_check` md5 unchanged between `-O0` and `-O2`). The
  batched/unbatched ratio widened from 2.0× to 3.0×.
- After sprint 009 the batcher is the **default** build and the vertex push path is inlined:
  `work = 0.87 ms + 6.35 µs × sprites`, 60 fps up to **1,600** sprites, and a mixed rotated/unrotated scene costs
  16.683 ms instead of 83.417. Of the 6.35 µs, 2.35 µs is the application's own per-object loop — the drawing path
  is 4.95 µs, and inlining it bought only 10 %: the cost is arithmetic and staging, not calls `[measured: sprint 009]`.
- **60 fps ceiling**, measured by bisection at `-O2`: triangles fall to 30 fps between **1,200 and 1,300** sprites,
  sprites between **1,500 and 1,700**. Below 1,000 the `-O0` arm was already out of budget.
- **Per-draw cost is what batching removed, and it is large**: 48.8 ms of work for 1,025 submissions at N=1,000 is
  ~45 µs per `glEnd`. A mixed sprite/triangle scene (every second sprite rotated) issues **1,001 draws/frame** and
  costs **83.4 ms** — worse than not having sprites at all. Two buffers, or a bind cache that stops re-binding the
  same texture, are the fix (phase 4, sprint 009).
`[measured: PSP-1000, 2026-11-20, sprint 008]`

Not addressed by any of this: fill rate, blending bandwidth, texture-cache misses, GE transform
throughput. Those bound a PSP at 480x272 long before submission overhead does in a fill-heavy scene.
`[practice]`

---

## 5. Roadmap

Each phase has its own page under [`phases/`](phases/index.md) with its preconditions, tasks and exit criteria.
This section is the summary and the ordering rationale; the phase pages are the working surface.

Phases are ordered so each one is measurable against the previous. Nothing here is a promise about
performance; it is a sequence for finding out.

### Phase 0 — Repository baseline — **done**
Fork, branch `psp-6.2` at tag `6.2`, remotes wired, identity repo-local, branch pushed.
Exit: branch visible on `origin`, contents identical to tag `6.2`. **Met.**

### Phase 1 — 6.2 boots on pspGL
Goal: 6.2 rendering on the console, no batching work yet.
- Import `rcore_psp.c` from `consoles/raylib4Consoles`; add `PLATFORM_PSP` to `src/Makefile`
  (and `CMakeLists.txt` if we want the CMake path).
- Implement `GetClipboardImage` (return an empty image), `GetKeyName` (static table), and
  `SetGamepadVibration` (accept and ignore; PSP has no rumble).
- Re-apply the three renderer guards (`rlgl.h` x2, `rshapes.c` x1) and record why each exists.
- Build `libraylib.a` with `-O2` first, not `-O3`; measure both later.
- Run `basic_shapes`, `collision_area`, `logo_raylib_anim`, `cubicmap`, `bunnymark` over PSPLink.
Exit: all five render; screenshots captured; no crash; framebuffer dumps archived per sample.

### Phase 2 — Instrumentation and baseline
Goal: numbers that exist before optimisation, so no change is unattributable.
- Modify `bunnymark` so its `batched draw calls` figure comes from the backend, not from
  `1 + count/MAX_BATCH_ELEMENTS` — on PSP that formula is fiction. `[public: bunnymark source]`
- Add counters: draw submissions, vertices submitted, texture binds, state changes, frame CPU time,
  GE wait time. Where they live depends on the phase-3 decision (raylib side or pspGL side).
- Run bunnymark at fixed counts (100, 1k, 5k, 10k, 20k) rather than free-running; record median and
  95th-percentile frame time over a fixed sample.
- Cross-check with a direct-GU reference (`PSPRuntime`'s bunnymark) once its list overflow is fixed.
Exit: two runs of the same build agree within 5%; every optimisation category below has a baseline.

### Phase 3 — pspGL upgrade A/B
Goal: find out how much of the cost is the library, before writing any of our own.
- Build `TheMrIron2/pspgl` HEAD against pspdev; same raylib binary, same workload, same clocks.
- Compare against the shipped 2022 PSPGL, one control run and one control-again.
- Decide: adopt the fork as our pspGL, or stay on the package.
Exit: quantified delta with the counters from phase 2, and a recorded decision either way.

### Phase 4 — PSP fixed-function `rlgl` batcher (over GL11)
Goal: collapse per-object submission without leaving OpenGL yet.
- Add a batch selected by `PLATFORM_PSP && GRAPHICS_API_OPENGL_11`; keep the portable GL11 path intact.
- Vertex layout: textured 24 B (`u16`-based or float UV + `u32` colour + 3 floats), colour-only 16 B,
  no normals for 2D.
- Flush only on: texture object, primitive mode/layout, blend state, scissor, render target, depth
  state, matrix state when vertices are not pre-transformed, capacity, `EndDrawing`.
- Turn `rlSetTexture()` into a state transition: bind at submission, never unbind per sprite.
- Use `GL_SPRITES_PSP` only for axis-aligned, unrotated, unflipped sprites; 4/6 vertices otherwise.
- Allocate batch storage once; zero heap allocations per sprite or glyph.

What pspGL does with this path, read from the adopted fork's source (sprint 007):
- **`GL_QUADS` is not batched geometry.** `geprim_tab` in `pspgl_varray.c` maps `GL_QUADS` to
  `GE_TRIANGLE_FAN`, so `glDrawArrays(GL_QUADS, 0, 4N)` renders one fan anchored on vertex 0 — every quad
  after the first shares it and the frame fills with spikes. Quads must be expanded to two triangles on
  push (6 vertices per sprite). This is invisible in `glBegin(GL_QUADS)`, which emits one fan per quad.
- **Every `glDrawArrays` copies the arrays** into a GE-visible vertex buffer (`__pspgl_gen_varray`). The
  copy is a plain `memcpy` when the layout is already GE-native, and a per-attribute convert loop
  otherwise. Native means, in this order: texcoord 2×`f32`, weight, colour 8888 as 4 bytes, normal,
  position 3×`f32` — so the 24-byte `{u,v, rgba, x,y,z}` interleaved layout is native and pays the
  `memcpy` only `[public: pspgl_varray.c]`.
- **`glLockArraysEXT` caches up to 128 KB of arrays once** and then draws from the cache; with the arrays
  in a pspGL buffer object it skips even that copy (`__pspgl_cache_arrays`). Locking per draw buys
  nothing — the cache is built on first use after the lock `[public: glLockArraysEXT.c]`.
- **Colour must be 4 bytes or 3 floats.** `glColorPointer(3, GL_UNSIGNED_BYTE)` is packed by a convert
  function; 3-float and 4-float colours are converted too. `GL_UNSIGNED_BYTE` with size 4 is passed
  through as-is, i.e. memory order `r,g,b,a` → GE `0xAABBGGRR`.
- **The default font atlas shares one texture id with the shapes texture** in 6.5-dev, so text and
  untextured rectangles coalesce into a single draw call. A frame's submission count is therefore
  "textures used", not "kinds of primitive `[measured: batch_check, 2 draws for 51 primitives]`.
Exit: same-texture run of N sprites produces ~`ceil(N/capacity)` submissions; screenshots match the
unbatched path; no per-sprite allocation measurable.

**Outcome (sprint 007):** 1,000 sprites 66.73 → 33.37 ms, 5,000 → 100.11 ms, submissions 1,025 → 3 and 5,024 → 9,
pixel-identical on a deterministic scene. Per-sprite marginal cost 62.6 → 16.7 µs. The residual is per submitted
vertex (2.4–2.6 µs), which batching cannot remove — see §4 and the sprint's §9.

### Phase 5 — Native GU backend (`GRAPHICS_API_PSP_GU`)
Goal: remove GL and pspGL from the hot path. This is the phase the whole fork exists for.
- Implement the rlgl API surface over `sceGu` as a new backend, keeping the platform layer untouched.
- Display lists: two rotating lists, 16-byte aligned, sized from a real worst case; submit frame N,
  sync frame N-1 (`GU_SYNC_WHAT_QUEUED`), never a full `FINISH` stall per frame.
- Never rewrite vertex memory the GE may still read: per-list vertex arenas with explicit lifetimes.
- Framebuffer: `GU_PSM_5551`, 512-px stride for a 480-wide view, no depth buffer.
- Pixel-exact 2D: `sceGuOffset(2048 - W/2, 2048 - H/2)` + `sceGuViewport(2048, 2048, W, H)` +
  `sceGumOrtho(0, W, H, 0, -1, 1)` (y-down, same space as raylib).
- Camera-relative transforms on the 3D path; float24 discipline.
Exit: all samples render identically to phase 4; frame time no worse than the best available pspGL;
VRAM accounting printed at boot and matching the budget in §3.

### Phase 6 — Textures and VRAM
- Swizzle directly into the destination (no memcpy to VRAM, no `sceGuCopyImage`).
- POT padding, CLUT for glyph atlases, no mipmaps unless measured to pay.
- Best-fit allocator over 512-byte blocks with coalescing; track free bytes and largest block
  separately; one allocator only, framebuffers included.
- Residency: priority-aware eviction (`glPrioritizeTextures` equivalent must actually influence it),
  usage that ages per frame, never accumulate forever.
- Range-based cache writeback on upload; never `WritebackInvalidateAll` in a stable frame.
Exit: no eviction when the working set fits; upload benchmarks by format and size; fragmentation
bounded under load/unload churn.

### Phase 7 — 2D specialisation
- Text: accumulate glyphs of one atlas into one run; measure short labels, paragraphs, counters.
- Sprites/tile maps: reject fully offscreen sprites before generating vertices; fast path for
  unrotated/uniformly-scaled; optional horizontal slicing for large sprites if the cache pays.
- Shapes: keep the shapes texture bound across consecutive calls; batch rectangles/triangles/lines.
- Particles: one dynamic stream per texture run, double or triple buffered.
Exit: text submissions scale with atlas runs, not glyph count; tile maps with atlas runs, not tiles.

### Phase 8 — Input
Goal: every control on the device reachable through raylib's normal API, with a nub that behaves.
The imported platform file already polls `sceCtrlReadBufferPositive()` once per frame in `PollInputEvents()` and
maps buttons to `GAMEPAD_BUTTON_*` with a 0.2 deadzone — start from that, then close its gaps.
- Verify the mapping button by button: CROSS/CIRCLE/SQUARE/TRIANGLE, D-pad, L/R shoulders,
  SELECT/START. Decide and document what HOME, HOLD, NOTE and the other PSPSDK-only bits do.
- Edge detection already works: `lastButtonPressed` is reset at the top of each poll and set on a new press
  (`rcore_psp.c:426,497`), so `GetGamepadButtonPressed()` reports a press for one frame. What is still wrong:
  a press shorter than the sampling period is lost, and interception/HOLD can produce a spurious press when
  input resumes.
- Stop exposing a fabricated right stick: `Rx`/`Ry` are outside the PSP-1000 contract and the read is
  unchecked. Clearing `axisCount` is not enough — `GetGamepadAxisMovement()` ignores it.
- `ReadBufferPositive(&pad, 1)` consumes at most one sample per call: a fast frame blocks until the next
  VBLANK, a slow frame falls behind. Decide between `Peek` and reading several samples and merging.
- Calibrate the nub instead of assuming 128 is centre: sample raw min/max/centre per unit, derive a radial
  deadzone and a magnitude clamp, provide a calibration path, and keep the deadzone configurable. PSP nubs
  are not symmetric.
- Decide whether to also feed `CORE.Input.Keyboard` and the mouse from the pad/nub. It makes upstream
  examples and Phase 13's tier-B samples run unmodified (`IsKeyDown(KEY_RIGHT)`) and costs nothing at runtime —
  but it changes what `IsKeyDown` and the mouse mean on this platform, so record the choice.
- Sampling: `sceCtrlSetSamplingCycle()` / mode, and whether to read once per frame or on a fixed rate.
- `SetGamepadVibration()` must stay a documented no-op — the PSP has no rumble.
Sprints 041–044 (P8-S1–P8-S4): 041 done (5/5) — input mapping, contract arms, held-button failure snapshot and
T5 regression device-validated; 042 T1 done (peek adopted; read/readn block ~13.7 ms with vsync off), T2 latch
built (device tap pending); 043–044 written.
Exit: a sample that draws every button's live state plus raw and normalised axis values; each control
pressed and confirmed; measured nub range recorded on this console; `GetGamepadButtonPressed` returns the
pressed button for exactly the frame whose poll saw it.

### Phase 9 — Audio
Goal: `InitAudioDevice()` works and sounds play without stealing frame time. This is the phase with
the least upstream help: miniaudio has **no PSP backend**, so something has to be written.
- Choose the integration: miniaudio's custom backend (`MA_ENABLE_CUSTOM`, `ma_backend_custom`,
  context callbacks) versus bypassing miniaudio's device layer and driving `sceAudio` from `raudio.c`.
  Prefer the custom backend if it keeps `raudio.c` untouched — a smaller fork of upstream.
- Output: reserve channels with `sceAudioChReserve()`, buffer 64–65,472 samples (multiple of 64),
  `sceAudioOutputPannedBlocking()` for left/right. **The blocking call must live on its own thread**,
  never in the main loop, or every frame inherits the audio latency.
- Rate: prefer `sceAudioSRCChReserve()` hardware SRC over CPU resampling for arbitrary sample rates.
  Measure both before choosing.
- Channel budget: 8 hardware channels exist; a mixer that sums sounds in software into one or two
  channels leaves the rest free for streaming music.
- Music: decode on the audio thread into a ring buffer, never open a file from the callback. Memory
  stick opens cost ~17 ms each, so few long-lived handles and large reads.
- Pitch (`SetSoundPitch`) has no hardware path: it is resampling, and its cost belongs in the profile.
- Optional later: the Media Engine can run codec work while the main CPU draws; treat as an
  optimisation, not a design assumption.
Exit: a sample playing several sounds plus one streamed track, 60 fps held on the device, audio thread
CPU cost measured, five minutes without a dropout, and no `sceAudio` call on the main thread.

### Phase 10 — 3D and advanced
- Model vertex data converted once to GE-native formats; index buffers where type and alignment allow.
- Fog, lighting and texture-environment mapped to GE state; document what cannot be expressed.
- Render-to-texture with explicit read-after-write hazard tracking; synchronise only on real
  dependencies.
- Culling: GE-side object culling and static bounds, as in the reference engines.
Exit: a 3D sample at target frame rate with no per-frame full waits.

### Phase 11 — General optimisation outside the renderer
Goal: remove costs that never show up in a draw-call count — compiler and linker flags, raylib's compile-time
feature surface, soft-float `double`, heap allocation and fragmentation, Memory Stick I/O, logging and startup.
`-O2` is the first measured example (33.367 → 16.683 ms at N=1,000, sprint 008); `-O3`/`-Os`, LTO, fast-math and
the `SUPPORT_*` flags in `config.h` are still open. Not in scope: `rlgl`, batching, draw paths, VRAM allocation,
input mapping, the audio backend. Exit: a real frame and a real load with every CPU, I/O, allocation and
GE-wait cost accounted for; no `double` site, allocation, file open or log write in Phase 11-owned code in a
steady frame; flags and config chosen by interleaved A/B at a measurable resolution, with `.text` size recorded.
DaedalusX64's PSP playbook
(`/home/psp/ref/daedalus/AI_MEMORY_MAP.md` §12), checked against its source, supplies code-checked values for the
same levers: `PSP_HEAP_SIZE_KB(-256)`, 333 MHz, VFPU sin/cos/pow but not sqrt/fabs, 64-byte VFPU memcpy, 8 KB
ROM chunks, and partition-5 volatile memory. Its first-fit heap does not coalesce — a counter-example, not a
model. `[external: DaedalusX64]` Sprints 052–059 are written: the harness first, then flags, feature surface,
frame CPU, VFPU, heap/stacks, load path and the startup/close. They are blocked on phases 7–10.

### Phase 12 — Release gate
Four configurations on the same console, same workload, same clocks, and the same Phase 11 build flags:
original pspGL + unbatched raylib; upgraded pspGL + unbatched; upgraded pspGL + batched; native GU. Report
frame-time percentiles, submission counts, GE waits, VRAM high-water, screenshots. Decide from the results
whether anything still needs the direct-GU path. Input and audio are checked in the same run: neither may
appear in the frame profile.

### Phase 13 — Samples porting — **proposed, continuous**
The upstream `core`, `shapes` and `textures` corpus is the regression suite. A 2026-11-21 audit of all 127
samples found **13 drop-in**, **61 that render but need input remapped**, **49 blocked** (16 render targets,
11 texture size/VRAM, 9 input hardware, 8 desktop-only, 2 audio, 2 clipboard, 1 texture readback), and 4 already
ported. A 768-wide sheet (scarfy) fits through two 512 windows; a GIF is a frame-sized image with stacked data,
not a stacked texture. The phase is capability-gated and runs alongside phases 6–11; it never blocks them.
Wave 0 needs no new capability. Sprints 024–034: S1 done (9) and S2 done (12), each with two identical captures;
P13-S7 samples captured; its T1b `batch_check` gate is blocked by a shared `libraylib.a` rebuild. S3–S6/S8–S11
remaining. Full tier lists, gates and the porting recipe:
[Phase 13](phases/13-samples-porting/index.md).

---

## 6. Measurement protocol

- Measure on the PSP-1000 only. PPSSPP answers "does it render", never "how fast" or "how much".
- `reset` in `pspsh` before any memory or performance measurement: `kill` leaks the module's heap and
  leaves later runs in a shrunken partition `[measured]`.
- Build samples with `PSP_LARGE_MEMORY=0`; the PSPSDK default asks for a 40 MB partition on a 32 MB
  console `[measured]`.
- One variable per run. For any speed claim: control, change, control-again, or interleaved variants in
  one run with a confidence interval. One run per arm separates nothing when variance is 10–30 %.
- Log a build number and the counter set at boot; a screenshot without a build number is not evidence.
- A probe must touch what it claims to measure. A counter that can be incremented two ways must be
  split.
- **Keep the shell attached for the whole run, and never send `exit`.** `pspsh -e` exits after issuing the
  command and takes the app's `TRACELOG` output with it; and `exit` typed into pspsh is PSPLink's own
  shutdown command, which drops the console to the XMB. Use
  `( printf './app.prx\n'; sleep 30 ) | pspsh` - pspsh disconnects on EOF and PSPLink stays up. A crash
  whose report was never read looks exactly like a hang `[measured]`.
- **Verified end to end:** two consecutive runs (`run -> DONE -> reset -> run`) both logged
  `DONE frames 600  fps 30` and PSPLink stayed enumerated between them. No console interaction is needed
  between runs `[measured]`.
- **The test must not call `sceKernelExitGame`.** On PSPLink v3.2.1 that ends the shell session and drops
  the console to the XMB, device gone; `resetonexit=0` only suppresses the reset loop. Log `DONE` and
  `sceKernelSleepThread()` instead, then `reset` from the host — that keeps the whole loop host-driven
  `[measured]`.
- **`scrshot` while the app is rendering both freezes it and can return a torn frame.** Two things, both
  measured. Freezing: two captures 2 s apart came back pixel-identical on an animating demo and the frame log
  stopped at the capture. Tearing: the same bunnymark binary on the same console gave **18, 2,169, 2,083, 18
  and 9,381** distinct colours across five captures — the clean ones with RAYWHITE over 55,663 px (background
  + header + text + one box), the torn ones over ~29,900 px because part of the surface was being written
  during the read.
- **Therefore a noisy capture is evidence of neither a bug nor correctness.** Take two captures and require
  them identical before trusting either; the frame to capture is one the app has stopped drawing (`DONE` and
  sleep, or after it exits) `[measured]`.

---

## 7. Reference sources (read-only, outside this repo)

Lives in `/home/psp/ref`. Nothing from here is pasted into this zlib tree.

| source | licence | what to take |
|---|---|---|
| `pspgl-pspdev` | BSD-3 | provenance of the shipped library only |
| `pspgl-themiriron2` | BSD-3 | profiling + configurable list capacity; candidate to adopt in phase 3 |
| `3D-PSP-Tutorials` | **none** | clean-room benchmark designs, ideas only |
| `PGE` (own) | GPL-3.0 | frame lists, VRAM, swizzle, RTT — ideas |
| `openTRI` (own) | GPL-2.0 | best-fit VRAM, residency, vertex rings — ideas |
| `PSPRuntime` (own, private) | none in repo; `vram.c` is Raphael 2007 "free to use with credit" | working GU renderer with raylib's API: batching structure, viewport idiom, best-fit allocator, bunnymark baseline |
| `daedalus` | GPL-2.0-or-later | PSP performance playbook (`AI_MEMORY_MAP.md` §12), VFPU math/copy, heap and ROM-cache patterns — ideas only |

**Take, with attribution rules in mind:** 1024-sprites-per-call batching structure; the pixel-exact 2D
viewport idiom; direct-swizzle upload; the 512-byte-block best-fit allocator; its bunnymark as a
direct-GU baseline once fixed. From Daedalus, take the non-renderer levers as ideas — heap sizing, clock and
FP-exception setup, VFPU sin/cos/pow versus FPU sqrt/fabs, 64-byte VFPU copies, chunked ROM/asset cache and
partition-5 volatile memory; do not copy its GPL code.

**Do not take:** per-frame `sceGuFinish` + `sceGuSync(FINISH,DONE)` then a vblank wait; a single
unrotated list with silent overflow; a depth buffer on a 2D renderer; two allocators over the same
EDRAM; `free()` on EDRAM pointers; global swizzle mode that forces every texture to the framebuffer
PSM.

Licence note: `PGE` and `openTRI` are the same author's code but GPL — dual-licence them or implement
the techniques clean-room if they are to enter this tree. `3D-PSP-Tutorials` has no licence at all.

---

## 8. Decision log

| date | decision | evidence |
|---|---|---|
| 2026-11-20 | **Adopt `TheMrIron2/pspgl` @ `ae9a668` with `PSPGL_DLIST_SIZE_WORDS=8192`** | A/B: 90.29 → 81.24 ms for the commits (−10.0 %), then 81.24 → 61.63 ms for the list size (−24.1 %); total −31.8 %. Superset ABI (287 + 6 symbols), BSD-3. Reproducible via `tools/pspgl-build.sh` (content hash `2ab7378e…`, stable across rebuilds except two `__DATE__` objects). Costs 512 KiB of main RAM |
| 2026-11-20 | Instrumentation counts at the GL boundary via `-Wl,--wrap` in the test harness, **not** inside `rlgl.h` | keeps rlgl byte-identical to upstream ahead of the planned native rGL backend, and counts what crosses into pspGL rather than what raylib intended; the same harness re-points at `sceGuDrawArray` in phase 5 |
| 2026-11-20 | Merged `psp-6.2` into `master` (6.5-dev) at the user's request | 0 textual conflicts; merged tree compiles (0 errors, 0 warnings), links `basic_shapes.prx` (1,415,522 B) and **runs on the device**: 600 frames, 30 fps, 284 colours, no exception |
| 2026-11-20 | Base on release tag 6.2, not `master` (6.5.0-dev) | stable baseline; full history keeps `git merge` available |
| 2026-11-20 | Start from upstream raylib, not from the port repos | `raylib4Psp` holds no library code; fork `master` is pure upstream 6.0; the PSP delta is one platform file |
| 2026-11-20 | Stage A on pspGL before any native backend | gives a working 6.2 to compare against, and the GL11 call list becomes the GU spec |
| 2026-11-20 | Do not run `pspbuild.sh` | would rebuild older 5.5 sources over a matching pspdev library |
| 2026-11-20 | Optimise raylib submission before optimising pspGL internals | per-sprite submits cost 512–1024x the necessary draw calls; fix the structural problem first |
| 2026-11-21 | Track the upstream `core`/`shapes`/`textures` corpus as **Phase 13**, gated by capability | Static audit of 127 samples: 13 drop-in, 61 input-gated, 49 blocked, 4 ported. Each wave names the phase whose capability it consumes; wave 0 needs none. Findings, gates and sprints 024–034 in [Phase 13](phases/13-samples-porting/index.md) |

---

## 9. Traps that will bite here

- **CPU writes vs GE reads.** Vertex data, textures and lists the GE reads must not be rewritten until
  it is done with them. Per-list arenas with explicit lifetimes, or a sync. `[measured: other projects]`
- **Data cache.** After writing vertex/texture memory the CPU wrote, `sceKernelDcacheWritebackRange()`
  over exactly that range, then publish the pointer.
- **`sceGuGetMemory` cannot fail.** It has no capacity check; the NULL checks people write are dead
  code and overflow walks off the end of the list. Size the list from a real bound and assert.
- **TEXCOORD is unsigned 16-bit.** Zero-centring UVs puts half the vertices across the wrap seam.
- **Vertex colour is `0xAABBGGRR`**, not RGBA.
- **A batched `GL_QUADS` run is one triangle fan in pspGL.** `pspgl_varray.c` maps `GL_QUADS` to
  `GE_TRIANGLE_FAN`, so a 4,000-vertex quad run fans from vertex 0 and the screen fills with spikes that
  all originate at the first sprite. `glBegin(GL_QUADS)` hides this because it emits one fan per quad.
  Expand quads to triangles yourself; the extra two vertices per sprite are the price `[measured:
  batch_check on a PSP-1000: 240-vertex quad run drew a single fan]`.
- **s16 positions are normalised.** Pair every s16 vertex buffer with a scale matrix.
- **float24 matrices.** Big world coordinates tremble; render camera-relative.
- **No X/Y clip.** A primitive with a vertex outside the window is dropped whole; tessellate.
- **`MEMSIZE` default.** PSPSDK writes `MEMSIZE=2` (40 MB request); set `PSP_LARGE_MEMORY = 0`.
- **Heap accounting.** The libc heap is taken during C runtime init, before `main`, so counters read
  inside `main` never show the pre-heap figure. Declare `PSP_HEAP_SIZE_KB` and interpret accordingly.
- **Blocking audio on the main thread.** `sceAudioOutput*Blocking()` paces itself to playback: call it
  in the frame loop and the frame time becomes the audio buffer length. Audio gets its own thread.
- **A thread stack comes from the user partition, not the heap.** A thread created after boot fails
  with `0x80020190` while the log shows megabytes free. Reserve stacks early `[measured]`.
- **Threads touching the VFPU need `PSP_THREAD_ATTR_VFPU`**, including the audio and loader threads if
  they use `-ffast-math` code paths `[public: pspthreadman.h]`.
- **Nub deadzone is not a constant.** Values assume 128 is centre; many units do not. Calibrate.
- **`reset`, not `kill`.** See §6.

---

## 10. Compute units beyond the main CPU

Two things a desktop port never has: a vector coprocessor on the main CPU, and a second CPU. Both are
optional and both have rules that each cost a debugging session.

### 10.1 VFPU — COP2, 128 single-precision registers

| fact | value | tag |
|---|---|---|
| Register file | 8 x 4x4 matrices (M000..M700), single precision only | `[public: vfpu-docs]` |
| Special functions (`vrcp vrsq vsqrt vsin vcos vexp2 vlog2 vasin vrot`) | table-driven quadratic interpolation, ~22 significant bits, truncated not rounded, deterministic | `[public: PPSSPP "VFPU math"]` |
| Accuracy | `vrcp` 6.3e-7 rel, `vrsq` 7.3e-7, `vsqrt` 7.1e-7, `vsin` 4.8e-7 abs, `vcos` 4.0e-7 abs, `vlog2` **3.0e-5**, `vasin` up to 2e-2 near ±1 | `[public: vfpu-docs]` |
| Angle unit | quarter turns — `vsin(x) = sin(x*pi/2)`; convert with `vcst.s VFPU_2_PI`, reduce the angle first (Cody-Waite) | `[public: vfpu-docs]` |
| Cycle cost | `vadd`/`vmul` 1, `vdot` 1.2, special `.s` 1, `vmmul.q` 16, `vtfm4.q` 4, `vdiv.s` 14 (use `vrcp`+`vmul`), `vt5551.q` 1 per four pixels | `[public: vfpu-docs, PSP-3000]` |
| Memory | `lv.q` ~15 B/cycle from D-cache, ~2 B/cycle from RAM; `lw` ~0.9 B/cycle from RAM | `[public: vfpu-docs, PSP-1000]` |

**Build-time traps (from the blueprint's porting notes, §15.6):** `__builtin_vfpu_*` and `__builtin_allegrex_*`
are Sony-compiler intrinsics and pspdev GCC 15.2.0 does not provide them — use `pspgum`, `pspvfpu.h` or inline
asm. `libgum_vfpu.a` is GUM compiled with `-DUSE_VFPU=1` and is not the default GUM. Reserve matrices with
`pspvfpu_initcontext()` / `pspvfpu_use_matrices()` before issuing VFPU instructions, and keep 16-byte alignment
(`ScePspFVector4` / `ScePspFMatrix4`). `[external: blueprint §15.6]`

Rules: a thread needs `PSP_THREAD_ATTR_VFPU` or its state is corrupted across switches (PPSSPP will not
tell you). gcc does not model VFPU registers — never keep a value in one across `__asm__` blocks. pspsdk's
`libpspgum_vfpu` uses matrices 0–3, so helpers should take 6–7. `vmmul`/`vtfm*`/`vqmul` need destination
distinct from sources; `vhtfm2.p`/`vhtfm3.t` are broken at register index ≥ 64; on a PSP-1000 `lvl.q`/
`lvr.q` corrupt an FPU register.

**Where it pays for us:** `vt5551.q`/`vt5650.q` convert four RGBA8 pixels per cycle during texture upload —
the reference engine does that per pixel in C, so this is the clearest single win. `vrot.p` for 2D rotation
maths and particle spawn, `vrsq` for normalisation, `vexp2`/`vlog2` for fog and falloff curves. Anything
bulk that ends up as GE-bound data.

**Where it does not:** values accumulated frame after frame (camera yaw, timers, physics) — the truncation
bias walks. Per-vertex skinning — the GE skins in hardware. Anything compared bit-for-bit with PC tools —
compare at ≥ 2^-22 relative. And before optimising maths at all, count soft-float `double`: every double
operation is a libgcc call (5,633 `__adddf3`-class call sites were counted in one PC port).

### 10.2 Media Engine — the second MIPS core

| fact | value | tag |
|---|---|---|
| Throughput | same clock as the main CPU; splitting work gave x2.0–2.9 on a 1000 and a 3000 | `[public: psp-me-bench]` |
| Caches | own 16 KB 2-way D-cache, 64-byte lines, **no coherence** with the main CPU | `[public: me-tiny-lib]` |
| Local memory | eDRAM at ME address 0: 2 MB on a 1000, 4 MB on 2000+; shared boot RAM holds exception vectors | `[public: PSPTEK]` |
| VFPU | **none** — scalar integer/FPU only | `[public: me-tiny-lib]` |
| Firmware users | AVC decode (`sceMpeg`), ATRAC, the `sceSas` mixer — a reset-vector takeover disables all three for the session | `[public: PPSSPP]` |
| Cost to start | kernel mode required; `kuKernelCall` through kubridge works on ARK | `[measured: other projects]` |
| Emulator | PPSSPP does not execute ME code — every ME claim needs a hardware run | `[public: PPSSPP]` |

**Worth moving, measured:** audio mixing/resampling took 5.8 ms per frame off the main CPU, and audio cost
fell from 135 to 30 ms per second of game time; building GE display-list words and culling also worked with
zero failures. **Not worth it:** anything VFPU-shaped, branchy code with file I/O or syscalls, or a session
that still needs firmware decoders.

**Rules:** set `$sp` in the stub — there is no stack after reset; no libc, no syscalls, no GE on the ME; an
exception on the ME is a silent hang, so it needs a handler and the main CPU needs a watchdog; reloadable
buffers need a publication counter, not a one-time invalidate; cross-core queues are generation-guarded
single-producer/single-consumer in uncached memory; suspend/resume must park the ME (`scePowerLock(0)` is the
cheap fix, a sysevent handler is the full one).

**Qualify in steps:** heartbeat → shadow mode (both CPUs compute, hashes compared) → live with a deadline
watchdog that hands work back to the main CPU.

Where this touches our roadmap: Phase 9 (audio) is the natural first customer, Phase 10 (3D) the second.
Neither is a premise for earlier phases.

---

## 11. Local source index

Everything below is on this disk. Search it before designing anything; the answer is usually already
written down.

### 11.1 Our code

| path | what | open it for |
|---|---|---|
| `/home/psp/raylib-psp` | this fork, branch `psp-6.2` | the work |
| `/home/psp/raylib-psp/docs/psp/memory-map.md` | this file | — |
| `/home/psp/raylib-performance-doc.md` | another agent's performance plan (written against 5.5) | phasing ideas, counter lists; verify claims against 6.2 |
| `/home/psp/hello-psp/NOTES.md` | environment notes: toolchain, device, PSPLink quirks, measured facts | before any hardware run |
| `/home/psp/hello-psp` | the instrumented hello world, `deploy.sh`, NOTES | a known-good EBOOT to compare against |

### 11.2 raylib sources

| path | what | open it for |
|---|---|---|
| `/home/psp/raylib4Psp` | wrapper repo: build scripts + five samples, submodule pinned at 5.5 | the regression corpus; `samples/*/*/Makefile` shows the link line |
| `/home/psp/raylib4consoles` | `master` = pure upstream 6.0; `raylib4Consoles` = 5.5 + PSP platform | the PSP platform file and its build glue (`git show consoles/raylib4Consoles:src/platforms/rcore_psp.c`) |

### 11.3 GU engines and libraries (read-only)

| path | licence | open it for |
|---|---|---|
| `/home/psp/ref/PSPRuntime` | none in repo; `vram.c` is Raphael 2007 "free to use with credit" | batching structure (`PSPBasicPlatformer/src/gpumanager.cpp`), viewport idiom, best-fit allocator (`src/vram.c`), bunnymark baseline |
| `/home/psp/ref/pspgl-themiriron2` | BSD-3 | **adopted**: `ae9a668`, built by `tools/pspgl-build.sh` at `PSPGL_DLIST_SIZE_WORDS=8192`. Content hash `2ab7378e…`; `--check` verifies the installed library |
| `/home/psp/ref/pspgl-pspdev` | BSD-3 | provenance of the shipped `libGL.a` (`de4260ad`) |
| `/home/psp/ref/pspgl-legacy` | BSD-3 | historical only |
| `/home/psp/ref/PGE` | GPL-3.0 | frame-list and VRAM ideas (own code, ideas only unless dual-licensed) |
| `/home/psp/ref/openTRI` | GPL-2.0 | allocator, residency, particle rings (same constraint) |
| `/home/psp/ref/daedalus` | GPL-2.0-or-later | `AI_MEMORY_MAP.md` §12 PSP performance map; `SysPSP/main.cpp`, `Math/Math.h`, `Utility/FastMemcpyPSP.cpp`, `Utility/MemoryHeap.cpp`, `RomFile/` — ideas only |
| `/home/psp/ref/3D-PSP-Tutorials` | **none** | clean-room benchmark designs; never copy |

### 11.4 SDK, toolchain, skill

| path | open it for |
|---|---|
| `/home/psp/pspdev/psp/sdk/include` | the API truth: `pspgu.h`, `pspgum.h`, `pspaudio.h`, `pspctrl.h`, `pspthreadman.h`, `pspvfpu.h`, `pspiofilemgr.h` |
| `/home/psp/pspdev/psp/sdk/samples` | working examples, including `me/basic` (reset-vector takeover) |
| `/home/psp/pspdev/psp/lib`, `.../sdk/lib` | shipped static libs: `libGL`, `libpspaudio`, `libpspaudiolib`, `libpspgu`, `libpspgum`, `libraylib` |
| `/home/psp/psp_sdk_660` | Sony's official devkit SDK, kept as a reading aid — see below |
| `/home/psp/psp_sdk_660/AI_MEMORY_MAP.md` | **not Sony material**: an AI-written map of that tree. **§16 is a renderer blueprint** (fixed constraints, VRAM map, 2D/3D paths, batching, levers, acceptance criteria) and §15 is a GU/VFPU symbol index. Summarised into [Phase 4](phases/04-rlgl-batcher/index.md) and [Phase 10](phases/10-3d-advanced/index.md) |
| `/home/psp/.kimi-code/skills/psp-developer/references/` | the PSP skill: `rendering.md`, `textures-uv.md`, `ge-arithmetic.md`, `vfpu-math.md`, `media-engine.md`, `audio.md`, `memory-map.md`, `file-io-memory-stick.md`, `instrumentation.md`, `optimization-profiling.md`, `real-hw-gotchas.md`, `power-exit-sleep.md` |

#### The Sony SDK: what it is for, and what it is not

`/home/psp/psp_sdk_660` is the official devkit SDK, extracted from `psp_sdk_660.7z` (which is kept beside
it): 1.6 GB, 9,384 files, 2017 vintage. Top level is `installers/`, `usr/` and a readme; inside
`usr/local/psp/devkit/` are `bin.linux`, `bin.windows`, `contrib` (a GIM Photoshop plugin), `data` (models,
DVD-UMD images) and `tool` (a VAG converter shipped as Windows `.exe` binaries).

**Use it as a reading aid only.** Specifically:

- Not a dependency: `PSPDEV` stays `~/pspdev`. Nothing links against this tree, and no build flag points at it.
- Nothing from it is copied or paraphrased into this tree — no header, no documentation text, no source.
- A fact that lives only there is not a fact we can put in code. Reproduce it from a public source (pspsdk
  headers, PPSSPP, ARK) or measure it on the console, and cite that instead.
- Legitimate uses: cross-checking a signature or field name when the open headers are ambiguous, understanding
  what an official tool was meant to do, and picking a behaviour worth testing on hardware.
- Its Windows-only tools are not needed: `atracdenc`, `ffmpeg` and pspdev's converters cover the same ground.

The reason for the discipline is not squeamishness: this tree is zlib-licensed and upstream-mergeable, and
code with unclear provenance cannot go into it without poisoning both properties.

### 11.5 Not on disk — clone on demand

Cited by the skill and by §10, none of them cloned yet:

- `mcidclan/me-tiny-lib`, `me-minimal-handler`, `me-soft-interrupt`, `me-spinlock`, `psp-me-bench`,
  `psp-media-engine-reload`, `psp-media-engine-safe-task`, `psp-media-engine-custom-core` — ME work
- `mcidclan/vfpu-render-565-from-8888` — VFPU colour conversion sample
- `pspdev/vfpu-docs` — instruction cycles, accuracy, errata
- `hrydgard/pspautotests` — behavioural tests (`cpu/vfpu/exact`)
- `uofw/uofw` — firmware behaviour, `me_wrapper`
- PSPTEK hardware notes — memory map, system controller registers

### 11.6 Search recipes

```sh
# how a GU feature is actually driven
grep -rn "sceGuTexImage\|sceGuDrawArray\|sceGuGetMemory" /home/psp/ref/PSPRuntime/PSPBasicPlatformer/src
# a pspGL fast path and whether the shipped copy has it
grep -rn "glLockArraysEXT\|glPrioritizeTextures" /home/psp/ref/pspgl-themiriron2/
# VFPU usage that survives pspdev gcc
grep -rn "vrsq\|vt5551\|vrot" /home/psp/ref/ /home/psp/pspdev/psp/sdk/samples
# what the platform interface really requires
grep -n "^[a-zA-Z].*(" /home/psp/raylib-psp/src/platforms/rcore_template.c
# audio channel and SRC signatures
grep -n "sceAudio" /home/psp/pspdev/psp/sdk/include/pspaudio.h
```

---

## 12. Open questions

1. Track 6.2 or 6.5.0-dev? Merging later is a normal merge, but every platform-interface change is
   rework. Revisit after phase 1.
2. Does the PSP platform file ever go upstream? A `PLATFORM_PSP` in raylib proper would end the fork's
   maintenance burden; the GL11 guards are the same trade-off upstream accepted for other consoles.
3. Is pspGL a permanent dependency or a bootstrap? Phase 5 assumes bootstrap; phase 3 may show the
   upgraded library is good enough for 2D.
4. Which PSM for the framebuffer: 5551 (cheaper, half the bandwidth) unless dithering or specific
   blending forces 8888 — decide in phase 5 by screenshot comparison, not preference.
5. Pad only, or pad plus keyboard emulation? Feeding `CORE.Input.Keyboard` makes upstream examples run
   unmodified but redefines `IsKeyDown` on this platform. Decide in phase 8, document it either way.
6. Audio route: miniaudio custom backend (keeps `raudio.c` closer to upstream) or a PSP device layer
   written straight into `raudio.c` (less abstraction, more fork). Phase 9 starts by measuring which
   costs less to maintain against 6.x.
7. How much audio work belongs on the Media Engine? It can decode while the CPU draws, but it is a
   second CPU with no libc and its own cache protocol — an optimisation for phase 9, never a premise.
8. Does anything in the asset path need CLUT/T4/T8 textures, or is RGBA8 + POT enough? The reference
   engine ran everything through 5551 and used T4 only for glyph atlases.
9. **Resolved** — bunnymark's "noise" was a torn capture, not a rendering fault: the same binary gave
   18 and 9,381 colours on different runs. See the capture rules in §6. The question remains open only for the
   throughput numbers, which sprint 003's counters will make meaningful.

---

## 13. Working method: sprints, features, recipes

Three document types, three different jobs. Templates live in `docs/psp/templates/`; the filled copies go
in sibling directories and render in this site.

| type | answers | lives in | template |
|---|---|---|---|
| **Sprint** | what we do in one time box, and which number is supposed to move | `docs/psp/sprints/NNN-phaseN-slug.md`, labelled `P<N>-S<n>` | [sprint](templates/sprint.md) |
| **Feature** | what behaviour a game gets: API surface, cost model, degradation | `docs/psp/features/slug.md` | [feature](templates/feature.md) |
| **Recipe** | a technique that already worked on the console | `docs/psp/recipes/slug.md` | [recipe](templates/recipe.md) |

Rules that keep this honest:

- A sprint belongs to **exactly one** phase of §5, and its "why now" names the exit criterion it advances.
  Two phases in one sprint means the measurement cannot be attributed.
- **Every sprint opens with a handover section (§0)** written for a reader with no chat history: situation,
  what to read first, the first concrete action, hard rules, copy-pasteable commands, what not to touch. If a
  sprint doc only makes sense to the session that wrote it, it is not finished — an agent or a colleague
  starting from it should be able to act without asking anything.
- A feature is designed before it is coded. Its §5 cost model is written first and then either confirmed or
  corrected by a measurement — a model that was only ever right in hindsight is not a model.
- A recipe is written **after** the technique ran on hardware. Before that it is a hypothesis and belongs in
  a sprint. An untested recipe is worse than none: it will be trusted.
- Templates are read-only. Copy, fill, leave the template intact.
- Results flow back here: facts to §2/§3/§9, decisions to §8, new unknowns to §12.
- Nothing in a sprint or feature doc may contradict this file. If it does, one of them is wrong; resolve it
  before starting the work rather than after.

### Task tracking

Tasks live in a table with a stable ID, one of five status words, a verification and an evidence cell. The ID
is what makes a task referenceable from outside the document — from a commit message, a measurement row, or a
note in another sprint.

| status | means |
|---|---|
| `todo` | not started |
| `doing` | in progress, nothing verified yet |
| `done` | verification ran and the evidence cell is filled (commit, measurement row, screenshot) |
| `blocked` | cannot proceed; the log line names the blocker and what would unblock it |
| `dropped` | no longer applies; the log line says why. IDs are never renumbered or reused |

This is a static documentation site, so the checkboxes in older phase pages are **not** interactive and nothing
persists a click. Tracking is therefore explicit text, which is a feature: it lives in git, so the status of
every task at any past commit is recoverable. What it is *not* is a project tracker — no assignee, no due date,
no notification. If a sprint needs those, use GitHub Issues and mirror the IDs into the sprint doc; do not
invent a second status system here.

Two audit greps that stay useful — anchored to table rows, so the vocabulary line above is not counted:

```sh
grep -c '^| T[0-9]* |.*`todo`'   docs/psp/sprints/*.md   # open work per sprint
grep -n '^| T[0-9]* |.*`blocked`' docs/psp/sprints/*.md  # needs a decision, not more work
```

The phase pages keep plain checklists. They track phase progress, not individual work; the trackable units are
the sprint tasks.

Build note: MkDocs reserves `/templates/` **at the docs root** — `_default_exclude` in
`mkdocs/structure/files.py` is `['.*', '/templates/']`, and files there are collected but built as
"excluded from the built site", with a link warning that `--strict` does not fail on. That is why these
templates live under `psp/`. Do not create `docs/templates/`.

---

## 14. What pspdev actually provides (verified 2026-11-20)

Checked against the toolchain we link, not against documentation. Paths are under `/home/psp/pspdev/psp`.

### Headers and libraries

| item | path | notes |
|---|---|---|
| GU API | `sdk/include/pspgu.h` | 1,000+ lines, heavily commented |
| GUM (matrix/scene) | `sdk/include/pspgum.h` | |
| VFPU context | `sdk/include/pspvfpu.h` | |
| GE + EDRAM | `sdk/include/pspge.h` | |
| scalar FPU + math helpers | `sdk/include/pspmath.h` | `libpspmath.a` |
| `libpspgu.a` | `sdk/lib` | 953,488 B |
| `libpspgum.a` | `sdk/lib` | 222,440 B — **the default**, what every sample links |
| `libpspgum_vfpu.a` | `sdk/lib` | 225,846 B — opt-in `-DUSE_VFPU=1` build; **no sample links it** |
| `libpspvfpu.a` | `sdk/lib` | 9,772 B |
| `libpspge.a` | `sdk/lib` | EDRAM/GE control |

A GU program links `-lpspgum -lpspgu` (that is the samples' line). GUM usually needs `-lm`-equivalent helpers
from `libpspmath` only if it is called directly.

### GU: what exists, verified by signature

- `int sceGuStart(int ctype, void* list)` — **two arguments** in PSPSDK. Sony's three-argument form does not
  exist here, and a port that keeps it will not compile.
- Primitives: `GU_POINTS 0`, `GU_LINES 1`, `GU_TRIANGLES 3`, `GU_TRIANGLE_STRIP 4`, `GU_TRIANGLE_FAN 5`,
  **`GU_SPRITES 6`** — the 2-vertex axis-aligned sprite primitive is real and available.
- Transform modes: **`GU_TRANSFORM_2D`** ("coordinate is passed directly to the rasterizer") and
  `GU_TRANSFORM_3D` ("transformed before passed to rasterizer"). Both exist; the header states the 2D form
  bypasses the transform, which is the through mode a 2D batcher wants.
- Vertex format flags: `GU_TEXTURE_16BIT`, `GU_TEXTURE_32BITF`, `GU_COLOR_5551`, `GU_COLOR_8888`,
  `GU_VERTEX_8BIT`, `GU_VERTEX_16BIT`, `GU_VERTEX_32BITF`, `GU_NORMAL_8BIT`, `GU_INDEX_16BIT`,
  `GU_WEIGHTS(n)` for 1–8 bones.
- `sceGuScissor(int x, int y, int w, int h)` — width/height, **not** an end coordinate.
- Textures: `sceGuTexImage(mipmap, width, height, tbw, tbp)`, `sceGuTexMode(tpsm, maxmips, mc, swizzle)`,
  `sceGuTexFlush()`, `sceGuTexSync()`, `sceGuClutLoad(num_blocks, cbp)`, `sceGuTexOffset(u, v)`,
  `sceGuTexScale(u, v)`.
- Depth: `sceGuDepthRange(near, far)`, `sceGuDepthOffset(offset)`.
- Matrices/bones: `sceGuSetMatrix(type, matrix)`, `sceGuBoneMatrix(index, matrix)`.

### GUM: CPU-side matrix maths, verified

`sceGumDrawArray(prim, vtype, count, indices, vertices)`, `sceGumMatrixMode`, `sceGumLoadIdentity`,
`sceGumOrtho`, `sceGumPerspective`, `sceGumLookAt`, `sceGumRotateXYZ`, `sceGumScale`, `sceGumPushMatrix`,
`sceGumPopMatrix`, `sceGumUpdateMatrix`. GUM computes matrices on the CPU and hands them to the GE — the
comment in `pspgu.h` is explicit that `sceGuDrawArray` does *not* apply the affine transform while
`sceGumDrawArray` does.

### VFPU: available, but not as intrinsics

- Context management: `pspvfpu_initcontext()`, `pspvfpu_use_matrices(ctx, ...)`, `pspvfpu_deletecontext()`,
  and `VFPU_ALIGNMENT` (= 16 bytes) for matrix loads/stores.
- **`__builtin_vfpu_*` and `__builtin_allegrex_*` do not exist in psp-gcc 15.2.0** — verified with
  `__has_builtin`, which answered no. VFPU work is inline asm (`mtv`/`mfv`/`vrsq`/`vrot`…), as the
  [compute-units section](#101-vfpu-cop2-128-single-precision-registers) describes.
- `libpspgum_vfpu.a` is the GUM built with VFPU paths. It is not the default and no sample uses it; adopting it
  is a deliberate choice and must be A/B measured like anything else.

### VRAM: 2 MB is the whole budget on this console

- `sceGeEdramGetAddr()` returns the base (`0x04000000`), `sceGeEdramGetSize()` the size.
- `sceGeEdramSetSize(int size)` takes `0x200000` or `0x400000`, and the header states **0x400000 returns an
  error on the PSP FAT** — our PSP-1000. So the "upper 2 MB" the blueprint mentions as opt-in is not available
  to us at all; 2 MB is the budget, of which the pspgu display (three framebuffers) and depth take **1,088 KiB**.
- **The command lists and vertex arenas are host RAM, not EDRAM.** pspgu keeps two 240 KiB display lists and
  two 128 KiB per-list vertex arenas in main memory; only framebuffers, depth and textures belong in the 2 MiB
  sum. A budget that adds the lists to VRAM double-counts memory in the wrong partition.
- Per-draw vertex allocations are **64-byte aligned** because the GE reads them after a D-cache writeback. With
  16-byte starts a draw could share a dirty cache line with its neighbour; `basic_shapes` then differed from
  pspGL by 26–71 edge pixels. The alignment made it byte-identical (sprint 014 T7).
- **Non-power-of-two textures are padded to powers of two in both dimensions** (height at least 8 rows for the
  8-row swizzle), the padding is zeroed and swizzled, and `sceGuTexScale(realW/padW, realH/padH)` restores the
  logical size. The padding is allocated and counted: 33×17 becomes 64×32, **8,192 B** against 2,244 B of
  logical texels (sprint 015 T1).
- **The pspgu allocator is a 512-byte block best-fit over the whole 2 MiB**: 4,096 blocks, 32 KiB of metadata in
  main RAM, coalescing on free. Framebuffers and depth are fixed 8 KiB-aligned reservations; textures allocate
  and free. It reports `used / free / largest / internal / allocs / frees / failures`, and `largest` is a real
  span — not the free tail the old bump arena could report. A 4-byte request costs one 512-byte block
  (sprint 015 T3: churn 100 × 16 textures returned free and largest exactly, 1,600 frees, 0 failures).
- **Texture-only work must end a frame.** The display list is 240 KiB and GU has no capacity check; a probe that
  uploaded 1,600 textures without `EndDrawing` overran it at cycle ~50. End a frame, or add a list high-water
  guard before a loading screen uploads textures in bulk.
- **Per-texture PSM is a memory lever, not a speed one.** pspgu defaults to 8888 for pixel parity;
  `PSP_GU_TEXFMT=auto` chooses 5650 for opaque `GL_RGB` and 4444 for `GL_RGBA`/`GL_LUMINANCE_ALPHA` whose alpha is
  entirely 0/255, otherwise 8888. A 32×32 `GL_RGB` texture is 2,048 B instead of 4,096; the 128×128 LA font is
  32,768 instead of 65,536 (its alpha is binary). The measured `auto` delta is 7,843 px in `batch_check` and 0 in
  `basic_shapes`; the default build does not move (sprint 015 T2).
- **Residency is priority + age, with a main-RAM shadow.** Each texture keeps a packed linear shadow (same bytes as
  its PSM allocation, main RAM), a 0–1 `glPrioritizeTextures` priority and a last-used frame. Allocation failure
  evicts the lowest priority, oldest first; a texture used in the last 2 frames is pinned so an in-flight list is
  never sampled from freed VRAM. A bind of a non-resident texture reloads it from the shadow. Counters live in the
  `RESIDENCY` line. Measured: fit 12×128² **0 evictions**; over 28×128² **3,363 evictions / 3,348 reloads / 0
  failed**; priority 1.0/0.1 swaps the victim; fragmentation coalesces a 256² after 3 evictions. Shadows measured
  0.85–1.90 MB of main RAM, not VRAM (sprint 016).
- **Full texture uploads use an 8-row band**, not a full-image scratch: the band is ≤16 KiB at 512×8888, the
  source reads stay sequential, and the swizzled blocks are written to a cached EDRAM base followed by one exact
  range writeback. A 256×256 8888 upload measured **21.088 → 19.849 ms** with the heap allocation removed. A
  literal 128-byte block path through the uncached alias measured 26.2 ms and was rejected. Widths above 512 are
  **not rejected**: the block is allocated and the texture marked used, but the swizzled upload returns early
  (`gl_texture.c:231`) and the GE samples uninitialised VRAM. A wide-but-short texture (640×64 8888 = 262,144 B,
  `tbw` 1024) therefore fits and is silently wrong. Phase 7 sprint 020 splits or fails loudly (sprint 018 audit).
- **Upload cost with the residency shadow (b23, 30-run median):** 8888 32² 4,096 B/0.499 ms, 64² 16,384/1.998,
  128² 65,536/8.228, 256² 262,144/33.332. Logical 384² and 512² pad to 512² = 1,048,576 B and **do not fit** the
  916,992 B free tail. `auto` halves the bytes for 5650/4444: 512² is 524,288 B at 113–130 ms.
- **2D workload baseline (b23, `psp-tests/scene_2d`, 300 frames, two runs each):** text 63/548/1,637 glyphs is
  **1 draw/frame** at all three (3,274 vertices at the top point, under the 4,096 cap) and costs
  1.226/6.827/19.680 ms work; 510 tiles is 1 draw/3.228 ms; 500/2,000 particles is 1 draw at 3.285/11.844 ms
  with 0.443/1.775 ms simulation; 3,000 shapes is **4 draws, 10,000 vertices, 1 bind + 1 unbind per frame** at
  21.79 ms. Four textures in one frame (mixed) is 4 draws and 3 binds. Boot free 847,360–916,992 B, largest =
  free, 0 evictions/reloads/failures. Immediate `glBegin/glEnd` paths are fully bypassed by the batcher (sprint
  018).
- **Text run path and font atlas guard (sprint 019, b23):** `DrawTextEx` now emits **one bind/begin/end per call**,
  `GetGlyphIndex` is O(1) for sequential fonts and cached for unordered ones, and per-glyph UVs/metrics are cached
  per font (keyed by glyphs, recs, count, base size, padding, texture id and dimensions). Work at 1/8/24 lines:
  **0.686/3.350/9.069 ms** (was 1.226/6.827/19.680, **−44/−51/−54 %**), captures byte-identical, gate
  `b69830b8…`. The 4,096-vertex cap gives 32/48 lines **2 draws** at 11.977/18.233 ms with no fixed penalty;
  raising it would need 196,608 B of batch memory and a larger arena, so it stays 4,096. A generated font atlas
  above 512 in either dimension is rejected in `LoadFontFromMemory` (one error, default font, no texture);
  KAISG 32 px = 512×256, KAISG 64 px = 1024×512 rejected. Two 512×256 8888 atlases do not fit the ~917 KiB tail
  (recorded: 3 evictions / 1 reload / 300 failed draws).
- **512-pixel windows for wide textures (sprint 020, b24):** the raw-GU probe proves full-stride windows in
  **linear and swizzled** layouts, horizontal and vertical (1024×128 at `tbw=1024`, 128×1024 at `tbw=128`). The
  upload chunks the 8-row band at 512 px, so `tbw=1024` keeps a 16 KiB stack; `padW/padH>1024` logs one error and
  creates no texture. `glDrawArrays` splits sprites and clipped triangles into ≤512 windows with UV remap and
  per-window `sceGuTexImage`/`TexScale(1,1)`; contiguous same-window pieces batch. Measured: 1024×128 = **2 window
  draws/frame**, rotated = 4, wide 1024×128 tile atlas (28 tiles) = **8 draws and 1 bind** vs 8 binds for the
  two-texture reference; captures are byte-identical to the pre-sliced references (`39d6547a…`, `ec18cdf7…`,
  `c34c01ce…`). The ≤512 path is unchanged (`windraws=0`), the gate is `b69830b8…`, and 512×512 8888 (1 MiB) does
  not fit beside the font, so the boundary was tested at 512×256.
- **Sprite culling and 3-vertex triangles (sprint 021, b24):** `DrawTexturePro`/`DrawRectanglePro` reject an
  unrotated draw wholly outside the screen before its corner math (`PSP_CULL=1`); 10,000 sprites at 50 %
  off-screen drop from **20,000 to 10,000 submitted vertices**, 5 to 3 draws, and **48.67 to 35.12 ms** work with
  an identical capture. On PSP, `DrawTriangleGradient` uses the 3-vertex untextured `RL_TRIANGLES` branch
  (`PSP_TRI_FAST=1`) instead of a 4-vertex quad: the shape scene goes from **10,000 to 7,000 vertices**, 4 to 3
  draws, **17.78 to 12.81 ms** work, and its median from **33.4 to 16.68 ms** (60 fps), capture byte-identical
  (`b9cd43b6…`). Shapes bind the shapes texture once per frame, not per object. Gate `b69830b8…`.
- **CLUT and mipmaps are dropped for phase 6, with measurements.** A 128×128 T4 glyph atlas is 8,192 B + 64 B
  palette vs 32,768 B at 4444, and a raw copy is 0.080 vs 0.532 ms — but that is 2.7 % of free and no workload is
  VRAM-bound. A 256×256 mip chain costs +33 % (349,504 vs 262,144 B) and produced an identical capture at 4× in
  the raw-GU probe. 3D mipmaps are phase 10; the CLUT can be revisited if a UI working set becomes tight.

### Samples worth reading

`psp/sdk/samples/gu/`: `beginobject`, `blend`, `blit`, `celshading`, `clut`, `copy`, `cube`, `doublelist`,
`envmap`, `font`, plus `common/`. They are short, they compile with our toolchain, and they show the idiomatic
call order for each feature. `doublelist` is directly relevant to phase 5's list rotation.

### Blueprint claims, checked

| blueprint claim | verified against pspsdk | verdict |
|---|---|---|
| `GU_SPRITES` = 2-vertex axis-aligned rect | `GU_SPRITES 6` exists | **confirmed** |
| through mode for 2D | `GU_TRANSFORM_2D` = "passed directly to the rasterizer" | **confirmed** |
| scissor takes width/height | `sceGuScissor(x, y, w, h)` | **confirmed** |
| `sceGuStart` arity differs from Sony's | 2 arguments here | **confirmed** |
| VFPU intrinsics absent from pspdev GCC | `__has_builtin` says no | **confirmed** |
| `libgum_vfpu` is not the default GUM | separate archive, no sample links it | **confirmed** |
| 4 MB EDRAM is opt-in, not automatic | `sceGeEdramSetSize(0x400000)` errors on PSP FAT | **confirmed, and unavailable to us** |
| 8 KiB texture cache, 512x512 max texture | not in the headers — hardware facts | **from the blueprint only; treat as untested by us** |
| through-mode UVs are texels and `TexScale`/`TexOffset` are ignored | header says the opposite direction: it suggests `sceGuTexOffset`/`sceGuTexScale` *for* scaling texcoords in this path | **conflict — verify with one quad on device before designing around it** |
