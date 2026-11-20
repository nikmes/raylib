# Sprint 002 — All five samples green, reference screenshots archived

| | |
|---|---|
| Roadmap phase | [Phase 1 — 6.2 boots on pspGL](../phases/01-bring-up-6-2.md) |
| Branch / base commit | `psp-6.2` @ `r2` build |
| Started / closed | — / — |
| Build number(s) | `r3` |
| Tasks | 0/7 done |

## 0. Handover

**Situation.** Sprint 001 produced a PSP `libraylib.a` from 6.2 and got `basic_shapes` on screen. Four samples
have not been tried. Your job: all five run, and each has an archived screenshot with its build number.

**Read before acting:**

1. [Memory map](../memory-map.md) §2, §3, §9 — and §4, which explains why bunnymark's numbers lie.
2. [Phase 1 page](../phases/01-bring-up-6-2.md), exit criteria.
3. Sprint 001's result section — it records whatever the first build actually needed.

**First action.** Confirm 001's state: `basic_shapes` still renders before you touch anything. If it does not,
you are debugging 001, not starting 002.

**Hard rules.**

- `usbhostfs_pc` must be rooted at `/home/psp/raylib4Psp/samples`: `cubicmap` and `bunnymark` load assets from
  absolute `host0:/textures/...` paths and will silently fail with a different root.
- Run `pspsh -e "reset"` before each session. `kill` leaks the module's heap and later runs happen in a
  partition that has already been eaten.
- Every screenshot gets the build number in its filename. A screenshot without one is not evidence.
- **bunnymark's "batched draw calls" figure is fictional on PSP.** It is computed as
  `1 + count/MAX_BATCH_ELEMENTS` on a path that does not batch. Do not record it; sprint 003 replaces it.
- Do not fix sample bugs beyond what blocks running — note them in §10 instead.

**Commands.**

```sh
cd /home/psp/raylib4Psp/samples && usbhostfs_pc &     # root = samples/
pspsh -e "reset"
pspsh -e "./textures/bunnymark/raylib.prx"
pspsh -e "scrshot host0:/shot.raw"                    # converts to PNG off-device
```

**Do not touch.** The renderer, the platform file beyond real breakage, and any sample's `main.c` (a sample
that needs editing to run is a finding, not a fix).

**Done means** §8 below: five samples either correct or documented as not working, with reasons.

## 1. Objective

All five samples — `basic_shapes`, `collision_area`, `logo_raylib_anim`, `cubicmap`, `bunnymark` — build
against 6.2, run on the console, and each has an archived screenshot named with its build number. Phase 1's
exit criterion is met with evidence, not with "it seemed to work".

## 2. Why now

Sprint 001 proved one sample renders. A regression baseline needs all five, because each exercises a different
path: shapes (quads disabled on PSP), collision (CPU-only, no renderer), logo animation (textures + timing),
cubicmap (textured 3D-ish quads), bunnymark (throughput). Missing one means the later phases cannot tell
"broken by my change" from "never worked".

## 3. Scope

**In:** the remaining four samples, asset path handling, screenshot archive, the correctness fingerprints.

**Out:** optimising anything, fixing upstream sample bugs beyond what blocks running, audio (no backend yet),
input beyond the existing pad mapping.

## 4. Preconditions

- [ ] Sprint 001 exit criteria met
- [ ] `usbhostfs_pc` rooted so that `host0:` resolves the samples' asset paths (`samples/` as root, since
      `cubicmap` and `bunnymark` use absolute `host0:/textures/...` paths)
- [ ] Screenshot procedure works: `pspsh -e "scrshot host0:/shot.raw"`, converted to PNG off-device
- [ ] `reset` before each measurement session

## 5. Tasks

| ID | task | status | verification | evidence |
|---|---|---|---|---|
| T1 | build and run `collision_area` | `todo` | renders; input moves the box (pad already mapped in the platform file) → expected: no renderer dependency — if this fails, the problem is not pspGL | — |
| T2 | build and run `logo_raylib_anim` | `todo` | animated logo, timing from the platform timer → expected: smooth; if it stutters, note the frame time before blaming the renderer | — |
| T3 | build and run `cubicmap` with its two PNGs on `host0:` | `todo` | textures load (log line per texture), cubic map drawn → expected: absolute `host0:/textures/...` paths resolve because usbhostfs is rooted at `samples/` | — |
| T4 | build and run `bunnymark` | `todo` | loads `wabbit_alpha.png`, spawns on CROSS, header text visible → expected: **its "batched draw calls" figure is fictional on PSP** — do not record it as a measurement | — |
| T5 | archive one screenshot per sample: `docs/psp/baselines/<sample>-<build>.png` plus a one-line | `todo` | each file has the build number in its name | — |
| T6 | record which samples do **not** work and why; a sample that cannot run on PSP is as important as | `todo` | — | — |
| T7 | write the findings into [§2](../memory-map.md#2-memory-map-where-the-work-happens), replacing | `todo` | — | — |

Status vocabulary: `todo` / `doing` / `done` / `blocked` / `dropped`. IDs are stable — never
renumber; a task that no longer applies is `dropped` with the reason in the log below.

### Task log (append-only)

- _(not started)_

## 6. Budgets

| budget | limit | measured | tag |
|---|---|---|---|
| sample crashes | 0 | | |
| boot-to-first-frame | < 5 s | | |

## 7. Measurements

| run | build | workload | metric | value | notes |
|---|---|---|---|---|---|
| `r3` | basic_shapes | distinct colours | | reference: 284 |
| `r3` | collision_area | renders | yes/no | | |
| `r3` | logo_raylib_anim | holds 60 fps | yes/no | | |
| `r3` | cubicmap | textures loaded | count | | |
| `r3` | bunnymark | spawns on CROSS | yes/no | | counter is fictional |

## 8. Exit criteria

- [ ] five samples build against 6.2 with the same configuration
- [ ] each either renders correctly or is documented as not working, with the reason
- [ ] screenshots archived with build numbers, fingerprints written down
- [ ] Phase 1 exit criteria recorded in the phase page

## 9. Result

(To fill in on close. Include the bunnymark render anomaly if it reproduces — the 5.5 build showed correct
header text over a noisy content area, which has never been explained.)

## 10. Follow-ups

- Bunnymark's counter replacement is sprint 003's job, not this one.
- If a sample needs a texture format pspGL rejects, that is a phase 6 concern — note it, do not fix it here.
