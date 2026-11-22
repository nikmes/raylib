/*******************************************************************************************
*
*   raylib [core] example - input keys
*
*   Example complexity rating: [★☆☆☆] 1/4
*
*   Ported for this tree from examples/core/core_input_keys.c (raylib master).
*   Adjustments: 480x272; a bounded frame count and a DONE line so the host can
*   capture deterministically. Input uses only IsKeyDown, so this file has no
*   platform headers or conditionals.
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

#ifndef FRAMES
    #define FRAMES 900
#endif

int main(void)
{
    const int screenWidth = 480;
    const int screenHeight = 272;

    InitWindow(screenWidth, screenHeight, "raylib [core] example - input keys");
    SetTargetFPS(60);

    Vector2 ballPosition = { (float)screenWidth/2, (float)screenHeight/2 };

    for (int frames = 0; frames < FRAMES; frames++)
    {
        if (IsKeyDown(KEY_RIGHT)) ballPosition.x += 2.0f;
        if (IsKeyDown(KEY_LEFT)) ballPosition.x -= 2.0f;
        if (IsKeyDown(KEY_UP)) ballPosition.y -= 2.0f;
        if (IsKeyDown(KEY_DOWN)) ballPosition.y += 2.0f;

        if (ballPosition.x < 50.0f) ballPosition.x = 50.0f;
        if (ballPosition.x > (float)screenWidth - 50.0f) ballPosition.x = (float)screenWidth - 50.0f;
        if (ballPosition.y < 50.0f) ballPosition.y = 50.0f;
        if (ballPosition.y > (float)screenHeight - 50.0f) ballPosition.y = (float)screenHeight - 50.0f;

        BeginDrawing();
        ClearBackground(RAYWHITE);
        DrawText("move the ball with arrow keys", 10, 10, 20, DARKGRAY);
        DrawCircleV(ballPosition, 50, MAROON);
        DrawText(TextFormat("x %.0f  y %.0f", ballPosition.x, ballPosition.y), 10, 240, 10, DARKGRAY);
        EndDrawing();

        if (((frames + 1)%60) == 0)
            TraceLog(LOG_INFO, "BALL frame=%i x=%.1f y=%.1f", frames + 1, ballPosition.x, ballPosition.y);
    }

    TraceLog(LOG_INFO, "DONE frames %i", FRAMES);

    // Stay resident without drawing: the host captures after DONE and resets.
    for (;;) WaitTime(0.1);

    return 0;
}
