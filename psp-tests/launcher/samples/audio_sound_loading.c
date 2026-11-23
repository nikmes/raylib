/*******************************************************************************************
*
*   raylib [audio] example - sound loading — PSP port (P13-S12)
*
*   Original example licensed under an unmodified zlib/libpng license.
*   Copyright (c) 2014-2025 Ramon Santamaria (@raysan5)
*
*   PSP adaptation: 480x272, bounded 600-frame run, pad triggers and a
*   deterministic auto-play so a headless capture exercises both sounds.
*
********************************************************************************************/

#include "raylib.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>



void sample_audio_sound_loading(void)
{
    const int screenWidth = 480;
    const int screenHeight = 272;
    InitAudioDevice();      // Initialize audio device

    Sound fxWav = LoadSound("resources/sound.wav");         // Load WAV audio file
    Sound fxOgg = LoadSound("resources/target.ogg");        // Load OGG audio file

    SetTargetFPS(60);
    //--------------------------------------------------------------------------------------

    for (int frame = 0; !IsGamepadButtonPressed(0, GAMEPAD_BUTTON_MIDDLE_RIGHT) && !WindowShouldClose(); frame++)
    {
        // CROSS plays the WAV, SQUARE the OGG. Frame 30/330 guarantee both are
        // exercised in a headless run.
        if ((frame == 30) || IsGamepadButtonPressed(0, GAMEPAD_BUTTON_RIGHT_FACE_DOWN)) PlaySound(fxWav);
        if ((frame == 330) || IsGamepadButtonPressed(0, GAMEPAD_BUTTON_RIGHT_FACE_LEFT)) PlaySound(fxOgg);

        BeginDrawing();

            ClearBackground(RAYWHITE);

            DrawText("P13-S12 audio_sound_loading", 8, 8, 10, DARKGRAY);
            DrawText("Press CROSS (SPACE) to PLAY the WAV sound!", 40, 110, 20, LIGHTGRAY);
            DrawText("Press SQUARE (ENTER) to PLAY the OGG sound!", 30, 150, 20, LIGHTGRAY);

        EndDrawing();

        if (((frame + 1)%60) == 0) TraceLog(LOG_INFO, "FRAME %i fps %i", frame + 1, GetFPS());
    }

    // De-Initialization
    //--------------------------------------------------------------------------------------
    UnloadSound(fxWav);
    UnloadSound(fxOgg);

    CloseAudioDevice();

    return;
}
