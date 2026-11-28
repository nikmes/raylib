# Sprint 100 (P10-S30) — The 2D quad fast path, without touching raylib core

| | |
|---|---|
| Roadmap phase | [Phase 10 — 3D and advanced](../phases/10-3d-advanced/index.md) |
| Sprint | P10-S30 |
| Branch / base commit | `master` @ `af9b76b3`; evidence: [099 §2 and §7](099-phase10-third-benchmark.md) |
| Started / closed | 2026-11-28 (written) |
| Build number(s) | driver unchanged (`b38`); the arm is `PSP_BATCH_FAST_QUAD=1` in `libraylib.a` |
| Tasks | 0/7 — attribution first; the seam-turn is written below only after T1 decides it |

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
| T1 | **Attribute, without a core edit.** Three arms at 7,000 sprites: `PSP_CULL=0` build; a benchmark arm that emits the quad through the seam; the existing prebuilt arm. Two runs each | `doing` | the 3.2 µs is allocated to one of the four candidates with numbers, or declared unattributable and the sprint stops | **cull block: 45.4 → 43.7 ms work (two runs, 43.686/43.689)**, i.e. **0.24 of the 3.2 µs per sprite** is the per-sprite `rlGetActiveFramebuffer()` test — not the cost. The seam arm (MODE 5) came back **slower**, 79.9 ms, so it is not a valid discriminator as built; see §T1 (partial) |
| T2 | **Take the lever T1 named** — `rlgl.h` framebuffer cache, or the guarded `rlpspDrawQuad`, or the two-vertex staging change | `todo` | the change is inside the stated budget; the arm it targets is named | — |
| T3 | **Measure.** 7,000 sprites, two runs; the 2D showcase unchanged (it is fill-bound) | `todo` | **≤32 ms** at 7,000 from 45.4, or dropped with the number | — |
| T4 | **Gates.** `batch_check` 613, `basic_shapes` 284, checksum, one benchmark capture | `todo` | exact | — |
| T5 | **Close.** Adopt or drop; recipe/skill updated; 099's attribution corrected if needed | `todo` | explicit state | — |

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

### T1 (partial), measured

| arm | work at 7,000 | per sprite |
|---|---|---|
| shipped (`PSP_CULL=1`) | 45.4 ms | 6.5 µs |
| `PSP_CULL=0` (build variable only) | **43.7 ms** (43.686 / 43.689) | 6.2 µs |

The per-sprite `rlGetActiveFramebuffer()` test costs 1.7 ms per frame — real, but 3.8 % of the frame, so it is not the
3.2 µs. Caching it remains a cheap separate win for any 2D scene with many small quads, and it needs no core behaviour
change (the value only moves on `BeginTextureMode`).

What that leaves is raylib's own per-sprite body versus the seam's staging. The obvious discriminator — issue the same
quad through the seam from the demo (MODE 5) — **failed as an instrument**: 79.9 ms against 45.4 for raylib's own call,
with the same draws, binds and vertex count. A slower arm cannot separate "raylib costs X" from "the seam costs Y"; the
likely cause is code generation in the demo's single large loop (four inlined vertex expansions, external `rlpsp`), not
the library's path. The arm needs a `noinline` helper to be comparable, or the per-sprite cost needs a call-level
counter inside `DrawTexturePro` using the existing probe scaffolding.

Until one of those runs, this sprint is **not** implementation-ready and the guarded core call site is not taken on an
assumption.

## 9. References

- [099 — the third benchmark](099-phase10-third-benchmark.md) §2 (45.4 vs 23.4 ms), §7.1 (list-local bought nothing)
- `/home/psp/ref/PSPRuntime/optimal-rendering-path.md` — the reference path (2 vertices, 1,024 per flush,
  `sceGuGetMemory`); a local checkout, not a doc link
- `src/platforms/rlpsp_batch.h` — the seam; `src/rlgl.h:636` — where it is included
- `src/rtextures.c:4880` — `DrawTexturePro`, the exact call sequence
