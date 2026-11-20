# Sprint 001 — Import the platform layer, first build

| | |
|---|---|
| Roadmap phase | [Phase 1 — 6.2 boots on pspGL](../phases/01-bring-up-6-2.md) |
| Branch / base commit | `psp-6.2` @ `02566453` |
| Started / closed | — / — |
| Build number(s) | `r1`, `r2` (first two raylib PSP builds) |
| Tasks | 0/7 done |

## 0. Handover

**Situation.** `/home/psp/raylib-psp` holds upstream raylib **6.2** on branch `psp-6.2`, with no PSP platform
code and no console platform support in its Makefile. A previous port exists in the `consoles` remote at 5.5.
Your job: produce `libraylib.a` for PSP and get `basic_shapes` rendering on the console through pspGL.

**Read before acting**, in this order:

1. [Memory map](../memory-map.md) §2 (where the work happens), §3 (hardware limits), §9 (traps).
2. [Phase 1 page](../phases/01-bring-up-6-2.md) — the full task list and exit criteria.
3. The 5.5 platform file, which is your starting point:
   `git -C /home/psp/raylib-psp show consoles/raylib4Consoles:src/platforms/rcore_psp.c`
4. The 5.5 `src/Makefile`, which shows what the PSP build needs — **but see T4, its structure does not port
   verbatim**: `git -C /home/psp/raylib-psp show consoles/raylib4Consoles:src/Makefile`
5. The example corpus, outside this tree: `/home/psp/raylib4Psp/samples/` (its `Makefile` shows the link line).

**First action.** T1: import the platform file alone, commit alone. Do not mix it with the guards or the
Makefile work — a bisect should be able to point at one decision.

**Hard rules.**

- Build with `PSP_LARGE_MEMORY=0`. The PSPSDK default writes `MEMSIZE=2`, a 40 MB request on a 32 MB console.
- **Zero** "stubs out of order" warnings from `psp-fixup-imports`. Do not add libraries to `LIBS` that
  `build.mak` already links — it splits import stubs and is a runtime bug, not a cosmetic warning.
- This tree is zlib-licensed and upstream-mergeable. No code from GPL sources (`ref/PGE`, `ref/openTRI`),
  from the unlicensed tutorial repo, or from the Sony SDK enters it.
- Desktop behaviour must not change: guard PSP-only paths with `PLATFORM_PSP`, and prefer 6.2's append-style
  variables over overwriting anything the desktop build depends on.
- **A sample that links `-lraylib` gets `$PSPDEV/psp/lib/libraylib.a`, not your build** (see T5). If you skip
  the staging step you will measure pspdev's 5.5 library and not notice.
- One task, one commit, message carrying the task ID.

**Known traps, verified against this tree.**

- 6.2's `src/Makefile` knows **no console platforms** — only desktop, web, Android, DRM, memory. The 5.5 fork
  added DREAMCAST/PSP/VITA itself. There is nothing to copy wholesale; you are adding the plumbing.
- 6.2 sets `CFLAGS = ...` centrally (line 383) and appends `CFLAGS += $(CUSTOM_CFLAGS)` (539). The 5.5 block
  *overwrites* `CFLAGS`, which would clobber `-D$(GRAPHICS)` and friends. Use the append variables.
- 6.2's `install` target **cannot** be used for this: it requires `ROOT=root` and `PLATFORM_OS=LINUX` and copies
  to `RAYLIB_INSTALL_PATH`. Stage the archive by hand.
- `clock_gettime` is provided by pspdev's `libcglue.a`, so adding PSP to the timer path works. `nanosleep`
  (used by `WaitTime`, rcore.c:1670) is a different story — do not add PSP there.

**Commands.**

```sh
export PSPDEV=/home/psp/pspdev
export PATH=$PATH:$PSPDEV/bin

# library
cd /home/psp/raylib-psp/src && make PLATFORM=PLATFORM_PSP

# stage it where samples look for it (backup first, see T5)
cp $PSPDEV/psp/lib/libraylib.a /home/psp/libraylib-pspdev-5.5.a.bak
cp /home/psp/raylib-psp/src/libraylib.a $PSPDEV/psp/lib/libraylib.a

# sample
cd /home/psp/raylib4Psp/samples/shapes/basic_shapes && make PSP_LARGE_MEMORY=0
pspsh -e "reset"                     # before any measurement
usbhostfs_pc &                       # in the sample directory
pspsh -e "./raylib.prx"
```

**Do not touch.** `rlgl.h` beyond the two documented guards; `rcore.c` beyond the platform include, the timer
list and the backend log line; anything in `raylib4Psp` (reference corpus, not ours); `CMakeLists.txt`.

**Done means** §8 below, with the task table filled and evidence in each row.

## 1. Objective

`libraylib.a` for PSP builds from 6.2 sources with the imported platform file, and one sample
(`basic_shapes`) renders on the console through pspGL — with zero import-stub warnings in the build log, and
with proof that the sample linked *our* archive and not the one pspdev shipped.

## 2. Why now

Nothing else can be measured until something runs. This sprint turns the tree from a copy of upstream into a
cross-compiled library, which is the precondition for every number in phases 2–11.

## 3. Scope

**In:** `src/platforms/rcore_psp.c` import, three interface implementations, four renderer guards plus the
`rcore.c` platform include and timer entry, PSP plumbing in `src/Makefile`, one sample built and run.

**Out:** batching, any renderer optimisation, `CMakeLists.txt` support, the other four samples (sprint 002),
audio, input changes beyond what the imported file already does, `-O` optimisation work.

## 4. Preconditions

- [ ] Sprint 000 closed (branch and remotes verified)
- [ ] `pspdev` on `PATH`; `psp-gcc --version` answers 15.2.0
- [ ] PSPLink reachable: `lsusb` shows `054c:01c9`, `pspsh -e "ver"` answers 3.2.1
- [ ] `pspsh -e "reset"` done, partition 2 reports 25,149,440 free
- [ ] pspdev's shipped `libraylib.a` archived with its sha256 (`1,146,230` bytes at v20261101), restorable
- [ ] The 5.5 reference `basic_shapes` build archived with its sha256, to compare against

## 5. Tasks

| ID | task | status | verification | evidence |
|---|---|---|---|---|
| T1 | `git checkout consoles/raylib4Consoles -- src/platforms/rcore_psp.c`, commit alone | `todo` | file is 730 lines; commit touches nothing else; first build may not need edits | — |
| T2 | implement the three functions the 5.5 file lacks: `Image GetClipboardImage(void)`, `const char *GetKeyName(int key)`, `void SetGamepadVibration(int gamepad, float leftMotor, float rightMotor, float duration)` | `todo` | signatures match `raylib.h` (1034, 1209, 1223) and `rcore_template.c` (291, 407, 389); compiles clean | — |
| T3 | renderer guards and `rcore.c` wiring: `rlgl.h:2337` skip `glHint`, `rlgl.h:3730` skip `glGetTexImage`, `rshapes.c:1486` and `:1569` add `&& !defined(PLATFORM_PSP)` to `SUPPORT_QUADS_DRAW_MODE`; `rcore.c`: `#elif defined(PLATFORM_PSP)` before the `#else` at 533, PSP in the `clock_gettime` list at 4312 **only**, and `SetTraceLogCallback(CustomLog)` at the top of `InitWindow` (585) so PSP logs work at all | `todo` | `git show --stat` = 3 files, 7 sites; each site commented with why | — |
| T4 | `src/Makefile`: add PSP following 6.2's structure — platform name, `PLATFORM_OS`, `GRAPHICS ?= GRAPHICS_API_OPENGL_11`, `CC = psp-gcc`, `AR = psp-ar`, `CUSTOM_CFLAGS += -nostartfiles -D__PSP__`, `EXTRA_INCLUDE_PATHS += -I$(PSPDEV)/psp/include -I$(PSPDEV)/psp/sdk/include` | `todo` | `make PLATFORM=PLATFORM_PSP` produces `libraylib.a`; build log shows `psp-gcc`, `-DGRAPHICS_API_OPENGL_11`, and the `-O` level you intended | — |
| T5 | stage the archive where samples resolve `-lraylib`, backing up pspdev's 5.5 first | `todo` | `sha256sum $PSPDEV/psp/lib/libraylib.a` equals `sha256sum /home/psp/raylib-psp/src/libraylib.a`; backup restorable | — |
| T6 | build `basic_shapes` with `PSP_LARGE_MEMORY=0`, confirm it linked ours | `todo` | `psp-objdump -f` says `mips:allegrex`; `PARAM.SFO` has empty `MEMSIZE`; 0 "stubs out of order" in the log; PRX ~920 KB | — |
| T7 | load over PSPLink and look at the screen | `todo` | `pspsh` load line names the module; framebuffer has raylib's palette (~284 distinct colours) → expected: same shapes as the 5.5 reference | — |

Status vocabulary: `todo` / `doing` / `done` / `blocked` / `dropped`. IDs are stable — never
renumber; a task that no longer applies is `dropped` with the reason in the log below.

### Task log (append-only)

- _(not started)_

## 6. Budgets

| budget | limit | measured | tag |
|---|---|---|---|
| library size | ~1.1 MB unstripped (pspdev 5.5: 1,146,230 B) | | |
| PRX size | ~920 KB (reference) | | |
| build warnings | 0 | | |
| stub warnings | 0 (hard gate) | | |

## 7. Measurements

| run | build | workload | metric | value | notes |
|---|---|---|---|---|---|
| reference | 5.5 | basic_shapes | distinct colours | 284 | `RAYWHITE`/`RED`/`BLUE` dominant |
| 6.2 | `r1` | basic_shapes | distinct colours | | |

## 8. Exit criteria

- [ ] `libraylib.a` builds with no stub warnings and no new warnings at `-Wall -Wextra`
- [ ] the staged archive is byte-identical to the one just built (sha256 both sides)
- [ ] `basic_shapes` renders on the PSP, visually equal to the 5.5 reference
- [ ] T1–T4 are separate commits, so a bisect points at one decision
- [ ] the 5.5 archive backup is restorable, and the restore command is written down
- [ ] anything surprising written back into [§2](../memory-map.md#2-memory-map-where-the-work-happens)

## 9. Result

(To fill in on close. If an interface function turned out to need real behaviour rather than a stub, say so
here — that is the sort of thing the next sprint depends on. If T4's plumbing needed more than the variables
listed, record which ones: 6.2's Makefile is the part most likely to surprise the next person.)

## 10. Follow-ups

- `CMakeLists.txt` is untouched, so the CMake build does not know about PSP. Own task if CI or a contributor
  needs it.
- `LoadImageFromScreen` returns a blank buffer on PSP (guarded `glGetTexImage`). Confirm which samples that
  affects in sprint 002.
- Restoring pspdev's 5.5 archive is a manual copy. If sprint 002 needs to A/B against it, script the swap
  rather than doing it by hand twice.
