/*******************************************************************************************
*
*   psp_2d_showcase - a 2D stress scene written for this port, not a ported sample.
*
*   About 2,300 additive sprites (a nebula of glowing particles plus a star field) over a
*   banded backdrop, with a ring of a second texture in the middle so the frame exercises
*   texture runs as well as vertex throughput, and a 2D HUD on top.
*
*   It is meant as a benchmark next to the 3D showcase: the animation is driven by the
*   FRAME INDEX, not by wall time, so frame N draws exactly the same scene on every run and
*   on every build. Compare STAT/SPLIT between builds and the counters between drivers.
*
*   Build:
*       make                        runs until power-off
*       make FRAMES=900             stops, prints statistics, sleeps (capture build)
*       make SPRITES=3000 STARS=0   sweep the load
*
*   Controls: none.
*
********************************************************************************************/

#include <pspkernel.h>
#include <stdio.h>
#include <math.h>

#include "raylib.h"
#include "rlgl.h"
#include "psp_counters.h"

PSP_MODULE_INFO("psp_2d_showcase", 0, 1, 1);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);

#ifndef SHOWCASE_FRAMES
    #define SHOWCASE_FRAMES 0
#endif
#ifndef SPRITES
    #define SPRITES 1300
#endif
#ifndef STARS
    #define STARS 250
#endif

#define GLOW_SIZE 96

// Deterministic pseudo-random in [0,1): the scene must not depend on libc rand().
static unsigned int seed = 0x12345678u;
static float frand(void)
{
    seed = seed*1664525u + 1013904223u;
    return (float)((seed >> 8) & 0xFFFFFF)/16777216.0f;
}

// A soft radial glow: alpha falls off quadratically, colour stays white so the sprite
// tint decides the hue. Additive blending turns overlaps into light.
static Image genGlow(int size)
{
    Image img = GenImageColor(size, size, BLANK);

    for (int y = 0; y < size; y++)
    {
        for (int x = 0; x < size; x++)
        {
            float dx = (x + 0.5f)/size*2.0f - 1.0f;
            float dy = (y + 0.5f)/size*2.0f - 1.0f;
            float d = sqrtf(dx*dx + dy*dy);
            float a = (d >= 1.0f)? 0.0f : (1.0f - d)*(1.0f - d);

            ImageDrawPixel(&img, x, y, (Color){ 255, 255, 255, (unsigned char)(a*255.0f) });
        }
    }

    return img;
}

// A thin ring, for the second texture run.
static Image genRing(int size)
{
    Image img = GenImageColor(size, size, BLANK);

    for (int y = 0; y < size; y++)
    {
        for (int x = 0; x < size; x++)
        {
            float dx = (x + 0.5f)/size*2.0f - 1.0f;
            float dy = (y + 0.5f)/size*2.0f - 1.0f;
            float d = sqrtf(dx*dx + dy*dy);
            float a = 1.0f - fabsf(d - 0.82f)*9.0f;

            ImageDrawPixel(&img, x, y, (Color){ 255, 255, 255, (unsigned char)((a < 0.0f)? 0.0f : a*255.0f) });
        }
    }

    return img;
}

int main(void)
{
    InitWindow(480, 272, "psp 2d showcase");
    SetTargetFPS(0);

    Image glowImg = genGlow(GLOW_SIZE);
    Texture2D glow = LoadTextureFromImage(glowImg);
    UnloadImage(glowImg);

    Image ringImg = genRing(128);
    Texture2D ring = LoadTextureFromImage(ringImg);
    UnloadImage(ringImg);

    // Per-particle setup, fixed once: position on a spiral, speed, size, hue.
    static float px[SPRITES], py[SPRITES], pr[SPRITES], pa[SPRITES], ps[SPRITES];
    static unsigned char pc[SPRITES][3];

    for (int i = 0; i < SPRITES; i++)
    {
        float u = frand();
        float arm = (float)(i % 3)*2.0944f;             // three arms
        float radius = 10.0f + 215.0f*u*u;

        pr[i] = radius;
        pa[i] = arm + u*2.2f + frand()*0.25f;
        ps[i] = 3.0f + 12.0f*frand();
        // Cyan core to magenta rim, then a little per-particle jitter.
        float t = u;
        pc[i][0] = (unsigned char)(40.0f + 200.0f*t + 20.0f*frand());
        pc[i][1] = (unsigned char)(210.0f - 150.0f*t + 20.0f*frand());
        pc[i][2] = (unsigned char)(255.0f - 60.0f*t);
    }

    // Star field, also fixed.
    static float sx[STARS], sy[STARS], ss[STARS];
    for (int i = 0; i < STARS; i++)
    {
        sx[i] = frand()*480.0f;
        sy[i] = frand()*272.0f;
        ss[i] = 1.0f + 3.0f*frand()*frand();
    }

    int frames = 0;
    double sum = 0.0;

    for (;;)
    {
        // Frame-indexed phase: same scene at frame N on every run and every build.
        float t = (float)frames/60.0f;

        BeginDrawing();
            ClearBackground((Color){ 4, 6, 14, 255 });

            // Backdrop: an opaque vertical gradient in 40 bands, one untextured run.
            for (int i = 0; i < 40; i++)
            {
                float k = (float)i/39.0f;
                DrawRectangle(0, i*7, 480, 7,
                              (Color){ (unsigned char)(4 + 22*k), (unsigned char)(6 + 10*k),
                                       (unsigned char)(16 + 34*k), 255 });
            }

            // Stars: dim additive dots, slow drift.
            BeginBlendMode(BLEND_ADDITIVE);
                for (int i = 0; i < STARS; i++)
                {
                    float x = sx[i] + sinf(t*0.12f + i*0.7f)*6.0f;
                    float y = sy[i] + cosf(t*0.09f + i*0.3f)*4.0f;
                    float s = ss[i];
                    DrawTextureEx(glow, (Vector2){ x - s*0.5f, y - s*0.5f }, 0.0f, s/GLOW_SIZE,
                                  (Color){ 170, 195, 255, 150 });
                }

                // Nebula: three rotating arms of glowing particles.
                for (int i = 0; i < SPRITES; i++)
                {
                    float wobble = sinf(t*0.9f + i*0.13f);
                    float a = pa[i] + t*0.55f*(1.0f + 12.0f/pr[i]);
                    float r = pr[i] + 10.0f*wobble;
                    float s = ps[i]*(0.85f + 0.15f*wobble);

                    px[i] = 240.0f + cosf(a)*r;
                    py[i] = 136.0f + sinf(a)*r*0.62f;

                    DrawTextureEx(glow, (Vector2){ px[i] - s*0.5f, py[i] - s*0.5f }, 0.0f, s/GLOW_SIZE,
                                  (Color){ pc[i][0], pc[i][1], pc[i][2], 255 });
                }

                // Hot core: 160 small, near-white particles hugging the centre.
                for (int i = 0; i < 160; i++)
                {
                    float a = t*1.7f + (float)i*0.3927f + sinf((float)i)*0.5f;
                    float r = 6.0f + 26.0f*fabsf(sinf((float)i*0.37f));
                    float s = 3.0f + 5.0f*fabsf(cosf((float)i*0.21f));
                    DrawTextureEx(glow, (Vector2){ 240.0f + cosf(a)*r - s*0.5f,
                                                   136.0f + sinf(a)*r*0.8f - s*0.5f }, 0.0f, s/GLOW_SIZE,
                                  (Color){ 210, 240, 255, 255 });
                }

                // A second texture in the middle: 16 rings, and one extra texture run.
                for (int i = 0; i < 16; i++)
                {
                    float a = t*0.8f + (float)i*0.3927f;
                    float r = 40.0f + 8.0f*sinf(t*2.0f + i);
                    float s = 46.0f;
                    DrawTextureEx(ring, (Vector2){ 240.0f + cosf(a)*r - s*0.5f,
                                                   136.0f + sinf(a)*r - s*0.5f }, 0.0f, s/128.0f,
                                  (Color){ 120, 220, 255, 200 });
                }
            EndBlendMode();

            // HUD: untextured plate, then text (one more texture run).
            DrawRectangle(8, 8, 244, 56, (Color){ 2, 4, 10, 190 });
            DrawRectangleLines(8, 8, 244, 56, (Color){ 70, 120, 190, 255 });
            DrawText("PSP-1000   pspgu   2D stress", 14, 14, 10, (Color){ 150, 190, 240, 255 });
            DrawText(TextFormat("%d sprites  %d draws  %d fps", SPRITES + STARS + 176,
                                (int)pspCounters.drawArrays, GetFPS()), 14, 28, 10, RAYWHITE);
            DrawText(TextFormat("%d binds", (int)pspCounters.binds), 14, 42, 10, (Color){ 120, 160, 210, 255 });

        EndDrawing();

        frames++;
        sum += GetFrameTime();

#if SHOWCASE_FRAMES > 0
        if (frames >= SHOWCASE_FRAMES) break;
#endif
    }

    double avg = (frames > 0)? (sum/frames)*1000.0 : 0.0;

    TraceLog(LOG_INFO, "STAT n=%d avg=%.3f fps=%.1f", frames, avg, (avg > 0.0)? 1000.0/avg : 0.0);
    PspCountersDump("SHOWCASE", frames);
    TraceLog(LOG_INFO, "DONE frames %d sprites %d", frames, SPRITES + STARS + 16);
    sceKernelSleepThread();

    return 0;
}
