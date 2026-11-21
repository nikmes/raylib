# Environment, toolchain and device

Operational facts for this machine and this console: what is installed, where, and how a build reaches the
hardware. Design facts live in the [memory map](memory-map.md) (§3 hardware limits, §10 compute units) and the
reference material index is §11 there — this page does not repeat them.

Last verified: 2026-11-20.

## Host

| item | value |
|---|---|
| OS | Ubuntu 24.04.4 LTS, x86_64, 20 cores |
| pspdev | `/home/psp/pspdev`, release **v20261101** |
| toolchain tarball | `pspdev-ubuntu-latest-x86_64.tar.gz`, sha256 `5dc9d785a5fd490ee9d5204b90f715efb6c46940ba3944036bbef9c993c17713` (matches the published release digest) |
| compiler | `psp-gcc 15.2.0` |
| PSPSDK | `/home/psp/pspdev/psp/sdk` |
| exports | `PSPDEV` and `PATH` in `~/.bashrc`; the archive also carries `usbhostfs_pc` and `pspsh` |
| PPSSPP | **not installed** — it answers "does it render", never "how fast" or "how much" |
| docs venv | `/home/psp/mkdocs-venv` (MkDocs 1.6.1 + Material 9.8.2), nothing installed system-wide |

```sh
export PSPDEV=/home/psp/pspdev
export PATH=$PATH:$PSPDEV/bin
```

## Device

- **PSP-1000**, 32 MB. Partition 2 is 25,165,824 B total and **25,149,440 B free** with PSPLink idle.
- Firmware **6.60**: `sceKernelDevkitVersion()` returns `0x06060010`.
- **ARK-family custom firmware**, established from export evidence rather than a version string: the
  `SystemControl` module (kernel, attr 2007) exports `syslib`, `KUBridge` and `SystemCtrlForUser` — the
  PRO/ARK lineage. `sctrlHoroscope` is resident, and `flash0:/kd/horoscope.prx` (3,248 B) is the only
  non-stock file found in `flash0:/kd`.
- The exact ARK version is **not readable** from the host or from PSPLink: no `SETTINGS.TXT` on the stick,
  `ms0:/PSP/SYSTEM` empty, no version-bearing module name among the 50 loaded, and `strings` on
  `horoscope.prx` yields only `sctrlHoroscope`. `modexp` prints NIDs, not names. Read it from the console's
  own CFW menu rather than guessing.
- Because `ms0:/PSP/SYSTEM` is empty, ARK reads its settings from the **flash copy**, not the stick. Harmless
  on a 32 MB model (high memory is never granted on a PSP-1000); it is the same fallback that silently gives
  a 64 MB console 24 MB when a stick mounts slowly.
- `satellite.prx` in `flash0:/vsh/module` is stock Sony (`~PSP`, `VshCtrlSatelite`), not an ARK file.
- No rumble motors, no analog triggers, one nub (raw 0–255 per axis).

## Memory facts (measured on this console)

- **`PSP_HEAP_SIZE_KB` is honoured.** `PSP_HEAP_SIZE_KB(1024)` costs 1,465,856 B off the partition:
  1 MB heap + 256 KB main-thread stack + 4 KB thread. Measured `entry free=23,683,584 max=23,421,440`.
- **With the auto heap the program sees ~768 KB free** (782,336 / 786,432 measured) even on a fresh console:
  libcglue takes `largest − 512 KB` at module init, *before* `main`. Moving the counter call to the top of
  `main` changes nothing. Declare the heap instead of chasing the read point.
- **`kill` in pspsh leaks the module's heap.** After killing one auto-heap module the partition stayed at
  1,202,944 instead of returning to 25,149,440. `reset` restores it exactly. Reset before any measurement.
- `mallinfo()` returns `arena=0 fordblks=0` here — unusable as a heap probe. `modinfo` works and gives the
  load address and segment size.

## Getting a build onto the console

**Memory stick (USB mass storage).** Device enumerates as `054c:01c8`; `1.9 GB` vfat on `/dev/sda1`.

```sh
sudo mount -o uid=1003,gid=1003,umask=000 /dev/sda1 /home/psp/psp-stick
```

`uid`/`umask` are what make the mount writable by uid 1003 — without them it is owned by root and deploys
fail. `psp` has `(ALL : ALL) NOPASSWD: ALL` (check with `sudo -n -l`; a single `sudo -n true` probe was
misleading once). Stick contains `MEMSTICK.IND` and `PSP/GAME/{ray,runtime2d}`. Always `sync`, verify both
sides of a copy, and `umount` before unplugging: an interrupted FAT copy costs a corrupt stick and can cost a
silent fallback to the small memory partition.

`hello-psp/deploy.sh` does copy → `sync` → sha256 both sides; it refuses to run if `PSP_MS_ROOT` is not a
mount point.

**PSPLink (USB).** Device enumerates as `054c:01c9`; the udev rule in `/etc/udev/rules.d/50-psplink.rules`
gives `MODE="0666"` and a `/dev/psp` symlink. `ms0:/PSP/GAME/psplink/psplink.ini` has `resetonexit=0`.

## PSPLink workflow

This is the version that works. Sprint 001 paid for every line.

```sh
# start in the directory you want as host0:
usbhostfs_pc &

pspsh -e "reset"                        # clean partition, no leaked heap

# run, keeping the log alive; NEVER send 'exit'
( printf './app.prx\n'; sleep 30 ) | pspsh

# capture only once the app has stopped drawing (see below)
pspsh -e "scrshot host0:/shot.raw"

# next run
pspsh -e "reset"
```

- **`exit` is PSPLink's shutdown command**, not a shell logout. Sending it ends the session and drops the
  console to the XMB; the USB device disappears. Let the pipe close on EOF instead — pspsh disconnects and
  PSPLink stays. `pspsh -e "cmd"` is safe: it never sends `exit`.
- **Keep the shell attached for the whole run.** `pspsh -e "./app.prx"` returns immediately and the app's
  `TRACELOG` output is lost; a crash then looks exactly like a hang.
- **The app must not call `sceKernelExitGame`.** `resetonexit=0` only suppresses the reset loop (the log line
  becomes `sceKernelExitGame caught!` instead of `Resetting psplink`); the session still ends. Tests log
  `DONE frames N` and call `sceKernelSleepThread()`, staying resident, so the host keeps control.
- **Do not `scrshot` while the app is rendering** — it freezes the app a couple of seconds later. Verified:
  two captures 2 s apart were pixel-identical on an animating demo, and the frame log stopped at the capture.
  Capture after `DONE`, when the app is asleep and the framebuffer holds the last frame.
- `reset`, never `kill`, between runs.
- If PSPLink does go away, relaunch it on the console: **Game → Memory Stick → PSPLink**.
- Verified end to end: two consecutive `run → DONE → reset → run` cycles both logged `DONE frames 600  fps 30`
  with PSPLink enumerated throughout.

### Driver switching and capture (sprint 014 corrections)

- `tools/pspgu-build.sh` installs pspgu over `$PSPDEV/psp/lib/libGL.a`; `--restore` puts pspGL back, and
  `--check` reports which is installed. Leave **pspGL** installed when you stop, so the toolchain is in its
  shipped state; `tools/pspgl-build.sh --check` must report content hash `2ab7378e`.
- The backup test was broken until sprint 014: it piped `grep -c pspguBeginFrame` (output `0`/`1`) into
  `grep -q pspgu`, so every pspgu install overwrote the pspGL backup with pspgu. An A/B could then measure
  pspgu against itself. `--restore` now refuses an archive containing `pspguBeginFrame`; rebuild pspGL from
  its pinned commit with `tools/pspgl-build.sh`.
- **Check the build marker before trusting a run.** The boot line is
  `[PSPGU] GU up: pspgu bNN, ...`. If it is not the build you just made, the archive is stale:
  `tools/pspgu-build.sh --verify-install` compares the installed object bytes with a fresh build. The content
  hash in `--check` covers symbols only, so it does **not** change when code changes.
- **`scrshot` writes its BMP in BGR byte order.** When comparing it with a `glReadPixels` PNG of the same
  frame, swap the capture's channels first; the two then agree within one 5-6-5 step (measured max 7).
- **`glReadPixels` reads the last completed frame** when called after `EndDrawing`, which is the frame the
  display received. Called while a list is open, it finishes and waits for that list and reads the current
  draw buffer. It is a diagnostic path, not a per-frame call.
- The display uses **three framebuffers** and switches with `PSP_DISPLAY_SETBUF_NEXTHSYNC` after the vblank
  wait. `NEXTFRAME` delays the switch by a full vblank, which makes the display read the buffer the next frame
  is drawing: the symptom is a torn horizontal band, visible only while the app is moving.

## Building

```sh
cd /home/psp/raylib-psp/src && make PLATFORM=PLATFORM_PSP     # -> libraylib.a
cp libraylib.a $PSPDEV/psp/lib/libraylib.a                    # samples resolve -lraylib here
for h in raylib.h rlgl.h raymath.h rcamera.h; do cp src/$h $PSPDEV/psp/include/$h; done
```

- Stage **headers as well as the archive**: samples compile against `$PSPDEV/psp/include`, so an unstaged
  header silently builds a 5.5-era app against a 6.x library.
- Apps pass `PSP_LARGE_MEMORY=0`; the PSPSDK default writes `MEMSIZE=2`, a 40 MB request on a 32 MB console.
- Backups of what pspdev shipped, restorable: `/home/psp/libraylib-pspdev-5.5.a.bak`,
  `/home/psp/libraylib-pspdev-5.5-headers/`, and the 6.2-validated archive
  `/home/psp/libraylib-psp62-validated.a.bak`.
- Staged at the time of writing: the merged 6.5-dev build, `sha256 13506883…`, headers
  `RAYLIB_VERSION "6.5-dev"`.
- **pspGL is no longer what pspdev shipped.** Since sprint 006 the toolchain carries
  `TheMrIron2/pspgl` @ `ae9a668` built with `PSPGL_DLIST_SIZE_WORDS=8192` — worth 31.8 % on 2D sprite work.
  Rebuild it with `tools/pspgl-build.sh`, check it with `--check` (content hash `2ab7378e…`; the archive's own
  sha256 changes every build because two objects embed `__DATE__`). Revert with
  `cp /home/psp/libGL-pspdev-shipped.a.bak $PSPDEV/psp/lib/libGL.a`. A pspdev package update will overwrite it —
  `--check` is how that is noticed.
- Test app: `psp-tests/basic_shapes` (bounded 600 frames, logs `DONE`, sleeps).

### Build switches for the batched path (sprint 007)

| switch | effect |
|---|---|
| `PSP_OPT` | optimisation level, **default `-O2`**. Empty or `-O0` reproduces the sprint 001–007 measurements — those were all taken with an unoptimised library, because the Makefile's `RELEASE` block covers desktop/web/android only. `-O2` alone took 1,000 sprites from 33.37 to 16.68 ms |
| `PSP_BATCH=1` / `0` | **`1` is the default since sprint 009**: one static 4,096-vertex buffer, one `glDrawArrays` per texture run, 2-vertex sprites for axis-aligned quads. `0` is the immediate GL11 path — one `glBegin`/`glEnd` per primitive — kept as the reference arm |
| `PSP_BATCH_NO_SPRITES=1` | forces the two-triangle quad path inside the batch, so the `GL_SPRITES_PSP` primitive can be A/B'd at one optimisation level |
| `PSP_BATCH_TRACE=1` | logs every `rlPspBatchFlush` (texture, mode, vertex count, sprites emitted) and every deferred texture switch — the only way to see what a wrong frame actually submitted |
| `PSP_BATCH_LOCK=1` | wraps each batched draw in `glLockArraysEXT`/`glUnlockArraysEXT`. Measured: no effect on pspGL `ae9a668` (sprint 007 T5) |

```sh
make PLATFORM=PLATFORM_PSP                                    # batched, -O2, 2-vertex sprites (default)
make PLATFORM=PLATFORM_PSP PSP_BATCH=1 PSP_BATCH_NO_SPRITES=1 # batched, triangles only
make PLATFORM=PLATFORM_PSP PSP_BATCH=0                        # reference library (immediate mode)
make PLATFORM=PLATFORM_PSP PSP_OPT=-O0                        # reproduce the pre-sprint-008 numbers
```

`PSP_BATCH=1` is a **2D** mode: `rlNormal3f` is dropped, so lit 3D is unaffected only because the PSP build
lights nothing. Do not default it on before the samples with 3D content have been compared.

## Documentation site

`raylib-psp-docs.service`, `enabled` via `multi-user.target.wants`, runs as `psp` from the repo and serves
**http://0.0.0.0:8100** (LAN `http://192.168.0.147:8100`). Ports 8000/8080 belong to `docker-proxy`;
10000–10007 belong to usbhostfs/PSPLink.

```sh
sudo systemctl restart raylib-psp-docs
journalctl -u raylib-psp-docs -f
/home/psp/mkdocs-venv/bin/mkdocs build --strict     # what CI would run
```

- The source is `docs/psp/memory-map.md` inside the repo; `PSP_MEMORY_MAP.md` at the repo root is a symlink.
  Documentation must stay inside `docs/` or `mkdocs serve` will not watch it.
- First start failed with `status=226/NAMESPACE`: `ReadWritePaths` named `.cache`, which did not exist, and
  systemd cannot bind-mount a missing path. Keep that directory present.
- Hardening: `NoNewPrivileges`, `PrivateTmp`, `ProtectSystem=strict`, `ProtectHome=read-only`, writes limited
  to `site/` and `.cache/`.
- MkDocs reserves `/templates/` at the docs root (`_default_exclude = ['.*', '/templates/']`); files there are
  collected but built as "excluded", and `--strict` does not fail on it. Templates live under `psp/templates/`.

## Evidence

Device evidence is kept outside the tree, never committed:

| path | contents |
|---|---|
| `/home/psp/evidence/sprint001/` | `frame-060.raw`/`.png` — basic_shapes on 6.2, 284 colours; `psplink-banner.raw`/`.png` — the black screen that was PSPLink, not the app |
| `/home/psp/evidence/sprint002/` | created by sprint 002 |

## Known unknowns

- **Target frame rate is not stated.** Observed: a steady **30 fps** with `SetTargetFPS(60)` on a demo that
  draws ~12 shapes. Whether that is pspGL's `eglSwapBuffers` pacing or our `SwapScreenBuffer` lacking vsync
  handling is unresolved — a phase 2 question.
- PPSSPP is not installed; if it is wanted for yes/no checks, it is an AppImage from ppsspp.org.
