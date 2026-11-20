# Phase 1 — 6.2 boots on pspGL

| | |
|---|---|
| State | not started |
| Depends on | Phase 0 |
| Exit criterion | all five samples render on the PSP from a 6.2 build, screenshots archived per sample |

## What this phase is about

Making raylib **6.2** draw on the console through the pspGL that pspdev already ships. No batching, no new
backend, no optimisation: the goal is a working baseline whose numbers mean something.

The whole PSP delta of the 5.5 port is small enough to carry over almost verbatim — one new platform file,
about fifteen additive lines in `rcore.c`, four guard lines in the renderer, and a Makefile block. The detail
of that delta is in [§2](../memory-map.md#2-memory-map-where-the-work-happens).

## What has to be true before starting

- [ ] Phase 0 exit criteria recorded
- [ ] `pspdev` toolchain and `pspsh`/`usbhostfs_pc` reachable
- [ ] The 5.5 reference build of `basic_shapes` runs, so there is something to compare against

## Tasks

- [ ] Import `src/platforms/rcore_psp.c` from `consoles/raylib4Consoles` as the first commit
- [ ] Implement the three missing interface functions: `GetClipboardImage` (empty image), `GetKeyName`
      (small static table), `SetGamepadVibration` (accept and ignore — no rumble on a PSP)
- [ ] Re-apply the renderer guards and `rcore.c` wiring, recording why each exists (including
      `SetTraceLogCallback(CustomLog)`, without which PSP logs go nowhere):
      `rlgl.h:2337` skips `glHint`, `rlgl.h:3730` skips `glGetTexImage`, `rshapes.c:1486` and `:1569` take PSP
      off `SUPPORT_QUADS_DRAW_MODE`; add the PSP platform include and the `clock_gettime` timer entry in
      `rcore.c`
- [ ] Add PSP plumbing to `src/Makefile` — **not** a copy of the 5.5 block: 6.2 knows no console platforms, and
      it sets `CFLAGS` centrally, so use `CUSTOM_CFLAGS` / `EXTRA_INCLUDE_PATHS` rather than overwriting
- [ ] Build `libraylib.a` at `-O2` and record the `-O` level actually used. `-O3` is a phase 3 question
- [ ] Stage the archive where samples resolve `-lraylib` (`$PSPDEV/psp/lib`), backing up pspdev's 5.5 copy and
      verifying sha256 both sides — otherwise the sample silently links the old library
- [ ] Run `basic_shapes`, `collision_area`, `logo_raylib_anim`, `cubicmap`, `bunnymark` over PSPLink
- [ ] Archive one screenshot per sample with its build number

## What we already know

- The 5.5 platform file implements **46 of 6.2's 49** platform functions; the gap is exactly the three above.
- 6.2's `src/Makefile` supports only desktop, web, Android, DRM and memory targets. The console platforms in
  the 5.5 fork are the fork's own additions, so the PSP plumbing has to be written against 6.2's structure.
- `llReadScreenPixels`/`LoadImageFromScreen` return a blank buffer on PSP, because `glGetTexImage` is guarded
  out. That is expected, not a bug to chase.
- PSPSDK's default `MEMSIZE=2` asks for a 40 MB partition on a 32 MB console: build with
  `PSP_LARGE_MEMORY=0`.
- The reference 5.5 build's `basic_shapes` ran with 284 distinct colours and raylib's own palette
  (`RAYWHITE`/`RED`/`BLUE`) — a cheap fingerprint to compare the 6.2 build against.

## Exit criteria

- [x] five samples boot and render on 6.5-dev: `basic_shapes` (284 colours), `collision_area` (23),
      `logo_raylib_anim` (6), `cubicmap` (812), `bunnymark` (renders; its earlier "noise" was a torn capture,
      proven by 18 vs 9,381 colours from the same binary — see the capture rules in the memory map §6)
- [ ] five samples boot, render and exit cleanly
- [ ] no import-stub warnings (`psp-fixup-imports` "stubs out of order" is a runtime bug, not cosmetic)
- [ ] screenshots archived, named with build number
- [ ] everything learned written back into [§2](../memory-map.md#2-memory-map-where-the-work-happens)

## Sprints and features

- [Sprint 001 — Import the platform layer, first build](../sprints/001-phase1-import-platform.md)
- [Sprint 002 — All five samples green, screenshots archived](../sprints/002-phase1-samples-green.md)
