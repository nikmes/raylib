/*******************************************************************************************
*
*   raylib [shapes] example - rectangle advanced
*
*   Example complexity rating: [★★★★] 4/4
*
*   Example originally created with raylib 5.5, last time updated with raylib 5.5
*
*   Example contributed by Everton Jr. (@evertonse) and reviewed by Ramon Santamaria (@raysan5)
*
*   Example licensed under an unmodified zlib/libpng license, which is an OSI-certified,
*   BSD-like license that allows static linking with closed source software
*
*   Copyright (c) 2024-2025 Everton Jr. (@evertonse) and Ramon Santamaria (@raysan5)
*
********************************************************************************************/

#include "raylib.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>



#include <GL/gl.h>
#include "rlgl.h"

#include <math.h>

//--------------------------------------------------------------------------------------
// Module Functions Declaration
//--------------------------------------------------------------------------------------
// Draw rectangle with rounded edges and horizontal gradient, with options to choose side of roundness
static void DrawRectangleRoundedGradientH(Rectangle rec, float roundnessLeft, float roundnessRight, int segments, Color left, Color right);

//------------------------------------------------------------------------------------
// Program main entry point
//------------------------------------------------------------------------------------
void sample_shapes_rectangle_advanced(void)
{
    // Initialization
    //--------------------------------------------------------------------------------------
    const int screenWidth = 480;
    const int screenHeight = 272;
    SetTargetFPS(60);               // Set our game to run at 60 frames-per-second
    //--------------------------------------------------------------------------------------

    // Main game loop
    for (int frames = 0; !IsGamepadButtonPressed(0, GAMEPAD_BUTTON_MIDDLE_RIGHT); frames++)    // Detect window close button or ESC key
    {
        // Update rectangle bounds
        //----------------------------------------------------------------------------------
        float width = GetScreenWidth()/2.0f, height = GetScreenHeight()/6.0f;
        Rectangle rec = {
            GetScreenWidth()/2.0f - width/2,
            GetScreenHeight()/2.0f - 5*(height/2),
            width, height
        };
        //--------------------------------------------------------------------------------------

        // Draw
        //----------------------------------------------------------------------------------
        BeginDrawing();
            ClearBackground(RAYWHITE);

            // Draw All Rectangles with different roundess  for each side and different gradients
            DrawRectangleRoundedGradientH(rec, 0.8f, 0.8f, 36, BLUE, RED);

            rec.y += rec.height + 1;
            DrawRectangleRoundedGradientH(rec, 0.5f, 1.0f, 36, RED, PINK);

            rec.y += rec.height + 1;
            DrawRectangleRoundedGradientH(rec, 1.0f, 0.5f, 36, RED, BLUE);

            rec.y += rec.height + 1;
            DrawRectangleRoundedGradientH(rec, 0.0f, 1.0f, 36, BLUE, BLACK);

            rec.y += rec.height + 1;
            DrawRectangleRoundedGradientH(rec, 1.0f, 0.0f, 36, BLUE, PINK);
        EndDrawing();

        if (((frames + 1)%60) == 0) TraceLog(LOG_INFO, "FRAME %i fps %i", frames + 1, GetFPS());
        //--------------------------------------------------------------------------------------
    }

    // De-Initialization
    //--------------------------------------------------------------------------------------

    return;
}

//--------------------------------------------------------------------------------------
// Module Functions Definition
//--------------------------------------------------------------------------------------
// Draw rectangle with rounded edges and horizontal gradient, with options to choose side of roundness
// NOTE: Adapted from both 'DrawRectangleRounded()' and 'DrawRectangleGradientH()' raylib [rshapes] implementations
static void DrawRectangleRoundedGradientH(Rectangle rec, float roundnessLeft, float roundnessRight, int segments, Color left, Color right)
{
    // Neither side is rounded
    if ((roundnessLeft <= 0.0f && roundnessRight <= 0.0f) || (rec.width < 1) || (rec.height < 1 ))
    {
        DrawRectangleGradientEx(rec, left, left, right, right);
        return;
    }

    if (roundnessLeft  >= 1.0f) roundnessLeft  = 1.0f;
    if (roundnessRight >= 1.0f) roundnessRight = 1.0f;

    // Calculate corner radius both from right and left
    float recSize = rec.width > rec.height ? rec.height : rec.width;
    float radiusLeft  = (recSize*roundnessLeft)/2;
    float radiusRight = (recSize*roundnessRight)/2;

    if (radiusLeft <= 0.0f) radiusLeft = 0.0f;
    if (radiusRight <= 0.0f) radiusRight = 0.0f;

    if (radiusRight <= 0.0f && radiusLeft <= 0.0f) return;

    float stepLength = 90.0f/(float)segments;

    /*
    Diagram Copied here for reference, original at 'DrawRectangleRounded()' source code

          P0____________________P1
          /|                    |\
         /1|          2         |3\
     P7 /__|____________________|__\ P2
       |   |P8                P9|   |
       | 8 |          9         | 4 |
       | __|____________________|__ |
     P6 \  |P11              P10|  / P3
         \7|          6         |5/
          \|____________________|/
          P5                    P4
    */

    // Coordinates of the 12 points also apdated from `DrawRectangleRounded`
    const Vector2 point[12] = {
        // PO, P1, P2
        {(float)rec.x + radiusLeft, rec.y}, {(float)(rec.x + rec.width) - radiusRight, rec.y}, { rec.x + rec.width, (float)rec.y + radiusRight },
        // P3, P4
        {rec.x + rec.width, (float)(rec.y + rec.height) - radiusRight}, {(float)(rec.x + rec.width) - radiusRight, rec.y + rec.height},
        // P5, P6, P7
        {(float)rec.x + radiusLeft, rec.y + rec.height}, { rec.x, (float)(rec.y + rec.height) - radiusLeft}, {rec.x, (float)rec.y + radiusLeft},
        // P8, P9
        {(float)rec.x + radiusLeft, (float)rec.y + radiusLeft}, {(float)(rec.x + rec.width) - radiusRight, (float)rec.y + radiusRight},
        // P10, P11
        {(float)(rec.x + rec.width) - radiusRight, (float)(rec.y + rec.height) - radiusRight}, {(float)rec.x + radiusLeft, (float)(rec.y + rec.height) - radiusLeft}
    };

    const Vector2 centers[4] = { point[8], point[9], point[10], point[11] };
    const float angles[4] = { 180.0f, 270.0f, 0.0f, 90.0f };

#if defined(SUPPORT_QUADS_DRAW_MODE)
    rlSetTexture(GetShapesTexture().id);
    Rectangle shapeRect = GetShapesTextureRectangle();

    glBegin(GL_QUADS);
        // Draw all the 4 corners: [1] Upper Left Corner, [3] Upper Right Corner, [5] Lower Right Corner, [7] Lower Left Corner
        for (int k = 0; k < 4; ++k)
        {
            Color color;
            float radius;
            if (k == 0) color = left,  radius = radiusLeft;     // [1] Upper Left Corner
            if (k == 1) color = right, radius = radiusRight;    // [3] Upper Right Corner
            if (k == 2) color = right, radius = radiusRight;    // [5] Lower Right Corner
            if (k == 3) color = left,  radius = radiusLeft;     // [7] Lower Left Corner
            float angle = angles[k];
            const Vector2 center = centers[k];

            for (int i = 0; i < segments/2; i++)
            {
                glColor4ub(color.r, color.g, color.b, color.a);
                rlTexCoord2f(shapeRect.x/texShapes.width, shapeRect.y/texShapes.height);
                glVertex2f(center.x, center.y);

                rlTexCoord2f((shapeRect.x + shapeRect.width)/texShapes.width, shapeRect.y/texShapes.height);
                glVertex2f(center.x + cosf(DEG2RAD*(angle + stepLength*2))*radius, center.y + sinf(DEG2RAD*(angle + stepLength*2))*radius);

                rlTexCoord2f((shapeRect.x + shapeRect.width)/texShapes.width, (shapeRect.y + shapeRect.height)/texShapes.height);
                glVertex2f(center.x + cosf(DEG2RAD*(angle + stepLength))*radius, center.y + sinf(DEG2RAD*(angle + stepLength))*radius);

                rlTexCoord2f(shapeRect.x/texShapes.width, (shapeRect.y + shapeRect.height)/texShapes.height);
                glVertex2f(center.x + cosf(DEG2RAD*angle)*radius, center.y + sinf(DEG2RAD*angle)*radius);

                angle += (stepLength*2);
            }

            // End one even segments
            if ( segments%2)
            {
                rlTexCoord2f(shapeRect.x/texShapes.width, shapeRect.y/texShapes.height);
                glVertex2f(center.x, center.y);

                rlTexCoord2f((shapeRect.x + shapeRect.width)/texShapes.width, (shapeRect.y + shapeRect.height)/texShapes.height);
                glVertex2f(center.x + cosf(DEG2RAD*(angle + stepLength))*radius, center.y + sinf(DEG2RAD*(angle + stepLength))*radius);

                rlTexCoord2f(shapeRect.x/texShapes.width, (shapeRect.y + shapeRect.height)/texShapes.height);
                glVertex2f(center.x + cosf(DEG2RAD*angle)*radius, center.y + sinf(DEG2RAD*angle)*radius);

                rlTexCoord2f((shapeRect.x + shapeRect.width)/texShapes.width, shapeRect.y/texShapes.height);
                glVertex2f(center.x, center.y);
            }
        }

        // Here we use the 'Diagram' to guide ourselves to which point receives what color
        // By choosing the color correctly associated with a pointe the gradient effect
        // will naturally come from OpenGL interpolation

        // [2] Upper Rectangle
        glColor4ub(left.r, left.g, left.b, left.a);
        rlTexCoord2f(shapeRect.x/texShapes.width, shapeRect.y/texShapes.height);
        glVertex2f(point[0].x, point[0].y);
        rlTexCoord2f(shapeRect.x/texShapes.width, (shapeRect.y + shapeRect.height)/texShapes.height);
        glVertex2f(point[8].x, point[8].y);

        glColor4ub(right.r, right.g, right.b, right.a);
        rlTexCoord2f((shapeRect.x + shapeRect.width)/texShapes.width, (shapeRect.y + shapeRect.height)/texShapes.height);
        glVertex2f(point[9].x, point[9].y);

        glColor4ub(right.r, right.g, right.b, right.a);
        rlTexCoord2f((shapeRect.x + shapeRect.width)/texShapes.width, shapeRect.y/texShapes.height);
        glVertex2f(point[1].x, point[1].y);

        // [4] Left Rectangle
        glColor4ub(right.r, right.g, right.b, right.a);
        rlTexCoord2f(shapeRect.x/texShapes.width, shapeRect.y/texShapes.height);
        glVertex2f(point[2].x, point[2].y);
        rlTexCoord2f(shapeRect.x/texShapes.width, (shapeRect.y + shapeRect.height)/texShapes.height);
        glVertex2f(point[9].x, point[9].y);
        rlTexCoord2f((shapeRect.x + shapeRect.width)/texShapes.width, (shapeRect.y + shapeRect.height)/texShapes.height);
        glVertex2f(point[10].x, point[10].y);
        rlTexCoord2f((shapeRect.x + shapeRect.width)/texShapes.width, shapeRect.y/texShapes.height);
        glVertex2f(point[3].x, point[3].y);

        // [6] Bottom Rectangle
        glColor4ub(left.r, left.g, left.b, left.a);
        rlTexCoord2f(shapeRect.x/texShapes.width, shapeRect.y/texShapes.height);
        glVertex2f(point[11].x, point[11].y);
        rlTexCoord2f(shapeRect.x/texShapes.width, (shapeRect.y + shapeRect.height)/texShapes.height);
        glVertex2f(point[5].x, point[5].y);

        glColor4ub(right.r, right.g, right.b, right.a);
        rlTexCoord2f((shapeRect.x + shapeRect.width)/texShapes.width, (shapeRect.y + shapeRect.height)/texShapes.height);
        glVertex2f(point[4].x, point[4].y);
        rlTexCoord2f((shapeRect.x + shapeRect.width)/texShapes.width, shapeRect.y/texShapes.height);
        glVertex2f(point[10].x, point[10].y);

        // [8] left Rectangle
        glColor4ub(left.r, left.g, left.b, left.a);
        rlTexCoord2f(shapeRect.x/texShapes.width, shapeRect.y/texShapes.height);
        glVertex2f(point[7].x, point[7].y);
        rlTexCoord2f(shapeRect.x/texShapes.width, (shapeRect.y + shapeRect.height)/texShapes.height);
        glVertex2f(point[6].x, point[6].y);
        rlTexCoord2f((shapeRect.x + shapeRect.width)/texShapes.width, (shapeRect.y + shapeRect.height)/texShapes.height);
        glVertex2f(point[11].x, point[11].y);
        rlTexCoord2f((shapeRect.x + shapeRect.width)/texShapes.width, shapeRect.y/texShapes.height);
        glVertex2f(point[8].x, point[8].y);

        // [9] Middle Rectangle
        glColor4ub(left.r, left.g, left.b, left.a);
        rlTexCoord2f(shapeRect.x/texShapes.width, shapeRect.y/texShapes.height);
        glVertex2f(point[8].x, point[8].y);
        rlTexCoord2f(shapeRect.x/texShapes.width, (shapeRect.y + shapeRect.height)/texShapes.height);
        glVertex2f(point[11].x, point[11].y);

        glColor4ub(right.r, right.g, right.b, right.a);
        rlTexCoord2f((shapeRect.x + shapeRect.width)/texShapes.width, (shapeRect.y + shapeRect.height)/texShapes.height);
        glVertex2f(point[10].x, point[10].y);
        rlTexCoord2f((shapeRect.x + shapeRect.width)/texShapes.width, shapeRect.y/texShapes.height);
        glVertex2f(point[9].x, point[9].y);

    glEnd();
    rlSetTexture(0);
#else

    // Here we use the 'Diagram' to guide ourselves to which point receives what color
    // By choosing the color correctly associated with a pointe the gradient effect
    // will naturally come from OpenGL interpolation
    // But this time instead of Quad, we think in triangles

    glBegin(GL_TRIANGLES);
        // Draw all of the 4 corners: [1] Upper Left Corner, [3] Upper Right Corner, [5] Lower Right Corner, [7] Lower Left Corner
        for (int k = 0; k < 4; ++k)
        {
            Color color = { 0 };
            float radius = 0.0f;
            if (k == 0) color = left,  radius = radiusLeft;     // [1] Upper Left Corner
            if (k == 1) color = right, radius = radiusRight;    // [3] Upper Right Corner
            if (k == 2) color = right, radius = radiusRight;    // [5] Lower Right Corner
            if (k == 3) color = left,  radius = radiusLeft;     // [7] Lower Left Corner

            float angle = angles[k];
            const Vector2 center = centers[k];

            for (int i = 0; i < segments; i++)
            {
                glColor4ub(color.r, color.g, color.b, color.a);
                glVertex2f(center.x, center.y);
                glVertex2f(center.x + cosf(DEG2RAD*(angle + stepLength))*radius, center.y + sinf(DEG2RAD*(angle + stepLength))*radius);
                glVertex2f(center.x + cosf(DEG2RAD*angle)*radius, center.y + sinf(DEG2RAD*angle)*radius);
                angle += stepLength;
            }
        }

        // [2] Upper Rectangle
        glColor4ub(left.r, left.g, left.b, left.a);
        glVertex2f(point[0].x, point[0].y);
        glVertex2f(point[8].x, point[8].y);
        glColor4ub(right.r, right.g, right.b, right.a);
        glVertex2f(point[9].x, point[9].y);
        glVertex2f(point[1].x, point[1].y);
        glColor4ub(left.r, left.g, left.b, left.a);
        glVertex2f(point[0].x, point[0].y);
        glColor4ub(right.r, right.g, right.b, right.a);
        glVertex2f(point[9].x, point[9].y);

        // [4] Right Rectangle
        glColor4ub(right.r, right.g, right.b, right.a);
        glVertex2f(point[9].x, point[9].y);
        glVertex2f(point[10].x, point[10].y);
        glVertex2f(point[3].x, point[3].y);
        glVertex2f(point[2].x, point[2].y);
        glVertex2f(point[9].x, point[9].y);
        glVertex2f(point[3].x, point[3].y);

        // [6] Bottom Rectangle
        glColor4ub(left.r, left.g, left.b, left.a);
        glVertex2f(point[11].x, point[11].y);
        glVertex2f(point[5].x, point[5].y);
        glColor4ub(right.r, right.g, right.b, right.a);
        glVertex2f(point[4].x, point[4].y);
        glVertex2f(point[10].x, point[10].y);
        glColor4ub(left.r, left.g, left.b, left.a);
        glVertex2f(point[11].x, point[11].y);
        glColor4ub(right.r, right.g, right.b, right.a);
        glVertex2f(point[4].x, point[4].y);

        // [8] Left Rectangle
        glColor4ub(left.r, left.g, left.b, left.a);
        glVertex2f(point[7].x, point[7].y);
        glVertex2f(point[6].x, point[6].y);
        glVertex2f(point[11].x, point[11].y);
        glVertex2f(point[8].x, point[8].y);
        glVertex2f(point[7].x, point[7].y);
        glVertex2f(point[11].x, point[11].y);

        // [9] Middle Rectangle
        glColor4ub(left.r, left.g, left.b, left.a);
        glVertex2f(point[8].x, point[8].y);
        glVertex2f(point[11].x, point[11].y);
        glColor4ub(right.r, right.g, right.b, right.a);
        glVertex2f(point[10].x, point[10].y);
        glVertex2f(point[9].x, point[9].y);
        glColor4ub(left.r, left.g, left.b, left.a);
        glVertex2f(point[8].x, point[8].y);
        glColor4ub(right.r, right.g, right.b, right.a);
        glVertex2f(point[10].x, point[10].y);
    glEnd();
#endif
}
