/*
 * Deterministic render check - sprint 007 T7, extended in 008 for the sprite path.
 *
 * bunnymark cannot prove correctness: it seeds positions from GetRandomValue() and
 * animates them, so two runs of the same build already differ. This draws a fixed
 * scene instead and then stops:
 *
 *   - an 8x4 grid of textured sprites at integer positions   (sprite-eligible)
 *   - a row at fractional positions                          (sprite-eligible, but the
 *     rasterisation is where a 2-vertex sprite and two triangles could disagree)
 *   - one rotated sprite                                     (must fall back to triangles)
 *   - one untextured rectangle and one string                (shapes + font atlas)
 *
 * Build for a given arm, run, capture after DONE, compare the captures:
 *   sprint 007 batched (triangles) vs sprint 008 batched (sprites) must match exactly.
 *   Unbatched vs batched must match too - that pair was proven in 007.
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
    Rectangle src = { 0.0f, 0.0f, (float)tex.width, (float)tex.height };

    PspCountersReset();

    for (int frame = 0; frame < FRAMES; frame++)
    {
        BeginDrawing();
            ClearBackground(RAYWHITE);

            // Integer grid: 8 x 4
            for (int y = 0; y < 4; y++)
            {
                for (int x = 0; x < 8; x++)
                {
                    Rectangle dst = { 16.0f + x*56.0f, 48.0f + y*40.0f, 32.0f, 32.0f };
                    Color tint = { (unsigned char)(30 + x*28), (unsigned char)(40 + y*40), 200, 255 };
                    DrawTexturePro(tex, src, dst, (Vector2){ 0.0f, 0.0f }, 0.0f, tint);
                }
            }

            // Fractional positions: still axis-aligned, but the edges land inside a pixel.
            // A 2-vertex sprite and two triangles do not have to round those the same way.
            for (int x = 0; x < 8; x++)
            {
                Rectangle dst = { 18.5f + x*55.75f, 210.25f, 32.0f, 32.0f };
                Color tint = { 200, (unsigned char)(60 + x*20), 90, 255 };
                DrawTexturePro(tex, src, dst, (Vector2){ 0.0f, 0.0f }, 0.0f, tint);
            }

            // Rotated sprite: not sprite-eligible, must take the triangle path
            DrawTexturePro(tex, src, (Rectangle){ 430.0f, 20.0f, 32.0f, 32.0f },
                           (Vector2){ 16.0f, 16.0f }, 30.0f, WHITE);

            // Texture-less quad and font atlas: two more textures in the frame
            DrawRectangle(16, 16, 400, 24, BLACK);
            DrawText("batch check", 24, 20, 14, GREEN);

        EndDrawing();
    }

    PspCountersDump("done", FRAMES);
    TraceLog(LOG_INFO, "DONE frames %i", FRAMES);

    // Stay resident so the host keeps control and the framebuffer keeps the last frame
    sceKernelSleepThread();

    return 0;
}
