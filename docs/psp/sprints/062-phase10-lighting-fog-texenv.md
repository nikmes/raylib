# Sprint 062 (P10-S3) — Lighting, fog and the texture environment

| | |
|---|---|
| Roadmap phase | [Phase 10 — 3D and advanced](../phases/10-3d-advanced/index.md) |
| Sprint | P10-S3 |
| Branch / base commit | `master` @ `177cbe53` (sprint 061 closed) |
| Started / closed | 2026-11-22 (written) / 2026-11-22 |
| Build number(s) | pspgu `b27` (060 = b25, 061 = b26); `libraylib.a` default `PSP_BATCH=1` |
| Tasks | 6/6 done — lighting, texenv and fog at 0 diff; bake measured |

## 0. Handover

**Situation.** Sprints 060 and 061 give us a mesh that draws, a GE-native vertex layout and a measured null for
camera-relative rendering (pspgu composes view×model on the CPU). The engine still has no lighting at all: this fork's `src/` contains no `Light` type, no `CreateLight`, and
no fog call — the shader examples define their own helpers, and the GL11 path emits only `glShadeModel`. This sprint
introduces a PSP-only lighting/fog/texture-environment surface, maps it onto GE state in pspgu, and uses pspGL's
fixed-function implementation as the mapping reference and pixel reference. Static lighting also gets a CPU bake,
because the blueprint's measured claim is that baked vertex colour is the cheapest world lighting.

**Read before acting**, in this order:

1. `../sprints/060-phase10-drawmesh-and-mesh-gate.md` and `../sprints/061-phase10-camera-relative-and-ge-native-models.md`
   §9 — the packing, the direct interleaved path (normals f32 at +8), the driver gate, and the camera-relative null.
   Both sprints are closed; their gates are the reference.
2. `../phases/10-3d-advanced/index.md` §Guidelines from the renderer blueprint — lighting economics, baked vertex
   colour, `GU_TFX_MODULATE` + `GU_TCC_RGBA`, pass order; §What we already know.
3. `tools/pspgu/gl_state.c:65-100` — `glEnable`/`glDisable`, which today ignores every cap it does not know;
   `gl_texture.c:307-340` — `applyTexture` hardcodes `GU_TFX_MODULATE` + `GU_TCC_RGBA`; `gl_draw.c` — the format
   bits, `vertexLayout`/`packVertex` and the direct-layout detector.
4. `src/rmodels.c:1450-1495` — `DrawMesh` sets the material colour with `rlColor4ub`, enables `GL_COLOR_ARRAY` only
   when `mesh.colors != NULL`, and enables normals when the mesh has them.
5. pspGL's reference mapping: `glLight.c:200`, `glMaterial.c:4`, `glLightModel.c:4`, `glFog.c:4`, `glTexEnv.c:11`.
   Copy the mechanism, not the values; `glFog.c` has a known inverted-parameter workaround. pspGL is the A/B
   reference and uses the float vertex path (061 gates packing to pspgu), so lit parity is comparable.
6. 061's `UploadMesh` packer and pspgu's direct path (`fmt=0x163`, normals present): lighting consumes the same
   buffer; do not bypass it to test lighting.
7. The GU reference (local notes §1): `sceGuLight`, `sceGuLightColor`, `sceGuLightSpot`, `sceGuLightAtt`,
   `sceGuLightMode`, `sceGuAmbient`, `sceGuModelColor`, `sceGuFog`.
8. `../memory-map.md` §4 and §6 — VRAM and the measurement protocol; §9 — the traps.

**First action.** Add `LIGHT=ge|bake|none`, `FOG=linear|none` and `TEXENV=modulate|replace|decal|add` arms to
`scene_3d`, and capture the **pspGL** reference for a lit, a fogged and a texenv cube before touching pspgu. pspGL
already implements these GL calls; that capture is the comparison target for every later task.

**Hard rules.**

- **Bump the build tag to `b27` in T2.** A `b26` boot line means the lighting code is not in the archive.
- **Light positions are world coordinates.** the GU API takes the vector as given; GL would transform a position by
  the current modelview at call time. pspgu deliberately takes it as world space, so set lights after
  `BeginMode3D` with world coordinates and document the difference.
- **Under lighting, `glColor*` maps to the model colour, not `sceGuColor`.** GE lighting ignores the vertex colour;
  per-vertex colour arrays are documented as ignored while `GL_LIGHTING` is on.
- **The 2D gate and the 060/061 3D gates are sacred.** The default texture environment stays MODULATE, and a frame
  with lighting and fog disabled must be byte-identical to the 061 build.
- **No state leaks.** Lighting, fog and texenv are opt-in; `EndMode3D` restores what `BeginMode3D` changed. A 2D
  frame drawn after a lit frame proves it.
- **A rejection is not a stub.** `GL_LIGHT_MODEL_TWO_SIDE`, `GL_EXP`/`GL_EXP2` fog, `GL_COMBINE*` texenv and more
  than four lights must log once and be ignored — never silently approximate. The phase's rule is "no silent stubs".
- **Normals are unit-length.** The GE has no `GL_NORMALIZE`; `glEnable(GL_NORMALIZE)` is documented as unsupported
  rather than faked. GenMesh data is unit-length; a loaded model is not assumed to be.
- **pspGL is a reference, not gospel.** Its fog mapping sends `1/(far-near)` and `far` to different registers than
  `sceGuFog(near, far, colour)` implies. Calibrate against captures; do not copy the constants.
- **Lighting is per-vertex.** Specular is per-vertex and single-sided; the sprint records that limit and does not
  attempt a per-pixel approximation.
- **No full GE wait in a measured scene.** The diagnostic arms may sync, as in 061; they are excluded from timing.
- **Suspect the instrument.** Prove each arm compiled (`EXTRA_CFLAGS`, a boot line naming the arm) and that the
  pspGL capture actually contains lighting before comparing.
- **Stage only this sprint's paths.** `git status` before every commit, never `git add -A`. Commit messages carry the
  task ID: `sprint 062 T2: ...`.

**Commands.**

```sh
export PSPDEV=/home/psp/pspdev
export PATH=$PSPDEV/bin:$PATH
set -e

cd /home/psp/raylib-psp
tools/pspgu-build.sh && tools/pspgu-build.sh --check
tools/pspgl-build.sh --check                              # 2ab7378e

build_lib() { ( cd src && make clean && make PLATFORM=PLATFORM_PSP -j8 $1 \
                && cp libraylib.a $PSPDEV/psp/lib/libraylib.a ); }

run_app() {   # $1 = app dir, $2 = tag, $3 = make vars
    ( cd "$1" && make clean && make $3 && cp *.prx /tmp/s3/app.prx )
    ( cd /tmp/s3
      timeout 20 pspsh -e "reset"; sleep 6
      ( printf './app.prx\n'; sleep 40 ) | timeout 70 pspsh
      pspsh -e "scrshot host0:/$2.raw"; pspsh -e "scrshot host0:/$2-b.raw" )
}

# Reference first (pspGL installed), then the same arms on pspgu.
run_app psp-tests/scene_3d p10s3-light-ref 'LIGHT=ge'
run_app psp-tests/scene_3d p10s3-fog-ref   'FOG=linear'
run_app psp-tests/scene_3d p10s3-tex-ref   'TEXENV=replace'
```

**Do not touch.** Render-to-texture (S4); culling/tessellation (S5); the 2D batcher, window splitter and texture
upload; `src/platforms/rcore_psp.c` timing hooks; any Phase 8/9/11/13 file with uncommitted work.

**Done means** §8 below, with evidence in each task row.

## 1. Objective

Falsifiable: a cube lit by one directional light plus ambient is pixel-identical to pspGL (or has a bounded,
explained difference — a systematic brightness or position error is a fail); the same holds for MODULATE/REPLACE
texenv and for a linear fog gradient within 1 px along a scanline; a static mesh baked at load is visually
indistinguishable from GE lighting under the same lights; and the 2D capture is byte-identical to the 061 build
when lighting, fog and texenv are off.

## 2. Why now

Phase 10's exit criteria require mapping lighting/fog to GE state and documenting what cannot be expressed. Doing it
before the 3D corpus grows keeps the unsupported list small and honest, and the bake path gives static scenes a
cheap default before the culling sprint starts counting draws.

## 3. Scope

**In:** a PSP-only wrapper API in `rlgl.h`; pspgu's `glEnable`/`glDisable` caps, `glLightfv`, `glLightModelfv`,
`glMaterialfv`, `glFogf`/`glFogfv`, `glTexEnvi`/`glTexEnvfv`; the scene arms and captures; a CPU bake helper; the
Phase 10 feature/limits table.

**Out:** shaders and per-pixel lighting; two-sided lighting; exponential fog; `GL_COMBINE` texture combiners;
more than four lights; shadow maps; render-to-texture (S4); performance work beyond recording cost.

## 4. Preconditions

- [x] 061 closed: packing/direct path on `b26`, 0-diff gates; the packed layout carries normals f32
- [ ] pspGL `2ab7378e` available; its lit, fogged and texenv captures exist **before** pspgu work starts
- [ ] `scene_3d` arms compile and are proven present in the binary
- [ ] pspgu content hash recorded; `tools/pspgu-build.sh --check`
- [ ] Reset between runs; app logs `DONE` and sleeps; two identical captures per point
- [ ] The 2D reference capture and the 061 cube capture are archived

## 5. Tasks

| ID | task | status | verification | evidence |
|---|---|---|---|---|
| T1 | PSP-only `rlpsp*` wrappers in `rlgl.h`; `scene_3d` arms `LIGHT=ge|bake|none`, `FOG=linear|none`, `TEXENV=modulate|replace|decal|add` | `done` | boot name; pspGL captures | wrappers call GL; both drivers build |
| T2 | GE lighting: caps, `glLightfv` position/colours/attenuation/spot, ambient, material incl. `sceGuSpecular`, colour mapping; `b27` | `done` | lit cube vs pspGL; cost | **0 diff**, 9,804 px; work 0.466 ms |
| T3 | Texture environment modes + env colour | `done` | parity per mode | MODULATE/REPLACE/DECAL/ADD all **0 diff** |
| T4 | Fog GL_LINEAR, calibrated | `done` | gradient edge vs pspGL | **0 diff**, 27 colours; work 0.518 ms |
| T5 | CPU bake and comparison | `done` | visual diff bounded; policy | dominant faces match (56/64); 3,884 px slivers, max Δ128; not default |
| T6 | Regression and write-back | `done` | 2D, 060/061 gates, shapes | 0/0/0, 284 colours |

### Design notes (as shipped)

**Colour alpha matters.** Material and light colours with alpha 0 made every lit fragment fully transparent under
the always-on GL blend; the cube disappeared although lighting was correct. All lighting colours are opaque, and
`applyGlobalColor` also emits `sceGuColor(col)` so the fragment-alpha register is never left at GU's reset value.

**Colour mapping.** With `GL_LIGHTING` on, `glColor*` updates the model **ambient** (pspGL overloads ambient the same
way) and the global colour; per-vertex colour arrays are ignored by GE lighting. `rlpspMaterial` sets ambient,
diffuse, specular, emission and shininess (`sceGuSpecular`).

**Normals on the packed path.** The quantisation matrix carries a uniform scale `S = 32768/K`. The GE's normal
transform is the inverse-transpose, so unit normals come out divided by `S` and lighting doubled. Packed normals are
pre-multiplied by `S`; lit packed output then matches the float path exactly. Non-uniform caller scale is not
corrected (no GE normalise) and remains a documented limit.

**Light positions.** Taken as world coordinates. The test sets them once, before `BeginMode3D`, so pspGL's
modelview-at-call-time rule does not rotate them and both drivers use the same direction.

**Fog.** `sceGuFog(start, end, colour)` maps directly: the gradient is byte-identical to pspGL for
start 3 / end 6. EXP/EXP2 are rejected once.

**Bake.** A colour-bearing mesh bypasses the packed layout (added colour would otherwise be silently ignored); it
takes the float path with `GL_COLOR_ARRAY`. With the light negated to match the GE's direction convention, the bake
matches the two dominant face colours (56/64) but differs on thin perspective slivers: 3,884 px, max Δ128. GE
lighting is the default; the bake is a static-world option, not a drop-in replacement, and costs 0.529 ms versus
0.466 ms at one cube.

Rules:

- **IDs are stable.** Never renumber; a task that no longer applies becomes `dropped` with the reason.
- **`done` requires evidence** in §7. Code written is `doing`.
- **`blocked` names its dependency** — `blocked by 061 T4`, not "blocked".

### Task log (append-only)

- 2026-11-22 — sprint written, all tasks `todo`

## 6. Budgets

| budget | limit | measured | tag |
|---|---|---|---|
| frame time | no target; record work at 1/10/100 draws | | |
| lighting cost | GE per-vertex; record delta vs unlit | | |
| fog cost | GE per-vertex; record delta vs unfogged | | |
| bake cost | one pass at load; record ms per 1,000 vertices | | |
| VRAM | 0 delta | | |
| heap | baked colours 4 B/vertex; record per mesh | | |

## 7. Measurements

Protocol: device only; pspGL reference first, then pspgu; two identical captures.

| run | build | workload | metric | value | notes |
|---|---|---|---|---|---|
| reference | pspGL | `SCENE=flat LIGHT=ge` | pixels | 9,804 | float path |
| change | b27 | same | diff / work | **0** / 0.466 ms | normal pre-scale fix |
| reference | pspGL | `SCENE=tex TEXENV=replace` | pixels | 9,804 | |
| change | b27 | replace / decal / add | diff | **0 / 0 / 0** | MODULATE from 061 |
| reference | pspGL | `SCENE=flat FOG=linear` | pixels | 27 colours | start 3, end 6 |
| change | b27 | same | diff / work | **0** / 0.518 ms | `sceGuFog` maps directly |
| change | b27 | `LIGHT=bake` vs GE | diff / work | 3,884 px, max Δ128 / 0.529 ms | dominant faces match |
| gate | b27 | 2D / probe / both | diff | **0 / 0 / 0** | |
| gate | b27 | `basic_shapes` | colours | **284** | |

## 8. Exit criteria

- [x] §1 met: lighting, texenv and fog are 0 diff from pspGL; the bake difference is bounded and explained
- [x] lighting, fog and texenv are opt-in; disabling them restores the 061 capture byte-identically
- [x] unsupported list recorded and loud once: two-sided, EXP/EXP2 fog, `GL_COMBINE*`, >4 lights, `GL_NORMALIZE`
- [x] pspGL parity for each mapped feature
- [x] bake compared under identical lights; policy: GE lighting default, bake static-only
- [x] 2D gate 0; 060/061 cube gates 0; `basic_shapes` 284; no full GE wait

## 9. Result

Sprint 062 closed 6/6. Lighting, texture environment and fog now exist and match pspGL **byte-for-byte** on the
test scenes; the bake is measured and deliberately not promoted.

- **Lighting:** a directional light plus ambient, material colours and `sceGuSpecular`, mapped in pspgu. Lit cube:
  **0 diff**, 9,804 px. Two mistakes cost runs and are now traps: alpha-0 material/light colours made lit fragments
  transparent under the always-on blend, and the packed path's 0.5 model scale doubled lighting because the GE
  normalises via the inverse-transpose. Pre-scaling packed normals by `S` fixes it.
- **Texture environment:** MODULATE (061), REPLACE, DECAL and ADD are each **0 diff** from pspGL.
- **Fog:** `sceGuFog(3, 6, red)` is **0 diff** from pspGL's gradient — the expected calibration turned out to be a
  direct mapping for GL_LINEAR.
- **Bake:** colour meshes bypass the packed layout (061's layout has no colour slot, so a bake would be silent). The
  bake matches the dominant face colours but differs on thin slivers: 3,884 px, max Δ128, 0.529 ms vs 0.466 ms.
  Policy: GE lighting is the default; bake remains for static worlds where the light is fixed.
- **Gates:** 2D 0, 060 probe 0, 061 both 0, `basic_shapes` 284. Boot tag `b27`.

## 10. Follow-ups

- Non-uniform caller scale on packed meshes is not normal-corrected (no GE `GL_NORMALIZE`); reject or document per
  scene. Lighting currently assumes uniform scale.
- Two-sided lighting and `glColorMaterial` remain unsupported and log once.
- EXP/EXP2 fog and `GL_COMBINE*` remain rejected; a scene that needs them needs a shader or a different backend.
- Spot lights are implemented but untested in a scene; add a probe if one is used.
- Bake policy: static worlds only, and it uses the float path; a packed colour layout (32-B stride) is the
  follow-up if bake cost ever matters.
- Per-pixel specular and shadows are out of scope; document the per-vertex limit in the sample docs.

## Correction — sprint 069, 2026-11-22

The lit gate ("0 diff, 9,804 px") above **cannot be reproduced** and is withdrawn as a parity claim.

- The exact combination from this sprint — the 062 app, the 062 library and the 062 driver, all rebuilt from
  `288d33bb` — renders the lit cube at **248 / 128 / 160** on the top / front / right faces, 9,804 px away from
  both the archived pspGL capture and the archived "pspgu" capture. pspGL (`2ab7378e`) renders **192 / 56–64 /
  80–88** and still reproduces its archived capture byte-for-byte; the two archived captures are identical to
  each other.
- pspGL is not a valid oracle here: 56 on a face that receives the 0x40 ambient term (64) cannot come from an
  additive Lambert model. The 062 "0 diff" therefore did not measure two correct implementations agreeing.
- The archive-swap bug recorded in `tools/pspgu-build.sh` (a pspgu install could overwrite the pspGL backup) is
  the only explanation consistent with all three measurements: the archived "pspgu" lit run measured pspGL.
- What stands from this sprint: the lighting API and its state mapping, `sceGuSpecular`, the texture-environment
  modes, the fog mapping (each still gated), and the bake comparison. Only the lit byte-for-byte claim is withdrawn;
  the lit gate now records the measured face colours.

Sprint 069 §7 and the Phase 10 page carry the numbers.

