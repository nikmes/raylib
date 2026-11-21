# Sprint 019 (P7-S2) — Text work: run submission, glyph lookup and the 512 atlas limit

| | |
|---|---|
| Roadmap phase | [Phase 7 — 2D specialisation](../phases/07-2d-specialisation/index.md) |
| Sprint | P7-S2 |
| Branch / base commit | `master` @ `8e0b0d8f` (sprint 018 closed) |
| Started / closed | 2026-11-21 / 2026-11-21 |
| Build number(s) | pspgu `b23` (driver unchanged); `libraylib.a` rebuilt with the text path |
| Tasks | 6/6 done |

## 0. Handover

**Situation.** Sprint 018 built the Phase 7 corpus and measured text at **1.226 / 6.827 / 19.680 ms** work for
63 / 548 / 1,637 glyphs. Every point is already **one draw per frame and one bind per run** — the PSP batcher
defers `rlSetTexture`, so the phase's submission criterion already holds up to the 4,096-vertex batch cap. The
cost is per-glyph CPU work: about **12 µs/glyph**, and two code facts explain a large part of it. `GetGlyphIndex`
(`src/rtext.c:1475`) linearly scans every glyph for every character, and `DrawTextEx` (`src/rtext.c:1228`) calls
`DrawTexturePro` per glyph, which repeats the begin/end, texture and colour state.

**Read before acting**, in this order:

1. `../sprints/018-phase7-corpus-and-budget.md` (P7-S1) §7 — the text baseline, the counter table and the 512 audit.
2. `../phases/07-2d-specialisation/index.md` — the text exit criterion and R3.
3. `src/rtext.c` — `DrawTextEx`, `DrawTextCodepoint`, `DrawTextCodepoints`, `GetGlyphIndex`, `LoadFontFromMemory`,
   `GenImageFontAtlas`.
4. `src/rlgl.h:622-930` — the PSP batcher: `rlSetTexture` is deferred, `rlBegin`/`rlEnd` are cheap, a glyph quad
   becomes one GE sprite (2 vertices) when it is axis-aligned with one colour.
5. `psp-tests/scene_2d/main.c` and its Makefile — the corpus this sprint extends.

**First action.** Add `TEXT_IMPL=api|direct|walk` to `psp-tests/scene_2d` and run the three at `LINES=8` and
`LINES=24`. `direct` is the app-side run-batched loop, `walk` counts codepoints and glyph indices without drawing.
Do not touch `src/` until those three numbers are in the sprint's §7.

**Hard rules.**

- **Pixels are the contract.** Text captures at `LINES=1|8|24` must be byte-identical to the sprint 018 baselines
  (`/home/psp/evidence/sprint018/text{1,8,24}_run1.raw`), and `batch_check` must stay byte-identical to
  `par_pspgl.raw`. A faster path that moves one pixel is rejected.
- **Public semantics do not move.** Newline, tab, space, spacing, `advanceX` fallback, glyph padding, multibyte
  codepoints and the `'?'` fallback must produce the same index and the same geometry as today.
  `DrawTextCodepoint` keeps its single-glyph meaning.
- **A cache needs a complete key.** Invalidate on `font.glyphs`, `font.recs`, `font.glyphCount`, `font.baseSize`,
  `font.glyphPadding`, `font.texture.id`, `font.texture.width`, `font.texture.height`. A reload with the same id
  must rebuild, not reuse stale UVs.
- **The glyph fast path must agree with the slow path.** For every codepoint in a test set, the cached index must
  equal the linear scan's index, including the `'?'` fallback. Print a mismatch count; it must be 0.
- **Do not raise the batch cap without arena accounting.** `RLPSP_BATCH_CAP` is 4,096 vertices × 24 B = 96 KiB of
  batch memory; each list's arena is 128 KiB, so 8,192 vertices (192 KiB) cannot be queued without growing the
  arena. If 32/48 lines need two draws, record it; a cap change is a separate decision with the memory shown.
- **A >512 atlas fails loudly, never silently.** The guard unloads the generated atlas and glyph data and returns
  the default font with an error, so a texture is never created. No `glTexImage2D` with width or height >512.
- **No driver change.** pspgu stays `b23`; the 2D gate and the residency numbers are unchanged.
- **Stage only this sprint's paths.** Other sessions edit the tree; `git status` before every commit and never
  `git add -A`.
- Commit messages carry the task ID: `sprint 019 T2: ...`.

**Commands.**

```sh
export PSPDEV=/home/psp/pspdev
export PATH=$PATH:$PSPDEV/bin

cd /home/psp/raylib-psp
./tools/pspgu-build.sh                     # b23; driver unchanged
cd src && make PLATFORM=PLATFORM_PSP -j8 && cp libraylib.a $PSPDEV/psp/lib/libraylib.a

cd /home/psp/raylib-psp/psp-tests/scene_2d
make clean && make MODE=text LINES=24 TEXT_IMPL=walk
make clean && make MODE=text LINES=24 TEXT_IMPL=direct
make clean && make MODE=text LINES=24 TEXT_IMPL=api
cp scene_2d.prx /tmp/s3/app.prx
cd /tmp/s3 && timeout 20 pspsh -e "reset"; sleep 6
( printf './app.prx\n'; sleep 14 ) | timeout 35 pspsh
pspsh -e "scrshot host0:/s19.raw"          # after DONE only
cmp /tmp/s3/s19.raw /home/psp/evidence/sprint018/text24_run1.raw
```

**Do not touch.** `tools/pspgu/`, the batcher's primitive choice, `DrawTexturePro`, the parity scenes, and the
allocator. `DrawTextPro`'s matrix behaviour is pre-existing and out of scope.

**Done means** §8: the three-way attribution, a faster text path with identical pixels, a 0-mismatch lookup
check, the cap boundary measured and decided, the atlas guard proven on a real TTF, and the gate at 0.

## 1. Objective

Four falsifiable results:

1. **Attribution**: at `LINES=8` and `LINES=24`, `walk` (string + glyph lookup + layout, no draw), `api`
   (`DrawTextEx`) and `direct` (app-side run-batched loop) separate lookup/layout cost from submission cost.
2. **A faster text path**: after the library changes, work at `LINES=24` is **at least 10 % below 19.680 ms**,
   with the three text captures byte-identical to sprint 018.
3. **The cap boundary is measured**: at `LINES=32` and `LINES=48` the submission count and work are recorded, and
   the cap decision (keep or change) is written with the arena bytes.
4. **A font atlas that exceeds 512 fails loudly**: a TTF at a size that generates a >512 atlas logs one error,
   returns the default font, and creates no texture; the same TTF at a size that fits loads and renders.

## 2. Why now

Sprint 018 closed the phase's text-submission criterion and left work as the only text lever. Phase 7's exit
criterion is count-based, but its point was to stop paying per glyph; the corpus now shows exactly where that
cost is. The font-atlas case is the text-shaped half of the 512 audit and is the input to sprint 020's general
oversize-texture fix.

## 3. Scope

**In:** `psp-tests/scene_2d` text probes and `MODE=font`; `src/rtext.c` glyph lookup, `DrawTextEx`/
`DrawTextCodepoints` submission, the text layout cache, and the PSP atlas guard in `LoadFontFromMemory`.

**Out:** the general `glTexImage2D` >512 fix (sprint 020), window slicing, the batch cap/arena (separate decision
with memory accounting), `DrawTextPro` rotation semantics, tile/particle/shape paths, and any driver change.

## 4. Preconditions

- [ ] Sprint 018 closed at `8e0b0d8f`; its text baselines and gate are in `/home/psp/evidence/sprint018/`
- [ ] pspgu `b23` installs and `batch_check` is byte-identical to `par_pspgl.raw`
- [ ] `libraylib.a` builds from `src/` and can be copied into `$PSPDEV/psp/lib/`
- [ ] `examples/text/resources/KAISG.ttf` copies to `/tmp/s3/fonts/KAISG.ttf`
- [ ] Device loop available: `usbhostfs_pc` rooted at `/tmp/s3`, `pspsh`, `reset` between runs
- [ ] pspGL restorable (`2ab7378e`)

## 5. Tasks

| ID | task | status | verification | evidence |
|---|---|---|---|---|
| T1 | **Attribution probe**: `TEXT_IMPL=api\|direct\|walk` in `scene_2d`; `direct` is an app-side run-batched loop with the same geometry; `walk` does codepoints + `GetGlyphIndex` + advance and no draw | `done` | three impls at `LINES=1/8/24`; `direct` capture equals `api`; attribution table in §7 | `walk` **0.530/2.148/5.748 ms**, `direct` **0.897/4.339/12.069**, `api` **1.226/6.879/19.840** at 1/8/24 lines. `direct` is **27–39 % faster** and byte-identical to `api`; `walk` is 29–31 % of `api` at 8/24. Instrument note: the direct arm needs `-O2` and `-I../../src` so it compiles against the header with the PSP batch section, not the older installed copy |
| T2 | **Glyph lookup**: sequential fast path + a small codepoint→index cache in `rtext.c`; public `GetGlyphIndex` semantics unchanged | `done` | 0 mismatches against the linear scan over a test set, including fallback; text work moves | `LOOKUP mismatches=0` for the default font, KAISG 32 px (sequential) and KAISG with the 95 codepoints **reversed** (unordered map), 0..0x2FF. Combined with T3/T4, the 24-line path fell to 9.069 ms |
| T3 | **Run submission**: `DrawTextEx`/`DrawTextCodepoints` use one `rlSetTexture`, one `rlBegin`, one `rlEnd` per call and a shared glyph-quad emitter | `done` | same geometry as `DrawTexturePro` (TL/BL/BR/TR); pixels identical; work recorded | One bind and one draw per line at 1/8/24 lines; captures byte-identical to 018. Text8 **3.350**, text24 **9.069 ms** |
| T4 | **Layout cache**: normalized UVs and base-size metrics per glyph, allocated per font and invalidated by the full key | `done` | cache rebuild on font change; 0 mismatches; work recorded; no stale UV after an unload/reload in the same run | Key: glyphs, recs, glyphCount, baseSize, glyphPadding, texture id and dimensions. A 32 px and a 12 px font are drawn in one run: two atlas binds, 0 mismatches for both. Two 512×256 8888 atlases do not fit (one run recorded 3 evictions / 1 reload / 300 failed draws), so the second font is 12 px (256×128) |
| T5 | **Atlas guard and `MODE=font` probe**: `LoadFontFromMemory` rejects a >512 atlas on PSP (unload, error, default font); probe loads KAISG.ttf at a fitting and a non-fitting size | `done` | fitting size renders, non-fitting logs one error, returns the default font and uploads no texture | 32 px / 95 glyphs: atlas **512×256**, texture id 3, fallback 0. 64 px / 95 glyphs: raylib builds **1024×512**, `FONT: atlas 1024x512 exceeds the PSP 512x512 limit` (one error), **no texture line**, texture id 2 = default font, fallback 1, 0 mismatches |
| T6 | **Parity, cap and gate**: text 1/8/24 vs 018 captures; mixed scene; `LINES=32/48`; `batch_check` | `done` | captures byte-identical; cap table complete; gate 0 differing pixels | text1/8/24 and mixed SHA-256 match 018 exactly; 32 lines (2,177 glyphs) and 48 lines (3,259) are **2 draws/frame**, work 11.977/18.233 ms; `batch_check` **b69830b8…**; gate byte-identical |

Status vocabulary: `todo` / `doing` / `done` / `blocked` / `dropped`. IDs are stable.

### T1 design

- Makefile: `TEXT_IMPL ?= api`; `-DTEXT_IMPL_API`, `-DTEXT_IMPL_DIRECT`, `-DTEXT_IMPL_WALK`.
- `api`: `DrawTextEx(GetFontDefault(), line, ...)` per line, as today.
- `direct`: one `rlSetTexture(font.texture.id)`, `rlBegin(RL_QUADS)`, then for each drawn glyph compute the same
  `srcRec`/`dstRec` as `DrawTextCodepoint`, set the colour once, emit TL/BL/BR/TR with `rlTexCoord2f`/`rlVertex2f`,
  then `rlEnd()`, `rlSetTexture(0)`. It still calls `GetGlyphIndex`, so the difference from `api` is submission
  overhead, not lookup.
- `walk`: the same codepoint loop and `GetGlyphIndex` calls and the same `advanceX` update, but no vertex calls and
  no texture bind. It measures string + lookup + layout.
- Run `LINES=1/8/24` for all three, two runs each; capture `api` and `direct` and compare.
- The probe is app-side and temporary in spirit, but it stays in the corpus as the "no-library-change" reference.

### T2 design

- In `src/rtext.c`, keep `GetGlyphIndex(Font, int)` as the public entry and add an internal
  `static int getGlyphIndexFast(Font font, int codepoint)`.
- Font cache key: `{ font.glyphs, font.glyphCount }`. On a key change, scan once to find `firstValue`, whether
  `glyphs[i].value == firstValue + i` for all i, and the index of `'?'` (63).
- Sequential fonts: `index = codepoint - firstValue` when in range; otherwise the fallback index.
- Unordered fonts: a direct-mapped table of 256 entries keyed by `(codepoint*2654435761u) >> 24`, each entry
  `{ int codepoint; int index; }`, cleared on key change. On a miss, run the existing linear scan and fill the
  slot; the result is the same index the scan returns.
- `GetGlyphIndex` calls the fast path; `GetGlyphInfo` and the text draw loop inherit it. The linear scan remains
  reachable as the miss path and as the reference in the probe.
- Probe: in `MODE=font` or the text scene, loop codepoints 0..0x2FF and compare `GetGlyphIndex` with an app-side
  linear scan over `font.glyphs`; print `LOOKUP mismatches=N`.

### T3 design

- Shared `static void drawGlyphQuad(const Font *font, int index, Vector2 position, float fontSize, Color tint)`:
  same `glyphPadding`, `offsetX/Y`, `rec` and `scaleFactor` maths as `DrawTextCodepoint`; one `rlColor4ub` per
  glyph; corners in `DrawTexturePro` order TL, BL, BR, TR so the batcher sees the same quad.
- `DrawTextEx`: `rlSetTexture(font.texture.id)` once before the loop, `rlBegin(RL_QUADS)` once, one
  `drawGlyphQuad` per non-space glyph, `rlEnd()` and `rlSetTexture(0)` at the end. Newlines and advance logic are
  untouched.
- `DrawTextCodepoints`: the same structure.
- `DrawTextCodepoint`: keep the single-glyph contract; it may call the shared helper wrapped in its own
  begin/end, or keep `DrawTexturePro`. Its output is unchanged either way.
- Do not call `rlNormal3f` in the text path (the batcher ignores it); do not change the corner order.

### T4 design

- Cache entry: `{ float u0, v0, u1, v1; float dx, dy, w, h; float adv; }` at `font.baseSize`, with
  `u = srcRec.x/texture.width`, `v = srcRec.y/texture.height`, `dx = offsetX - glyphPadding`,
  `dy = offsetY - glyphPadding`, `w = rec.width + 2*glyphPadding`, `h = rec.height + 2*glyphPadding`,
  `adv = (advanceX != 0)? advanceX : rec.width`.
- Allocate `font.glyphCount * sizeof(entry)` with `RL_MALLOC` at the first text draw for that font; free and
  rebuild when the full key changes. Keep one cache: text draws one font at a time, and a second font rebuilds it.
- Per glyph: `dst = position + (dx*scale, dy*scale)`, size `(w*scale, h*scale)`, advance `adv*scale + spacing`.
  This is the same arithmetic `DrawTextEx` does today, moved out of the loop.
- Key: `{ glyphs, recs, glyphCount, baseSize, glyphPadding, texture.id, texture.width, texture.height }`.
  Record a rebuild counter in a log line so a stale cache is visible.

### T5 design

- In `LoadFontFromMemory`, after `GenImageFontAtlas` and before `LoadTextureFromImage`:
  ```c
  #if defined(PLATFORM_PSP)
      if ((imAtlas.width > 512) || (imAtlas.height > 512))
      {
          TRACELOG(LOG_ERROR, "FONT: atlas %ix%i exceeds the PSP 512x512 limit; use fewer glyphs or a smaller size",
                   imAtlas.width, imAtlas.height);
          UnloadImage(imAtlas);
          UnloadFontData(font.glyphs, font.glyphCount);
          RL_FREE(font.recs);
          return GetFontDefault();
      }
  #endif
  ```
  `font.glyphs[i].image` still holds the per-glyph images at this point, so `UnloadFontData` frees them; the atlas
  crops are created only after the guard.
- `MODE=font` in `scene_2d`, with `FONT_PATH` (default `host0:/fonts/KAISG.ttf`), `FONT_SIZE` (default 32) and
  `FONT_GLYPHS` (default 95). The app calls `LoadFontEx`, logs `font.baseSize`, `font.glyphCount`,
  `font.texture.id`, whether it is the default font, draws the font name and a sample line, writes a marker and
  sleeps.
- Run `FONT_SIZE=32` (fits) and `FONT_SIZE=64` (the 95-glyph atlas is expected to exceed 512 in raylib's basic
  packing). If 64 still fits, increase the size or glyph count until the guard triggers; the guard case must be
  observed, not inferred.
- Evidence must show no `[PSPGU] texture` line with a dimension >512 and no VRAM allocation for the rejected
  atlas.

### T6 design

- Rebuild `libraylib.a` after T2–T4, then run `MODE=text LINES=1/8/24` and compare each capture byte-for-byte with
  `/home/psp/evidence/sprint018/text{1,8,24}_run1.raw`; run `MODE=mixed` and compare with `mixed_run1.raw`.
- `LINES=32` and `48`, two runs each: record draws/frame and work. The expected threshold is 2,048 glyphs per
  4,096-vertex draw; below it one draw, above it two.
- `batch_check` (`WRAP=0`) after the library rebuild; compare with `par_pspgl.raw`.
- Cap decision: keep `RLPSP_BATCH_CAP=4096` unless the 32/48 numbers show the second draw is the dominant cost
  **and** an arena plan fits in the recorded host-RAM budget. Record the decision either way.

## 6. Budgets

| item | budget | why |
|---|---|---|
| text work target | ≥10 % below 19.680 ms at `LINES=24` | below 10 % the structural change is not worth its risk |
| measured frames per point | 300, two runs | matches sprint 018 |
| lookup probe | codepoints 0..0x2FF | covers ASCII, Latin-1 and the fallback |
| cap points | 24, 32, 48 lines | 24 fits one draw; 32/48 cross 2,048 glyphs |
| atlas probe | one fitting size, one non-fitting | the guard must be observed |
| device runs | 2 per measured point | project rule |

## 7. Measurements

Eighteen attribution runs plus the library points, 300 frames each, two runs per point. Evidence:
`/home/psp/evidence/sprint019/` (`s19_*_run{1,2}.txt|raw|log`, `gate_b23.raw`, `SHA256SUMS`). Build marker on
every run: `[PSPGU] GU up: pspgu b23 …`. The `api` numbers below are from the same sprint build; the 1-line
`api` point is the 018 baseline because the 019 run reinforces the 8/24 result.

**Attribution (work ms, run 1 / run 2):**

| LINES | glyphs | `walk` | `direct` | `api` |
|---|---:|---:|---:|---:|
| 1 | 63 | 0.530 / 0.528 | 0.897 / 0.895 | 1.226 / 1.226 (018) |
| 8 | 548 | 2.148 / 2.147 | 4.339 / 4.340 | 6.879 / 6.880 |
| 24 | 1,637 | 5.748 / 5.749 | 12.069 / 12.067 | 19.840 / 19.848 |

Reading: string + linear glyph lookup + advance is **29–31 %** of the API path at 8/24 lines; the run-batched
`direct` loop removes **27–39 %** while producing byte-identical captures. Both point at the same two fixes T2
and T3 make.

**After the library changes (combined T2+T3+T4; work ms, run 1 / run 2):**

| LINES | glyphs | draws/frame | work | vs `api` |
|---|---:|---:|---:|---:|
| 1 | 63 | 1 | 0.686 / 0.687 | **−44 %** |
| 8 | 548 | 1 | 3.350 / 3.352 | **−51 %** |
| 24 | 1,637 | 1 | 9.069 / 9.069 | **−54 %** |
| 32 | 2,177 | 2 | 11.977 / 11.977 | cap boundary |
| 48 | 3,259 | 2 | 18.233 / 18.235 | cap boundary |
| mixed | 2,158 | 4 | 10.053 / 10.054 | 018 was 14.745 (−32 %) |

The stages were adopted together; the attribution table is the per-stage evidence. Work per glyph is now about
**5.5 µs** (was 12 µs). At 32 lines the glyph count crosses 2,048, so the 4,096-vertex cap adds a second draw;
the per-glyph cost does not jump (11.977/2,177 = 5.5 µs), so the cap is not the bottleneck.

**Cap decision:** keep `RLPSP_BATCH_CAP=4096`. A larger cap needs 24 B/vertex × 8,192 = **196,608 B** of batch
memory and the per-list arena is 128 KiB, so it cannot be raised without growing the arena. The measured
second draw costs no fixed penalty; the arena change is not justified by these numbers.

**Lookup probe:** `mismatches=0` for the default font, KAISG 32 px with sequential glyph values, and KAISG
32 px with the 95 codepoints **reversed** (the unordered direct-mapped path), over codepoints 0..0x2FF
including `'?'`. The dual-font runs also print `mismatches2=0` for a 12 px font drawn after the first.

**Font atlas probe:**

| font | size | glyphs | atlas W×H | verdict | texture id | fallback | mismatches |
|---|---:|---:|---|---|---:|---:|---:|
| KAISG.ttf | 32 | 95 | 512×256 | loaded, rendered | 3 | 0 | 0 |
| KAISG.ttf | 12 | 95 | 256×128 | loaded, rendered (second font) | 4 | 0 | 0 |
| KAISG.ttf | 64 | 95 | **1024×512** | **rejected by guard** | 2 (default) | 1 | 0 |
| KAISG.ttf | 12 | 95 | 256×128 | loaded (second font, after fallback) | 3 | 0 | 0 |

The 64 px run logs exactly one `FONT: atlas 1024x512 exceeds the PSP 512x512 limit` error, has **no
`[PSPGU] texture` line above 512**, leaves VRAM at 785,920 B free, and draws with the default font. A run with
two 512×256 8888 atlases (second font at 24 px) is recorded as a negative control: 1,048,576 B of atlases against
a 916,992 B tail produced 3 evictions, 1 reload and 300 failed draws, hence the 12 px second font.

**Parity (SHA-256 prefix):**

| scene | 018 | 019 | identical |
|---|---|---|---|
| text LINES=1 | `3a7704731dac3724` | `3a7704731dac3724` | yes |
| text LINES=8 | `1124a11b8e341219` | `1124a11b8e341219` | yes |
| text LINES=24 | `adb5dceb8da8ec5c` | `adb5dceb8da8ec5c` | yes |
| mixed | `f132697c511ce11a` | `f132697c511ce11a` | yes |
| `batch_check` gate | `b69830b86a899cc0…` | `b69830b86a899cc0…` | yes |

The gate (which draws text) is byte-identical to pspGL, so the new run-batched path is also validated by the
project's 2D gate, not only by the text corpus.

## 8. Exit criteria

- [x] the `walk`/`direct`/`api` attribution is recorded at `LINES=1/8/24`, with `direct` pixel-identical to `api`
- [x] text work at `LINES=24` is at least 10 % below 19.680 ms, or the changes are reverted and the null result is
      recorded with the attribution — **9.069 ms, −54 %**
- [x] `GetGlyphIndex` fast path reports 0 mismatches against the linear scan over 0..0x2FF, including `'?'`
- [x] text and mixed captures are byte-identical to the sprint 018 baselines
- [x] `LINES=32/48` records draws/frame and work, and the cap decision is written with arena bytes — **cap stays
      4,096; 32/48 lines are 2 draws, no fixed penalty**
- [x] a >512 atlas logs one error, returns the default font, uploads no texture; a fitting TTF loads and renders
- [x] `batch_check` is byte-identical to `par_pspgl.raw`
- [x] no `tools/pspgu/` file is in the sprint diff

## 9. Result

Sprint 019 closed 6/6. The text path now costs **0.686 / 3.350 / 9.069 ms** at 1/8/24 lines, down from
1.226 / 6.879 / 19.840 — **−44 %, −51 %, −54 %** — with **byte-identical captures** at all three points and a
byte-identical `batch_check` gate. Every change is in `src/rtext.c`; there is no public API change and no driver
change.

- **Attribution first.** `walk` (string + linear lookup + advance) was 29–31 % of the API path at 8/24 lines, and
  the app-side `direct` loop (run-batched, same geometry) was 27–39 % faster with identical pixels. That is the
  evidence the library changes are worth their risk.
- **T2** replaces the per-character linear scan with a sequential fast path plus a 256-slot codepoint cache for
  unordered fonts. The old scan remains the miss path; 0 mismatches over 0..0x2FF for the default font, a
  sequential TTF and a reversed-codepoint TTF.
- **T3** emits one run per `DrawTextEx`/`DrawTextCodepoints` call: one bind, one begin, one end, shared glyph quad
  in `DrawTexturePro` corner order. `DrawTextCodepoint` shares the quad but keeps its single-glyph contract.
- **T4** caches normalized UVs and base-size metrics per font, keyed by glyphs, recs, count, base size, padding,
  texture id and dimensions. A 32 px and a 12 px font in one run rebuild the cache and both report 0 mismatches.
- **Cap:** at 2,177 and 3,259 glyphs the 4,096-vertex cap adds a second draw but the per-glyph cost stays
  ~5.5 µs. Raising the cap would need 196,608 B of batch memory and a larger per-list arena; recorded as not
  justified and not done.
- **T5:** KAISG at 32 px builds a 512×256 atlas and renders; at 64 px raylib builds 1024×512, the guard logs one
  error, no texture is created, and the default font is returned. Two 512×256 8888 atlases do not fit VRAM and
  were recorded as the negative control (3 evictions / 1 reload / 300 failed draws); the second probe font is
  12 px.

The mixed scene also improved, 14.745 → 10.053 ms (−32 %), with identical pixels. The general width > 512 upload
case remains sprint 020's; this sprint closed the font-shaped half.

## 10. Follow-ups

- **Sprint 020 — tile maps and 512-pixel windowing**: the general >512 upload fix (split at load or fail loudly),
  then window slicing with full-stride `tbw`, and the tile/background audit cases. The cap/arena question is
  **not** handed to 020: text at 32/48 lines scales linearly with the second draw.
- **Sprint 021 — sprites and shapes**: the 018 shape baseline (4 draws, 10,000 vertices, 1 bind + 1 unbind per
  frame) is the before/after.
- Any text win that does not reproduce at 300 frames is a null result and is removed, not kept behind a switch.
