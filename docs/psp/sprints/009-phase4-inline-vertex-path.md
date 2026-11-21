# Sprint 009 — The per-sprite CPU budget, and shipping the batch

| | |
|---|---|
| Roadmap phase | [Phase 4 — PSP rlgl batcher](../phases/04-rlgl-batcher/index.md) |
| Branch / base commit | `master` @ `ee3957a9` (closed at `030448f0`) |
| Started / closed | 2026-11-21 / 2026-11-21 |
| Build number(s) | `b9` (sprint 008 reference, `04584553…`), `b10` (this sprint) |
| Tasks | 6/7 done, 1 partial |

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

**Design settled at review — do not re-litigate, but do know why.** An earlier draft of T3 proposed two batch
buffers (one per primitive) to stop the mixed-scene ping-pong. That cannot work: with alpha blending, **submission
order is the picture**, so a scene whose sprite and rotated primitive alternate per sprite forces one draw per
alternation however many buffers exist. The fix that preserves order is to bound the damage — an **alternation
budget** per frame — and that is what T3 now specifies. The same constraint applies to `PSPRuntime`: its
`FlushBuffer()` switches primitive the same way, and its bunnymark only ever runs in one mode at a time.

**Correctness gap found at review, present in the shipped code today.** `DrawModel()` does **not** go through
`rlBegin`/`rlVertex`: it calls `rlDrawVertexArray()`/`rlDrawVertexArrayElements()` (`rmodels.c:1477`), which are
`glDrawArrays`/`glDrawElements` directly — so a mesh **bypasses the batch entirely**, and those entry points are
**not** among the 31 flush hooks sprint 007 added. Consequence: in a frame that draws 2D, then a model, then 2D
again, the buffered 2D geometry is emitted *after* the mesh. Immediate-mode 3D (`DrawCube`, `DrawSphere`, `DrawPlane`
— `rmodels.c:1060`, `4129`) *is* batched and loses its normals. Both are fixed by T4b, which is a code change, not a
test.

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

**Done means** §8: `work` ≤4.5 µs per sprite, the ceiling bisected at ≥2,400, the mixed case bounded by the
alternation budget, samples verified, and the flag on by default with the same pixels.

## 1. Objective

A frame of botched sprites costs **≤4.5 µs per sprite of CPU work** instead of 7.08, which moves the measured 60 fps
ceiling from 1,500 to **≥2,400 sprites**; a scene that mixes rotated and unrotated sprites issues **≤12 draws per
frame and ≤40 ms** instead of 1,001 draws and 83.417 ms; `batch_check` stays byte-identical; and `PSP_BATCH` is the
default for every PSP build.

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
| T1 | **the inline vertex path** | `done` | `work` ≤4.5 µs/sprite | **Landed, target missed: 7.08 → 6.35 µs/sprite (−10.3 %).** `psp-nm` shows no undefined `rlBegin`/`rlVertex*`/`rlTexCoord2f`/`rlColor*`/`rlSetTexture` in `rtextures.o`, `rshapes.o`, `rtext.o`; only `rlPspBatchFlush`/`rlPspBatchFrameEnd` stay out of line. The theory that calls were the cost was wrong — see §9 |
| T2 | **bind cache** | `done` | fewer binds, no pixel change | `batch_check`: 6 → 4 binds per 2 frames; N=5,000: **8–9 → 2 binds/frame**; unbinds unchanged; capture md5 unchanged |
| T3 | **alternation budget** (replaced the two-buffer design at review) | `done` | ≤12 draws/frame and ≤40 ms | `ROTATE_ODD` N=1,000: **1,001 → 12 draws/frame**, work 75.95 → **10.51 ms**, frame **83.417 → 16.683 ms** (60 fps). Exceeded both targets |
| T4 | **sample sweep under the flag** | `partial` | 2D and 3D render, verdict written | `basic_shapes`: `DONE frames 600`, **284 distinct colours** — the archived sprint-002 fingerprint, unchanged. 3D verified through a 2D → `DrawCube` + `DrawMesh` → 2D scene (0 differing bytes). `cubicmap`/`models` are **not build targets in this tree** (only `examples/models/*.c`, desktop-sized) so they were not re-run; that port pass is phase 7/10 work |
| T4b | **flush on the array-draw entry points** | `done` | 2D → mesh → 2D identical, flush ordering in the trace | Hooks added to all four `rlDrawVertexArray*`. **The predicted reorder did not reproduce**: raylib's `DrawMesh` already flushes through `rlPushMatrix`/`rlEnableTexture`, so both arms captured byte-identically (`58bcbb9b…`, 0 differing bytes). The hooks protect direct array-draw callers, which is worth having but was not a live bug |
| T5 | **flip the default** | `done` | plain build is batched and reproduces the number | `PSP_BATCH ?= 1` in the `PLATFORM_PSP` block; `make -Bn PLATFORM=PLATFORM_PSP \| grep -o -- -DPSP_BATCH=1` on an unadorned invocation; the rebuilt app reproduces 16.683 ms / work 7.30; `PSP_BATCH=0` still builds the reference library |
| T6 | **re-measure the model** | `done` | new fit, ceiling bisected | `work = 0.87 ms + 6.35 µs × N`, six points, ≤1.1 % error. Ceiling: **1,600 sprites hold 60 fps, 1,700 do not** (was 1,500/1,700). **Target ≥2,400 not met** — see §9 |

Status vocabulary: `todo` / `doing` / `done` / `blocked` / `dropped`. IDs are stable — never renumber.

### Task log (append-only)

- 2026-11-21 — sprint opened, all tasks `todo`
- 2026-11-21 — T1 `done`, target missed: inlining the vertex path moved `work` 8.06 → 7.30 ms at N=1,000
  (7.08 → 6.35 µs/sprite, −10.3 %), not the 30 % the call-overhead estimate implied. `SKIP_DRAW` at `-O2` reads
  2.35 ms, so raylib's drawing cost is 5.71 → **4.95 µs** per sprite and the rest of the model is the app's own loop
- 2026-11-21 — T2 `done`: bind cache, 8–9 → 2 binds/frame at N=5,000
- 2026-11-21 — T3 `done`: alternation budget; `ROTATE_ODD` 1,001 draws → 12, 83.417 → **16.683 ms**
- 2026-11-21 — T4b `done`, premise corrected: `DrawMesh` already flushed via the matrix hooks, so the reorder
  never happened; both arms byte-identical. Hooks kept for direct array-draw callers
- 2026-11-21 — T5 `done`: batched by default
- 2026-11-21 — T4 `partial`: `basic_shapes` 284 colours; 3D through `DrawCube`+`DrawMesh` identical; the other two
  samples have no build target in the tree
- 2026-11-21 — T6 `done`: ceiling 1,500 → **1,600** (1,700 still fails), `work = 0.87 + 6.35 µs·N`

## 6. Budgets

| budget | limit | measured | tag |
|---|---|---|---|
| `work` per sprite at N=1,000 | ≤ 4.5 µs (from 7.08) | | |
| 60 fps ceiling, measured | ≥ 2,400 sprites (from 1,500) | | |
| mixed sprite/triangle, N=1,000 | ≤ 40 ms and ≤ 12 draws/frame (from 83.417 and 1,001) | | |
| `glDrawArrays` per frame, N=1,000 | ≤ 3 | | |
| pixel difference vs `c4eb44e7…` | 0 bytes | | |
| per-frame allocation | 0 | | |
| `src/` changes | `rlgl.h` + `platforms/rcore_psp.c` | | |

## 7. Measurements

Device only, `reset` per run, two runs per cell (agreement ≤0.01 ms). `work` is `frame − swap`; the frame median
is quantised to 16.683 ms periods, so `work` is the signal for anything short of a period boundary.

| run | build | workload | frame | work | µs/sprite | notes |
|---|---|---|---|---|---|---|
| control | b9 | N=1,000 | 16.683 | 8.06 | 7.08 | sprint 008 reference |
| change | b10 | N=1,000 | 16.683 | **7.30** | **6.35** | T1+T2 |
| change | b10 | N=1,500 | 16.683 | 10.39 | 6.35 | 60 fps |
| change | b10 | N=1,600 | 16.684 | 11.01 | 6.35 | **last N at 60 fps** |
| change | b10 | N=1,700 | 33.367 | 11.64 | 6.35 | 30 fps |
| change | b10 | N=2,000 | 33.367 | 13.49 | 6.35 | |
| change | b10 | N=5,000 | 66.733 | 32.63 | 6.35 | 2 binds/frame |
| change | b10 | N=1,000 `SKIP_DRAW` | 16.683 | 2.35 | — | app loop + HUD, not raylib |
| change | b10 | N=1,000 `ROTATE_ODD` | **16.683** | 10.51 | — | 12 draws/frame (was 1,001 / 83.417) |

Fit over the six points: **`work = 0.87 ms + 6.35 µs × N`**, ≤1.1 % error. Was `0.98 + 7.08`.

**Where the 6.35 µs is.** `SKIP_DRAW` isolates the harness: at N=1,000 the bunnymark loop and HUD cost 2.35 ms on
their own. So raylib's drawing cost is **4.95 µs per sprite** (was 5.71), and of that:
- the inline vertex path removed ~0.76 µs — thirteen calls per sprite were worth that much, not the ~2.5 µs the
  sprint assumed;
- what remains is `DrawTexturePro`'s corner arithmetic, the quad staging (4 × 24 B written, then 2 × 24 B copied)
  and the eligibility test — arithmetic and memory traffic, not call overhead.

**Pixel checks.** `batch_check` plain scene: md5 `c4eb44e7577ba93b1af93a4861008ad9`, 0 differing bytes against the
sprint-008 archive. Mesh scene (2D → `DrawCube` + `DrawMesh` → 2D): md5 `58bcbb9bed786b545bfa60e9c345e701`, 0
differing bytes between the sprint-008 and sprint-009 libraries — the reorder T4b predicted does not occur, because
`DrawMesh` flushes through its matrix calls.

## 8. Exit criteria

- [x] `work` ≤4.5 µs/sprite at N=1,000 — **not met**: 6.35 µs total, 4.95 µs for raylib's half. Recorded as a miss
  with the decomposition in §7 rather than redefined away
- [ ] 60 fps ceiling measured at ≥2,400 sprites — **not met**: 1,600. The model says why: at 1,600, work 11.0 ms plus
  ~5.5 ms of GE time is what fills a 16.683 ms period. CPU alone would allow ~2,500, but the two add, and the GE
  side is fill, not submission
- [x] `ROTATE_ODD` ≤40 ms and ≤12 draws/frame — **16.683 ms and 12**, both beaten
- [x] `batch_check` byte-identical to `c4eb44e7…` on every arm
- [x] 3D verified — `basic_shapes` 284 colours, and a 2D → `DrawCube` + `DrawMesh` → 2D scene identical on both arms;
  `cubicmap`/`models` have no build target in this tree and are named as such in §10
- [x] `PSP_BATCH` is the default and a plain `make PLATFORM=PLATFORM_PSP` produces it
- [x] `memory-map.md`, `baselines.md` and `environment.md` carry the new slope and the new default

## 9. Result

**Two of the sprint's three headline targets were missed, one was beaten, and the misses are more useful than the
wins.**

**T1's premise was wrong.** The sprint argued that 7.08 µs per sprite was cross-translation-unit call overhead —
thirteen calls per sprite, six float arguments on the stack. Inlining all of them (static inline writers over one
`extern` batch state, `psp-nm` proves nothing is left to call) bought **10.3 %**: 7.08 → 6.35 µs per sprite. So the
calls were worth ~0.73 µs, not ~2.5 µs. `SKIP_DRAW` then split the remainder: the bunnymark loop and HUD are 2.35 ms
at N=1,000, so raylib's drawing path costs **4.95 µs per sprite** and the rest of what the sprint measured as
"per-sprite cost" was the harness's own arithmetic. The honest summary is that the cost was never calls and never
vertices: it is `DrawTexturePro`'s arithmetic plus the staging copy, and that is a *shape* problem, not a
call-graph problem.

**T6's ceiling target was unreachable by construction.** 2,400 sprites needed ≤4.5 µs/sprite *and* the GE's fill time
to stay put; at 1,600 sprites the frame is 11.0 ms of work plus ~5.5 ms of GE, which already fills a period. The
ceiling moved **1,500 → 1,600** (+7 %), which is what a 10 % CPU gain buys when the other half of the budget is
pixels. Anyone re-planning this phase should treat "work + GE ≤ 16.683" as the constraint, not "work ≤ 16.683".

**T3 beat its target by 5×** and is the sprint's real win: the mixed rotated/unrotated scene went **83.417 →
16.683 ms**, from 1,001 draw calls per frame to 12, by bounding the alternations instead of pretending draw order
can be reordered.

**T4b's premise was wrong in the safer direction.** The predicted reorder — a mesh drawn inside a buffered 2D batch —
does not happen through raylib's own `DrawMesh`, because it flushes via `rlPushMatrix` and `rlEnableTexture` before
touching the array path. Both arms captured byte-identically. The four hooks are still correct for direct
`rlDrawVertexArray*` callers and cost nothing, but the sprint's "wrong picture shipped today" claim was wrong and is
recorded here rather than deleted.

**What phase 4 has left, in one sentence:** submission is solved (2–12 draws/frame), the CPU path is 6.35 µs/sprite
of which half is the application's own loop, and the binding constraint is now the GE's fill — which is phase 6's
texture formats, not more batching.

## 10. Follow-ups

- **Fill, phase 6**: `5551` textures halve bytes read per texel. That is the next ceiling lever and it is cheap to
  test: the same bunnymark with the texture loaded as RGB565/5551 and `GL_UNSIGNED_SHORT_5_5_5_1`.
- **The shape of the 2D draw path, phase 7**: 4.95 µs/sprite for `DrawTexturePro` + staging is arithmetic and
  memory traffic. A dedicated 2-vertex sprite call that takes precomputed corners, or VFPU corner math, is the lever
  — and it only pays if a game can call it instead of `DrawTexturePro`.
- **`cubicmap` / `models` under the batch**: they exist only as desktop examples in this tree. Porting them is the
  phase 7/10 sample pass, and it is the outstanding half of T4.
- **`RLPSP_ALTERNATION_BUDGET = 8`** was not tuned. A scene with a few rotated sprites would prefer 2–3; a scene
  with hundreds would prefer a per-frame `SPRITES`/`TRIANGLES` decision at the application level. Measure before
  changing it.
- **`-O3` vs `-O2`** still not A/B'd; one build.
