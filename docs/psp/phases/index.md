# Phases

The roadmap in [§5 of the memory map](../memory-map.md#5-roadmap) is a sequence, not a schedule. Each phase
below has its own page: what it is about, what has to be true before starting, the tasks, and the exit
criteria that end it.

A phase is not started until the previous one's criteria are met **and recorded**. Work inside a phase happens
in [sprints](../templates/sprint.md), and each sprint names the exit criterion it advances.

| phase | about | state |
|---|---|---|
| [0 — Baseline](00-baseline.md) | fork, branch at 6.2, remotes, docs site | **done** |
| [1 — 6.2 boots on pspGL](01-bring-up-6-2.md) | the port's platform file, three stubs, four guards | not started |
| [2 — Instrumentation](02-instrumentation.md) | counters and baselines; nothing measurable yet exists | not started |
| [3 — pspGL upgrade A/B](03-pspgl-upgrade.md) | eleven ready-made commits, measured | not started |
| [4 — rlgl batcher](04-rlgl-batcher.md) | collapse per-sprite submission, still on GL | not started |
| [5 — Native GU backend](05-native-gu-backend.md) | drop OpenGL and pspGL entirely | not started |
| [6 — Textures and VRAM](06-textures-vram.md) | swizzle, allocation, residency | not started |
| [7 — 2D specialisation](07-2d-specialisation.md) | text, tile maps, shapes, particles | not started |
| [8 — Input](08-input.md) | pad mapping, nub calibration, edge detection | not started |
| [9 — Audio](09-audio.md) | `sceAudio` output without costing frame time | not started |
| [10 — 3D and advanced](10-3d-advanced.md) | models, RTT, culling, ME options | not started |
| [11 — Release gate](11-release-gate.md) | four configurations, one report | not started |

Two notes on ordering that are easy to get wrong:

- **Phase 2 before phase 3 and 4.** Without counters there is no way to attribute a gain to a change, and the
  first two optimisations available are ones we did not write.
- **Input and audio are late, not unimportant.** They are independent of the renderer, they do not block the
  measurements, and starting them early would mix two kinds of risk in one debug session.
