/*******************************************************************************************
*
*   raylib [textures] example - screen buffer
*
*   Example complexity rating: [★★☆☆] 2/4
*
*   Example originally created with raylib 5.5, last time updated with raylib 5.5
*
*   Example contributed by Agnis Aldiņš (@nezvers) and reviewed by Ramon Santamaria (@raysan5)
*
*   Example licensed under an unmodified zlib/libpng license, which is an OSI-certified,
*   BSD-like license that allows static linking with closed source software
*
*   Copyright (c) 2025 Agnis Aldiņš (@nezvers)
*
********************************************************************************************/

#include "raylib.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>



#include <stdlib.h>     // Required for: calloc(), free()

#define MAX_COLORS      256
#define SCALE_FACTOR      2

//------------------------------------------------------------------------------------
// Program main entry point
//------------------------------------------------------------------------------------
void sample_textures_screen_buffer(void)
{
    // Initialization
    //--------------------------------------------------------------------------------------
    const int screenWidth = 480;
    const int screenHeight = 272;
    int imageWidth = screenWidth/SCALE_FACTOR;
    int imageHeight = screenHeight/SCALE_FACTOR;
    int flameWidth = screenWidth/SCALE_FACTOR;

    Color palette[MAX_COLORS] = { 0 };
    unsigned char *indexBuffer = RL_CALLOC(imageWidth*imageWidth, sizeof(unsigned char));
    unsigned char *flameRootBuffer = RL_CALLOC(flameWidth, sizeof(unsigned char));

    Image imScreen = GenImageColor(imageWidth, imageHeight, BLACK);
    Texture2D texScreen = LoadTextureFromImage(imScreen);

    // Generate flame color palette
    for (int i = 0; i < MAX_COLORS; i++)
    {
        float t = (float)i/(float)(MAX_COLORS - 1);
        float hue = t*t;
        float saturation = t;
        float value = t;

        palette[i] = ColorFromHSV(250.0f + 150.0f*hue, saturation, value);
    }

    SetTargetFPS(60);
    //--------------------------------------------------------------------------------------

    // Main game loop
    for (int frames = 0; !IsGamepadButtonPressed(0, GAMEPAD_BUTTON_MIDDLE_RIGHT); frames++)    // Detect window close button or ESC key
    {
        // Update
        //----------------------------------------------------------------------------------
        if (frames < 598)
        {
        // Grow flameRoot
        for (int x = 2; x < flameWidth; x++)
        {
            int flame = (int)flameRootBuffer[x];
            flame += GetRandomValue(0, 2);
            flameRootBuffer[x] = (flame > 255)? 255: (unsigned char)flame;
        }

        // Transfer flameRoot to indexBuffer
        for (int x = 0; x < flameWidth; x++)
        {
            int i = x + (imageHeight - 1)*imageWidth;
            indexBuffer[i] = flameRootBuffer[x];
        }

        // Clear top row, because it can't move any higher
        for (int x = 0; x < imageWidth; x++)
        {
            if (indexBuffer[x] != 0) indexBuffer[x] = 0;
        }

        // Skip top row, it is already cleared
        for (int y = 1; y < imageHeight; y++)
        {
            for (int x = 0; x < imageWidth; x++)
            {
                unsigned int i = x + y*imageWidth;
                unsigned char colorIndex = indexBuffer[i];

                if (colorIndex != 0)
                {
                    // Move pixel a row above
                    indexBuffer[i] = 0;
                    int moveX = GetRandomValue(0, 2) - 1;
                    int newX = x + moveX;

                    if ((newX > 0) && (newX < imageWidth))
                    {
                        unsigned int iabove = i - imageWidth + moveX;
                        int decay = GetRandomValue(0, 3);
                        colorIndex -= (decay < colorIndex)? decay : colorIndex;
                        indexBuffer[iabove] = colorIndex;
                    }
                }
            }
        }

        // Update screenImage with palette colors
        for (int y = 1; y < imageHeight; y++)
        {
            for (int x = 0; x < imageWidth; x++)
            {
                unsigned int i = x + y*imageWidth;
                unsigned char colorIndex = indexBuffer[i];
                Color col = palette[colorIndex];

                ImageDrawPixel(&imScreen, x, y, col);
            }
        }

        UpdateTexture(texScreen, imScreen.data);
        }
        //----------------------------------------------------------------------------------

        // Draw
        //----------------------------------------------------------------------------------
        BeginDrawing();

            ClearBackground(RAYWHITE);

            DrawTextureEx(texScreen, (Vector2){ 0, 0 }, 0.0f, 2.0f, WHITE);

        EndDrawing();


        if (((frames + 1)%60) == 0) TraceLog(LOG_INFO, "FRAME %i fps %i", frames + 1, GetFPS());
        //----------------------------------------------------------------------------------
    }

    // De-Initialization
    //--------------------------------------------------------------------------------------
    RL_FREE(indexBuffer);
    RL_FREE(flameRootBuffer);
    UnloadTexture(texScreen);
    UnloadImage(imScreen);

    return;
}
