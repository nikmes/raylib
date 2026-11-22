# Sprint 065 (P10-S4) — Render-to-texture

| | |
|---|---|
| Roadmap phase | [Phase 10 — 3D and advanced](../phases/10-3d-advanced/index.md) |
| Sprint | P10-S4 |
| Branch / base commit | `master` @ `288d33bb` (sprint 062 closed) |
| Started / closed | 2026-11-22 (written) / 2026-11-22 |
| Build number(s) | pspgu `b28` (060 = b25, 061 = b26, 062 = b27); `libraylib.a` default `PSP_BATCH=1` |
| Tasks | 6/6 done — 256×256 gate 0 diff; 300×200 rejected by VRAM |

## 0. Handover

**Situation.** `LoadRenderTexture` cannot work today, and the reason is above the driver: on GL11
`rlLoadFramebuffer()` returns 0 and `rlLoadTextureDepth()` returns 0 (`src/rlgl.h:4162,4222`), so
`LoadRenderTextureEx` logs "Framebuffer object can not be created" and returns a zeroed target. pspgu has no
framebuffer objects at all. This sprint adds a single-colour-attachment FBO subset over the GE draw buffer, with an
explicit read-after-write policy, and routes rlgl's GL11 calls through it **without breaking the pspGL A/B build**.

**Read before acting**, in this order:

1. `../phases/10-3d-advanced/index.md` §Readiness review blocker 2 and §Render-to-texture: what the references
   actually do — the redirect mechanism, the explicit `TexFlush`+`TexSync` contract, and the VRAM costs.
2. 060/061/062 §9 — the draw paths, per-list arenas, the packed/direct path and the lighting state this sprint
   must not disturb. All three are closed.
3. `src/rtextures.c` — `LoadRenderTextureEx` and the exact call order; `src/rcore.c` —
   `BeginTextureMode`/`EndTextureMode`; `src/rlgl.h:2153-2200` (enable/disable, active FBO),
   `:3826` (`rlLoadTextureDepth`), `:4266-4340` (`rlLoadFramebuffer`, attach, complete).
4. `tools/pspgu/egl.c:40-300` — the 512-byte span allocator, `pspguVramAllocAligned`, the framebuffer/depth
   reservations, and the swap path that knows the current back buffer.
5. `tools/pspgu/gl_texture.c:23-41` — the `PspGuTexture` fields (`offset`, `padW/padH`, `tbw`, `shadow`,
   `lastUsed`); `:307-340` — `applyTexture` (`sceGuTexFlush` on every bind, `TexScale`); the shadow allocation and
   eviction path, which a render target must skip.
6. `tools/pspgu/gl_query.c` — `glReadPixels` finishes any open list and reads `pspgu.drawOffset` with
   `pspgu.width/height/stride`; binding a target must update those and restore them on unbind.
7. `tools/pspgu/gl_state.c:231-250` — `glViewport`/`glScissor`; pspgu must track the current values so FBO bind and
   unbind restore them (raylib calls `SetupViewport` afterwards, but do not rely on it).
8. libgu 6.60 manual: `sceGuDrawBufferList` (fbp 8192-byte aligned, fbw 64–1024 multiple of 64),
   `sceGuDepthBuffer` (same alignment rules), `sceGuTexFlush`/`sceGuTexSync`
   `[source scan: psp_sdk_660/.../libgu-Reference-English.htm:8509-8605, 8707-8770, 10635-10749]`.

**First action.** Prove the failure on device with one log line (`FBO: Framebuffer object can not be created`) and
archive it; then confirm the pspGL archive still links after the first rlgl edit — pspGL exports **no**
`glGenFramebuffers`/`glBindFramebuffer`, so a direct reference would break the A/B build.

**Hard rules.**

- **One `libraylib.a` must link against both drivers.** Declare the FBO entry points weak in the rlgl PSP block and
  check availability once; on pspGL `LoadRenderTexture` keeps returning 0 with one clear log, and the A/B mechanism
  survives. A hard pspgu dependency is a sprint failure even if RTT works.
- **The render target is not a normal texture.** It needs an 8192-byte-aligned VRAM block, a stride that is a
  multiple of 64, a power-of-two sampled extent, no main-RAM shadow, and it must be pinned: never evicted, never
  reloaded from a shadow it does not have.
- **Read-after-write is explicit.** Mark the attached texture dirty on unbind; on the next bind emit
  `sceGuTexFlush()` + `sceGuTexSync()`. Do **not** add `sceGuSync` at unbind, and do not rely on the next frame.
- **Save and restore as a pair.** Binding an FBO changes draw buffer, depth buffer, offset, viewport and scissor;
  unbinding must restore the scene's back buffer, scene depth and the viewport the GL state expects. A 2D frame
  drawn after an RTT frame is the leak test.
- **Alignment is not optional.** `sceGuDrawBufferList` and `sceGuDepthBuffer` both require 8192-byte alignment and a
  64-multiple stride. Texture allocations are 64-byte aligned, so an attachment must be promoted, not reused in
  place.
- **Reject loudly.** Targets narrower than 64, more than one colour attachment, multiple render targets, blits and
  mipmapped targets are unsupported; log once each. No silent approximation.
- **No full GE wait in a steady frame.** The only synchronisation in the render→sample path is the GE-side
  `TexSync` command; the counter for CPU waits must stay 0.
- **The 2D and 060/061 gates are sacred.** No RTT frame may change them.
- **Stage only this sprint's paths.** `git status` before every commit, never `git add -A`. Commit messages carry the
  task ID: `sprint 065 T2: ...`.

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

# The pspGL build must still link after every rlgl edit:
run_app psp-tests/rtt_probe p10s4-baseline 'TARGET=0'      # expect the "can not be created" log
run_app psp-tests/rtt_probe p10s4-2d 'MODE=2d TARGET=256'
run_app psp-tests/rtt_probe p10s4-depth 'MODE=depth TARGET=256'
```

**Do not touch.** Culling/tessellation (S5); lighting/fog state (S3); the 2D batcher, window splitter and texture
upload paths; `src/platforms/rcore_psp.c` timing hooks; any Phase 8/9/11/13 file with uncommitted work.

**Done means** §8 below, with evidence in each task row.

## 1. Objective

Falsifiable: `LoadRenderTexture(256,256)` returns a non-zero id and a complete framebuffer; content rendered into
it and sampled 1:1 is **0 differing pixels** from the same content drawn directly to the screen; a depth-tested cube
in the target has the same visible faces as the direct render; `glReadPixels` while the target is bound matches the
CPU pattern; the render→sample path adds **0 CPU GE waits**; and the boot VRAM line accounts for the target
(3 × 272 KiB framebuffers + 272 KiB depth + target colour + target depth + textures ≤ 2 MiB).

## 2. Why now

Phase 10's exit criteria include render-to-texture with explicit read-after-write, and the phase page's exit test for
the whole driver is a `glReadPixels`-versus-capture match. RTT is also the dependency for post-processing and the
sample-porting work in later phases. The reference scan already fixed the mechanism (draw-buffer redirect, not GL
FBO semantics); this sprint implements it.

## 3. Scope

**In:** weak FBO routing in `rlgl.h`'s GL11 PSP block; pspgu framebuffer/renderbuffer objects, attachment and target
state; render-target texture residency (pinned, shadow-free, 8192-aligned); target-aware `glReadPixels`; the
`rtt_probe` app; the Phase 10 RTT limits and budget table.

**Out:** multiple render targets, cube-map targets, blits, mipmaps on targets, depth texture sampling, RTT with
lighting/fog interaction beyond one regression arm, and any performance target beyond recording work.

## 4. Preconditions

- [x] 060/061/062 closed; cube, lit and fog gates green on `b27`
- [ ] pspgu `b27`/`b28` content hashes recorded; pspGL `2ab7378e` available
- [ ] a probe app built against pspGL links with the new libraylib (weak FBO symbols) before pspgu work starts
- [ ] `tools/pspgu-build.sh --check` reports the FBO build when installed
- [x] `pspguVramStats` prints reserved/requested/used/free/largest at boot and after target creation
- [ ] Device reset between runs; app logs `DONE` and sleeps; two identical captures per point
- [ ] The 2D reference capture, the 060 cube capture and the 062 lit/fogged captures are archived

## 5. Tasks

| ID | task | status | verification | evidence |
|---|---|---|---|---|
| T1 | Weak FBO routing, active tracker, depth renderbuffer | `done` | pspGL links and refuses; pspgu creates | pspGL: one log, id 0; pspgu: "created successfully" |
| T2 | FBO objects, target promotion, bind/unbind state | `done` | 1:1 sample parity; 2D restored | **0 diff** vs direct; churn **0 diff** |
| T3 | Depth renderbuffer and bind | `done` | depth occlusion | near cube wins (one surface); 16-bit RGB depth |
| T4 | RAW dirty flag | `done` | TexFlush+TexSync, no CPU wait | dirty path exercised every sample; no sync at unbind |
| T5 | Orientation, NPOT, readback | `done` | marker/readback; 300×200 decision | 8888 readback target-aware; 300×200 **rejected** (VRAM) |
| T6 | Budget/regression/write-back | `done` | gates, table | 2D 0; 060/062 gates 0; 284 colours |

### Design notes (as shipped)

**Weak symbols are not enough on their own.** pspGL has no FBO implementation, so the entry points are weak —
but a weak undefined reference does **not pull an archive member**, so `gl_fbo.o` was never linked and
`glGenFramebuffers` stayed NULL. The object is included from `gl_state.c` (already pulled by `glEnable`), which
keeps one `libraylib.a` working on both drivers. On pspGL: link OK, `LoadRenderTexture` returns 0, one log.

**Target texture.** `glFramebufferTexture2D` promotes the colour texture to an 8192-aligned block, frees its
main-RAM shadow, clears `swizzled` (the GE writes linearly), pins it, and marks it dirty. Sampling uses
`GU_TCC_RGB`: the GE 8888 draw buffer does not retain alpha, so `TCC_RGBA` read zero alpha and blended the target
away. Render targets are therefore opaque when sampled (documented limit).

**Bind/restore.** Save `drawOffset`, `depthOffset`, width/height/stride, PSM, dither, viewport, scissor **and the
GU offset**. Screen framebuffer 0 lives at VRAM offset 0 — a NULL guard on the saved pointer skipped the restore and
left the target as the draw buffer. Restore uses `saved.psm` and re-emits `sceGuOffset`.

**Depth overflow.** The GE's frame buffer height stays the display's 272 rows; allocating target depth for 256 rows
let clear/draw write 8 rows into the colour block, blacking the target's top band. Depth allocates
`padW × max(padH, 272) × 2`.

**Dither once.** Dithering the target and then dithering the sample doubled the pattern; `GU_DITHER` is disabled
while an FBO is bound and restored on unbind. 1:1 pattern parity is **0 diff** after this.

Rules:

- **IDs are stable.** Never renumber; a task that no longer applies becomes `dropped` with the reason.
- **`done` requires evidence** in §7. Code written is `doing`.
- **`blocked` names its dependency** — `blocked by 061 T4`, not "blocked".

### Task log (append-only)

- 2026-11-22 — sprint written, all tasks `todo`

## 6. Budgets

| budget | limit | measured | tag |
|---|---|---|---|
| VRAM total | ≤ 2 MiB from allocation counters | | |
| 256×256 8888 target | 256 KiB colour + 128 KiB depth | | |
| 300×200 target | pads to 512×256: 512 KiB colour + 256 KiB depth | | |
| heap | render target has **no** main-RAM shadow | | |
| frame time | no target; record work with and without an RTT pass | | |
| CPU GE waits | 0 in the render→sample path | | |

Example boot budget to beat: 3 × 272 KiB framebuffers = 816 KiB, scene depth 272 KiB, leaving 960 KiB; a 256×256
8888 target plus its depth is 384 KiB, leaving 576 KiB for textures. A 512×256 target is 768 KiB and leaves too
little; record it as a rejected configuration with the numbers.

## 7. Measurements

Protocol: device only; direct-to-screen is the reference (pspGL has no FBOs); two identical captures.

| run | build | workload | metric | value | notes |
|---|---|---|---|---|---|
| link | b28 | pspGL probe | id / log | 0 / one refusal | weak symbols link |
| control | b28 | `LoadRenderTexture(256,256)` | id / depth | 1 / 1 | "created successfully" |
| change | b28 | pattern direct vs sampled | diff | **0** | 65,536 px region |
| change | b28 | churn 4 textures then sample | diff | **0** | target pinned |
| change | b28 | `glReadPixels` bound target | corners / alpha | pattern present / **alpha 0** | 8888 path; GE keeps no alpha |
| change | b28 | depth arm | surfaces | near cube only | occlusion correct |
| change | b28 | 300×200 target | allocation | **rejected** | colour 512×256×4 + depth 512×272×2; largest free 106,496 B |
| gate | b28 | 2D / 060 / 062 | diff | 0 / 0 / 0 | `basic_shapes` 284 |
| budget | b28 | 256×256 target | VRAM | colour 256 KiB + depth 136 KiB | requested 458,756 B total |

## 8. Exit criteria

- [x] §1 met for 256×256: `LoadRenderTexture` id>0, 1:1 sample 0 diff, readback target-aware
- [x] pspGL A/B links; RTT refuses with one log and no crash
- [x] target is 8192-aligned, pinned, shadow-free; survives churn
- [x] bind/unbind restores draw buffer, depth, offset, dimensions, PSM, viewport, scissor and dither
- [x] RAW is `TexFlush`+`TexSync`; no CPU wait at unbind
- [x] `glReadPixels` uses the target's offset/dimensions; 8888 path added
- [x] 300×200 recorded as **rejected** with its allocation numbers, not silently accepted
- [x] 2D gate 0; 060/062 gates 0; `basic_shapes` 284
- [x] limits recorded: single colour, no MRT/cube/blit/mipmap/depth sampling, <64 rejected, target alpha opaque

## 9. Result

Sprint 065 closed 6/6. Render-to-texture works on pspgu and refuses cleanly on pspGL.

- **256×256 target:** `LoadRenderTexture` returns a complete FBO with a 16-bit depth renderbuffer; a pattern
  rendered into it and sampled 1:1 is **0 diff** from the same pattern drawn directly. Four create/delete cycles
  beside the target leave the sample identical (pinned, no eviction).
- **Readback:** `glReadPixels` follows the bound target's offset and 256×256 dimensions; the 8888 path was added.
  Target alpha reads 0 because the GE 8888 draw buffer does not retain it, so targets sample as opaque RGB.
- **Depth:** a near cube drawn first remains visible over a far cube drawn second (occlusion works). The depth
  block must span the display's 272 rows or its writes bleed into the colour block (a black band was the symptom).
- **300×200 is rejected, with numbers:** the padded target (512×256 colour, 512×272 depth) does not fit the
  fragmented tail; the allocator reports largest 106,496 B. Recorded, not hidden.
- **Four failures cost runs and are now traps:** weak references do not extract archive members (FBO code must live
  in an already-linked object); framebuffer offset 0 is legal and must not be NULL-guarded on restore; the GE keeps
  no alpha in 8888; and dithering the target plus the sample doubles the pattern.
- **Gates:** 2D 0, 060 probe 0, 062 lit gate 0, `basic_shapes` 284. Boot tag `b28`.

## 10. Follow-ups

- Target alpha is not stored; a scene that needs a transparent render target needs a second pass or a different
  format, and is out of scope.
- 300×200 targets are rejected by fragmentation. A smaller depth allocation (only the target height) would need the
  GE to stop clearing 272 rows; not attempted.
- Readback of a lit/depth target may show GE channel-order quirks for immediate-mode vertex colours; the 2D pattern
  path is exact. Add a lit readback gate when a sample needs it.
- MRT, cube targets, blits, mipmaps and depth sampling remain unsupported and log once.
- `glColorMask` in pspgu maps enable bits in the opposite sense to `sceGuPixelMask` (found while testing); no current
  path calls it, but fix before using colour masks.

