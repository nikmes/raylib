/*******************************************************************************************
*
*   raylib [textures] example - blend modes
*
*   Example complexity rating: [★☆☆☆] 1/4
*
*   NOTE: Images are loaded in CPU memory (RAM); textures are loaded in GPU memory (VRAM)
*
*   Example originally created with raylib 3.5, last time updated with raylib 3.5
*
*   Example contributed by Karlo Licudine (@accidentalrebel) and reviewed by Ramon Santamaria (@raysan5)
*
*   Example licensed under an unmodified zlib/libpng license, which is an OSI-certified,
*   BSD-like license that allows static linking with closed source software
*
*   Copyright (c) 2020-2025 Karlo Licudine (@accidentalrebel)
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
void sample_textures_blend_modes(void)
{
    // Initialization
    //--------------------------------------------------------------------------------------
    const int screenWidth = 480;
    const int screenHeight = 272;
    // NOTE: Textures MUST be loaded after Window initialization (OpenGL context is required)
    Image imBackground = LoadImage("resources/cyberpunk_street_background.png"); // Loaded in CPU memory (RAM)
    Texture2D texBackground = LoadTextureFromImage(imBackground); // Image converted to texture, GPU memory (VRAM)

    Image imForeground = LoadImage("resources/cyberpunk_street_foreground.png"); // Loaded in CPU memory (RAM)
    Texture2D texForeground = LoadTextureFromImage(imForeground); // Image converted to texture, GPU memory (VRAM)

    // Once image has been converted to texture and uploaded to VRAM, it can be unloaded from RAM
    UnloadImage(imBackground);
    UnloadImage(imForeground);

    const int blendCountMax = 4;
    BlendMode blendMode = 0;

    SetTargetFPS(60);               // Set our game to run at 60 frames-per-second
    //---------------------------------------------------------------------------------------

    // Main game loop
    for (int frames = 0; !IsGamepadButtonPressed(0, GAMEPAD_BUTTON_MIDDLE_RIGHT) && !WindowShouldClose(); frames++)    // Detect window close button or ESC key
    {
        // Update
        //----------------------------------------------------------------------------------
        if (IsKeyPressed(KEY_SPACE))
        {
            if (blendMode >= (blendCountMax - 1)) blendMode = 0;
            else blendMode++;
        }
        //----------------------------------------------------------------------------------

        // Draw
        //----------------------------------------------------------------------------------
        BeginDrawing();

            ClearBackground(RAYWHITE);

            DrawTexture(texBackground, screenWidth/2 - texBackground.width/2, screenHeight/2 - texBackground.height/2, WHITE);

            // Apply the blend mode and then draw the foreground texture
            BeginBlendMode(blendMode);
                DrawTexture(texForeground, screenWidth/2 - texForeground.width/2, screenHeight/2 - texForeground.height/2, WHITE);
            EndBlendMode();

            // Draw the texts
            DrawText("Press SPACE to change blend modes.", 10, 230, 10, GRAY);

            switch (blendMode)
            {
                case BLEND_ALPHA: DrawText("Current: BLEND_ALPHA", (screenWidth/2) - 60, 244, 10, GRAY); break;
                case BLEND_ADDITIVE: DrawText("Current: BLEND_ADDITIVE", (screenWidth/2) - 60, 244, 10, GRAY); break;
                case BLEND_MULTIPLIED: DrawText("Current: BLEND_MULTIPLIED", (screenWidth/2) - 60, 244, 10, GRAY); break;
                case BLEND_ADD_COLORS: DrawText("Current: BLEND_ADD_COLORS", (screenWidth/2) - 60, 244, 10, GRAY); break;
                default: break;
            }

            DrawText("(c) Cyberpunk Street Environment by Luis Zuno (@ansimuz)", 10, screenHeight - 18, 10, GRAY);

        EndDrawing();


        if (((frames + 1)%60) == 0) TraceLog(LOG_INFO, "FRAME %i fps %i", frames + 1, GetFPS());
        //----------------------------------------------------------------------------------
    }

    // De-Initialization
    //--------------------------------------------------------------------------------------
    UnloadTexture(texForeground); // Unload foreground texture
    UnloadTexture(texBackground); // Unload background texture

    return;
}
