/*
 * bunnymark with counters - sprint 003.
 *
 * Modelled on raylib4Psp/samples/textures/bunnymark (one DrawTexture per bunny,
 * which is the workload phases 3-5 are measured against), with three changes so
 * it can be driven entirely from the host:
 *
 *   - BUNNY_COUNT is a build parameter, spawned at startup. The corpus sample
 *     needs a CROSS press per 100 bunnies.
 *   - the loop is bounded; it logs DONE and sleeps instead of calling
 *     CloseWindow(), because sceKernelExitGame ends the PSPLink session.
 *   - counters are dumped periodically, and once more at DONE.
 */

#include <pspkernel.h>
#include <stdlib.h>

#include "raylib.h"
#include "psp_counters.h"

PSP_MODULE_INFO("bunnymark", 0, 1, 1);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);

#ifndef BUNNY_COUNT
    #define BUNNY_COUNT 1000
#endif

#ifndef TOTAL_FRAMES
    #define TOTAL_FRAMES 600        // ~20 s at 30 fps, ~60 s at the 10 fps 1000 bunnies cost
#endif

#define MAX_BUNNIES        50000

typedef struct Bunny {
    Vector2 position;
    Vector2 speed;
    Color color;
} Bunny;

int main(void)
{
    InitWindow(480, 272, "bunnymark [counters]");
    SetTargetFPS(60);

    int count = BUNNY_COUNT;
    if (count > MAX_BUNNIES) count = MAX_BUNNIES;

    Bunny *bunnies = (Bunny *)malloc(count*sizeof(Bunny));
    if (bunnies == NULL)
    {
        TraceLog(LOG_ERROR, "SPAWN: could not allocate %i bunnies", count);
        sceKernelSleepThread();
    }

    // Spawn at startup, spread over the play area, and never respawn: the same
    // workload every frame makes the counters comparable between runs.
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

    PspCountersReset();
    TraceLog(LOG_INFO, "SPAWN %i bunnies, %i frames, texture %ix%i", count, TOTAL_FRAMES,
             texBunny.width, texBunny.height);

    double frameTimeSum = 0.0;

    for (int frame = 1; frame <= TOTAL_FRAMES; frame++)
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
            // Mutation for T5: with SKIP_DRAW the same frame is drawn without the
            // bunnies, so submissions must collapse while the logical count does not.
            for (int i = 0; i < count; i++)
            {
                DrawTexture(texBunny, (int)bunnies[i].position.x, (int)bunnies[i].position.y,
                            bunnies[i].color);
            }
#endif

            DrawRectangle(0, 0, GetScreenWidth(), 40, BLACK);
            DrawText(TextFormat("bunnies: %i", count), 120, 15, 10, GREEN);
            DrawText("counters in log", 200, 15, 10, MAROON);
            DrawFPS(10, 10);
        EndDrawing();

        frameTimeSum += GetFrameTime();

        if ((frame % 60) == 0)
        {
            TraceLog(LOG_INFO, "FRAME %i fps %i", frame, GetFPS());
            PspCountersDump("frame", frame);
        }
    }

    PspCountersDump("done", TOTAL_FRAMES);
    TraceLog(LOG_INFO, "AVG frames=%i avgms=%.3f fps=%.1f", TOTAL_FRAMES,
             (frameTimeSum/TOTAL_FRAMES)*1000.0, TOTAL_FRAMES/frameTimeSum);
    TraceLog(LOG_INFO, "DONE frames %i  bunnies %i", TOTAL_FRAMES, count);
    sceKernelSleepThread();

    return 0;
}
