/*******************************************************************************************
*
*   raylib [core] example - random sequence
*
*   Example complexity rating: [★☆☆☆] 1/4
*
*   Example originally created with raylib 5.0, last time updated with raylib 5.0
*
*   Example contributed by Dalton Overmyer (@REDl3east) and reviewed by Ramon Santamaria (@raysan5)
*
*   Example licensed under an unmodified zlib/libpng license, which is an OSI-certified,
*   BSD-like license that allows static linking with closed source software
*
*   Copyright (c) 2023-2025 Dalton Overmyer (@REDl3east)
*
********************************************************************************************/

#include "raylib.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "psp_input.h"


#include "raymath.h"

#include <stdlib.h>     // Required for: malloc(), free()

//----------------------------------------------------------------------------------
// Types and Structures Definition
//----------------------------------------------------------------------------------
typedef struct ColorRect {
    Color color;
    Rectangle rect;
} ColorRect;

//------------------------------------------------------------------------------------
// Module Functions Declaration
//------------------------------------------------------------------------------------
static Color GenerateRandomColor(void);
static ColorRect *GenerateRandomColorRectSequence(float rectCount, float rectWidth, float screenWidth, float screenHeight);
static void ShuffleColorRectSequence(ColorRect *rectangles, int rectCount);

//------------------------------------------------------------------------------------
// Program main entry point
//------------------------------------------------------------------------------------
void sample_core_random_sequence(void)
{
    // Initialization
    //--------------------------------------------------------------------------------------
    const int screenWidth = 800;
    const int screenHeight = 450;
    int rectCount = 20;
    float rectSize = (float)screenWidth/rectCount;
    ColorRect *rectangles = GenerateRandomColorRectSequence((float)rectCount, rectSize, (float)screenWidth, 0.75f*screenHeight);

    SetTargetFPS(60);
    //--------------------------------------------------------------------------------------

    // Main game loop
    for (int frame = 0; !IsGamepadButtonPressed(0, GAMEPAD_BUTTON_MIDDLE_RIGHT); frame++)
    {
        pspFrame = frame;
        // Update
        //----------------------------------------------------------------------------------
        if (IsKeyPressed(KEY_SPACE)) ShuffleColorRectSequence(rectangles, rectCount);

        if (IsKeyPressed(KEY_UP))
        {
            rectCount++;
            rectSize = (float)screenWidth/rectCount;
            RL_FREE(rectangles);

            // Re-generate random sequence with new count
            rectangles = GenerateRandomColorRectSequence((float)rectCount, rectSize, (float)screenWidth, 0.75f*screenHeight);
        }

        if (IsKeyPressed(KEY_DOWN))
        {
            if (rectCount >= 4)
            {
                rectCount--;
                rectSize = (float)screenWidth/rectCount;
                RL_FREE(rectangles);

                // Re-generate random sequence with new count
                rectangles = GenerateRandomColorRectSequence((float)rectCount, rectSize, (float)screenWidth, 0.75f*screenHeight);
            }
        }
        //----------------------------------------------------------------------------------

        // Draw
        //----------------------------------------------------------------------------------
        BeginDrawing();

            BeginMode2D(PspSceneCamera());

            ClearBackground(RAYWHITE);

            for (int i = 0; i < rectCount; i++)
            {
                DrawRectangleRec(rectangles[i].rect, rectangles[i].color);
            }

            DrawText("Press SPACE to shuffle the current sequence", 10, screenHeight - 96, 20, BLACK);
            DrawText("Press UP to add a rectangle and generate a new sequence", 10, screenHeight - 64, 20, BLACK);
            DrawText("Press DOWN to remove a rectangle and generate a new sequence", 10, screenHeight - 32, 20, BLACK);

            DrawText(TextFormat("Count: %d rectangles", rectCount), 10, 10, 20, MAROON);

            DrawFPS(screenWidth - 80, 10);

                    EndMode2D();
            DrawText("core_random_sequence", 8, 8, 10, DARKGRAY);

        EndDrawing();
        //----------------------------------------------------------------------------------
    }

    // De-Initialization
    //--------------------------------------------------------------------------------------
    RL_FREE(rectangles);
    //--------------------------------------------------------------------------------------

    return;
}

//------------------------------------------------------------------------------------
// Module Functions Definition
//------------------------------------------------------------------------------------
static Color GenerateRandomColor(void)
{
    Color color = {
        GetRandomValue(0, 255),
        GetRandomValue(0, 255),
        GetRandomValue(0, 255),
        255
    };

    return color;
}

static ColorRect *GenerateRandomColorRectSequence(float rectCount, float rectWidth, float screenWidth, float screenHeight)
{
    ColorRect *rectangles = (ColorRect *)RL_CALLOC((int)rectCount, sizeof(ColorRect));

    int *seq = LoadRandomSequence((unsigned int)rectCount, 0, (unsigned int)rectCount - 1);
    float rectSeqWidth = rectCount*rectWidth;
    float startX = (screenWidth - rectSeqWidth)*0.5f;

    for (int i = 0; i < rectCount; i++)
    {
        int rectHeight = (int)Remap((float)seq[i], 0, rectCount - 1, 0, screenHeight);

        rectangles[i].color = GenerateRandomColor();
        rectangles[i].rect = CLITERAL(Rectangle){ startX + i*rectWidth, screenHeight - rectHeight, rectWidth, (float)rectHeight };
    }

    UnloadRandomSequence(seq);

    return rectangles;
}

static void ShuffleColorRectSequence(ColorRect *rectangles, int rectCount)
{
    int *seq = LoadRandomSequence(rectCount, 0, rectCount -  1);

    for (int i1 = 0; i1 < rectCount; i1++)
    {
        ColorRect *r1 = &rectangles[i1];
        ColorRect *r2 = &rectangles[seq[i1]];

        // Swap only the color and height
        ColorRect tmp = *r1;
        r1->color = r2->color;
        r1->rect.height = r2->rect.height;
        r1->rect.y = r2->rect.y;
        r2->color = tmp.color;
        r2->rect.height = tmp.rect.height;
        r2->rect.y = tmp.rect.y;
    }

    UnloadRandomSequence(seq);
}
