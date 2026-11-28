# Sprint 095 (P10-S25) — Keeping the port mergeable: quarantine PSP code out of core raylib files

| | |
|---|---|
| Roadmap phase | [Phase 10 — 3D and advanced](../phases/10-3d-advanced/index.md) (port maintenance spanning phases 04, 05, 07, 09) |
| Sprint | P10-S25 |
| Branch / base commit | `master` @ `21bbb3f4`; the `upstream-11-26` merge is written but **unlanded** (device gates pending) — rebase onto it before T1 if it lands first |
| Started / closed | 2026-11-26 (written) / — |
| Build number(s) | driver unchanged; `libraylib.a` hash changes (files moved), measurements must not |
| Tasks | 0/7 |

## 0. Handover

The PSP port is split unevenly. Its platform layer is idiomatic raylib — `rcore_psp.c` (1,321 lines),
`raudio_psp.c` (256, `#include`d by `raudio.c` the way miniaudio backends are), `rlpsp_math.c` — and `rcore.c`
carries only 26 guarded lines. That part matches how raylib adds a platform.

But **1,617 PSP-guarded lines sit inside core portable files**, and they are not all the same kind of deviation:

| file | lines | largest block | what it is |
|---|---|---|---|
| `src/rmodels.c` | **754** | 626 | `PspGeMesh` cache, bounds, the cull classifier, the merge machinery |
| `src/rlgl.h` | 402 | 232 | the PSP batcher (`RLPspVertex`, flush, merge glue) |
| `src/raudio.c` | 257 | 190 | a PSP music-decoder thread, while the device lives in `raudio_psp.c` |
| `src/rshapes.c` | 129 | 44 | mostly **negative** guards, `#if !defined(PLATFORM_PSP)`, replacing raylib's quad paths |
| `src/rtextures.c`, `rtext.c`, `raylib.h`, `rlpsp_math.h` | 49 | — | small additions |

`rlgl.h` is the least objectionable: rlgl is header-only and raylib keeps every `GRAPHICS_API_*` variant inside it,
so a guarded backend there is the same shape. `rmodels.c` and `rshapes.c` are the problem, because our code sits
**inside functions upstream actively edits** (`DrawMesh`, `DrawRectangleLines`, `LoadModelAnimations`) — which is
exactly why the 2026-11-26 upstream merge needed manual work and why the `rshapes` half-pixel fix landed in code we
had replaced.

## 1. Objective

Move PSP-specific implementation out of the portable files and into `src/platforms/`, leaving only small guarded
call sites, **with no behaviour change**. Success is measured three ways: the core-file line count, identical
symbols, identical gates and pair.

## 2. Why now

Every upstream merge taxes this code. The last one conflicted in `rcore.c` and auto-merged `rlgl.h`/`rmodels.c`,
which then require re-verification by hand. Doing this before the *next* merge is cheaper than doing it after two
more.

## 3. Scope

**In:** the four moves below; the visibility changes they need; the acceptance measurements.

**Out:** any optimisation, any behaviour change, any new feature. A moved line must do exactly what it did. The open
redesign sprints (090-094) must not be folded in here.

## 4. Preconditions

- [x] the core-file line count is reproducible (the script in §6)
- [x] `libraylib.a` and driver hashes recorded before the move
- [x] `PSP_CALL_PROBE` and the bench harness exist for the pair
- [x] the two threads that ran without `PSP_THREAD_ATTR_VFPU` are fixed (`640ece10`, `21bbb3f4`) — port-layer
      changes like these are exactly what this sprint is meant to keep out of core files
- [ ] the upstream merge is either landed or rebased onto — do not mix a merge and a move in one diff
- [ ] the counts are re-measured **on the tree T1 starts from** (T0): they differ by the merge:
      master **1,626** lines, `upstream-11-26` **1,617** (the 9-line difference is `rcore.c`, where upstream's
      `#elif` consolidation replaced our split guards)

## 5. Tasks

| # | Task | Status | Acceptance | Result |
|---|---|---|---|---|
| T0 | **Settle the baseline.** Land or rebase `upstream-11-26`, re-run the §6 count on that tree, and record the per-file numbers the move will be measured against | `todo` | counts recorded on the tree T1 starts from; a trial `git merge upstream/master` shows what conflicts today | — |
| T1 | **Mesh path → `src/platforms/rlpsp_mesh.c/.h`.** Move `PspGeMesh`, the bounds table, `pspGeClassifyMesh` family and the merge helpers out of `rmodels.c`; leave guarded call sites in `DrawMesh`/`UnloadMesh`/`rlpspMergeFlush` | `todo` | `rmodels.c` PSP lines ≤ 40; symbols exported through the new header; no double definition | — |
| T2 | **Batcher → `src/platforms/rlpsp_batch.h`.** Move the 402 `rlgl.h` lines behind one `#include` under one guard, preserving include order (the block runs before the GL headers) | `todo` | `rlgl.h` PSP lines ≤ 10; header-only semantics intact; desktop build unchanged | — |
| T3 | **Shapes → one switch.** Replace the scattered `#if !defined(PLATFORM_PSP)` exclusions with a single `PSP_SHAPES_FAST` condition, so upstream quad-path edits merge cleanly | `todo` | `rshapes.c` PSP lines ≤ 40; 2D output byte-identical; the four affected functions named and their upstream history checked | — |
| T4 | **Audio decoder → `raudio_psp.c`.** Move the music-decoder thread next to the device that owns it | `todo` | `raudio.c` PSP lines ≤ 20; audio smoke unchanged | — |
| T5 | **Prove no behaviour change.** Same driver hash, same `libraylib` symbols for our API, gates and pair within tolerance | `todo` | 613 / 284 / 0 / 0 / clip; pair 33.5 / 33.8 ±2 %; `psp-nm` diff empty for `rlpsp*`/`pspgu*` | — |
| T6 | **Close.** Record the line counts before/after, update the port notes, state the merge surface left | `todo` | one number per file, before and after | — |

## 6. Design notes

**The count is the metric.** Reproduce it with:

```sh
python3 - <<'PY'
import re,glob
def blocks(p):
    L=open(p,encoding='utf-8',errors='ignore').read().split('\n'); out=[]; i=0
    while i<len(L):
        if re.match(r'\s*#\s*if.*PLATFORM_PSP',L[i]):
            d=0; j=i; n=0
            while j<len(L):
                if re.match(r'\s*#\s*if',L[j]): d+=1
                elif re.match(r'\s*#\s*endif',L[j]):
                    d-=1
                    if d==0: break
                j+=1; n+=1
            out.append(n); i=j
        i+=1
    return out
for p in sorted(glob.glob('src/*.c')+glob.glob('src/*.h')):
    b=blocks(p)
    if b: print(f"{p}: {len(b)} blocks, {sum(b)} lines")
PY
```

Target: **1,617 → ≤ 200**, with the remainder being single call-site guards.

**Static helpers become external.** Most of `rmodels.c`'s block is `static`. Moving it means non-static functions
with `rlpsp_`-prefixed names in `rlpsp_mesh.h`, and one owner for the globals (`rlpspCulling`, the mesh/bounds
tables). Keep the public names that samples already call (`rlpspEnableCulling`, `rlpspMergeFlush`) exactly as they
are — they are part of the link surface and `psp-nm` will catch a rename.

**rlgl stays header-only.** T2 is an `#include`, not a new translation unit. The block must stay where the current
one is in `rlgl.h` (before the GL headers) or the enums it defines will be missing; the existing comment there
already warns about this.

**Negative guards are the risky kind.** `rshapes.c`'s `#if !defined(PLATFORM_PSP)` removes raylib code rather than
adding ours. Two consequences: an upstream edit to that function conflicts even when we change nothing, and a
reviewer cannot see what PSP does *instead*. Collapsing them behind one switch fixes both.

**Which functions are actually affected** (checked 2026-11-28, merge branch): `DrawTriangleGradient` (326),
`DrawPoly` (1531), `DrawPolyLinesEx` (1616) replace raylib's quad paths, and `DrawRectanglePro` (622) adds a cull
path. **`DrawRectangleLines` is not one of ours**, so upstream's half-pixel fix (#6430) applies to PSP normally —
T3 must not chase a divergence that does not exist. What T3 does need is a check of which of these four functions
upstream has changed since the base, so any future conflict is expected rather than discovered.

**Move only.** No constant, threshold or call order changes in this sprint. If a move forces a behaviour decision,
stop and record it as a separate task — the acceptance for T5 is exact equality on the gates.

## 7. Gates

| gate | expected |
|---|---|
| `batch_check` / `basic_shapes` | 613 / 284, byte-identical captures |
| `scene_3d SCENE=both`, `MESH_SCENE`, RTT | 0 diff |
| `clip_probe` | 237/720 |
| benchmark pair | 33.5 / 33.8 ms ±2 %, `work` unchanged within tolerance |
| link surface | `psp-nm` shows the same `rlpsp*`/`pspgu*` symbols |
| audio | `audio_smoke` unchanged |

## 8. Exit criteria

- [ ] core-file PSP lines ≤200, measured by the script, per file, on the tree the move started from
- [ ] the baseline in T0 matches the tree T1 used (master vs merge branch differs by 9 lines in `rcore.c`)
- [ ] `psp-nm` diff empty for the port's public symbols
- [ ] gates byte-identical; pair within 2 %
- [ ] desktop build unaffected (`PLATFORM_PSP` absent ⇒ the moved files are not compiled)
- [ ] the next upstream merge touches our code only in `platforms/` (checked by a trial merge)
- [ ] strict docs build passes; hashes recorded

## 9. Result

Pending — no code moved yet.

## 10. Follow-ups

- After the move, a trial `git merge upstream/master` should show conflicts only in `platforms/`, which is the point.
- If `rlpsp_batch.h` grows past ~400 lines, it is a candidate for a real `.c` with an init hook, but only if the
  header-only cost shows up in build time.
- The line-count script belongs in `tools/` if it is going to be quoted again.

## 11. References

- `src/platforms/rcore_psp.c`, `raudio_psp.c`, `rlpsp_math.c` — the idiomatic part of the port
- `src/rmodels.c:1330-1960` (mesh/cull/merge), `src/rlgl.h:634+` (batcher), `src/rshapes.c:326,622,1531,1616`
  (negative guards), `src/raudio.c:2847+` (decoder thread)
- `docs/psp/recipes/backend-ab-pspgu-vs-pspgl.md` — the harness this sprint's acceptance uses
- upstream merge of 2026-11-26 (`upstream-11-26`, `ddbd5340`) — the conflict this sprint is meant to prevent
- port fixes `640ece10` / `21bbb3f4` — `PSP_THREAD_ATTR_VFPU` on the update and music-decoder threads
