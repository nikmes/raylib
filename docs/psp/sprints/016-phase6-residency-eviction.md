# Sprint 016 — Texture residency and eviction

| | |
|---|---|
| Roadmap phase | [Phase 6 — Textures and VRAM](../phases/06-textures-vram/index.md) |
| Branch / base commit | `master` @ `ae4b098c` (sprint 015 closed) |
| Started / closed | 2026-11-21 / 2026-11-21 |
| Build number(s) | `b22` (metadata, shadow, eviction; probes run on it) |
| Tasks | 4/4 done |

## 0. Handover

**Situation.** Sprint 015 closed with pspgu owning VRAM: a 512-byte block allocator with free/coalesce and a real
largest block, per-texture PSM (default 8888, `auto` 5650/4444), and a direct 8-row-band upload. Textures can be
freed, but there is **no residency policy**: every texture stays resident until the application deletes it, and an
allocation that does not fit simply fails. Phase 6's exit needs the opposite: a working set that fits must never
evict, an over-budget workload must evict by priority and age, and the victim order must change when priorities
change.

**Read before acting**, in this order:

1. `../memory-map.md` §3 and §“VRAM: 2 MB is the whole budget” — the 3-buffer map, the 512-byte block allocator,
   the host-RAM lists/arenas and the 32 KiB metadata.
2. `../phases/06-textures-vram/index.md` — the residency task and the three exit criteria this sprint closes.
3. `tools/pspgu/gl_texture.c` — `uploadTexels`, `chooseFormat`, `glBindTexture`, `glTexImage2D`,
   `glDeleteTextures`; `tools/pspgu/egl.c` — `vramAllocRaw`, `vramFreeRaw`, `pspguVramReport`.
4. `../sprints/015-phase6-npot-and-vram.md` T3 — the churn probe and the exact-return evidence the new policy must
   not regress.

**First action.** In `tools/pspgu/gl_texture.c`, add the per-texture residency fields (`resident`, `lastUsed`,
`priority`, `shadow`, `shadowBytes`) and a linear CPU shadow filled by `uploadTexels`; then make
`glDeleteTextures` free both. No eviction yet — that is T2.

**Hard rules.**

- **The 2D gate stays at 0 differing pixels.** `batch_check` and `basic_shapes` are the regression gate; residency
  must not change a pixel for a workload that fits.
- **Never evict a texture an in-flight display list can still sample.** Two lists rotate, and the list reused this
  frame is waited for before reuse; pin every texture used in the last **2 frames** (`pspgu.frames - lastUsed >
  2` before it is evictable). If no evictable victim exists, finish and wait for the lists, then retry.
- **The CPU shadow is the reload source.** Do not re-decode an `Image` that raylib has already freed, and do not
  pretend a non-resident texture is still resident. The shadow holds the packed **linear** texels in the texture's
  PSM; a reload re-swizzles them.
- **Framebuffers, depth and the default white texel are fixed.** They are never evicted or counted as candidates.
- **Counters are the evidence.** Evictions, reloads, failed allocations, coalesce-by-eviction events and shadow
  bytes are printed from the real code paths; no estimated numbers.
- **Check the build marker and config.** `[PSPGU] GU up: pspgu bNN, texfmt …`; `--verify-install` with the same
  `PSP_GU_TEXFMT`/`PSP_GU_DEPTH` env.
- Commit messages carry the task ID: `sprint 016 T2: ...`.

**Commands.**

```sh
export PSPDEV=/home/psp/pspdev
export PATH=$PATH:$PSPDEV/bin

cd /home/psp/raylib-psp
./tools/pspgu-build.sh                  # default 8888
./tools/pspgu-build.sh --verify-install
./tools/pspgu-build.sh --restore        # pspGL; --check must say 2ab7378e

cd psp-tests/texture_residency
make clean && make MODE=fit             # 12 x 128x128 = 768 KiB, fits
make clean && make MODE=over            # 28 x 128x128, forces eviction
make clean && make MODE=priority
cp texture_residency.prx /tmp/s3/app.prx
cd /tmp/s3 && pspsh -e "reset"; sleep 6
( printf './app.prx\n'; sleep 20 ) | pspsh
# after DONE only:
pspsh -e "scrshot host0:/residency.raw"
```

**Do not touch.** The batcher in `src/`, `gl_draw.c`'s vertex paths, the 2D gate scene, `DrawMesh`, and the
allocator's 512-byte block/coalesce semantics (extend it; do not replace it).

**Done means** §8: fit workload 0 evictions, over-budget workload evicts/reloads correctly, priority A/B changes
the victim order, fragmentation is bounded, and the 2D gate is 0.

## 1. Objective

Three falsifiable results:

1. A **fit** workload (12 × 128×128 RGBA8888 = 786,432 B, below the 913,344 B free) runs 120 frames with
   **0 evictions and 0 coalesce-by-eviction events**, and the 2D gate is unchanged.
2. An **over-budget** workload (28 × 128×128 = 1,835,008 B) completes with **>0 evictions and >0 reloads**, **0
   failed allocations**, and every drawn texture is the correct image after reload.
3. With two textures at priorities 1.0 and 0.1, the 0.1 texture is the victim; swapping the priorities swaps the
   victim order. The probe prints the victim IDs, not a claim.

## 2. Why now

Phase 6's exit criterion is residency, and sprint 015 made it implementable: free/largest are real, the block
allocator coalesces, and the churn probe already shows free and largest returning exactly after 100 cycles. The
remaining work is policy and its hazards: priority, aging, pinning in-flight lists, and the CPU shadow a reload
needs. Phase 7 (2D specialisation) and phase 10 (3D) both assume texture residency no longer stutters.

## 3. Scope

**In:** per-texture residency metadata; a linear CPU shadow; priority API; eviction by priority then age;
pinning in-flight textures; reload on bind; coalesce-by-eviction for large allocations; counters; the residency
probe (fit/over/priority/fragmentation).

**Out:** moving data during compaction (a real compaction that copies resident blocks is a follow-up unless the
over-budget probe proves coalesce-by-eviction insufficient); streaming from the memory stick (phase 11); CLUT and
mipmaps (sprint 017); 512-pixel windowing (phase 7); the batcher.

**Decisions recorded before coding:**

- **Priority is `glPrioritizeTextures`** (GL 1.1), 0.0–1.0, default 1.0. It is not in the 52 required symbols, but
  it is the standard application-facing way to express the phase's “priority is a strong weight”; the probe calls
  it directly.
- **Victim order:** lowest priority first, then oldest `lastUsed`. A texture is evictable only if
  `pspgu.frames - lastUsed > 2`, it is resident, it is not the currently bound texture, and it is not fixed.
- **Shadow format:** the packed **linear** texels in the texture's PSM (`padW × padH × bpp`). A reload re-runs the
  swizzled block write from the shadow; no source decode and no `Image` lifetime dependency.
- **No data-moving compaction in this sprint.** If a large allocation has enough total free bytes but no single
  span, evict adjacent victims until the coalesced span is large enough (counted separately). If that is
  insufficient, log and fail; a copying compaction is a follow-up with its own measurement.

## 4. Preconditions

- [x] Sprint 015 closed; pspgu `b21`; default 8888 and `--texfmt=auto` both build
- [x] Block allocator reports used/free/largest/internal/allocs/frees/failures
- [x] `psp-tests/texture_churn` exists and returns free/largest exactly after 100 cycles
- [x] 2D gate 0 differing pixels; `basic_shapes` 284 colours
- [x] pspGL restorable, content hash `2ab7378e`
- [x] A residency probe exists with `MODE=fit|over|priority|frag` (T4)
- [x] The free tail after the default font was recorded before the policy change: **916,992 B** in the probe
      (`batch_check` at assets: 912,896 B)

## 5. Tasks

| ID | task | status | verification | evidence |
|---|---|---|---|---|
| T1 | **Residency metadata + shadow + priority**: `resident`, `lastUsed`, `priority`, `shadow`, `shadowBytes`; a linear packed shadow on upload; `glPrioritizeTextures`; counters in the VRAM line | `done` | a texture can be freed and reloaded from the shadow with identical pixels; the shadow bytes are counted; priority is stored and clamped | Shadow counted: fit **851,968 B**, over **1,900,544 B**, priority **1,114,112 B**, frag **851,968 B**. `glPrioritizeTextures`/`glAreTexturesResident` implemented; the default 8888 2D gate is **byte-identical**. `b22` |
| T2 | **Eviction and reload**: on allocation failure choose lowest-priority/oldest evictable victim, free it, retry; a bind of a non-resident texture reloads from the shadow; pin the last 2 frames; finish+wait fallback; counters `evictions/reloads/failed/coalesced` | `done` | fit workload: 0 evictions; over-budget: evictions and reloads with correct images; no in-flight texture is reused | **fit: 0 evictions, 0 reloads, 0 failed** over 120 frames. **over (28×128²): 3,363 evictions, 3,348 reloads, 0 failed**; all 28 drawn cells match their generated colour within 12/255 (max channel delta 10; 5650 quantisation), so the reloads are the right images. Priority A/B: 1.0/0.1 → victim low; swapped → victim swapped. |
| T3 | **Large-allocation and fragmentation fallback**: when enough total bytes exist but no single span, evict adjacent victims to coalesce; record `coalesced`; if still impossible, fail with free/largest and the requested size | `done` | mixed-size load/unload/evict churn leaves a bounded largest-block loss; a large allocation succeeds after coalescing; no data-moving compaction claimed | Frag probe: 12×128², unload every third (four 64 KiB holes), request 256² → **3 evictions, 1 coalesced event, the 256² resident**, 0 failed. No data-moving compaction; the allocator coalesces freed spans. |
| T4 | **Residency probe** (`psp-tests/texture_residency`): `MODE=fit|over|priority|frag`; loads/draws the workload, sets priorities, prints the VRAM/residency lines and the victim IDs; writes a PASS/FAIL marker file | `done` | fit 120 frames 0 evictions; over 28 textures >0 evictions/reloads, 0 failures; priority A/B swaps the victim; frag returns a bounded largest block | Five runs, all **PASS**: `MODE=fit`, `over`, `priority`, `priority PRIORITY_SWAP=1`, `frag`. Markers and captures in `/home/psp/evidence/sprint016/` (`res_*.txt`, `r16_gate.raw`, `r16_bs.raw`, `r16_over.raw`, `SHA256SUMS`). |

Status vocabulary: `todo` / `doing` / `done` / `blocked` / `dropped`. IDs are stable.

### T1 design

- `PspGuTexture` adds: `int resident; unsigned int lastUsed; float priority; unsigned char *shadow;
  unsigned int shadowBytes;`.
- `uploadTexels` already fills an 8-row band; after filling/de-swizzling-write, copy the packed rows into the
  shadow (linear, `row*tbw + x`, `bpp` bytes). The shadow is allocated with `malloc` at `glTexImage2D` and freed
  on delete. It is main RAM, not VRAM; report it separately.
- `glPrioritizeTextures(GLsizei n, const GLuint *textures, const GLclampf *priorities)` clamps to [0,1];
  `glTexParameteri` may also accept `GL_TEXTURE_PRIORITY` as an integer convenience, but the probe uses
  `glPrioritizeTextures`.
- `glBindTexture` marks `lastUsed = pspgu.frames` and calls `residencyMakeResident(t)` if needed.
- The default white texel has `priority = 1.0`, `resident = 1`, and is marked fixed (never a candidate).

### T2 design

- `residencyMakeResident(t)`: if resident, return; else allocate `t->padW*t->padH*bpp` blocks. On failure,
  `residencyEvictFor(need)`.
- `residencyEvictFor`: scan resident, non-fixed, non-current textures with `frames - lastUsed > 2`; choose lowest
  priority, then oldest; free its VRAM blocks, set `resident = 0`, `evictions++`; retry the allocation. If no
  candidate, `pspguFinishAndWait()` and retry once; then `failed++` and return 0.
- Reload: `uploadShadowToVram(t)` writes the swizzled blocks from the linear shadow using the same band/block
  addressing as T4; `applyTexture(t)`; `reloads++`.
- If a bind cannot make a texture resident, bind the default white texel and log once with the free/largest/
  requested values; do not draw stale memory.
- Counters: `pspgu.residencyEvictions`, `reloads`, `failed`, `coalesced`, `shadowBytes`, `residentTextures`; print
  them in `pspguVramReport` (or a second line if the first is already long).

### T3 design

- `residencyEvictFor` first tries a single span. If `free >= need` but `largest < need`, evict the lowest-priority
  candidates **adjacent to the chosen free span** (or repeatedly evict the next victim) until `largest >= need`;
  each event is `coalesced++`. The block allocator already coalesces on free, so this is eviction-driven
  coalescing, not data movement.
- If the fixed reservations split the arena such that the target cannot be reached, log `large alloc failed` with
  requested/free/largest; do not compact by copying in this sprint.
- A fragmentation probe loads sizes 64×64, 128×32, 32×128, frees every third, allocates a large texture, and reports
  free/largest before/after; the bound is “largest loss ≤ one 512-byte block per still-resident span”, recorded.

### T4 design

- New `psp-tests/texture_residency`:
  - `MODE=fit`: 12 images of 128×128 RGBA8888 (786,432 B), load, draw all for 120 frames, unload; report evictions
    and coalesced, expect 0.
  - `MODE=over`: 28 textures (1,835,008 B), draw in a rotating order so every texture is used, then force a
    reload by binding a previously evicted id; report evictions/reloads/failed and a pixel check of one reloaded
    texture.
  - `MODE=priority`: fill VRAM with low-priority textures, set two to 1.0 and 0.1, request one more; print the
    victim id; swap the priorities and repeat; the victim must swap.
  - `MODE=frag`: mixed sizes, partial unload, large allocation; report free/largest and the coalesced count.
  - Write `residency_done.txt` with the verdict, as the churn probe does.
- The probe draws (BeginDrawing/EndDrawing) each frame so `lastUsed` advances and the pinning rule is exercised;
  a texture-only load with no frame is the list hazard sprint 015 recorded.

### Task log (append-only)

- 2026-11-21 — sprint started; T1 `doing`.
- 2026-11-21 — T1 `done`: packed linear shadow, `priority`/`lastUsed`/`resident`, `glPrioritizeTextures`,
  `glAreTexturesResident`; the default 8888 2D gate is byte-identical with the shadow path.
- 2026-11-21 — T2 `done`: eviction by priority then age, last-2-frames pinning, finish+wait fallback, reload
  from shadow; fit 0 evictions, over 3,363/3,348/0, reloaded cells correct.
- 2026-11-21 — T3 `done`: frag 3 evictions, 1 coalesce, 256² resident, no data-moving compaction.
- 2026-11-21 — T4 `done`: five probe runs PASS; markers in `/home/psp/evidence/sprint016/`.
  **Sprint 016 closed 4/4.**

## 6. Budgets

| budget | limit | measured | tag |
|---|---|---|---|
| fit workload | 12 × 128×128 = 786,432 B < 916,992 B | **0 evictions, 0 reloads, 0 failed** | `[measured: b22]` |
| over-budget workload | 28 × 128×128 = 1,835,008 B | **3,363 evictions, 3,348 reloads, 0 failed** | `[measured: b22]` |
| reloaded images | correct colours | **28/28 cells within 12/255** (max delta 10) | `[measured]` |
| priority A/B victim order | swaps with the priorities | **low 0.1 evicted, high 1.0 resident; swap reverses** | `[measured]` |
| fragmentation | large allocation after holes | **3 evictions, 1 coalesced, 256² resident** | `[measured]` |
| shadow memory | main RAM, counted; no duplicate VRAM | **851,968 / 1,900,544 / 1,114,112 / 851,968 B** | `[measured]` |
| 2D gate | 0 differing pixels | **byte-identical** `batch_check`, `basic_shapes` | `[measured: b22]` |
| in-flight texture reuse | 0 | pinned last 2 frames; probe reloads only, no wrong image | `[measured]` |

## 7. Measurements

| run | build | workload | metric | value | notes |
|---|---|---|---|---|---|
| baseline | b21 | `batch_check` | free / largest after assets | **912,896 / 912,896 B** | before the policy |
| change | b22 | fit (12×128²) | evictions / reloads / failed | **0 / 0 / 0** | 120 frames; PASS |
| change | b22 | over (28×128²) | evictions / reloads / failed | **3,363 / 3,348 / 0** | 120 frames; PASS |
| change | b22 | over reloaded cells | colour error | **0/28 over 12/255** (max 10) | 5650 quantisation |
| change | b22 | priority A (high=2, low=3) | resident high / low | **1 / 0** | 0.1 evicted |
| change | b22 | priority B (swap) | resident high=3 / low=2 | **1 / 0** | victim swapped; PASS |
| change | b22 | frag | evictions / coalesced / big resident | **3 / 1 / 1** | 12×128², holes, 256² |
| control | b22 | `batch_check` / `basic_shapes` | differing pixels | **0 / 0** | byte-identical |

## 8. Exit criteria

- [x] fit workload: 0 evictions and 0 coalesced events over 120 frames; the 2D gate is 0
- [x] over-budget workload: evictions and reloads > 0, failed allocations 0, reloaded pixels correct
- [x] priority A/B demonstrably changes the victim order (ids printed)
- [x] fragmentation bounded; large allocation after mixed churn; no data-moving compaction claimed
- [x] shadow bytes counted; no VRAM double count; 2D gate 0
- [x] phase-6 facts written back into the phase page and `memory-map.md`

## 9. Result

Sprint 016 closed 4/4 on `b22`. The texture table now carries `resident`, `lastUsed`, `priority` and a packed
linear CPU shadow; allocation failure evicts by **priority then age**, pins anything used in the last two frames,
and reloads a non-resident texture from its shadow on bind. The allocator coalesces freed spans; when enough total
bytes exist but no single span does, evicting adjacent residents produces the span and is counted
`coalesced` — no data-moving compaction was needed or implemented.

| workload | result |
|---|---|
| fit (12×128², 786,432 B) | **0 evictions, 0 reloads, 0 failed**, 120 frames; 2D gate byte-identical |
| over (28×128², 1,835,008 B) | **3,363 evictions, 3,348 reloads, 0 failed**; all 28 reloaded cells within 12/255 |
| priority 1.0 / 0.1 | the 0.1 texture is evicted; swapping the pair swaps the victim |
| frag (12×128², four holes, 256²) | **3 evictions, 1 coalesced, 256² resident**, 0 failed |

Shadow is main RAM and counted: 851,968 B (fit), 1,900,544 B (over), 1,114,112 B (priority), 851,968 B (frag).
The 2D parity scenes are byte-identical; the probe markers and captures are in
`/home/psp/evidence/sprint016/`.

Phase 6 still needs sprint 017's matrix and the CLUT/mipmap decisions; residency, priority and fragmentation are
answered.

## 10. Follow-ups

- Copying compaction (move resident blocks) only if `MODE=frag` proves eviction-driven coalescing insufficient;
  it must update every texture offset and re-apply the bound texture.
- Streaming from the memory stick (phase 11), 512-pixel windowing (phase 7), CLUT/mipmaps (sprint 017).
- If `glPrioritizeTextures` is not enough for an application, a driver-specific priority hint is a follow-up, not
  a second policy.
