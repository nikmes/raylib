/*******************************************************************************************
*
*   raylib [audio] example - sound multi — PSP port (P13-S12)
*
*   Original example licensed under an unmodified zlib/libpng license.
*   Copyright (c) 2023-2025 Jeffery Myers (@JeffM2501)
*
*   PSP adaptation: 480x272, bounded 600-frame run, pad trigger and a
*   deterministic auto-play that cycles all ten aliases.
*
********************************************************************************************/

#include "raylib.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>



#define MAX_SOUNDS 10

void sample_audio_sound_multi(void)
{
    const int screenWidth = 480;
    const int screenHeight = 272;
    InitAudioDevice();

    Sound sounds[MAX_SOUNDS] = { 0 };
    int currentSound = 0;

    sounds[0] = LoadSound("resources/sound.wav");

    for (int i = 1; i < MAX_SOUNDS; i++) sounds[i] = LoadSoundAlias(sounds[0]);

    SetTargetFPS(60);
    //--------------------------------------------------------------------------------------

    for (int frame = 0; !IsGamepadButtonPressed(0, GAMEPAD_BUTTON_MIDDLE_RIGHT); frame++)
    {
        // One alias every 60 frames exercises all ten; CROSS plays the next slot.
        if (((frame%60) == 30) || IsGamepadButtonPressed(0, GAMEPAD_BUTTON_RIGHT_FACE_DOWN))
        {
            PlaySound(sounds[currentSound]);
            currentSound++;

            if (currentSound >= MAX_SOUNDS) currentSound = 0;
        }

        BeginDrawing();

            ClearBackground(RAYWHITE);

            DrawText("P13-S12 audio_sound_multi", 8, 8, 10, DARKGRAY);
            DrawText("Press CROSS (SPACE) to PLAY a WAV sound!", 45, 110, 20, LIGHTGRAY);
            DrawText(TextFormat("alias slot: %i", currentSound), 45, 145, 20, DARKGRAY);

        EndDrawing();

        if (((frame + 1)%60) == 0) TraceLog(LOG_INFO, "FRAME %i fps %i", frame + 1, GetFPS());
    }

    // De-Initialization
    //--------------------------------------------------------------------------------------
    for (int i = 1; i < MAX_SOUNDS; i++) UnloadSoundAlias(sounds[i]);
    UnloadSound(sounds[0]);

    CloseAudioDevice();

    return;
}
