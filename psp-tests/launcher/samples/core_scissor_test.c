/*******************************************************************************************
*
*   raylib [core] example - scissor test
*
*   Example complexity rating: [★☆☆☆] 1/4
*
*   Example originally created with raylib 2.5, last time updated with raylib 3.0
*
*   Example contributed by Chris Dill (@MysteriousSpace) and reviewed by Ramon Santamaria (@raysan5)
*
*   Example licensed under an unmodified zlib/libpng license, which is an OSI-certified,
*   BSD-like license that allows static linking with closed source software
*
*   Copyright (c) 2019-2025 Chris Dill (@MysteriousSpace)
*
********************************************************************************************/

#include "raylib.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "psp_input.h"


//------------------------------------------------------------------------------------
// Program main entry point
//------------------------------------------------------------------------------------
void sample_core_scissor_test(void)
{
    // Initialization
    //--------------------------------------------------------------------------------------
    const int screenWidth = 800;
    const int screenHeight = 450;
    Rectangle scissorArea = { 0, 0, 300, 300 };
    bool scissorMode = true;

    SetTargetFPS(60);               // Set our game to run at 60 frames-per-second
    //--------------------------------------------------------------------------------------

    // Main game loop
    for (int frame = 0; !IsGamepadButtonPressed(0, GAMEPAD_BUTTON_MIDDLE_RIGHT); frame++)
    {
        pspFrame = frame;
        // Update
        //----------------------------------------------------------------------------------
        if (IsKeyPressed(KEY_S)) scissorMode = !scissorMode;

        // Centre the scissor area around the mouse position
        scissorArea.x = GetMouseX() - scissorArea.width/2;
        scissorArea.y = GetMouseY() - scissorArea.height/2;
        //----------------------------------------------------------------------------------

        // Draw
        //----------------------------------------------------------------------------------
        BeginDrawing();

            BeginMode2D(PspSceneCamera());

            ClearBackground(RAYWHITE);

            if (scissorMode) BeginScissorMode((int)scissorArea.x, (int)scissorArea.y, (int)scissorArea.width, (int)scissorArea.height);

            // Draw full screen rectangle and some text
            // NOTE: Only part defined by scissor area will be rendered
            DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), RED);
            DrawText("Move the mouse around to reveal this text!", 190, 200, 20, LIGHTGRAY);

            if (scissorMode) EndScissorMode();

            DrawRectangleLinesEx(scissorArea, 1, BLACK);
            DrawText("Press S to toggle scissor test", 10, 10, 20, BLACK);

                    EndMode2D();
            DrawText("core_scissor_test", 8, 8, 10, DARKGRAY);

        EndDrawing();
        //----------------------------------------------------------------------------------
    }

    // De-Initialization
    //--------------------------------------------------------------------------------------
    //--------------------------------------------------------------------------------------

    return;
}
