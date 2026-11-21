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
static Texture2D tileTex;
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

static void textDraw(void)
{
    Font font = GetFontDefault();

    for (int l = 0; l < SCENE_LINES; l++)
        DrawTextEx(font, textLines[l], (Vector2){ 4.0f, 4.0f + 10.0f*l }, 10.0f, 0.0f, RAYWHITE);
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
}

static void tilesDraw(void)
{
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

static void particlesDraw(void)
{
    Rectangle src = { 0.0f, 0.0f, 32.0f, 32.0f };

    for (int i = 0; i < particleCount; i++)
    {
        Rectangle dst = { particles[i].pos.x, particles[i].pos.y, 32.0f, 32.0f };
        DrawTexturePro(particleTex, src, dst, (Vector2){ 0.0f, 0.0f }, 0.0f, particles[i].color);
    }
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
// Scene dispatch
// ---------------------------------------------------------------------------------
static int sceneObjects(void)
{
#if defined(MODE_TEXT)
    return textGlyphs;
#elif defined(MODE_TILES)
    return TILES_X*TILES_Y;
#elif defined(MODE_PARTICLES)
    return particleCount;
#elif defined(MODE_SHAPES)
    return SCENE_COUNT*3;
#else
    return textGlyphs + TILES_X*TILES_Y + ((SCENE_COUNT < 500)? SCENE_COUNT : 500) + 200*3;
#endif
}

static int sceneRuns(void)
{
#if defined(MODE_TEXT) || defined(MODE_TILES) || defined(MODE_PARTICLES) || defined(MODE_SHAPES)
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
#elif defined(MODE_TEXT) || defined(MODE_TILES) || defined(MODE_SHAPES)
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
    SaveFileText("scene2d_done.txt",
                 TextFormat("DONE mode=%s objects=%i runs=%i frames=%i begins=%u ends=%u draws=%u binds=%u unbinds=%u verts=%u work=%.3f swap=%.3f sim=%.3f median=%.3f p95=%.3f free=%u largest=%u evict=%u reload=%u failed=%u verdict=%s\n",
                            sceneName(), sceneObjects(), sceneRuns(), frames,
                            cBegins, cEnds, cDraws, cBinds, cUnbinds, cVerts,
                            workAvg, swapAvg, simAvg, median*1000.0f, p95*1000.0f,
                            vram.free, vram.largest,
                            resAfter.evictions, resAfter.reloads, resAfter.failed,
                            verdict));

    TraceLog(LOG_INFO, "DONE scene=%s objects=%i runs=%i frames=%i %s", sceneName(),
             sceneObjects(), sceneRuns(), frames, verdict);
    sceKernelSleepThread();

    return 0;
}
