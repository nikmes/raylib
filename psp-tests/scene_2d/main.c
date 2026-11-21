/*
 * scene_2d - sprint 018: the Phase 7 workload corpus and its counters.
 *
 * Five deterministic scenes, one per build:
 *
 *   MODE=text       fixed 78-character lines of the default font
 *   MODE=tiles      30 x 17 visible 16-pixel tiles from a 120 x 68 logical map
 *   MODE=particles  COUNT deterministic textured particles
 *   MODE=shapes     COUNT rectangles, triangles and lines in three contiguous groups
 *   MODE=mixed      all four in one frame
 *
 * Every scene is a pure function of MODE, COUNT, LINES and the frame index: no
 * GetRandomValue and no wall-clock animation. The final state is drawn for three
 * frozen frames after the measured ones, so a capture taken after DONE is stable.
 *
 * Counters are the link-time --wrap counters used by bunnymark/batch_check, at the
 * GL boundary. They count ends (one glEnd), draws (glDrawArrays + glDrawElements),
 * binds (non-zero glBindTexture), unbinds and arrayVertices. The marker reports the
 * totals for the measured frames; the sprint divides by FRAMES for a per-frame figure.
 */

#include <pspkernel.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "raylib.h"
#include "psp_counters.h"

#if defined(TEXT_IMPL_DIRECT) || defined(PARTICLE_IMPL_DIRECT)
    #include "rlgl.h"       // the same inline PSP batcher the library uses
#endif

// The driver's test-facing symbols; the structs must match pspgu_internal.h.
typedef struct {
    unsigned int reserved;
    unsigned int requested;
    unsigned int used;
    unsigned int free;
    unsigned int largest;
    unsigned int internal;
    unsigned int allocs, frees, failures;
} PspGuVramStats;

typedef struct {
    unsigned int resident;
    unsigned int shadowBytes;
    unsigned int evictions;
    unsigned int reloads;
    unsigned int failed;
    unsigned int coalesced;
} PspGuResidencyStats;

void pspguVramStats(PspGuVramStats *out);
void pspguVramReport(const char *when);
void pspguResidencyStats(PspGuResidencyStats *out);
void pspguWindowStats(unsigned int *draws, unsigned int *switches);

PSP_MODULE_INFO("scene_2d", 0, 1, 1);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);

#ifndef SCENE_FRAMES
    #define SCENE_FRAMES 300
#endif
#ifndef SCENE_COUNT
    #define SCENE_COUNT 2000
#endif
#ifndef SCENE_LINES
    #define SCENE_LINES 8
#endif
#ifndef BUDGET_FLOOR_KB
    #define BUDGET_FLOOR_KB 128
#endif
#ifndef TARGET_FPS
    #define TARGET_FPS 0
#endif
#ifndef FONT_PATH
    #define FONT_PATH "host0:/fonts/KAISG.ttf"
#endif
#ifndef FONT_SIZE
    #define FONT_SIZE 32
#endif
#ifndef FONT_SIZE2
    #define FONT_SIZE2 12
#endif
#ifndef FONT_GLYPHS
    #define FONT_GLYPHS 95
#endif
#ifndef OVERSIZE_W
    #define OVERSIZE_W 1024
#endif
#ifndef OVERSIZE_H
    #define OVERSIZE_H 128
#endif
#ifndef OVERSIZE_ROTATE
    #define OVERSIZE_ROTATE 0
#endif
#ifndef SCENE_MARKER
    #define SCENE_MARKER "scene2d_done.txt"
#endif

#define MAX_FRAMES     2000
#define MAX_PARTICLES  4096
#define MAX_LINES       64
#define LINE_CHARS      78

#define SCREEN_W 480
#define SCREEN_H 272
#define TILE     16
#define TILES_X  (SCREEN_W/TILE)        // 30
#define TILES_Y  (SCREEN_H/TILE)        // 17
#define MAP_W    120
#define MAP_H     68

static float frameTimes[MAX_FRAMES];
#if defined(MODE_TEXT) || defined(MODE_MIXED)
static char textLines[MAX_LINES][LINE_CHARS + 1];
static int  textGlyphs;
#endif
static int  gFrame;
static int  tileOffsetX;
static int  tileOffsetY;

#if defined(MODE_TILES) || defined(MODE_MIXED)
#if !defined(ATLAS_WIDE) || !defined(ATLAS_REF)
static Texture2D tileTex;
#endif
#ifdef ATLAS_REF
static Texture2D tileTexA, tileTexB;
#endif
#endif
#if defined(MODE_PARTICLES) || defined(MODE_MIXED)
static Texture2D particleTex;

typedef struct {
    Vector2 pos;
    Vector2 vel;
    Color color;
} Particle;

static Particle *particles;
static int particleCount;
#endif

// Deterministic 32-bit LCG. Fixed seed; every scene that needs numbers gets them here.
#if defined(MODE_PARTICLES) || defined(MODE_MIXED)
static uint32_t rngState = 0x9E3779B9u;

static uint32_t rngNext(void)
{
    rngState = rngState*1664525u + 1013904223u;
    return rngState;
}

static float rngFloat(void)
{
    return (float)(rngNext() >> 8)*(1.0f/16777216.0f);
}
#endif

#if defined(MODE_SHAPES) || defined(MODE_MIXED)

static unsigned int hashIndex(unsigned int i)
{
    uint32_t x = i*2654435761u;
    x ^= x >> 13;
    x *= 1274126177u;
    x ^= x >> 16;
    return (unsigned int)x;
}
#endif

static int cmpFloat(const void *a, const void *b)
{
    float fa = *(const float *)a;
    float fb = *(const float *)b;
    return (fa > fb) - (fa < fb);
}

// ---------------------------------------------------------------------------------
// Text
// ---------------------------------------------------------------------------------
#if defined(MODE_TEXT) || defined(MODE_MIXED)
static const char *TEXT_SRC =
    "The quick brown fox jumps over the lazy dog. Pack my box with five dozen liquor "
    "jugs. 0123456789 ABCDEFGHIJKLMNOPQRSTUVWXYZ abcdefghijklmnopqrstuvwxyz";

static int countGlyphs(const char *s)
{
    int n = 0;
    int i = 0;
    int len = (int)strlen(s);

    while (i < len)
    {
        int bytes = 0;
        int cp = GetCodepointNext(&s[i], &bytes);

        if ((cp != ' ') && (cp != '\t') && (cp != '\n')) n++;
        i += bytes;
    }

    return n;
}

static void textInit(void)
{
    int srcLen = (int)strlen(TEXT_SRC);

    for (int l = 0; l < SCENE_LINES; l++)
    {
        for (int i = 0; i < LINE_CHARS; i++) textLines[l][i] = TEXT_SRC[(l*17 + i)%srcLen];
        textLines[l][LINE_CHARS] = 0;
    }

    textGlyphs = 0;
    for (int l = 0; l < SCENE_LINES; l++) textGlyphs += countGlyphs(textLines[l]);
}

#if !defined(TEXT_IMPL_WALK) && !defined(TEXT_IMPL_DIRECT)
static void textApi(void)
{
    Font font = GetFontDefault();

    for (int l = 0; l < SCENE_LINES; l++)
        DrawTextEx(font, textLines[l], (Vector2){ 4.0f, 4.0f + 10.0f*l }, 10.0f, 0.0f, RAYWHITE);
}
#endif

#if defined(TEXT_IMPL_WALK)
// Sprint 019 T1: the codepoint walk, glyph lookup and advance only.
static void textWalk(void)
{
    Font font = GetFontDefault();
    float scale = 10.0f/(float)font.baseSize;

    for (int l = 0; l < SCENE_LINES; l++)
    {
        const char *s = textLines[l];
        float offsetX = 0.0f;
        int i = 0;

        while (s[i] != 0)
        {
            int bytes = 0;
            int cp = GetCodepointNext(&s[i], &bytes);
            int index = GetGlyphIndex(font, cp);

            if (cp == '\n') offsetX = 0.0f;
            else
            {
                if ((cp != ' ') && (cp != '\t'))
                {
                    // Same layout arithmetic the draw path performs, no vertices.
                    volatile float x = 4.0f + offsetX + font.glyphs[index].offsetX*scale;
                    (void)x;
                }

                if (font.glyphs[index].advanceX == 0) offsetX += font.recs[index].width*scale;
                else offsetX += (float)font.glyphs[index].advanceX*scale;
            }

            i += bytes;
        }
    }
}
#endif

#if defined(TEXT_IMPL_DIRECT)
// Sprint 019 T1: the app-side run-batched loop, same geometry as DrawTextEx.
static void textDirect(void)
{
    Font font = GetFontDefault();
    float scale = 10.0f/(float)font.baseSize;
    float pad = (float)font.glyphPadding;
    float texW = (float)font.texture.width;
    float texH = (float)font.texture.height;

    rlSetTexture(font.texture.id);
    rlBegin(RL_QUADS);

    for (int l = 0; l < SCENE_LINES; l++)
    {
        const char *s = textLines[l];
        float offsetX = 0.0f;
        int i = 0;

        while (s[i] != 0)
        {
            int bytes = 0;
            int cp = GetCodepointNext(&s[i], &bytes);
            int index = GetGlyphIndex(font, cp);

            if (cp == '\n') offsetX = 0.0f;
            else
            {
                if ((cp != ' ') && (cp != '\t'))
                {
                    Rectangle rec = font.recs[index];
                    float x = 4.0f + offsetX + font.glyphs[index].offsetX*scale - pad*scale;
                    float y = 4.0f + 10.0f*l + font.glyphs[index].offsetY*scale - pad*scale;
                    float w = (rec.width + 2.0f*pad)*scale;
                    float h = (rec.height + 2.0f*pad)*scale;
                    float u0 = (rec.x - pad)/texW;
                    float v0 = (rec.y - pad)/texH;
                    float u1 = (rec.x + rec.width + pad)/texW;
                    float v1 = (rec.y + rec.height + pad)/texH;

                    rlColor4ub(RAYWHITE.r, RAYWHITE.g, RAYWHITE.b, RAYWHITE.a);

                    rlTexCoord2f(u0, v0); rlVertex2f(x, y);
                    rlTexCoord2f(u0, v1); rlVertex2f(x, y + h);
                    rlTexCoord2f(u1, v1); rlVertex2f(x + w, y + h);
                    rlTexCoord2f(u1, v0); rlVertex2f(x + w, y);
                }

                if (font.glyphs[index].advanceX == 0) offsetX += font.recs[index].width*scale;
                else offsetX += (float)font.glyphs[index].advanceX*scale;
            }

            i += bytes;
        }
    }

    rlEnd();
    rlSetTexture(0);
}
#endif

static void textDraw(void)
{
#if defined(TEXT_IMPL_WALK)
    textWalk();
#elif defined(TEXT_IMPL_DIRECT)
    textDirect();
#else
    textApi();
#endif
}
#endif

// ---------------------------------------------------------------------------------
// Tiles
// ---------------------------------------------------------------------------------
#if defined(MODE_TILES) || defined(MODE_MIXED)
static const Color tilePalette[8] = {
    { 220,  60,  60, 255 }, {  60, 200,  70, 255 }, {  60, 100, 220, 255 },
    { 220, 200,  60, 255 }, { 200,  70, 200, 255 }, {  70, 210, 210, 255 },
    { 230, 140,  50, 255 }, { 170, 170, 170, 255 }
};

static void tilesInit(void)
{
#ifdef ATLAS_WIDE
    // 1024x128: 16 x 2 tiles of 64 px. The x=512 boundary falls inside a tile sheet, so
    // a row that crosses it exercises the window split.
    Image atlas = GenImageColor(1024, 128, BLANK);

    for (int i = 0; i < 32; i++)
    {
        int x = (i%16)*64;
        int y = (i/16)*64;
        Color c = tilePalette[i%8];

        ImageDrawRectangle(&atlas, x, y, 64, 64, c);
        ImageDrawRectangle(&atlas, x + (i%3)*7 + 2, y + (i%4)*5 + 2, 8, 5, BLACK);
    }

#ifdef ATLAS_REF
    {
        Image a = ImageFromImage(atlas, (Rectangle){ 0, 0, 512, 128 });
        Image b = ImageFromImage(atlas, (Rectangle){ 512, 0, 512, 128 });
        tileTexA = LoadTextureFromImage(a);
        tileTexB = LoadTextureFromImage(b);
        UnloadImage(a); UnloadImage(b);
    }
#else
    tileTex = LoadTextureFromImage(atlas);
#endif

    UnloadImage(atlas);
#else
    Image atlas = GenImageColor(128, 128, BLANK);

    for (int i = 0; i < 64; i++)
    {
        int x = (i%8)*TILE;
        int y = (i/8)*TILE;
        Color c = tilePalette[i%8];

        ImageDrawRectangle(&atlas, x, y, TILE, TILE, c);
        // A deterministic notch makes a wrong UV visible without changing the palette.
        ImageDrawRectangle(&atlas, x + (i%3)*3 + 1, y + (i%4)*2 + 1, 3, 2, BLACK);
    }

    tileTex = LoadTextureFromImage(atlas);
    UnloadImage(atlas);
#endif
}

static void tilesDraw(void)
{
#ifdef ATLAS_WIDE
    const int cols = 7;
    const int rows = 4;
    int col0 = 6;
    int row0 = 0;

#ifdef SCROLL
    col0 += (gFrame/60) % (MAP_W - cols);
    row0 += (gFrame/60) % (MAP_H - rows);
#endif

    for (int vy = 0; vy < rows; vy++)
    {
        for (int vx = 0; vx < cols; vx++)
        {
            int mx = col0 + vx;
            int my = row0 + vy;
            int idx = (mx%16) + (my%2)*16;      // adjacent visible columns stay adjacent in the atlas
            int gx = (idx%16)*64;
            int gy = (idx/16)*64;
            Rectangle src = { (float)gx, (float)gy, 64.0f, 64.0f };
            Rectangle dst = { (float)(vx*64), (float)(vy*64), 64.0f, 64.0f };

#ifdef ATLAS_REF
            if (gx < 512)
            {
                DrawTexturePro(tileTexA, src, dst, (Vector2){ 0.0f, 0.0f }, 0.0f, WHITE);
            }
            else
            {
                src.x = (float)(gx - 512);
                DrawTexturePro(tileTexB, src, dst, (Vector2){ 0.0f, 0.0f }, 0.0f, WHITE);
            }
#else
            DrawTexturePro(tileTex, src, dst, (Vector2){ 0.0f, 0.0f }, 0.0f, WHITE);
#endif
        }
    }
#else
    Rectangle src = { 0.0f, 0.0f, TILE, TILE };

    for (int vy = 0; vy < TILES_Y; vy++)
    {
        for (int vx = 0; vx < TILES_X; vx++)
        {
            int mx = (tileOffsetX + vx)%MAP_W;
            int my = (tileOffsetY + vy)%MAP_H;
            int idx = (mx*7 + my*13)%64;

            src.x = (float)((idx%8)*TILE);
            src.y = (float)((idx/8)*TILE);

            Rectangle dst = { (float)(vx*TILE), (float)(vy*TILE), TILE, TILE };
            DrawTexturePro(tileTex, src, dst, (Vector2){ 0.0f, 0.0f }, 0.0f, WHITE);
        }
    }
#endif
}
#endif

// ---------------------------------------------------------------------------------
// Particles
// ---------------------------------------------------------------------------------
#if defined(MODE_PARTICLES) || defined(MODE_MIXED)
static void particlesInit(void)
{
    particleCount = SCENE_COUNT;
    if (particleCount > MAX_PARTICLES) particleCount = MAX_PARTICLES;

    particles = (Particle *)malloc((size_t)particleCount*sizeof(Particle));
    if (particles == NULL)
    {
        TraceLog(LOG_ERROR, "SCENE particles: could not allocate %i", particleCount);
        sceKernelSleepThread();
        return;
    }

    Image img = GenImageColor(32, 32, (Color){ 20, 20, 20, 255 });
    ImageDrawRectangle(&img,  0,  0, 16, 16, (Color){ 230,  40,  40, 255 });
    ImageDrawRectangle(&img, 16,  0, 16, 16, (Color){  40, 220,  60, 255 });
    ImageDrawRectangle(&img,  0, 16, 16, 16, (Color){  50,  90, 230, 255 });
    ImageDrawRectangle(&img, 16, 16, 16, 16, (Color){ 230, 220,  50, 255 });
    particleTex = LoadTextureFromImage(img);
    UnloadImage(img);

    for (int i = 0; i < particleCount; i++)
    {
        particles[i].pos = (Vector2){ rngFloat()*(float)SCREEN_W, rngFloat()*(float)SCREEN_H };
        particles[i].vel = (Vector2){ (rngFloat()*2.0f - 1.0f)*1.5f,
                                      (rngFloat()*2.0f - 1.0f)*1.5f };
        particles[i].color = (Color){ (unsigned char)(80 + rngNext()%176),
                                      (unsigned char)(80 + rngNext()%176),
                                      (unsigned char)(80 + rngNext()%176), 255 };
    }
}

static void particlesUpdate(void)
{
    for (int i = 0; i < particleCount; i++)
    {
        particles[i].pos.x += particles[i].vel.x;
        particles[i].pos.y += particles[i].vel.y;

        if (particles[i].pos.x < -32.0f) particles[i].pos.x += SCREEN_W + 32.0f;
        if (particles[i].pos.x >  SCREEN_W) particles[i].pos.x -= SCREEN_W + 32.0f;
        if (particles[i].pos.y < -32.0f) particles[i].pos.y += SCREEN_H + 32.0f;
        if (particles[i].pos.y >  SCREEN_H) particles[i].pos.y -= SCREEN_H + 32.0f;
    }
}

#if !defined(PARTICLE_IMPL_DIRECT)
static void particlesDrawApi(void)
{
    Rectangle src = { 0.0f, 0.0f, 32.0f, 32.0f };

    for (int i = 0; i < particleCount; i++)
    {
        Rectangle dst = { particles[i].pos.x, particles[i].pos.y, 32.0f, 32.0f };
        DrawTexturePro(particleTex, src, dst, (Vector2){ 0.0f, 0.0f }, 0.0f, particles[i].color);
    }
}
#endif

#if defined(PARTICLE_IMPL_DIRECT)
static void particlesDrawDirect(void)
{
    rlSetTexture(particleTex.id);
    rlBegin(RL_QUADS);

    for (int i = 0; i < particleCount; i++)
    {
        float x = particles[i].pos.x;
        float y = particles[i].pos.y;
        Color c = particles[i].color;

        rlColor4ub(c.r, c.g, c.b, c.a);

        rlTexCoord2f(0.0f, 0.0f); rlVertex2f(x, y);
        rlTexCoord2f(0.0f, 1.0f); rlVertex2f(x, y + 32.0f);
        rlTexCoord2f(1.0f, 1.0f); rlVertex2f(x + 32.0f, y + 32.0f);
        rlTexCoord2f(1.0f, 0.0f); rlVertex2f(x + 32.0f, y);
    }

    rlEnd();
    rlSetTexture(0);
}
#endif

static void particlesDraw(void)
{
#if defined(PARTICLE_IMPL_DIRECT)
    particlesDrawDirect();
#else
    particlesDrawApi();
#endif
}
#endif

// ---------------------------------------------------------------------------------
// Shapes
// ---------------------------------------------------------------------------------
#if defined(MODE_SHAPES)
static void shapesDraw(void)
{
    for (int i = 0; i < SCENE_COUNT; i++)
    {
        unsigned int h = hashIndex((unsigned int)i*3u + 0u);
        unsigned int h2 = hashIndex((unsigned int)i*3u + 1u);
        unsigned int h3 = hashIndex((unsigned int)i*3u + 2u);
        Color c = { (unsigned char)(40 + h%200), (unsigned char)(40 + h2%200),
                    (unsigned char)(40 + h3%200), 255 };

        float x = (float)(h % (SCREEN_W - 40));
        float y = (float)(h2 % (SCREEN_H - 40));
        float w = (float)(2 + h3%40);
        float hh = (float)(2 + h%30);
        DrawRectangle((int)x, (int)y, (int)w, (int)hh, c);
    }

    for (int i = 0; i < SCENE_COUNT; i++)
    {
        unsigned int h = hashIndex((unsigned int)i*5u + 11u);
        unsigned int h2 = hashIndex((unsigned int)i*5u + 12u);
        unsigned int h3 = hashIndex((unsigned int)i*5u + 13u);
        Color c = { (unsigned char)(40 + h%200), (unsigned char)(40 + h2%200),
                    (unsigned char)(40 + h3%200), 255 };

        Vector2 a = { (float)(h % SCREEN_W), (float)(h2 % SCREEN_H) };
        Vector2 b = { (float)(a.x + 4 + h3%20), (float)(a.y + 4 + h2%20) };
        Vector2 cc = { (float)(a.x - 4 - h%10), (float)(a.y + 6 + h3%14) };
        DrawTriangle(a, b, cc, c);
    }

    for (int i = 0; i < SCENE_COUNT; i++)
    {
        unsigned int h = hashIndex((unsigned int)i*7u + 21u);
        unsigned int h2 = hashIndex((unsigned int)i*7u + 22u);
        unsigned int h3 = hashIndex((unsigned int)i*7u + 23u);
        Color c = { (unsigned char)(40 + h%200), (unsigned char)(40 + h2%200),
                    (unsigned char)(40 + h3%200), 255 };

        int x1 = (int)(h % SCREEN_W);
        int y1 = (int)(h2 % SCREEN_H);
        int x2 = (int)(h3 % SCREEN_W);
        int y2 = (int)((h + h3) % SCREEN_H);
        DrawLine(x1, y1, x2, y2, c);
    }
}
#endif

// ---------------------------------------------------------------------------------
// Font atlas probe (sprint 019 T5)
// ---------------------------------------------------------------------------------
#if defined(MODE_FONT)
static Font probeFont;
static Font probeFont2;
static int probeFallback;
static int probeMismatches;
static int probeMismatches2;
static const char *FONT_SAMPLE = "Sphinx of black quartz, judge my vow. 0123456789";

// The reference is the linear scan GetGlyphIndex used before sprint 019, including
// the '?' fallback. Any cached index must equal it.
static int lookupMismatches(Font font)
{
    int bad = 0;

    if ((font.glyphs == NULL) || (font.glyphCount <= 0)) return -1;

    for (int cp = 0; cp <= 0x2FF; cp++)
    {
        int ref = 0;
        int fallback = 0;

        for (int i = 0; i < font.glyphCount; i++)
        {
            if (font.glyphs[i].value == 63) fallback = i;
            if (font.glyphs[i].value == cp) { ref = i; break; }
        }

        if ((ref == 0) && (font.glyphs[0].value != cp)) ref = fallback;
        if (GetGlyphIndex(font, cp) != ref) bad++;
    }

    return bad;
}

static void fontInit(void)
{
#if defined(FONT_ORDER_REVERSE)
    // Exercise the unordered-glyph map in GetGlyphIndex: the same 95 codepoints, reversed.
    static int codepoints[FONT_GLYPHS];
    for (int i = 0; i < FONT_GLYPHS; i++) codepoints[i] = 32 + (FONT_GLYPHS - 1 - i);
    probeFont = LoadFontEx(FONT_PATH, FONT_SIZE, codepoints, FONT_GLYPHS);
#else
    probeFont = LoadFontEx(FONT_PATH, FONT_SIZE, NULL, FONT_GLYPHS);
#endif
    probeFallback = (probeFont.texture.id == GetFontDefault().texture.id);
    probeMismatches = lookupMismatches(probeFont);

    // A second size forces the glyph-index and layout caches to rebuild inside one run.
    probeFont2 = LoadFontEx(FONT_PATH, FONT_SIZE2, NULL, FONT_GLYPHS);
    probeMismatches2 = lookupMismatches(probeFont2);

    TraceLog(LOG_INFO, "FONT probe path=%s size=%i glyphs=%i tex=%ix%i id=%u fallback=%i mismatches=%i",
             FONT_PATH, FONT_SIZE, probeFont.glyphCount, probeFont.texture.width,
             probeFont.texture.height, probeFont.texture.id, probeFallback, probeMismatches);
    TraceLog(LOG_INFO, "FONT probe2 path=%s size=%i glyphs=%i tex=%ix%i id=%u mismatches=%i",
             FONT_PATH, FONT_SIZE2, probeFont2.glyphCount, probeFont2.texture.width,
             probeFont2.texture.height, probeFont2.texture.id, probeMismatches2);
}

static void fontDraw(void)
{
    DrawTextEx(probeFont, FONT_SAMPLE, (Vector2){ 4.0f, 4.0f }, (float)FONT_SIZE, 1.0f, RAYWHITE);
    DrawTextEx(probeFont2, FONT_SAMPLE, (Vector2){ 4.0f, 8.0f + (float)FONT_SIZE }, (float)FONT_SIZE2, 1.0f, RAYWHITE);
}
#endif

// ---------------------------------------------------------------------------------
// Oversized texture scene (sprint 020 T4)
// ---------------------------------------------------------------------------------
#if defined(MODE_OVERSIZE)
#ifndef OVERSIZE_REF
static Texture2D oversizeTex;
#else
static Texture2D oversizeRefA, oversizeRefB;
#endif

static const Color oversizeGray[4] = {
    { 0, 0, 0, 255 }, { 85, 85, 85, 255 }, { 170, 170, 170, 255 }, { 255, 255, 255, 255 }
};

static Image makeOversizeImage(int w, int h)
{
    Image img = GenImageColor(w, h, BLACK);

    for (int y = 0; y < h; y++)
    {
        for (int x = 0; x < w; x++)
        {
            ImageDrawPixel(&img, x, y, oversizeGray[((x/16) + (y/16))%4]);
        }
    }

    if (w > 512)
    {
        for (int y = 0; y < h; y++)
        {
            ImageDrawPixel(&img, 511, y, RED);
            ImageDrawPixel(&img, 512, y, GREEN);
        }
    }
    if (h > 512)
    {
        for (int x = 0; x < w; x++)
        {
            ImageDrawPixel(&img, x, 511, RED);
            ImageDrawPixel(&img, x, 512, GREEN);
        }
    }

    return img;
}

static void oversizeInit(void)
{
    Image img = makeOversizeImage(OVERSIZE_W, OVERSIZE_H);

#ifdef OVERSIZE_REF
    if (OVERSIZE_W > 512)
    {
        Image a = ImageFromImage(img, (Rectangle){ 0, 0, 512, (float)OVERSIZE_H });
        Image b = ImageFromImage(img, (Rectangle){ 512, 0, (float)(OVERSIZE_W - 512), (float)OVERSIZE_H });
        oversizeRefA = LoadTextureFromImage(a);
        oversizeRefB = LoadTextureFromImage(b);
        UnloadImage(a); UnloadImage(b);
    }
    else
    {
        Image a = ImageFromImage(img, (Rectangle){ 0, 0, (float)OVERSIZE_W, 512 });
        Image b = ImageFromImage(img, (Rectangle){ 0, 512, (float)OVERSIZE_W, (float)(OVERSIZE_H - 512) });
        oversizeRefA = LoadTextureFromImage(a);
        oversizeRefB = LoadTextureFromImage(b);
        UnloadImage(a); UnloadImage(b);
    }
#else
    oversizeTex = LoadTextureFromImage(img);
#endif

    UnloadImage(img);
}

static void oversizeDraw(void)
{
    if (OVERSIZE_ROTATE != 0)
    {
#ifndef OVERSIZE_REF
        Rectangle src = { 0.0f, 0.0f, (float)OVERSIZE_W, (float)OVERSIZE_H };
        Rectangle dst = { 0.0f, 106.0f, 480.0f, 60.0f };
        DrawTexturePro(oversizeTex, src, dst, (Vector2){ 240.0f, 30.0f }, (float)OVERSIZE_ROTATE, WHITE);
#endif
        return;
    }

    float x = (OVERSIZE_W > 512)? -272.0f : 0.0f;
    float y = (OVERSIZE_H > 512)? -376.0f : 0.0f;

#ifndef OVERSIZE_REF
    Rectangle src = { 0.0f, 0.0f, (float)OVERSIZE_W, (float)OVERSIZE_H };
    DrawTexturePro(oversizeTex, src, (Rectangle){ x, y, (float)OVERSIZE_W, (float)OVERSIZE_H },
                   (Vector2){ 0.0f, 0.0f }, 0.0f, WHITE);
#else
    if (OVERSIZE_W > 512)
    {
        DrawTexturePro(oversizeRefA, (Rectangle){ 0, 0, 512, (float)OVERSIZE_H },
                       (Rectangle){ x, y, 512, (float)OVERSIZE_H }, (Vector2){ 0, 0 }, 0.0f, WHITE);
        DrawTexturePro(oversizeRefB, (Rectangle){ 0, 0, (float)(OVERSIZE_W - 512), (float)OVERSIZE_H },
                       (Rectangle){ x + 512, y, (float)(OVERSIZE_W - 512), (float)OVERSIZE_H },
                       (Vector2){ 0, 0 }, 0.0f, WHITE);
    }
    else
    {
        DrawTexturePro(oversizeRefA, (Rectangle){ 0, 0, (float)OVERSIZE_W, 512 },
                       (Rectangle){ x, y, (float)OVERSIZE_W, 512 }, (Vector2){ 0, 0 }, 0.0f, WHITE);
        DrawTexturePro(oversizeRefB, (Rectangle){ 0, 0, (float)OVERSIZE_W, (float)(OVERSIZE_H - 512) },
                       (Rectangle){ x, y + 512, (float)OVERSIZE_W, (float)(OVERSIZE_H - 512) },
                       (Vector2){ 0, 0 }, 0.0f, WHITE);
    }
#endif
}
#endif

// ---------------------------------------------------------------------------------
// Sprite culling scene (sprint 021 T1)
// ---------------------------------------------------------------------------------
#if defined(MODE_SPRITES)
static Texture2D spriteTex;
static int spriteVisible;

static void spritesInit(void)
{
    Image img = GenImageColor(32, 32, (Color){ 20, 20, 20, 255 });
    ImageDrawRectangle(&img,  0,  0, 16, 16, (Color){ 230,  40,  40, 255 });
    ImageDrawRectangle(&img, 16,  0, 16, 16, (Color){  40, 220,  60, 255 });
    ImageDrawRectangle(&img,  0, 16, 16, 16, (Color){  50,  90, 230, 255 });
    ImageDrawRectangle(&img, 16, 16, 16, 16, (Color){ 230, 220,  50, 255 });
    spriteTex = LoadTextureFromImage(img);
    UnloadImage(img);

    int offscreen = (SCENE_COUNT*SCENE_OFFSCREEN_PCT)/100;
    spriteVisible = SCENE_COUNT - offscreen;
}

static void spritesDraw(void)
{
    Rectangle src = { 0.0f, 0.0f, 32.0f, 32.0f };
    int offscreen = SCENE_COUNT - spriteVisible;

    for (int i = 0; i < offscreen; i++)
    {
        float x = 0.0f;
        float y = 0.0f;

        switch (i%4)
        {
            case 0: x = -64.0f - (float)((i%8)*40); y = (float)((i%7)*38); break;
            case 1: x = (float)SCREEN_W + 32.0f + (float)((i%4)*8); y = (float)((i%7)*38); break;
            case 2: x = (float)((i%14)*34); y = -64.0f - (float)((i%8)*40); break;
            default: x = (float)((i%14)*34); y = (float)SCREEN_H + 32.0f + (float)((i%4)*8); break;
        }

        DrawTexturePro(spriteTex, src, (Rectangle){ x, y, 32.0f, 32.0f },
                       (Vector2){ 0.0f, 0.0f }, 0.0f, WHITE);
    }

    for (int i = 0; i < spriteVisible; i++)
    {
        float x = (float)((i%14)*34);
        float y = (float)(((i/14)%7)*38);

        DrawTexturePro(spriteTex, src, (Rectangle){ x, y, 32.0f, 32.0f },
                       (Vector2){ 0.0f, 0.0f }, 0.0f, WHITE);
    }
}
#endif

// ---------------------------------------------------------------------------------
// Texture-cache strip scene (sprint 023 T1)
// ---------------------------------------------------------------------------------
#if defined(MODE_CACHE)
static Texture2D cacheTex;
static int cacheQuads;

static void cacheInit(void)
{
    // 480x256 logical; pspgu pads to 512x256 = 524,288 B at 8888 and it fits.
    Image img = GenImageColor(480, 256, BLACK);

    for (int y = 0; y < 256; y++)
    {
        for (int x = 0; x < 480; x++)
        {
            int checker = ((x/32) + (y/32))&1;
            Color c = {
                (unsigned char)(checker? 200 : 80),
                (unsigned char)y,
                (unsigned char)((x*255)/479),
                255
            };
            ImageDrawPixel(&img, x, y, c);
        }
    }

    cacheTex = LoadTextureFromImage(img);
    UnloadImage(img);

    int strips = (480 + SCENE_STRIP - 1)/SCENE_STRIP;
    cacheQuads = strips*SCENE_REPEAT;
}

static void cacheDraw(void)
{
    for (int r = 0; r < SCENE_REPEAT; r++)
    {
        for (int x0 = 0; x0 < 480; x0 += SCENE_STRIP)
        {
            int w = SCENE_STRIP;
            if ((x0 + w) > 480) w = 480 - x0;

            Rectangle src = { (float)x0, 0.0f, (float)w, 256.0f };
            Rectangle dst = { (float)x0, 0.0f, (float)w, 256.0f };
            DrawTexturePro(cacheTex, src, dst, (Vector2){ 0.0f, 0.0f }, 0.0f, WHITE);
        }
    }
}
#endif

// ---------------------------------------------------------------------------------
// Scene dispatch
// ---------------------------------------------------------------------------------
static int sceneObjects(void)
{
#if defined(MODE_TEXT)
    return textGlyphs;
#elif defined(MODE_TILES)
    #ifdef ATLAS_WIDE
        return 28;
    #else
        return TILES_X*TILES_Y;
    #endif
#elif defined(MODE_PARTICLES)
    return particleCount;
#elif defined(MODE_SHAPES)
    return SCENE_COUNT*3;
#elif defined(MODE_SPRITES)
    return SCENE_COUNT;
#elif defined(MODE_CACHE)
    return cacheQuads;
#elif defined(MODE_FONT)
    return TextLength(FONT_SAMPLE);
#elif defined(MODE_OVERSIZE)
    return 1;
#else
    return textGlyphs + TILES_X*TILES_Y + ((SCENE_COUNT < 500)? SCENE_COUNT : 500) + 200*3;
#endif
}

static int sceneRuns(void)
{
#if defined(MODE_TEXT) || defined(MODE_TILES) || defined(MODE_PARTICLES) || \
    defined(MODE_SHAPES) || defined(MODE_FONT) || defined(MODE_OVERSIZE) || defined(MODE_SPRITES) || \
    defined(MODE_CACHE)
    return 1;
#else
    return 4;
#endif
}

static void sceneInit(void)
{
#if defined(MODE_TEXT)
    textInit();
#elif defined(MODE_TILES)
    tilesInit();
#elif defined(MODE_PARTICLES)
    particlesInit();
#elif defined(MODE_SHAPES)
    // No assets; the default white texture is the only texture.
#elif defined(MODE_FONT)
    fontInit();
#elif defined(MODE_OVERSIZE)
    oversizeInit();
#elif defined(MODE_SPRITES)
    spritesInit();
#elif defined(MODE_CACHE)
    cacheInit();
#else
    textInit();
    tilesInit();
    particlesInit();
    if (particleCount > 500) particleCount = 500;   // mixed is a short combined scene
#endif
}

static void sceneUpdate(void)
{
#if defined(MODE_PARTICLES)
    particlesUpdate();
#elif defined(MODE_TEXT) || defined(MODE_TILES) || defined(MODE_SHAPES) || defined(MODE_FONT) || defined(MODE_OVERSIZE) || defined(MODE_SPRITES) || defined(MODE_CACHE)
    // stationary
#else
    particlesUpdate();
#endif

#if defined(MODE_TILES)
    #ifdef SCROLL
        if (gFrame > 0)
        {
            tileOffsetX = (gFrame/60) % (MAP_W - TILES_X);
            tileOffsetY = (gFrame/60) % (MAP_H - TILES_Y);
        }
    #endif
#endif
}

static void sceneDraw(void)
{
#if defined(MODE_TEXT)
    textDraw();
#elif defined(MODE_TILES)
    tilesDraw();
#elif defined(MODE_PARTICLES)
    particlesDraw();
#elif defined(MODE_SHAPES)
    shapesDraw();
#elif defined(MODE_FONT)
    fontDraw();
#elif defined(MODE_OVERSIZE)
    oversizeDraw();
#elif defined(MODE_SPRITES)
    spritesDraw();
#elif defined(MODE_CACHE)
    cacheDraw();
#else
    tilesDraw();
    particlesDraw();
    textDraw();
    for (int i = 0; i < 200; i++)
    {
        unsigned int h = hashIndex((unsigned int)i*3u);
        unsigned int h2 = hashIndex((unsigned int)i*3u + 1u);
        DrawRectangle((int)(h % (SCREEN_W - 40)), (int)(h2 % (SCREEN_H - 40)),
                      2 + (int)(h%20), 2 + (int)(h2%20),
                      (Color){ (unsigned char)(40 + h%200), (unsigned char)(40 + h2%200), 90, 255 });
    }
    for (int i = 0; i < 200; i++)
    {
        unsigned int h = hashIndex((unsigned int)i*7u + 3u);
        unsigned int h2 = hashIndex((unsigned int)i*7u + 4u);
        DrawLine((int)(h % SCREEN_W), (int)(h2 % SCREEN_H),
                 (int)((h*3u) % SCREEN_W), (int)((h2*5u) % SCREEN_H),
                 (Color){ (unsigned char)(40 + h%200), 200, (unsigned char)(40 + h2%200), 255 });
    }
#endif
}

static const char *sceneName(void)
{
#if defined(MODE_TEXT)
    return "text";
#elif defined(MODE_TILES)
    return "tiles";
#elif defined(MODE_PARTICLES)
    return "particles";
#elif defined(MODE_SHAPES)
    return "shapes";
#elif defined(MODE_FONT)
    return "font";
#elif defined(MODE_OVERSIZE)
    return "oversize";
#elif defined(MODE_SPRITES)
    return "sprites";
#elif defined(MODE_CACHE)
    return "cache";
#else
    return "mixed";
#endif
}

int main(void)
{
    int frames = SCENE_FRAMES;
    if (frames > MAX_FRAMES) frames = MAX_FRAMES;

    InitWindow(SCREEN_W, SCREEN_H, "scene_2d");
    SetTargetFPS(TARGET_FPS);

    sceneInit();

    PspGuResidencyStats resBefore = { 0 };
    PspGuResidencyStats resAfter = { 0 };

    PspCountersReset();
    pspguVramReport("boot2d");
    pspguResidencyStats(&resBefore);

    PspGuVramStats vram = { 0 };
    pspguVramStats(&vram);

    int budgetOk = (vram.free >= (unsigned int)BUDGET_FLOOR_KB*1024u) &&
                   (vram.largest >= 32u*1024u);

    TraceLog(LOG_INFO, "SCENE mode=%s objects=%i runs=%i frames=%i count=%i lines=%i offset=%i,%i",
             sceneName(), sceneObjects(), sceneRuns(), frames, SCENE_COUNT, SCENE_LINES,
             tileOffsetX, tileOffsetY);
    TraceLog(LOG_INFO, "BUDGET free=%u largest=%u floor=%u verdict=%s",
             vram.free, vram.largest, (unsigned int)BUDGET_FLOOR_KB*1024u,
             budgetOk? "OK" : "FAIL");

    double sum = 0.0;
    double simSum = 0.0;

    for (int frame = 0; frame < frames; frame++)
    {
        gFrame = frame;

        double t0 = GetTime();
        sceneUpdate();
        simSum += GetTime() - t0;

        BeginDrawing();
            ClearBackground(BLACK);
            sceneDraw();
        EndDrawing();

        frameTimes[frame] = GetFrameTime();
        sum += frameTimes[frame];
    }

    PspCountersDump("done", frames);
    pspguResidencyStats(&resAfter);

    // Snapshot the measured-frame totals; the three freeze frames below must not leak
    // into the marker.
    unsigned int cBegins = pspCounters.begins;
    unsigned int cEnds = pspCounters.ends;
    unsigned int cDraws = pspCounters.drawArrays + pspCounters.drawElements;
    unsigned int cBinds = pspCounters.binds;
    unsigned int cUnbinds = pspCounters.unbinds;
    unsigned int cVerts = pspCounters.arrayVertices;
    unsigned int winDraws = 0, winSwitches = 0;
    pspguWindowStats(&winDraws, &winSwitches);

    int residencyMoved = (resAfter.evictions != resBefore.evictions) ||
                         (resAfter.reloads != resBefore.reloads) ||
                         (resAfter.failed != resBefore.failed);

    qsort(frameTimes, frames, sizeof(float), cmpFloat);

    float median = frameTimes[frames/2];
    float p95 = frameTimes[(int)(frames*0.95f) < frames? (int)(frames*0.95f) : frames - 1];
    float worst = frameTimes[frames - 1];
    float best = frameTimes[0];
    double avg = sum/frames;
    double swapAvg = (PspCountersSwapCalls() > 0)?
                     (PspCountersSwapSeconds()/PspCountersSwapCalls())*1000.0 : 0.0;
    double workAvg = avg*1000.0 - swapAvg;
    double simAvg = (simSum/frames)*1000.0;

    TraceLog(LOG_INFO, "STAT n=%i min=%.3f median=%.3f p95=%.3f worst=%.3f avg=%.3f",
             frames, best*1000.0f, median*1000.0f, p95*1000.0f, worst*1000.0f, (float)avg*1000.0f);
    TraceLog(LOG_INFO, "SPLIT n=%i work=%.3f swap=%.3f swapcalls=%u", frames, workAvg, swapAvg,
             PspCountersSwapCalls());
    TraceLog(LOG_INFO, "SIM n=%i sim=%.3f work=%.3f", frames, simAvg, workAvg);
    TraceLog(LOG_INFO, "RESIDENCY [measured]: evictions=%u reloads=%u failed=%u moved=%i",
             resAfter.evictions, resAfter.reloads, resAfter.failed, residencyMoved);

    // Three frozen frames: the last measured state is what the capture must show, and
    // the queued display lists need more than one frame to present it.
    for (int i = 0; i < 3; i++)
    {
        BeginDrawing();
            ClearBackground(BLACK);
            sceneDraw();
        EndDrawing();
    }

    const char *verdict = (budgetOk && !residencyMoved)? "PASS" : "FAIL";

    char sceneExtra[240] = "";
#if defined(MODE_FONT)
    snprintf(sceneExtra, sizeof(sceneExtra),
             " fontsize=%d fontglyphs=%d tex=%dx%d fallback=%d mismatches=%d tex2=%dx%d mismatches2=%d",
             FONT_SIZE, probeFont.glyphCount, probeFont.texture.width, probeFont.texture.height,
             probeFallback, probeMismatches, probeFont2.texture.width, probeFont2.texture.height,
             probeMismatches2);
#elif defined(MODE_OVERSIZE)
    snprintf(sceneExtra, sizeof(sceneExtra), " oversize=%dx%d ref=%d rotate=%d",
             OVERSIZE_W, OVERSIZE_H,
#if defined(OVERSIZE_REF)
             1,
#else
             0,
#endif
             OVERSIZE_ROTATE);
#elif defined(MODE_TILES) && defined(ATLAS_WIDE)
    snprintf(sceneExtra, sizeof(sceneExtra), " atlas=1024x128 ref=%d",
#if defined(ATLAS_REF)
             1);
#else
             0);
#endif
#elif defined(MODE_SPRITES)
    snprintf(sceneExtra, sizeof(sceneExtra), " sprites total=%d visible=%d offscreen_pct=%d",
             SCENE_COUNT, spriteVisible, SCENE_OFFSCREEN_PCT);
#elif defined(MODE_CACHE)
    snprintf(sceneExtra, sizeof(sceneExtra), " cache strip=%d repeat=%d quads=%d",
             SCENE_STRIP, SCENE_REPEAT, cacheQuads);
#endif

    SaveFileText(SCENE_MARKER,
                 TextFormat("DONE mode=%s objects=%i runs=%i frames=%i begins=%u ends=%u draws=%u binds=%u unbinds=%u verts=%u windraws=%u winswitch=%u work=%.3f swap=%.3f sim=%.3f median=%.3f p95=%.3f free=%u largest=%u evict=%u reload=%u failed=%u verdict=%s%s\n",
                            sceneName(), sceneObjects(), sceneRuns(), frames,
                            cBegins, cEnds, cDraws, cBinds, cUnbinds, cVerts,
                            winDraws, winSwitches,
                            workAvg, swapAvg, simAvg, median*1000.0f, p95*1000.0f,
                            vram.free, vram.largest,
                            resAfter.evictions, resAfter.reloads, resAfter.failed,
                            verdict, sceneExtra));

    TraceLog(LOG_INFO, "DONE scene=%s objects=%i runs=%i frames=%i %s", sceneName(),
             sceneObjects(), sceneRuns(), frames, verdict);
    sceKernelSleepThread();

    return 0;
}
