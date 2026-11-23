# Phases

The roadmap in [§5 of the memory map](../memory-map.md#5-roadmap) is a sequence, not a schedule. Each phase
below has its own page: what it is about, what has to be true before starting, the tasks, and the exit
criteria that end it.

A phase is not started until the previous one's criteria are met **and recorded**. Work inside a phase happens
in [sprints](../templates/sprint.md), and each sprint names the exit criterion it advances.

Sprints carry two identifiers: `NNN` is the global file/traceability number, and `P<N>-S<n>` is the phase/S
label used in titles, the nav and cross-links. Example: `P6-S2` is sprint 015.

| phase | about | state |
|---|---|---|
| [0 — Baseline](00-baseline/index.md) | fork, branch at 6.2, remotes, docs site | **done** |
| [1 — 6.2 boots on pspGL](01-bring-up-6-2/index.md) | the port's platform file, three stubs, four guards | **done** |
| [2 — Instrumentation](02-instrumentation/index.md) | counters and baselines; nothing measurable yet exists | **done** |
| [3 — pspGL upgrade A/B](03-pspgl-upgrade/index.md) | eleven ready-made commits, measured | **done** — fork `ae9a668` @ 8192 words |
| [4 — rlgl batcher](04-rlgl-batcher/index.md) | collapse per-sprite submission, still on GL | **done** — 1,000 sprites at 60 fps, ceiling 1,600 |
| [5 — Native GU backend](05-native-gu-backend/index.md) | drop OpenGL and pspGL entirely | **done** — 2,375 sprites at 60 fps, 2D byte-identical; `DrawMesh` parity deferred to phase 10 |
| [6 — Textures and VRAM](06-textures-vram/index.md) | swizzle, allocation, residency | **done** — sprints 015, 016, 017; CLUT/mipmaps dropped with measurements; 018 not needed |
| [7 — 2D specialisation](07-2d-specialisation/index.md) | text, tile maps, shapes, particles | **done** — 018–023; all exit criteria; cache strips measured null |
| [8 — Input](08-input/index.md) | pad mapping, nub calibration, edge detection | **done** — P8-S1–S4 closed (5/5, 6/6, 5/5 resolved, 5/5) |
| [9 — Audio](09-audio/index.md) | `sceAudio` output without costing frame time | **done** — route A: 8 sounds + 1 streamed track at 60 fps, 0 dropouts in 18,000 frames, clean close; SAS/ME deferred (P9-S6/S7 dropped) |
| [10 — 3D and advanced](10-3d-advanced/index.md) | models, RTT, culling, ME options | **done** — P10-S7 re-tested the criteria: 60 fps at N=50, work 11.56 ms, 0 arena resets; lit claim corrected; P10-S8 carries the remaining CPU cost |
| [11 — General optimisation](11-general-optimisation.md) | compiler, libc, allocator, I/O, startup — not the renderer | not started — reviewed; 8 sprints written (052–059); blocked on 7–10 |
| [12 — Release gate](12-release-gate/index.md) | four configurations, one report | not started |
| [13 — Samples porting](13-samples-porting/index.md) | the whole upstream example corpus (227 samples, 8 categories) as a gated regression suite | in progress — core/shapes/textures audited (68 ported); audio 9; text/models/shaders/others unaudited; P13-S7 T1b gate blocked |

Notes on ordering that are easy to get wrong:

- **We deviated from the sequence twice, and both deviations are deliberate.** Sprint 010 ran a *probe* inside
  phase 6 before phase 5 because the answer — is fill texel-byte-bound? — decided whether phase 5 was worth
  building. It came back no (fill is rasterisation-rate-bound), so phase 5's case is smaller than it looked and
  phase 6's own deliverable (residency, eviction, per-texture formats) still cannot start: it needs a driver that
  owns VRAM, which is phase 5. A probe is not the phase it lives in; the dependencies below are unchanged.
- **Phase 7's draw rejection does not depend on 5 or 6.** Skipping off-screen draws happens above the driver, in
  rlgl or the batch, so it can be measured before a backend exists — but only on a benchmark whose overdraw is not
  bunnymark's 9.4×.

- **Phase 2 before phase 3 and 4.** Without counters there is no way to attribute a gain to a change, and the
  first two optimisations available are ones we did not write.
- **Input and audio are late, not unimportant.** They are independent of the renderer, they do not block the
  measurements, and starting them early would mix two kinds of risk in one debug session.
- **Phase 11 sits after the renderer phases on purpose.** It profiles the frame and load path those phases
  produce; running it earlier would tune a workload that is about to change.
- **Phase 13 is continuous, not a successor to 12.** It consumes the capabilities of phases 5–11 as they land
  and never blocks them; wave 0 (12 drop-in samples) needs no new capability.
