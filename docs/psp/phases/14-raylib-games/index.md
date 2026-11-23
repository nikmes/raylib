# Phase 14 — PSP Raylib Games

| | |
|---|---|
| State | **written, not started** — readiness-reviewed 2026-11-23; three sprints (082–084); first target CppGalaga v3 |
| Depends on | Phases 5–10 closed (renderer, textures/VRAM, input, audio, 3D); Phase 13 for the text and input paths it validated |
| Exit criterion | one complete third-party game runs on a PSP-1000 at 60 fps from a cold launch to game over, with stable input, all sprites and text, audio without dropouts, two identical captures, and no crash in a 10-minute soak — without a change to raylib's public API; any library change is its own sprint |

## What this phase is about

Phases 1–13 proved the library with probes, benchmarks and the upstream example corpus. None of them is a game:
they run for seconds, have no state machine, do not load a full asset set, and do not keep audio, input and
rendering alive together for minutes. Phase 14 ports **complete, small games** as integration tests with
gameplay. A game exercises the boundaries between features, where the probes stop: a title scene handing over
to a stage, entities spawning and dying while a music stream plays, a font atlas used every frame, and memory
that must not fragment over a long run.

The phase is deliberately gated on a real, existing codebase rather than a purpose-built demo. The first target
is [CppGalaga v3](https://github.com/TheCoderRaman/CppGalaga): a 224×288, 60 Hz, single-player Galaga clone
with 24 engine-agnostic translation units and interchangeable SDL2/SFML/olcPixelGameEngine backends. It is the
right size — small enough to finish, large enough to find the seams.

Port code lives **outside** this repository. CppGalaga has no licence file and carries Namco Galaga art and
audio; the port, its assets and any EBOOT stay local (`/home/psp/galaga-psp`) and are never committed. This
repository keeps the phase and sprint documents, the measurement records, and any library fix that the port
proves is needed.

## Target audit — CppGalaga v3

Source: `/home/psp/ref/CppGalaga/Galaga_v3`, cloned from `TheCoderRaman/CppGalaga` at `b1232a6` (2019-06-03),
read-only. Three variants exist; v3 is the only one with the media-layer split.

| item | value | tag |
|---|---|---|
| Game core | 24 non-backend `.cpp` files, 4,051 lines, engine-agnostic; largest is `GameController.cpp` (1,135 lines) | `[measured: source scan, 2026-11-23]` |
| Platform seam | `Renderer` (6 methods), `Game` (one entry point), `AudioPlayer` (6 functions), `Keyboard` (2 functions) | `[public: include/infra/media_layer/, include/infra/Keyboard.hpp]` |
| Input used by gameplay | `LEFT`, `RIGHT`, `FIRE` only; `PAUSE` and `EXIT` exist in the enum but are never read | `[measured: grep, 2026-11-23]` |
| Logical screen | 224×288, fixed 60 Hz step (`TIME_STEP` 16,667 µs) | `[public: include/infra/Util.hpp, src/infra/*_impl/Game.cpp]` |
| Graphics | 27 PNGs; animation sheets are 384×16 (24 frames of 16×16); `title_shine` is 540×36; `enemy_beam` is 138×80 | `[measured: PNG headers, 2026-11-23]` |
| Audio | 23 mono 44.1 kHz 16-bit WAVs, 7.0 MB on disk; decoded to the device's **f32 stereo** format they are **27.5 MiB** (SFX 11.8, music 15.7), so music and long clips must stream | `[derived: src/raudio.c f32/stereo conversion + WAV headers, 2026-11-23]` |
| Heap | the auto heap leaves ~768 KB free; an asset-heavy port must declare `PSP_HEAP_SIZE_KB` in its main source | `[measured: PSP-1000, environment.md]` |
| Text | PressStart2P at 8 px, fixed 8-pixel advance; two text calls alias to the same renderer path | `[public: res/font/, src/infra/*_impl/Renderer.cpp]` |
| Core build | 23 of the 24 non-backend files pass `psp-gcc 15.2.0 -std=gnu++14 -fsyntax-only`; the 24th, `src/infra/Game.cpp`, is an obsolete pre-split entry point that includes headers that no longer exist and is deleted by the port | `[measured: psp-gcc 15.2.0, 2026-11-23]` |
| Licence | no `LICENSE` in the repository; art and audio are Namco material | `[measured: repository scan, 2026-11-23]` |

**Texture budget is the first real constraint.** At pspgu's default 8888 the padded set is **908,544 B** against
**913,344 B free** (memory map §3) — before the font atlas. The largest items are `title_shine` 540×36 →
1024×64 = 262,144 B, `enemy_beam` 138×80 → 256×128 = 131,072 B, and thirteen 384×16 sheets at 512×16 =
32,768 B each (425,984 B). At 16 bits the same set is **454,272 B**. `[derived: PNG dimensions + pspgu
power-of-two padding, 2026-11-23]`

## Capability map

The port needs nothing new from the library; every row below is a measured path from an earlier phase.

| CppGalaga seam | raylib-psp facility | state |
|---|---|---|
| `drawPoint` (100 background stars) | `DrawPixel` on the shapes texture; the PSP batch coalesces the run | Phase 7: 3,000 shapes = 4 draws, one bind |
| `drawRectangle` (outline, debug only) | `DrawRectangleLines` | Phase 7 |
| `drawSprite(sprite, dst, src, size)` | `DrawTextureRec`/`DrawTexturePro`; 512-pixel windows for wide sheets | Phases 6–7 |
| `drawText1`/`drawText2` (8 px grid) | `LoadFontEx` at 8 px plus **fixed 8-pixel advance** in the backend, or a pre-baked bitmap atlas | Phase 7 text path; fixed advance is port-side |
| `setTranslation` | logical→physical offset in the backend | — |
| `isKeyPressed`/`isKeyPressedOnce` | D-pad/nub plus CROSS; the game's own consumed-state map is kept | Phase 8 |
| `playSound`/`playMusic` | `Sound`/`Music` over miniaudio's PSP backend; aliases for overlapping repeats | Phase 9 |
| game loop / fixed step | `InitWindow`/`EndDrawing`, vsync-paced, accumulator at 1/60 s | Phases 1–5 |
| asset paths | `host0:` for PSPLink, `ms0:/PSP/GAME/<TITLE>/` for an EBOOT | Phase 13 harness pattern |

## Known gaps and risks

- **Screen geometry.** The game is 224×288 portrait; the PSP displays 480×272 landscape. There is no integer
  scale that fits. The recommended v1 is **logical height 272**, drawn 1:1 and centred at x = 128: pixel-perfect,
  224×272 visible, 16 logical rows shorter. The layout change is more than the constant: the HUD's scale centre
  `(112,144)` must become `(112,136)`, the lives text at `Hud.cpp:145` (`y = 278`) must move to
  `SCREEN_HEIGHT - 10`, and `Path.cpp`'s cell height changes. The alternative is a rotated **tate** mode
  (288×224), deferred until the straight port runs.
- **The installed `libGL.a` is not pspgu.** `tools/pspgu-build.sh --check` currently reports a pspGL build. The
  512-window, residency and 16-bit-format facilities the port relies on are pspgu features. S1 settles the target:
  install the current pspgu (b37 on `master`; build it from a **clean worktree**, because the main tree carries
  uncommitted b33-era `tools/pspgu/` content) or repack every texture to ≤512 and accept pspGL's 2022 path.
- **`PlaySound` restarts a buffer; it does not allocate a voice.** Overlapping repeats of the same SFX cut each
  other off. Galaga repeats `fighter_shot` and explosion sounds, so the backend keeps a small `LoadSoundAlias`
  pool and round-robins it.
- **Audio memory is the second budget.** `LoadSound` converts every file to f32 stereo at the device rate, so the
  7.0 MB WAV set costs 27.5 MiB as Sounds. Music must be streamed; the long SFX (over ~2 s: `fighter_captured`,
  `formation_scaled`, `fighter_explosion`, `enemy_beam`, `enemy_beam_fighter_captured`, `fighter_rescued`) need a
  streaming or trimming decision before loading all 17 as Sounds.
- **Heap must be declared.** The auto heap leaves ~768 KB. Declare `PSP_HEAP_SIZE_KB(n);` in the port's main
  source (the PSPSDK Make variable of the same name is not wired by `build.mak`) and verify the boot free figure.
- **Music semantics.** SDL_mixer plays one music stream at a time; raylib starts a stream per call and does not
  stop the previous one. The backend must stop the current stream before starting the next, and pump
  `UpdateMusicStream` once per frame.
- **Startup I/O.** ~50 file opens (27 PNG + 23 WAV); Memory Stick opens cost ~17 ms each. A cold load of 1–2 s is
  expected and must be measured, not hidden behind a black screen.
- **128-pixel side bars.** At 1:1, 224 of 480 columns are occupied. This is a scope decision, not a defect; the
  tate option is the only way to use the panel's long axis for the game's long axis.

## Readiness review (2026-11-23)

The three sprints were re-read against the source and the toolchain after they were written. Six findings changed
the plan; all are now in the task tables.

| finding | evidence | fixed in |
|---|---|---|
| **The C++ build recipe works, but `build.mak` does not provide it.** | A `/tmp` app using `std::string`, `std::map`, `std::list`, `std::shared_ptr` and `std::function` linked to a 2.64 MB PRX with an explicit `%.o: %.cpp` rule and `-lstdc++`. PSPSDK's `build.mak` supplies no `.cpp` rule and links with `psp-gcc`. | P14-S1 T2 |
| **The heap cannot be left on auto.** | Auto heap leaves ~768 KB (`environment.md`); the STL and `LoadSound` need more. `PSP_HEAP_SIZE_KB(n);` is a source macro, not a `build.mak` variable. | P14-S1 T2, P14-S2 T5 |
| **Audio does not fit as decoded Sounds.** | `LoadSound` converts to f32 stereo at the device rate: the 7.0 MB WAV set becomes **27.5 MiB** (SFX 11.8, music 15.7). Music must stream; long SFX must stream or be shortened. | P14-S2 T4 |
| **The 8-pixel text path has public accessors.** | `GetGlyphIndex()` and `GetGlyphAtlasRec()` are exported; `LoadFontEx` already sets `TEXTURE_FILTER_POINT`. The port must still force an 8-pixel advance. | P14-S1 T4 |
| **Submission counters need a source.** | pspgu exports `pspguVramStats`, `pspguResidencyStats` and `pspguWindowStats`; raylib exposes no public draw counter. Logical draw counts must be counted in the backend. | P14-S1 T6/T8 |
| **The installed driver is pspGL, not pspgu.** | `tools/pspgu-build.sh --check` reports a pspGL build; pspgu b37 is on `master`. The main tree's `tools/pspgu/` is b33-era and locally edited, so the driver must be built from a clean worktree. | P14-S1 T1 |

The review did **not** change the display recommendation (272-high, 1:1, centred), the local-only licensing rule,
or the sprint order. It did add an explicit heap decision to S1, an audio streaming decision to S2, and the
counter-source split to S1.

## Sprints

| sprint | scope | state |
|---|---|---|
| [P14-S1 — Backend and first frame](../../sprints/082-phase14-backend-and-first-frame.md) | port scaffold, display decision, renderer, input, game loop, title and first stage without audio | `todo` |
| [P14-S2 — Assets, VRAM and audio](../../sprints/083-phase14-assets-vram-and-audio.md) | texture budget/repack, 8 px font, audio with alias pools, asset staging, EBOOT | `todo` |
| [P14-S3 — Gameplay validation and close](../../sprints/084-phase14-gameplay-validation-and-close.md) | deterministic run, 10-minute soak, attribution, packaging, phase close | `todo` |

The order is deliberate: the seam and the display decision first, because every later measurement is taken
through them; assets and audio next, because they decide whether the port fits at all; validation last, because
it measures a build that is no longer moving.

## Exit criteria

- [ ] P14-S1: title scene and one stage frame render correctly, input works, 600 frames without a crash
- [ ] P14-S2: texture set fits with zero allocation failures; 8 px text matches the 8-pixel grid; audio plays
      with zero dropouts in a 5-minute run
- [ ] P14-S3: a 10-minute deterministic run reaches game over with stable heap and VRAM, 60 fps and p95 frame
      time recorded
- [ ] two identical captures for the title and the first stage, archived with the build number
- [ ] no change to raylib's public API; if a library fix was needed, it has its own sprint and the port links the
      fixed library
- [ ] the library's own gates still pass on the same install (`basic_shapes`, `batch_check`, `scene_2d`)

## Open decisions

1. **Display mode** — A (272, 1:1, centred; recommended) or C (tate, rotated). Settled in P14-S1 T1.
2. **Driver target** — the current pspgu (b37 on `master`) with `--texfmt=auto` (recommended) or the currently
   installed pspGL with all textures repacked to ≤512. Settled in P14-S1 T1.
3. **Port home** — `/home/psp/galaga-psp` (outside this repository), keeping upstream pristine. Settled in
   P14-S1 T2.
4. **Distribution** — local only until the upstream licence and asset rights are resolved. Recorded here as the
   standing assumption.

## Related

- [Memory map §5 — roadmap](../../memory-map.md#5-roadmap) and [§7 — reference sources](../../memory-map.md#7-reference-sources-read-only-outside-this-repo)
- [Phase 6 — Textures and VRAM](../06-textures-vram/index.md), [Phase 7 — 2D specialisation](../07-2d-specialisation/index.md)
- [Phase 8 — Input](../08-input/index.md), [Phase 9 — Audio](../09-audio/index.md)
- [Phase 13 — Samples porting](../13-samples-porting/index.md) for the asset-staging and input-remap patterns
- [Sprint template](../../templates/sprint.md)
