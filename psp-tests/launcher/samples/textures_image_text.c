/*******************************************************************************************
*
*   raylib [textures] example - image text
*
*   Example complexity rating: [★★☆☆] 2/4
*
*   Example originally created with raylib 1.8, last time updated with raylib 4.0
*
*   Example licensed under an unmodified zlib/libpng license, which is an OSI-certified,
*   BSD-like license that allows static linking with closed source software
*
*   Copyright (c) 2017-2025 Ramon Santamaria (@raysan5)
*
********************************************************************************************/

#include "raylib.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>



//------------------------------------------------------------------------------------
// Program main entry point
//------------------------------------------------------------------------------------
void sample_textures_image_text(void)
{
    // Initialization
    //--------------------------------------------------------------------------------------
    const int screenWidth = 480;
    const int screenHeight = 272;
    Image parrots = LoadImage("resources/parrots.png"); // Load image in CPU memory (RAM)

    // When no codepoints are provided, LoadFontEx() loads a default set of codepoints
    // This set includes '@', which "KAISG.ttf" doesn't have a glyph for, causing a warning to be logged
    // We avoid this by just excluding it from the list of codepoints we load
    int codepointCount = 0;
    int *codepoints = LoadCodepoints(" !\"#$%&'()*+,-./0123456789:;<=>?ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~", &codepointCount);
    // TTF Font loading with custom generation parameters
    Font font = LoadFontEx("resources/KAISG.ttf", 20, codepoints, codepointCount);
    UnloadCodepoints(codepoints);

    // Draw over image using custom font
    ImageDrawTextEx(&parrots, font, "[Parrots font drawing]", (Vector2){ 20.0f, 20.0f }, (float)font.baseSize, 0.0f, RED);

    Texture2D texture = LoadTextureFromImage(parrots);  // Image converted to texture, uploaded to GPU memory (VRAM)
    UnloadImage(parrots);   // Once image has been converted to texture and uploaded to VRAM, it can be unloaded from RAM

    bool showFont = false;

    SetTargetFPS(60);
    //--------------------------------------------------------------------------------------

    // Main game loop
    for (int frames = 0; !IsGamepadButtonPressed(0, GAMEPAD_BUTTON_MIDDLE_RIGHT); frames++)    // Detect window close button or ESC key
    {
        // Update
        //----------------------------------------------------------------------------------
        if (IsKeyDown(KEY_SPACE)) showFont = true;
        else showFont = false;
        //----------------------------------------------------------------------------------

        // Draw
        //----------------------------------------------------------------------------------
        BeginDrawing();

            ClearBackground(RAYWHITE);

            if (!showFont)
            {
                DrawTextureEx(texture, (Vector2){ 0, 0 }, 0.0f, 0.5f, WHITE);
                DrawTextEx(font, "[Parrots font drawing]", (Vector2){ 20, 220 }, 20.0f, 0.0f, WHITE);
            }
            else
            {
                // Make the font atlas texture fit the screen, so we can see the whole thing
                float scale = 1.0f;

                float atlasRatio = (float)font.texture.width/(float)font.texture.height;
                float screenRatio = (float)screenWidth/(float)screenHeight;
                if (atlasRatio >= screenRatio) scale = (float)screenWidth/(float)font.texture.width;
                else scale = (float)screenHeight/(float)font.texture.height;

                float width = (float)font.texture.width * scale;
                float height = (float)font.texture.height * scale;

                DrawTextureEx(font.texture, (Vector2){ ((float)screenWidth - width)/2, ((float)screenHeight - height)/2 }, 0, scale, BLACK);
            }

            DrawText("PRESS SPACE to SHOW FONT ATLAS USED", 10, screenHeight - 18, 10, DARKGRAY);

        EndDrawing();


        if (((frames + 1)%60) == 0) TraceLog(LOG_INFO, "FRAME %i fps %i", frames + 1, GetFPS());
        //----------------------------------------------------------------------------------
    }

    // De-Initialization
    //--------------------------------------------------------------------------------------
    UnloadTexture(texture);     // Texture unloading

    UnloadFont(font);           // Unload custom font

    return;
}