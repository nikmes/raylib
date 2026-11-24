# Sprint 093 (P10-S23) — World-space bake arm and the merge decision

| | |
|---|---|
| Roadmap phase | [Phase 10 — 3D and advanced](../phases/10-3d-advanced/index.md) |
| Sprint | P10-S23 |
| Branch / base commit | `master` @ `fe1171fe` — design: [Redesigning the 3D path](../phases/10-3d-advanced/pspgu-redesign.md) |
| Started / closed | 2026-11-24 (written) / — |
| Build number(s) | depends on S22; no new default until the decision |
| Tasks | 0/6 |

## 0. Handover

S22 gives per-object draws a baked buffer and a thin entry. Two questions remain, and they are the same question from
two sides:

1. **Is the per-object matrix upload worth avoiding?** `sceGuSetMatrix(GU_MODEL, m)` costs 64 B and a few commands
   per object — ~25 KiB of list at 400 objects. A world-space bake removes it entirely, at one buffer per instance.
2. **Does merging still earn its place?** It cost 31.92 ms at 400 cubes against M3D's 8.48 ms, and it owns the arena
   wall (~680 cubes, 154 replays at 1,200). But it also removes 99.5 % of draws, and if S22's thin path does not beat
   it, merging stays.

M3D-lessons F9 is the constraint: merging exists only for uniform scale, one mesh/material/texture, no vertex
colours and no skinning. The thin path has no such key.

## 1. Objective

Measure the world-space bake against the per-object matrix, then decide merging's fate with numbers at three sizes,
and adopt a single 3D architecture.

## 2. Why now

S22 can end with two viable 3D paths. Leaving both live is how a renderer grows two half-tested code paths; the phase
needs one answer before it closes.

## 3. Scope

**In:** a world-space bake arm for a static set; the merge A/B at 400/800/1,200; the key-limit comparison; the
replay/arena check; the gates; the decision.

**Out:** changing the winning path further (that is S24's close), 2D, submission, list-local vertices (089 closed
that).

## 4. Preconditions

- [x] S22's thin path exists and its per-cube cost is known
- [x] the arena wall is documented (~680 cubes; 154 replays at 1,200)
- [ ] the world-space arm has a scene where the transforms genuinely do not change

## 5. Tasks

| # | Task | Status | Acceptance | Result |
|---|---|---|---|---|
| T1 | **World-space bake arm.** Bake a static set into world space (one buffer per instance or one merged buffer) and draw it with no matrix upload | `todo` | 0 diff; bytes/frame and uploads/frame reported; valid only for static sets | — |
| T2 | **Merge A/B.** Merged against thin, 400/800/1,200, two runs | `todo` | work and avg per cell; the winner named per size | — |
| T3 | **Key limits.** State what merging can express and what it cannot (F9), and what that costs on the sample set | `todo` | the comparison includes content the merge key would reject | — |
| T4 | **Replays and arena.** Confirm 0 replays for the thin path at 1,200; report the merged wall's number | `todo` | no `MERGEPROBE` replays on the adopted path | — |
| T5 | **Gates + pair.** Full pixel table on the adopted path; the benchmark pair | `todo` | 0 diff; pair recorded | — |
| T6 | **Decision.** Keep one 3D architecture, or keep both with a stated rule for which to use | `todo` | one paragraph, one number per path | — |

## 6. Design notes

**World-space is "merge once, at load".** It removes the per-object upload and the per-frame expansion, which is why
it can beat both other paths on a truly static set. Its cost is memory proportional to instances, so it is measured
with the ledger, not by assertion.

**What falsifies the thin path.** If merging still wins at every size on the sample content, the design's premise is
wrong for this renderer — the bake and the clip hint would remain as per-object improvements, but merging stays the
architecture. That is a legitimate outcome and must be recorded as such, not softened.

**Do not keep two defaults.** If both survive, the rule must be mechanical (e.g. "baked static meshes take the thin
path, everything else merges"), stated in the driver's log at boot so a run cannot be misread.

## 7. Gates

| gate | expected |
|---|---|
| `batch_check` / `basic_shapes` | 613 / 284 |
| `scene_3d SCENE=both`, `MESH_SCENE`, RTT | 0 diff |
| `clip_probe` | 237/720 |
| 1,200 cubes | replays stated per path; no overflow |
| benchmark pair | two runs per point; adoption bar >0.05 ms |

## 8. Exit criteria

- [ ] merged vs thin measured at three sizes with two runs each
- [ ] the world-space arm's bytes and uploads are stated
- [ ] one architecture is chosen, or the rule for two is mechanical and logged
- [ ] the pixel table is 0 diff on whatever is chosen
- [ ] strict docs pass; default state explicit

## 9. Result

Pending — no device runs yet.

## 10. Follow-ups

- If the thin path wins, the merge code can be retired in S24 with its numbers kept in the phase record.
- If merging wins, the redesign's premise is refuted for this content and the next target is raylib's scene layer.
- Either way, S24 owns the matched ceiling sweep so the phase closes on one layout.

## 11. References

- [Redesign](../phases/10-3d-advanced/pspgu-redesign.md) — §3.4, §4
- [M3D compared](../phases/10-3d-advanced/m3d-lessons.md) — F5, F9, R3
- sprint 078 (merged runs, the arena wall, `MERGEPROBE`), 085 T2 (s8), 089 (§9.4)
- `src/rlgl.h` (`PSP_MERGE_DRAWS`), `tools/pspgu/gl_draw.c` (`pspguDrawMergedArrays`)
