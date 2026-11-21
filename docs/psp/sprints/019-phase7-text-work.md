# Sprint 019 — Text work: run submission, glyph lookup and the 512 atlas limit

| | |
|---|---|
| Roadmap phase | [Phase 7 — 2D specialisation](../phases/07-2d-specialisation.md) |
| Branch / base commit | `master` @ `8e0b0d8f` (sprint 018 closed) |
| Started / closed | — / — |
| Build number(s) | pspgu `b23` (driver unchanged); `libraylib.a` rebuilt |
| Tasks | 0/6 done |

## 0. Handover

**Situation.** Sprint 018 built the Phase 7 corpus and measured text at **1.226 / 6.827 / 19.680 ms** work for
63 / 548 / 1,637 glyphs. Every point is already **one draw per frame and one bind per run** — the PSP batcher
defers `rlSetTexture`, so the phase's submission criterion already holds up to the 4,096-vertex batch cap. The
cost is per-glyph CPU work: about **12 µs/glyph**, and two code facts explain a large part of it. `GetGlyphIndex`
(`src/rtext.c:1475`) linearly scans every glyph for every character, and `DrawTextEx` (`src/rtext.c:1228`) calls
`DrawTexturePro` per glyph, which repeats the begin/end, texture and colour state.

**Read before acting**, in this order:

1. `../sprints/018-phase7-corpus-and-budget.md` §7 — the text baseline, the counter table and the 512 audit.
2. `../phases/07-2d-specialisation.md` — the text exit criterion and R3.
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
| T1 | **Attribution probe**: `TEXT_IMPL=api|direct|walk` in `scene_2d`; `direct` is an app-side run-batched loop with the same geometry; `walk` does codepoints + `GetGlyphIndex` + advance and no draw | `todo` | three impls at `LINES=8/24`; `direct` capture equals `api`; attribution table in §7 | |
| T2 | **Glyph lookup**: sequential fast path + a small codepoint→index cache in `rtext.c`; public `GetGlyphIndex` semantics unchanged | `todo` | 0 mismatches against the linear scan over a test set, including fallback; text work moves | |
| T3 | **Run submission**: `DrawTextEx`/`DrawTextCodepoints` use one `rlSetTexture`, one `rlBegin`, one `rlEnd` per call and a shared glyph-quad emitter | `todo` | same geometry as `DrawTexturePro` (TL/BL/BR/TR); pixels identical; work recorded | |
| T4 | **Layout cache**: normalized UVs and base-size metrics per glyph, allocated per font and invalidated by the full key | `todo` | cache rebuild on font change; 0 mismatches; work recorded; no stale UV after an unload/reload in the same run | |
| T5 | **Atlas guard and `MODE=font` probe**: `LoadFontFromMemory` rejects a >512 atlas on PSP (unload, error, default font); probe loads KAISG.ttf at a fitting and a non-fitting size | `todo` | fitting size renders, non-fitting logs one error, returns the default font and uploads no texture | |
| T6 | **Parity, cap and gate**: text 1/8/24 vs 018 captures; mixed scene; `LINES=32/48`; `batch_check` | `todo` | captures byte-identical; cap table complete; gate 0 differing pixels | |

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

Filled by T1–T6. Every table carries the build marker and the command. Baselines below are sprint 018, same
scene strings and frame count.

**Attribution (work ms, run 1 / run 2):**

| LINES | glyphs | `walk` | `direct` | `api` (018) |
|---|---:|---:|---:|---:|
| 1 | 63 | | | 1.226 / 1.226 |
| 8 | 548 | | | 6.827 / 6.829 |
| 24 | 1,637 | | | 19.680 / 19.673 |

**After the library changes (work ms, run 1 / run 2):**

| LINES | draws/frame | T2 | T3 | T4 |
|---|---:|---|---|---|
| 1 | | | | |
| 8 | | | | |
| 24 | | | | |
| 32 | | | | |
| 48 | | | | |

**Lookup probe:** `LOOKUP mismatches=…` for the default font and KAISG.

**Font atlas probe:**

| font | size | glyphs | atlas W×H | verdict | texture id | fallback |
|---|---:|---:|---|---|---:|---|
| KAISG.ttf | 32 | 95 | | | | |
| KAISG.ttf | 64 | 95 | | | | |

**Parity:**

| scene | 018 SHA-256 | 019 SHA-256 | identical |
|---|---|---|---|
| text LINES=1 | | | |
| text LINES=8 | | | |
| text LINES=24 | | | |
| mixed | | | |
| `batch_check` gate | `b69830b8…` | | |

## 8. Exit criteria

- [ ] the `walk`/`direct`/`api` attribution is recorded at `LINES=1/8/24`, with `direct` pixel-identical to `api`
- [ ] text work at `LINES=24` is at least 10 % below 19.680 ms, or the changes are reverted and the null result is
      recorded with the attribution
- [ ] `GetGlyphIndex` fast path reports 0 mismatches against the linear scan over 0..0x2FF, including `'?'`
- [ ] text and mixed captures are byte-identical to the sprint 018 baselines
- [ ] `LINES=32/48` records draws/frame and work, and the cap decision is written with arena bytes
- [ ] a >512 atlas logs one error, returns the default font, uploads no texture; a fitting TTF loads and renders
- [ ] `batch_check` is byte-identical to `par_pspgl.raw`
- [ ] no `tools/pspgu/` file is in the sprint diff

## 9. Result

Not run yet.

## 10. Follow-ups

- **Sprint 020 — tile maps and 512-pixel windowing**: the general >512 upload fix (split at load or fail loudly),
  then window slicing with full-stride `tbw`. If 019 finds the batch cap blocks a single-run text submission at
  48 lines, the arena/cap question is handed to 020 with the measured bytes.
- **Sprint 021 — sprites and shapes**: the 018 shape baseline (4 draws, 10,000 vertices, 1 bind + 1 unbind per
  frame) is the before/after.
- Any text win that does not reproduce at 300 frames is a null result and is removed, not kept behind a switch.
