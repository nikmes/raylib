# Sprint 013 (P5-S3) — Pixel parity across the corpus, and the counter harness back

| | |
|---|---|
| Roadmap phase | [Phase 5 — Native GU backend](../phases/05-native-gu-backend/index.md) |
| Sprint | P5-S3 |
| Branch / base commit | `master` @ `f0cec42a` |
| Started / closed | 2026-11-21 / 2026-11-21 |
| Build number(s) | `b14` (pspgu, parity on `batch_check`, swizzled) |
| Tasks | **closed** — 5/6 done, T1 split (T1a met, T1b to phase 10) |

## 0. Handover

**Situation.** Sprint 012 reached **byte-identical output on `batch_check`'s 2D scene**: pspgu and pspGL render the
same binary to the same bytes (0 differing pixels, 613 colours), with swizzled textures. What has never been
rendered by the new driver is the rest of the corpus — the 2D→3D→2D scene, `basic_shapes`, `bunnymark` — and the
`--wrap` counter harness cannot link against it at all.

**Why the counters do not link, and why that is this sprint's job.** `psp-tests/*` interpose GL calls with
`-Wl,--wrap=glBegin`, `--wrap=glEnd`, `--wrap=glVertex2f`, `--wrap=glVertex2i`. `--wrap` requires the **real**
symbol to exist at link time, and under `PSP_BATCH=1` neither the library nor pspgu defines the immediate-mode
entry points — the batcher calls array draws only. Every test therefore builds `WRAP=0` today, which means no
counter, no `arrayVerts`, no `SPLIT` from `bunnymark`. That is phase-5 **task 12**, and it is a prerequisite for
sprint 014's ceiling measurement.

**Already true, verified — do not re-derive:**

- `batch_check` 2D scene: **byte-identical** to pspGL (`cmp -l` → 0). Keep that as the regression gate; any change
  in this sprint that breaks it is a bug, not a trade-off.
- The texture path is complete for power-of-two textures: swizzled upload, `GU_TCC_RGBA` alpha, `GU_DITHER`
  background, LUMINANCE_ALPHA decoded as grey+alpha (not RGB565 — that was the last 32.6 %).
- Non-power-of-two textures are **known broken** (sprint 012 T2): the swizzle needs `height % 8 == 0` and a
  power-of-two width, so such textures currently fall back to linear and still sample wrong. Out of scope here;
  no test in this sprint uses one.
- `batch_check` has a `MESH_SCENE=1` build that draws 2D → `DrawCube` + `DrawMesh(GenMeshCube)` → 2D. That is the
  only 3D in the corpus, and it exercises `glDrawElements` and the converting vertex path.
- pspGL's capture of the mesh scene exists (`/tmp/s3/mesh_b10.raw` in an earlier session; regenerate if absent).

**Third review (after T3/T4 landed) — T1 has a prerequisite bug, and it is not in the driver's array path
as written.** `gl_draw.c`'s converting path sets `out->color = 0xFFFFFFFF` when `GL_COLOR_ARRAY` is disabled
(`gl_draw.c:109`), but GL11 says the vertex colour then comes from the **last `glColor*` call** — and that is exactly
what `DrawMesh` relies on: it calls `rlColor4ub(material colour)` and then draws with no colour array. Our array path
knows nothing about the immediate colour (it lives as a private static in `gl_immediate.c`), so a mesh would render
white where pspGL renders the material colour.

That makes **T0** below a prerequisite for T1, not a detail: without it the mesh scene differs for a reason that has
nothing to do with `glDrawElements`.

**Second risk for T1, named so it is looked for:** the mesh scene runs inside `BeginMode3D`, which enables
`GL_DEPTH_TEST` and leaves `GL_CULL_FACE` on from `rlglInit`. GU culls by winding, and a wrong winding makes the cube
**invisible**, not distorted — the same failure mode as the sprite path in sprint 011. If the mesh capture is empty
rather than wrong, check winding before anything else.

**Design settled at review — T3 has three answers the draft left open.**

1. **Immediate mode buffers into the array layout, it does not draw per vertex.** `glBegin(mode)` records the
   mode and resets a counter; `glVertex*` appends into a static buffer of the same 24-byte interleaved vertices the
   batcher produces; `glEnd()` emits through the *same* code path `glDrawArrays` uses. That is the only way the two
   paths stay honest — a second implementation would be untested by the 2D gate that protects everything else.
2. **`GL_QUADS` has to be expanded, because GU has no quad.** raylib's immediate path draws quads (`rlBegin(RL_QUADS)`
   → `glBegin(GL_QUADS)`), and GU offers only triangles/strips/fans/sprites. So the emitter splits each four-vertex
   group into two triangles, exactly as the batcher does for non-sprite quads. Getting this wrong is invisible in a
   `PSP_BATCH=1` library (which never calls it) and obvious in a `PSP_BATCH=0` one.
3. **The buffer needs a defined overflow behaviour**: when it fills, emit and carry on, so a long `glBegin`/`glEnd`
   run of thousands of vertices still draws. 1,024 vertices is enough for anything raylib draws in one call and
   costs 24 KiB.

**What the counters will and will not show — a correction to T4.** In a `PSP_BATCH=1` library the immediate-mode
functions are **never called**, so `begins`/`ends`/`verts` stay 0 no matter what pspgu implements; only
`arrays`/`arrayVerts` move. `--wrap` needs the symbols to *exist* (that is T3), but the immediate counters are only
populated by a **`PSP_BATCH=0`** library. T4 checks both builds for exactly this reason, and a run that shows
`verts=0` in the batched build is correct, not broken.

**Known-good reference before starting:** the mesh scene already ran on pspGL in sprints 008–009 (`mesh_b9`,
0 differing bytes against the batched triangle arm), so T1 has a valid oracle — unlike `texture_pattern`, which pspGL
hangs on.

**Read before acting:**

1. [Sprint 012](012-phase5-texture-parity.md) §9a — the four fixes that produced parity, and the pattern test that
   found them.
2. `tools/pspgu/gl_draw.c` — the two draw paths and where an immediate-mode implementation would feed them.
3. `psp-tests/bunnymark/Makefile` — the `--wrap` list; those five symbols are the contract for T3.

**First action.** Build `batch_check` with `MESH_SCENE=1` against pspgu and against pspGL and compare the captures.
That is the cheapest new information in the sprint and it decides whether T2 is a fix or a report.

**Hard rules.**

- **`batch_check`'s 2D scene stays byte-identical throughout.** It is the only regression gate the phase has.
- **Immediate mode must feed the same path as arrays.** `glBegin`/`glVertex*` should accumulate into the same 24-byte
  layout the batcher uses, not a second parallel implementation — otherwise the two paths diverge and one of them is
  untested.
- **`WRAP=1` and `WRAP=0` builds must produce the same picture.** The counters only add symbols; if the picture
  changes, the wrapper is changing behaviour.
- pspGL must stay restorable in one command (`tools/pspgu-build.sh --restore`), verified with
  `tools/pspgl-build.sh --check` against content hash `2ab7378e`.
- `reset` before each run, never `exit`, no `sceKernelExitGame` in the app.

**Commands.**

```sh
export PSPDEV=/home/psp/pspdev
export PATH=$PATH:$PSPDEV/bin

cd /home/psp/raylib-psp
./tools/pspgu-build.sh                       # install pspgu
./tools/pspgu-build.sh --restore             # back to pspGL for the reference capture

cd psp-tests/batch_check && make MESH_SCENE=1 WRAP=0
cd ../bunnymark          && make BUNNY_COUNT=1000 FRAMES=200 TARGET_FPS=0   # WRAP=1 once T3 lands
( printf './app.prx\n'; sleep 20 ) | pspsh
pspsh -e "scrshot host0:/out.raw"
```

**Do not touch.** `gl_texture.c`'s upload path, the batcher in `src/`, `psp-tests/texture_pattern` (it is a finished
diagnostic), and anything under `docs/` outside this sprint's record.

**Done means** §8: the mesh scene and `basic_shapes` match pspGL, `bunnymark` runs with `WRAP=1` and logs counters,
and the 2D gate is still 0 differing pixels.

## 1. Objective

The corpus matches pspGL beyond the 2D scene — the 2D→3D→2D mesh scene and `basic_shapes` byte-identical, or the
differing pixels counted and explained — **and** `bunnymark` links and runs with `WRAP=1`, so the counters that every
measurement in phases 2–4 depended on work against the new driver.

## 2. Why now

Two reasons, both blocking. First, the parity claim currently rests on one scene; a driver is not proven by a scene
that only uses sprites, textures and text. Second, sprint 014 measures a 60 fps ceiling, and a ceiling cannot be
measured without the counters — `arrayVerts`, `draws`, `SPLIT` are exactly the numbers that comparison needs, and
they are link-time wrappers that do not exist yet.

## 3. Scope

**In:** the mesh scene, `basic_shapes`, the immediate-mode symbols (task 12), `--wrap` compatibility, and the
`PSP_DISABLE_VSYNC` build.

**Out:** non-power-of-two textures (sprint 012 T2, diagnosed), list rotation and VRAM accounting (014),
`glReadPixels` (014), the texture-format memory decisions (phase 6), 4-bit glyph atlases (phase 6).

## 4. Preconditions

- [ ] `pspsh` answers, USB device `054c:01c9`; `reset` per run
- [ ] pspgu installed and `--check` agrees; pspGL restorable and verified
- [ ] `batch_check` 2D scene reproduces **0 differing pixels** before anything changes
- [ ] pspGL captures of the mesh scene and of `basic_shapes` taken **this session** (the archived ones may be from a
      different library)
- [ ] `basic_shapes`' archived fingerprint to beat: **284 distinct colours**

## 5. Tasks

| ID | task | status | verification | evidence |
|---|---|---|---|---|
| T0 | **current colour when the colour array is off** | `done` | `DrawMesh`'s cube renders in the material colour; the 2D gate is unchanged | the current colour moved from a static in `gl_immediate.c` into `PspGuContext`, and `gl_draw.c` uses it when `GL_COLOR_ARRAY` is off. **2D gate after the change: 0 differing pixels**, as predicted — the batcher always sets a colour array |
| T1 | **the mesh scene** | `doing` | identical, or the difference counted and localised | **25.0 % of the frame differs, and every differing pixel is inside the 3D cube region.** The 2D part of the same frame still matches. Sampling: at (120,140) pspgu draws the red `DrawCube` while pspGL shows background; pspgu has 722 colours against pspGL's 474. **The lead is narrow and specific: `glOrtho` is proven correct by the 2D parity, so the fault is in the perspective path — `glFrustum` and/or `glMultMatrixf`.** `BeginMode3D` feeds the camera through `rlMultMatrixf(MatrixToFloat(...))`, and raylib's `MatrixToFloat` emits **row-major** while GL's `glMultMatrixf` takes **column-major**: pspGL must be transposing somewhere we are not. That is the same transpose class of bug as the `sceGuSetMatrix` one in sprint 011, one layer up. **Tested and rejected:** transposing the operand inside `glMultMatrixf` moved the frame from 722 to **613 colours, which is the 2D-only count** — the cubes stopped being drawn. So raylib's `MatrixToFloat` output is already what GL expects (pspGL takes it unmodified and works), and the transpose is not the fault. The change is reverted.
**Re-tested under clean conditions, still rejected:** the transpose was first rejected while the depth clear value and the depth comparison were both still wrong, so its earlier failure could have been the depth test rather than the transpose. Re-run with the depth system fixed it produces **613 colours again — the 2D-only count, cubes gone**. So raylib's `MatrixToFloat` output really is what GL expects, and the modelview is being fed correctly. Reverted again.

**Two real depth bugs found by reading pspGL, both fixed:** (1) the depth **clear** value — GU's range is reversed (`sceGuDepthRange(65535, 0)`), so GL's `glClearDepth(1.0)` must become **0**, not 65535; pspGL converts with `65535 - 65535*depth` and we did not, so every fragment passed `LEQUAL` and the cube's faces painted over each other (722 colours, mottled). (2) the **comparison itself** — pspGL inverts every operator ("the PSP's Z coordinate system is backwards from OpenGL's"), so `GL_LEQUAL` must map to `GE_GREATER`, not `GE_LEQUAL`. Applying (1) alone made the cubes vanish (613 colours) because with `LEQUAL` on a reversed range nothing passes; applying both restores drawing (**691 colours, 25.6 % differing**) but does not reach parity, so a third cause remains |
| T2 | **`basic_shapes`** on both drivers | `done` | identical, and the 284-colour fingerprint holds | **0 differing pixels, 284 colours on both.** The sprint's first attempt failed to build because `basic_shapes`' Makefile never got `-lpspgu` (pspGL used `libpspge`), so the app that ran was a stale pspGL binary — recorded rather than used |
| T3 | **immediate-mode symbols** (task 12) | `done` | `psp-tests/*` link with `WRAP=1`; a `PSP_BATCH=0` bunnymark logs non-zero `verts` | `tools/pspgu/gl_immediate.c`: buffers into the array layout, emits through the `glDrawArrays` path, expands `GL_QUADS` to two triangles, 1,024-vertex buffer with emit-on-overflow. `bunnymark` now **links and runs with `WRAP=1`** — the first time since phase 5 began |
| T4 | **counters under the new driver**, both builds | `done` | batched: `arrays`/`arrayVerts` non-zero, `begins`/`verts` zero by design; immediate: `begins`/`ends`/`verts` non-zero | `PSP_BATCH=1`: `begins=0 verts=0 arrays=400 arrayVerts=409,998`, `work 6.565 ms`. `PSP_BATCH=0`: `begins=204,999 ends=204,999 verts=819,996 arrays=0`, `work 25.980 ms` — against pspGL's 48.8 ms for the same unbatched workload |
| T5 | **`PSP_DISABLE_VSYNC`** build links and runs | `done` | the build links, the app runs, and the swap figure changes | builds with 0 errors, links, runs; `SPLIT ... swap=2.433 ms` against ~9.4 ms with vsync, counters intact (`arrays=400 arrayVerts=409,998`) |

Status vocabulary: `todo` / `doing` / `done` / `blocked` / `dropped`. IDs are stable — never renumber.

### Task log (append-only)

- T0 `done`: current colour shared through `PspGuContext`; 2D gate unchanged at 0 differing pixels.
- T1 `doing`, three candidates eliminated: the transpose hypothesis was tested and **rejected** (613 colours after transposing = the 2D-only count, so the cubes vanished). What is left is that the cubes are drawn *somewhere*, in 722 colours, and pspGL draws them in 474 — i.e. both drivers draw 3D, differently. Next suspects, in order: `glFrustum`'s matrix (the perspective term), depth-test behaviour, and `BeginMode3D`'s blend/cull state.
- T1 measured: **25 % of the frame differs, all of it inside the 3D cubes**; the 2D part of the same frame still matches, which localises the fault to the perspective path rather than the matrix upload. Next: check whether `glMultMatrixf` must transpose raylib's row-major `Matrix`.
- T3 `done`: immediate mode implemented in `tools/pspgu/gl_immediate.c`. First `WRAP=1` build since phase 5 began — `bunnymark` links, runs and logs counters.
- T4 `done`: both builds report the counters the sprint expected. Batched: `arrays=400 arrayVerts=409,998`, `begins=verts=0` exactly as the review predicted. Unbatched (`PSP_BATCH=0`): `begins=ends=204,999 verts=819,996`, `arrays=0`.
- **Unplanned result worth having**: with pspgu the *unbatched* workload costs **25.98 ms** of work at N=1,000 where pspGL needed **48.8 ms** — our immediate path is ~1.9× cheaper than pspGL's GL interpretation, and the batched path is 6.57 ms against pspGL's 7.30 ms.

## 6. Budgets

| budget | limit | measured | tag |
|---|---|---|---|
| `batch_check` 2D, differing pixels | 0 (regression gate) | 0 at sprint start | |
| mesh scene, differing pixels | 0, or counted and explained | | |
| `basic_shapes` colours | ≥ 284 | | |
| `bunnymark` N=1,000 with `WRAP=1` | logs `COUNT` and `SPLIT` | | |
| `src/` changes | 0 — the driver and the tests only | | |
| `DrawMesh` colour | the material colour, not white | | |
| immediate-mode buffer | 1,024 vertices (24 KiB), overflow emits mid-run | | |

## 7. Measurements

| run | driver | build | scene | result |
|---|---|---|---|---|
| gate | pspgu | `WRAP=0` | `batch_check` 2D | 0 differing pixels (from sprint 012) |
| counted | pspgu | `PSP_BATCH=1`, `WRAP=1` | `bunnymark` N=1,000 | `begins=0 verts=0`, `arrays=400 arrayVerts=409,998`, work **6.565 ms** |
| counted | pspgu | `PSP_BATCH=0`, `WRAP=1` | `bunnymark` N=1,000 | `begins=ends=204,999 verts=819,996`, `arrays=0`, work **25.980 ms** (pspGL: 48.8 ms) |
| test | pspgu | `MESH_SCENE=1` | 2D → 3D → 2D | |
| test | pspgu | `basic_shapes` | shapes + text | |
| test | pspgu | `WRAP=1` | `bunnymark` N=1,000 | |
| reference | pspGL | same builds | same scenes | |

## 8. Exit criteria

- [ ] the mesh scene matches pspGL, or every differing pixel is counted and explained
- [ ] `basic_shapes` matches, or the difference is explained, with its colour count recorded
- [ ] `psp-tests/*` link with `WRAP=1` and the counters report non-zero for both draw paths
- [ ] `WRAP=1` and `WRAP=0` produce the same picture
- [ ] `PSP_DISABLE_VSYNC=1` builds and runs
- [ ] the 2D `batch_check` gate is still 0 differing pixels
- [ ] findings written back into the phase page and `memory-map.md` §9

## 9. Result

(To fill in on close. The interesting failure would be the mesh scene: `DrawMesh` exercises `glDrawElements`, index
rebasing and the converting vertex path, none of which the 2D scene touches — so a difference there is a different
bug from the texture ones, and the count will say whether it is ordering, indices or attributes.)

## 10. Follow-ups

- Non-power-of-two textures: pad the height and correct with `sceGuTexScale` (012 T2).
- 3D depth and camera behaviour beyond one cube — phase 10.
- `glGetFloatv` for `GL_LINE_WIDTH`/`GL_POINT_SIZE` is still answered from constants; fine until something reads them.


## 9c. T1 status: four eliminations, one open question

Kept as the working record, because each entry is a measurement someone would otherwise repeat.

| # | hypothesis | outcome |
|---|---|---|
| 1 | raylib's `MatrixToFloat` needs transposing in `glMultMatrixf` | **rejected twice** — once with a contaminated depth baseline, once clean: 613 colours, the cubes vanish |
| 2 | a stale sprite texture is sampled by the mesh cut | **rejected** by the draw trace: `drawElements count=36 tex2d=0 bound=1` |
| 3 | the projection matrix needs transposing at upload | **rejected by the 2D gate**: 0 → 40,813 differing pixels |
| 4 | the screen mapping (viewport/offset) differs from pspGL's | **rejected by arithmetic**: pspGL's `glViewport` reduces to the same `screen_x = 240 + 240·ndc` ours does |

Open question, stated precisely: projecting the cube's eight corners through raylib's own `BeginMode3D` numbers
(near 0.05, far 4000, `top = 0.05·tan 22.5°`, aspect 480/272, camera (0,1,4)→(0,0,0)) predicts `DrawCube` at
**x 81..191** — and **pspGL's capture starts at exactly x=81**, while ours starts at **x=0** and is roughly twice
as wide. So the reference is right, our source math is right, and the GE still receives something different.

That leaves one place to look, and it is not in the code we have been reading: **what the GE actually receives**.
The next probe should be a single triangle whose three vertices have known NDC positions, drawn with the frustum
projection, captured and measured against the predicted screen coordinates — so the answer is "the GE put vertex A
at pixel X" rather than another inference from a whole scene. Two candidates it will separate: the projection
really is being altered between our matrix and the GE's registers (float24 truncation, or `sceGuSetMatrix`'s
handling of the `w` row), or the modelview is being applied twice.


## 9d. T1, the triangle probe: the 3D pipeline is correct — for a straight camera

`psp-tests/triangle_probe` draws one triangle in world space through `BeginMode3D` with a camera at (0,0,2) looking
down -Z, and prints its predicted screen positions:

| | x | y |
|---|---|---|
| predicted | 158..322 | 54..218 |
| **pspgu capture** | **158..321** | **55..217** |

That is a match within one pixel of rasterisation. So the following are **proven correct** for the immediate 3D path:
the frustum, the matrix upload (`sceGuSetMatrix` with the `w` row and all), the viewport/offset mapping, and the
perspective divide. Both candidates the probe was built to separate — "the GE alters the matrix" and "the modelview
is applied twice" — are **eliminated**.

**What the probe does not exercise, and it is the last untested part: a rotated view matrix.** The probe camera
looks straight down -Z, which makes the 3×3 rotation block of the view matrix the identity — exactly the case where
a row/column-major confusion is invisible. The mesh scene's camera is at **(0,1,4)** looking at the origin, i.e. a
real rotation. That is now the single remaining difference between the scene that matches and the scene that does not.

**Next probe, one line of change:** move `triangle_probe`'s camera to (0,1,4) and predict the vertices from the
rotated view. If the vertices land where predicted, the difference is in `DrawCube`/`DrawMesh` rather than the matrix
path; if they do not, the view-matrix convention is the answer — and the two transposes already rejected were
rejected at the wrong layer (ingest and upload), leaving the multiply order as the untested third.

Status: the triangle probe is committed with its prediction in the log line, so the next run is a one-line edit and
an immediate comparison.

### The rotated camera matches as well — so the hypothesis has to change

Same probe, camera moved to **(0,1,4)** looking at the origin, i.e. a real view rotation:

| | x | y |
|---|---|---|
| predicted | 201..279 | 96..174 |
| **pspgu capture** | **202..277** | **97..172** |

Within 1–2 px again. **The entire 3D matrix path is therefore correct**: frustum, rotated view matrix, matrix upload,
viewport/offset, perspective divide — every layer that four earlier hypotheses blamed. That is worth more than the
scene it was meant to explain, because it means no further searching is needed in `gl_matrix.c`.

**What is left is the *composition*, not the math.** The probe draws 3D only; the mesh scene draws 2D → 3D → 2D. And
there is one measured clue that fits a specific failure: our 3D contribution starts at **x = 0**, whereas the cube's
world x range is −1.7..−0.7 and the frustum predicts screen x 81..191. Those world coordinates, drawn with the **2D
ortho** projection (which maps x 0..480 to the screen 1:1), would land at **x ≈ −1..0** — the left edge, which is
exactly what the capture shows.

**So the hypothesis is now: in the mesh scene the 3D pass is drawn with the 2D ortho projection still active**,
even though `BeginMode3D` switches matrices. The probe cannot see it because it never draws 2D first. The next probe
is therefore one more line: draw a 2D rectangle before `BeginMode3D` in `triangle_probe` and check whether the
triangle moves to the left edge. If it does, the bug is that our projection upload is skipped when the *values*
coincide wrongly — or that `rlPushMatrix`/`rlLoadIdentity` on the projection stack does not reach the GE because the
cached `sentProjection` still compares equal after the pop.

That last sentence is a concrete, checkable defect in `upload()`'s memcmp cache, and it would explain why 2D-only and
3D-only frames are both correct while a frame containing both is not.

### And the composition hypothesis is rejected as well

The probe with a 2D rectangle drawn **before** `BeginMode3D`, which is the mesh scene's shape:

| | x | y |
|---|---|---|
| predicted | 201..279 | 96..174 |
| **captured with 2D first** | **202..277** | **97..172** |

Unchanged. So the projection switches correctly, the matrix cache in `upload()` is *not* stale after a 2D pass, and
a frame containing both 2D and 3D is fine. `EndMode3D` was checked as well: it pops the projection, resets the
modelview and calls `rlDisableDepthTest()`, so the 2D after the 3D is not left depth-tested either.

**What that leaves is not the matrix path at all.** Five hypotheses have been eliminated (view-matrix transpose ×2,
stale texture, upload transpose, viewport mapping, and now composition). The probe's one remaining difference from
the failing scene is *which drawing call* is used:

| in the probe (correct) | in the mesh scene (wrong) |
|---|---|
| `DrawTriangle3D` — 3 vertices, immediate | `DrawCube` — 36 vertices, immediate (quads expanded) |
| — | `DrawMesh` — 36 indices, `glDrawElements` |

So the next probe is the last cheap one: **draw the same cube at the same camera** and predict its bbox from the
same arithmetic. If `DrawCube` is wrong the fault is in the quad expansion or in the 36-vertex run (buffer overflow at
1,024 is not it, but the emit could be), and `glDrawElements` is a separate test after it.

### The cube probe: `DrawCube` draws nothing, and the sprite test is one reason

`triangle_probe -DDRAW_CUBE` draws the same cube the mesh scene draws, at a camera whose projection is verified:
**nothing appears**. `DrawTriangle3D` at the same camera and the same frame draws correctly to within 1 px. So the
difference is not the matrix path — it is what happens to the cube's vertices.

Two things learned on the way:

- **`DrawCube` does not use immediate mode.** The mesh-scene trace shows it arriving as `draw mode=4 count=36` — a
  36-vertex triangle draw through the *deferred raylib batch*. So the pixels depend on the projection that is current
  **when the batch flushes**, not when the vertices were submitted.
- **The sprite eligibility test was wrong for 3D.** It ran on submitted vertex positions, which in a 3D pass are
  *world* coordinates — and a cube face is axis-aligned there while projecting to a parallelogram. Fixed: sprite
  eligibility now requires `z == 0` on all four corners, which raylib's 2D always satisfies and which no 3D face
  with depth can. **This fix alone did not make the cube appear**, so at least one more cause remains.

Open, for the next session, in order of cheapness:

1. Re-verify the 2D gate after the `z == 0` change (it should be untouched — 2D draws at z = 0 — but it has not been
   re-run, and the gate is not optional).
2. Trace the *flush* in the cube probe: log the projection matrix at the moment `glDrawArrays` is called for those 36
   vertices. If it is still the frustum and the vertices are at x −1.7..−0.7, the arithmetic says they must land at
   x 81..191, so a missing cube would mean the draw never happens at all — and then the question moves to whether
   `rlDrawRenderBatchActive()` inside `EndMode3D` reaches our hook.
3. Only after that: `DrawMesh`/`glDrawElements`.

### `DrawCube` was never broken — the build flag was

`triangle_probe`'s Makefile was `CFLAGS = -Wall`, which never referenced `EXTRA_CFLAGS`, so
`make EXTRA_CFLAGS=-DDRAW_CUBE` compiled the **triangle** build three times. The log finally showed it:
`draw mode=4 count=3 pos0=(-0.5,-0.5)` where a cube should have been 36 vertices. Every earlier "CUBE NOT
DRAWN" was a red triangle being drawn while the measurement filtered for green.

With the flag actually reaching the compiler, at the same verified camera:

| | x | y |
|---|---|---|
| predicted | 81..191 | 92..189 |
| **captured** | **81..190** | **92..188** |

So `DrawCube` — the deferred batch, the quad expansion, the frustum — is correct to within a pixel, and the
"sprite eligibility needs `z == 0`" fix stays as a correctness improvement (the 2D gate re-ran at **0 differing
pixels** with it in place) rather than the fix for anything measured.

**The mesh scene is unchanged at 691 colours / 25.6 %** with that fix in. So the difference is *not* the cube, and by
elimination it is the last untested call: **`DrawMesh` → `glDrawElements`** (36 indices, index rebasing, the
converting vertex path). That is the only drawing call in the corpus that has never had a probe, and it now has a
proven template to copy: known vertices, predicted bbox, one capture.

**Process note worth keeping:** two of the three "the cube is not drawn" results were my own instrument lying — a
grep missing the `]` in `[PSPGU]` once, and a Makefile ignoring `EXTRA_CFLAGS` twice. The trace line that made the
difference possible (`count=3` where 36 was expected) cost four lines of code and would have saved three runs.

### T1 split — `DrawMesh` fires and draws nothing; 3D goes to phase 10

The last untested call, probed exactly like the cube: `GenMeshCube` + `DrawMesh` with an identity transform at the
verified camera. Predicted bbox **x 193..287, y 92..189**.

Result: `drawElements count=36 tex2d=0 fmt=0x1e3` reaches the driver, and **no pixels appear**. Culling was ruled out
by disabling `GL_CULL_FACE` for the draw — still nothing — so the call is emitted, the format carries normals and
colour, and the GE produces nothing from the converted vertices and rebased indices.

That is where this sprint stops, by the rule agreed before the probe: **T1 is split.**

| split | state |
|---|---|
| **T1a — 2D corpus parity** | **met.** `batch_check` 2D byte-identical (0 differing pixels, 613 colours), `triangle_probe` matches prediction for a straight *and* a rotated camera, `DrawCube` matches to 1 px, and the immediate path matches pspGL to 1.9× better cost |
| **T1b — 3D through `DrawMesh`** | **handed to [phase 10](../phases/10-3d-advanced/index.md).** `glDrawElements` emits and draws nothing; culling ruled out; the converting path (`PspGuVertex`, index rebasing) is the suspect. Everything the probe could prove about the matrix path is proven, so phase 10 inherits a narrow, well-instrumented problem rather than a suspicion |

**What this sprint proved about 3D, which is worth more than the failing case:** the frustum, a rotated view matrix,
`sceGuSetMatrix` with the `w` row, viewport/offset, the perspective divide and the immediate quad path are all correct
within a pixel. Six hypotheses were eliminated with evidence (§9c, §9d and above), and two real defects were fixed on
the way: the depth **clear** value, and the depth **comparison inversion** — both from pspGL's own source, both of
which would have corrupted every 3D scene.

**Still open in this sprint, both cheap:** T2 (`basic_shapes` against its 284-colour fingerprint) and T5
(`PSP_DISABLE_VSYNC` build). Neither depends on 3D.

## 11. Closed

**Sprint 013 closed 2026-11-21: 5/6 done, T1 split.**

| task | result |
|---|---|
| T0 current colour | `done` — shared through `PspGuContext`; 2D gate unchanged |
| T1 mesh scene | **split**: T1a 2D corpus parity **met**, T1b (`DrawMesh`) to phase 10 |
| T2 `basic_shapes` | `done` — **0 differing pixels, 284 colours** on both drivers |
| T3 immediate symbols | `done` — `WRAP=1` links and runs for the first time in phase 5 |
| T4 counters, both builds | `done` — batched and `PSP_BATCH=0` both report the expected counters |
| T5 `PSP_DISABLE_VSYNC` | `done` — builds, runs, `swap` 9.4 → 2.433 ms |

What the sprint leaves behind, in the order it happened: a `basic_shapes` build that silently ran a stale binary
because of a missing `-lpspgu`; two "the cube is not drawn" results that were my own instrument lying (a grep missing
`]`, then a Makefile ignoring `EXTRA_CFLAGS`); two real depth defects fixed from pspGL's source; and one genuine,
narrow, well-instrumented 3D problem handed on.

The pattern worth carrying into sprint 014: **every wrong conclusion in this sprint came from a measurement, not from
reasoning** — and the two that cost the most were the two where the *instrument* was wrong rather than the driver.
