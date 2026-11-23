/*******************************************************************************************
*
*   raylib [textures] example - npatch drawing
*
*   Example complexity rating: [★★★☆] 3/4
*
*   NOTE: Images are loaded in CPU memory (RAM); textures are loaded in GPU memory (VRAM)
*
*   Example originally created with raylib 2.0, last time updated with raylib 2.5
*
*   Example contributed by Jorge A. Gomes (@overdev) and reviewed by Ramon Santamaria (@raysan5)
*
*   Example licensed under an unmodified zlib/libpng license, which is an OSI-certified,
*   BSD-like license that allows static linking with closed source software
*
*   Copyright (c) 2018-2025 Jorge A. Gomes (@overdev) and Ramon Santamaria (@raysan5)
*
********************************************************************************************/

#include "raylib.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>



//------------------------------------------------------------------------------------
// Program main entry point
//------------------------------------------------------------------------------------
void sample_textures_npatch_drawing(void)
{
    // Initialization
    //--------------------------------------------------------------------------------------
    const int screenWidth = 480;
    const int screenHeight = 272;
    // NOTE: Textures MUST be loaded after Window initialization (OpenGL context is required)
    Texture2D texNPatch = LoadTexture("resources/ninepatch_button.png");

    Vector2 mousePosition = { 0 };
    Vector2 origin = { 0.0f, 0.0f };

    // Position and size of the n-patches
    Rectangle dstRec1 = { 330.0f, 110.0f, 80.0f, 80.0f };
    Rectangle dstRec2 = { 230.0f, 110.0f, 80.0f, 80.0f };
    Rectangle dstRecH = { 140.0f, 80.0f, 80.0f, 40.0f };
    Rectangle dstRecV = { 90.0f, 110.0f, 40.0f, 80.0f };

    // A 9-patch (NPATCH_NINE_PATCH) changes its sizes in both axis
    NPatchInfo ninePatchInfo1 = { (Rectangle){ 0.0f, 0.0f, 64.0f, 64.0f }, 12, 40, 12, 12, NPATCH_NINE_PATCH };
    NPatchInfo ninePatchInfo2 = { (Rectangle){ 0.0f, 128.0f, 64.0f, 64.0f }, 16, 16, 16, 16, NPATCH_NINE_PATCH };

    // A horizontal 3-patch (NPATCH_THREE_PATCH_HORIZONTAL) changes its sizes along the x axis only
    NPatchInfo h3PatchInfo = { (Rectangle){ 0.0f,  64.0f, 64.0f, 64.0f }, 8, 8, 8, 8, NPATCH_THREE_PATCH_HORIZONTAL };

    // A vertical 3-patch (NPATCH_THREE_PATCH_VERTICAL) changes its sizes along the y axis only
    NPatchInfo v3PatchInfo = { (Rectangle){ 0.0f, 192.0f, 64.0f, 64.0f }, 6, 6, 6, 6, NPATCH_THREE_PATCH_VERTICAL };

    SetTargetFPS(60);
    //---------------------------------------------------------------------------------------

    // Main game loop
    for (int frames = 0; !IsGamepadButtonPressed(0, GAMEPAD_BUTTON_MIDDLE_RIGHT) && !WindowShouldClose(); frames++)    // Detect window close button or ESC key
    {
        // Update
        //----------------------------------------------------------------------------------
        mousePosition = GetMousePosition();

        // No mouse on PSP: keep the default patch sizes
        if ((mousePosition.x > 0.0f) && (mousePosition.y > 0.0f))
        {
        // Resize the n-patches based on mouse position
        dstRec1.width = mousePosition.x - dstRec1.x;
        dstRec1.height = mousePosition.y - dstRec1.y;
        dstRec2.width = mousePosition.x - dstRec2.x;
        dstRec2.height = mousePosition.y - dstRec2.y;
        dstRecH.width = mousePosition.x - dstRecH.x;
        dstRecV.height = mousePosition.y - dstRecV.y;

        // Set a minimum width and/or height
        if (dstRec1.width < 1.0f) dstRec1.width = 1.0f;
        if (dstRec1.width > 300.0f) dstRec1.width = 300.0f;
        if (dstRec1.height < 1.0f) dstRec1.height = 1.0f;
        if (dstRec2.width < 1.0f) dstRec2.width = 1.0f;
        if (dstRec2.width > 300.0f) dstRec2.width = 300.0f;
        if (dstRec2.height < 1.0f) dstRec2.height = 1.0f;
        if (dstRecH.width < 1.0f) dstRecH.width = 1.0f;
        if (dstRecV.height < 1.0f) dstRecV.height = 1.0f;
        }
        //----------------------------------------------------------------------------------

        // Draw
        //----------------------------------------------------------------------------------
        BeginDrawing();

            ClearBackground(RAYWHITE);

            // Draw the n-patches
            DrawTextureNPatch(texNPatch, ninePatchInfo2, dstRec2, origin, 0.0f, WHITE);
            DrawTextureNPatch(texNPatch, ninePatchInfo1, dstRec1, origin, 0.0f, WHITE);
            DrawTextureNPatch(texNPatch, h3PatchInfo, dstRecH, origin, 0.0f, WHITE);
            DrawTextureNPatch(texNPatch, v3PatchInfo, dstRecV, origin, 0.0f, WHITE);

            // Draw the source texture
            DrawRectangleLines(2, 5, 74, 266, BLUE);
            DrawTexture(texNPatch, 5, 10, WHITE);
            DrawText("TEXTURE", 5, 260, 10, DARKGRAY);

            DrawText("NPATCH: nine-patch and 3-patch", 10, 2, 10, DARKGRAY);

        EndDrawing();


        if (((frames + 1)%60) == 0) TraceLog(LOG_INFO, "FRAME %i fps %i", frames + 1, GetFPS());
        //----------------------------------------------------------------------------------
    }

    // De-Initialization
    //--------------------------------------------------------------------------------------
    UnloadTexture(texNPatch);       // Texture unloading

    return;
}
