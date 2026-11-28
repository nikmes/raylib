# Benchmarking the port

> Comparing **pspgu against pspGL** (as opposed to two builds of one backend) has its own procedure and traps:
> see [backend AB: pspgu vs pspGL](backend-ab-pspgu-vs-pspgl.md).

Two demos exist to measure the renderer as a whole, one per pipeline, and one script runs both and records the
numbers so a change can be judged against the build before it.

## The pair

| Demo | Path | What it loads |
|---|---|---|
| `psp_showcase` (3D) | `psp-tests/samples/psp/psp_showcase` | 400 lit cubes in two merged colour runs over a fogged ground: the per-call path, merging, the clip classifier, fixed-function lighting/fog |
| `psp_2d_showcase` (2D) | `psp-tests/samples/psp/psp_2d_showcase` | ~1,566 additive sprites in 4 texture runs: the batcher, fill rate, texture switching |

Both drive their animation from the **frame index**, not from wall time, so frame N draws exactly the same scene on
every build. That is what makes a before/after comparison a measurement rather than a coincidence.

## Running it

```sh
PSP_DEVICE_APPROVED=1 ./tools/psp-bench.sh --label before
# ... make a change, rebuild the library, install it ...
PSP_DEVICE_APPROVED=1 ./tools/psp-bench.sh --label after --baseline psp-tests/bench/results/before.json
```

The script:

1. selects the driver (`--driver pspgu|pspgl`) and restores pspGL at the end unless `--keep-driver`;
2. optionally rebuilds `libraylib.a` with `--rebuild --batch 0|1 --merge 0|1` (without `--rebuild` it uses the
   installed library and records its hash);
3. builds each demo with `FRAMES=` set, copies the PRX to the host filesystem, resets the PSP between runs and
   runs each point **twice**;
4. writes `psp-tests/bench/results/<label>.json` with frames, average frame time, fps, work (frame time minus the
   swap wait), `glDrawArrays`/`glDrawElements` draws, texture binds, submitted vertices and merged runs;
5. prints a table, and with `--baseline` a delta table for average frame time, fps and work.

Repeatability on the current build: two consecutive runs of the same label differ by **0.001–0.005 ms**, so a
change of more than ~0.05 ms in work is real.

**Two consoles on one PC.** Each console has its own `usbhostfs_pc` on its own base port, and `HOSTFS` must be that
instance's root. `pspsh` defaults to port 10000, so name the console explicitly:

```sh
PSP_DEVICE_APPROVED=1 HOSTFS=/path/to/that/instance/root \
  ./tools/psp-bench.sh --port 11000 --keep-driver --label p10s26-close
```

The pre-flight check matches the requested `-b` port, so a missing instance fails loudly instead of benching the
other console. Note that `lib_hash` in the JSON is the hash of the installed archive: `psp-ar` stores member
timestamps, so two builds of identical sources get different hashes. Compare builds by `git` plus `driver_hash`;
`lib_hash` tells apart reinstalls of one session.

## Reading the numbers

- **work** (from the demo's own SPLIT line) is the metric, not the median frame time: the frame is quantised to
  the 16.683 ms vsync period, so a 40 % CPU change can leave the median untouched.
- **draws** are calls at the raylib↔driver boundary. For the 3D demo the merged runs go through
  `pspguDrawMergedArrays`, which is below that boundary, so they appear in **runs** (from `MERGEPROBE`) instead.
- **binds** are `glBindTexture` calls with a non-zero id; a run per texture is what the batcher is built to produce.
- **verts** are vertices that passed through `glDrawArrays`; the 3D demo's meshes go through `glDrawElements`, so
  its number covers only the HUD.

## Captures

The demos print `DONE` and sleep when built with `FRAMES>0`, which is when `scrshot` is safe. The raw format is a
54-byte header followed by 480×272 pixels in **B,G,R** order, **bottom-up**, 5-6-5 quantised. The header length is
derived as `len − 480×272×3`, so another capture size fails loudly instead of being shifted.

`tools/scrshot2png.py <raw> <out.png> [--probe X,Y]` converts and probes it. The tool's header comment and the
sprint 080 T3 note are the single source of truth for the layout; do not read the body as top-down RGB.

## The checksum gate

Capture-at-DONE cannot see scrambled geometry, so `psp_showcase CHECKSUM=1` switches the animation to the frame index
and prints `PSP_FramebufferHash()` at frames 60/120/240; `PSP_FramebufferHash` waits for the list that drew the
buffer, then FNV-1a hashes the visible 480×272. Current baseline:

```
frame 60   86b1e3c2
frame 120  c4b13eb4
frame 240  670d502c
```

Rules that were learned by getting them wrong (092 T4, 096 §9.5):

- **The gate build must not draw a measured number.** The HUD used to print `"%d fps", GetFPS()`; a no-op library
  change moved the digit, which moved 12 glyph pixels and therefore all three hashes. The checksum arm draws
  `"checksum build"` instead. If you add anything to a hashed frame, ask whether it can change without the geometry
  changing.
- **Prove invariance before quoting it.** Build the scene twice with the change absent/present: equal hashes is the
  property that makes the gate usable across builds.
- **The hash is all-or-nothing.** One pixel changes everything it says, so it answers "identical or not", never "how
  much". When it moves, capture both arms at a fixed frame:

```sh
# build with CHECKSUM=1 FRAMES=241, run it, then
pspsh -p 11000 -e "scrshot host0:/arm.raw"
python3 -   # count differing pixels between the two arms, print their x,y
```

## When the pair cannot decide

The 3D demo's frame time sits near a vblank step. The same sources have read **33.44**, **41.24** and **50.00 ms**
across sessions, because the frame lands on 2, 2.5 or 3 periods — and the `work` value only differs by ~1 ms between
those states. A single-session A/B that puts one arm below the step and the other above it will "win" by 8 ms that
no code change explains.

- Interleave the arms inside one session, and report the swap wait for each run. A swap near 2.7 ms means one
  period of wait, 16.3 ms means the frame missed and paid a whole extra one.
- For any change whose expected gain is under one period, decide with a **step-free** metric: the free-running
  frame (`--vsync 0`) for attribution, or the last cube count that holds 30 fps for capacity.
- `work = avg − swap` is a difference of two quantised values; on a step it is not a measurement.


## What is not measured

Audio, memory pressure, texture streaming and the Media Engine. Those have their own harnesses (`psp-tests/audio_*`,
`texture_churn`, `texture_residency`). This pair answers one question: did the renderer get faster without changing
a pixel.
