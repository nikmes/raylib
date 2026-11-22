/*******************************************************************************************
*
*   raylib [audio] example - mixed processor — PSP port (P13-S12)
*
*   Original example licensed under an unmodified zlib/libpng license.
*   Copyright (c) 2023-2025 hkc (@hatkidchan)
*
*   PSP adaptation: 480x272, bounded 600-frame run, pad controls and
*   deterministic exponent/coin events for a headless run.
*
********************************************************************************************/

#include "raylib.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <math.h>


static float exponent = 1.0f;
static float averageVolume[400] = { 0.0f };

//------------------------------------------------------------------------------------
// Audio processing function
//------------------------------------------------------------------------------------
void ProcessAudio(void *buffer, unsigned int frames)
{
    float *samples = (float *)buffer;
    float average = 0.0f;

    for (unsigned int frame = 0; frame < frames; frame++)
    {
        float *left = &samples[frame*2 + 0], *right = &samples[frame*2 + 1];

        *left = powf(fabsf(*left), exponent)*( (*left < 0.0f)? -1.0f : 1.0f );
        *right = powf(fabsf(*right), exponent)*( (*right < 0.0f)? -1.0f : 1.0f );

        average += fabsf(*left)/frames;
        average += fabsf(*right)/frames;
    }

    for (int i = 0; i < 399; i++) averageVolume[i] = averageVolume[i + 1];

    averageVolume[399] = average;
}

void sample_audio_mixed_processor(void)
{
    const int screenWidth = 480;
    const int screenHeight = 272;
    InitAudioDevice();

    AttachAudioMixedProcessor(ProcessAudio);

    Music music = LoadMusicStream("resources/country.mp3");
    Sound sound = LoadSound("resources/coin.wav");

    PlayMusicStream(music);

    SetTargetFPS(60);
    //--------------------------------------------------------------------------------------

    for (int frame = 0; !IsGamepadButtonPressed(0, GAMEPAD_BUTTON_MIDDLE_RIGHT); frame++)
    {
        UpdateMusicStream(music);

        // D-pad adjusts distortion; the frame events walk the range so the
        // headless capture shows the effect responding.
        if (IsGamepadButtonPressed(0, GAMEPAD_BUTTON_LEFT_FACE_LEFT)) exponent -= 0.05f;
        if (IsGamepadButtonPressed(0, GAMEPAD_BUTTON_LEFT_FACE_RIGHT)) exponent += 0.05f;

        if (frame == 120) exponent = 0.8f;
        if (frame == 300) exponent = 1.6f;
        if (frame == 480) exponent = 1.0f;

        if (exponent <= 0.5f) exponent = 0.5f;
        if (exponent >= 3.0f) exponent = 3.0f;

        if ((frame == 60) || (frame == 240) ||
            IsGamepadButtonPressed(0, GAMEPAD_BUTTON_RIGHT_FACE_DOWN)) PlaySound(sound);

        BeginDrawing();

            ClearBackground(RAYWHITE);

            DrawText("P13-S12 audio_mixed_processor", 8, 8, 10, DARKGRAY);
            DrawText("MUSIC SHOULD BE PLAYING!", 120, 45, 20, LIGHTGRAY);
            DrawText(TextFormat("EXPONENT = %.2f", exponent), 160, 75, 20, LIGHTGRAY);

            DrawRectangle(79, 109, 322, 34, LIGHTGRAY);
            for (int i = 0; i < 400; i++)
            {
                int x = 81 + i*320/400;
                DrawLine(x, 140 - (int)(averageVolume[i]*32), x, 140, MAROON);
            }
            DrawRectangleLines(79, 109, 322, 34, GRAY);

            DrawText("CROSS PLAY OTHER SOUND", 110, 165, 20, LIGHTGRAY);
            DrawText("D-pad LEFT/RIGHT ALTER DISTORTION", 45, 200, 20, LIGHTGRAY);

        EndDrawing();

        if (((frame + 1)%60) == 0) TraceLog(LOG_INFO, "FRAME %i fps %i", frame + 1, GetFPS());
    }

    // De-Initialization
    //--------------------------------------------------------------------------------------
    UnloadMusicStream(music);
    UnloadSound(sound);

    DetachAudioMixedProcessor(ProcessAudio);

    CloseAudioDevice();

    return;
}
