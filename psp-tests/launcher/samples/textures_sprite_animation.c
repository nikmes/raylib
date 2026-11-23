/*******************************************************************************************
*
*   raylib [textures] example - sprite animation
*
*   Example complexity rating: [★★☆☆] 2/4
*
*   Example originally created with raylib 1.3, last time updated with raylib 1.3
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



#define MAX_FRAME_SPEED     15
#define MIN_FRAME_SPEED      1

//------------------------------------------------------------------------------------
// Program main entry point
//------------------------------------------------------------------------------------
void sample_textures_sprite_animation(void)
{
    // Initialization
    //--------------------------------------------------------------------------------------
    const int screenWidth = 480;
    const int screenHeight = 272;
    // NOTE: Textures MUST be loaded after Window initialization (OpenGL context is required)

    // Load spritesheet animation, 6 frames width
    Texture2D texture = LoadTexture("resources/scarfy.png");

    Vector2 position = { 176.0f, 80.0f };
    Rectangle frameRec = { 0.0f, 0.0f, (float)texture.width/6, (float)texture.height };
    int currentFrame = 0;

    int framesCounter = 0;
    int framesSpeed = 8;            // Number of spritesheet frames shown by second

    SetTargetFPS(60);               // Set our game to run at 60 frames-per-second
    //--------------------------------------------------------------------------------------

    // Main game loop
    for (int frames = 0; !IsGamepadButtonPressed(0, GAMEPAD_BUTTON_MIDDLE_RIGHT) && !WindowShouldClose(); frames++)    // Detect window close button or ESC key
    {
        // Update
        //----------------------------------------------------------------------------------
        if (frames < 598)
        {
        framesCounter++;

        if (framesCounter >= (60/framesSpeed))
        {
            framesCounter = 0;
            currentFrame++;

            if (currentFrame > 5) currentFrame = 0;

            frameRec.x = (float)currentFrame*(float)texture.width/6;
        }

        // Control frames speed
        if (IsKeyPressed(KEY_RIGHT)) framesSpeed++;
        else if (IsKeyPressed(KEY_LEFT)) framesSpeed--;

        if (framesSpeed > MAX_FRAME_SPEED) framesSpeed = MAX_FRAME_SPEED;
        else if (framesSpeed < MIN_FRAME_SPEED) framesSpeed = MIN_FRAME_SPEED;
        }
        //----------------------------------------------------------------------------------

        // Draw
        //----------------------------------------------------------------------------------
        BeginDrawing();

            ClearBackground(RAYWHITE);

            DrawTextureEx(texture, (Vector2){ 8, 8 }, 0.0f, 0.5f, WHITE);
            DrawRectangleLines(8, 8, texture.width/2, texture.height/2, LIME);
            DrawRectangleLines(8 + (int)(frameRec.x/2), 8, (int)(frameRec.width/2), (int)(frameRec.height/2), RED);

            DrawText("FRAME SPEED: ", 10, 215, 10, DARKGRAY);
            DrawText(TextFormat("%02i FPS", framesSpeed), 10, 228, 10, DARKGRAY);
            DrawText("PRESS RIGHT/LEFT KEYS to CHANGE SPEED!", 10, 250, 10, DARKGRAY);

            for (int i = 0; i < MAX_FRAME_SPEED; i++)
            {
                if (i < framesSpeed) DrawRectangle(10 + 11*i, 238, 10, 10, RED);
                DrawRectangleLines(10 + 11*i, 238, 10, 10, MAROON);
            }

            DrawTextureRec(texture, frameRec, position, WHITE);  // Draw part of the texture

            DrawText("(c) Scarfy spritesheet by Eiden Marsal", 10, 262, 10, GRAY);

        EndDrawing();


        if (((frames + 1)%60) == 0) TraceLog(LOG_INFO, "FRAME %i fps %i", frames + 1, GetFPS());
        //----------------------------------------------------------------------------------
    }

    // De-Initialization
    //--------------------------------------------------------------------------------------
    UnloadTexture(texture);       // Texture unloading

    return;
}