/*******************************************************************************************
*
*   raylib [textures] example - polygon drawing
*
*   Example complexity rating: [★☆☆☆] 1/4
*
*   Example originally created with raylib 3.7, last time updated with raylib 3.7
*
*   Example contributed by Chris Camacho (@chriscamacho) and reviewed by Ramon Santamaria (@raysan5)
*
*   Example licensed under an unmodified zlib/libpng license, which is an OSI-certified,
*   BSD-like license that allows static linking with closed source software
*
*   Copyright (c) 2021-2025 Chris Camacho (@chriscamacho) and Ramon Santamaria (@raysan5)
*
********************************************************************************************/

#include <pspkernel.h>
#include "raylib.h"

PSP_MODULE_INFO("polygon_drawing", 0, 1, 1);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);

#include <GL/gl.h>
#include "rlgl.h"           // Required for: Vertex definition
#include "raymath.h"

#define MAX_POINTS  11      // 10 points and back to the start

// Draw textured polygon, defined by vertex and texture coordinates
void DrawTexturePoly(Texture2D texture, Vector2 center, Vector2 *points, Vector2 *texcoords, int pointCount, Color tint);

//------------------------------------------------------------------------------------
// Program main entry point
//------------------------------------------------------------------------------------
int main(void)
{
    // Initialization
    //--------------------------------------------------------------------------------------
    const int screenWidth = 480;
    const int screenHeight = 272;

    InitWindow(screenWidth, screenHeight, "raylib [textures] example - polygon drawing");

    // Define texture coordinates to map our texture to poly
    Vector2 texcoords[MAX_POINTS] = {
        (Vector2){ 0.75f, 0.0f },
        (Vector2){ 0.25f, 0.0f },
        (Vector2){ 0.0f, 0.5f },
        (Vector2){ 0.0f, 0.75f },
        (Vector2){ 0.25f, 1.0f},
        (Vector2){ 0.375f, 0.875f},
        (Vector2){ 0.625f, 0.875f},
        (Vector2){ 0.75f, 1.0f},
        (Vector2){ 1.0f, 0.75f},
        (Vector2){ 1.0f, 0.5f},
        (Vector2){ 0.75f, 0.0f}  // Close the poly
    };

    // Define the base poly vertices from the UV's
    // NOTE: They can be specified in any other way
    Vector2 points[MAX_POINTS] = { 0 };
    for (int i = 0; i < MAX_POINTS; i++)
    {
        points[i].x = (texcoords[i].x - 0.5f)*256.0f;
        points[i].y = (texcoords[i].y - 0.5f)*256.0f;
    }

    // Define the vertices drawing position
    // NOTE: Initially same as points but updated every frame
    Vector2 positions[MAX_POINTS] = { 0 };
    for (int i = 0; i < MAX_POINTS; i++) positions[i] = points[i];

    // Load texture to be mapped to poly
    Texture2D texture = LoadTexture("resources/cat.png");

    float angle = 0.0f; // Rotation angle (in degrees)

    SetTargetFPS(60);               // Set our game to run at 60 frames-per-second
    //--------------------------------------------------------------------------------------

    // Main game loop
    for (int frames = 0; frames < 600; frames++)    // Detect window close button or ESC key
    {
        // Update
        //----------------------------------------------------------------------------------
        // Update points rotation with an angle transform
        // NOTE: Base points position are not modified
        if (frames < 598)
        {
            angle++;
            for (int i = 0; i < MAX_POINTS; i++) positions[i] = Vector2Rotate(points[i], angle*DEG2RAD);
        }
        //----------------------------------------------------------------------------------

        // Draw
        //----------------------------------------------------------------------------------
        BeginDrawing();

            ClearBackground(RAYWHITE);

            DrawText("textured polygon", 20, 20, 20, DARKGRAY);

            DrawTexturePoly(texture, (Vector2){ GetScreenWidth()/2.0f, GetScreenHeight()/2.0f },
                            positions, texcoords, MAX_POINTS, WHITE);

        EndDrawing();


        if (((frames + 1)%60) == 0) TraceLog(LOG_INFO, "FRAME %i fps %i", frames + 1, GetFPS());
        //----------------------------------------------------------------------------------
    }

    // De-Initialization
    //--------------------------------------------------------------------------------------
    UnloadTexture(texture); // Unload texture

    TraceLog(LOG_INFO, "DONE frames %i", 600);
    sceKernelSleepThread();

    return 0;
}

// Draw textured polygon, defined by vertex and texture coordinates
// NOTE: Polygon center must have straight line path to all points
// without crossing perimeter, points must be in anticlockwise order
void DrawTexturePoly(Texture2D texture, Vector2 center, Vector2 *points, Vector2 *texcoords, int pointCount, Color tint)
{
    glBindTexture(GL_TEXTURE_2D, texture.id);
    glBegin(GL_TRIANGLES);

        glColor4ub(tint.r, tint.g, tint.b, tint.a);

        for (int i = 0; i < pointCount - 1; i++)
        {
            glTexCoord2f(0.5f, 0.5f);
            glVertex2f(center.x, center.y);

            glTexCoord2f(texcoords[i].x, texcoords[i].y);
            glVertex2f(points[i].x + center.x, points[i].y + center.y);

            glTexCoord2f(texcoords[i + 1].x, texcoords[i + 1].y);
            glVertex2f(points[i + 1].x + center.x, points[i + 1].y + center.y);
        }
    glEnd();

    glBindTexture(GL_TEXTURE_2D, 0);
}
