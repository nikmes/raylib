/*
 * Deterministic render check - sprint 007 T7.
 *
 * The sprint's batch skeleton must produce the same picture as the immediate-mode
 * path. bunnymark cannot prove that: it seeds positions from GetRandomValue() and
 * animates them, so two runs of the same build already differ. This draws a fixed
 * scene instead - a grid of textured sprites, one untextured rectangle and one text
 * string, covering the three paths a 2D frame takes (textured quad, texture-less
 * quad, font atlas quad) - twice, then stops.
 *
 * Build for either arm of PSP_BATCH and compare the captures byte for byte:
 *   PSP_BATCH=0 build -> run -> scrshot -> a.raw
 *   PSP_BATCH=1 build -> run -> scrshot -> b.raw
 * Identical captures = the batch is a pure re-submission of the same geometry.
 */

#include <pspkernel.h>
#include <stdio.h>

#include "raylib.h"
#include "psp_counters.h"

PSP_MODULE_INFO("batch_check", 0, 1, 1);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);

#define FRAMES 2                    // Two frames: the second one is the settled capture

int main(int argc, char **argv)
{
    InitWindow(480, 272, "batch_check");
    SetTargetFPS(0);

    Texture2D tex = LoadTexture("host0:/textures/bunnymark/wabbit_alpha.png");
    TraceLog(LOG_INFO, "TEXID wabbit=%u shapes=%u font=%u", (unsigned)tex.id,
             (unsigned)GetShapesTexture().id, (unsigned)GetFontDefault().texture.id);

    PspCountersReset();

    for (int frame = 0; frame < FRAMES; frame++)
    {
        BeginDrawing();
            ClearBackground(RAYWHITE);

            // Textured sprites on a fixed grid: 8 x 5, 48 px apart
            for (int y = 0; y < 5; y++)
            {
                for (int x = 0; x < 8; x++)
                {
                    Rectangle dst = { 16.0f + x*56.0f, 48.0f + y*40.0f, 32.0f, 32.0f };
                    Rectangle src = { 0.0f, 0.0f, (float)tex.width, (float)tex.height };
                    Color tint = { (unsigned char)(30 + x*28), (unsigned char)(40 + y*40), 200, 255 };
                    DrawTexturePro(tex, src, dst, (Vector2){ 0.0f, 0.0f }, 0.0f, tint);
                }
            }

            // Texture-less quad: the batch has to split here
            DrawRectangle(16, 16, 448, 24, BLACK);

            // Font atlas: a different texture again
            DrawText("batch check", 24, 20, 14, GREEN);

        EndDrawing();
    }

    PspCountersDump("done", FRAMES);
    TraceLog(LOG_INFO, "DONE frames %i", FRAMES);

    // Stay resident so the host keeps control and the framebuffer keeps the last frame
    sceKernelSleepThread();

    return 0;
}
