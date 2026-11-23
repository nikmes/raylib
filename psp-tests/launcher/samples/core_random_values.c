/*******************************************************************************************
*
*   raylib [core] example - random values
*
*   Example complexity rating: [★☆☆☆] 1/4
*
*   Example originally created with raylib 1.1, last time updated with raylib 1.1
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



//------------------------------------------------------------------------------------
// Program main entry point
//------------------------------------------------------------------------------------
void sample_core_random_values(void)
{
    // Initialization
    //--------------------------------------------------------------------------------------
    const int screenWidth = 480;
    const int screenHeight = 272;
    // SetRandomSeed(0xaabbccff);   // Set a custom random seed if desired, by default: "time(NULL)"

    int randValue = GetRandomValue(-8, 5);   // Get a random integer number between -8 and 5 (both included)

    unsigned int framesCounter = 0; // Variable used to count frames

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

            // Every two seconds (120 frames) a new random value is generated
            if (((framesCounter/120)%2) == 1)
            {
                randValue = GetRandomValue(-8, 5);
                framesCounter = 0;
            }
        }
        //----------------------------------------------------------------------------------

        // Draw
        //----------------------------------------------------------------------------------
        BeginDrawing();

            ClearBackground(RAYWHITE);

            const char *message = "Every 2 seconds a new random value is generated:";
            DrawText(message, (screenWidth - MeasureText(message, 10))/2, 80, 10, MAROON);

            const char *value = TextFormat("%i", randValue);
            DrawText(value, (screenWidth - MeasureText(value, 80))/2, 140, 80, LIGHTGRAY);

        EndDrawing();


        if (((frames + 1)%60) == 0) TraceLog(LOG_INFO, "FRAME %i fps %i", frames + 1, GetFPS());
        //----------------------------------------------------------------------------------
    }

    // De-Initialization
    //--------------------------------------------------------------------------------------

    return;
}