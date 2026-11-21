# Sprint 057 (P11-S6) — Heap, stacks and alignment

| | |
|---|---|
| Roadmap phase | [Phase 11 — General optimisation outside the renderer](../phases/11-general-optimisation.md) |
| Sprint | P11-S6 |
| Branch / base commit | `master` @ `e307fe05` (docs; no Phase 11 code yet) |
| Started / closed | 2026-11-21 (written) / — |
| Build number(s) | pspgu `b24`; `libraylib.a` with `PSP_HEAP_KB`/pool switches; numbers assigned when the sprint opens |
| Tasks | 0/8 done |

## 0. Handover

**Situation.** No `PSP_HEAP_SIZE_KB` is declared anywhere in the tree. The shipped ELF links libcglue's weak
`sce_newlib_heap_kb_size` and `_sbrk` chooses the size; the "1 MB heap" figure in the memory map is a past
probe. The memory map also records that thread stacks come from the user partition, not the heap, and that a
late thread can fail with `0x80020190` while the log shows megabytes free. This sprint measures the real heap,
decides the size, removes per-frame allocation where the counters find it, and makes thread stacks and buffer
alignment explicit.

**Read before acting**, in this order:

1. `../memory-map.md` §3 — the partition, free-memory and thread-stack numbers; §6 — the `reset` and measurement
   protocol; §9 — the “late thread fails with free heap” trap.
2. `../phases/11-general-optimisation.md` §0 finding 1 — why the “declared 1 MB heap” premise is stale; the
   Daedalus cross-check — `PSP_HEAP_SIZE_KB(-256)` and the non-coalescing heap counter-example.
3. `../sprints/052-phase11-measurement-harness.md` §5 T3 — the allocation/open counters this sprint uses.
4. `$PSPDEV/psp/sdk/include/pspmoduleinfo.h:95-101` — `PSP_HEAP_SIZE_KB` and the threshold macro;
   `$PSPDEV/psp/sdk/lib/libcglue.a` `_sbrk.o` — the weak symbol that chooses the size.
5. `src/platforms/rcore_psp.c:540-560` — the existing `update_thread` creation and stack size; `:586-765` —
   `InitPlatform` and the boot line.
6. `src/rcore.c` — `RL_MALLOC`/`RL_CALLOC`/`RL_FREE` and `LoadFileData`; `src/rtextures.c` — image buffers;
   `src/rtext.c` — font buffers; `tools/pspgu/` — the driver's own allocations (read-only here).

**First action.** Add a boot heap map to the sprint 052 boot line: declared size (or `weak default`), user
partition free, `sceKernelTotalFreeMemSize()`, `sceKernelMaxFreeMemSize()`, main-thread stack and the known
thread stacks. Run it once with no declaration and once with `PSP_HEAP_SIZE_KB(-256)` before changing any
allocation code.

**Hard rules.**

- **The declaration is in the executable, not `libraylib.a`.** `PSP_HEAP_SIZE_KB` defines a symbol in the PRX
  main object. The library can document the default; only the sample's PRX boilerplate can declare it.
- **`PSP_LARGE_MEMORY=0` stays.** The PSPSDK default asks for a 40 MB partition on a 32 MB console.
- **`reset` before every memory run.** `kill` leaks the module heap and leaves later runs in a shrunken
  partition; a heap comparison without `reset` is invalid.
- **A stable frame with a shrinking largest block is still a leak.** The soak records free bytes and largest
  free block, not just a successful run.
- **A late thread failure is a result.** If thread creation fails with `0x80020190`, record the free/largest
  values at that moment; do not “fix” it by shrinking a stack without measuring.
- **Alignment is a correctness requirement.** VFPU, GE and DMA buffers are 16-byte aligned; over-align
  long-lived buffers instead of calling `memalign` per object.
- **Volatile partition 5 is Slim-only.** It cannot be measured on the PSP-1000 under §6; the decision is
  recorded, and an implementation must fail visibly rather than fall back to `malloc`.
- **No renderer change.** `tools/pspgu/` owns its VRAM/EDRAM allocator; this sprint touches the user heap and
  asset buffers only.
- **Stage only this sprint's paths.** `git status` before every commit; commit messages carry the task ID.

**Commands.**

```sh
export PSPDEV=/home/psp/pspdev
export PATH=$PSPDEV/bin:$PATH
set -e

cd /home/psp/raylib-psp
mkdir -p /home/psp/evidence/sprint057

# Heap map: compile the same PRX with no declaration, then with -256.
heap_arm() {                                     # $1 tag, $2 PSP_HEAP_KB make var
    ( cd psp-tests/heap_probe && make clean \
        && make PSP_LARGE_MEMORY=0 PSP_HEAP_KB="$2" \
        && cp heap_probe.prx /tmp/p11s7/app.prx )
    ( cd /tmp/p11s7
      timeout 20 pspsh -e "reset"; sleep 6
      ( printf './app.prx\n'; sleep 60 ) | timeout 80 pspsh
      pspsh -e "scrshot host0:/$1.raw" )
}

heap_arm s057-weak ""
heap_arm s057-1mb  "1024"
heap_arm s057-minus256 "-256"

# Soak: the chosen arm, 30 minutes, log free/largest every 60 s.
( cd psp-tests/heap_probe && make clean \
    && make PSP_LARGE_MEMORY=0 PSP_HEAP_KB="-256" SOAK_SECONDS=1800 \
    && cp heap_probe.prx /tmp/p11s7/app.prx )
( cd /tmp/p11s7
  timeout 30 pspsh -e "reset"; sleep 6
  ( printf './app.prx\n'; sleep 1900 ) | timeout 1920 pspsh )
```

**Do not touch.** `tools/pspgu/` (driver VRAM), `src/rlgl.h`, the batcher, `src/rshapes.c`, `src/rtextures.c`
texture upload and the public API. This sprint changes heap declaration, asset buffers and thread setup.

**Done means** §8, with the heap map, the chosen declaration, zero steady-frame allocations, a stable 30-minute
soak and the thread/alignment evidence.

## 1. Objective

Six falsifiable results:

1. The boot line reports the declared heap (or `weak default`), user-partition free, total free, largest free,
   main-thread stack and the known thread stacks.
2. No heap declaration, 1 MB and `PSP_HEAP_SIZE_KB(-256)` each have a boot map and a frame/load row; the chosen
   declaration has a written reserve for thread stacks.
3. The S1 allocation counter reports the per-frame `malloc`/`free` count; every non-zero owned site is replaced
   by a fixed pool or proven to be load-time only.
4. A 30-minute soak records free bytes and largest free block at fixed intervals; the largest block does not
   shrink without an explanation.
5. A late thread that uses VFPU is created with `PSP_THREAD_ATTR_VFPU` and a measured stack; if it fails, the
   free/largest values at failure are recorded.
6. Asset buffers are 16-byte aligned (VFPU/GE/DMA) and long-lived buffers are over-aligned; the D-cache
   writeback uses exact ranges, not a whole-cache invalidate in a steady frame.

## 2. Why now

The memory map's heap premise is stale, and the phase cannot budget memory until the real declaration and the
free/largest numbers are measured. The allocation counters from 052 now exist, so per-frame churn can be found
instead of guessed. This sprint advances the phase's heap and thread-stack exit criteria and unblocks the
loader thread in 058.

## 3. Scope

**In:** the test PRX heap declaration (`psp-tests/*/main.c` or `psp_module.c`), a `heap_probe` harness, the boot
line, `RL_MALLOC`/`RL_FREE` call sites that the counters identify as per-frame, asset-buffer alignment in
`rcore.c`/`rtextures.c`/`rtext.c`, thread creation in `src/platforms/rcore_psp.c`, and the D-cache range helper
for asset buffers.

**Out:** pspgu's VRAM/EDRAM allocator (phase 5/6), the per-frame renderer allocations inside the driver, the
VFPU copy helper (056), the loader thread (058), and the volatile partition implementation if the decision is
deferred.

## 4. Preconditions

- [ ] Sprint 052 counters and boot line are in place; sprint 053's flags are the default
- [ ] `pspsh` answers; `reset` leaves partition 2 at 25,149,440 bytes
- [ ] The weak-default build boots and prints a heap map
- [ ] The allocation counter can see `malloc`/`free` and is proven to fire on a known allocation
- [ ] The capture gate and the full-feature build are available
- [ ] The PSP-1000 is the measurement console; a Slim is not required for this sprint's measured rows
- [ ] `git status` understood; only this sprint's paths will be staged

## 5. Tasks

Status vocabulary: `todo` · `doing` · `done` · `blocked` · `dropped`.

| ID | task | status | verification | evidence |
|---|---|---|---|---|
| T1 | **Boot heap map**: declared size, partition free, `sceKernelTotalFreeMemSize()`, `sceKernelMaxFreeMemSize()`, main stack and known thread stacks; log one line | `todo` | values change with the declaration and agree with the memory map's arithmetic | — |
| T2 | **Declaration switch**: `PSP_HEAP_KB` in the test PRX boilerplate; arms for weak default, 1 MB and `-256`; document that the symbol is in the executable | `todo` | `psp-nm` shows `sce_newlib_heap_kb_size` with the expected value; no declaration means weak | — |
| T3 | **Heap-size A/B**: weak vs 1 MB vs `-256`; frame/load zones, free/largest, thread creation | `todo` | each arm boots; the largest free and the reserve are recorded; no late-thread failure in the chosen arm | — |
| T4 | **Allocation audit and pools**: use the S1 counter; classify per-frame vs load-time; replace owned per-frame sites with fixed pools sized from the max; record rejected sites | `todo` | steady frame reports zero allocations in owned code; a known allocation is still counted | — |
| T5 | **Alignment**: 16-byte for VFPU/GE/DMA buffers; over-align long-lived buffers; remove per-object `memalign` where a pool can carry the alignment | `todo` | addresses are 16-byte aligned; no per-frame `memalign`; size cost recorded | — |
| T6 | **Threads and stacks**: create the VFPU worker early; size the stack from a measurement; set `PSP_THREAD_ATTR_VFPU`; test a deliberately late create | `todo` | thread starts; VFPU results are correct; the late-create failure records free/largest or the arm passes | — |
| T7 | **Volatile partition 5**: decide defer/implement; if implemented, Slim-only, `scePowerLock(0)` while held, and a visible failure (no `malloc` fallback); on the Phat it must fail visibly | `todo` | the decision is written; a Phat probe reports the failure, not a silent fallback | — |
| T8 | **D-cache and soak**: exact `sceKernelDcacheWritebackRange` for asset buffers; no whole-cache invalidate in a steady frame; 30-minute free/largest log | `todo` | range writeback time is measured; free/largest stable over 30 min; no whole-cache call in the frame path | — |

### T1 design

- Use the SDK calls the platform already exposes: `sceKernelTotalFreeMemSize()` and
  `sceKernelMaxFreeMemSize()`. Partition free can be read with `sceKernelPartitionInfo` or from the boot line's
  existing values; record the source.
- Print one line, not per-frame: `HEAP declared=<n|weak> total=<n> largest=<n> mainStack=<n> threads=<n>`.
- Compare the arithmetic with memory-map §3: a declared heap costs bytes off the partition; a thread stack is a
  separate allocation, not heap. If the numbers do not add up, fix the map, not the print.

### T2 design

- `PSP_HEAP_SIZE_KB(size_kb)` is a macro that defines `int sce_newlib_heap_kb_size = (size_kb);`. Put it in the
  PRX's `main.c`/`psp_module.c`, behind a `PSP_HEAP_KB` make variable:
  ```c
  #if defined(PSP_HEAP_KB)
      PSP_HEAP_SIZE_KB(PSP_HEAP_KB);
  #endif
  ```
  A negative value is the pspdev “all remaining minus reserve” form; confirm the `_sbrk` handling with a boot
  map, not from the Daedalus comment alone.
- The weak default is a real arm. The current shipped behaviour is “no declaration”; the sprint must measure it
  before replacing it.

### T3 design

- Keep the same workload and clock across arms; `reset` between them. The heap size changes the free/largest
  values and can change allocator behaviour, so record the allocation count too.
- The reserve must cover the main stack, the `update_thread` stack and any worker created before the first
  frame. A late worker in 058 needs its stack reserved at boot; if it is not, the `-256` arm is invalid.
- If `-256` wins only because it removes a failure, say so; the number is the free/largest and the frame, not
  the crash.

### T4 design

- The S1 counter reports all `malloc`/`free`/`calloc`/`realloc`, including the library and pspgu. Attribute the
  owned sites with the static call graph; do not claim a per-call dynamic split the counter cannot produce.
- A fixed pool is a static array or a preallocated block with a free list; size it from the high-water, not the
  worst case. A pool that changes the public API is out of scope.
- `LoadFileData`'s allocation is load-time, not per-frame; it belongs to 058's `calloc`/`malloc` decision. Do not
  pool it here unless the counter shows a frame-time call.

### T5 design

- Newlib's `malloc` alignment is not a VFPU/GE/DMA contract; use `memalign(16, ...)` for the long-lived buffers
  or over-align the pool's base. The driver's own allocations are phase 5/6.
- Audit the buffers that cross a DMA or VFPU boundary: texture upload scratch, font atlas, vertex arenas (driver),
  and any `lv.q`/`sv.q` copy source/destination. Record the address and the alignment.
- Remove a per-object `memalign` only when the pool already carries the alignment; otherwise the fix is worse
  than the call.

### T6 design

- The current `update_thread` is created with a 0xFA0-byte stack and no VFPU attribute. A worker that calls the
  056 helpers needs `PSP_THREAD_ATTR_VFPU`; a thread without it corrupts its VFPU state across switches.
- Size the stack from the measured high-water plus a margin. Do not shrink the main stack to make a number; the
  main stack is not the heap.
- The late-create test deliberately allocates most of the partition (or delays the create until after a large
  load) and records `0x80020190` plus free/largest at failure. A failure that cannot be reproduced is recorded
  as such.

### T7 design

- On the PSP-1000 there is no volatile partition 5; `sceKernelVolatileMemLock` fails. The phase's measurement
  console cannot test the Slim path. The decision is therefore “defer” unless a Slim is available and the
  protocol is extended to it.
- If implemented, use partition 5, `scePowerLock(0)` while held, and fail visibly. Do not copy Daedalus's
  `malloc` fallback: it turns a missing feature into an untracked heap allocation.
- Record the model check (`sceKernelGetModel()`) and the fallback policy in the boot line.

### T8 design

- Use `sceKernelDcacheWritebackRange`/`sceKernelDcacheWritebackInvalidateRange` for the exact asset range; no
  `sceKernelDcacheWritebackInvalidateAll()` in a steady frame. Measure the range call's cost with the S1 zone.
- The soak runs 30 minutes with the chosen heap arm; log every 60 s: free, largest, allocated, frame median.
  A flat largest block is not a leak; a shrinking one is, even if no allocation fails.
- Save the log and plot under `/home/psp/evidence/sprint057/`.

### Task log (append-only)

- 2026-11-21 — sprint written; all tasks `todo`, no device run

## 6. Budgets

| budget | limit | measured | tag |
|---|---|---|---|
| heap | chosen declaration with a stated stack reserve | | |
| steady frame | zero owned allocations | | |
| soak | free/largest stable for 30 minutes | | |
| thread | creation succeeds with the chosen declaration | | |
| alignment | 16-byte for VFPU/GE/DMA | | |
| D-cache | range writeback measured; no whole-cache in frame | | |

## 7. Measurements

Protocol: device only; `reset` before every memory arm; one variable per arm. Every row carries the boot id.

**Heap map:**

| arm | declaration | partition free | total free | largest | main stack | threads | frame median | notes |
|---|---|---|---|---|---|---|---|---|
| weak | none | | | | | | | |
| 1 MB | `PSP_HEAP_SIZE_KB(1024)` | | | | | | | |
| reserve | `PSP_HEAP_SIZE_KB(-256)` | | | | | | | |

**Allocation and soak:**

| run | build | workload | allocations/frame | free | largest | frame median | notes |
|---|---|---|---|---|---|---|---|
| before | | bunnymark | | | | | |
| after | | bunnymark | | | | | |
| soak 0 min | | chosen arm | | | | | |
| soak 30 min | | chosen arm | | | | | |

**Threads and D-cache:**

| test | result | free/largest | notes |
|---|---|---|---|
| early VFPU worker | | | |
| late create | | | |
| range writeback | | | |

## 8. Exit criteria

- [ ] the boot line reports the declared heap, free, largest and stacks
- [ ] weak, 1 MB and `-256` each have a boot map and a frame/load row
- [ ] the chosen declaration has a stated stack reserve and no late-thread failure
- [ ] the steady frame reports zero owned allocations, or every non-zero site is documented as load-time
- [ ] asset/VFPU/GE/DMA buffers are 16-byte aligned; the alignment evidence is recorded
- [ ] VFPU threads carry the attribute and the matrices are not clobbered
- [ ] the volatile partition decision is written; a Phat probe fails visibly if implemented
- [ ] the 30-minute soak has stable free/largest; the D-cache range cost is measured
- [ ] the capture gate and the 2D corpus are unchanged
- [ ] the archive has `SHA256SUMS`

## 9. Result

Not started. Record the real heap/default, the chosen declaration and reserve, the allocation sites, the soak
table, the thread result and the volatile decision. A stale memory-map number is corrected here.

## 10. Follow-ups

- **Sprint 058** creates the loader thread; it consumes the stack/attribute rules from T6.
- **Sprint 059** uses the heap map in the boot-time split and the final soak.
- If the volatile partition is deferred, record the Slim dependency in the phase page and memory-map §3.
