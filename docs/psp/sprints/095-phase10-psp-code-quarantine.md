# Sprint 095 (P10-S25) — Keeping the port mergeable: quarantine PSP code out of core raylib files

| | |
|---|---|
| Roadmap phase | [Phase 10 — 3D and advanced](../phases/10-3d-advanced/index.md) (port maintenance spanning phases 04, 05, 07, 09) |
| Sprint | P10-S25 |
| Branch / base commit | `master` @ `21bbb3f4`; the `upstream-11-26` merge is written but **unlanded** (device gates pending) — rebase onto it before T1 if it lands first |
| Started / closed | 2026-11-26 (written) / 2026-11-28 (closed) |
| Build number(s) | driver unchanged; `libraylib.a` hash changes (files moved), measurements must not |
| Tasks | 3/4 closed — T1 and T2 done, T3 dropped as a move, T4 deferred with its blocker (§9.3) |

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

**Reduce the number of files that upstream edits and we have modified** — not the line count. That is the property
that decides what a merge costs, and this sprint has already shown the two are not the same: a rename can halve the
count and change nothing (T3), while moving one contiguous block removes a file's worth of conflict surface (T2).

Measured by: which files appear in a trial `git merge upstream/master` as conflicts, the per-file PSP-guarded line
count, identical symbols, and identical gates/pair. The line count is evidence, not the goal.

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
| T1 | **Mesh path → `src/platforms/rlpsp_mesh.c/.h`.** Move `PspGeMesh`, the bounds table, `pspGeClassifyMesh` family and the merge helpers out of `rmodels.c`; leave guarded call sites in `DrawMesh`/`UnloadMesh`/`rlpspMergeFlush` | `done` | `rmodels.c` PSP lines ≤ 40; symbols exported through the new header; no double definition | 2026-11-28: **744 lines** moved to `platforms/rlpsp_mesh.c` with eleven entry points in `rlpsp_mesh.h`; `rmodels.c` keeps **27** PSP-marked lines (call sites only). The probe counters and their scaffolding moved too (the macros `PSP_CALL_BEGIN`/`PSP_CALL_END`, `PSP_MTX_*` and `PSP_DRIVER_*` live in the header), so the 40-line target survived the probes. Both the shipped build and `PSP_CALL_PROBE=1 PSP_MATRIX_PROBE=1` compile — the instrumented paths did not rot. Gates on hardware after the move: `batch_check` **613**, `basic_shapes` **284**, checksum `86b1e3c2 / c4b13eb4 / 670d502c` (identical to before), `MERGEPROBE runs=482` over 241 frames. §9.4 |
| T2 | **Batcher → `src/platforms/rlpsp_batch.h`.** Move the 402 `rlgl.h` lines behind one `#include` under one guard, preserving include order (the block runs before the GL headers) | `done` | `rlgl.h` PSP lines ≤ 10; header-only semantics intact; desktop build unchanged | done — 231 lines moved to `platforms/rlpsp_batch.h`; rlgl.h 402 → 173 lines; gates 613 / 2D 33.801 |
| T3 | **Shapes → one switch.** *Dropped as a move (§9.2).* Renaming the negative guards to one macro cuts the count and not the conflict surface; a real fix is function-level variants selected in one place, which is a refactor. Filed to §10 | `dropped` | — | §9.2 |
| T4 | **Audio decoder → `raudio_psp.c`.** *Deferred (§9.3).* `raudio.c:185` includes the backend before the decoder type at `:187`, so the thread cannot move alone; the type, its create/destroy helpers and the include position move together | `deferred` | `raudio.c` PSP lines ≤ 20; audio smoke unchanged | §9.3 |
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

- [ ] target after T1: **total ≤700** (1,397 − rmodels' 763 + ~40 call-site lines). **≤200 is not reachable by moves** — it needs the T3 hook indirection filed to §10
- [ ] the baseline in T0 matches the tree T1 used (master vs merge branch differs by 9 lines in `rcore.c`)
- [ ] `psp-nm` diff empty for the port's public symbols
- [ ] gates byte-identical; pair within 2 %
- [ ] desktop build unaffected (`PLATFORM_PSP` absent ⇒ the moved files are not compiled)
- [ ] the next upstream merge touches our code only in `platforms/` (checked by a trial merge)
- [ ] strict docs build passes; hashes recorded

## 9. Result

### 9.1 T2 — the batcher (`done`)

231 lines of batch state and machinery moved verbatim to `src/platforms/rlpsp_batch.h`, included at
the same point under the same condition. `rlgl.h` went **402 → 173** PSP lines (file 6,008 → 5,779).
Gates: `batch_check` **613**, 2D showcase **33.801** against a 33.800 baseline.

**Scope finding.** T2's "≤10 lines" target is not reachable under the move-only rule: of the original
402 lines, only 232 were a self-contained block. The remaining ~170 are call-site guards *inside*
`rlDrawVertexArray*`, `rlBegin`/`rlEnd`, and the negative `!PLATFORM_PSP` exclusions. Moving those needs no-op
hook functions that the rest of rlgl calls unconditionally — a shape change, not a move. The sprint's own rule
applies, so T2 is recorded as **173 lines and one `#include`**, and the hook indirection is filed as a design
task rather than smuggled into a move.

### 9.2 T3 — the shape guards: reframed, not done

The four `rshapes.c` guards (129 lines) are **negative**: `#if !defined(PLATFORM_PSP)` removes raylib's quad
paths. Renaming the condition to one `PSP_SHAPES_FAST` macro would reduce the *count* without reducing the
*conflict surface* — an upstream edit to `DrawPoly` still lands in the middle of a function we have gutted.
A real fix is one function-level variant per shape selected at a single point, which is a refactor, not a move.
Recorded so the metric is not gamed.

### 9.3 T4 — the audio decoder: blocked by include order

`raudio.c:185` includes `platforms/raudio_psp.c` **before** `typedef struct PspMusicDecoder` at `:187`, so the
decoder thread (`:2849`) cannot simply move into the backend: it uses the struct's fields. Moving it means
moving the type and its create/destroy helpers and relocating the include — a change with an initialisation
order to preserve, not a cut-and-paste.

### 9.4 T1 — the mesh path: not attempted

754 lines (763 after sprint 090's hint helper) of mesh cache, bounds, classifier and merge machinery, with
cross-references into `DrawMesh`. It remains the largest single win for mergeability and needs its own session:
extract to `platforms/rlpsp_mesh.c` with a small header, leave ≤40 lines of call sites, then run the full table.

### 9.5 Where the count stands, and the revised goal

**1,617 → 1,397** (−220 net: rlgl −229, rmodels +9 from sprint 090's clean-hint helper). With T1 done the total
lands near **674**; that is the realigned target (**≤700**). The original ≤200 was reachable only by renaming
guards (T3), which changes no conflict, so it is withdrawn rather than pursued.

The realigned sprint therefore has **one code move left (T1)** plus the close, and two design tasks parked in §10.

## 10. Follow-ups

- **Shape variants (from T3).** Replace the four gutted `rshapes.c` functions with function-level PSP variants
  selected in one place. That is a refactor with pixel gates, and it is the only way to make those four functions
  mergeable.
- **Audio move (from T4).** Move `PspMusicDecoder`, its create/destroy helpers and the `raudio_psp.c` include
  together, preserving initialisation order.
- **Re-run the 3D pair.** Not measured since 090/095 (2D is identical at 33.801 and the clean-hint arm is off by
  default, but the 3D number after these commits is unverified).
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

### 9.4 T1 — the move, and how it was checked (2026-11-28)

`rmodels.c` 7,640 lines, of which 27 carry a PSP marker; `platforms/rlpsp_mesh.c` holds 744 moved lines plus the
probe block. The header declares the whole surface, including the counters and the five call-site macros, so the
probe builds (`PSP_CALL_PROBE`, `PSP_MATRIX_PROBE`) still compile — a move that left them broken would have been
discovered only by the next measurement session.

Why the probe scaffolding moved as well: at 57 PSP-marked lines the move had met its intent but not its number. The
macros are the difference between a call site and an implementation, so they belong on the implementation side.

The check is the same gates the sprint required, on the device, after the move:

| gate | before | after |
|---|---|---|
| `batch_check` | 613 | **613** |
| `basic_shapes` | 284 | **284** |
| checksum 60/120/240 | `86b1e3c2` / `c4b13eb4` / `670d502c` | **identical** |
| `MERGEPROBE` | 2 runs/frame | 482 runs over 241 frames |

A move that changed a pixel or stopped the merge would show in either instrument; neither moved. The library hash
changes, as the sprint predicted — and, per 096 §9.7, that hash identifies an install rather than a content.
