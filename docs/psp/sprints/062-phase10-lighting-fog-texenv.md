# Sprint 062 (P10-S3) — Lighting, fog and the texture environment

| | |
|---|---|
| Roadmap phase | [Phase 10 — 3D and advanced](../phases/10-3d-advanced/index.md) |
| Sprint | P10-S3 |
| Branch / base commit | `master` @ the commit that closes sprint 061 |
| Started / closed | 2026-11-22 (written) / — |
| Build number(s) | pspgu after the 060/061 format work plus this sprint's state mapping (content hash recorded); `libraylib.a` default `PSP_BATCH=1` |
| Tasks | 0/6 done |

## 0. Handover

**Situation.** Sprints 060 and 061 give us a mesh that draws, a camera-relative convention and a GE-native vertex
layout. The engine still has no lighting at all: this fork's `src/` contains no `Light` type, no `CreateLight`, and
no fog call — the shader examples define their own helpers, and the GL11 path emits only `glShadeModel`. This sprint
introduces a PSP-only lighting/fog/texture-environment surface, maps it onto GE state in pspgu, and uses pspGL's
fixed-function implementation as the mapping reference and pixel reference. Static lighting also gets a CPU bake,
because the blueprint's measured claim is that baked vertex colour is the cheapest world lighting.

**Read before acting**, in this order:

1. `../sprints/060-phase10-drawmesh-and-mesh-gate.md` and `../sprints/061-phase10-camera-relative-and-ge-native-models.md`
   — the packing, the direct interleaved path, and whether its layout already has a colour slot. **This sprint needs
   061 T3/T4 `done`; lighting adds a second vertex-colour path that must not break the first two.**
2. `../phases/10-3d-advanced/index.md` §Guidelines from the renderer blueprint — lighting economics, baked vertex
   colour, `GU_TFX_MODULATE` + `GU_TCC_RGBA`, pass order; §What we already know.
3. `tools/pspgu/gl_state.c:65-100` — `glEnable`/`glDisable`, which today ignores every cap it does not know;
   `gl_texture.c:307-340` — `applyTexture` hardcodes `GU_TFX_MODULATE` + `GU_TCC_RGBA`; `gl_draw.c:422-480` —
   the format bits and `convertVertex`.
4. `src/rmodels.c:1450-1495` — `DrawMesh` sets the material colour with `rlColor4ub`, enables `GL_COLOR_ARRAY` only
   when `mesh.colors != NULL`, and enables normals when the mesh has them.
5. pspGL's reference mapping: `glLight.c:200`, `glMaterial.c:4`, `glLightModel.c:4`, `glFog.c:4`, `glTexEnv.c:11`.
   Copy the mechanism, not the values; `glFog.c` has a known inverted-parameter workaround.
6. libgu 6.60 manual: `sceGuLight` (§5770), `sceGuLightColor` (§5906), `sceGuLightSpot` (§6007),
   `sceGuLightAtt` (§6093), `sceGuLightMode` (§6169), `sceGuAmbient` (§6254), `sceGuModelColor` (§6541),
   `sceGuFog` (§8844) `[source scan: psp_sdk_660/.../libgu-Reference-English.htm]`.
7. `../memory-map.md` §4 and §6 — VRAM and the measurement protocol; §9 — the traps.

**First action.** Add `LIGHT=ge|bake|none`, `FOG=linear|none` and `TEXENV=modulate|replace|decal|add` arms to
`scene_3d`, and capture the **pspGL** reference for a lit, a fogged and a texenv cube before touching pspgu. pspGL
already implements these GL calls; that capture is the comparison target for every later task.

**Hard rules.**

- **The 2D gate and the 060/061 3D gates are sacred.** The default texture environment stays MODULATE, and a frame
  with lighting and fog disabled must be byte-identical to the 061 build.
- **No state leaks.** Lighting, fog and texenv are opt-in; `EndMode3D` restores what `BeginMode3D` changed. A 2D
  frame drawn after a lit frame proves it.
- **A rejection is not a stub.** `GL_LIGHT_MODEL_TWO_SIDE`, `GL_EXP`/`GL_EXP2` fog, `GL_COMBINE*` texenv and more
  than four lights must log once and be ignored — never silently approximate. The phase's rule is "no silent stubs".
- **Normals are unit-length.** The GE has no `GL_NORMALIZE`; `glEnable(GL_NORMALIZE)` is documented as unsupported
  rather than faked. GenMesh data is unit-length; a loaded model is not assumed to be.
- **pspGL is a reference, not gospel.** Its fog mapping sends `1/(far-near)` and `far` to different registers than
  libgu's `sceGuFog(near, far, colour)` implies. Calibrate against captures; do not copy the constants.
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

- [ ] 061 T3/T4 `done`: the direct interleaved path exists and its layout can carry a colour slot (or the sprint
      adds it without changing the 2D path)
- [ ] pspGL `2ab7378e` available; its lit, fogged and texenv captures exist **before** pspgu work starts
- [ ] `scene_3d` arms compile and are proven present in the binary
- [ ] pspgu content hash recorded; `tools/pspgu-build.sh --check`
- [ ] Reset between runs; app logs `DONE` and sleeps; two identical captures per point
- [ ] The 2D reference capture and the 061 cube capture are archived

## 5. Tasks

| ID | task | status | verification | evidence |
|---|---|---|---|---|
| T1 | Audit and surface: prove the engine emits no light/fog/texenv calls; add PSP-only `rlpsp*` wrappers in `rlgl.h` (lights, material, ambient, fog, texenv) that call GL, so both drivers accept them; add the scene arms | `todo` | boot log names each arm; pspGL captures exist for all three | — |
| T2 | GE lighting in pspgu: caps `GL_LIGHTING`/`GL_LIGHT0..3`; `glLightfv` (position, ambient/diffuse/specular, attenuation, spot); `glLightModelfv` ambient; `glMaterialfv` and `glShadeModel`; four lights max, single-sided | `todo` | lit cube vs pspGL; light count/type table; per-frame cost at 1/10/100 draws | — |
| T3 | Texture environment: `glTexEnvi`/`glTexEnvfv` for MODULATE, REPLACE, DECAL, ADD and the env colour; `GL_COMBINE*` rejected once; default MODULATE unchanged | `todo` | parity vs pspGL for each mode; 2D gate 0 diff | — |
| T4 | Fog: `glFogf`/`glFogfv` for GL_LINEAR and `GL_FOG_COLOR`; calibrate `sceGuFog` against pspGL's gradient; GL_EXP/GL_EXP2 rejected once | `todo` | gradient edge within 1 px of pspGL; cost at 1/10/100 draws | — |
| T5 | CPU bake: `rlpspBakeLighting(mesh, lights, ambient)` computes Lambert + ambient into `mesh.colors` once at load; compare with GE lighting under identical lights | `todo` | visual diff bounded; bake cost and per-frame saving measured | — |
| T6 | Regression and write-back: lit/fogged/texenv parity, 2D gate, 061 cube gate, `basic_shapes` 284; feature/limits table on the Phase 10 page | `todo` | tables in §7; 0 diff 2D; 284 colours; unsupported list recorded | — |

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
| reference | pspGL | `LIGHT=ge` | pixels | | pspGL fixed-function |
| change | T2 | `LIGHT=ge` | differing pixels / max channel Δ | | |
| change | T2 | lit cube, 1/10/100 draws | work (ms) | | |
| reference | pspGL | `TEXENV=replace` | pixels | | |
| change | T3 | each texenv mode | differing pixels | | MODULATE must stay 0 |
| reference | pspGL | `FOG=linear` | gradient edge x | | |
| change | T4 | `FOG=linear` | gradient edge x | | target ≤1 px |
| change | T5 | baked vs GE, same lights | differing pixels | | |
| control | 061 build | 2D frame | differing pixels | | must be 0 |

## 8. Exit criteria

- [ ] §1 met, with the tables in §7
- [ ] lighting, fog and texenv are opt-in; disabling them restores the 061 capture byte-identically
- [ ] the GE unsupported list is recorded and each item fails loudly once: two-sided, EXP/EXP2 fog,
      `GL_COMBINE*`, >4 lights, `GL_NORMALIZE`, per-pixel specular
- [ ] pspGL parity or a bounded, explained difference for each mapped feature
- [ ] bake vs GE lighting compared under identical lights, with a stated default policy
- [ ] 2D gate 0 diff; 061 cube gate 0; `basic_shapes` 284 colours; no full GE wait in measured scenes

## 9. Result

—

## 10. Follow-ups

- s8 normals, now that lighting makes their error observable → **P10-S3 follow-up or P10-S5**, with a lit capture
- Two-sided lighting and per-pixel specular, if a scene needs them → outside Phase 10, shader work
- Pass ordering for translucent/cutout materials under lighting → **P10-S5/S6**
- Fog and lighting interaction with render-to-texture → **P10-S4**
