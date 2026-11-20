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
| Remotes | `origin` = `nikmes/raylib` (fork), `upstream` = `raysan5/raylib`, `consoles` = `mcidclan/raylib4Consoles` |
| PSP platform code | **not imported yet** — nothing of the port is in this tree |
| Build on PSP | not attempted |
| Hardware | PSP-1000, 32 MB, firmware 6.60 with ARK CFW |
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
| Framebuffers (5551, 512x272 stride x2) + depth | 816 KiB, of which 272 KiB is depth | `[derived]` |
| Textures left after the above | ~1.26 MiB | `[derived]` |
| Vertex colour order | `0xAABBGGRR` | `[measured]` |
| 16-bit texture coordinates | unsigned; zero-centred UVs wrap the long way round | `[measured]` |
| s16 vertex positions | normalised (value/32768) in 3D transform mode | `[public: PPSSPP GE docs]` |
| GE matrices | float24, ~16 significant bits: render camera-relative | `[public + measured]` |
| X/Y clipping | none; a primitive crossing the 4096 window is dropped whole | `[public: PPSSPP GE docs]` |
| Depth buffer on a 2D-on-rails renderer | allocatable but useless; the reference engine allocates and never enables it | `[measured: PSPRuntime]` |
| Hardware audio channels | 8 (indices 0–7); per-channel buffer 64–65,472 samples, a multiple of 64; volume 0–`0x8000` | `[public: pspdev/psp/sdk/include/pspaudio.h]` |
| Audio sample-rate conversion | `sceAudioSRCChReserve(samplecount, freq, channels)` + `sceAudioSRCOutputBlocking()` — hardware SRC for arbitrary rates instead of resampling on the CPU | `[public: same header]` |
| Input hardware | one nub (2 axes, raw 0–255), D-pad, 4 face buttons, 2 shoulder buttons, SELECT/START/HOME/HOLD/NOTE. **No analog triggers, no rumble** | `[public: pspctrl.h]` |

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

A proven upper bound for both, from a working GU renderer (`PSPRuntime`, see §7): 2 vertices per sprite
with `GU_SPRITES` gives **1024 sprites per draw call**, quads (4 v + 6 i) give 512, and its bunnymark
therefore issues `ceil(N/1024)` calls per frame. `[measured: code scan]` That is the structure our
batcher has to reach, and the number to beat when the native backend lands.

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
Exit: same-texture run of N sprites produces ~`ceil(N/capacity)` submissions; screenshots match the
unbatched path; no per-sprite allocation measurable.

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
The 5.5 port already polls `sceCtrlReadBufferPositive()` once per frame in `PollInputEvents()` and maps
buttons to `GAMEPAD_BUTTON_*` with a 0.2 deadzone — start from that, then close its gaps.
- Verify the mapping button by button: CROSS/CIRCLE/SQUARE/TRIANGLE, D-pad, L/R shoulders,
  SELECT/START. Decide and document what HOME, HOLD and NOTE do (system-owned vs delivered).
- `GetGamepadButtonPressed()` never fires in the 5.5 port: it resets `lastButtonPressed` without ever
  setting it. Fix the edge detection.
- Calibrate the nub instead of assuming 128 is centre: sample raw min/max/centre per unit, provide a
  calibration path, and keep the deadzone configurable. PSP nubs are not symmetric.
- Decide whether to also feed `CORE.Input.Keyboard` from the pad. It makes upstream examples run
  unmodified (`IsKeyDown(KEY_RIGHT)`) and costs nothing at runtime — but it changes what `IsKeyDown`
  means on this platform, so record the choice.
- Sampling: `sceCtrlSetSamplingCycle()` / mode, and whether to read once per frame or on a fixed rate.
- `SetGamepadVibration()` must stay a documented no-op — the PSP has no rumble.
Exit: a sample that draws every button's live state plus raw and normalised axis values; each control
pressed and confirmed; measured nub range recorded per console; `GetGamepadButtonPressed` returns the
pressed button exactly once.

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

### Phase 11 — Release gate
Four configurations on the same console, same workload: original pspGL + unbatched raylib; upgraded
pspGL + unbatched; upgraded pspGL + batched; native GU. Report frame-time percentiles, submission
counts, GE waits, VRAM high-water, screenshots. Decide from the results whether anything still needs
the direct-GU path. Input and audio are checked in the same run: neither may appear in the frame
profile.

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

**Take, with attribution rules in mind:** 1024-sprites-per-call batching structure; the pixel-exact 2D
viewport idiom; direct-swizzle upload; the 512-byte-block best-fit allocator; its bunnymark as a
direct-GU baseline once fixed.

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
| 2026-11-20 | Base on release tag 6.2, not `master` (6.5.0-dev) | stable baseline; full history keeps `git merge` available |
| 2026-11-20 | Start from upstream raylib, not from the port repos | `raylib4Psp` holds no library code; fork `master` is pure upstream 6.0; the PSP delta is one platform file |
| 2026-11-20 | Stage A on pspGL before any native backend | gives a working 6.2 to compare against, and the GL11 call list becomes the GU spec |
| 2026-11-20 | Do not run `pspbuild.sh` | would rebuild older 5.5 sources over a matching pspdev library |
| 2026-11-20 | Optimise raylib submission before optimising pspGL internals | per-sprite submits cost 512–1024x the necessary draw calls; fix the structural problem first |

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
| `/home/psp/ref/pspgl-themiriron2` | BSD-3 | profiling counters, command-list capacity (`pspgl_dlist.c`, `glVertex.c`, `glEnd.c`) |
| `/home/psp/ref/pspgl-pspdev` | BSD-3 | provenance of the shipped `libGL.a` (`de4260ad`) |
| `/home/psp/ref/pspgl-legacy` | BSD-3 | historical only |
| `/home/psp/ref/PGE` | GPL-3.0 | frame-list and VRAM ideas (own code, ideas only unless dual-licensed) |
| `/home/psp/ref/openTRI` | GPL-2.0 | allocator, residency, particle rings (same constraint) |
| `/home/psp/ref/3D-PSP-Tutorials` | **none** | clean-room benchmark designs; never copy |

### 11.4 SDK, toolchain, skill

| path | open it for |
|---|---|
| `/home/psp/pspdev/psp/sdk/include` | the API truth: `pspgu.h`, `pspgum.h`, `pspaudio.h`, `pspctrl.h`, `pspthreadman.h`, `pspvfpu.h`, `pspiofilemgr.h` |
| `/home/psp/pspdev/psp/sdk/samples` | working examples, including `me/basic` (reset-vector takeover) |
| `/home/psp/pspdev/psp/lib`, `.../sdk/lib` | shipped static libs: `libGL`, `libpspaudio`, `libpspaudiolib`, `libpspgu`, `libpspgum`, `libraylib` |
| `/home/psp/psp_sdk_660` | Sony's official devkit SDK, kept as a reading aid — see below |
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

---

## 13. Working method: sprints, features, recipes

Three document types, three different jobs. Templates live in `docs/psp/templates/`; the filled copies go
in sibling directories and render in this site.

| type | answers | lives in | template |
|---|---|---|---|
| **Sprint** | what we do in one time box, and which number is supposed to move | `docs/psp/sprints/NNN-slug.md` | [sprint](templates/sprint.md) |
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
