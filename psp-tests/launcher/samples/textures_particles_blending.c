/*******************************************************************************************
*
*   raylib [textures] example - particles blending
*
*   Example complexity rating: [★☆☆☆] 1/4
*
*   Example originally created with raylib 1.7, last time updated with raylib 3.5
*
*   Example licensed under an unmodified zlib/libpng license, which is an OSI-certified,
*   BSD-like license that allows static linking with closed source software
*
*   Copyright (c) 2017-2025 Ramon Santamaria (@raysan5)
*
********************************************************************************************/

#include "raylib.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>



#define MAX_PARTICLES 200

//----------------------------------------------------------------------------------
// Types and Structures Definition
//----------------------------------------------------------------------------------
// Particle structure
typedef struct {
    Vector2 position;
    Color color;
    float alpha;
    float size;
    float rotation;
    bool active;        // NOTE: Use it to activate/deactive particle
} Particle;

//------------------------------------------------------------------------------------
// Program main entry point
//------------------------------------------------------------------------------------
void sample_textures_particles_blending(void)
{
    // Initialization
    //--------------------------------------------------------------------------------------
    const int screenWidth = 480;
    const int screenHeight = 272;
    // Load smoke particle
    Texture2D texSmoke = LoadTexture("resources/spark_flame.png");

    // Particles pool, reusing them
    Particle mouseTail[MAX_PARTICLES] = { 0 };

    // Initialize particles
    for (int i = 0; i < MAX_PARTICLES; i++)
    {
        mouseTail[i].position = (Vector2){ 0, 0 };
        mouseTail[i].color = (Color){ GetRandomValue(0, 255), GetRandomValue(0, 255), GetRandomValue(0, 255), 255 };
        mouseTail[i].alpha = 1.0f;
        mouseTail[i].size = (float)GetRandomValue(1, 30)/20.0f;
        mouseTail[i].rotation = (float)GetRandomValue(0, 360);
        mouseTail[i].active = false;
    }

    float gravity = 3.0f;
    int blending = BLEND_ALPHA;

    SetTargetFPS(60);
    //--------------------------------------------------------------------------------------

    // Main game loop
    for (int frames = 0; !IsGamepadButtonPressed(0, GAMEPAD_BUTTON_MIDDLE_RIGHT) && !WindowShouldClose(); frames++)    // Detect window close button or ESC key
    {
        // Update
        //----------------------------------------------------------------------------------
        if (frames < 598)
        {
        // Activate one particle every frame and Update active particles
        // NOTE: Particles initial position should be mouse position when activated
        // NOTE: Particles fall down with gravity and rotation... and disappear after 2 seconds (alpha = 0)
        // NOTE: When a particle disappears, active = false and it can be reused
        for (int i = 0; i < MAX_PARTICLES; i++)
        {
            if (!mouseTail[i].active)
            {
                mouseTail[i].active = true;
                mouseTail[i].alpha = 1.0f;
                mouseTail[i].position = (Vector2){ screenWidth/2.0f, screenHeight/2.0f };
                i = MAX_PARTICLES;
            }
        }

        for (int i = 0; i < MAX_PARTICLES; i++)
        {
            if (mouseTail[i].active)
            {
                mouseTail[i].position.y += gravity/2;
                mouseTail[i].alpha -= 0.005f;

                if (mouseTail[i].alpha <= 0.0f) mouseTail[i].active = false;

                mouseTail[i].rotation += 2.0f;
            }
        }

        if (IsKeyPressed(KEY_SPACE))
        {
            if (blending == BLEND_ALPHA) blending = BLEND_ADDITIVE;
            else blending = BLEND_ALPHA;
        }
        }
        //----------------------------------------------------------------------------------

        // Draw
        //----------------------------------------------------------------------------------
        BeginDrawing();

            ClearBackground(DARKGRAY);

            BeginBlendMode(blending);

                // Draw active particles
                for (int i = 0; i < MAX_PARTICLES; i++)
                {
                    if (mouseTail[i].active) 
                        DrawTexturePro(texSmoke, (Rectangle){ 0.0f, 0.0f, (float)texSmoke.width, (float)texSmoke.height },
                            (Rectangle){ mouseTail[i].position.x, mouseTail[i].position.y, texSmoke.width*mouseTail[i].size, texSmoke.height*mouseTail[i].size },
                            (Vector2){ (float)(texSmoke.width*mouseTail[i].size/2.0f), (float)(texSmoke.height*mouseTail[i].size/2.0f) }, mouseTail[i].rotation,
                            Fade(mouseTail[i].color, mouseTail[i].alpha));
                }

            EndBlendMode();

            const char *note = "PRESS SPACE to CHANGE BLENDING MODE";
            DrawText(note, (screenWidth - MeasureText(note, 10))/2, 10, 10, BLACK);

            const char *mode = (blending == BLEND_ALPHA)? "ALPHA BLENDING" : "ADDITIVE BLENDING";
            DrawText(mode, (screenWidth - MeasureText(mode, 10))/2, screenHeight - 20, 10,
                     (blending == BLEND_ALPHA)? BLACK : RAYWHITE);

        EndDrawing();


        if (((frames + 1)%60) == 0) TraceLog(LOG_INFO, "FRAME %i fps %i", frames + 1, GetFPS());
        //----------------------------------------------------------------------------------
    }

    // De-Initialization
    //--------------------------------------------------------------------------------------
    UnloadTexture(texSmoke);

    return;
}