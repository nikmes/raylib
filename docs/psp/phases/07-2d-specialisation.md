# Phase 7 — 2D specialisation

| | |
|---|---|
| State | not started |
| Depends on | Phase 5 or 6 (batching and texture residency in place) |
| Exit criterion | text submissions scale with atlas runs, not glyph count; tile maps with atlas runs, not tile count |

## What this phase is about

The generic batcher handles "many sprites sharing state". This phase handles the four workloads that dominate
real 2D games and have their own traps: text, tile maps, shapes and particles. Each gets a fast path that
removes work *before* vertices are generated — the cheapest vertex is the one never built.

## What has to be true before starting

- [ ] Batching works; submissions already scale with state runs
- [ ] Counters can separate logical objects from submissions
- [ ] A representative workload per category exists (a UI screen, a tile map, a particle emitter)

## Tasks

- [ ] **Text**: accumulate consecutive glyphs of one atlas into a single run; no per-glyph bind
- [ ] **Sprites**: reject fully offscreen sprites before generating vertices; fast path for unrotated and
      uniformly scaled; optional horizontal slicing for large sprites if cache benchmarks justify it
- [ ] **Tile maps**: draw per atlas/state run, not per tile; verify against a large map
- [ ] **Shapes**: keep the shapes texture bound across consecutive calls; batch rectangles, triangles and lines
      into separate compatible runs
- [ ] **Particles**: one dynamic vertex stream per texture run, double or triple buffered; measure simulation
      cost separately from rendering
- [ ] Cache glyph layout where raylib semantics allow it (no re-shaping per frame)

## What we already know

- `rtext.c` draws every glyph through `DrawTexturePro`, which binds and unbinds per call — that is the single
  worst behaviour for dialogue-heavy screens
- `rshapes.c` is already forced off the shapes-quads path on PSP in the 5.5 port; that guard has a reason and
  must be understood before it is removed
- Text and sprites both need the **unsigned 16-bit TEXCOORD** rule respected: negative or zero-centred UVs wrap
- The reference engine's `MAX_BUNNIES 50000` was unreachable in practice because ~96 KiB per flush against a
  2 MiB list capped it around 21 flushes — capacity planning is part of this phase, not an afterthought

## Exit criteria

- [ ] text: submissions proportional to atlas runs, measured on short labels and paragraphs
- [ ] tile map: submissions proportional to atlas runs on a map large enough to matter
- [ ] particles: one submission per compatible system, CPU simulation cost measured separately
- [ ] no visual regression in the samples, screenshots compared

## Sprints and features

None yet. Use the [sprint template](../templates/sprint.md).
