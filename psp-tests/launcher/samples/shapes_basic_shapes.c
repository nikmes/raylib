/*******************************************************************************************
*
*   raylib [shapes] example - basic shapes (PSP parity harness)
*
*   Ported from the sprint-001 the_psp_basic_shapes harness; licensed zlib/libpng.
*
********************************************************************************************/

#include "raylib.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>



void sample_shapes_basic_shapes(void)
{
    const int screenWidth = 480;
    const int screenHeight = 272;
    float rotation = 0.0f;

    SetTargetFPS(60);

    for (int frame = 0; !IsGamepadButtonPressed(0, GAMEPAD_BUTTON_MIDDLE_RIGHT); frame++)
    {
        rotation += 0.2f;

        BeginDrawing();

            ClearBackground(RAYWHITE);

            DrawText("some basic shapes available on raylib", 20, 20, 20, DARKGRAY);

            DrawCircle(screenWidth/5, 75, 30, DARKBLUE);
            DrawCircleGradient((Vector2){ screenWidth/5.0f, 145.0f }, 35, GREEN, SKYBLUE);
            DrawCircleLines(screenWidth/5, 220, 37, DARKBLUE);

            DrawRectangle(screenWidth/4*2 - 60, 60, 120, 60, RED);
            DrawRectangleGradientH(screenWidth/4*2 - 70, 130, 160, 80, MAROON, GOLD);
            DrawRectangleLines(screenWidth/4*2 - 40, 220, 80, 40, ORANGE);

            DrawTriangle((Vector2){ screenWidth/4.0f*3.0f + 30.0f, 60.0f },
                         (Vector2){ screenWidth/4.0f*3.0f, 110.0f },
                         (Vector2){ screenWidth/4.0f*3.0f + 60.0f, 110.0f }, VIOLET);
            DrawTriangleLines((Vector2){ screenWidth/4.0f*3.0f + 30.0f, 120.0f },
                              (Vector2){ screenWidth/4.0f*3.0f + 10.0f, 170.0f },
                              (Vector2){ screenWidth/4.0f*3.0f + 50.0f, 170.0f }, DARKBLUE);

            DrawPoly((Vector2){ screenWidth/4.0f*3 + 30.0f, 220 }, 6, 30, rotation, BROWN);
            DrawPolyLines((Vector2){ screenWidth/4.0f*3 + 30.0f, 220 }, 6, 35, rotation, BROWN);
            DrawPolyLinesEx((Vector2){ screenWidth/4.0f*3 + 30.0f, 220 }, 6, 45, rotation, 6, BEIGE);

            DrawLine(18, 42, screenWidth - 18, 42, BLACK);

        EndDrawing();

        if (((frame + 1)%60) == 0) TraceLog(LOG_INFO, "FRAME %i fps %i", frame + 1, GetFPS());
    }

    return;
}
