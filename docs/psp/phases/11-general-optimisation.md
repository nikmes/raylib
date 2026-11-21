# Phase 11 — General optimisation outside the renderer

| | |
|---|---|
| State | not started — reviewed 2026-11-21; blocked on phases 7–10 |
| Depends on | Phases 1–10; a build already at `-O2`, and one asset-heavy sample as well as bunnymark |
| Exit criterion | every remaining CPU, memory, I/O and startup cost in a real frame and a real load is measured, and either removed or explicitly budgeted |

## 0. Review findings (2026-11-21) — parked correctly, but not implementation-ready

The phase is correctly held: phase 7 is in progress and phases 8–10 are not started. The task inventory is
reasonable, but several premises do not survive the current tree, and three measurement designs cannot meet the
exit criteria as written. None of this changes the phase's scope; it changes what the first sprint must do.

**Finding 1 — there is no declared heap to map.** `PSP_HEAP_SIZE_KB` does not appear anywhere in `src/`,
`psp-tests/` or the PSP Makefiles. The shipped build links libcglue's weak `sce_newlib_heap_kb_size`, and
`_sbrk` chooses the size. The "declared 1 MB heap" in [§3](../memory-map.md#3-hardware-facts-that-bound-the-design)
is a past probe, not the current baseline. Re-measure the default at boot before proposing `-256`, and record
which declaration each A/B arm used.

**Finding 2 — frame time cannot resolve compiler flags.** The bunnymark baselines are vblank multiples: 16.683,
33.367, 50.051 and 100.107 ms are 1–6 frames at 59.94 Hz; `scene_2d` shares the same swap path. `TARGET_FPS=0`
does not remove the wait, because pspgu's swap still waits. A 0.1 % A/B on a 16.683 ms quantum is invisible.
Measurement arms need
`PSP_DISABLE_VSYNC=1` (sprint 013: swap 9.4 → 2.433 ms) **and** a CPU-zone timer around update/draw, not
`GetFrameTime()`. "Two runs per arm" also conflicts with §6's 10–30 % run-to-run variance; use interleaved
control/change/control arms and report median/p95 with a confidence interval.

**Finding 3 — "zero doubles" is a cross-phase criterion.** The archive has 949 soft-float double relocation
sites: `rtextures` 443, `rmodels` 349, `rcore` 92, `rtext` 65; `rshapes` and `rlgl` have none. The candidate
sites in a 2D frame are mostly `rcore` timing — `EndDrawing` 7, `WaitTime` 9, `GetFPS` 6, `BeginDrawing` 2,
`GetTime`/`GetFrameTime` 1 each — plus gestures (`ProcessGestureEvent` 26 static sites, phase 8). `BeginMode3D`,
`rlOrtho` and `rlFrustum` (12 + 6 + 6) are renderer work, and pspgu adds 42 in `gl_matrix.c` (`glOrtho` 12,
`glFrustum` 14, `upload` 16). Phase 11 can remove the timing sites; it cannot remove the input, matrix or driver
sites without breaking its own "not the renderer" rule. Reword the exit criterion to "zero double sites in
Phase 11-owned code", with a named handoff to phases 8 and 10/5.

**Finding 4 — the build tasks name paths that do not exist.** There is no PSP CMake or Zig path:
`CMakeLists.txt` and `build.zig` contain no `PLATFORM_PSP`, so "Makefile and CMake" is Makefile-only. `-G0` is
not inherited by accident — `src/Makefile:551` sets it explicitly, and psp-gcc's default is the opposite
(gp-relative `%gp_rel`). The real A/B is the current `-G0` against `-G8`/the toolchain default, and the safety
claim in the Makefile comment ("crt0 sets gp for its own layout") is the hypothesis to test. Feature stripping
belongs in `RAYLIB_CONFIG_FLAGS` with `EXTERNAL_CONFIG_FLAGS` (`src/Makefile:395`), not edits to `src/config.h`;
config.h's `MAX_*` block is commented documentation, and the effective limits live in `rcore.c`, `rgestures.h`
and the module headers.

**Finding 5 — the logging task has no mechanism.** `TRACELOG` is all-or-nothing at compile time
(`SUPPORT_TRACELOG`, `src/config.h:378`); there is no INFO/DEBUG compile-time level. "Compile INFO/DEBUG out,
keep one boot line" therefore needs a PSP-specific level macro or a boot print outside TRACELOG. Today the boot
line logs raylib `6.5-dev`, the module list and the GL strings, but not the build number, compiler flags or
config flags (`src/rcore.c:601`, `src/platforms/rcore_psp.c:760`), so the "build records its flags" precondition
is unmet.

**Finding 6 — the load path uses stdio and calloc.** `LoadFileData` opens with `fopen`, seeks twice to size the
file, reads once and allocates with `RL_CALLOC` (`src/rcore.c:1961-2017`) — the whole asset is zero-filled
before it is overwritten. The task must decide stdio versus `sceIo*` (the file callbacks exist but are unused)
and test `malloc` against `calloc`; "one allocation, large reads" is already true, "no copy that can be avoided"
is not. There is no loader thread in the tree — only the platform's `update_thread` (`rcore_psp.c:556`) — so
the loader-thread task is new work, not a check.

**Finding 7 — ownership overlaps must be stated.** `SUPPORT_MODULE_RAUDIO` is phase 9's decision (`src/Makefile`
already forces it to 0); `SUPPORT_FILEFORMAT_*` must follow phase 13's per-sample manifest; volatile partition 5
exists only on Slim and cannot be measured on the PSP-1000 under §6, so mark it explicitly deferred or optional.
The "five samples" correctness gate predates phase 13's 127-sample corpus; use the phase 13 tier-A set, or say
why five is still the oracle.

**Finding 8 — the counters need names.** Nothing in the tree counts doubles, allocations, opens or log lines
dynamically. Specify the mechanism up front, as the GL work did: `--wrap` for
`__adddf3/__subdf3/__muldf3/__divdf3/...` and for `malloc/free/calloc/realloc/fopen/sceIoOpen/printf`, plus
`psp-objdump -dr` for static call sites. Add the missing bulk-copy A/B: the cross-check says the 64-byte VFPU
copy transfers, but no task bullet tests it at loader sizes.

**Verified against the sources.**

- `-O2` is the PSP default (`src/Makefile:597`). The `-O2` archives exist under `/home/psp/evidence/sprint008`
  but have no `SHA256SUMS`; add one or weaken the "archived by hash" precondition.
- The Daedalus values check out, but the citation paths need the `Source/` prefix:
  `Source/SysPSP/main.cpp:80/103/107/108`, `Source/SysPSP/Math/Math.h`, `Source/SysPSP/Utility/FastMemcpyPSP.cpp`,
  `Source/SysPSP/Utility/VolatileMemPSP.cpp:56` (the malloc fallback), `Source/RomFile/...`,
  `Source/Utility/MemoryHeap.cpp`, `Source/CMakeLists.txt`.
- `pspFpuSetEnable(0)` is a bitmask of exceptions to enable, so "FP exceptions off" is the right reading.
- pspgu uses no VFPU and sets neither clock nor FP exceptions, so those tasks are real.
- `libpspmath` and `libpspvfpu` are installed in the SDK (`psp/sdk/lib`).

**Recommended first sprint, once phases 7–10 close:** not a flag sweep. Build the measurement harness — boot
line, CPU zones, wrap counters, load timer and a `PSP_DISABLE_VSYNC` arm — then re-baseline the heap and frame.
Without that harness every number the rest of the phase produces is uninterpretable.

## What this phase is about

Phases 4–7 attack submission, VRAM and the 2D paths; phases 8–10 attack input, audio and 3D. This phase attacks
what is left: the compiler, libc, allocator, file I/O, logging, startup and raylib's compile-time feature set.
The target is not "make raylib faster" in the abstract. It is to remove costs that never appear in a draw-call
count — a soft-float `double`, a per-frame `malloc`, a Memory Stick open, a log line over USB, or a texture
decoder that runs on the render thread.

Two rules decide what belongs here:

- **Not the renderer.** `rlgl.h`, the batcher, draw paths, texture upload/VRAM allocation, input mapping and the
  audio backend stay in their own phases. If a change would move a vertex, bind a texture or mix a sample, it is
  not this phase.
- **Measured, or it is not an optimisation.** A flag, a pool or a format change is adopted only by the protocol
  in [§6 of the memory map](../memory-map.md#6-measurement-protocol): device only, one variable per run,
  control → change → control, 0.1 % threshold.

## What has to be true before starting

- [ ] Phase 7 closed and phases 8–10 closed or explicitly deferred; this phase profiles the frame those phases
      produce
- [ ] The phase 2/3 counters can separate frame work from GE wait, and a load phase can be timed separately
- [ ] A measurement arm can be built with `PSP_DISABLE_VSYNC=1` and CPU-zone timing, because frame time alone is
      vblank-quantised to 16.683 ms steps
- [ ] At least two workloads exist: a CPU-bound steady frame (bunnymark) and an asset-heavy load (a sample that
      decodes images and opens files)
- [ ] The build records its flags, raylib config flags and build number at boot
- [ ] The `-O2` arm is archived by hash, so a later flag change is attributable
- [ ] A full-feature build exists for correctness checks; the stripped build is for measurement only

## Tasks

### Build and compile-time surface

- [ ] Make `-O2` the default on the only PSP build path, `src/Makefile`; there is no PSP CMake or Zig path to
      fix. Record the effective flags in the boot line
- [ ] A/B `-O2` vs `-O3` vs `-Os` on the CPU-bound and load-bound workloads. Record a CPU-zone time (with
      `PSP_DISABLE_VSYNC=1`) **and** the `.text` size of both the archive and the linked PRX; `-O3` is not
      assumed to win on a 16 KiB I-cache
- [ ] A/B link-time optimisation and `-ffunction-sections -fdata-sections -Wl,--gc-sections`; record code-size and
      frame deltas
- [ ] A/B `-ffast-math -fno-math-errno`. Before trusting it, run a numerical smoke test for NaN/Inf, comparisons,
      camera math and collision; document what changed
- [ ] Strip unused raylib modules and formats through `RAYLIB_CONFIG_FLAGS` with `EXTERNAL_CONFIG_FLAGS`
      (`src/Makefile:395`), not by editing `src/config.h`, so desktop and web builds stay upstream. Candidates,
      all enabled by default:
      `SUPPORT_MODULE_RMODELS`, `SUPPORT_MODULE_RAUDIO` (only if unused), `SUPPORT_FILEFORMAT_BMP`,
      `SUPPORT_FILEFORMAT_GIF`, `SUPPORT_FILEFORMAT_QOI`, `SUPPORT_FILEFORMAT_DDS`, `SUPPORT_FILEFORMAT_TTF`,
      `SUPPORT_FILEFORMAT_FNT`, `SUPPORT_FILEFORMAT_OBJ`, `SUPPORT_FILEFORMAT_MTL`, `SUPPORT_FILEFORMAT_IQM`,
      `SUPPORT_FILEFORMAT_GLTF`, `SUPPORT_FILEFORMAT_VOX`, `SUPPORT_FILEFORMAT_M3D`, `SUPPORT_FILEFORMAT_WAV`,
      `SUPPORT_FILEFORMAT_OGG`, `SUPPORT_FILEFORMAT_MP3`, `SUPPORT_FILEFORMAT_QOA`, `SUPPORT_IMAGE_EXPORT`,
      `SUPPORT_IMAGE_GENERATION`, `SUPPORT_COMPRESSION_API`, `SUPPORT_SCREEN_CAPTURE`. Keep a full build and a
      per-sample list of what it actually needs
- [ ] Audit the effective `MAX_*` limits against the 32 MB device; a limit that can never be reached is memory
      reserved for nothing. The block in `config.h` is commented documentation — the live values are in
      `rcore.c`, `rgestures.h` and the module headers
- [ ] A/B the explicit `-G0` (`src/Makefile:551`) against `-G8`/the psp-gcc default, which is gp-relative.
      The Makefile's safety claim ("crt0 sets gp for its own layout") is a hypothesis to test, not a reason to
      skip the measurement

### CPU and libc

- [ ] Count `double` operations in a steady frame and on the load path, both statically
      (`__adddf3`/`__subdf3`/`__muldf3`/`__divdf3` call sites) and dynamically. Remove doubles from per-frame
      timing, camera, collision, interpolation and random code where `float` or fixed point is sufficient
- [ ] Find integer division and modulo in hot loops; replace with shifts or reciprocals only where the result is
      provably identical
- [ ] A/B `libpspmath` or VFPU helpers against libm for `sin`, `cos`, `sqrt`, `atan2`. Record accuracy as well as
      speed, and keep frame-accumulated state out of VFPU (the truncation bias documented in §10.1). The
      Daedalus cross-reference below has source-authored cycle counts, including the rule that `sqrt` and
      `fabs` are faster on the FPU
- [ ] A/B `pspFpuSetEnable(0)` (FP exceptions off) against the default; verify raylib does not rely on FP traps
- [ ] Audit tiny `memcpy`/`memset`/`strlen`/`strcmp` calls in per-frame code; a call to a PLT stub for 8 bytes
      can cost more than the copy
- [ ] A/B the 64-byte `lv.q`/`sv.q` VFPU copy (with the `ulv.q` unaligned-source path and 16-byte tail) against
      libc at the sizes the loader actually uses; this is the bulk-copy lever the cross-check identifies, and it
      needs a task of its own
- [ ] Remove `TextFormat`/`sprintf` from per-frame paths where the result can be cached or built once
- [ ] Measure the cost of one `TraceLog` line over PSPLink. `TRACELOG` is all-or-nothing at compile time
      (`SUPPORT_TRACELOG`), so "compile INFO/DEBUG out" needs a PSP level macro or a boot print outside
      `TRACELOG`; keep exactly one boot line with build number, flags and counters

### Memory

- [ ] Map the heap at boot: `PSP_HEAP_SIZE_KB`, `PSP_LARGE_MEMORY=0`, pre-`main` allocation, user partition,
      free bytes and largest block. No `PSP_HEAP_SIZE_KB` is declared anywhere in the tree, so the current build
      uses libcglue's weak default; the "1 MB heap" figure in [§3](../memory-map.md#3-hardware-facts-that-bound-the-design)
      is a past probe. Re-measure the shipped default, then A/B Daedalus's `PSP_HEAP_SIZE_KB(-256)` — all
      remaining user memory minus a 256 KB reserve — with the reserve sized for thread stacks. Reconcile the map
      against §3's numbers and record the declaration used by each arm
- [ ] Replace per-frame transient `malloc`/`free` with fixed pools. Verify with allocation counters, not by
      reading the code
- [ ] Track heap high-water and fragmentation over a 30-minute run; record free bytes and largest free block.
      A stable frame with a shrinking largest block is still a leak
- [ ] Keep 16-byte alignment for VFPU, GE and DMA buffers; over-align long-lived buffers instead of calling
      `memalign` per object
- [ ] Reserve thread stacks early from the user partition, size them from a measurement, and add
      `PSP_THREAD_ATTR_VFPU` wherever VFPU code runs. A late thread fails with `0x80020190` while the log shows
      megabytes free
- [ ] Decide whether a large read-only asset cache belongs in volatile partition 5 on Slim+, as Daedalus does
      for GU lists and texture overflow: `sceKernelVolatileMemLock`, `scePowerLock(0)` while held, and a
      model check. Its fallback to `malloc()` on a Phat is explicitly dangerous; ours must fail visibly
- [ ] Write back exact D-cache ranges; no `sceKernelDcacheWritebackInvalidateAll()` in a steady frame. Measure
      the cost of the range writeback itself
- [ ] Read the linker map for `.text` hotspots and cold code. With a 16 KiB I-cache, a smaller build can be the
      faster build — this is the reason `-Os` stays in the A/B set

### File I/O and the load path

- [ ] Measure `sceIoOpen`/`sceIoRead`/`sceIoClose` on the Memory Stick at several sizes; confirm the ~17 ms open
      cost and find the read size where throughput flattens
- [ ] Keep file handles alive; never open the same asset per frame. One long-lived handle and large sequential
      reads beats many small ones
- [ ] A/B a chunk cache for streamed assets. Daedalus uses 8 KB chunks with MRU replacement (8 MB on Slim,
      2 MB on Phat); test 2/8/32/64 KB and measure the hit rate as well as the open/read cost removed
- [ ] Move decode and load off the render thread: a loader thread with a double buffer, and no file I/O inside a
      frame. Reserve its stack and VFPU attribute before the first load
- [ ] A/B blocking I/O on the loader thread against `sceIoOpenAsync`/`sceIoReadAsync`/`sceIoWaitAsync`; pick the
      one with the lower main-thread stall, not the lower total time
- [ ] Check `LoadFileData`: one allocation, large reads, no per-chunk seek, no copy that can be avoided
- [ ] Prefer pre-converted assets — raw texels, swizzled blocks, CLUT atlases, pre-baked fonts — over decoding
      PNG/TTF on the device. Measure decode time against the I/O it saves
- [ ] Replace runtime directory scans with an offline manifest; the Memory Stick is not a database
- [ ] If compression is used, measure inflate CPU against the I/O saved; decompression on the render thread is a
      frame-time bug even when it is a size win

### Startup, shutdown and power

- [ ] Measure module load → first presented frame. Split static initialisation, heap setup, asset load and audio
      init; report each
- [ ] Defer optional subsystems until first use; a 300 ms font or audio init on the boot path is visible
- [ ] A/B the default clock against 333 MHz on the same workload, and record battery/heat cost. Clock changes are
      a reported variable, never a design premise
- [ ] Check teardown for double free and double `Terminate`; it is not in the frame budget, but it decides whether
      a `reset`-driven loop stays reliable
- [ ] Verify suspend/resume leaves pools, handles and pointers valid; no per-frame work is lost

### Observability and attribution

- [ ] Add CPU zones for update, load, list build and GE wait; report median and 95th percentile, never the mean
- [ ] Log build number, compiler flags and raylib config flags at boot; a number without them is not evidence
- [ ] Archive the library and its hash per arm; the same source must rebuild to the same measurement
- [ ] Add regression guards to the timing build: zero `double` sites in Phase 11-owned code, zero allocations,
      zero file opens and zero log lines in a steady frame

## DaedalusX64 cross-check (code-read, not copied)

`/home/psp/ref/daedalus` is GPL-2.0-or-later and read-only here. Its `AI_MEMORY_MAP.md` §12 is a PSP
CPU/GPU/RAM/VRAM/VFPU playbook; the rows below were checked against the source rather than accepted from the
document. GPU and VRAM levers in §12 (16-bit framebuffer, double-buffered GU lists, VRAM-first textures) belong
to Phases 5–6 and are not repeated. Only the non-renderer levers are Phase 11 material.

| lever | source | code-checked value | tag |
|---|---|---|---|
| Heap size | `Source/SysPSP/main.cpp:103` | `PSP_HEAP_SIZE_KB(-256)`: all remaining user memory minus 256 KB (semantics from §12.1) | `[source + playbook]` |
| Clock | `Source/SysPSP/main.cpp:107` | `scePowerSetClockFrequency(333, 333, 166)` | `[source]` |
| FP exceptions | `Source/SysPSP/main.cpp:108` | `pspFpuSetEnable(0)` | `[source]` |
| VFPU thread | `Source/SysPSP/main.cpp:80` | `PSP_MAIN_THREAD_ATTR(USER + VFPU)` on the main thread | `[source]` |
| Scalar math | `Source/SysPSP/Math/Math.h:19-38` | source comments: `sinf` 856 → 160 cycles, `cosf` 990 → 154, `acosf` 1433 → 107, `powf` 3488 → 412; `fabsf` 7 → 93 and `sqrtf` 40 → 240 are slower, so the header maps them to `pspFpuAbs`/`pspFpuSqrt` | `[external: Daedalus source comment]` |
| Bulk copy | `Source/SysPSP/Utility/FastMemcpyPSP.cpp:25-140` | 64-byte `lv.q`/`sv.q` loop; `ulv.q` when only the source is 4-byte aligned; 16-byte tail; falls back to `memcpy` below 16 B or on mismatched 4-byte phase | `[source]` |
| Allocator | `Source/Utility/MemoryHeap.cpp:181-255` | first-fit over a contiguous region; `Free()` removes the chunk and does **not** coalesce | `[source]` |
| ROM chunk cache | `Source/RomFile/RomFileCache.cpp:71-83` | 8 KB chunks, MRU; 1024 chunks = 8 MB on Slim, 256 = 2 MB on Phat | `[source]` |
| ROM heap | `Source/RomFile/RomFileMemory.cpp:72-76` | 16 MB on Slim, 2 MB on Phat, shared by the fixed buffer and the chunk cache | `[source]` |
| Volatile partition | `Source/SysPSP/Utility/VolatileMemPSP.cpp:20-48` | partition 5 via `sceKernelVolatileMemLock`, `scePowerLock(0)` while held, `sceKernelAllocPartitionMemory`; `malloc` fallback at line 56 | `[source]` |
| Release flags | `Source/CMakeLists.txt:28-48` | the CMake logic sets IPO/LTO true for a non-UNIX Release build and leaves CMake's default `-O3 -DNDEBUG`; only CTR overrides the flags explicitly | `[source]` |

**What transfers:** heap sizing, clock and FP-exception setup, VFPU scalar math and bulk copy, a chunked ROM/asset
cache, and partition-5 volatile memory for a read-only working set. These are the levers already in the task list,
now with a second codebase as evidence.

**What does not transfer:** `SIM_DOUBLES` typedefs `d64 = f32` for emulated N64 doubles — an accuracy trade
specific to an emulator; raylib should remove real doubles, not simulate them. The dynarec code cache (two 3 MB
buffers, 64-byte blocks, I-cache invalidation) has no raylib analogue, and GU list double-buffering, texture-hash
frequency and per-ROM knobs are renderer or emulator policy.

**One caution:** Daedalus's `CMemoryHeap` is the pspGL first-fit allocator with no coalescing, and it is used for
the ROM heap, VRAM and the volatile texture heap. That is a counter-example for our Phase 6 allocator, not a
model: alloc/free churn fragments it, and its size comments ("32Mb") already disagree with the arithmetic.

## What we already know

- **The compiler is the largest general lever measured so far.** Until sprint 008 the PSP build came out at
  `-O0` because the Makefile's `RELEASE` optimisation block covered desktop, web and android only. At `-O2`,
  bunnymark at N=1,000 went **33.367 → 16.683 ms** with the batcher, and 66.733 → 50.051 ms unbatched.
  `-O2` is now the PSP default; `-O3`/`-Os`/LTO are still unmeasured. `[measured: sprint 008]`
- **There is no double FPU.** Every `double` operation becomes a libgcc call; one PC port counted 5,633
  `__adddf3`-class call sites. Ours: **949** relocation sites across `libraylib.a` (`rtextures` 443,
  `rmodels` 349, `rcore` 92, `rtext` 65; `rshapes` and `rlgl` 0), before any hot/cold split. `[measured
  2026-11-21; external: memory-map §10.1]`
- **Code size is a performance variable.** The PSP-1000 has 16 KiB I-cache and 16 KiB D-cache with 64-byte lines;
  feature stripping helps only if it removes code that is actually fetched. `[external: blueprint §16.1]`
- **Memory Stick opens cost ~17 ms each.** Few long-lived handles and large reads; audio already depends on this
  rule. `[measured]`
- **Thread stacks come from the user partition, not the heap.** A thread created late fails with `0x80020190`
  while the log shows free heap. `[measured]`
- **`PSP_LARGE_MEMORY=0` is mandatory.** The PSPSDK default asks for a 40 MB partition on a 32 MB console.
  `[measured]`
- **A whole-cache invalidate is not free.** It looks like one syscall but discards the loader's and the CPU's
  working set; range writeback is the default, whole-cache only at a phase boundary. `[practice]`

## Traps

- **`-ffast-math` changes behaviour, not just speed.** NaN/Inf handling, `x != x`, and reassociation can break
  camera math, collision and timing. Run correctness before quoting a frame time.
- **`-O3` can lose to `-O2` here.** A 16 KiB I-cache punishes code growth; the A/B must record `.text` size.
- **Stripping a format can silently break a sample.** Keep the full build in CI/correctness runs and a
  per-sample list of required `SUPPORT_*` flags.
- **Disabling `SUPPORT_TRACELOG` removes the evidence.** Time the quiet build, diagnose with the logging build;
  do not compare them as if they were the same arm.
- **Caching a file descriptor is not caching the read.** A `seek` per chunk still pays the seek; read
  sequentially or map the whole asset.
- **Optimising code that is not in the profile.** The frame and load profiles decide the order; intuition does
  not.
- **PPSSPP is not an arm.** It answers "does it still work", never "how fast".

## Exit criteria

- [ ] a full-frame and full-load profile that accounts for every measured CPU, I/O, allocation and GE-wait zone
      in a named budget table, with the unaccounted residual stated rather than assumed zero
- [ ] no `double` site, heap allocation, file open or log write in Phase 11-owned code on a steady-state frame,
      verified by counters; the phase 8 input and phase 10/pspgu matrix sites are handed off explicitly, not
      silently included or excluded
- [ ] compiler flags (`-O2`+, LTO, section GC, fast-math, `-G0` vs `-G8`) and raylib config flags chosen by
      interleaved A/B arms, median/p95 with a confidence interval, 0.1 % minimum delta, with archive and PRX
      `.text` sizes recorded
- [ ] VFPU use is per operation and measured: `sin`/`cos`/`pow` where VFPU wins, `sqrt`/`fabs` on the FPU, and
      bulk copies against libc at the sizes the loader actually uses
- [ ] the load path runs off the render thread, or a measurement explains why it does not need to
- [ ] heap high-water and largest free block stable over a 30-minute soak
- [ ] boot line reports build number, flags and config; every improvement has a control/change/control record
- [ ] the phase 13 tier-A corpus (or the five phase-1 samples until phase 13 lands) still renders identically
      with the full-feature build

## Sprints and features

None yet. Use the [sprint template](../templates/sprint.md). A sprint here belongs to this phase only; if it
changes a draw path, VRAM allocation, input mapping or audio mixing, it belongs in the phase that owns that
surface.
