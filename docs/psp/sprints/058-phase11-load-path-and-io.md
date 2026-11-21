# Sprint 058 (P11-S7) — Load path and I/O

| | |
|---|---|
| Roadmap phase | [Phase 11 — General optimisation outside the renderer](../phases/11-general-optimisation.md) |
| Sprint | P11-S7 |
| Branch / base commit | `master` @ `e307fe05` (docs; no Phase 11 code yet) |
| Started / closed | 2026-11-21 (written) / — |
| Build number(s) | pspgu `b24`; `libraylib.a` with the loader/I/O switches; numbers assigned when the sprint opens |
| Tasks | 0/8 done |

## 0. Handover

**Situation.** `LoadFileData` uses newlib stdio: `fopen`, two `fseek`s to size the file, one `fread`, and
`RL_CALLOC` for the destination, which zero-fills the whole asset before overwriting it. The file callbacks exist
but no sample sets them. The Memory Stick open is about 17 ms, and nothing in the tree has a loader thread: the
platform creates only `update_thread`. This sprint measures open/read/decode/upload, decides stdio versus
`sceIo*`, removes the avoidable zero-fill and copy, and moves decode/load off the render thread.

**Read before acting**, in this order:

1. `../memory-map.md` §6 — the measurement protocol; §9 — the “caching a descriptor is not caching the read”
   and “Memory Stick is not a database” traps.
2. `../phases/11-general-optimisation.md` §0 finding 6 — the stdio/calloc path; the file-I/O task list and the
   Daedalus chunk-cache cross-check.
3. `../sprints/052-phase11-measurement-harness.md` §5 T3/T5 — the open/read counters and the load workload.
4. `../sprints/057-phase11-heap-and-stacks.md` §5 T6 — the stack and VFPU-attribute rules for the loader thread.
5. `src/rcore.c:1961-2017` — `LoadFileData`; `:2026-2085` — `SaveFileData`; `:2100-2200` —
   `LoadFileText`/`SaveFileText` and the callback setters; `:2400-2500` — directory scans.
6. `src/rtextures.c` — `LoadImage`/`LoadImageRaw`/`LoadTexture`; `src/rtext.c` — `LoadFontEx`/`LoadFontFromMemory`;
   `src/rmodels.c` — `LoadModel` and its file reads.
7. `$PSPDEV/psp/sdk/include/pspiofilemgr.h` — `sceIoOpen`/`sceIoRead`/`sceIoClose`/`sceIoLseek`/`sceIoGetstat`;
   `/home/psp/ref/daedalus/Source/RomFile/RomFileCache.cpp:71-83` and `RomFileMemory.cpp:72-76` — the chunk cache.

**First action.** Add `psp-tests/io_probe` (or extend `psp-tests/load_path` from 052) to time `sceIoOpen`,
`sceIoRead`, `sceIoClose` and the stdio equivalents at 1 KiB, 16 KiB, 256 KiB and 2 MiB, with `reset` before
each run. Do not change `LoadFileData` until the open cost and the read-size knee are on the page.

**Hard rules.**

- **No file I/O in a steady frame.** The target is zero opens/reads in the frame window; a load is a separate
  phase. The S1 counters are the proof.
- **One allocation, no avoidable copy.** `LoadFileData` may allocate once and read into it; a zero-fill or a
  second buffer that is immediately overwritten is a finding, not a style choice.
- **Pick the lower main-thread stall, not the lower total time.** A loader thread that finishes sooner but
  blocks the main thread at the wrong moment loses. Report both.
- **The loader stack is reserved before the first load.** Use the 057 rules; a late thread fails with
  `0x80020190` while the heap looks free.
- **A chunk cache is for streamed assets.** Reading a whole file once does not benefit; a cache only pays for
  models/music/streamed data with repeated access. Measure the hit rate, not only the bytes saved.
- **Do not copy the GPL reference code.** The Daedalus chunk cache is an idea; implement the technique, not its
  source.
- **Do not change a decoder to win a timing.** The PNG/TTF/OBJ decode path stays; the sprint changes how the
  bytes arrive, not what the decoder does.
- **Stage only this sprint's paths.** `git status` before every commit; commit messages carry the task ID.

**Commands.**

```sh
export PSPDEV=/home/psp/pspdev
export PATH=$PSPDEV/bin:$PATH
set -e

cd /home/psp/raylib-psp
mkdir -p /home/psp/evidence/sprint058

# I/O probe: open/read/close at sizes, stdio vs sceIo.
( cd psp-tests/io_probe && make clean && make PSP_LARGE_MEMORY=0 \
    && cp io_probe.prx /tmp/p11s8/app.prx )
( cd /tmp/p11s8
  timeout 20 pspsh -e "reset"; sleep 6
  ( printf './app.prx\n'; sleep 90 ) | timeout 110 pspsh )

# Load arms: stdio/calloc (control), sceIo/malloc (change), then the loader thread.
( cd src && make clean && make PLATFORM=PLATFORM_PSP -j8 PSP_LOAD_IO=0 \
    && cp libraylib.a $PSPDEV/psp/lib/libraylib.a )
( cd psp-tests/load_path && make clean && make PSP_LARGE_MEMORY=0 LOAD_ARM=stdio \
    && cp load_path.prx /tmp/p11s8/app.prx )
( cd /tmp/p11s8
  timeout 20 pspsh -e "reset"; sleep 6
  ( printf './app.prx\n'; sleep 90 ) | timeout 110 pspsh )

( cd src && make clean && make PLATFORM=PLATFORM_PSP -j8 PSP_LOAD_IO=1 \
    && cp libraylib.a $PSPDEV/psp/lib/libraylib.a )
# repeat the load run with LOAD_ARM=sceio, then LOAD_ARM=thread
```

**Do not touch.** The decoders (`stb_image`, `stb_truetype`, `cgltf`, `tinyobjloader`), the texture upload/VRAM
path (phase 6), pspgu, the audio backend (phase 9) and the renderer. This sprint changes the byte path and the
loader, not the formats or the draw paths.

**Done means** §8, with the I/O table, the chosen `LoadFileData` path, the loader-thread stall table and zero
steady-frame file operations.

## 1. Objective

Six falsifiable results:

1. `sceIoOpen`, `sceIoRead` and `sceIoClose` are timed at several sizes; the ~17 ms open cost is confirmed or
   corrected, and the read size where throughput flattens is named.
2. `LoadFileData` has a stdio arm and a direct-`sceIo` arm, and a `calloc` arm and a `malloc` arm; the chosen
   path has one allocation, no zero-fill, no second copy and a measured load time.
3. Long-lived handles are used where an asset is read more than once; no asset is opened per frame; the open
   count in a steady frame is zero.
4. A chunk cache is tested at 2/8/32/64 KiB on a streamed workload; the table reports hit rate, opens removed
   and main-thread stall, not only bytes.
5. A loader thread with a reserved stack and the VFPU attribute (if it uses VFPU) moves decode/load off the
   render thread; blocking and `sceIo*Async` are compared by main-thread stall.
6. Pre-converted assets (raw texels, swizzled blocks, CLUT atlas, pre-baked font) are compared with device
   decode for at least one asset class; the manifest replaces any runtime directory scan.

## 2. Why now

The heap/stacks sprint (057) made the loader thread possible and the S1 counters made the open/read cost
visible. The load path is the largest remaining block of unmeasured CPU and I/O, and its `double` decoders are
already counted in the 949-site map. This sprint advances the phase's load-path and file-I/O exit criteria.

## 3. Scope

**In:** `src/rcore.c` (`LoadFileData`, `LoadFileText`, the callbacks, `ScanDirectoryFiles`), a new
`psp-tests/io_probe` and `psp-tests/load_path`, the loader thread in `src/platforms/rcore_psp.c` or a test
harness, and the load-path switches. Asset conversion tooling is in scope only if it is needed to produce one
pre-converted test asset.

**Out:** decoder changes, texture upload/swizzle (phase 6), audio streaming (phase 9), pspgu I/O, and the
renderer. A cache for VRAM textures belongs to phase 6, not this sprint.

## 4. Preconditions

- [ ] Sprints 052/056/057 are in place: counters, VFPU copy (if adopted), heap/stack rules
- [ ] A file on the Memory Stick (or `host0:`) is available at several sizes, with a known checksum
- [ ] `pspsh` answers; `reset` done; the open cost can be measured repeatedly
- [ ] The steady-frame open/read counter is proven to fire on a known `fopen`
- [ ] The loader stack is reserved and the thread attribute is understood
- [ ] A streamed workload exists (music or a model), or a repeated-read probe stands in
- [ ] `git status` understood; only this sprint's paths will be staged

## 5. Tasks

Status vocabulary: `todo` · `doing` · `done` · `blocked` · `dropped`.

| ID | task | status | verification | evidence |
|---|---|---|---|---|
| T1 | **I/O table**: `sceIoOpen`/`sceIoRead`/`sceIoClose` and stdio at 1 KiB, 16 KiB, 256 KiB, 2 MiB; repeat opens; record the ~17 ms open and the read-size knee | `todo` | each size has a row; the open cost is confirmed or corrected; the knee is named | — |
| T2 | **`LoadFileData` arms**: stdio vs direct `sceIo`; `RL_CALLOC` vs `RL_MALLOC`; one allocation, one read, no second copy; measure load time and peak heap | `todo` | the chosen arm has one allocation and no zero-fill; the other arm is archived | — |
| T3 | **Handles and callbacks**: keep a handle for repeated reads; use `SetLoadFileDataCallback` where it removes a copy; no per-frame open | `todo` | steady-frame open count is zero; a repeated read uses the handle | — |
| T4 | **Chunk cache**: 2/8/32/64 KiB MRU on a streamed workload; record hit rate, opens/reads removed and main-thread stall | `todo` | hit rate is measured, not guessed; the best size is explicit; a whole-file load is not claimed as a cache win | — |
| T5 | **Loader thread**: double buffer, reserved stack, VFPU attribute if used; blocking vs async; measure main-thread stall and total | `todo` | no file I/O in the frame window; stall lower than the blocking control; values survive the switch | — |
| T6 | **Pre-converted assets**: compare raw/swizzled/CLUT/pre-baked font with device PNG/TTF decode for one asset class | `todo` | decode time, I/O time and total are separate; the chosen form is recorded | — |
| T7 | **Manifest**: replace a runtime directory scan with an offline list; measure the scan removed and the boot/load time | `todo` | no directory scan in the measured path; the manifest covers the sample's files | — |
| T8 | **Compression decision**: if compression is used, measure inflate CPU and I/O saved; if not, record the decision | `todo` | a row with both times, or a written “not used” decision | — |

### T1 design

- Test both `sceIo*` and stdio on the same file. Newlib's `fopen` may already map to `sceIo*`; the measurement
  decides, not the header. Use `PSP_DISABLE_VSYNC` if the probe is frame-adjacent, but the I/O itself should be
  timed with `sceKernelGetSystemTimeWide()` and not include a swap.
- Open cost: repeat open/close 100 times on a warm cache; report median and p95. The first open after `reset`
  may include the Memory Stick wake; record it separately.
- Read-size knee: increase the read size until throughput flattens; record the size and the plateau. A single
  large read is expected to beat many small ones, but the knee is the evidence.

### T2 design

- `LoadFileData` currently uses `RL_CALLOC`. Replace with `RL_MALLOC` only if the decoder does not rely on zeroed
  bytes; verify the PNG/TTF/OBJ paths and a raw file. A partial read must set `dataSize` and either accept the
  short buffer or fail; do not leave uninitialised bytes that a decoder reads.
- Direct `sceIo`: use `sceIoOpen`, `sceIoGetstat` or one `sceIoLseek` for the size, one `sceIoRead` loop and
  `sceIoClose`; handle short reads. Do not `fseek` twice if a stat gives the size.
- The callback path is the escape hatch for an app that wants a different loader; do not build a second public
  API. If the callback is used, it must still return one allocation the caller frees with `UnloadFileData`.

### T3 design

- A handle cache is only valid for files read repeatedly; a one-shot asset should close. Record the maximum
  handle count and the Memory Stick's limit if one is known.
- `LoadFileText` and `LoadFileData` share the same rules; do not fix one and leave the other.
- A per-frame open is a bug, not a cache candidate: find it with the counter and remove the call, then consider
  a handle.

### T4 design

- The cache is a fixed pool of chunks with an MRU list; the reference uses 8 KiB and 8 MB/2 MB. Test 2/8/32/64
  KiB on the same streamed workload, and record the hit rate, the opens/reads removed and the main-thread stall.
- A whole-file `LoadFileData` does not hit the cache and should not be used as the cache benchmark. Use a model
  or music stream that reads sequentially and sometimes revisits.
- On the PSP-1000, the cache size must fit the heap/partition; the 057 map decides the budget. A cache that
  causes a late-thread failure is rejected.

### T5 design

- The loader thread is created after the heap/stack reservation and before the first load; a late create is the
  failure case 057 tests. The thread uses a double buffer so the render thread can consume the previous asset
  while the next is decoded.
- Blocking I/O on the thread vs `sceIoOpenAsync`/`sceIoReadAsync`/`sceIoWaitAsync`: compare the main-thread stall
  and the total load time. The lower total with a higher stall loses.
- The render thread must not call `sceIo*` or the decoder while the loader is active; a lock or a queue with a
  clear ownership rule is required. Record the ownership.
- The thread's VFPU attribute is required only if the decoder or the copy uses VFPU; the 056 copy may. If not,
  do not add the attribute just in case.

### T6 design

- Asset classes: a PNG texture, a TTF font, one model format, and a streamed audio file if phase 9 is in. For
  each, compare device decode with a pre-converted form: raw RGBA/PSM texels, swizzled blocks, a CLUT atlas,
  a pre-baked font atlas.
- The pre-converted file may be larger on the Memory Stick; record both bytes and time. The decision is total
  boot/load time, not file size.
- Swizzle and CLUT layout are phase 6's; this sprint only measures the load/decode difference. Do not move the
  texture upload path into the loader.

### T7 design

- Find `ScanDirectoryFiles`/`LoadDirectoryFiles` calls in the measured boot/load path; replace with an offline
  manifest (a text or binary list) generated by the build/host. The Memory Stick is not a database.
- The manifest must be versioned with the assets; a missing entry is a visible failure, not a silent skip.

### T8 design

- If a compressed asset is used (zlib/deflate), measure inflate CPU with the S1 zone and the I/O saved by the
  smaller file. Decompression on the render thread is a frame bug even if it is a size win; move it to the
  loader or reject it.
- If no compression is used, write the decision and the reason; do not leave the task blank.

### Task log (append-only)

- 2026-11-21 — sprint written; all tasks `todo`, no device run

## 6. Budgets

| budget | limit | measured | tag |
|---|---|---|---|
| steady frame | zero file opens/reads | | |
| load | open/read/decode/upload separated; no frame stall | | |
| heap | one allocation per file; cache within the 057 budget | | |
| main-thread stall | async/loader lower than blocking control | | |
| boot/load | the manifest and pre-converted assets reduce total time or are rejected | | |

## 7. Measurements

Protocol: device only; `reset` before memory/I/O arms; one variable per arm. Every row carries the boot id.

**I/O:**

| API | size | open µs | read µs | close µs | throughput | notes |
|---|---|---|---|---|---|---|
| `sceIo*` | 1 KiB | | | | | |
| `sceIo*` | 16 KiB | | | | | |
| `sceIo*` | 256 KiB | | | | | |
| `sceIo*` | 2 MiB | | | | | |
| stdio | 2 MiB | | | | | |

**Load path:**

| arm | allocation | zero-fill | copies | load median | peak heap | steady opens | notes |
|---|---|---|---|---|---|---|---|
| stdio/calloc | | | | | | | |
| sceIo/malloc | | | | | | | |
| thread | | | | | | | |

**Cache and assets:**

| cache | chunk | hit rate | opens removed | main stall | notes |
|---|---|---|---|---|---|
| | 2 KiB | | | | |
| | 8 KiB | | | | |
| | 32 KiB | | | | |
| | 64 KiB | | | | |

| asset | device decode | pre-converted | bytes | total | chosen |
|---|---|---|---|---|---|
| | | | | | |

## 8. Exit criteria

- [ ] the open cost is confirmed or corrected and the read-size knee is named
- [ ] `LoadFileData` has one allocation, no zero-fill and no avoidable copy in the chosen arm
- [ ] the steady frame has zero file opens/reads
- [ ] the chunk cache has a hit-rate table and an explicit size, or is rejected with the reason
- [ ] the loader thread runs with a reserved stack and the correct attribute; main-thread stall is lower than
      the blocking control, or the blocking control is chosen
- [ ] one asset class is compared device-decode vs pre-converted, with bytes and time
- [ ] the runtime directory scan is replaced by a manifest or explicitly kept with a reason
- [ ] the compression decision is written
- [ ] the capture gate and the 2D corpus are unchanged
- [ ] the archive has `SHA256SUMS`

## 9. Result

Not started. Record the I/O table, the chosen `LoadFileData` path, the cache decision, the loader-thread stall,
the asset comparison and the manifest/compression decisions. A cache that does not pay is a result.

## 10. Follow-ups

- **Sprint 059** uses the load phase in the boot-time split and the final profile.
- **Phase 9** owns audio streaming; the loader thread must not duplicate its buffering.
- **Phase 6** owns texture upload/swizzle; the pre-converted asset format must not move that path here.
