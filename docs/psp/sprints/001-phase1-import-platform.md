# Sprint 001 (P1-S1) — Import the platform layer, first build

| | |
|---|---|
| Roadmap phase | [Phase 1 — 6.2 boots on pspGL](../phases/01-bring-up-6-2/index.md) |
| Sprint | P1-S1 |
| Branch / base commit | `psp-6.2` @ `02566453` + docs commits |
| Started / closed | 2026-11-20 / 2026-11-20 |
| Build number(s) | `r1` … `r7` (final: release library + `basic_shapes.prx`) |
| Tasks | 7/7 done |

## 0. Handover

**This sprint is closed.** It ran on 2026-11-20 and its results are below. What a reader needs from it now:

- **raylib 6.2 renders on a PSP-1000 through pspGL.** 600 frames, steady 30 fps, framebuffer matching the
  5.5 reference palette (284 distinct colours), clean exit, no exception.
- Three things in §9 are worth reading before sprint 002: the `SetupViewport`/`eglMakeCurrent` ordering bug,
  the measured GL gap (exactly one symbol), and the fact that the sample corpus is 5.5-era.
- One thing is still unverified: the **release** (`-O`, no `-g`) binary has not been run. Everything on the
  device so far used the debug build. One run settles it — see "Follow-ups".

**Situation at start.** `/home/psp/raylib-psp` held upstream raylib **6.2** on branch `psp-6.2` with no PSP
platform code and no console platform support in its Makefile. The task was to produce `libraylib.a` for PSP
and get one sample rendering on the console.

**Read before acting** (still valid for a re-run):

1. [Memory map](../memory-map.md) §2 (where the work happens), §3 (hardware limits), §9 (traps).
2. [Phase 1 page](../phases/01-bring-up-6-2/index.md).
3. The 5.5 platform file — the starting point, and the source of every 5.5→6.2 surprise:
   `git -C /home/psp/raylib-psp show consoles/raylib4Consoles:src/platforms/rcore_psp.c`

**Environment** (as it now stands, after the changes this sprint forced):

```sh
export PSPDEV=/home/psp/pspdev
export PATH=$PATH:$PSPDEV/bin
cd /home/psp/raylib-psp/src && make PLATFORM=PLATFORM_PSP      # library -> libraylib.a
cp libraylib.a $PSPDEV/psp/lib/libraylib.a                     # samples resolve -lraylib here
cd /home/psp/raylib-psp/psp-tests/basic_shapes && make          # PRX + EBOOT
( printf './basic_shapes.prx\n'; sleep 30; printf 'exit\n' ) | pspsh   # keeps logs alive
```

**Hard rules that were validated here** (all of them mattered):

- `PSP_LARGE_MEMORY=0`: the PSPSDK default asks for 40 MB on a 32 MB console.
- Zero "stubs out of order" warnings; do not add libraries `build.mak` already links.
- No GPL or unlicensed code into this zlib tree.
- Keep the shell attached for the whole run, or `TRACELOG` output is lost with `pspsh -e`.

**Do not touch.** The reference corpus in `raylib4Psp` (5.5-era, not ours); `CMakeLists.txt`.

## 1. Objective

`libraylib.a` for PSP builds from 6.2 sources with the imported platform file, and one sample renders on the
console through pspGL — with zero import-stub warnings and with proof that the sample linked *our* archive and
not the one pspdev shipped.

## 2. Why now

Nothing else can be measured until something runs. This sprint turns the tree from a copy of upstream into a
cross-compiled library, which is the precondition for every number in phases 2–11.

## 3. Scope

**In:** platform layer import, three interface implementations, renderer guards, `rcore.c` wiring, PSP
plumbing in `src/Makefile`, one sample built and run.

**Out:** batching, renderer optimisation, `CMakeLists.txt`, the other four samples (sprint 002), audio, input
changes, `-O` tuning.

## 4. Preconditions

- [x] Sprint 000 closed
- [x] `pspdev` on `PATH`; `psp-gcc 15.2.0`
- [x] PSPLink reachable (`054c:01c9`)
- [x] partition 2 clean at 25,149,440 bytes after `reset`
- [x] pspdev's shipped `libraylib.a` (1,146,230 B) and headers backed up to `/home/psp/libraylib-pspdev-5.5*.a.bak` and `/home/psp/libraylib-pspdev-5.5-headers/`

## 5. Tasks

| ID | task | status | verification | evidence |
|---|---|---|---|---|
| T1 | import `rcore_psp.c` alone | `done` | 730 lines, only that file in the commit | `f4659bfc` |
| T2 | implement `GetClipboardImage`, `GetKeyName`, `SetGamepadVibration` | `done` | signatures match `raylib.h` 1034/1209/1223, compiles clean | `5d8e9919` |
| T3 | renderer guards + `rcore.c` wiring | `done` | 8 sites across 3 files, compile clean | `f3cfd769`, `4a8f195b` |
| T4 | PSP plumbing in `src/Makefile` | `done` | `make PLATFORM=PLATFORM_PSP` produces `libraylib.a`; log shows `-DGRAPHICS_API_OPENGL_11` | `4fd7fb25` |
| T5 | stage library and headers | `done` | sha256 equal on both sides; 5.5 backups restorable | staged, `3ed042a4…` |
| T6 | build `basic_shapes`, 0 stub warnings | `done` | `MEMSIZE` empty, `mips:allegrex`, 0 "stubs out of order" | `48eedc50` |
| T7 | run on device | `done` | 600 frames, 30 fps, 284 colours, clean exit, no exception | `d0e9d9f0` |

Status vocabulary: `todo` / `doing` / `done` / `blocked` / `dropped`. IDs are stable — never renumber.

### Task log (append-only)

- 2026-11-20 — sprint opened, all tasks `todo`
- 2026-11-20 — T1 `done`, `f4659bfc`. Import was CRLF in a LF tree: normalised, `.gitattributes` pins it
- 2026-11-20 — T2 `doing` — imported file did not compile against 6.2 (3 API-drift issues, see §9)
- 2026-11-20 — T3 `doing`, then first library build: 0 errors but `rmodels.c` failed in `external/m3d.h`
- 2026-11-20 — T4 `doing` — added `-Wno-incompatible-pointer-types` (GCC 14+ default) and
  `-DSUPPORT_MODULE_RAUDIO=0` (miniaudio needs `dlfcn.h`)
- 2026-11-20 — T2/T3/T4 `done`; library builds with 0 errors, 0 warnings
- 2026-11-20 — T5 `done`: library staged, then headers too after noticing samples would compile against
  pspdev's 5.5 `raylib.h`
- 2026-11-20 — T6 `doing` — first link failed on `glPointSize`; measured the whole GL surface, it was the
  only missing symbol of 55. Sample corpus also failed to compile: `DrawCircleGradient` changed signature
- 2026-11-20 — T6 `done`; T7 `doing` — first device run: **bus error** at init (see §9)
- 2026-11-20 — T2 follow-up `e716d034` fixes the crash; second run: full init, 600 frames, clean exit
- 2026-11-20 — T7 `done`. `psplink.ini` set to `resetonexit=0` so a clean app exit stops killing PSPLink

## 6. Budgets

| budget | limit | measured | tag |
|---|---|---|---|
| library size | ~1.1 MB (pspdev 5.5: 1,146,230 B) | **1,718,210 B** release, 2,801,422 B with `-g` | `[measured]` |
| PRX size | ~920 KB (5.5 reference) | **1,313,386 B** | `[measured]` |
| build warnings | 0 | 0 | `[measured]` |
| stub warnings | 0 (hard gate) | 0 | `[measured]` |

Both size budgets were exceeded by ~50 %: 6.2 carries more code than 5.5 and the 5.5 package was built
differently. Not a blocker at 24 MB, but the sizes are real and recorded.

## 7. Measurements

| run | build | workload | metric | value | notes |
|---|---|---|---|---|---|
| reference | 5.5 | basic_shapes | distinct colours | 284 | `RAYWHITE` 46,916 / `RED` 2,700 / `BLUE` 1,576 |
| 6.2 | `r7` | basic_shapes | distinct colours | **284** | `RAYWHITE` 46,814 / `RED` 2,700 / `BLUE` 1,576 |
| 6.2 | `r7` | basic_shapes | frames rendered | **600** | bounded run, no exception |
| 6.2 | `r7` | basic_shapes | frame rate | **30 fps** | `SetTargetFPS(60)`; see follow-ups |
| 6.2 | `r7` | init | EGL context | success | pspGL 1.3, `OpenGL ES-CM 1.1` |

Evidence retained outside the tree: `/home/psp/evidence/sprint001/` (`frame-060.raw`/`.png`,
`psplink-banner.raw`/`.png`).

## 8. Exit criteria

- [x] `libraylib.a` builds with no stub warnings and no new warnings at `-Wall -Wextra`
- [x] staged archive byte-identical to the built one at the time of staging
- [x] `basic_shapes` renders on the PSP, matching the 5.5 reference palette
- [x] T1–T4 are separate commits
- [x] 5.5 backup restorable, path recorded
- [x] findings written back into the memory map and phase page
- [ ] **release (`-O`, no `-g`) binary run on device** — the only unmet line, see follow-ups

## 9. Result

**It works, and it took four changes that the plan did not predict.**

1. **`SetupViewport` is not `SetupFramebuffer`.** 6.2's function issues real GL calls (`rlViewport`,
   `rlOrtho`, `rlLoadIdentity`). The import called it before the EGL surface existed and before
   `eglMakeCurrent`; pspGL faulted with no current context:

   ```
   [PSP][INFO]PLATFORM: eglChooseConfig success.
   Exception - Bus error (data)
   EPC 0x088D14AC:  lw $s1, 2064($a0)  with a0 = 0
   ```

   Renaming the call preserved the 5.5 position but not its semantics. Fixed by moving it into the
   post-`eglMakeCurrent` block (`e716d034`). **This is the finding worth remembering**: a pure rename in a
   port is a behaviour change whenever the renamed function grew side effects.

2. **The GL gap is exactly one symbol.** Rather than fixing link errors one at a time, I diffed the `gl*`
   symbols our objects reference (55) against what `libGL.a` exports (178). The difference was `glPointSize`.
   Guarded like the other GL11 gaps.

3. **GCC 14+ made `incompatible-pointer-types` an error**, and `external/m3d.h` trips it. This is why the 5.5
   port carried `-Wno-incompatible-pointer-types`; it was not cargo cult.

4. **miniaudio needs `dlfcn.h`**, absent here, so `SUPPORT_MODULE_RAUDIO=0` for now. The 5.5 port shipped the
   same way (`raudio: not loaded`).

**Also discovered:** the sample corpus in `raylib4Psp` is 5.5-era and does not compile against 6.2
(`DrawCircleGradient(x, y, r, …)` is now `(Vector2, r, …)`). The smoke test therefore lives in
`psp-tests/basic_shapes/` with that one line fixed, rather than editing the reference corpus.

**Two mistakes of mine, both fixed:** T6 committed 5.2 MB of build outputs (untracked and ignored in
`d75886f6`), and the first device runs were unreadable because `pspsh -e` exits and takes the log with it
(hence the bounded, self-logging test in `d0e9d9f0`).

## 10. Follow-ups

- **Run the release binary.** Everything on the device so far used the `-g` library. One 10-second run
  confirms the shipped configuration; until then, phase 2 baselines would be measuring a debug build.
- **30 fps, not 60.** `SetTargetFPS(60)` reported a steady 30. Candidates: `eglSwapBuffers` pacing
  (pspGL double buffering), or `SwapScreenBuffer` having no vsync handling of its own. This is a phase 2
  measurement question, not a bug — but it must be understood before any frame-time work.
- **Corpus drift is sprint 002's first problem**, not its second: each 5.5-era sample may need the same kind
  of one-line porting, so budget for it.
- `CMakeLists.txt` still knows nothing about PSP.
- `LoadImageFromScreen` returns a blank buffer on PSP (guarded `glGetTexImage`); confirm which samples care.
