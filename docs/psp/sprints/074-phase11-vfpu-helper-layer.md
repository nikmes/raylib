# Sprint 074 (P11-S12) — VFPU/matrix helper layer: license-clean wrappers and clean-room ops

| | |
|---|---|
| Roadmap phase | [Phase 11 — General optimisation outside the renderer](../phases/11-general-optimisation.md) |
| Sprint | P11-S12 |
| Branch / base commit | `master` @ `816a8c25` (pin at start) |
| Started / closed | 2026-11-22 (written) / — |
| Build number(s) | pspgu as installed (`--check` recorded); `libraylib.a` with `PSP_VFPU_HELPERS=0|1` |
| Tasks | 0/5 done |

## 0. Handover

**Situation.** The reference scan found the expected richness — `PGE/pgeMath.h` (scalar + vec3 + rect),
`openTRI/triVMath_vfpu.h` (113 vec, 10 mat, 15 quat, 5 colour ops), `daedalus/.../Math.h` (VFPU
sin/cos/atan2/pow/invSqrt) — **but all three are GPL** (GPL-3, GPL-2, GPL-2), and
[memory map §7](../memory-map.md#7-reference-sources-read-only-outside-this-repo) says nothing from
`/home/psp/ref` is pasted into this zlib tree. The permissive equivalents already ship in the SDK:
`pspmath.h` (scalar `vfpu_*`), `pspgum.h` (vec/mat/quat) and `pspvfpu.h` (context). This sprint builds one
**project-owned** `src/rlpsp_math.h` that wraps the SDK and adds clean-room missing ops, so other sprints
include one internal header instead of hand-rolling VFPU asm.

**Scan inventory** (paths are read-only references; licences checked at the file/repo level):

| source | licence | families | disposition |
|---|---|---|---|
| `/home/psp/ref/PGE/pgeMath.h` | **GPL-3.0** | scalar sin/cos/tan/asin/acos/atan/atan2/exp/log/pow/invSqrt, vec3 add/sub/dot/length/normalize/rotateZ, rect | **ideas only** — no code |
| `/home/psp/ref/openTRI/triVMath_vfpu.h` | **GPL-2.0** | 113 `triVec*`, 10 `triMat*`, 15 `triQuat*`, 5 `triColor*` | **ideas only** — no code |
| `/home/psp/ref/daedalus/Source/SysPSP/Math/Math.h` | **GPL-2.0-or-later** | `vfpu_sinf/cosf/sincos/atan2f/powf/fabsf/sqrtf/invSqrt`, `vfpu_norm_3Dvec`; `FastMemcpy` | **ideas only** — no code |
| `/home/psp/ref/pspgl-themiriron2/pspgl_codegen.h` | **BSD-3** | VFPU codegen macros for pspGL's IR, not a math API | not needed |
| `/home/psp/ref/PSPRuntime` | mixed; `vram.c` "free to use with credit" | renderer, not a math header | not needed |
| `/home/psp/ref/3D-PSP-Tutorials` | **none** | tutorial code | no |
| `$PSPDEV/psp/sdk/include/pspmath.h` | PSPSDK (permissive) | scalar `vfpu_*` | **wrap** |
| `$PSPDEV/psp/sdk/include/pspgum.h` | PSPSDK (permissive) | vec/mat/quat (`gum*`) | **wrap** |
| `$PSPDEV/psp/sdk/include/pspvfpu.h` | PSPSDK (permissive) | context/matrix reservation | **use** |

**Readiness facts (verified 2026-11-22).**

- `pspmath.h` and `pspgum.h` are **BSD** (PSPSDK), not GPL; `pspvfpu.h` provides
  `pspvfpu_initcontext()`/`pspvfpu_use_matrices()`/`pspvfpu_deletecontext()`.
- Daedalus (`copying.txt`) is GPL-2, openTRI GPL-2, PGE GPL-3 — references only.
- The main thread attribute is set **by each app** (`PSP_MAIN_THREAD_ATTR`), not by the library; every test
  app currently uses `THREAD_ATTR_USER` only.
- No current Makefile links `-lpspmath` or `-lpspgum`; the test link line is
  `-lraylib ... -lpspgu -lpspvfpu` plus `-lpspaudio -lpthread -latomic`.
- Implementation shape: header-only math in `src/rlpsp_math.h` plus one tiny TU
  `src/platforms/rlpsp_math.c` for the VFPU context; no public API. The switch is in `src/Makefile`'s PSP block.
- **Link policy:** `PSP_VFPU_HELPERS=0` adds no dependency; `=1` needs `-lpspmath -lpspvfpu`
  (scalar VFPU trig + context). `RLPSP_VFPU_GUM=1` additionally links `-lpspgum`; the copy uses
  `memcpy` unless `RLPSP_VFPU_COPY=1`.
- **Thread contract:** call `rlpsp_vfpu_init()` once per VFPU thread before the first helper;
  `pspvfpu_initcontext()` sets the calling thread's VFPU attribute per `pspvfpu.h`. The probe also sets
  `THREAD_ATTR_VFPU`; T4 checks whether init alone suffices.

**Read before acting**, in this order:

1. `../memory-map.md` §7 — the licence rule; §10.1 — VFPU costs, accuracy, quarter turns, thread/context;
   §14 — what the SDK actually ships.
2. `../sprints/056-phase11-vfpu-math-and-copy.md` — per-operation adoption, accuracy rows, copy rules.
3. `../sprints/055-phase11-frame-cpu.md` — the double-site census and why scalar helpers matter.
4. `../sprints/071…073` — the first consumers (image conversion, mesh packing, 2D attribution); `070`
   (camera/matrix) for GUM.
5. `$PSPDEV/psp/sdk/include/pspmath.h`, `pspgum.h`, `pspvfpu.h` — the actual symbols and signatures.
6. The GPL headers above — read for **function behaviour and naming**, never copy code or comments.

**First action.** Write the licence/provenance table in §7 (T1) and confirm the SDK symbol list with
`psp-nm $PSPDEV/psp/sdk/lib/libpspmath.a`/`libpspgum.a`. No code before the table exists.

**Hard rules.**

- **No GPL/ref code in the tree.** PGE/openTRI/Daedalus are ideas only. If a function is needed, reimplement
  it clean-room from public behaviour and the VFPU facts; cite the reference in the sprint, not in the source.
- **Wrap the SDK first.** `libpspmath`, `libpspgum` and `libpspvfpu` are permissive and already installed;
  prefer a thin wrapper over new asm. Write asm only for a measured missing operation.
- **One internal header, namespaced.** `src/rlpsp_math.h` with `rlpsp_*` names; `#ifdef PLATFORM_PSP`; a
  scalar/`libm` fallback so desktop builds and `PSP_VFPU_HELPERS=0` compile unchanged. No public raylib API.
- **On demand.** Other sprints include the header only where they opt in; the header must not force VFPU
  context or a thread attribute at include time.
- **VFPU discipline.** Calling threads carry `PSP_THREAD_ATTR_VFPU`; `rlpsp_vfpu_init()` wraps
  `pspvfpu_initcontext()`/`pspvfpu_use_matrices()`; helpers use matrices 6–7 (`pspgum` owns 0–3); 16-byte
  alignment; PPSSPP is not evidence.
- **The attribute is an app contract.** The library cannot change `PSP_MAIN_THREAD_ATTR`; T5 updates the
  test apps/samples behind the switch. `PSP_VFPU_HELPERS=0` keeps today's attributes and link line.
- **No auto-init.** Do not call `pspvfpu_initcontext()` from a static inline on first use; the caller owns
  the lifecycle (`rlpsp_vfpu_init`/`rlpsp_vfpu_shutdown`).
- **No intrinsics.** `__builtin_vfpu_*` does not exist in psp-gcc 15.2.0; use the SDK, `pspvfpu.h` or inline
  asm.
- **Pure GUM only.** Use `gumDotProduct`/`gumCrossProduct`/`gumMultMatrix`/`gumLookAt`/`gumPerspective`/
  `gumOrtho`; never `sceGum*` stack calls. T4 must verify `gumMultMatrix` order against raymath and the
  `ScePspFMatrix4` layout before adoption.
- **Per operation, measured.** Each helper has a µs row and an accuracy row; no global `-ffast-math` swap;
  no accumulated-state call.
- Commit messages carry the task ID.

**Commands.**

```sh
export PSPDEV=/home/psp/pspdev
export PATH=$PATH:$PSPDEV/bin

/home/psp/raylib-psp/tools/pspgu-build.sh --check
cd /home/psp/raylib-psp/src
make PLATFORM=PLATFORM_PSP -j8 PSP_VFPU_HELPERS=0     # baseline
make PLATFORM=PLATFORM_PSP -j8 PSP_VFPU_HELPERS=1     # adopted set
cp libraylib.a $PSPDEV/psp/lib/libraylib.a

cd ../psp-tests/math_probe        # built in T4; helpers=1 and helpers=0 arms
make PSP_VFPU_HELPERS=1
( printf './math_probe.prx\n'; sleep 60 ) | pspsh
pspsh -e "reset"

# helpers=1 consumer link (add to the app Makefile)
#   LIBS += -lpspmath -lpspgum -lpspvfpu
# and the app thread attribute:
#   PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU)
```

**Do not touch.** `/home/psp/ref/**` (read-only), renderer/draw paths, public headers, audio, input, and the
sprint documents that consume the helpers. The helper header is internal.

**Done means** §8, with a licence table, the adopted op list, time/accuracy rows and an on/off compile proof.

## 1. Objective

Create one license-clean internal header that exposes the measured VFPU/scalar/vector/matrix/quaternion/
packed/copy helpers other sprints can opt into, with a scalar fallback, a licence provenance table, and a
per-operation accuracy/time record — without importing any GPL reference code.

## 2. Why now

Four written sprints (056, 070, 071, 072) and the 2D attribution (073) need the same primitives. Doing them
ad hoc would duplicate VFPU asm and, worse, invite a GPL copy. Settling the licence and the API once lets the
consumers stay small and auditable.

## 3. Scope

**In:** header-only `src/rlpsp_math.h`; wrappers over `pspmath`/`pspgum`/`pspvfpu`; clean-room missing ops;
a math probe; `PSP_VFPU_HELPERS`; licence/provenance and docs.

**Minimum op set** (each with a time and an accuracy row):
- scalar: `sinf`, `cosf`, `sincosf`, `atan2f`, `powf`, `expf`, `logf`, `rsqrtf`, `sqrtf` (wrap `vfpu_*`);
- vec2/3/4: add, sub, scale, dot, cross, length, normalize, lerp;
- mat4: identity, multiply, transpose, transform, lookAt, perspective, ortho (pure `gum*`, never the `sceGum*`
  stack);
- quat: identity, multiply, normalize, rotate, from-axis-angle;
- packed: RGBA8↔565/5551/4444, float→s16;
- copy: the 64-byte `lv.q`/`sv.q` helper with the unaligned/tail rules.

**Out:** ref-repo code, public API changes, renderer/audio changes, global libm replacement, and the consumer
sprints' adoption decisions (they use the helper only where their own measurements justify it).

## 4. Preconditions

- [ ] P11-S1 harness (052) available for CPU-zone/accuracy measurement
- [ ] `pspmath`, `pspgum`, `pspvfpu` headers and archives present; symbol list recorded
- [ ] Licence table (T1) approved; `/home/psp/ref` remains read-only
- [ ] A VFPU-capable thread policy decided (app main thread vs dedicated worker); helper consumers identified
- [ ] Consumer link policy agreed: helpers=0 no new libs; helpers=1 adds `-lpspmath -lpspgum -lpspvfpu`
- [ ] PSPLink available; the user has approved the run

## 5. Tasks

| ID | task | status | verification | evidence |
|---|---|---|---|---|
| T1 | **Licence/provenance audit**: table above, plus the SDK symbol inventory; adopt/wrap/reimplement/reject per family | `done` | every candidate header has a licence and a disposition; no GPL path marked “copy” | SDK `pspmath.h`/`pspgum.h` BSD, `pspvfpu.h` context; PGE/openTRI/Daedalus GPL — no code copied |
| T2 | **API and skeleton**: header-only `src/rlpsp_math.h`; scalar, vec2/3/4, mat4, quat, colour/packed, bulk copy; `rlpsp_*`; `rlpsp_vfpu_init`/`shutdown`; scalar fallback; `PSP_VFPU_HELPERS` in `src/Makefile` | `done` | header compiles on PSP with helpers 0/1 and on desktop fallback; no public API change | `src/rlpsp_math.h`, `src/platforms/rlpsp_math.c`, `src/Makefile`; `libraylib.a` builds both arms |
| T3 | **Wrap + clean-room**: wrappers over `vfpu_*`/`gum*`; clean-room missing ops with `pspvfpu.h`/inline asm; matrices 6–7; **helpers=1 link policy** | `done` | symbol-level check; no GPL text; helpers=0 no new symbols; default helpers=1 links `-lpspmath -lpspvfpu` | **Measured decision:** VFPU scalar trig only. GUM vec/mat/quat and the VFPU copy are slower at raylib's sizes and stay opt-in (`RLPSP_VFPU_GUM`, `RLPSP_VFPU_COPY`). Camera projections stay raymath |
| T4 | **Accuracy/time probe**: `psp-tests/math_probe` (scalar/vec/mat/quat/packed/copy); golden tables; per-op µs and error; matrix-order/layout check; adopted/rejected list | `done` | each op has time and error; `gumMultMatrix` order matches raymath; the adopted set is explicit | **Adopted:** `vfpu_sinf/cosf/atan2f/powf` (2–6×). **Rejected:** GUM vec dot/cross/normalize, mat4 multiply, quat multiply/rotate, VFPU copy. See §7 |
| T5 | **Integration/guardrails**: add `THREAD_ATTR_VFPU` to the test apps/samples that enable helpers; add the libs to their link lines; init/teardown; consumer smoke (one vector and one packed op, no renderer change); licence note; docs | `doing` | stress run no corruption; helpers=0 attributes/link unchanged; `batch_check` unchanged; docs build | defaults and probe done; consumer migration (attrs/link) remains |

### Task log

- 2026-11-22 — sprint written; not started
- 2026-11-22 — T1–T3 implemented: header, context TU, Makefile switch; probe arms build
- 2026-11-22 — probe helpers=1/0: 3 vs 0 vfpu/gum symbols
- 2026-11-22 — device probe: VFPU trig wins; GUM/copy rejected with numbers; defaults flipped

## 6. Budgets

| budget | limit | measured | tag |
|---|---|---|---|
| helper code size | record; keep under a small header/TU | | |
| per-op accuracy | stated per function (VFPU ~22-bit) | | |
| per-op win | ≥ 10 % per op at the consumer size, or a named load/frame saving ≥ 0.1 % | | |
| public API change | 0 | | |
| VFPU state corruption | 0 over a switch stress run | | |

## 7. Measurements

| run | build | workload | metric | value | notes |
|---|---|---|---|---|---|
| baseline | `P11-S12 a` | helpers off | µs/op, libm | | |
| helpers | `P11-S12 b` | helpers on | µs/op, error | | per-op |
| copy | `P11-S12 b` | 16 B…2 MiB | µs/row | | aligned/unaligned |
| integration | `P11-S12 b` | consumer smoke | frame/captures | | no renderer change |
| trig VFPU | `P11-S12 b` | 10k calls | µs/op | sin 0.347, cos 0.332, atan2 0.687, pow 0.501; err 4.8e-7 | scalar: 1.207/1.200/1.615/3.373 |
| vec3 dot/cross/norm | `P11-S12 b` | 10k calls | µs/op | **0.332/0.486/0.684** | scalar 0.205/0.069/0.626 — GUM loses |
| mat4 multiply | `P11-S12 b` | 10k calls | µs/op | **6.284** | raymath 2.532 — GUM loses |
| quat multiply/rotate | `P11-S12 b` | 10k calls | µs/op | **1.468**; rotate diff 1.883 | raymath 0.069 — GUM loses/mismatches |
| copy 64 B / 1 MiB | `P11-S12 b` | 8 copies | µs/ms | **3.0 / 15.605 ms** | memcpy 2.375 / 15.602 ms — no win |
| code size | `P11-S12 a/b` | helpers off/on | bytes | | `.text` delta |
| link arms | `P11-S12 a/b` | app link | symbols | | off adds none |

## 8. Exit criteria

- [ ] licence/provenance table with a disposition per header; no GPL code in the tree
- [ ] `src/rlpsp_math.h` compiles with `PSP_VFPU_HELPERS=0|1` and has a scalar fallback
- [ ] adopted op list with µs and accuracy rows; rejected ops named
- [ ] thread/context/matrix-register rules exercised in a stress run
- [ ] at least one written sprint can consume it without new asm
- [ ] helpers=0 keeps the current app attributes and link line; helpers=1 has the documented contract
- [ ] no public API change; `batch_check` unchanged; docs build `--strict`

## 9. Result

**Measured on the PSP-1000; the helper layer adopts VFPU scalar trig only.** The probe overturned the
GUM-for-small-math assumption:

- **Adopt:** `vfpu_sinf` 0.347 vs 1.207 µs, `cosf` 0.332 vs 1.200, `atan2f` 0.687 vs 1.615, `powf` 0.501 vs
  3.373; error ~4.8e-7 (truncated, one-shot only).
- **Reject:** GUM `vec3` dot/cross/normalize (0.332/0.486/0.684 vs 0.205/0.069/0.626), mat4 multiply
  (6.284 vs 2.532), quat multiply (1.468 vs 0.069); `gumRotateVector` also disagrees with raymath by 1.88.
  The inlined scalar/raymath paths win because the GUM calls pay call overhead on these small ops.
- **Reject:** the 64-byte VFPU copy (64 B 3.0 vs 2.375 µs; 1 MiB 15.605 vs 15.602 ms). Keep `memcpy`.

The layer still delivers the license-clean, on-demand API and the trig win; GUM vec/mat/quat and the VFPU
copy stay behind `RLPSP_VFPU_GUM`/`_COPY` for future A/B. `src/rlpsp_math.h`, `src/platforms/rlpsp_math.c`,
`src/Makefile` (`PSP_VFPU_HELPERS`) and `psp-tests/math_probe` are in place; T5's consumer migration
(attributes/link) remains.

## 10. Follow-ups

- 056 (VFPU math/copy) consumes the scalar/copy set; 071/072 use the packed conversion; 073 can use `vrot`;
  070 decides GUM for matrices.
- Add a helper only with the same licence, time and accuracy rows; never paste a GPL implementation.
- If a missing op cannot be written clean-room from the facts, leave it out and name the blocker.
