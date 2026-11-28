# Benchmarking pspgu against pspGL — procedure and landmines

For an agent or a person about to measure one GL backend against the other. The commands are exact; the landmines
are the ones that have actually produced wrong numbers here.

## The two backends

- **pspGL** — the toolchain's shipped `libGL.a`, built from a pinned fork (`tools/pspgl-build.sh`). It is the pixel
  reference: the recorded gate values come from it.
- **pspgu** — this project's own GL 1.1 implementation over `sceGu`/`sceGum` (`tools/pspgu/`). It is what the
  optimisation sprints measure.

Both are swapped in place at `$PSPDEV/psp/lib/libGL.a`. Nothing above the GL boundary knows which one is installed.

## Switching

**The benchmark pair** (installs the driver, rebuilds both samples, two runs each, restores pspGL on exit):

```sh
cd /home/psp/raylib-psp
PSP_DEVICE_APPROVED=1 ./tools/psp-bench.sh --driver pspgl --label pspgl-control
PSP_DEVICE_APPROVED=1 ./tools/psp-bench.sh --driver pspgu --label pspgu-arm \
    --baseline psp-tests/bench/results/pspgl-control.json
```

**One sample or one gate:**

```sh
./tools/pspgu-build.sh                  # install pspgu
(cd psp-tests/batch_check && rm -f *.o *.elf *.prx && make)
cp psp-tests/batch_check/batch_check.prx /tmp/s3/
# run over PSPLink, capture, count
./tools/pspgu-build.sh --restore        # back to pspGL
```

For the pspGL arm use `./tools/pspgl-build.sh` (no arguments installs), and `./tools/pspgu-build.sh --restore` to
return.

**Checking what is installed** — do this before and after any comparison:

```sh
./tools/pspgu-build.sh --check    # "installed libGL.a is a pspgu build" + content hash + pspGL object count
./tools/pspgl-build.sh  --check   # pinned commit + content hash
```

At runtime the boot line settles it: pspgu prints `GU up: pspgu bNN, …`; pspGL prints its own banner.

## Landmines

Each one has produced a wrong result here at least once.

1. **Stale objects after a driver swap.** `libGL.a` is linked statically into every PRX, so a sample built before the
   swap keeps the old backend. *Symptom:* identical numbers for "both" drivers, or a boot line naming the wrong one.
   *Fix:* `rm -f *.o *.elf *.prx` before every sample build. This has bitten twice (sprint 079 §9, and the `ge_probe`
   `main.o`), and `make clean` is not always enough — remove the objects explicitly.
2. **The bench script restores pspGL when it exits** unless `--keep-driver` is passed. An agent that runs the bench
   and then a manual test is measuring pspGL.
3. **The pspGL backup slot can be polluted.** `libGL.a.pspgl-backup` once held a pspgu build, so `--restore`
   silently restored the wrong backend; the script now tests the archive's symbols. If `--check` reports neither
   backend, restore the shipped archive: `cp /home/psp/libGL-pspdev-shipped.a.bak $PSPDEV/psp/lib/libGL.a`.
4. **`work` and `swap` are not comparable across backends.** `work = avg − swap`, and `swap` counts whatever runs
   inside the wrapped `eglSwapBuffers`. pspgu submits from there, so a sync lands in `swap`; pspGL may place its
   wait elsewhere. *Use `avg` for the driver comparison*; compare `work` only between arms of the *same* backend.
5. **The backends have different feature sets on purpose.** pspgu has no CLUT upload path, has its own
   render-to-texture and state scoping; pspGL has its own gaps. A pixel difference is a finding, not automatically a
   pspgu bug — check which side is missing the feature before "fixing" anything.
6. **pspgu build flags have no pspGL counterpart.** `PSPGU_SUBMIT`, `PSPGU_LIST_VERTICES`, `PSP_GU_DEPTH`,
   `PSPGU_TEXFMT`, `PSPGU_REPLAY` exist only in `tools/pspgu/`. An arm using them cannot be compared to pspGL; it is
   a pspgu-internal A/B.
7. **Frame times quantise to the vblank.** Values land on ≈16.67, 33.3, 50 ms. Two cells both at the 60 fps cap
   (16.756 vs 16.757, sprint 089) separate nothing. Pick a scene *above* the cap before claiming a difference.

   **A scene just below a step measures the step, not the change.** Measured 2026-11-28 on the upstream merge: the
   library's CPU work differed by ≤2 % (31.95 → 32.62 ms at 400 cubes) and the average went **33.5 → 40.4 ms
   (+20 %)**, because the frame crossed the 33.3 ms step and ~40 % of frames took 50 ms instead. The same two
   libraries at **64 cubes are identical** (16.716 vs 16.718 ms; work 6.30 vs 6.49). At GRID=14 master alternated
   16.7/33.3 ms while the merge was pinned at 33.3 — a 0.2 ms difference deciding every frame.

   So: for an A/B decision, run the scene **well below** the next step, or compare `work` (unquantised) rather than
   `avg`. A ceiling sweep (last N at 60 fps) is a step measurement and is fine; an A/B at a fixed N near a step is
   not. If `swap` jumps by ~16 ms while `work` moves by <1 ms, you are looking at the cliff, not at a regression.
8. **List overflow is silent.** `sceGuGetMemory` and the list have no bounds check; an overrun writes past the
   buffer and the GE executes garbage. *Symptom:* a hang, a garbled frame, or a missing `STAT` line. *Fix:* grep the
   log for `LIST OVERFLOW` before blaming the hardware, and size command headroom from a measurement, not a guess
   (16 KiB was too small; the 3D command stream is ≈27 KiB).
9. **A missing `STAT` line is a failed run, not a fast one.** Check for `STAT n=…` and `SPLIT n=…`; their absence
   means a crash, an overflow or a hung console.
10. **Reset discipline.** The scripts reset and sleep ≈6 s between runs. A manual run after a hang inherits a dead
    console; reset first. Only one session should own PSPLink during a comparison — a second one mid-run produces a
    sample with no `STAT` rather than a wrong number, but it wastes the run.
11. **One run is not a result.** Two runs per point, 2 % tolerance; the script warns when they disagree. When
    comparing variants, keep a control and a control-again, or interleave them.
12. **Emulator numbers are fiction for time.** PPSSPP answers "does it draw", not "how long". Every number in these
    docs came from hardware over PSPLink.
13. **Gates need pspGL baselines.** `batch_check` 613 colours, `basic_shapes` 284, `scene_3d` 0 diff, RTT, clip —
    the expected values are pspGL's, so the gate is parity, not self-consistency.
14. **Record the hashes.** The bench JSON carries `driver_hash` and `lib_hash`. A number without them cannot be
    attributed later; a driver whose hash changed for a log-only edit is still a different archive.
15. **End restored.** Unless the session's owner wants pspgu left installed, finish with
    `./tools/pspgu-build.sh --restore` and state the final hash.

## Before / after checklist

- [ ] `PSP_DEVICE_APPROVED=1`, `usbhostfs_pc` running with `HOSTFS=/tmp/s3`
- [ ] `--check` says which backend is installed, and it is the one you intend
- [ ] every sample rebuilt with `rm -f *.o *.elf *.prx`
- [ ] boot line names the expected backend
- [ ] `STAT`/`SPLIT` present; no `LIST OVERFLOW`
- [ ] two runs within 2 %
- [ ] driver and library hashes recorded with the numbers
- [ ] pspGL restored at the end (or the choice stated)

## Expected values, so a broken baseline is obvious

| measure | expected | source |
|---|---|---|
| 3D `psp_showcase` | **40.3 ms, work 32.6** after the 2026-11-28 upstream merge; 33.5 before it — same work, different side of the 33.3 ms step (landmine 7) | measured 2026-11-28 |
| 2D `psp_2d_showcase` | 33.803 ms, work 21.590, swap 12.213 | sprint 085 §9.9 |
| `batch_check` | 613 distinct 5-6-5 colours | sprints 085, 088 |
| `basic_shapes` | 284 colours | sprint 014 |
| pspgu hash (current) | `0a65c7a8…` | `tools/pspgu-build.sh --check` |
| `libraylib.a` | `c2004b266a6c3291` | bench JSON |

A run far from these, on the same scene and library, is a broken run before it is a discovery.
