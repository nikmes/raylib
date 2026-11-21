/*******************************************************************************************
*
*   raylib [textures] example - background scrolling
*
*   Example complexity rating: [★☆☆☆] 1/4
*
*   Example originally created with raylib 2.0, last time updated with raylib 2.5
*
*   Example licensed under an unmodified zlib/libpng license, which is an OSI-certified,
*   BSD-like license that allows static linking with closed source software
*
*   Copyright (c) 2019-2025 Ramon Santamaria (@raysan5)
*
********************************************************************************************/

#include <pspkernel.h>
#include "raylib.h"

PSP_MODULE_INFO("background_scrolling", 0, 1, 1);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);

//------------------------------------------------------------------------------------
// Program main entry point
//------------------------------------------------------------------------------------
int main(void)
{
    // Initialization
    //--------------------------------------------------------------------------------------
    const int screenWidth = 480;
    const int screenHeight = 272;

    InitWindow(screenWidth, screenHeight, "raylib [textures] example - background scrolling");

    // NOTE: Be careful, background width must be equal or bigger than screen width
    // if not, texture should be draw more than two times for scrolling effect
    Texture2D texBackground = LoadTexture("resources/cyberpunk_street_background.png");
    Texture2D texMidground = LoadTexture("resources/cyberpunk_street_midground.png");
    Texture2D texForeground = LoadTexture("resources/cyberpunk_street_foreground.png");

    float scrollingBackground = 0.0f;
    float scrollingMidground = 0.0f;
    float scrollingForeground = 0.0f;

    SetTargetFPS(60);               // Set our game to run at 60 frames-per-second
    //--------------------------------------------------------------------------------------

    // Main game loop
    for (int frames = 0; frames < 600; frames++)    // Detect window close button or ESC key
    {
        // Update
        //----------------------------------------------------------------------------------
        if (frames < 598)
        {
            scrollingBackground -= 0.1f;
            scrollingMidground -= 0.5f;
            scrollingForeground -= 1.0f;

            if (scrollingBackground <= -texBackground.width) scrollingBackground = 0;
            if (scrollingMidground <= -texMidground.width) scrollingMidground = 0;
            if (scrollingForeground <= -texForeground.width) scrollingForeground = 0;
        }
        //----------------------------------------------------------------------------------

        // Draw
        //----------------------------------------------------------------------------------
        BeginDrawing();

            ClearBackground(GetColor(0x052c46ff));

            // Draw background image twice
            // NOTE: Texture is scaled twice its size
            DrawTextureEx(texBackground, (Vector2){ scrollingBackground, 20 }, 0.0f, 1.0f, WHITE);
            DrawTextureEx(texBackground, (Vector2){ texBackground.width + scrollingBackground, 20 }, 0.0f, 1.0f, WHITE);

            // Draw midground image twice
            DrawTextureEx(texMidground, (Vector2){ scrollingMidground, 20 }, 0.0f, 1.0f, WHITE);
            DrawTextureEx(texMidground, (Vector2){ texMidground.width + scrollingMidground, 20 }, 0.0f, 1.0f, WHITE);

            // Draw foreground image twice
            DrawTextureEx(texForeground, (Vector2){ scrollingForeground, 70 }, 0.0f, 1.0f, WHITE);
            DrawTextureEx(texForeground, (Vector2){ texForeground.width + scrollingForeground, 70 }, 0.0f, 1.0f, WHITE);

            DrawText("BACKGROUND SCROLLING & PARALLAX", 10, 10, 20, RED);
            DrawText("(c) Cyberpunk Street Environment by Luis Zuno (@ansimuz)", screenWidth - 330, screenHeight - 20, 10, RAYWHITE);

        EndDrawing();


        if (((frames + 1)%60) == 0) TraceLog(LOG_INFO, "FRAME %i fps %i", frames + 1, GetFPS());
        //----------------------------------------------------------------------------------
    }

    // De-Initialization
    //--------------------------------------------------------------------------------------
    UnloadTexture(texBackground);  // Unload background texture
    UnloadTexture(texMidground);   // Unload midground texture
    UnloadTexture(texForeground);  // Unload foreground texture

    TraceLog(LOG_INFO, "DONE frames %i", 600);
    sceKernelSleepThread();

    return 0;
}