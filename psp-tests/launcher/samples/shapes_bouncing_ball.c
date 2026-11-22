/*******************************************************************************************
*
*   raylib [shapes] example - bouncing ball
*
*   Example complexity rating: [★☆☆☆] 1/4
*
*   Example originally created with raylib 2.5, last time updated with raylib 2.5
*
*   Example contributed by Ramon Santamaria (@raysan5), reviewed by Jopestpe (@jopestpe)
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


//------------------------------------------------------------------------------------
// Program main entry point
//------------------------------------------------------------------------------------
void sample_shapes_bouncing_ball(void)
{
    // Initialization
    //---------------------------------------------------------
    const int screenWidth = 800;
    const int screenHeight = 450;
    Vector2 ballPosition = { GetScreenWidth()/2.0f, GetScreenHeight()/2.0f };
    Vector2 ballSpeed = { 5.0f, 4.0f };
    int ballRadius = 20;
    float gravity = 0.2f;

    bool useGravity = true;
    bool pause = 0;
    int framesCounter = 0;

    SetTargetFPS(60);               // Set our game to run at 60 frames-per-second
    //----------------------------------------------------------

    // Main game loop
    for (int frame = 0; !IsGamepadButtonPressed(0, GAMEPAD_BUTTON_MIDDLE_RIGHT); frame++)
    {
        pspFrame = frame;
        // Update
        //-----------------------------------------------------
        if (IsKeyPressed(KEY_G)) useGravity = !useGravity;
        if (IsKeyPressed(KEY_SPACE)) pause = !pause;

        if (!pause)
        {
            ballPosition.x += ballSpeed.x;
            ballPosition.y += ballSpeed.y;

            if (useGravity) ballSpeed.y += gravity;

            // Check walls collision for bouncing
            if ((ballPosition.x >= (GetScreenWidth() - ballRadius)) || (ballPosition.x <= ballRadius)) ballSpeed.x *= -1.0f;
            if ((ballPosition.y >= (GetScreenHeight() - ballRadius)) || (ballPosition.y <= ballRadius)) ballSpeed.y *= -0.95f;
        }
        else framesCounter++;
        //-----------------------------------------------------

        // Draw
        //-----------------------------------------------------
        BeginDrawing();

            BeginMode2D(PspSceneCamera());

            ClearBackground(RAYWHITE);

            DrawCircleV(ballPosition, (float)ballRadius, MAROON);
            DrawText("PRESS SPACE to PAUSE BALL MOVEMENT", 10, GetScreenHeight() - 25, 20, LIGHTGRAY);

            if (useGravity) DrawText("GRAVITY: ON (Press G to disable)", 10, GetScreenHeight() - 50, 20, DARKGREEN);
            else DrawText("GRAVITY: OFF (Press G to enable)", 10, GetScreenHeight() - 50, 20, RED);

            // On pause, we draw a blinking message
            if (pause && ((framesCounter/30)%2)) DrawText("PAUSED", 350, 200, 30, GRAY);

            DrawFPS(10, 10);

                    EndMode2D();
            DrawText("shapes_bouncing_ball", 8, 8, 10, DARKGRAY);

        EndDrawing();
        //-----------------------------------------------------
    }

    // De-Initialization
    //---------------------------------------------------------
    //----------------------------------------------------------

    return;
}