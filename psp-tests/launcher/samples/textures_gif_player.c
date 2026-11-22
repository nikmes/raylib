/*******************************************************************************************
*
*   raylib [textures] example - gif player
*
*   Example complexity rating: [★★★☆] 3/4
*
*   Example originally created with raylib 4.2, last time updated with raylib 4.2
*
*   Example licensed under an unmodified zlib/libpng license, which is an OSI-certified,
*   BSD-like license that allows static linking with closed source software
*
*   Copyright (c) 2021-2025 Ramon Santamaria (@raysan5)
*
********************************************************************************************/

#include "raylib.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>



#define MAX_FRAME_DELAY     20
#define MIN_FRAME_DELAY      1

//------------------------------------------------------------------------------------
// Program main entry point
//------------------------------------------------------------------------------------
void sample_textures_gif_player(void)
{
    // Initialization
    //--------------------------------------------------------------------------------------
    const int screenWidth = 480;
    const int screenHeight = 272;
    int animFrames = 0;

    // Load all GIF animation frames into a single Image
    // NOTE: GIF data is always loaded as RGBA (32bit) by default
    // NOTE: Frames are just appended one after another in image.data memory
    Image imScarfyAnim = LoadImageAnim("resources/scarfy_run.gif", &animFrames);

    // Load texture from image
    // NOTE: We will update this texture when required with next frame data
    // WARNING: It's not recommended to use this technique for sprites animation,
    // use spritesheets instead, like illustrated in textures_sprite_anim example
    Texture2D texScarfyAnim = LoadTextureFromImage(imScarfyAnim);

    unsigned int nextFrameDataOffset = 0;  // Current byte offset to next frame in image.data

    int currentAnimFrame = 0;       // Current animation frame to load and draw
    int frameDelay = 8;             // Frame delay to switch between animation frames
    int frameCounter = 0;           // General frames counter

    SetTargetFPS(60);               // Set our game to run at 60 frames-per-second
    //--------------------------------------------------------------------------------------

    // Main game loop
    for (int frames = 0; !IsGamepadButtonPressed(0, GAMEPAD_BUTTON_MIDDLE_RIGHT); frames++)    // Detect window close button or ESC key
    {
        // Update
        //----------------------------------------------------------------------------------
        if (frames < 598)
        {
        frameCounter++;
        if (frameCounter >= frameDelay)
        {
            // Move to next frame
            // NOTE: If final frame is reached we return to first frame
            currentAnimFrame++;
            if (currentAnimFrame >= animFrames) currentAnimFrame = 0;

            // Get memory offset position for next frame data in image.data
            nextFrameDataOffset = imScarfyAnim.width*imScarfyAnim.height*4*currentAnimFrame;

            // Update GPU texture data with next frame image data
            // WARNING: Data size (frame size) and pixel format must match already created texture
            UpdateTexture(texScarfyAnim, ((unsigned char *)imScarfyAnim.data) + nextFrameDataOffset);

            frameCounter = 0;
        }

        }

        // Control frames delay
        if (IsKeyPressed(KEY_RIGHT)) frameDelay++;
        else if (IsKeyPressed(KEY_LEFT)) frameDelay--;

        if (frameDelay > MAX_FRAME_DELAY) frameDelay = MAX_FRAME_DELAY;
        else if (frameDelay < MIN_FRAME_DELAY) frameDelay = MIN_FRAME_DELAY;
        //----------------------------------------------------------------------------------

        // Draw
        //----------------------------------------------------------------------------------
        BeginDrawing();

            ClearBackground(RAYWHITE);

            DrawText(TextFormat("TOTAL GIF FRAMES:  %02i", animFrames), 10, 10, 10, LIGHTGRAY);
            DrawText(TextFormat("CURRENT FRAME: %02i", currentAnimFrame), 10, 24, 10, GRAY);
            DrawText(TextFormat("FRAME DATA OFFSET: %02i", nextFrameDataOffset), 10, 38, 10, GRAY);

            DrawTexture(texScarfyAnim, GetScreenWidth()/2 - texScarfyAnim.width/2, 70, WHITE);

            DrawText(TextFormat("FRAMES DELAY: %02i", frameDelay), 10, 215, 10, DARKGRAY);
            for (int i = 0; i < MAX_FRAME_DELAY; i++)
            {
                if (i < frameDelay) DrawRectangle(10 + 21*i, 230, 18, 18, RED);
                DrawRectangleLines(10 + 21*i, 230, 18, 18, MAROON);
            }

            DrawText("(c) Scarfy sprite by Eiden Marsal", 10, 255, 10, GRAY);

        EndDrawing();


        if (((frames + 1)%60) == 0) TraceLog(LOG_INFO, "FRAME %i fps %i", frames + 1, GetFPS());
        //----------------------------------------------------------------------------------
    }

    // De-Initialization
    //--------------------------------------------------------------------------------------
    UnloadTexture(texScarfyAnim);   // Unload texture
    UnloadImage(imScarfyAnim);      // Unload image (contains all frames)

    return;
}