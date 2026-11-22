/*******************************************************************************************
*
*   bunnymark — sprite benchmark (PSP launcher harness)
*
*   A reduced form of psp-tests/bunnymark: the same 1,000-sprite workload and
*   per-frame update, without the counter/statistics plumbing. The asset is
*   generated so the harness needs no external image.
*
********************************************************************************************/

#include <pspkernel.h>
#include "raylib.h"
#include <stdlib.h>

PSP_MODULE_INFO("textures_bunnymark", 0, 1, 1);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);

#define BUNNY_COUNT 1000
#define MAX_BUNNIES 50000

typedef struct Bunny {
    Vector2 position;
    Vector2 speed;
    Color color;
} Bunny;

int main(void)
{
    const int screenWidth = 480;
    const int screenHeight = 272;

    InitWindow(screenWidth, screenHeight, "bunnymark [PSP]");

    Texture2D tex = LoadTexture("resources/wabbit_alpha.png");
    if (tex.id == 0)
    {
        Image img = GenImageColor(32, 32, MAROON);
        tex = LoadTextureFromImage(img);
        UnloadImage(img);
    }

    int count = BUNNY_COUNT;
    if (count > MAX_BUNNIES) count = MAX_BUNNIES;

    Bunny *bunnies = (Bunny *)malloc(count*sizeof(Bunny));
    if (bunnies == NULL) { TraceLog(LOG_ERROR, "SPAWN: no memory"); return 0; }

    for (int i = 0; i < count; i++)
    {
        bunnies[i].position.x = (float)GetRandomValue(16, screenWidth - 16);
        bunnies[i].position.y = (float)GetRandomValue(16, screenHeight - 16);
        bunnies[i].speed.x = (float)GetRandomValue(-250, 250)/60.0f;
        bunnies[i].speed.y = (float)GetRandomValue(-250, 250)/60.0f;
        bunnies[i].color = (Color){ GetRandomValue(50, 240), GetRandomValue(80, 240),
                                    GetRandomValue(100, 240), 255 };
    }

    Rectangle src = { 0.0f, 0.0f, (float)tex.width, (float)tex.height };

    SetTargetFPS(60);

    for (int frame = 0; frame < 600; frame++)
    {
        for (int i = 0; i < count; i++)
        {
            bunnies[i].position.x += bunnies[i].speed.x;
            bunnies[i].position.y += bunnies[i].speed.y;

            if ((bunnies[i].position.x > (screenWidth - 16)) ||
                (bunnies[i].position.x < 16)) bunnies[i].speed.x *= -1.0f;
            if ((bunnies[i].position.y > (screenHeight - 16)) ||
                (bunnies[i].position.y < 16)) bunnies[i].speed.y *= -1.0f;
        }

        BeginDrawing();

            ClearBackground(RAYWHITE);

            for (int i = 0; i < count; i++)
            {
                Rectangle dst = { bunnies[i].position.x, bunnies[i].position.y,
                                  (float)tex.width, (float)tex.height };
                DrawTexturePro(tex, src, dst, (Vector2){ 0.0f, 0.0f }, 0.0f, bunnies[i].color);
            }

            DrawText(TextFormat("bunnies: %i", count), 10, 10, 20, DARKGRAY);

        EndDrawing();

        if (((frame + 1)%60) == 0) TraceLog(LOG_INFO, "FRAME %i fps %i", frame + 1, GetFPS());
    }

    free(bunnies);
    UnloadTexture(tex);

    TraceLog(LOG_INFO, "DONE frames %i bunnies %i", 600, count);
    sceKernelSleepThread();

    return 0;
}
