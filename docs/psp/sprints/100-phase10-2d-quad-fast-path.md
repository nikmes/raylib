# Sprint 100 (P10-S30) — The 2D quad fast path, without touching raylib core

| | |
|---|---|
| Roadmap phase | [Phase 10 — 3D and advanced](../phases/10-3d-advanced/index.md) |
| Sprint | P10-S30 |
| Branch / base commit | `master` @ `af9b76b3`; evidence: [099 §2 and §7](099-phase10-third-benchmark.md) |
| Started / closed | 2026-11-28 (written) |
| Build number(s) | driver unchanged (`b38`); the arm is `PSP_BATCH_FAST_QUAD=1` in `libraylib.a` |
| Tasks | 0/7 |

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

## 2. Why the call surface costs what it does

Per sprite, `DrawTexturePro` issues about **twelve calls** into our seam: 2 texture binds, begin, one colour, eight
texture/vertex calls, end. At 222 MHz, 6.4 µs is ~1,400 cycles for twelve calls plus raylib's corner maths — i.e. it is
**call and check overhead**, not arithmetic. The seam currently does, on every one of those calls: a pending-texture
compare, a capacity check, a mode compare, and — for vertices — a copy through a 4-corner staging array before the quad
is converted to two sprite vertices.

There is also a per-sprite block in `DrawTexturePro` guarded by `PLATFORM_PSP && PSP_CULL` that asks
`rlGetActiveFramebuffer()`; T1 measures whether that is a driver call per sprite.

## 3. Design — two steps, with a stated core-diff budget

**Step 1 (T1–T4): all inside the seam. Core diff: zero.**
`platforms/rlpsp_batch.h` is edited only. The shape:

- **Hoist every per-call check to the quad boundary.** `rlSetTexture` and `rlBegin` compare and set state once;
  the four vertex calls become stores with no compares.
- **Write the two sprite vertices directly.** For an axis-aligned quad the seam already reaches `GU_SPRITES`; the cost
  is staging 4 vertices first. The quad can be written as: first corner → sprite vertex 0, third corner → sprite
  vertex 1, corners 2 and 4 skipped when they are the rectangles' other corners (they are, by construction of
  `DrawTexturePro`). Fall back to the staging array whenever the shape is not that exact sequence.
- **Keep the texture unbind free.** `rlSetTexture(0)` at the end of every sprite must not end the run; it sets a
  pending id that the next `rlSetTexture` overrides without a flush.
- **Make the seam functions force-inline** (`static inline` with no branch on cold paths) so the 12 calls collapse into
  the caller.

Target: **45.4 → ≤32 ms** at 7,000 sprites, 0 pixel difference.

**Step 2 (T5), only if step 1 plateaus: at most one guarded block in core.**
If the seam cannot go further because the sequence itself is the cost, add `rlpspDrawQuad(texture, src, dst, tint)` to
the header and call it from `DrawTexturePro` behind `#if defined(PLATFORM_PSP)` — **one call per quad**, about eight
lines, in one place, next to the existing `PLATFORM_PSP` block in that function. That is the entire permitted core
diff for this sprint, and it is taken only with a measurement showing step 1 stalled.

The rule, written down so it survives: *new behaviour for a raylib API on PSP goes behind the `rl*` seam; a core file may
gain one guarded call site, never a parallel implementation.*

## 4. Scope

**In:** the seam, the drawn-quad shape, a build arm, the 7,000-sprite measurement, the pixel gates, an attribution that
says where the cycles went.

**Out:** the GE path, the merge path (3D), texture formats, list-local memory (099 §7.1 closed it), and any change to
`DrawMesh`/3D.

## 5. Tasks

| # | Task | Status | Acceptance | Result |
|---|---|---|---|---|
| T1 | **Attribute the 12 calls.** Per-line counters at 7,000 sprites: `rlSetTexture` ×2, `rlBegin`/`rlEnd`, colour, texcoord, vertex, and the `PSP_CULL`/`rlGetActiveFramebuffer` block | `todo` | the per-sprite total is allocated to lines within ±10 % | — |
| T2 | **Hoist the checks.** Texture compare and capacity check once per quad; per-call stores have no compares | `todo` | per-sprite time falls; counters prove no extra flushes | — |
| T3 | **Direct two-vertex emission.** First and third corner write the sprite pair; other shapes fall back | `todo` | pixel-identical to the staging path on the 2D showcase and `batch_check` | — |
| T4 | **Measure step 1.** 7,000 sprites, two runs; and the 2D showcase to prove a fill-bound scene is untouched | `todo` | **≤32 ms** at 7,000, or step 2 is taken | — |
| T5 | **Step 2, only on a measurement.** One guarded `rlpspDrawQuad` call in `DrawTexturePro` | `todo` | the diff is ≤10 lines, all inside one `#if defined(PLATFORM_PSP)` | — |
| T6 | **Gates.** `batch_check` 613, `basic_shapes` 284, checksum, 2D demo capture | `todo` | exact | — |
| T7 | **Close.** Adopt or drop with the number; recipe/skill updated | `todo` | explicit state | — |

## 6. Gates

| gate | expected |
|---|---|
| `batch_check` / `basic_shapes` | 613 / 284 |
| checksum 60/120/240 | `86b1e3c2` / `c4b13eb4` / `670d502c` |
| 2D demo | same work as today within noise (it is fill-bound, 099 showed fill is not this sprint's lever) |
| draw counters | 7,000 sprites still 5–7 draws per frame, one bind per run |

## 7. Exit criteria

- [ ] per-sprite cost at 7,000 stated before and after, two runs each
- [ ] 0 pixel difference on every gate
- [ ] the core diff is zero, or exactly one guarded call site with its line count stated
- [ ] the 2D demo is unchanged, and that is recorded rather than hidden
- [ ] lane unlocked; docs strict

## 8. Follow-ups

- If the seam reaches ~23 ms, the remaining 3.3 µs/sprite is the GE/driver per-primitive cost and 099's conclusion holds:
  the next lever is not this batcher.
- The same hoisting applies to `DrawRectangle*` and text glyph quads, which share the seam — the fit is not sprite-only.

## 9. References

- [099 — the third benchmark](099-phase10-third-benchmark.md) §2 (45.4 vs 23.4 ms), §7.1 (list-local bought nothing)
- `/home/psp/ref/PSPRuntime/optimal-rendering-path.md` — the reference path (2 vertices, 1,024 per flush,
  `sceGuGetMemory`); a local checkout, not a doc link
- `src/platforms/rlpsp_batch.h` — the seam; `src/rlgl.h:636` — where it is included
- `src/rtextures.c:4880` — `DrawTexturePro`, the exact call sequence
