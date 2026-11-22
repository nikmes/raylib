/*******************************************************************************************
*
*   raylib [textures] example - image rotate
*
*   Example complexity rating: [★★☆☆] 2/4
*
*   Example originally created with raylib 1.0, last time updated with raylib 1.0
*
*   Example licensed under an unmodified zlib/libpng license, which is an OSI-certified,
*   BSD-like license that allows static linking with closed source software
*
*   Copyright (c) 2014-2025 Ramon Santamaria (@raysan5)
*
********************************************************************************************/

#include "raylib.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>



#define NUM_TEXTURES  3

//------------------------------------------------------------------------------------
// Program main entry point
//------------------------------------------------------------------------------------
void sample_textures_image_rotate(void)
{
    // Initialization
    //--------------------------------------------------------------------------------------
    const int screenWidth = 480;
    const int screenHeight = 272;
    // NOTE: Textures MUST be loaded after Window initialization (OpenGL context is required)
    Image image45 = LoadImage("resources/raylib_logo.png");
    Image image90 = LoadImage("resources/raylib_logo.png");
    Image imageNeg90 = LoadImage("resources/raylib_logo.png");

    ImageRotate(&image45, 45);
    ImageRotate(&image90, 90);
    ImageRotate(&imageNeg90, -90);

    Texture2D textures[NUM_TEXTURES] = { 0 };

    textures[0] = LoadTextureFromImage(image45);
    textures[1] = LoadTextureFromImage(image90);
    textures[2] = LoadTextureFromImage(imageNeg90);

    UnloadImage(image45);
    UnloadImage(image90);
    UnloadImage(imageNeg90);

    int currentTexture = 0;

    SetTargetFPS(60);               // Set our game to run at 60 frames-per-second
    //---------------------------------------------------------------------------------------

    // Main game loop
    for (int frames = 0; !IsGamepadButtonPressed(0, GAMEPAD_BUTTON_MIDDLE_RIGHT); frames++)    // Detect window close button or ESC key
    {
        // Update
        //----------------------------------------------------------------------------------
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) || IsKeyPressed(KEY_RIGHT))
        {
            currentTexture = (currentTexture + 1)%NUM_TEXTURES; // Cycle between the textures
        }
        //----------------------------------------------------------------------------------

        // Draw
        //----------------------------------------------------------------------------------
        BeginDrawing();

            ClearBackground(RAYWHITE);

            const float scale = 0.7f;
            DrawTextureEx(textures[currentTexture],
                          (Vector2){ (screenWidth - textures[currentTexture].width*scale)/2,
                                     (screenHeight - textures[currentTexture].height*scale)/2 },
                          0.0f, scale, WHITE);

            const char *note = "Press LEFT MOUSE BUTTON to rotate the image clockwise";
            DrawText(note, (screenWidth - MeasureText(note, 10))/2, screenHeight - 20, 10, DARKGRAY);

        EndDrawing();


        if (((frames + 1)%60) == 0) TraceLog(LOG_INFO, "FRAME %i fps %i", frames + 1, GetFPS());
        //----------------------------------------------------------------------------------
    }

    // De-Initialization
    //--------------------------------------------------------------------------------------
    for (int i = 0; i < NUM_TEXTURES; i++) UnloadTexture(textures[i]);

    return;
}
