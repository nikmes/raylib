/*******************************************************************************************
*
*   raylib [textures] example - image processing
*
*   Example complexity rating: [★★★☆] 3/4
*
*   NOTE: Images are loaded in CPU memory (RAM); textures are loaded in GPU memory (VRAM)
*
*   Example originally created with raylib 1.4, last time updated with raylib 3.5
*
*   Example licensed under an unmodified zlib/libpng license, which is an OSI-certified,
*   BSD-like license that allows static linking with closed source software
*
*   Copyright (c) 2016-2025 Ramon Santamaria (@raysan5)
*
********************************************************************************************/

#include "raylib.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>



#include <stdlib.h>             // Required for: free()

#define NUM_PROCESSES    9

typedef enum {
    NONE = 0,
    COLOR_GRAYSCALE,
    COLOR_TINT,
    COLOR_INVERT,
    COLOR_CONTRAST,
    COLOR_BRIGHTNESS,
    GAUSSIAN_BLUR,
    FLIP_VERTICAL,
    FLIP_HORIZONTAL
} ImageProcess;

static const char *processText[] = {
    "NO PROCESSING",
    "COLOR GRAYSCALE",
    "COLOR TINT",
    "COLOR INVERT",
    "COLOR CONTRAST",
    "COLOR BRIGHTNESS",
    "GAUSSIAN BLUR",
    "FLIP VERTICAL",
    "FLIP HORIZONTAL"
};

//------------------------------------------------------------------------------------
// Program main entry point
//------------------------------------------------------------------------------------
void sample_textures_image_processing(void)
{
    // Initialization
    //--------------------------------------------------------------------------------------
    const int screenWidth = 480;
    const int screenHeight = 272;
    // NOTE: Textures MUST be loaded after Window initialization (OpenGL context is required)

    Image imOrigin = LoadImage("resources/parrots.png");   // Loaded in CPU memory (RAM)
    ImageFormat(&imOrigin, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);         // Format image to RGBA 32bit (required for texture update) <-- ISSUE
    Texture2D texture = LoadTextureFromImage(imOrigin);    // Image converted to texture, GPU memory (VRAM)

    Image imCopy = ImageCopy(imOrigin);

    int currentProcess = COLOR_GRAYSCALE;
    bool textureReload = true;

    Rectangle toggleRecs[NUM_PROCESSES] = { 0 };
    int mouseHoverRec = -1;

    for (int i = 0; i < NUM_PROCESSES; i++) toggleRecs[i] = (Rectangle){ 40.0f, (float)(50 + 32*i), 150.0f, 30.0f };

    SetTargetFPS(60);
    //---------------------------------------------------------------------------------------

    // Main game loop
    for (int frames = 0; !IsGamepadButtonPressed(0, GAMEPAD_BUTTON_MIDDLE_RIGHT); frames++)    // Detect window close button or ESC key
    {
        // Update
        //----------------------------------------------------------------------------------

        // Mouse toggle group logic
        for (int i = 0; i < NUM_PROCESSES; i++)
        {
            if (CheckCollisionPointRec(GetMousePosition(), toggleRecs[i]))
            {
                mouseHoverRec = i;

                if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
                {
                    currentProcess = i;
                    textureReload = true;
                }
                break;
            }
            else mouseHoverRec = -1;
        }

        // Keyboard toggle group logic
        if (IsKeyPressed(KEY_DOWN))
        {
            currentProcess++;
            if (currentProcess > (NUM_PROCESSES - 1)) currentProcess = 0;
            textureReload = true;
        }
        else if (IsKeyPressed(KEY_UP))
        {
            currentProcess--;
            if (currentProcess < 0) currentProcess = (NUM_PROCESSES - 1);
            textureReload = true;
        }

        // Reload texture when required
        if (textureReload)
        {
            UnloadImage(imCopy);                // Unload image-copy data
            imCopy = ImageCopy(imOrigin);     // Restore image-copy from image-origin

            // NOTE: Image processing is a costly CPU process to be done every frame,
            // If image processing is required in a frame-basis, it should be done
            // with a texture and by shaders
            switch (currentProcess)
            {
                case COLOR_GRAYSCALE: ImageColorGrayscale(&imCopy); break;
                case COLOR_TINT: ImageColorTint(&imCopy, GREEN); break;
                case COLOR_INVERT: ImageColorInvert(&imCopy); break;
                case COLOR_CONTRAST: ImageColorContrast(&imCopy, -40); break;
                case COLOR_BRIGHTNESS: ImageColorBrightness(&imCopy, -80); break;
                case GAUSSIAN_BLUR: ImageBlurGaussian(&imCopy, 10); break;
                case FLIP_VERTICAL: ImageFlipVertical(&imCopy); break;
                case FLIP_HORIZONTAL: ImageFlipHorizontal(&imCopy); break;
                default: break;
            }

            Color *pixels = LoadImageColors(imCopy);    // Load pixel data from image (RGBA 32bit)
            UpdateTexture(texture, pixels);             // Update texture with new image data
            UnloadImageColors(pixels);                  // Unload pixels data from RAM

            textureReload = false;
        }
        //----------------------------------------------------------------------------------

        // Draw
        //----------------------------------------------------------------------------------
        BeginDrawing();

            ClearBackground(RAYWHITE);

            DrawText(TextFormat("IMAGE PROCESSING: %s", processText[currentProcess]), 10, 10, 10, DARKGRAY);

            float scale = 0.5f;
            float w = texture.width*scale;
            float h = texture.height*scale;
            DrawTextureEx(texture, (Vector2){ (screenWidth - w)/2, (screenHeight - h)/2 + 10 }, 0.0f, scale, WHITE);
            DrawRectangleLines((int)((screenWidth - w)/2), (int)((screenHeight - h)/2 + 10), (int)w, (int)h, BLACK);

        EndDrawing();


        if (((frames + 1)%60) == 0) TraceLog(LOG_INFO, "FRAME %i fps %i", frames + 1, GetFPS());
        //----------------------------------------------------------------------------------
    }

    // De-Initialization
    //--------------------------------------------------------------------------------------
    UnloadTexture(texture);       // Unload texture from VRAM
    UnloadImage(imOrigin);        // Unload image-origin from RAM
    UnloadImage(imCopy);          // Unload image-copy from RAM

    return;
}