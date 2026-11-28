/**********************************************************************************************
*
*   psp_2d_bunnymark - the third benchmark: N textured sprites at a fixed, frame-indexed scene.
*
*   Why it exists: /home/psp/ref/PSPRuntime/optimal-rendering-path.md reports 7,000 bunnies at
*   59 fps on a PSP Street using one two-vertex GU_SPRITES pair per bunny, and 30 fps at the
*   same count with triangles. Our own 2D demo is an additive nebula (~2,300 large glow
*   sprites) and reads 33.8 ms, so it cannot say whether the difference is the primitive, the
*   fill, or our batcher. This demo is the matched scene: small, alpha-blended, 32x32 quads,
*   one texture, nothing else, so the count itself is the variable.
*
*   Arms (build-time):
*       make MODE=1            axis-aligned quads  -> the batcher's GU_SPRITES path (2 verts)
*       make MODE=3            quads rotated by 0.001 deg -> the 6-vertex triangle path
*       make SCALE=64          twice the linear size -> 4x the fill, same sprite count
*
*   Movement is frame-indexed (t = frames/60), so frame N is the same scene on every build and
*   a fixed count can be compared with the ceiling protocol (median/p95 against the period).
*
**********************************************************************************************/

#include "raylib.h"
#include "raymath.h"
#if (MODE == 5)
#include "rlgl.h"     // the seam itself: same rlpsp state (defined once in rcore_psp.c)
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <pspkernel.h>
#include <GL/gl.h>

#if defined(WRAP_COUNTERS)
    #include "../../../bunnymark/psp_counters.h"
#endif

PSP_MODULE_INFO("psp_2d_bunnymark", 0, 1, 1);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);

#ifndef COUNT
    #define COUNT 7000
#endif
#ifndef FRAMES
    #define FRAMES 200
#endif
#ifndef VSYNC
    #define VSYNC 1
#endif
#ifndef MODE
    #define MODE 1              // 1 = sprite quads, 3 = rotated quads (triangle path)
#endif
#ifndef SCALE
    #define SCALE 32.0f
#endif
#ifndef TEXSIZE
    #define TEXSIZE 32.0f       // the bunny asset is 32x32
#endif
#ifndef ALIGN
    #define ALIGN 64            // MODE 2 only: alignment of the vertex array
#endif
#define GL_SPRITES_PSP 0x000A   // pspgu's GL mode for GU_SPRITES

// MODE 2: the same two vertices per bunny, built once per frame in one array and submitted
// with a single glDrawArrays. No raylib call per sprite, no matrix, no per-quad assembly:
// this is the arm that says whether the cost is the API surface or the GE.
typedef struct {
    float u, v;
    unsigned char r, g, b, a;
    float x, y, z;
} DirectVertex;

static DirectVertex dverts[2*COUNT] __attribute__((aligned(ALIGN)));

static int cmpFloat(const void *a, const void *b)
{
    float x = *(const float *)a, y = *(const float *)b;

    return (x < y)? -1 : (x > y);
}

int main(void)
{
    static float bx[COUNT], by[COUNT], bvx[COUNT], bvy[COUNT];
    static unsigned char bc[COUNT][3];
    float times[FRAMES];
    double sum = 0.0;

    InitWindow(480, 272, "bunnymark");
    SetTargetFPS(VSYNC? 60 : 0);

    Texture2D tex = LoadTexture("host0:/textures/bunnymark/wabbit_alpha.png");

    // Deterministic initial state: no RNG, so every build and every run draws the same frame.
    for (int i = 0; i < COUNT; i++)
    {
        bx[i] = (float)((i*97) % 480);
        by[i] = (float)((i*53) % 272);
        bvx[i] = 20.0f + (float)((i*7) % 60);
        bvy[i] = 15.0f + (float)((i*11) % 55);
        bc[i][0] = (unsigned char)(120 + (i*13) % 120);
        bc[i][1] = (unsigned char)(120 + (i*29) % 120);
        bc[i][2] = (unsigned char)(120 + (i*17) % 120);
    }

    TraceLog(LOG_INFO, "BUNNY count=%d mode=%d scale=%.0f frames=%d tex=%d",
             COUNT, MODE, (double)SCALE, FRAMES, tex.id);

#if defined(WRAP_COUNTERS)
    PspCountersReset();
#endif

    for (int frame = 0; frame < FRAMES; frame++)
    {
        float t = (float)frame/60.0f;

        BeginDrawing();
            ClearBackground((Color){ 8, 10, 18, 255 });

#if (MODE == 2)
            for (int i = 0; i < COUNT; i++)
            {
                float x = fmodf(bx[i] + bvx[i]*t, 512.0f) - 16.0f;
                float y = fmodf(by[i] + bvy[i]*t, 304.0f) - 16.0f;
                DirectVertex *v = &dverts[2*i];

                v[0].u = 0.0f; v[0].v = 0.0f;
                v[0].r = bc[i][0]; v[0].g = bc[i][1]; v[0].b = bc[i][2]; v[0].a = 255;
                v[0].x = x; v[0].y = y; v[0].z = 0.0f;
                v[1].u = 1.0f; v[1].v = 1.0f;
                v[1].r = bc[i][0]; v[1].g = bc[i][1]; v[1].b = bc[i][2]; v[1].a = 255;
                v[1].x = x + SCALE; v[1].y = y + SCALE; v[1].z = 0.0f;
            }

            glEnable(GL_TEXTURE_2D);
            glBindTexture(GL_TEXTURE_2D, tex.id);
            glEnableClientState(GL_TEXTURE_COORD_ARRAY);
            glTexCoordPointer(2, GL_FLOAT, sizeof(DirectVertex), &dverts[0].u);
            glEnableClientState(GL_COLOR_ARRAY);
            glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(DirectVertex), &dverts[0].r);
            glEnableClientState(GL_VERTEX_ARRAY);
            glVertexPointer(3, GL_FLOAT, sizeof(DirectVertex), &dverts[0].x);
            glDrawArrays(GL_SPRITES_PSP, 0, 2*COUNT);
            glDisableClientState(GL_VERTEX_ARRAY);
            glDisableClientState(GL_COLOR_ARRAY);
            glDisableClientState(GL_TEXTURE_COORD_ARRAY);
            glDisable(GL_TEXTURE_2D);
#else
            for (int i = 0; i < COUNT; i++)
            {
                // Wrap in a 512x304 field so the motion stays on screen without collisions.
                float x = fmodf(bx[i] + bvx[i]*t, 512.0f) - 16.0f;
                float y = fmodf(by[i] + bvy[i]*t, 304.0f) - 16.0f;
                Color tint = { bc[i][0], bc[i][1], bc[i][2], 255 };

#if (MODE == 5)
                // The exact sequence DrawTexturePro emits, issued here so the seam runs without
                // raylib's per-sprite body (cull test, corner maths, source/dest handling).
                // Same state, same vertices: the difference from MODE 1 is raylib's own work.
                rlSetTexture(tex.id);
                rlBegin(RL_QUADS);
                rlColor4ub(bc[i][0], bc[i][1], bc[i][2], 255);
                rlTexCoord2f(0.0f, 0.0f);           rlVertex2f(x, y);
                rlTexCoord2f(0.0f, 1.0f);           rlVertex2f(x, y + SCALE);
                rlTexCoord2f(1.0f, 1.0f);           rlVertex2f(x + SCALE, y + SCALE);
                rlTexCoord2f(1.0f, 0.0f);           rlVertex2f(x + SCALE, y);
                rlEnd();
                rlSetTexture(0);
#elif (MODE == 0)
                // Untextured: the PSPRuntime doc's mode 0. If this is much faster than mode 1,
                // the cost is the texture fetch, not the primitive or the batcher.
                DrawRectanglePro((Rectangle){ x, y, SCALE, SCALE }, (Vector2){ 0, 0 }, 0.0f, tint);
#elif (MODE == 3)
                // A rotation no viewer can see: the corners stop being axis-aligned, so the
                // batcher emits six vertices instead of one sprite, at the same fill.
                DrawTexturePro(tex, (Rectangle){ 0, 0, TEXSIZE, TEXSIZE },
                               (Rectangle){ x, y, SCALE, SCALE }, (Vector2){ 0, 0 }, 0.001f, tint);
#else
                // DrawTexturePro, not DrawTextureEx: Ex pushes/translates/rotates/pops a matrix
                // per sprite, and in this driver each of those is GE list traffic. The reference
                // path appends two vertices and nothing else, so the arm has to as well.
                DrawTexturePro(tex, (Rectangle){ 0, 0, TEXSIZE, TEXSIZE },
                               (Rectangle){ x, y, SCALE, SCALE }, (Vector2){ 0, 0 }, 0.0f, tint);
#endif
            }
#endif

            // A panel behind the HUD: at 7,000 sprites the scene fills the strip and text
            // drawn straight onto it reads as noise, not as numbers.
            DrawRectangle(4, 4, 236, 16, (Color){ 0, 0, 0, 230 });
            DrawText(TextFormat("%d bunnies  mode %d  %d fps", COUNT, MODE, GetFPS()),
                     8, 8, 10, RAYWHITE);
        EndDrawing();

        times[frame] = (float)GetFrameTime();
        sum += times[frame];
    }

    qsort(times, FRAMES, sizeof(float), cmpFloat);

    float median = times[FRAMES/2];
    float p95 = times[(int)(FRAMES*0.95f) < FRAMES ? (int)(FRAMES*0.95f) : FRAMES - 1];
#if defined(WRAP_COUNTERS)
    double swapAvg = (PspCountersSwapCalls() > 0)? (PspCountersSwapSeconds()/PspCountersSwapCalls())*1000.0 : 0.0;
#else
    double swapAvg = 0.0;
#endif

    TraceLog(LOG_INFO, "STAT n=%d min=%.3f median=%.3f p95=%.3f worst=%.3f avg=%.3f",
             FRAMES, times[0]*1000.0f, median*1000.0f, p95*1000.0f, times[FRAMES - 1]*1000.0f,
             (float)(sum/FRAMES)*1000.0);
    TraceLog(LOG_INFO, "SPLIT n=%d work=%.3f swap=%.3f", FRAMES,
             (double)(sum/FRAMES)*1000.0 - swapAvg, swapAvg);
#if defined(WRAP_COUNTERS)
    PspCountersDump("BUNNY", FRAMES);
#endif
    TraceLog(LOG_INFO, "DONE count=%d mode=%d", COUNT, MODE);

    CloseWindow();
    sceKernelSleepThread();

    return 0;
}
