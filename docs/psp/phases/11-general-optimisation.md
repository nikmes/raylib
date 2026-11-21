# Phase 11 — General optimisation outside the renderer

| | |
|---|---|
| State | not started |
| Depends on | Phases 1–10; a build already at `-O2`, and one asset-heavy sample as well as bunnymark |
| Exit criterion | every remaining CPU, memory, I/O and startup cost in a real frame and a real load is measured, and either removed or explicitly budgeted |

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

- [ ] The phase 2/3 counters can separate frame work from GE wait, and a load phase can be timed separately
- [ ] At least two workloads exist: a CPU-bound steady frame (bunnymark) and an asset-heavy load (a sample that
      decodes images and opens files)
- [ ] The build records its flags, raylib config flags and build number at boot
- [ ] The `-O2` arm is archived by hash, so a later flag change is attributable
- [ ] A full-feature build exists for correctness checks; the stripped build is for measurement only

## Tasks

### Build and compile-time surface

- [ ] Make `-O2` the default on every PSP build path (Makefile and CMake), not just the one target sprint 008
      fixed. Record the effective flags in the boot line
- [ ] A/B `-O2` vs `-O3` vs `-Os` on the CPU-bound and load-bound workloads. Record frame time **and** `.text`
      size; `-O3` is not assumed to win on a 16 KiB I-cache
- [ ] A/B link-time optimisation and `-ffunction-sections -fdata-sections -Wl,--gc-sections`; record code-size and
      frame deltas
- [ ] A/B `-ffast-math -fno-math-errno`. Before trusting it, run a numerical smoke test for NaN/Inf, comparisons,
      camera math and collision; document what changed
- [ ] Strip unused raylib modules and formats in `src/config.h`. Candidates, all enabled by default:
      `SUPPORT_MODULE_RMODELS`, `SUPPORT_MODULE_RAUDIO` (only if unused), `SUPPORT_FILEFORMAT_BMP`,
      `SUPPORT_FILEFORMAT_GIF`, `SUPPORT_FILEFORMAT_QOI`, `SUPPORT_FILEFORMAT_DDS`, `SUPPORT_FILEFORMAT_TTF`,
      `SUPPORT_FILEFORMAT_FNT`, `SUPPORT_FILEFORMAT_OBJ`, `SUPPORT_FILEFORMAT_MTL`, `SUPPORT_FILEFORMAT_IQM`,
      `SUPPORT_FILEFORMAT_GLTF`, `SUPPORT_FILEFORMAT_VOX`, `SUPPORT_FILEFORMAT_M3D`, `SUPPORT_FILEFORMAT_WAV`,
      `SUPPORT_FILEFORMAT_OGG`, `SUPPORT_FILEFORMAT_MP3`, `SUPPORT_FILEFORMAT_QOA`, `SUPPORT_IMAGE_EXPORT`,
      `SUPPORT_IMAGE_GENERATION`, `SUPPORT_COMPRESSION_API`, `SUPPORT_SCREEN_CAPTURE`. Keep a full build and a
      per-sample list of what it actually needs
- [ ] Audit `MAX_*` limits in `config.h` against the 32 MB device; a limit that can never be reached is memory
      reserved for nothing
- [ ] Decide `-G0` versus small-data explicitly and measure it; do not inherit the flag by accident

### CPU and libc

- [ ] Count `double` operations in a steady frame and on the load path, both statically
      (`__adddf3`/`__subdf3`/`__muldf3`/`__divdf3` call sites) and dynamically. Remove doubles from per-frame
      timing, camera, collision, interpolation and random code where `float` or fixed point is sufficient
- [ ] Find integer division and modulo in hot loops; replace with shifts or reciprocals only where the result is
      provably identical
- [ ] A/B `libpspmath` or VFPU helpers against libm for `sin`, `cos`, `sqrt`, `atan2`. Record accuracy as well as
      speed, and keep frame-accumulated state out of VFPU (the truncation bias documented in §10.1)
- [ ] Audit tiny `memcpy`/`memset`/`strlen`/`strcmp` calls in per-frame code; a call to a PLT stub for 8 bytes
      can cost more than the copy
- [ ] Remove `TextFormat`/`sprintf` from per-frame paths where the result can be cached or built once
- [ ] Measure the cost of one `TraceLog` line over PSPLink. Compile INFO/DEBUG out of the timing build; keep
      exactly one boot line with build number, flags and counters

### Memory

- [ ] Map the heap at boot: `PSP_HEAP_SIZE_KB`, `PSP_LARGE_MEMORY=0`, pre-`main` allocation, user partition,
      free bytes and largest block. Reconcile the map against the numbers in [§3](../memory-map.md#3-hardware-facts-that-bound-the-design)
- [ ] Replace per-frame transient `malloc`/`free` with fixed pools. Verify with allocation counters, not by
      reading the code
- [ ] Track heap high-water and fragmentation over a 30-minute run; record free bytes and largest free block.
      A stable frame with a shrinking largest block is still a leak
- [ ] Keep 16-byte alignment for VFPU, GE and DMA buffers; over-align long-lived buffers instead of calling
      `memalign` per object
- [ ] Reserve thread stacks early from the user partition, size them from a measurement, and add
      `PSP_THREAD_ATTR_VFPU` wherever VFPU code runs. A late thread fails with `0x80020190` while the log shows
      megabytes free
- [ ] Write back exact D-cache ranges; no `sceKernelDcacheWritebackInvalidateAll()` in a steady frame. Measure
      the cost of the range writeback itself
- [ ] Read the linker map for `.text` hotspots and cold code. With a 16 KiB I-cache, a smaller build can be the
      faster build — this is the reason `-Os` stays in the A/B set

### File I/O and the load path

- [ ] Measure `sceIoOpen`/`sceIoRead`/`sceIoClose` on the Memory Stick at several sizes; confirm the ~17 ms open
      cost and find the read size where throughput flattens
- [ ] Keep file handles alive; never open the same asset per frame. One long-lived handle and large sequential
      reads beats many small ones
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
- [ ] Add regression guards to the timing build: zero `double` operations, zero allocations, zero file opens and
      zero log lines in a steady frame

## What we already know

- **The compiler is the largest general lever measured so far.** Until sprint 008 the PSP build came out at
  `-O0` because the Makefile's `RELEASE` optimisation block covered desktop, web and android only. At `-O2`,
  bunnymark at N=1,000 went **33.367 → 16.683 ms** with the batcher, and 66.733 → 50.051 ms unbatched.
  `-O2` is now the PSP default; `-O3`/`-Os`/LTO are still unmeasured. `[measured: sprint 008]`
- **There is no double FPU.** Every `double` operation becomes a libgcc call; one PC port counted 5,633
  `__adddf3`-class call sites. That is a reason to count ours, not a claim that raylib has the same number.
  `[external: memory-map §10.1]`
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

- [ ] a full-frame and full-load profile with every CPU, I/O, allocation and GE-wait cost accounted for
- [ ] no `double` operation, heap allocation, file open or log write in a steady-state frame, verified by
      counters rather than by inspection
- [ ] compiler flags (`-O2`+, LTO, section GC, fast-math) and raylib config flags chosen by A/B, two runs per
      arm, 0.1 % threshold, with `.text` size recorded
- [ ] the load path runs off the render thread, or a measurement explains why it does not need to
- [ ] heap high-water and largest free block stable over a 30-minute soak
- [ ] boot line reports build number, flags and config; every improvement has a control/change/control record
- [ ] the five samples still render identically with the full-feature build

## Sprints and features

None yet. Use the [sprint template](../templates/sprint.md). A sprint here belongs to this phase only; if it
changes a draw path, VRAM allocation, input mapping or audio mixing, it belongs in the phase that owns that
surface.
