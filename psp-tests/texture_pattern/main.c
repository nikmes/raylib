/*
 * texture_pattern - sprint 012 T1: one capture that separates "wrong texel" from
 * "wrong alpha" from "wrong blend".
 *
 * Three hypothesis-driven runs failed to explain why pspgu's sprites differ from pspGL's.
 * This draws textures whose contents are known, so a wrong texel is a wrong COLOUR in a
 * known place rather than a vague impression:
 *
 *   1. a 4x4 texture, four 2x2 quadrants: red, green, blue, and fully transparent,
 *      drawn 16x at (40,40)  -> texel order, corner mapping and alpha in one sprite
 *   2. a 33x17 checkerboard (not a power of two) drawn 3x at (240,40)
 *                          -> the padded stride path: wrong tbw shows as diagonal shear
 *   3. a 32x32 half-red/half-transparent sprite at (40,140)
 *                          -> alpha per texel, no tint, straight 1:1
 *
 * Everything is drawn twice and the app sleeps, so a capture after DONE shows a settled
 * frame. Compare the two drivers' captures pixel by pixel.
 */

#include <pspkernel.h>
#include <stdio.h>

#include "raylib.h"

PSP_MODULE_INFO("texture_pattern", 0, 1, 1);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);

#define FRAMES 2

static Texture2D makeQuadrantTexture(void)
{
    Image img = GenImageColor(4, 4, BLANK);
    Color *px = (Color *)img.data;              // RGBA8888: one Color per pixel

    for (int y = 0; y < 4; y++)
    {
        for (int x = 0; x < 4; x++)
        {
            Color c;
            if ((x < 2) && (y < 2))      c = (Color){ 255,   0,   0, 255 };   // TL red
            else if ((x >= 2) && (y < 2)) c = (Color){   0, 255,   0, 255 };   // TR green
            else if ((x < 2) && (y >= 2)) c = (Color){   0,   0, 255, 255 };   // BL blue
            else                          c = (Color){   0,   0,   0,   0 };   // BR transparent
            px[y*4 + x] = c;
        }
    }

    Texture2D t = LoadTextureFromImage(img);
    UnloadImage(img);

    return t;
}

static Texture2D makeCheckerTexture(void)
{
    // 33x17: not a power of two, so the padded stride path is exercised
    Image img = GenImageColor(33, 17, BLANK);
    Color *px = (Color *)img.data;

    for (int y = 0; y < 17; y++)
    {
        for (int x = 0; x < 33; x++)
        {
            px[y*33 + x] = ((x + y) & 1)? (Color){ 255, 255,   0, 255 }
                                        : (Color){   0, 128, 255, 255 };
        }
    }

    Texture2D t = LoadTextureFromImage(img);
    UnloadImage(img);

    return t;
}

static Texture2D makeHalfAlphaTexture(void)
{
    Image img = GenImageColor(32, 32, BLANK);
    Color *px = (Color *)img.data;

    for (int y = 0; y < 32; y++)
    {
        for (int x = 0; x < 32; x++)
        {
            px[y*32 + x] = (x < 16)? (Color){ 255, 255, 255, 255 }
                                   : (Color){ 255,   0,   0, 128 };   // half alpha on the right
        }
    }

    Texture2D t = LoadTextureFromImage(img);
    UnloadImage(img);

    return t;
}

int main(int argc, char **argv)
{
    InitWindow(480, 272, "texture_pattern");
    SetTargetFPS(0);

    Texture2D quadrants = makeQuadrantTexture();
    Texture2D checker = makeCheckerTexture();
    Texture2D halfAlpha = makeHalfAlphaTexture();

    TraceLog(LOG_INFO, "PATTERN textures: quadrant=%u checker=%u half=%u", quadrants.id, checker.id, halfAlpha.id);
#ifdef PATTERN_NPOT
    TraceLog(LOG_INFO, "PATTERN NPOT=1: 33x17 checkerboard IS in the scene");
#endif

    for (int frame = 0; frame < FRAMES; frame++)
    {
        BeginDrawing();
            ClearBackground((Color){ 32, 32, 32, 255 });        // dark grey: stands out from every quadrant

            // 1. four quadrants, 16x
            DrawTexturePro(quadrants, (Rectangle){ 0, 0, 4, 4 }, (Rectangle){ 40, 30, 64, 64 },
                           (Vector2){ 0, 0 }, 0.0f, WHITE);

#ifdef PATTERN_NPOT
            // 2. non-power-of-two checkerboard, 3x. OFF by default: pspGL HANGS on this
            // texture (see the sprint log), so it cannot be part of a comparison scene.
            // Our driver may well handle it - that would be a capability pspGL lacks, but
            // it has to be measured on its own, not mixed into the parity test.
            DrawTexturePro(checker, (Rectangle){ 0, 0, 33, 17 }, (Rectangle){ 160, 30, 99, 51 },
                           (Vector2){ 0, 0 }, 0.0f, WHITE);
#endif

            // 3. alpha per texel, 1:1
            DrawTexturePro(halfAlpha, (Rectangle){ 0, 0, 32, 32 }, (Rectangle){ 320, 30, 32, 32 },
                           (Vector2){ 0, 0 }, 0.0f, WHITE);

        EndDrawing();
    }

    TraceLog(LOG_INFO, "DONE frames %i", FRAMES);
    sceKernelSleepThread();

    return 0;
}
