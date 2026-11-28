# Sprint 097 (P10-S27) — The 078 regression: bisect the merged path

| | |
|---|---|
| Roadmap phase | [Phase 10 — 3D and advanced](../phases/10-3d-advanced/index.md) |
| Sprint | P10-S27 |
| Branch / base commit | `master` @ `8d876b7d`; reference point `d4310135` (078 adopted, pspgu `b33`, `libraylib.a c2004b26`) |
| Started / closed | 2026-11-28 (written) |
| Build number(s) | driver and library rebuilt at every bisect point; the point's own `b` tag is recorded |
| Tasks | 3/5 — premise withdrawn (§9); no bisect result to report |

## 0. Handover

096 §9.8, measured today: the merged path holds **250** cubes at 60 fps; 078 recorded **270**, and at COUNT=270 the
same tool read work **15.245** then against **16.60** now. The scene, the tool (`matrix_probe MODE=scene PROBE=0`), the
frame count and the layout are all identical, so **7–10 % per cube was lost between `d4310135` and `8d876b7d`** on the
path every demo ships.

That is the largest unexplained number in the phase, and unlike the pair it is not a step artefact: a count and a
median at a fixed COUNT are stable across runs.

## 1. Objective

Find the commit, and if it is a defect, the line. Do not optimise; localise.

## 2. Why the ceiling and not the pair

At COUNT=270 the frame alternates periods, which is exactly where a count is binary and unambiguous: median 16.68
(the 078 behaviour) or 33.35 (today's). `work` is reported beside it but is not the criterion — 096 §9.7 showed it
moves by under 1 ms between the two states.

## 3. Scope

**In:** a bisect over commits that change built code between `d4310135` and `HEAD`; the criterion at COUNT=270; the
endpoint validation; a statement of the commit and the line, or of the fact that no single commit carries it.

**Out:** the fix (its own task once the cause is known, unless it is a revert); the copy removal (098); the thin close
(099).

## 4. Method

At each point, build **both** the library and the driver from that commit, so the point is the shipped pair of its day.

```sh
git worktree add /tmp/bisect <sha>
cd /tmp/bisect/src && rm -f *.o platforms/*.o libraylib.a \
  && make PLATFORM=PLATFORM_PSP PSP_BATCH=1 PSP_MERGE_DRAWS=1 -j8 \
  && cp libraylib.a $PSPDEV/psp/lib/
cd /tmp/bisect && ./tools/pspgu-build.sh
cd psp-tests/matrix_probe && make MODE=scene PROBE=0 COUNT=270 FRAMES=200
# copy the PRX to the host root, run twice, reset between
```

Criterion: **median 16.68 passes, median 33.35 fails**, twice.

The window, code-changing commits only, in order: the 2026-11-25 upstream merge and its rmodels/rlgl/rtextures/rshapes
commits, 079 (sincos) and its two driver reverts, b37 (depth mask, blend, dither, clip planes), 080 (lighting/fog
scoping), the phase-14 audio commits, 085 (split, list-local vertices), 088/089 (submission and vertex memory), the two
VFPU thread attributes, 090 (clean hint), 095 T2 (batcher move), 092 (lazy bake), 091 T1/T4b, 096 T1/T2a/T4, and the
merge-path probe commits.

A bisect that lands on a **docs-only or test-only** commit is a bisect gone wrong, not a cause: those points must be
re-tested.

## 5. Tasks

| # | Task | Status | Acceptance | Result |
|---|---|---|---|---|
| T1 | **Validate the endpoints.** `d4310135` must pass at 270; `8d876b7d` must fail, both twice | `todo` | a clean pass/fail pair, or the regression is not in this window and the sprint stops | — |
| T2 | **Bisect.** Halve the window over code-changing commits, building library + driver each time | `todo` | the boundary commit is stated with its two medians | — |
| T3 | **Localise.** Read the boundary commit's diff against its parent and name the line(s) that can cost ~1.4 ms at 270 instances | `todo` | a named change, or "no single commit carries it" with the measurements that say so | — |
| T4 | **Decide.** Revert, fix, or accept with the number; if fixed, re-measure the ceilings (250/570) and the gates | `todo` | explicit state, gates green | — |
| T5 | **Close.** Numbers into 096 §9.8 and the phase index | `todo` | docs strict | — |

## 6. Design notes

**Bisect the shipped pair, not one half.** The window contains two driver tags (b33 → b36/b37) and one upstream merge;
testing only the library would blame raylib for a driver change or the reverse.

**Do not use `psp_showcase` for the criterion.** Its frame contains fog, ground and HUD, so its 33 ms budget is not the
ceiling's; it is useful afterwards as a confirmation.

**Keep the driver restored.** Every point installs a driver into `$PSPDEV`; the last point and the exit both rebuild
`master`'s pair (`./tools/pspgu-build.sh`, `make` in `src`).

## 7. Gates

| gate | expected |
|---|---|
| endpoint at 270 | 16.68 at `d4310135`, 33.35 at `8d876b7d` |
| each bisect point | two runs agreeing, reset between |
| after any fix | 613 / 284 / checksum `86b1e3c2 / c4b13eb4 / 670d502c`, ceilings 250/570 or better |

## 8. Exit criteria

- [ ] the boundary commit is named, or the window is cleared with measurements
- [ ] the cause is a stated line, or declared unattributable
- [ ] a decision is recorded (revert / fix / accept with the number)
- [ ] `$PSPDEV` holds `master`'s library and driver at the end
- [ ] docs strict passes

## 9. Result — the premise was withdrawn, not confirmed

T1 was executed: at COUNT=270 `d4310135` (b33) read median 16.684 / 16.683 with work **15.285 / 15.293**, so the
window looked real.

**Then the console proved to be shared.** A sibling session was running `live-capture.sh --port 11000 --root
/home/psp/galaga-host/b` — this console and this host root — and both of us installed into one `$PSPDEV`. Re-running
the same b33 build under a quiet console and master's toolchain read **15.807 / 15.806 / 15.815**, i.e. master's own
number, and master re-read 15.81. The ceilings moved the same way: 250/570 in the shared batch, **270/610** quiet.

| claim | status |
|---|---|
| 078 held 270 where master held 250 | **withdrawn** — master holds 270 quiet; b33 and master measure equal in work |
| work at 270 fell 7–10 % (15.25 → 16.60) | **withdrawn** — 15.81 quiet on both |
| a library commit between b33 and master costs ~0.5 ms | **not demonstrated**; not bisected further, because the precondition (a stable baseline) failed |

T2 was partly executed before the withdrawal: `ddbd5340`, `037bfd67` and `a2b774db` all read 15.38–15.82 while the
console was shared, and the driver was isolated as irrelevant (master library + b33 driver read 15.81, same as b37).
Those readings are recorded as *shared-console* data; none is evidence of a commit's cost.

T3–T5 are therefore closed as: no cause to name, no fix, no revert. The lasting output of the sprint is the lane
setup (`tools/psp-lane.sh`, two toolchains) and the measurement rule in 096 §9.9 and the benchmarking recipe.

## 10. Follow-ups

- Whatever 098 and 099 build on assumes the ceiling baseline; if this sprint moves 250/570, their acceptance numbers
  are restated before they run.

## 11. References

- 078 (`boon` adoption and its ceiling), 096 §9.7–§9.8 (the pair vs the count), `psp-tests/matrix_probe`
- [Benchmarking recipe](../recipes/benchmarking.md) — the Ceilings section
