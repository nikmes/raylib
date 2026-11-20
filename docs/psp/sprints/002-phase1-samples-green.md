# Sprint 002 — All five samples green, reference screenshots archived

| | |
|---|---|
| Roadmap phase | [Phase 1 — 6.2 boots on pspGL](../phases/01-bring-up-6-2.md) |
| Branch / base commit | `master` @ `975ec47e` (6.5-dev + port, device-validated) |
| Started / closed | — / — |
| Build number(s) | `s1` … `s4`, one per sample |
| Tasks | 0/7 done |

## 0. Handover

**Situation.** Sprint 001 put raylib on the console: `libraylib.a` builds, `master` is validated on hardware,
and `basic_shapes` is already ported and running from `psp-tests/basic_shapes`. Four samples have not been
tried. Your job: four samples run, each with an archived screenshot and a fingerprint.

**Read before acting:**

1. [Memory map](../memory-map.md) §6 — the measurement protocol. It contains four traps that cost sprint 001
   whole runs, and this sprint walks into all four if ignored.
2. Sprint 001 §9 — what the first build actually needed.
3. [Phase 1 page](../phases/01-bring-up-6-2.md), exit criteria.

**Verified before this sprint started** (2026-11-20, this toolchain, staged 6.5-dev library
`13506883…`, headers `RAYLIB_VERSION "6.5-dev"`):

| sample | compiles against 6.5-dev | assets it loads | exits on |
|---|---|---|---|
| `collision_area` | **yes**, 0 errors | none | START |
| `logo_raylib_anim` | **yes**, 0 errors | none | START |
| `cubicmap` | **yes**, 0 errors | `host0:/models/cubicmap/cubicmap.png`, `…_atlas.png` | START |
| `bunnymark` | **yes**, 0 errors | `host0:/textures/bunnymark/wabbit_alpha.png` | START |

So the corpus drift that broke `basic_shapes` (`DrawCircleGradient` signature) does **not** affect these four.
Do not budget for porting them; budget for running them.

**First action.** Confirm sprint 001's state is still true before touching anything:
`pspsh -e "ver"` answers, then run `psp-tests/basic_shapes` once and see `DONE frames 600`. If that fails you
are debugging 001, not starting 002.

**Hard rules** (each one is a session lost in sprint 001):

- **Never send `exit`.** `exit` in pspsh is PSPLink's own shutdown command; the console drops to the XMB and
  the device disappears. Let the pipe end instead — pspsh disconnects on EOF and PSPLink stays.
- **Keep the shell attached for the whole run.** `pspsh -e "./app.prx"` returns immediately and the app's
  `TRACELOG` output goes nowhere: a crash then looks like a hang.
- **Never `scrshot` while the app is rendering.** It freezes the app a couple of seconds later. Capture while
  the app is *not* drawing (see T5) or accept that the app is frozen afterwards and `reset`.
- **`PSP_LARGE_MEMORY=0` on every build.** None of these four Makefiles set it; the PSPSDK default asks for a
  40 MB partition on a 32 MB console.
- `usbhostfs_pc` rooted at `/home/psp/raylib4Psp/samples`: `cubicmap` and `bunnymark` load absolute
  `host0:/…` paths and fail silently under a different root.
- `reset` between runs; `kill` leaks the module's heap.
- **bunnymark's "batched draw calls" figure is fictional on PSP** (`1 + count/MAX_BATCH_ELEMENTS` on a path
  that does not batch). Do not record it; sprint 003 replaces it.

**Commands.**

```sh
export PSPDEV=/home/psp/pspdev
export PATH=$PATH:$PSPDEV/bin

cd /home/psp/raylib4Psp/samples && usbhostfs_pc &   # root = samples/ (assets!)
pspsh -e "reset"

# build (per sample directory)
cd /home/psp/raylib4Psp/samples/models/cubicmap && make PSP_LARGE_MEMORY=0

# run, keeping logs alive and never sending exit
( printf './raylib.prx\n'; sleep 25 ) | pspsh
```

**Do not touch.** The renderer, the platform layer, and the sample sources. A sample that needs editing to run
is a finding for §10 — `basic_shapes` needed exactly one line and it lives in `psp-tests/`, not there.

**Done means** §8: four samples run, each with a screenshot and a fingerprint, or documented as not working
with the reason.

## 1. Objective

The four remaining corpus samples run on the console against the staged 6.5-dev library, each with an archived
screenshot and a written fingerprint (colours, resolutions, texture load lines, anomalies). Phase 1's exit
criterion — "the samples render" — is then met with evidence for the whole corpus.

## 2. Why now

Sprint 001 proved one sample and one code path. The corpus covers the paths later phases will regress:
`collision_area` (CPU-only, no renderer), `logo_raylib_anim` (textures + timing), `cubicmap` (image load →
texture, 3D-ish quads), `bunnymark` (throughput, and the counter sprint 003 must fix). Without them, "broken by
my change" is indistinguishable from "never worked".

## 3. Scope

**In:** building and running the four samples, asset path handling, screenshots, fingerprints, findings.

**Out:** `basic_shapes` (done, in `psp-tests/`), optimising anything, fixing sample bugs beyond what blocks a
run, audio (no backend), input changes, porting samples into `psp-tests/` (see follow-ups).

## 4. Preconditions

- [ ] PSPLink up: `lsusb` shows `054c:01c9`
- [ ] `pspsh -e "reset"` done; partition 2 at 25,149,440 bytes
- [ ] Staged library is the validated one: `sha256sum $PSPDEV/psp/lib/libraylib.a` starts `13506883…`,
      headers report `RAYLIB_VERSION "6.5-dev"`
- [ ] `usbhostfs_pc` running with `/home/psp/raylib4Psp/samples` as root
- [ ] `mkdir -p /home/psp/evidence/sprint002`
- [ ] Sprint 001 re-confirmed: `psp-tests/basic_shapes` reaches `DONE frames 600`

## 5. Tasks

| ID | task | status | verification | evidence |
|---|---|---|---|---|
| T1 | build and run `collision_area` (`shapes/collision_area`) | `todo` | logs show init + no exception; a screenshot shows the boxes → expected: works, it has no renderer dependency, so a failure here is not a pspGL problem | — |
| T2 | build and run `logo_raylib_anim` (`shapes/logo_raylib_anim`) | `todo` | animated logo visible; note the observed fps → expected: smooth; if it stutters, record frame time before blaming the renderer | — |
| T3 | build and run `cubicmap` (`models/cubicmap`) with both PNGs | `todo` | two `TEXTURE: [ID n] Texture loaded successfully` lines and the cubic map drawn → expected: absolute `host0:/models/cubicmap/…` paths resolve under the `samples/` root | — |
| T4 | build and run `bunnymark` (`textures/bunnymark`) | `todo` | `wabbit_alpha.png` loads; header text visible; it starts with **0 bunnies** — press CROSS to spawn groups of 100 → expected: **do not record "batched draw calls"**, it is fictional | — |
| T5 | capture one screenshot per sample into `/home/psp/evidence/sprint002/<sample>-<build>.png` | `todo` | capture **after** the run has stopped drawing (or accept a frozen app, then `reset`); verify each capture has plausible colour count, not a black frame or the PSPLink banner | — |
| T6 | record a fingerprint per sample: colour count, top colours, texture lines, anomalies | `todo` | each row in §7 filled | — |
| T7 | write anything surprising into [§2](../memory-map.md#2-memory-map-where-the-work-happens)/§9 | `todo` | memory map updated in the same commit | — |

Status vocabulary: `todo` / `doing` / `done` / `blocked` / `dropped`. IDs are stable — never renumber.

### Task log (append-only)

- _(not started)_

## 6. Budgets

| budget | limit | measured | tag |
|---|---|---|---|
| sample crashes | 0 | | |
| boot-to-first-frame | < 5 s | | |
| build warnings | 0 | | |

## 7. Measurements

| build | workload | metric | value | notes |
|---|---|---|---|---|
| `s1` | collision_area | renders | | |
| `s1` | collision_area | distinct colours | | |
| `s2` | logo_raylib_anim | renders / animates | | |
| `s2` | logo_raylib_anim | observed fps | | |
| `s3` | cubicmap | textures loaded | | expect 2 |
| `s3` | cubicmap | distinct colours | | |
| `s4` | bunnymark | texture loaded | | `wabbit_alpha.png` |
| `s4` | bunnymark | bunnies spawned | | manual: CROSS ×N |

## 8. Exit criteria

- [ ] four samples build against the staged 6.5-dev library with `PSP_LARGE_MEMORY=0`
- [ ] each either renders correctly or is documented as not working, with the reason and the log line that
      shows it
- [ ] one screenshot per sample in `/home/psp/evidence/sprint002/`, filename carrying the build
- [ ] fingerprints in §7 filled, anomalies recorded rather than explained away
- [ ] Phase 1 exit criterion recorded in the phase page

## 9. Result

(To fill in on close. Include the bunnymark render anomaly if it reproduces: the 5.5 build showed correct header
text over a noisy content area, and that has never been explained.)

## 10. Follow-ups

- **Port the corpus into `psp-tests/`** if phase 2 needs repeatable per-sample runs. As they stand, each sample
  exits on START and takes PSPLink with it; only `basic_shapes` has the bounded `DONE` + sleep pattern.
- Bunnymark's counter replacement is sprint 003's job.
- A texture format pspGL rejects is a phase 6 concern — note it, do not fix it here.
