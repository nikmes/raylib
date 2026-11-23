# Sprint 081 (P10-S14) — The two reference ideas: verify, measure, adopt or drop

| | |
|---|---|
| Roadmap phase | [Phase 10 — 3D and advanced](../phases/10-3d-advanced/index.md) |
| Sprint | P10-S14 |
| Branch / base commit | `master` @ `1dd95cf9` (benchmark runner; showcase pair) |
| Started / closed | 2026-11-23 (written) / — |
| Build number(s) | pspgu `b33` + one experimental flag per task; `libraylib.a` unchanged; benchmarks from `tools/psp-bench.sh` |
| Tasks | 0/4 done |

## 0. Handover

**Where the two ideas came from.** A read-only survey of `/home/psp/ref/PSPRuntime` (and its two sibling projects)
found a different set of render-path choices: GUM float matrices, float32 positions, 5551 vertex colour, a 5551
framebuffer, `GU_TSPRITES`, `sceGuGetMemory` per flush, and a full `sceGuSync` every frame. Two of those looked
adoptable. Reading our driver before writing this sprint changed what they are:

1. **"5551 framebuffer to halve fill bandwidth" is not available.** `tools/pspgu/egl.c:372` already allocates
   **`GU_PSM_5650`** — our framebuffer is 16-bit today (272 KiB each, three of them). The reference's 5551 is the
   same bandwidth with a different channel split (5-5-5-1 instead of 5-6-5-0). What is left to test is the *alpha
   bit and the colour split*, not the bandwidth.
2. **"Inverted depth" is already our convention, expressed differently.** We set `sceGuDepthRange(65535, 0)` and map
   `GL_LESS` to `GU_GEQUAL` (`tools/pspgu/gl_state.c:51`, `:166`), and clear through
   `sceGuClearDepth(65535 - depth*65535)` (`:57`). The reference clears to 0 and uses `GU_GEQUAL` directly. These
   are the same arithmetic; what is untested is whether our clear/range pairing behaves identically on every path.

So this sprint is not "adopt two ideas". It is: **prove the equivalence we believe in, measure the one real variant,
and adopt only what the benchmarks show.**

**The instrument this sprint leans on** is `tools/psp-bench.sh` from `1dd95cf9`: both demos, two runs per point,
frame-indexed animation, work/draws/binds/runs recorded, `--baseline` for deltas. Repeatability measured at
0.001–0.005 ms, so a change under ~0.05 ms in work is not a result.

**The baselines to beat** (2026-11-23, pspgu b33, `tools/psp-bench.sh`):

| demo | avg | fps | work | draws | binds | runs |
|---|---|---|---|---|---|---|
| `psp_showcase` (3D) | 50.08 ms | 20.0 | **34.88 ms** | 4.0 | 5.0 | 2.00 |
| `psp_2d_showcase` (2D) | 33.80 ms | 29.6 | **26.58 ms** | 6.0 | 4.0 | — |

**Read before acting:** `tools/pspgu/egl.c` (framebuffer allocation, clear, depth range), `tools/pspgu/gl_state.c`
(`glClearDepth`, `glDepthFunc`, `glFogfv`), `tools/psp-bench.sh`, `docs/psp/recipes/benchmarking.md`, and the two
demo sources. Reference material: `/home/psp/ref/PSPRuntime` (read-only, local, not part of this repo).

**First action, and it is not code:** make sure `tools/pspgu/` has no uncommitted work from another session —
sprint 079 has been editing it. A baseline taken on someone else's half-finished driver measures their build, not
ours. Record the driver hash from the runner's header line in §7.

## 1. Objective

Falsifiable, pre-registered:

1. **Depth parity:** with the reference's exact depth form (`clear 0`, `GU_GEQUAL`, same range), every gate is
   **0 differing pixels** and the benchmarks move by **less than 1 %**. If so, the idea is recorded as *already
   adopted*, with no shipped change.
2. **Framebuffer format:** `PSP_GU_FB_FMT=5551` changes **work by less than 2 %** (same bandwidth) and its colour
   delta is quantified (max channel error and the share of differing pixels). It is adopted only if it wins, or if a
   concrete feature needs framebuffer alpha; otherwise it is dropped with the numbers.
3. **No benchmark regresses** for either arm: every gate in §6 stays at its recorded value.

## 2. Why now

The benchmark pair and the runner exist, and the survey produced exactly two candidates. Before the port accumulates
more of its own conventions, it should be able to say "we looked at the reference and measured the difference"
rather than "we use our own idiom". The 2D demo is fill-bound (26.6 ms of 33.8), so a real bandwidth change would
show there first; the 3D demo carries the depth path (34.9 ms, 2 merged runs, one clipped ground draw).

## 3. Scope

**In:** `tools/pspgu/egl.c` and `gl_state.c` behind build switches (`PSP_GU_DEPTH_REF`, `PSP_GU_FB_FMT=5551`), the
benchmark runner (only if a switch needs to be recorded), the docs.

**Out:** the depth *function mapping* for apps (unchanged), the buffer formats of render targets (owned by P10-S4),
vertex formats and compact colours (dropped in sprint 077 with numbers), GUM (070), the sync model (Phase 5), and
anything in sprint 079/080.

## 4. Preconditions

- [ ] `master` @ `1dd95cf9`; `tools/pspgu/` clean (`git status --short tools/pspgu` empty)
- [ ] `tools/psp-bench.sh` runs end to end; a fresh `--label baseline` is recorded in §7 with its driver hash
- [ ] Gates available: `batch_check`, `scene_3d`, `basic_shapes`, `clip_probe`, RTT, colour gate if 080 has landed
- [ ] The user has authorised the device session

## 5. Tasks

Status vocabulary — `todo` · `doing` · `done` · `blocked` · `dropped`.

| ID | task | status | verification | evidence |
|---|---|---|---|---|
| T1 | **Baseline and method.** Run `psp-bench.sh --label baseline-081 --runs 2`; capture every gate; record the driver hash, git rev and both demo rows in §7. Re-run to confirm the repeatability band before trusting later deltas | `todo` | two baselines agree within ~0.01 ms | |
| T2 | **Depth equivalence (`PSP_GU_DEPTH_REF`).** Add the reference form as a compile switch: clear the depth buffer to the far value with `sceGuClearDepth(0)`, keep `sceGuDepthRange(65535, 0)` and `GU_GEQUAL`, and leave `glDepthFunc`'s mapping untouched. Run both demos and every gate | `todo` | 0 differing pixels on every gate; \|Δwork\| < 1 % on both demos | |
| T3 | **Framebuffer format (`PSP_GU_FB_FMT=5551`).** Switch the three framebuffers to `GU_PSM_5551`, keep size and stride, and run both demos and every gate. Quantify the colour delta against 5650 (max channel error, share of differing pixels) and re-check the RTT readback path, where framebuffer alpha now exists | `todo` | \|Δwork\| < 2 % on both demos, or the win recorded; colour delta quantified; RTT readback behaviour recorded | |
| T4 | **Decide and close.** Adopt or drop each arm against §1; if adopted, flip the default, re-baseline with the runner and update the phase page. Remove the switch that did not win, or keep it documented as a measurement-only build. Re-run the gates and the sample | `todo` | decision table in §9; every gate at its recorded value; pspGL restored | |

### Design notes

**T2 — what "the same arithmetic" means.** Our clear path converts a GL depth in [0,1] to `65535 - depth*65535`
because the GE's range is reversed; the reference clears to 0 and asks for `GEQUAL`. Both leave the buffer at "far"
and accept nearer fragments. The test is not whether they look the same in one scene: it is whether the *mapping*
holds for every depth function the port actually uses (`GL_LESS`, `GL_LEQUAL`, and nothing else in our gates). If a
gate moves by one pixel, the clear/range pairing has a case we have not mapped, and that is the finding.

**T2 — scope of the switch.** Keep it to the clear and the function; do not change how apps set depth. If the arm
wins nothing (expected), delete the switch after the sprint and keep the table: the value is the proof, not the code.

**T3 — the split, not the width.** 5650 is 5-6-5 with alpha dropped; 5551 is 5-5-5-1. Green loses a bit and alpha
appears. Expect a small colour delta everywhere, so this arm can never be "0 diff"; that is why §1 asks for the
delta and a work threshold rather than parity. Sample the delta on `basic_shapes` (a flat-colour gate, 284 colours)
and on the 3D demo (gradients through fog).

**T3 — what framebuffer alpha would buy.** Nothing today: the render targets set their own formats, `glReadPixels`
readback already reports alpha 0 on the screen buffer, and no sample uses framebuffer alpha. If that changes, the
argument for 5551 is a feature, not a benchmark.

**T3 — fill-bound or not.** The 2D demo's work is dominated by additive fill. If a format change does not move it,
the demo is not fill-bandwidth-bound at this resolution; that is itself a result and it points the next optimisation
at vertex/CPU or blending state instead.

**Measurement protocol.** Runner first, always: `--label` per arm, `--runs 2`, `--baseline` the previous label.
Gates with the usual two identical captures after `DONE`. Never compare a number from the runner with one from a
hand-run build.

## 6. Budgets

| budget | limit | measured | tag |
|---|---|---|---|
| T2 depth parity | 0 differing pixels on every gate | | |
| T2 work change | \|Δ\| < 1 % on both demos | | |
| T3 work change | \|Δ\| < 2 %, or the win recorded | | |
| T3 colour delta | max channel error and % pixels recorded | | |
| existing gates | every gate at its recorded value | | |
| repeatability | two baselines within 0.01 ms | | |

## 7. Measurements

| run | build | workload | metric | value | notes |
|---|---|---|---|---|---|
| baseline | b33 | runner, both demos | avg / work / fps | | driver hash: |
| T2 | b33 + switch | runner, both demos | avg / work | | ≤1 % expected |
| T2 | b33 + switch | gates | diff | | 0 expected |
| T3 | b33 + switch | runner, both demos | avg / work | | ≤2 % expected |
| T3 | b33 + switch | `basic_shapes` vs 5650 | colour delta | | |
| T3 | b33 + switch | RTT readback | alpha / diff | | |

## 8. Exit criteria

- [ ] T1: a baseline with its driver hash and repeatability is recorded
- [ ] T2: 0 diff and <1 % work, or the arm is recorded as a finding with the pixel that moved
- [ ] T3: work delta and colour delta recorded; adopt/drop decision recorded
- [ ] T4: defaults and docs match the decision; every gate at its recorded value; pspGL restored

## 9. Result

—

## 10. Follow-ups

- If the 2D demo does not move with a format change, the next question is where its 26.6 ms goes: vertex submission,
  blending state changes or fill. A sprite-count/size sweep with the runner answers it.
- Framebuffer alpha becomes interesting only when a feature needs it (a translucent overlay read back, a composed
  RTT). Record the decision then, not now.
- The reference's other choices were already measured and rejected: compact vertex colours (077), GUM (070), the
  full-sync model (Phase 5).
