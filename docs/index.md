# raylib 6.2 on PSP

Working documentation for this fork: a PSP backend for raylib that starts on OpenGL and ends on
`sceGu`, plus the measurements behind each step.

The source of truth is [Memory map & roadmap](psp/memory-map.md) — it is the raw `PSP_MEMORY_MAP.md`
from the repository root, rendered here unchanged. Nothing on this page supersedes it.

## Where we are

| | |
|---|---|
| **Base** | raylib **6.2** — tag `02566453`, branch `psp-6.2` |
| **Target** | PSP-1000, 32 MB, firmware 6.60 + ARK, 24 MB user partition |
| **Toolchain** | pspdev v20261101, `psp-gcc 15.2.0` |
| **Renderer today** | pspGL (2022 code, shipped by pspdev) — OpenGL 1.1 immediate mode |
| **Renderer goal** | a `GRAPHICS_API_PSP_GU` backend talking to the GE directly |
| **Code changes so far** | none — the port is not imported yet |

## The one number that drives everything

A sprite in raylib today costs a texture bind, a `glBegin`, four vertices, a `glEnd` and an unbind.
On the PSP that is one submission per sprite. A hand-written GU renderer reaches **1024 sprites per
draw call** for axis-aligned sprites and 512 per call for quads.

```text
today:   N x (bind + glBegin + 4 verts + glEnd + unbind)
target:  1 bind + ceil(N / 1024) native array submissions
```

Everything in the roadmap follows from closing that gap without changing raylib's API.

## Roadmap at a glance

| phase | goal | state |
|---|---|---|
| [0](psp/phases/00-baseline.md) | Repository baseline: fork, branch at 6.2, remotes | done |
| [1](psp/phases/01-bring-up-6-2.md) | 6.2 boots on pspGL: platform file, 3 stubs, guards, Makefile | next |
| [2](psp/phases/02-instrumentation.md) | Instrumentation and baseline numbers | |
| [3](psp/phases/03-pspgl-upgrade.md) | pspGL upgrade A/B (11 ready-made commits) | |
| [4](psp/phases/04-rlgl-batcher.md) | PSP fixed-function `rlgl` batcher over GL11 | |
| [5](psp/phases/05-native-gu-backend.md) | Native GU backend, list rotation, no per-frame stall | |
| [6](psp/phases/06-textures-vram.md) | Textures, swizzle, best-fit VRAM, residency | |
| [7](psp/phases/07-2d-specialisation.md) | 2D specialisation: text, tile maps, shapes, particles | |
| [8](psp/phases/08-input.md) | Input: pad mapping, nub calibration, edge detection | |
| [9](psp/phases/09-audio.md) | Audio: `sceAudio` output on its own thread | |
| [10](psp/phases/10-3d-advanced.md) | 3D and advanced: models, RTT, culling | |
| [11](psp/phases/11-release-gate.md) | Release gate: four-configuration comparison | |

Each phase has its own page — tasks, preconditions and exit criteria — starting from the
[phase index](psp/phases/index.md). A phase is not started before the previous one's criteria are met and
recorded.

## Reference material

Six external sources are kept read-only outside this tree — pspGL (three variants), a GU tutorial
repository, and two older GU engines by the same author. Their licences decide what may be used:
two are GPL (ideas only unless dual-licensed), one has no licence at all, and the working GU renderer
that supplied the batching numbers is private. The table with licences and the "take / do not take"
split is in the roadmap, §7.

## Two more compute units

Beyond the main CPU the PSP offers:

- **VFPU** — a vector coprocessor with 128 single-precision registers and table-driven `sin`/`cos`/`rsqrt`/
  `rcp` at one cycle each. Roughly 22 significant bits, deterministic, no IEEE. For us the clearest win is
  `vt5551.q`, which converts four RGBA8 pixels per cycle during texture upload.
- **Media Engine** — a second MIPS core at the same clock, measured at x2.0–2.9 when work is split
  correctly, with no cache coherence, no VFPU, and no presence in PPSSPP at all. Audio mixing and building
  GE display-list words are its measured sweet spots.

Both are optional and both are gated behind rules — see [compute units](psp/memory-map.md#10-compute-units-beyond-the-main-cpu).
Neither is a premise for the rendering phases.

## Where to read

Everything is on this disk: our fork, two raylib trees, six GU/pspGL references, the pspdev headers and
samples, and the Sony SDK (reference only, never copied). When planning a sprint, start from the
[local source index](psp/memory-map.md#11-local-source-index) — it maps each path to the question it answers,
with grep recipes and the list of ME/VFPU repositories worth cloning on demand.

## Measuring

Numbers come from the console, never from the emulator. Memory and speed figures require a `reset` in
`pspsh` first, because `kill` leaks the module's heap. One variable per run; control, change, and
control again. See §6 of the roadmap for the full protocol.
