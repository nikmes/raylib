/*******************************************************************************************
*
*   raylib [textures] example - image kernel
*
*   Example complexity rating: [★★★★] 4/4
*
*   NOTE: Images are loaded in CPU memory (RAM); textures are loaded in GPU memory (VRAM)
*
*   Example contributed by Karim Salem (@kimo-s) and reviewed by Ramon Santamaria (@raysan5)
*
*   Example originally created with raylib 1.3, last time updated with raylib 1.3
*
*   Example licensed under an unmodified zlib/libpng license, which is an OSI-certified,
*   BSD-like license that allows static linking with closed source software
*
*   Copyright (c) 2015-2025 Karim Salem (@kimo-s)
*
********************************************************************************************/

#include <pspkernel.h>
#include "raylib.h"

PSP_MODULE_INFO("image_kernel", 0, 1, 1);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);

//------------------------------------------------------------------------------------
// Module Functions Declaration
//------------------------------------------------------------------------------------
static void NormalizeKernel(float *kernel, int size);

//------------------------------------------------------------------------------------
// Program main entry point
//------------------------------------------------------------------------------------
int main(void)
{
    // Initialization
    //--------------------------------------------------------------------------------------
    const int screenWidth = 480;
    const int screenHeight = 272;

    InitWindow(screenWidth, screenHeight, "raylib [textures] example - image kernel");

    Image imCat = LoadImage("resources/cat.png"); // Loaded in CPU memory (RAM)

    float gaussiankernel[] = {
        1.0f, 2.0f, 1.0f,
        2.0f, 4.0f, 2.0f,
        1.0f, 2.0f, 1.0f
    };

    float sobelkernel[] = {
        1.0f, 0.0f, -1.0f,
        2.0f, 0.0f, -2.0f,
        1.0f, 0.0f, -1.0f
    };

    float sharpenkernel[] = {
        0.0f, -1.0f, 0.0f,
       -1.0f, 5.0f, -1.0f,
        0.0f, -1.0f, 0.0f
    };

    NormalizeKernel(gaussiankernel, 9);
    NormalizeKernel(sharpenkernel, 9);
    NormalizeKernel(sobelkernel, 9);

    Image imCatSharpend = ImageCopy(imCat);
    ImageKernelConvolution(&imCatSharpend, sharpenkernel, 9);

    Image imCatSobel = ImageCopy(imCat);
    ImageKernelConvolution(&imCatSobel, sobelkernel, 9);

    Image imCatGaussian = ImageCopy(imCat);

    for (int i = 0; i < 6; i++)
    {
        ImageKernelConvolution(&imCatGaussian, gaussiankernel, 9);
    }

    ImageCrop(&imCat, (Rectangle){ 0, 0, (float)200, (float)450 });
    ImageCrop(&imCatGaussian, (Rectangle){ 0, 0, (float)200, (float)450 });
    ImageCrop(&imCatSobel, (Rectangle){ 0, 0, (float)200, (float)450 });
    ImageCrop(&imCatSharpend, (Rectangle){ 0, 0, (float)200, (float)450 });

    // Images converted to texture, GPU memory (VRAM)
    Texture2D texCat = LoadTextureFromImage(imCat);
    Texture2D texCatSharpend = LoadTextureFromImage(imCatSharpend);
    Texture2D texCatSobel = LoadTextureFromImage(imCatSobel);
    Texture2D texCatGaussian = LoadTextureFromImage(imCatGaussian);

    // Once images have been converted to texture and uploaded to VRAM,
    // they can be unloaded from RAM
    UnloadImage(imCat);
    UnloadImage(imCatGaussian);
    UnloadImage(imCatSobel);
    UnloadImage(imCatSharpend);

    SetTargetFPS(60);     // Set our game to run at 60 frames-per-second
    //---------------------------------------------------------------------------------------

    // Main game loop
    for (int frames = 0; frames < 600; frames++)    // Detect window close button or ESC key
    {
        // Update
        //----------------------------------------------------------------------------------
        // TODO: Update your variables here
        //----------------------------------------------------------------------------------

        // Draw
        //----------------------------------------------------------------------------------
        BeginDrawing();

            ClearBackground(RAYWHITE);

            DrawTextureEx(texCatSharpend, (Vector2){ 0, 10 }, 0.0f, 0.5f, WHITE);
            DrawTextureEx(texCatSobel, (Vector2){ 120, 10 }, 0.0f, 0.5f, WHITE);
            DrawTextureEx(texCatGaussian, (Vector2){ 240, 10 }, 0.0f, 0.5f, WHITE);
            DrawTextureEx(texCat, (Vector2){ 360, 10 }, 0.0f, 0.5f, WHITE);

        EndDrawing();


        if (((frames + 1)%60) == 0) TraceLog(LOG_INFO, "FRAME %i fps %i", frames + 1, GetFPS());
        //----------------------------------------------------------------------------------
    }

    // De-Initialization
    //--------------------------------------------------------------------------------------
    UnloadTexture(texCat);
    UnloadTexture(texCatGaussian);
    UnloadTexture(texCatSobel);
    UnloadTexture(texCatSharpend);

    TraceLog(LOG_INFO, "DONE frames %i", 600);
    sceKernelSleepThread();

    return 0;
}

//------------------------------------------------------------------------------------
// Module Functions Definition
//------------------------------------------------------------------------------------
static void NormalizeKernel(float *kernel, int size)
{
    float sum = 0.0f;
    for (int i = 0; i < size; i++) sum += kernel[i];

    if (sum != 0.0f)
    {
        for (int i = 0; i < size; i++) kernel[i] /= sum;
    }
}
