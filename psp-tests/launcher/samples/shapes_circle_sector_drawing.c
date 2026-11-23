/*******************************************************************************************
*
*   raylib [shapes] example - circle sector drawing
*
*   Example complexity rating: [★★★☆] 3/4
*
*   Example originally created with raylib 2.5, last time updated with raylib 2.5
*
*   Example contributed by Vlad Adrian (@demizdor) and reviewed by Ramon Santamaria (@raysan5)
*
*   Example licensed under an unmodified zlib/libpng license, which is an OSI-certified,
*   BSD-like license that allows static linking with closed source software
*
*   Copyright (c) 2018-2025 Vlad Adrian (@demizdor) and Ramon Santamaria (@raysan5)
*
********************************************************************************************/

#include "raylib.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "raygui.h"                 // Required for GUI controls

//------------------------------------------------------------------------------------
// Program main entry point
//------------------------------------------------------------------------------------
void sample_shapes_circle_sector_drawing(void)
{
    // Initialization
    //--------------------------------------------------------------------------------------
    const int screenWidth = 480;
    const int screenHeight = 272;
    Vector2 center = { 140.0f, GetScreenHeight()/2.0f };

    float outerRadius = 110.0f;
    float startAngle = 0.0f;
    float endAngle = 180.0f;
    float segments = 10.0f;
    float minSegments = 4;

    SetTargetFPS(60);               // Set our game to run at 60 frames-per-second
    //--------------------------------------------------------------------------------------

    // Main game loop
    for (int frames = 0; !IsGamepadButtonPressed(0, GAMEPAD_BUTTON_MIDDLE_RIGHT) && !WindowShouldClose(); frames++)    // Detect window close button or ESC key
    {
        // Update
        //----------------------------------------------------------------------------------
        // NOTE: All variables update happens inside GUI control functions
        //----------------------------------------------------------------------------------

        // Draw
        //----------------------------------------------------------------------------------
        BeginDrawing();

            ClearBackground(RAYWHITE);

            DrawLine(280, 0, 280, GetScreenHeight(), Fade(LIGHTGRAY, 0.6f));
            DrawRectangle(280, 0, GetScreenWidth() - 280, GetScreenHeight(), Fade(LIGHTGRAY, 0.3f));

            DrawCircleSector(center, outerRadius, startAngle, endAngle, (int)segments, Fade(MAROON, 0.3f));
            DrawCircleSectorLines(center, outerRadius, startAngle, endAngle, (int)segments, Fade(MAROON, 0.6f));

            // Draw GUI controls
            //------------------------------------------------------------------------------
            GuiSliderBar((Rectangle){ 300, 30, 160, 20}, "Start", TextFormat("%.2f", startAngle), &startAngle, 0, 720);
            GuiSliderBar((Rectangle){ 300, 60, 160, 20}, "End", TextFormat("%.2f", endAngle), &endAngle, 0, 720);

            GuiSliderBar((Rectangle){ 300, 100, 160, 20}, "Radius", TextFormat("%.2f", outerRadius), &outerRadius, 0, 120);
            GuiSliderBar((Rectangle){ 300, 130, 160, 20}, "Segments", TextFormat("%.2f", segments), &segments, 0, 100);
            //------------------------------------------------------------------------------

            minSegments = truncf(ceilf((endAngle - startAngle)/90));
            DrawText(TextFormat("MODE: %s", (segments >= minSegments)? "MANUAL" : "AUTO"), 300, 165, 10, (segments >= minSegments)? MAROON : DARKGRAY);

            DrawFPS(10, 10);

        EndDrawing();


        if (((frames + 1)%60) == 0) TraceLog(LOG_INFO, "FRAME %i fps %i", frames + 1, GetFPS());
        //----------------------------------------------------------------------------------
    }

    // De-Initialization
    //--------------------------------------------------------------------------------------

    return;
}
