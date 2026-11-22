/*******************************************************************************************
*
*   raylib [shapes] example - easings ball
*
*   Example complexity rating: [★★☆☆] 2/4
*
*   Example originally created with raylib 2.5, last time updated with raylib 2.5
*
*   Example licensed under an unmodified zlib/libpng license, which is an OSI-certified,
*   BSD-like license that allows static linking with closed source software
*
*   Copyright (c) 2014-2025 Ramon Santamaria (@raysan5)
*
********************************************************************************************/

#include <pspkernel.h>
#include "raylib.h"
#include "psp_input.h"

PSP_MODULE_INFO("shapes_easings_ball", 0, 1, 1);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);

#include "reasings.h"                // Required for easing functions

//------------------------------------------------------------------------------------
// Program main entry point
//------------------------------------------------------------------------------------
int main(void)
{
    // Initialization
    //--------------------------------------------------------------------------------------
    const int screenWidth = 800;
    const int screenHeight = 450;

    InitWindow(480, 272, "raylib [shapes] example - easings ball");

    // Ball variable value to be animated with easings
    int ballPositionX = -100;
    int ballRadius = 20;
    float ballAlpha = 0.0f;

    int state = 0;
    int framesCounter = 0;

    SetTargetFPS(60);               // Set our game to run at 60 frames-per-second
    //--------------------------------------------------------------------------------------

    // Main game loop
    for (int frame = 0; frame < 600; frame++)
    {
        pspFrame = frame;
        // Update
        //----------------------------------------------------------------------------------
        if (state == 0)             // Move ball position X with easing
        {
            framesCounter++;
            ballPositionX = (int)EaseElasticOut((float)framesCounter, -100, screenWidth/2.0f + 100, 120);

            if (framesCounter >= 120)
            {
                framesCounter = 0;
                state = 1;
            }
        }
        else if (state == 1)        // Increase ball radius with easing
        {
            framesCounter++;
            ballRadius = (int)EaseElasticIn((float)framesCounter, 20, 500, 200);

            if (framesCounter >= 200)
            {
                framesCounter = 0;
                state = 2;
            }
        }
        else if (state == 2)        // Change ball alpha with easing (background color blending)
        {
            framesCounter++;
            ballAlpha = EaseCubicOut((float)framesCounter, 0.0f, 1.0f, 200);

            if (framesCounter >= 200)
            {
                framesCounter = 0;
                state = 3;
            }
        }
        else if (state == 3)        // Reset state to play again
        {
            if (IsKeyPressed(KEY_ENTER))
            {
                // Reset required variables to play again
                ballPositionX = -100;
                ballRadius = 20;
                ballAlpha = 0.0f;
                state = 0;
            }
        }

        if (IsKeyPressed(KEY_R)) framesCounter = 0;
        //----------------------------------------------------------------------------------

        // Draw
        //----------------------------------------------------------------------------------
        BeginDrawing();

            BeginMode2D(PspSceneCamera());

            ClearBackground(RAYWHITE);

            if (state >= 2) DrawRectangle(0, 0, screenWidth, screenHeight, GREEN);
            DrawCircle(ballPositionX, 200, (float)ballRadius, Fade(RED, 1.0f - ballAlpha));

            if (state == 3) DrawText("PRESS [ENTER] TO PLAY AGAIN!", 240, 200, 20, BLACK);

                    EndMode2D();
            DrawText("shapes_easings_ball", 8, 8, 10, DARKGRAY);

        EndDrawing();
        //----------------------------------------------------------------------------------
    }

    // De-Initialization
    //--------------------------------------------------------------------------------------
    TraceLog(LOG_INFO, "DONE frames %i", 600);
    sceKernelSleepThread();        // Close window and OpenGL context
    //--------------------------------------------------------------------------------------

    return 0;
}