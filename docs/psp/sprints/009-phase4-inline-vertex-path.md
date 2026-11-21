# Sprint 009 — The per-sprite CPU budget, and shipping the batch

| | |
|---|---|
| Roadmap phase | [Phase 4 — PSP rlgl batcher](../phases/04-rlgl-batcher.md) |
| Branch / base commit | `master` @ `17def498` |
| Started / closed | — / — |
| Build number(s) | `b9` (sprint 008 reference, `04584553…`), `b10` (this sprint) |
| Tasks | 0/6 done |

## 0. Handover

**Situation.** Phase 4's objective is met: 1,000 sprites run at 60 fps (16.683 ms) and 1,025 draw calls per frame
became 2–3. Two things are unfinished. The sprite path we added in sprint 008 **regresses any scene that mixes
rotated and unrotated sprites** — 1,001 draws/frame, 83.417 ms — and the whole feature is still **off by default**,
so nothing ships it. This sprint fixes the regression, buys the per-sprite CPU headroom that sets the ceiling, and
turns the flag on.

**The model this sprint is trying to move.** Measured over 100 → 8,000 sprites, eight points, 1.1 % maximum error:

```
work (ms)   = 0.98 + 7.08 µs × sprites        # CPU cost of building and submitting a frame
frame (ms)  = ceil((work + ~5)/16.683) × 16.683   # display periods, exact at every point below 4,000
```

That is why the 60 fps ceiling is 1,500 sprites and not 2,230: 16.683 ms of budget, minus ~5 ms of pipeline
overhead, minus 0.98 ms, divided by 7.08 µs. **`7.08 µs` per 32×32 sprite is the whole of what sprint 009 attacks.**

**Where 7.08 µs goes, and why it is not arithmetic.** At 222 MHz that is ~1,570 cycles for ~30 float operations,
four texture-coordinate stores, four vertex pushes and a 96-byte staging copy. The rest is call overhead:
`rlVertex2f`, `rlTexCoord2f`, `rlColor4ub`, `rlPspBatchVertex` and `rlPspReserve` are ordinary functions defined in
`platforms/rcore_psp.c` — **a different translation unit** from the callers (`rtextures.c`, `rshapes.c`, `rtext.c`),
with six float arguments each on the stack under the o32 ABI. Nothing inlines. A sprite pays 15–20 of them.

**Already true, verified — do not re-derive:**

- The chain is raylib → rlgl GL11 + our batch → **pspGL** → raw GE packets → `sceGeListEnQueue`. **GU and GUM are
  not in the binary**: no `-lpspgu`, no `-lpspgum` on the link line, no `sceGu*` call in pspGL, and pspGL already
  keeps its own GE register shadow and submits lists itself. pspGL's matrix stack *does* use the VFPU
  (`pspgl_matrix.c`). So "use GU/GUM" is not a shortcut to anything we lack — it is a second driver, which is
  phase 5's question, not this sprint's.
- `-O2` is the default (`PSP_OPT`), the batch is `PSP_BATCH=1`, sprites are `GL_SPRITES_PSP`.
- The counters exist: `arrays`, `arrayVerts`, `binds`, `unbinds`, `verts`, `ends`, plus `ROTATE_ODD` and
  `PSP_BATCH_NO_SPRITES` switches.
- `batch_check` is the correctness gate; its capture md5 is `c4eb44e7577ba93b1af93a4861008ad9` for every arm
  measured so far.
- The other limit is **fill**: 4× the pixel area at N=1,500 takes the frame from 16.684 to 50.050 ms with CPU work
  unchanged, and above ~4,000 sprites the frame is a period worse than the CPU model — so pixels start to bind
  around 4–5 M texels. That is why this sprint targets ~2,400 and not 5,000: past ~3,000 the next wall is fill, and
  fill is phase 6's texture formats, not this sprint's code.

**Read before acting:**

1. `src/platforms/rcore_psp.c`, the `RLPSP_BATCH` block — `rlPspBatchVertex`, `rlPspReserve`, `rlPspBatchFlush`,
   `rlPspIsSpriteQuad`. The staging array `rlPspQuad[4]` and the eligibility test live there.
2. `src/rlgl.h`, the guarded block near line 1160 and the GL11 vertex block near line 1450 — the declarations and
   the one-line delegations this sprint replaces.
3. [Baselines § "Scaling past the ceiling"](../baselines.md) — the model above, and the eight points it fits.

**First action.** Add `measure.sh 3` noise floor: run the N=1,000 sprite arm twice and record `work` before touching
anything (it should read 8.06 ± 0.02 ms). Then, and only then, move the batch state into an `extern` struct.

**Hard rules.**

- **Measure `work`, not the frame median.** The frame is quantised to 16.683 ms periods, so a 40 % CPU improvement
  moves the median by *nothing* at N=1,000. `work` (from the app's own split) is the signal; the ceiling bisection
  is the confirmation.
- **The `extern` struct must be defined exactly once.** Declare it in `rlgl.h` behind the guard, define it in
  `platforms/rcore_psp.c`, export the pointer. A `static` in the header would be one copy per translation unit —
  sprint 007 already paid for that lesson (`rcore.o` bss 19,996 → 118,324 B was the proof it is one).
- **Pixels are the gate, not the speed.** Any change to the vertex path can reorder or corrupt a frame in ways a
  frame timer cannot see. `batch_check` must stay at md5 `c4eb44e7…`, byte-for-byte.
- Do not change what raylib asks for: no edits in `rshapes.c`, `rtextures.c`, `rtext.c`.
- `reset` before each run, never `exit`, the app must not call `sceKernelExitGame`.
- Threshold 0.1 %; two runs per configuration.

**Commands.**

```sh
export PSPDEV=/home/psp/pspdev
export PATH=$PATH:$PSPDEV/bin

cd /home/psp/raylib-psp/src
make clean && make PLATFORM=PLATFORM_PSP PSP_BATCH=1        # PSP_OPT defaults to -O2
cp libraylib.a $PSPDEV/psp/lib/libraylib.a

cd ../psp-tests/bunnymark
./measure.sh 3                     # N = 100 / 1,000 / 5,000, two runs, and the control
./probes_ceiling.sh                # bisection + ROTATE_ODD (edit the arm numbers as needed)
```

**Do not touch.** `rlgl.h`'s flush hooks (proven in 007), the `-O2` default, `tools/pspgl-build.sh`, the adopted
pspGL build, `psp-tests/basic_shapes`, and anything under `docs/` outside this sprint's record.

**Done means** §8: `work` ≤4.5 µs per sprite, the ceiling bisected at ≥2,400, the mixed case under 20 ms, samples
verified, and the flag on by default with the same pixels.

## 1. Objective

A frame of botched sprites costs **≤4.5 µs per sprite of CPU work** instead of 7.08, which moves the measured 60 fps
ceiling from 1,500 to **≥2,400 sprites**; a scene that mixes rotated and unrotated sprites costs **≤20 ms** instead
of 83.417 ms; `batch_check` stays byte-identical; and `PSP_BATCH` is the default for every PSP build.

## 2. Why now

This is the last phase-4 sprint and its two unfinished items are both **shipping risks**, not optimisations: a
feature that regresses a mixed scene by 5× cannot be enabled, and a feature that is off by default is worth zero.
The CPU work it removes is also the number that decides what comes after phase 4: at 4.5 µs/sprite the next wall is
fill (phase 6's texture formats); if it refuses to move, the remaining cost is raylib's 2D API shape itself, which
is phase 7's specialisation or phase 5's backend — and that choice needs the number.

## 3. Scope

**In:** the vertex push path in `platforms/rcore_psp.c` and its declarations in `rlgl.h`; a texture-bind cache; a
second batch buffer so sprite and triangle runs coexist; bunnymark's harness; `batch_check`; the sample sweep; the
`PSP_BATCH` default.

**Out:**

- Compact vertex formats (16 B, `u16` UVs, `5551` colour) — phase 6, and pointless before the CPU path stops being
  call-bound.
- 5551 textures / VRAM accounting — phase 6; that is the *fill* lever, measured at ~4–5 M texels.
- A native GU backend — phase 5. Nothing here needs it, and §0 records why GU/GUM is not a shortcut.
- `rlNormal3f` support in the batch (lit 3D) — phase 10; T4 either proves the 3D samples unlit-safe or marks the
  flag 2D-only.

## 4. Preconditions

- [ ] `pspsh` answers, USB device is `054c:01c9`; `reset` before each run
- [ ] Sprint 008 libraries archived: `/home/psp/evidence/sprint008/libraylib-sprites-O2.a` (`04584553…`),
      `…-tri-O2.a`, `…-unbatched-O2.a`, and the captures
- [ ] `PSP_OPT` defaults to `-O2`; `make PLATFORM=PLATFORM_PSP -n | grep -c -- -O2` is nonzero
- [ ] Baseline `work` at N=1,000 recorded this session: expected **8.06 ms** (7.08 µs/sprite + 0.98 ms)
- [ ] Baseline ceiling recorded: 1,500 sprites hold 60 fps, 1,700 do not
- [ ] Baseline mixed measurement: `ROTATE_ODD=1`, N=1,000 → 1,001 draws/frame, 83.417 ms
- [ ] `batch_check` capture md5 for the reference build: `c4eb44e7577ba93b1af93a4861008ad9`

## 5. Tasks

| ID | task | status | verification | evidence |
|---|---|---|---|---|
| T1 | **the inline vertex path**: move the batch state into one `RLPspBatch` struct defined in `platforms/rcore_psp.c`, exported as a pointer, and make the GL11 vertex/texcoord/colour entry points in `rlgl.h` write into it directly, with the sprite-quad staging and eligibility test inlined in the caller's translation unit. Only `rlPspBatchFlush()` stays an out-of-line call, because it is the rare path | `todo` | `work` at N=1,000 ≤ **4.5 ms** (from 8.06) → expected 7.08 → ≤4.5 µs/sprite; `nm` shows no `rlPspBatchVertex`/`rlPspBatchTexCoord`/`rlPspBatchColor` symbols left, and `rtextures.o`/`rshapes.o`/`rtext.o` grow (the code moved in) | — |
| T2 | **bind cache**: remember the texture actually bound to the GL and skip `glBindTexture` when the run needs the same one. `rlPspBatchFlush` currently binds on every flush, and the mixed case flushes ~1,000 times a frame | `todo` | `binds`/frame drops from 2 to ≤2 with no change to `unbinds` in the normal case, and by ~1,000/frame in the mixed case; `ROTATE_ODD` improves measurably before T3 lands | — |
| T3 | **two batch buffers**: one for `GL_SPRITES_PSP` runs and one for `GL_TRIANGLES`, flushed at `rlDrawRenderBatchActive` in submission order. A mixed scene must not ping-pong; order is preserved by flushing the other buffer whenever the primitive class changes only when it contains older geometry | `todo` | `ROTATE_ODD=1` N=1,000 ≤ **20 ms** and `draws`/frame ≤ ~50 (from 83.417 ms and 1,001) → if `draws` stays at 1,001, the design is wrong, not the code | — |
| T4 | **sample sweep under `PSP_BATCH=1`**: `basic_shapes`, `cubicmap`, `models` on the device, with `basic_shapes`/`cubicmap` frozen-scene captures compared against the unbatched arm. This is the first time the batch sees 3D content, which the flag drops normals for | `todo` | all three render and reach `DONE`; captures identical or the differing pixels counted and explained; the 3D verdict written down either way | — |
| T5 | **flip the default**: `PSP_BATCH=1` for `PLATFORM_PSP` unless T4 says otherwise, `PSP_BATCH=0` still building and still the documented escape hatch | `todo` | a clean build with **no** `PSP_BATCH` on the command line produces a batched library, and re-running the N=1,000 row reproduces the sprint's number | — |
| T6 | **re-measure the model**: `work` at N = 100 / 1,000 / 1,500 / 2,000 / 2,500 / 4,000, the ceiling bisected, and `work` per sprite re-fitted. The sprint's headline is the slope, not one frame time | `todo` | a new `work = a + b·N` fit with `b ≤ 4.5 µs`, and the measured ceiling bracket ≥ 2,400 sprites | — |

Status vocabulary: `todo` / `doing` / `done` / `blocked` / `dropped`. IDs are stable — never renumber.

### Task log (append-only)

- _(not started)_

## 6. Budgets

| budget | limit | measured | tag |
|---|---|---|---|
| `work` per sprite at N=1,000 | ≤ 4.5 µs (from 7.08) | | |
| 60 fps ceiling, measured | ≥ 2,400 sprites (from 1,500) | | |
| mixed sprite/triangle, N=1,000 | ≤ 20 ms (from 83.417) | | |
| `glDrawArrays` per frame, N=1,000 | ≤ 3 | | |
| pixel difference vs `c4eb44e7…` | 0 bytes | | |
| per-frame allocation | 0 | | |
| `src/` changes | `rlgl.h` + `platforms/rcore_psp.c` | | |

## 7. Measurements

Protocol: device only, `reset` per run, two runs per cell, `work` quoted alongside the frame median because the
median is quantised to display periods.

| run | build | workload | frame | work | µs/sprite | notes |
|---|---|---|---|---|---|---|
| control | b9 | N=1,000 | 16.683 | 8.06 | 7.08 | sprint 008 reference |
| change | b10 | N=1,000 | | | | |
| change | b10 | N=2,500 | | | | |
| change | b10 | N=4,000 | | | | |
| change | b10 | N=1,000 `ROTATE_ODD` | | | | mixed-scene fix |
| control again | b9 | N=1,000 | | | | |

Ceiling: bisect between the last N that holds 16.683 and the first that does not, per arm.

## 8. Exit criteria

- [ ] `work` ≤4.5 µs/sprite at N=1,000, two runs, and a re-fitted slope over at least four N values
- [ ] 60 fps ceiling measured at ≥2,400 sprites, by bisection not extrapolation
- [ ] `ROTATE_ODD` ≤20 ms, with `draws`/frame in the log proving the ping-pong is gone rather than hidden
- [ ] `batch_check` byte-identical to `c4eb44e7…` at every arm
- [ ] `basic_shapes`, `cubicmap`, `models` verified under `PSP_BATCH=1`, with the 3D verdict in §9
- [ ] `PSP_BATCH` is the default and a plain `make PLATFORM=PLATFORM_PSP` produces it
- [ ] `memory-map.md` §4, `baselines.md` and `environment.md` updated with the new slope and the new default

## 9. Result

(To fill in on close. The interesting failure is a slope that does not move: if inlining buys less than 20 %, the
call overhead was not the cost and the next phase's question changes — the remaining 7 µs would be the *shape* of
`DrawTexturePro`'s work, which only a different 2D API or VFPU math removes, and that is a phase 5/7 decision made
on evidence rather than on a hunch.)

## 10. Follow-ups

- Fill budget: 4–5 M blended texels per frame is where pixels start to bind. `5551` textures halve the bytes read
  per texel — phase 6, and the cheapest remaining lever once `work` is under 4.5 µs.
- VFPU corner math for `DrawTexturePro`'s four corners: phase 7, only if T1's slope stops at ~4 µs.
- The mixed-scene design (two buffers) is a general facility; `rlNormal3f`/3D runs would need a third class of
  primitive if the 3D samples show a need (phase 10).
- A second automatic sprite path — a 2-vertex call that skips the eligibility test when the caller already knows it
  is passing a rectangle — is only worth it if T1 lands above 3 µs.
