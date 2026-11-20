/*
 * bunnymark with counters and timing - sprints 003/004.
 *
 * Modelled on raylib4Psp/samples/textures/bunnymark (one primitive per bunny,
 * the workload phases 3-5 are measured against), driven entirely from the host:
 *
 *   BUNNY_COUNT   bunnies spawned at startup (default 1000)
 *   FRAMES        frames before DONE + sleep (default 600)
 *   TARGET_FPS    SetTargetFPS() value; 0 = uncapped (default 60)
 *   SPRITE_SCALE  draw sprites at N x size to change fill without changing
 *                 submission count (default 1)
 *   SKIP_DRAW     compile out the sprite loop (mutation test)
 *
 * Reports, at DONE: counters at the GL boundary, per-frame statistics
 * (min/median/p95/worst) and the frame count actually measured.
 */

#include <pspkernel.h>
#include <stdlib.h>

#include "raylib.h"
#include "psp_counters.h"

#ifdef PSPGL_PROFILE_APP
    // Counters from inside pspGL: needs the fork built with PSPGL_PROFILE=1 and
    // -I pointing at its tree. Never part of the A/B - profiling changes cost.
    #include "pspgl_profile.h"
#endif

PSP_MODULE_INFO("bunnymark", 0, 1, 1);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);

#ifndef BUNNY_COUNT
    #define BUNNY_COUNT 1000
#endif
#ifndef TOTAL_FRAMES
    #define TOTAL_FRAMES 600
#endif
#ifndef TARGET_FPS
    #define TARGET_FPS 60
#endif
#ifndef SPRITE_SCALE
    #define SPRITE_SCALE 1
#endif

#define MAX_BUNNIES 50000
#define MAX_FRAMES  2000        // statistics are only valid up to this many frames

typedef struct Bunny {
    Vector2 position;
    Vector2 speed;
    Color color;
} Bunny;

static float frameTimes[MAX_FRAMES];

static int cmpFloat(const void *a, const void *b)
{
    float fa = *(const float *)a;
    float fb = *(const float *)b;
    return (fa > fb) - (fa < fb);
}

int main(void)
{
    InitWindow(480, 272, "bunnymark [counters]");
    SetTargetFPS(TARGET_FPS);

    int count = BUNNY_COUNT;
    if (count > MAX_BUNNIES) count = MAX_BUNNIES;

    int frames = TOTAL_FRAMES;
    if (frames > MAX_FRAMES) frames = MAX_FRAMES;

    Bunny *bunnies = (Bunny *)malloc(count*sizeof(Bunny));
    if (bunnies == NULL)
    {
        TraceLog(LOG_ERROR, "SPAWN: could not allocate %i bunnies", count);
        sceKernelSleepThread();
    }

    for (int i = 0; i < count; i++)
    {
        bunnies[i].position.x = (float)GetRandomValue(16, GetScreenWidth() - 16);
        bunnies[i].position.y = (float)GetRandomValue(56, GetScreenHeight() - 16);
        bunnies[i].speed.x = (float)GetRandomValue(-250, 250)/60.0f;
        bunnies[i].speed.y = (float)GetRandomValue(-250, 250)/60.0f;
        bunnies[i].color = (Color){ GetRandomValue(50, 240), GetRandomValue(80, 240),
                                    GetRandomValue(100, 240), 255 };
    }

    Texture2D texBunny = LoadTexture("host0:/textures/bunnymark/wabbit_alpha.png");
    Rectangle src = { 0.0f, 0.0f, (float)texBunny.width, (float)texBunny.height };

    PspCountersReset();
#ifdef PSPGL_PROFILE_APP
    pspgl_profile_reset();
#endif
    TraceLog(LOG_INFO, "SPAWN %i bunnies, %i frames, target_fps %i, scale %i, texture %ix%i",
             count, frames, TARGET_FPS, SPRITE_SCALE, texBunny.width, texBunny.height);

    double sum = 0.0;

    for (int frame = 0; frame < frames; frame++)
    {
        for (int i = 0; i < count; i++)
        {
            bunnies[i].position.x += bunnies[i].speed.x;
            bunnies[i].position.y += bunnies[i].speed.y;

            if (((bunnies[i].position.x + texBunny.width/2) > GetScreenWidth()) ||
                ((bunnies[i].position.x + texBunny.width/2) < 0)) bunnies[i].speed.x *= -1;
            if (((bunnies[i].position.y + texBunny.height/2) > GetScreenHeight()) ||
                ((bunnies[i].position.y + texBunny.height/2 - 40) < 0)) bunnies[i].speed.y *= -1;
        }

        BeginDrawing();
            ClearBackground(RAYWHITE);

#ifndef SKIP_DRAW
            // DrawTexturePro keeps this at one primitive per sprite, same as
            // DrawTexture, so SPRITE_SCALE changes fill only.
            for (int i = 0; i < count; i++)
            {
                Rectangle dst = { bunnies[i].position.x, bunnies[i].position.y,
                                  texBunny.width*(float)SPRITE_SCALE,
                                  texBunny.height*(float)SPRITE_SCALE };
                DrawTexturePro(texBunny, src, dst, (Vector2){ 0.0f, 0.0f }, 0.0f, bunnies[i].color);
            }
#endif

            DrawRectangle(0, 0, GetScreenWidth(), 40, BLACK);
            DrawText(TextFormat("bunnies: %i", count), 120, 15, 10, GREEN);
            DrawText(TextFormat("scale %ix", SPRITE_SCALE), 200, 15, 10, MAROON);
            DrawFPS(10, 10);
        EndDrawing();

        frameTimes[frame] = GetFrameTime();
        sum += frameTimes[frame];

        if (((frame + 1) % 60) == 0)
        {
            TraceLog(LOG_INFO, "FRAME %i fps %i", frame + 1, GetFPS());
            PspCountersDump("frame", frame + 1);
        }
    }

    // Statistics: a mean hides stalls, so report the distribution.
    qsort(frameTimes, frames, sizeof(float), cmpFloat);

    float median = frameTimes[frames/2];
    float p95 = frameTimes[(int)(frames*0.95f) < frames ? (int)(frames*0.95f) : frames - 1];
    float worst = frameTimes[frames - 1];
    float best = frameTimes[0];

    PspCountersDump("done", frames);
#ifdef PSPGL_PROFILE_APP
    struct pspgl_profile_stats ps;
    pspgl_profile_snapshot(&ps);
    TraceLog(LOG_INFO, "PSPGL draw_calls=%llu verts=%llu words=%llu subs=%llu rollovers=%llu "
             "cap_words=%llu wait_us=%llu max_wait_us=%llu binds=%llu bind_changes=%llu "
             "redundant=%llu flushes=%llu finishes=%llu queue_waits=%llu",
             ps.draw_calls, ps.vertices_submitted, ps.command_words, ps.command_list_submissions,
             ps.command_list_rollovers, ps.command_list_capacity_words,
             ps.command_list_reuse_sync_wait_us, ps.command_list_reuse_sync_wait_max_us,
             ps.texture_bind_calls, ps.texture_bind_changes, ps.texture_bind_redundant,
             ps.explicit_flushes, ps.finishes, ps.queue_waits);
#endif
    TraceLog(LOG_INFO, "STAT n=%i min=%.3f median=%.3f p95=%.3f worst=%.3f avg=%.3f",
             frames, best*1000.0f, median*1000.0f, p95*1000.0f, worst*1000.0f,
             (float)(sum/frames)*1000.0f);
    double swapAvg = (PspCountersSwapCalls() > 0)? (PspCountersSwapSeconds()/PspCountersSwapCalls())*1000.0 : 0.0;
    double workAvg = (float)(sum/frames)*1000.0 - swapAvg;
    TraceLog(LOG_INFO, "SPLIT n=%i work=%.3f swap=%.3f swapcalls=%u", frames, workAvg, swapAvg,
             PspCountersSwapCalls());
    TraceLog(LOG_INFO, "DONE frames %i  bunnies %i  scale %i", frames, count, SPRITE_SCALE);
    sceKernelSleepThread();

    return 0;
}
