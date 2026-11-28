# Sprint 099 (P10-S29) — The third benchmark: 7,000 sprites, and where our 2D goes

| | |
|---|---|
| Roadmap phase | [Phase 10 — 3D and advanced](../phases/10-3d-advanced/index.md) |
| Sprint | P10-S29 |
| Branch / base commit | `master` @ `c6329c59`; reference: `/home/psp/ref/PSPRuntime/optimal-rendering-path.md` (7,000 bunnies, 59 fps, PSP Street) |
| Started / closed | 2026-11-28 (written) / 2026-11-28 (measured, open for its next lever) |
| Build number(s) | pspgu `b38`, `libraylib.a` with `PSP_MERGE_DIRECT=1` |
| Tasks | 3/6 — the benchmark, the four arms and the attribution are done; the list-local arm, the gates and the close remain |

## 0. Handover

The 2D demo we ship is an additive nebula of ~2,300 large glow sprites and reads 33.8 ms; it cannot say whether the
cost is the primitive, the fill, the texture or our batcher. PSPRuntime's document reports 7,000 small textured sprites
at 59 fps on a PSP Street with one two-vertex `GU_SPRITES` pair per bunny, and 30 fps for the same count as triangles.
This sprint builds the matched scene — small quads, one texture, nothing else — so the count is the only variable.

**The measured answer: our 2D is per-sprite bound, not fill bound, and 55 % of the per-sprite cost is raylib's own
call surface.** The remaining 3.3 µs/sprite is above PSPRuntime's ≤2.4 µs, and the next lever for it is list-local
vertices, the arm 085 T6a dropped for a 3D reason.

## 1. The benchmark

`psp-tests/samples/psp/psp_2d_bunnymark` (the third benchmark, next to `psp_showcase` and `psp_2d_showcase`):

| build | what it submits |
|---|---|
| `MODE=1` (default) | `DrawTexturePro`, rotation 0 → the batcher's `GU_SPRITES` path, 2 vertices per bunny |
| `MODE=3` | `DrawTexturePro`, rotation 0.001° → corners stop being axis-aligned, 6 vertices per bunny |
| `MODE=0` | `DrawRectanglePro` → untextured quads, same geometry |
| `MODE=2` | vertices built once per frame in one array, **one `glDrawArrays`**, no raylib call per sprite |
| `SCALE=` | sprite size in pixels (fill control) |
| `COUNT=`, `FRAMES=` | sprite count, frames; movement is frame-indexed so frame N is identical on every build |

It prints `STAT` (min/median/p95/worst/avg), `SPLIT` (work/swap) and the wrap counters, so the same ceiling protocol
as 096 §9.9 applies: a point holds when the median is at the period and p95 does not drop.

## 2. Measured — 7,000 bunnies, 32×32, PSP-1000 lane a, two runs per point

| arm | verts/frame | work | presented | per sprite |
|---|---|---|---|---|
| `MODE=1` textured sprites | 13,067 | **45.4 ms** | 50.05 (3 periods) | 6.5 µs |
| `MODE=1` **SCALE=16** (¼ fill) | 13,065 | **47.6 ms** | 50.05 | 6.8 µs |
| `MODE=0` untextured | 13,067 | **47.4 ms** | 50.05 | 6.8 µs |
| `MODE=3` triangles (6 verts) | 42,042 | **61.6 ms** | 66.97 (4 periods) | 8.8 µs |
| `MODE=2` prebuilt, one draw | 14,000 | **23.4 ms** | 33.37 (2 periods) | 3.3 µs |
| PSPRuntime, sprites (their text) | 14,000 | — | 16.97 (1 period) | ≤2.4 µs |

Three facts fall out:

1. **Fill is not the limit.** A quarter of the pixels (16×16) changes nothing: 47.6 vs 45.4 ms. Whatever costs 45 ms is
   paid per sprite, not per pixel.
2. **Texture is not the limit.** Untextured is the same speed as textured (47.4 vs 45.4), which matches PSPRuntime's
   own mode 0 = mode 1 result from the other direction.
3. **The raylib call surface is half the cost.** The same 14,000 vertices, built into one array and submitted with a
   single `glDrawArrays`, take **23.4 ms instead of 45.4** — 3.2 µs per sprite saved by not calling `DrawTexturePro`
   seven thousand times.

What remains after that, **3.3 µs per sprite**, still sits above PSPRuntime's ≤2.4 µs, and their path differs in one
specific way: their vertices go into `sceGuGetMemory` inside the display list and the GE reads them from there, while
our arrays path copies 336 KiB per frame into the driver arena first.

## 3. The datatypes and alignment (asked while this ran)

The vertex types are not the difference on the textured path; the alignment discipline is.

| | PSPRuntime `TVertex` | raylib `RLPspVertex` |
|---|---|---|
| layout | `float u,v; u16 colour; float x,y,z;` | `float u,v; u8 r,g,b,a; float x,y,z;` |
| size | 22 B → **24 B** | **24 B** |
| colour | 5551 in the 4-byte GU slot | 8888 in the same slot |
| GU flags | `VERTEX_32BITF + TEXTURE_32BITF + COLOR_5551` | the same widths with `COLOR_8888` |
| transform | none for sprites (screen space) | none for 2D |

Where alignment matters on this hardware, in the order it bites:

- **The display list must be 16-byte aligned** (`sceGuStart`), which is why both our lists are `memalign(64)`.
- **Memory the GE reads by pointer should be at least 16-byte, and ideally cache-line, aligned.** Our pinned merge
  buffers are `aligned(64)` for exactly this (098), and the driver arena hands out 16-byte blocks. PSPRuntime pads its
  untextured `Vertex`/`Vertex2D` to 16 bytes with `__attribute__((aligned(16)))`, trading 25 % more vertex bytes for a
  16-byte stride; their `TVertex` is not padded, at 24.
- **A 24-byte stride means every second vertex straddles a 32-byte cache line.** At 7,000 sprites that is 7,000 split
  lines per frame for the CPU writes, and for any copy. Padding to 32 would align every vertex, but the GU vertex
  format has no padding field: our driver rejects a stride that does not match the format, so the only way to a
  32-byte stride is a padded vertex *format* (wasting 8 bytes of GE bandwidth per vertex). Not done here; the
  measurement says vertex bandwidth is not the wall at this count, so padding would buy alignment at a cost with no
  demonstrated benefit.
- **Our arrays path copies into a 16-aligned arena**, so the alignment of raylib's array only affects the copy's
  source. That is why `MODE=2` carries an `ALIGN=` knob: it can be 4, 16 or 64 without changing the GE's view.

## 4. Tasks

| # | Task | Status | Acceptance | Result |
|---|---|---|---|---|
| T1 | **The benchmark.** `psp_2d_bunnymark` with four arms, deterministic frame-indexed scene, STAT/SPLIT | `done` | same frame on every build; arms differ only in submission | built; arms 0/1/2/3 |
| T2 | **Harness.** `tools/psp-bench.sh --samples` so the third benchmark can be run and recorded like the pair | `done` | default unchanged; a JSON row per run | `--samples psp_2d_bunnymark` |
| T3 | **The four arms at 7,000**, two runs each, plus the fill control | `done` | work and presented per arm | §2 |
| T4 | **Attribute the remaining 3.3 µs.** The list-local arm: build the 2D batch with `sceGuGetMemory` in the display list instead of the arena, as PSPRuntime does; measure the 60 fps count | `todo` | the arm moves the count; 085 T6a's 2D win re-measured with the current driver | — |
| T5 | **Gates.** `batch_check` 613, `basic_shapes` 284, checksum, and a capture of the benchmark itself | `todo` | exact, and the HUD readable in the capture | — |
| T6 | **Close.** Adopt or drop the list-local arm; update the recipe and this sprint | `todo` | explicit state | — |

## 5. Gates

| gate | expected |
|---|---|
| `batch_check` / `basic_shapes` | 613 / 284 |
| checksum 60/120/240 | `86b1e3c2` / `c4b13eb4` / `670d502c` |
| benchmark capture | HUD text readable; the four modes visually identical apart from the HUD |
| counters | `MODE=2` shows one `glDrawArrays` per frame, `MODE=1` five |

## 6. Exit criteria

- [ ] the list-local arm measured at 7,000, with the 60 fps count per arm
- [ ] the FPS gain, if any, stated as a count and not as an average
- [ ] gates exact
- [ ] lane unlocked, driver and library left in the adopted state
- [ ] docs strict

## 7. Result (partial — see §2)

The third benchmark exists and its first four arms are measured. The actionable finding is quantitative: **3.2 of the
6.5 µs per sprite is raylib's per-sprite call surface** (removable only by batching at a different layer), and the
remaining **3.3 µs is submission/GE with the arena copy still in the path** — the arm T4 measures.

## 8. Follow-ups

- T4 is the only remaining lever with a stated mechanism; if it does not move the count, our 2D ceiling stands at the
  measured per-sprite cost and the next step is a different batcher, not a different primitive.
- PSPRuntime's numbers were taken on the PSP Street lane; ours on the PSP-1000 lane. The comparison is per-sprite cost
  on the same geometry, not an fps comparison across models.

## 9. References

- `/home/psp/ref/PSPRuntime/optimal-rendering-path.md` — the 7,000-bunny result, its method and its caveats
- 085 T6a (list-local vertices: −1.49 ms on 2D, dropped for a 3D blocker), 088 (arena chosen), 098 (pinned merge path)
- [Benchmarking recipe](../recipes/benchmarking.md) — ceilings, lanes, the lock
