/*******************************************************************************************
*
*   raylib [shapes] example - colors palette
*
*   Example complexity rating: [★★☆☆] 2/4
*
*   Example originally created with raylib 1.0, last time updated with raylib 2.5
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

PSP_MODULE_INFO("shapes_colors_palette", 0, 1, 1);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);

#define MAX_COLORS_COUNT    21          // Number of colors available

//------------------------------------------------------------------------------------
// Program main entry point
//------------------------------------------------------------------------------------
int main(void)
{
    // Initialization
    //--------------------------------------------------------------------------------------
    const int screenWidth = 800;
    const int screenHeight = 450;

    InitWindow(480, 272, "raylib [shapes] example - colors palette");

    Color colors[MAX_COLORS_COUNT] = {
        DARKGRAY, MAROON, ORANGE, DARKGREEN, DARKBLUE, DARKPURPLE, DARKBROWN,
        GRAY, RED, GOLD, LIME, BLUE, VIOLET, BROWN, LIGHTGRAY, PINK, YELLOW,
        GREEN, SKYBLUE, PURPLE, BEIGE };

    const char *colorNames[MAX_COLORS_COUNT] = {
        "DARKGRAY", "MAROON", "ORANGE", "DARKGREEN", "DARKBLUE", "DARKPURPLE",
        "DARKBROWN", "GRAY", "RED", "GOLD", "LIME", "BLUE", "VIOLET", "BROWN",
        "LIGHTGRAY", "PINK", "YELLOW", "GREEN", "SKYBLUE", "PURPLE", "BEIGE" };

    Rectangle colorsRecs[MAX_COLORS_COUNT] = { 0 };     // Rectangles array

    // Fills colorsRecs data (for every rectangle)
    for (int i = 0; i < MAX_COLORS_COUNT; i++)
    {
        colorsRecs[i].x = 20.0f + 100.0f *(i%7) + 10.0f *(i%7);
        colorsRecs[i].y = 80.0f + 100.0f *((int)i/7) + 10.0f *((float)i/7);
        colorsRecs[i].width = 100.0f;
        colorsRecs[i].height = 100.0f;
    }

    int colorState[MAX_COLORS_COUNT] = { 0 };           // Color state: 0-DEFAULT, 1-MOUSE_HOVER

    Vector2 mousePoint = { 0.0f, 0.0f };

    SetTargetFPS(60);               // Set our game to run at 60 frames-per-second
    //--------------------------------------------------------------------------------------

    // Main game loop
    for (int frame = 0; frame < 600; frame++)
    {
        pspFrame = frame;
        // Update
        //----------------------------------------------------------------------------------
        mousePoint = GetMousePosition();

        for (int i = 0; i < MAX_COLORS_COUNT; i++)
        {
            if (CheckCollisionPointRec(mousePoint, colorsRecs[i])) colorState[i] = 1;
            else colorState[i] = 0;
        }
        //----------------------------------------------------------------------------------

        // Draw
        //----------------------------------------------------------------------------------
        BeginDrawing();

            BeginMode2D(PspSceneCamera());

            ClearBackground(RAYWHITE);

            DrawText("raylib colors palette", 28, 42, 20, BLACK);
            DrawText("press SPACE to see all colors", GetScreenWidth() - 180, GetScreenHeight() - 40, 10, GRAY);

            for (int i = 0; i < MAX_COLORS_COUNT; i++)    // Draw all rectangles
            {
                DrawRectangleRec(colorsRecs[i], Fade(colors[i], colorState[i]? 0.6f : 1.0f));

                if (IsKeyDown(KEY_SPACE) || colorState[i])
                {
                    DrawRectangle((int)colorsRecs[i].x, (int)(colorsRecs[i].y + colorsRecs[i].height - 26), (int)colorsRecs[i].width, 20, BLACK);
                    DrawRectangleLinesEx(colorsRecs[i], 6, Fade(BLACK, 0.3f));
                    DrawText(colorNames[i], (int)(colorsRecs[i].x + colorsRecs[i].width - MeasureText(colorNames[i], 10) - 12),
                        (int)(colorsRecs[i].y + colorsRecs[i].height - 20), 10, colors[i]);
                }
            }

                    EndMode2D();
            DrawText("shapes_colors_palette", 8, 8, 10, DARKGRAY);

        EndDrawing();
        //----------------------------------------------------------------------------------
    }

    // De-Initialization
    //--------------------------------------------------------------------------------------
    TraceLog(LOG_INFO, "DONE frames %i", 600);
    sceKernelSleepThread();                // Close window and OpenGL context
    //--------------------------------------------------------------------------------------

    return 0;
}