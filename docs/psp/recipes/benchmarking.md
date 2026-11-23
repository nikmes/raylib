# Benchmarking the port

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
54-byte header followed by 480×272 pixels in **B,G,R** order, **bottom-up**, 5-6-5 quantised — see the note in
sprint 080 T3. A converter that gets both of those right is still a follow-up.

## What is not measured

Audio, memory pressure, texture streaming and the Media Engine. Those have their own harnesses (`psp-tests/audio_*`,
`texture_churn`, `texture_residency`). This pair answers one question: did the renderer get faster without changing
a pixel.
