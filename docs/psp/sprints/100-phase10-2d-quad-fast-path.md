# Sprint 100 (P10-S30) — The 2D quad fast path, without touching raylib core

| | |
|---|---|
| Roadmap phase | [Phase 10 — 3D and advanced](../phases/10-3d-advanced/index.md) |
| Sprint | P10-S30 |
| Branch / base commit | `master` @ `af9b76b3`; evidence: [099 §2 and §7](099-phase10-third-benchmark.md) |
| Started / closed | 2026-11-28 (written) |
| Build number(s) | driver unchanged (`b38`); the arm is `PSP_BATCH_FAST_QUAD=1` in `libraylib.a` |
| Tasks | 5/5 — adopted, default on (`RLPSP_FAST_QUAD=1`); T1 done: the 3.5 µs splits **1.4 raylib body / 2.2 seam staging**, and the instrument defect that hid it is fixed |

## 0. Handover

099 measured 7,000 small sprites at **45.4 ms** through the per-sprite API and **23.4 ms** for the same vertices in one
`glDrawArrays`: about **3.2 µs per sprite is the API surface**, not the GE, the fill or the texture. That is the number
this sprint exists to remove.

The important structural fact, checked against the tree: **the 2D submission path is already platform code.**
`rlgl.h:636` includes `platforms/rlpsp_batch.h` under PSP, and raylib's `DrawTexturePro` (`rtextures.c:4880`) does
nothing but call `rlSetTexture`, `rlBegin(RL_QUADS)`, one `rlColor4ub`, four `rlTexCoord2f` + `rlVertex2f` pairs,
`rlEnd` and `rlSetTexture(0)` — every one of which this project defines. So the cheap path can be built **entirely
outside raylib's core files**.

## 1. Objective

Cut the per-sprite API cost at least in half with **zero diff in raylib core files**, and keep the two-vertex sprite
primitive and the pixel result exactly as they are.

## 2. What is *not* the cost — checked before coding

The first version of this sprint assumed the seam did a pending-texture compare, a capacity check and a mode compare on
every one of the twelve calls. **Reading the seam refutes that** (`src/platforms/rlpsp_batch.h:120-200`):

- `rlSetTexture` is one store; `rlColor4ub` four byte stores; `rlTexCoord2f` two float stores; `rlEnd` empty.
- `rlBegin(RL_QUADS)` sets a flag and returns; the primitive is chosen on the fourth corner.
- The only work per quad is one `rlPspReserve(4)` and, on the fourth corner, a reserve of 2 plus a 2×24-byte copy.
- All of them are `static inline` in a header `rtextures.c` includes, so they are already in the caller.

So the 3.2 µs/sprite is **not** a function-call tax in our seam. Before touching anything, T1 has to say where it is,
and the candidates are distinguishable **without a core edit**:

| candidate | how it is separated |
|---|---|
| the `PSP_CULL` block's `rlGetActiveFramebuffer()` call per sprite (`rtextures.c:4897`) | rebuild the library with `PSP_CULL=0` — an existing build variable, no source change |
| raylib's own per-sprite body (call, corner maths, 4 attribute stores, 4 pushes) | a new benchmark arm that emits the same quad through the seam from the demo — if it matches the prebuilt arm, the body is free |
| the seam's staging (4 corners then 2 vertices) | compare that arm against the prebuilt array at the same vertex count |
| the GE/driver, per draw | the prebuilt arm already has one draw of 14,000 vertices; if the seam arm has the same vertex count and the same draw count, the GE is constant |

Nothing in that table needs a raylib core edit. The sprint proceeds on the answer, not on the assumption.

## 3. Design — the lever follows the attribution

**The core-diff budget, decided up front:** step 1 may change only `platforms/rlpsp_batch.h` (and the benchmark).
A raylib core file may gain **one guarded call site, never a parallel implementation**, and only in step 2.

**If T1 blames the `PSP_CULL` block (measured: 0.24 µs of 3.2 — worth caching, not worth a sprint on its own):**
the cheapest fix is in the driver/`rlgl.h` boundary, not in `DrawTexturePro`: cache the active framebuffer in a variable
the seam reads (it changes only on `BeginTextureMode`), so the per-sprite test is a field read. `rlgl.h` is already this
project's file (it holds the seam include at `:636`), so that is not a fork of upstream logic.

**If T1 blames raylib's body:** step 2 — one guarded `rlpspDrawQuad(texture, src, dst, tint)` in `DrawTexturePro`,
about eight lines under `#if defined(PLATFORM_PSP)`, replacing the twelve-call sequence with one call.

**If T1 blames the seam's staging:** the quad can write its two sprite vertices at the first and third corner
(raylib emits topLeft, bottomLeft, bottomRight, topRight, so the pair is 1 and 3 — the same two vertices the current
assembly copies at the fourth corner), skipping two of the four 24-byte stores.

**If T1 finds nothing** — all arms within noise — the sprint stops and 099's 3.3 µs conclusion is corrected instead of
pursued.

## 4. Scope

**In:** the seam, the drawn-quad shape, a build arm, the 7,000-sprite measurement, the pixel gates, an attribution that
says where the cycles went.

**Out:** the GE path, the merge path (3D), texture formats, list-local memory (099 §7.1 closed it), and any change to
`DrawMesh`/3D.

## 5. Tasks

| # | Task | Status | Acceptance | Result |
|---|---|---|---|---|
| T1 | **Attribute, without a core edit.** Three arms at 7,000 sprites: `PSP_CULL=0` build; a benchmark arm that emits the quad through the seam; the existing prebuilt arm. Two runs each | `done` | the 3.2 µs is allocated to one of the four candidates with numbers, or declared unattributable and the sprint stops | At **-O2** (the samples were building at -O0, see §T1): raylib `DrawTexturePro` **40.83 ms**, seam sequence from the demo **31.19**, prebuilt one-draw **16.04**. The 3.5 µs/sprite splits **1.4 µs raylib body** (including 0.24 cull) **+ 2.2 µs seam staging** |
| T2 | **Take the lever T1 named** | `done` | the change is inside the stated budget; the arm it targets is named | **One guarded call site** in `DrawTexturePro` + `rlpspDrawQuad` in the seam. It removes both measured parts at once: the body (1.4 µs) and the staging (2.2 µs), because the four corners are already in hand |
| T3 | **Measure.** 7,000 sprites, two runs; the 2D showcase | `done` | **≤24 ms** at 7,000 from 40.8 | **28.94 / 28.93 ms** at 7,000 (from 40.83) and the frame drops from 3 periods to 2, i.e. **20 → 30 fps**. The ≤24 target was **missed**; the residual is 1.8 µs/sprite of call+reserve, not body or staging. 2D demo control: **25.16 → 15.09 ms** work, 29.8 → 33.9 fps |
| T4 | **Gates.** `batch_check` 613, `basic_shapes` 284, checksum, one benchmark capture | `done` | exact | **613 / 284 / `86b1e3c2` `c4b13eb4` `670d502c`**, and again 613 after the restore |
| T5 | **Close.** Adopt or drop; recipe/skill updated | `done` | explicit state | **Adopted**, default `RLPSP_FAST_QUAD=1`, `PSP_BATCH_FAST_QUAD=0` restores the control. Core diff: **one** guarded block, 12 lines, in `DrawTexturePro` |

## 6. Gates

| gate | expected |
|---|---|
| `batch_check` / `basic_shapes` | 613 / 284 |
| checksum 60/120/240 | `86b1e3c2` / `c4b13eb4` / `670d502c` |
| 2D demo | same work as today within noise (it is fill-bound, 099 showed fill is not this sprint's lever) |
| draw counters | 7,000 sprites still ~5 draws per frame and 2 texture binds, as 099 measured |

## 7. Exit criteria

- [ ] the 3.2 µs is attributed to one candidate, with the arms that show it
- [ ] per-sprite cost at 7,000 stated before and after, two runs each
- [ ] 0 pixel difference on every gate
- [ ] the core diff is zero, or exactly one guarded call site with its line count stated
- [ ] the 2D demo is unchanged, and that is recorded rather than hidden
- [ ] lane unlocked; docs strict

## 8. Follow-ups

- If the seam reaches ~23 ms, the remaining 3.3 µs/sprite is the GE/driver per-primitive cost and 099's conclusion holds:
  the next lever is not this batcher.
- The same hoisting applies to `DrawRectangle*` and text glyph quads, which share the seam — the fit is not sprite-only.

### T1, measured — and the instrument defect that hid the answer

The first attempt to split raylib's body from the seam produced an arm **slower** than raylib's own call (79.9 vs 45.4
ms), which cannot discriminate anything. The cause was not the seam: **the sample harness compiled app code at `-O0`**
(`psp-tests/samples/Makefile.inc` had no `-O` flag at all) while `libraylib.a` is `-O2`. The demo's copy of the sequence
was unoptimised against an optimised library.

Fixed (`-O2` in `Makefile.inc`, with the reason in the comment), then the three arms at 7,000 sprites, two runs each:

| arm | work | per sprite |
|---|---|---|
| raylib `DrawTexturePro` (library, `-O2`) | **40.83 ms** (40.828 / 40.848) | 5.8 µs |
| the same seam sequence, emitted by the demo | **31.19 ms** (31.189 / 31.186) | 4.5 µs |
| prebuilt vertices, one `glDrawArrays` | **16.04 ms** (16.041 / 16.044) | 2.3 µs |

So the 3.5 µs per sprite decomposes:

- **1.4 µs — raylib's per-sprite body**: call, source/dest handling, corner maths, and the 0.24 µs cull query.
- **2.2 µs — the seam's quad staging**: four corner stores, two reserve checks, the 2×24-byte copy to the sprite pair,
  and the per-call branches. This is step 1's target, entirely inside `platforms/rlpsp_batch.h`.
- **2.3 µs — the floor** with the vertices prebuilt, i.e. the driver/GE per primitives as they are today.

Both levers are now quantified, and they add to the prebuilt floor: seam staging (~2.2 µs) is step 1 without any core
change; raylib's body (~1.4 µs) is step 2's single guarded call site. Together they are the whole 3.5 µs.

**Consequence for every earlier sample number:** the pair and the 2D demo were measured with `-O0` app code until this
commit. Ceilings are unaffected (`matrix_probe` builds its own `-O2`), but `psp_showcase`/`psp_2d_showcase` numbers from
before today include unoptimised demo loops and should be re-taken before they are compared with anything new.

## 8. Result — adopted, with the target missed and stated

| arm (all `-O2`) | work | presented |
|---|---|---|
| 7,000 sprites, twelve-call path | 40.83 ms | 50.05 (3 periods) |
| 7,000 sprites, fast quad | **28.94 ms** | **33.37 (2 periods)** |
| 2D demo, control (`PSP_BATCH_FAST_QUAD=0`) | 25.16 ms | 33.58 (29.8 fps) |
| 2D demo, fast quad | **15.09 ms** | **29.46 (33.9 fps)** |

- **The shipped demo improves 40 % in work and crosses a vblank**: 33.6 → 29.5 ms, 29.8 → 33.9 fps, measured against a
  control built from the same library with the feature off.
- At 7,000 sprites the frame drops from three periods to two (20 → 30 fps); the sprint's ≤24 ms target was **not** met.
- The residual 1.8 µs/sprite is the per-sprite call plus `rlPspReserve` (texture settle + capacity + pointer), not the
  body or the staging this sprint removed. It is the next lever, and it is inside the seam.

The core diff is exactly what the budget allowed: one `#if defined(PLATFORM_PSP) && defined(RLPSP_BATCH_ENABLED) &&
(RLPSP_FAST_QUAD == 1)` block of 12 lines in `DrawTexturePro`, calling one seam function.

### 8.1 The residual, attacked and bounded

`DrawTexturePro` divided `srcrec.x`, `srcrec.y` and both edges by the texture size — four float divisions per sprite,
the same four PSPRuntime's `DrawTextureSpriteRecEx` does and that its `-ffast-math` folds into one reciprocal. Moving
one reciprocal into the seam (pixels in, UVs out) changed 7,000 sprites from **28.94 to 28.71 ms work** — 0.8 %, not
the 1.8 µs. So the remaining cost is the **call and argument setup of a non-inlined raylib API invoked 7,000 times**,
not arithmetic, not the body's maths, and not our seam's stores. That is a shape limit of the API: the only way past it
is fewer calls per sprite, which is exactly what the prebuilt arm (16.0 ms) does.

The reciprocal stays (it is free and matches what a fast-math build would emit), and the 1.8 µs is recorded as
unreachable from inside this seam.

## 9. References

- [099 — the third benchmark](099-phase10-third-benchmark.md) §2 (45.4 vs 23.4 ms), §7.1 (list-local bought nothing)
- `/home/psp/ref/PSPRuntime/optimal-rendering-path.md` — the reference path (2 vertices, 1,024 per flush,
  `sceGuGetMemory`); a local checkout, not a doc link
- `src/platforms/rlpsp_batch.h` — the seam; `src/rlgl.h:636` — where it is included
- `src/rtextures.c:4880` — `DrawTexturePro`, the exact call sequence
