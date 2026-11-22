/*******************************************************************************************
*
*   raylib [shapes] example - outlines thickness
*
*   Example complexity rating: [★☆☆☆] 1/4
*
*   Example originally created with raylib 6.1, last time updated with raylib 6.1
*
*   Example contributed by Matthew Roush (@MatthewRoush) and reviewed by Ramon Santamaria (@raysan5)
*
*   Example licensed under an unmodified zlib/libpng license, which is an OSI-certified,
*   BSD-like license that allows static linking with closed source software
*
*   Copyright (c) 2026 Matthew Roush (@MatthewRoush)
*
********************************************************************************************/

#include "raylib.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "raygui.h"         // Required for GUI controls

//------------------------------------------------------------------------------------
// Program main entry point
//------------------------------------------------------------------------------------
void sample_shapes_outlines_thickness(void)
{
    // Initialization
    //--------------------------------------------------------------------------------------
    const int screenWidth = 480;
    const int screenHeight = 272;
    float thick = 5.0f;

    SetTargetFPS(60);
    //--------------------------------------------------------------------------------------

    // Main game loop
    for (int frames = 0; !IsGamepadButtonPressed(0, GAMEPAD_BUTTON_MIDDLE_RIGHT); frames++)    // Detect window close button or ESC key
    {
        // Update
        //----------------------------------------------------------------------------------
        // TODO: Update variables / Implement example logic at this point
        //----------------------------------------------------------------------------------

        // Draw
        //----------------------------------------------------------------------------------
        BeginDrawing();

            ClearBackground(RAYWHITE);

            GuiSliderBar((Rectangle){ 130, 15, 220, 24 }, "Thickness", TextFormat("%.2f", thick), &thick, -30.0f, 30.0f);

            DrawRectangle(10, 90, 140, 140, LIGHTGRAY);
            DrawRectangleLinesEx((Rectangle){ 10, 90, 140, 140 }, thick, BLUE);
            DrawText("RectangleLinesEx", 10, 75, 10, BLACK);

            DrawRectangleRounded((Rectangle){ 170, 90, 140, 140 }, 0.2f, 9, LIGHTGRAY);
            DrawRectangleRoundedLinesEx((Rectangle){ 170, 90, 140, 140 }, 0.2f, 9, thick, BLUE);
            DrawText("RoundedLinesEx", 170, 75, 10, BLACK);

            DrawCircle(410, 160, 55, LIGHTGRAY);
            DrawCircleLinesEx((Vector2){ 410, 160 }, 55, thick, BLUE);
            DrawText("CircleLinesEx", 365, 75, 10, BLACK);

        EndDrawing();


        if (((frames + 1)%60) == 0) TraceLog(LOG_INFO, "FRAME %i fps %i", frames + 1, GetFPS());
        //----------------------------------------------------------------------------------
    }

    // De-Initialization
    //--------------------------------------------------------------------------------------

    return;
}
