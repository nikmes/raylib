/*******************************************************************************************
*
*   raylib [shapes] example - following eyes
*
*   Example complexity rating: [★★☆☆] 2/4
*
*   Example originally created with raylib 2.5, last time updated with raylib 2.5
*
*   Example licensed under an unmodified zlib/libpng license, which is an OSI-certified,
*   BSD-like license that allows static linking with closed source software
*
*   Copyright (c) 2013-2025 Ramon Santamaria (@raysan5)
*
********************************************************************************************/

#include "raylib.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "psp_input.h"


#include <math.h>       // Required for: atan2f()

//------------------------------------------------------------------------------------
// Program main entry point
//------------------------------------------------------------------------------------
void sample_shapes_following_eyes(void)
{
    // Initialization
    //--------------------------------------------------------------------------------------
    const int screenWidth = 800;
    const int screenHeight = 450;
    Vector2 scleraLeftPosition = { GetScreenWidth()/2.0f - 100.0f, GetScreenHeight()/2.0f };
    Vector2 scleraRightPosition = { GetScreenWidth()/2.0f + 100.0f, GetScreenHeight()/2.0f };
    float scleraRadius = 80;

    Vector2 irisLeftPosition = { GetScreenWidth()/2.0f - 100.0f, GetScreenHeight()/2.0f };
    Vector2 irisRightPosition = { GetScreenWidth()/2.0f + 100.0f, GetScreenHeight()/2.0f };
    float irisRadius = 24;

    float angle = 0.0f;
    float dx = 0.0f, dy = 0.0f, dxx = 0.0f, dyy = 0.0f;

    SetTargetFPS(60);               // Set our game to run at 60 frames-per-second
    //--------------------------------------------------------------------------------------

    // Main game loop
    for (int frame = 0; !IsGamepadButtonPressed(0, GAMEPAD_BUTTON_MIDDLE_RIGHT) && !WindowShouldClose(); frame++)
    {
        pspFrame = frame;
        // Update
        //----------------------------------------------------------------------------------
        irisLeftPosition = GetMousePosition();
        irisRightPosition = GetMousePosition();

        // Check not inside the left eye sclera
        if (!CheckCollisionPointCircle(irisLeftPosition, scleraLeftPosition, scleraRadius - irisRadius))
        {
            dx = irisLeftPosition.x - scleraLeftPosition.x;
            dy = irisLeftPosition.y - scleraLeftPosition.y;

            angle = atan2f(dy, dx);

            dxx = (scleraRadius - irisRadius)*cosf(angle);
            dyy = (scleraRadius - irisRadius)*sinf(angle);

            irisLeftPosition.x = scleraLeftPosition.x + dxx;
            irisLeftPosition.y = scleraLeftPosition.y + dyy;
        }

        // Check not inside the right eye sclera
        if (!CheckCollisionPointCircle(irisRightPosition, scleraRightPosition, scleraRadius - irisRadius))
        {
            dx = irisRightPosition.x - scleraRightPosition.x;
            dy = irisRightPosition.y - scleraRightPosition.y;

            angle = atan2f(dy, dx);

            dxx = (scleraRadius - irisRadius)*cosf(angle);
            dyy = (scleraRadius - irisRadius)*sinf(angle);

            irisRightPosition.x = scleraRightPosition.x + dxx;
            irisRightPosition.y = scleraRightPosition.y + dyy;
        }
        //----------------------------------------------------------------------------------

        // Draw
        //----------------------------------------------------------------------------------
        BeginDrawing();

            BeginMode2D(PspSceneCamera());

            ClearBackground(RAYWHITE);

            DrawCircleV(scleraLeftPosition, scleraRadius, LIGHTGRAY);
            DrawCircleV(irisLeftPosition, irisRadius, BROWN);
            DrawCircleV(irisLeftPosition, 10, BLACK);

            DrawCircleV(scleraRightPosition, scleraRadius, LIGHTGRAY);
            DrawCircleV(irisRightPosition, irisRadius, DARKGREEN);
            DrawCircleV(irisRightPosition, 10, BLACK);

            DrawFPS(10, 10);

                    EndMode2D();
            DrawText("shapes_following_eyes", 8, 8, 10, DARKGRAY);

        EndDrawing();
        //----------------------------------------------------------------------------------
    }

    // De-Initialization
    //--------------------------------------------------------------------------------------
    //--------------------------------------------------------------------------------------

    return;
}