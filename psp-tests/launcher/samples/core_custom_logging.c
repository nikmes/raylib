/*******************************************************************************************
*
*   raylib [core] example - custom logging
*
*   Example complexity rating: [★★★☆] 3/4
*
*   Example originally created with raylib 2.5, last time updated with raylib 2.5
*
*   Example contributed by Pablo Marcos Oltra (@pamarcos) and reviewed by Ramon Santamaria (@raysan5)
*
*   Example licensed under an unmodified zlib/libpng license, which is an OSI-certified,
*   BSD-like license that allows static linking with closed source software
*
*   Copyright (c) 2018-2025 Pablo Marcos Oltra (@pamarcos) and Ramon Santamaria (@raysan5)
*
********************************************************************************************/

#include "raylib.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>



#include <stdio.h>          // Required for: printf(), vprintf(), fprintf()
#include <time.h>           // Required for: time_t, tm, time(), localtime(), strftime()

// Custom logging function
void CustomTraceLog(int msgType, const char *text, va_list args)
{
    char timeStr[64] = { 0 };
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);

    if (tm_info != NULL) strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", tm_info);
    printf("[%s] ", timeStr);

    switch (msgType)
    {
        case LOG_INFO: printf("[INFO] : "); break;
        case LOG_ERROR: printf("[ERROR]: "); break;
        case LOG_WARNING: printf("[WARN] : "); break;
        case LOG_DEBUG: printf("[DEBUG]: "); break;
        default: break;
    }

    vprintf(text, args);
    printf("\n");
    fflush(stdout);         // PSPLink reads stdout over USB; flush each line
}

//------------------------------------------------------------------------------------
// Program main entry point
//------------------------------------------------------------------------------------
void sample_core_custom_logging(void)
{
    // Initialization
    //--------------------------------------------------------------------------------------
    const int screenWidth = 480;
    const int screenHeight = 272;
    // The PSP platform installs its own log callback during InitWindow, so ours goes after it
    SetTraceLogCallback(CustomTraceLog);

    SetTargetFPS(60);               // Set our game to run at 60 frames-per-second
    //--------------------------------------------------------------------------------------

    // Main game loop
    for (int frames = 0; !IsGamepadButtonPressed(0, GAMEPAD_BUTTON_MIDDLE_RIGHT) && !WindowShouldClose(); frames++)    // Detect window close button or ESC key
    {
        // Update
        //----------------------------------------------------------------------------------
        // TODO: Update your variables here
        //----------------------------------------------------------------------------------

        // Draw
        //----------------------------------------------------------------------------------
        BeginDrawing();

        ClearBackground(RAYWHITE);

        const char *line1 = "Check out the console output";
        const char *line2 = "to see the custom logger in action!";
        DrawText(line1, (screenWidth - MeasureText(line1, 10))/2, screenHeight/2 - 10, 10, LIGHTGRAY);
        DrawText(line2, (screenWidth - MeasureText(line2, 10))/2, screenHeight/2 + 4, 10, LIGHTGRAY);

        EndDrawing();


        if (((frames + 1)%60) == 0) TraceLog(LOG_INFO, "FRAME %i fps %i", frames + 1, GetFPS());
        //----------------------------------------------------------------------------------
    }

    // De-Initialization
    //--------------------------------------------------------------------------------------

    return;
}
