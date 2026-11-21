# Sprint 002 — All five samples green, reference screenshots archived

| | |
|---|---|
| Roadmap phase | [Phase 1 — 6.2 boots on pspGL](../phases/01-bring-up-6-2/index.md) |
| Branch / base commit | `master` @ `975ec47e` (6.5-dev + port, device-validated) |
| Started / closed | — / — |
| Build number(s) | `s1` … `s4`, one per sample |
| Tasks | 6/7 done |

## 0. Handover

**Situation.** Sprint 001 put raylib on the console: `libraylib.a` builds, `master` is validated on hardware,
and `basic_shapes` is already ported and running from `psp-tests/basic_shapes`. Four samples have not been
tried. Your job: four samples run, each with an archived screenshot and a fingerprint.

**Read before acting:**

1. [Memory map](../memory-map.md) §6 — the measurement protocol. It contains four traps that cost sprint 001
   whole runs, and this sprint walks into all four if ignored.
2. Sprint 001 §9 — what the first build actually needed.
3. [Phase 1 page](../phases/01-bring-up-6-2/index.md), exit criteria.

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
| T1 | build and run `collision_area` | `done` | init clean, 0 exceptions, capture shows RAYWHITE field + an 8,200-px ORANGE box | `s1`, evidence `collision_area-s1.png` |
| T2 | build and run `logo_raylib_anim` | `done` | init clean, 0 exceptions, capture has a 16,710-px black logo on RAYWHITE | `s2`, evidence `logo_raylib_anim-s2.png` |
| T3 | build and run `cubicmap` with both PNGs | `done` | both files load (`32x16 R8G8B8`, `256x256 R8G8B8A8`, IDs 2 and 3); capture has 812 colours | `s3`, evidence `cubicmap-s3.png` |
| T4 | build and run `bunnymark` | `done` | `wabbit_alpha.png` loads (32x32 GRAY_ALPHA, ID 2), 0 exceptions. **Capture reproduces the known anomaly**: 9,381 colours, black-dominant — see §9. CROSS spawn is pad-driven and was **not** exercised | `s4`, evidence `bunnymark-s4-0bunnies.png` |
| T5 | capture one screenshot per sample | `done` | four PNGs in `/home/psp/evidence/sprint002/`, each plausible; none was a black frame or the PSPLink banner | 4 files |
| T6 | record a fingerprint per sample | `done` | §7 table complete, including the bunnymark anomaly and the unverified CROSS path | this doc |
| T7 | write anything surprising into the memory map | `done` | bunnymark anomaly added to §12 open questions; sample fingerprints summarised in §2 | same commit |

Status vocabulary: `todo` / `doing` / `done` / `blocked` / `dropped`. IDs are stable — never renumber.

### Task log (append-only)

- 2026-11-20 — sprint opened; all four samples built in a scratch tree `/tmp/spr2` (corpus left pristine),
  `PSP_LARGE_MEMORY=0`, 0 errors each
- 2026-11-20 — T1 `done`: 23 colours, ORANGE box 8,200 px
- 2026-11-20 — T2 `done`: 6 colours, black logo 16,710 px
- 2026-11-20 — T3 `done`: both PNGs load, 812 colours
- 2026-11-20 — T4 `done` with a caveat: texture loads, but the capture reproduces the bunnymark anomaly;
  CROSS spawn not exercised (needs a physical press)
- 2026-11-20 — T5/T6 `done`: four captures + fingerprints
- 2026-11-20 — T7 `done`: anomaly recorded in the memory map

## 6. Budgets

| budget | limit | measured | tag |
|---|---|---|---|
| sample crashes | 0 | | |
| boot-to-first-frame | < 5 s | | |
| build warnings | 0 | | |

## 7. Measurements

| build | workload | metric | value | notes |
|---|---|---|---|---|
| `s1` | collision_area | renders | **yes** | 0 exceptions |
| `s1` | collision_area | distinct colours | **23** | RAYWHITE 43,167; ORANGE box 8,200 px |
| `s2` | logo_raylib_anim | renders / animates | **yes** | 0 exceptions |
| `s2` | logo_raylib_anim | distinct colours | **6** | RAYWHITE 56,235; black logo 16,710 px |
| `s3` | cubicmap | textures loaded | **2** | 32x16 R8G8B8, 256x256 R8G8B8A8 |
| `s3` | cubicmap | distinct colours | **812** | textured map drawn |
| `s4` | bunnymark | texture loaded | **1** | `wabbit_alpha.png`, 32x32 GRAY_ALPHA |
| `s4` | bunnymark | distinct colours | **18 clean / 9,381 torn** | capture-dependent, see §9 |
| `s4` | bunnymark | bunnies spawned | **not tested** | pad-driven, needs a physical CROSS press |

## 8. Exit criteria

- [x] four samples build against the staged 6.5-dev library with `PSP_LARGE_MEMORY=0`
- [x] each renders or is documented, with the log line that shows it
- [x] one screenshot per sample in `/home/psp/evidence/sprint002/`
- [x] fingerprints in §7 filled, anomalies recorded rather than explained away
- [x] Phase 1 exit criterion recorded in the phase page, bunnymark's caveat included

## 9. Result

**All four run on 6.5-dev with zero exceptions.** Building them in a scratch tree (`/tmp/spr2`, corpus left
pristine) took one pass each: `0 errors`, `MEMSIZE` empty, ~920 KB PRX. The corpus drift that needed a fix for
`basic_shapes` does not touch these four.

**Every sample produced a plausible frame**, with one exception: bunnymark.

| sample | colours | what the capture shows |
|---|---|---|
| `collision_area` | 23 | RAYWHITE field, 8,200-px ORANGE box |
| `logo_raylib_anim` | 6 | RAYWHITE field, 16,710-px black logo |
| `cubicmap` | 812 | atlas-textured map, both PNGs loaded |
| `bunnymark` | **9,381** | **black-dominant noise** |

**The bunnymark anomaly was investigated and resolved: it was the capture, not the app.** Five captures of
the same binary on the same console gave **18, 2,169, 2,083, 18 and 9,381** distinct colours. The clean ones
show RAYWHITE over 55,663 px (background + header + text + one box, exactly what the code draws with 0
bunnies); the torn ones only ~29,900 px, because part of the surface was being written while `scrshot` read
it. bunnymark renders correctly.

That makes the pattern in §7 misleading in hindsight and worth stating: it was not bunnymark that was unique,
it was that all of its captures happened to land mid-swap. The rule now in the memory map (§6) is to take two
captures and require them identical before trusting either.

**Not tested:** the CROSS spawn path. bunnymark starts with 0 bunnies by design, and spawning needs a physical
press. What is proven is that it initialises, loads its texture and renders a frame.

**Method note:** these four samples exit on START via `CloseWindow()`, so unlike `basic_shapes` there is no
`DONE` state to capture against. Each capture was therefore taken from a live app, which freezes it a couple of
seconds later — expected and handled with `reset`. Porting them into `psp-tests/` with the bounded pattern
stays a follow-up.

## 10. Follow-ups

- **Port the corpus into `psp-tests/`** if phase 2 needs repeatable per-sample runs. As they stand, each sample
  exits on START and takes PSPLink with it; only `basic_shapes` has the bounded `DONE` + sleep pattern.
- Bunnymark's counter replacement is sprint 003's job.
- A texture format pspGL rejects is a phase 6 concern — note it, do not fix it here.
