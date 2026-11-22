# Sprint 065 (P10-S4) — Render-to-texture

| | |
|---|---|
| Roadmap phase | [Phase 10 — 3D and advanced](../phases/10-3d-advanced/index.md) |
| Sprint | P10-S4 |
| Branch / base commit | `master` @ `288d33bb` (sprint 062 closed) |
| Started / closed | 2026-11-22 (written) / — |
| Build number(s) | pspgu `b28` (060 = b25, 061 = b26, 062 = b27); `libraylib.a` default `PSP_BATCH=1` |
| Tasks | 0/6 done |

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
| T1 | rlgl GL11 routing: weak `glGenFramebuffers`, `glDeleteFramebuffers`, `glBindFramebuffer`, `glFramebufferTexture2D`, `glFramebufferRenderbuffer`, `glCheckFramebufferStatus`, `glGenRenderbuffers`, `glDeleteRenderbuffers`, `glBindRenderbuffer`, `glRenderbufferStorage`; static active-FBO tracker for `rlGetActiveFramebuffer`; `rlLoadTextureDepth` allocates a renderbuffer when the symbols exist; `rlUnloadFramebuffer` deletes the FBO and its attached renderbuffers (the colour texture is unloaded separately); bump `b28` | `todo` | pspGL app links and logs one refusal; pspgu `LoadRenderTexture` returns id>0 and "created successfully" | — |
| T2 | pspgu FBO + attachment: id table, single colour attachment; promote the texture to an **8192-byte-aligned** block (`pspguVramAllocAligned(bytes, 8192)`, raw copy if resident), free its main-RAM shadow, mark `renderTarget` + pinned; stride = `padW` (POT, multiple of 64); save/restore `drawOffset`, `depthOffset`, `width/height/stride`, viewport and scissor | `todo` | clear-to-colour + sample parity; 2D frame after RTT 0 diff; target survives texture churn | — |
| T3 | Depth: renderbuffer id → `{offset, stride, height}` table; `sceGuDepthBuffer(offset, padW)` on bind; scene depth restored on unbind; depth is GE 16-bit and cleared with the target | `todo` | depth-tested cube in target matches the direct render | — |
| T4 | Explicit RAW: mark the colour texture dirty on unbind; `applyTexture` emits `sceGuTexFlush` + `sceGuTexSync` once for a dirty render target; no `sceGuSync` at unbind; count flushes/syncs and CPU waits | `todo` | alternating render/sample frames correct; CPU wait counter 0; no tearing over 120 frames | — |
| T5 | Orientation, NPOT and readback: corner marker; 300×200 pads to 512×256 and samples with `TexScale(300/512, 200/256)`; `glReadPixels` while bound uses the target's `drawOffset` and `width/height/stride`; targets <64 rejected | `todo` | marker at expected corner; 1:1 parity 0 diff; readback 0 diff; rejection logged once | — |
| T6 | Budget, regression, write-back: VRAM accounting (3×272 KiB + scene depth + target colour/depth + textures ≤2 MiB); 2D/060/062 gates; `basic_shapes` 284; RTT limits table; unload order | `todo` | stats table; gates 0 diff; table matches allocations | — |

### Design notes (implementation-ready)

**Weak-symbol recipe.** pspGL has no FBO implementation — only `*EXT` prototypes in `GL/glext.h`, no symbols in
`libGL.a`. Declare each entry point with `__attribute__((weak))` in the rlgl PSP block and keep a one-time
`rlpspFboAvailable()` that returns false when `glGenFramebuffers == NULL`. On pspGL, `rlLoadFramebuffer` returns 0,
`rlLoadTextureDepth` returns 0, `rlFramebufferComplete` returns false, one clear log; no crash and no link error.
Build a probe app against pspGL **before** writing pspgu code to prove the link.

**pspgu objects.** Add two small tables:
`PspGuRenderbuffer { used, offset, width, height, stride }` and `PspGuFramebuffer { used, colorTex, depthRb }`.
Texture entries gain `int renderTarget; int pinned; int dirty;`. A render target has no shadow and is never
evicted or reloaded; `residencyAlloc` treats `pinned` as "used this frame".

**Attachment flow (colour).** `glFramebufferTexture2D`:
1. Look up the texture id; require `used` and a known `padW/padH` (POT ≥64).
2. If `offset % 8192 != 0`, allocate `padW*padH*bpp` with `pspguVramAllocAligned(bytes, 8192)`, copy the raw bytes
   from the old block if resident, free the old block, update `offset`.
3. Free `shadow` and set `shadowBytes = 0`; set `renderTarget = 1`, `pinned = 1`, `dirty = 1`.
4. Store `colorTex` in the FBO; return complete if the attachment succeeded.
Refuse (leave the FBO incomplete, log once) when the texture is currently bound or in flight, or the allocation
fails; do not move a texture a queued list may be sampling.

**Bind/unbind state.** `glBindFramebuffer(0, fbo)` saves `pspgu.drawOffset`, `depthOffset`, `width`, `height`,
`stride`, and the current viewport/scissor; it then sets the target's `sceGuDrawBufferList(psm, offset, padW)`,
`sceGuDepthBuffer(depthOffset, padW)`, offset/viewport/scissor to `0..width/height`, and the target's
`width/height/stride` for `glReadPixels`. `glBindFramebuffer(0, 0)` restores all of it. `rlGetActiveFramebuffer`
returns the tracker, which is what `EndMode3D` uses to skip screen scale.

**Sampling and RAW.** `applyTexture` uses the target's `padW/padH` and `TexScale = width/padW, height/padH` (the
existing padded-texture path). If `dirty`, emit `sceGuTexFlush()` then `sceGuTexSync()` once, clear `dirty`.
Sampling the currently bound target is refused and logged (feedback loop).

**Unload order.** `UnloadRenderTexture` calls `rlUnloadFramebuffer` first, then unloads the colour texture and depth
object. pspgu's delete must detach the colour texture (without deleting it), free attached renderbuffers, and clear
the FBO entry; the texture then returns to a normal (non-pinned) object or is freed by `glDeleteTextures`.

**Depth.** `glRenderbufferStorage` allocates `padW*padH*2` with 8192-byte alignment; the id table maps it to the
block. `rlLoadTextureDepth(..., true)` always asks for a renderbuffer in `LoadRenderTextureEx`; a depth *texture*
request is unsupported and returns 0. `ClearBackground` inside `BeginTextureMode` clears the bound target's depth.

**Limits.** One colour attachment, no MRT, cube targets, blits, mipmaps, depth sampling; `<64` targets rejected;
NPOT pads to POT and samples with `TexScale`. Each rejection logs once.

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
| control | pre-sprint | `LoadRenderTexture(256,256)` | id / log | | expected 0 / "can not be created" |
| link | T1 | probe app on pspGL | links / RTT result | | must link, return 0, one log |
| change | T1 | same | id / completeness | | target >0, "created successfully" |
| change | T2 | 2D pattern, sample 1:1 | differing pixels vs direct | | target 0 |
| change | T2 | 2D frame after an RTT frame | differing pixels | | target 0 |
| change | T3 | depth cube in target vs direct | differing pixels / faces | | |
| change | T4 | 120 alternating frames | CPU waits / artifacts | | waits 0 |
| change | T5 | 300×200 target | padded extent / parity | | 512×256 |
| change | T5 | corner marker in target | marker position | | orientation check |
| change | T5 | `glReadPixels` bound target | differing bytes vs CPU pattern | | uses target offset/dims |
| churn | T2 | texture create/delete beside target | target offset / id | | pinned, no eviction |
| change | T6 | boot + one target | VRAM reserved/used/free/largest | | |

## 8. Exit criteria

- [ ] §1 met, with the tables in §7
- [ ] the pspGL A/B build still links; on pspGL RTT refuses with one clear log and no crash
- [ ] render target is 8192-aligned, pinned, shadow-free; texture churn does not evict it
- [ ] binding/unbinding restores draw buffer, depth, offset, viewport and scissor — proven by a 2D frame
- [ ] RAW is `TexFlush`+`TexSync` only; CPU wait counter 0 across the alternating run
- [ ] `glReadPixels` is target-aware (offset and dimensions) and matches the CPU pattern
- [ ] VRAM accounting adds up; the 512×256 configuration is recorded as rejected or accepted with numbers
- [ ] 2D gate 0 diff; 060 cube gate 0; 062 lit/fogged gates unchanged; `basic_shapes` 284 colours
- [ ] orientation marker lands where the direct render puts it (V flip documented)
- [ ] unsupported list recorded: MRT, cube targets, blits, mipmaps, depth sampling, targets <64

## 9. Result

—

## 10. Follow-ups

- RTT + lighting/fog in one frame: one regression arm, not a feature → **P10-S6**
- Multiple render targets and post-processing blits, if a sample needs them → Phase 10+ or samples
- Depth-texture sampling and soft particles → outside Phase 10
- Framebuffer-object memory accounting in the memory map → after T6's numbers exist
