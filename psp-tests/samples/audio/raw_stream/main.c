/*******************************************************************************************
*
*   raylib [audio] example - raw stream — PSP port (P13-S13)
*
*   Original example licensed under an unmodified zlib/libpng license.
*   Copyright (c) 2015-2026 Ramon Santamaria (@raysan5) and James Hofmann (@triplefox)
*
*   PSP adaptation: 480x272, bounded 600-frame run, pad controls and
*   deterministic frequency changes for a headless run.
*
********************************************************************************************/

#include <pspkernel.h>
#include "raylib.h"
#include <math.h>

PSP_MODULE_INFO("audio_raw_stream", 0, 1, 1);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);

#define BUFFER_SIZE 4096
#define SAMPLE_RATE 44100

static float buffer[BUFFER_SIZE] = { 0 };

int main(void)
{
    const int screenWidth = 480;
    const int screenHeight = 272;

    InitWindow(screenWidth, screenHeight, "raylib [audio] example - raw stream");

    InitAudioDevice();

    SetAudioStreamBufferSizeDefault(BUFFER_SIZE);

    AudioStream stream = LoadAudioStream(SAMPLE_RATE, 32, 1);
    float pan = 0.0f;
    SetAudioStreamPan(stream, pan);
    PlayAudioStream(stream);

    int sineFrequency = 440;
    int newSineFrequency = 440;
    int sineIndex = 0;
    double sineStartTime = 0.0;

    SetTargetFPS(30);
    //--------------------------------------------------------------------------------------

    for (int frame = 0; frame < 600; frame++)
    {
        if (IsGamepadButtonDown(0, GAMEPAD_BUTTON_LEFT_FACE_UP))
        {
            newSineFrequency += 10;
            if (newSineFrequency > 12500) newSineFrequency = 12500;
        }

        if (IsGamepadButtonDown(0, GAMEPAD_BUTTON_LEFT_FACE_DOWN))
        {
            newSineFrequency -= 10;
            if (newSineFrequency < 20) newSineFrequency = 20;
        }

        if (frame == 120) newSineFrequency = 660;
        if (frame == 300) newSineFrequency = 330;
        if (frame == 480) newSineFrequency = 440;

        if (IsGamepadButtonDown(0, GAMEPAD_BUTTON_LEFT_FACE_LEFT))
        {
            pan -= 0.01f;
            if (pan < -1.0f) pan = -1.0f;
            SetAudioStreamPan(stream, pan);
        }

        if (IsGamepadButtonDown(0, GAMEPAD_BUTTON_LEFT_FACE_RIGHT))
        {
            pan += 0.01f;
            if (pan > 1.0f) pan = 1.0f;
            SetAudioStreamPan(stream, pan);
        }

        if (IsAudioStreamProcessed(stream))
        {
            for (int i = 0; i < BUFFER_SIZE; i++)
            {
                int wavelength = SAMPLE_RATE/sineFrequency;
                buffer[i] = sinf(2*PI*sineIndex/wavelength);
                sineIndex++;

                if (sineIndex >= wavelength)
                {
                    sineFrequency = newSineFrequency;
                    sineIndex = 0;
                    sineStartTime = GetTime();
                }
            }

            UpdateAudioStream(stream, buffer, BUFFER_SIZE);
        }

        BeginDrawing();
            ClearBackground(RAYWHITE);

            DrawText("P13-S13 audio_raw_stream", 8, 8, 10, DARKGRAY);
            DrawText(TextFormat("sine frequency: %i", sineFrequency), 280, 10, 16, RED);
            DrawText(TextFormat("pan: %.2f", pan), 280, 30, 16, RED);
            DrawText("Up/down to change frequency", 10, 30, 16, DARKGRAY);
            DrawText("Left/right to pan", 10, 52, 16, DARKGRAY);

            int windowStart = (int)((GetTime() - sineStartTime)*SAMPLE_RATE);
            int windowSize = SAMPLE_RATE/10;
            int wavelength = SAMPLE_RATE/sineFrequency;

            for (int i = 0; i < screenWidth; i++)
            {
                int t0 = windowStart + i*windowSize/screenWidth;
                int t1 = windowStart + (i + 1)*windowSize/screenWidth;
                Vector2 startPos = { (float)i, 180 + 40*sinf(2*PI*t0/wavelength) };
                Vector2 endPos = { (float)i + 1, 180 + 40*sinf(2*PI*t1/wavelength) };
                DrawLineV(startPos, endPos, RED);
            }

        EndDrawing();

        if (((frame + 1)%60) == 0) TraceLog(LOG_INFO, "FRAME %i fps %i", frame + 1, GetFPS());
    }

    // De-Initialization
    //--------------------------------------------------------------------------------------
    UnloadAudioStream(stream);
    CloseAudioDevice();

    TraceLog(LOG_INFO, "DONE frames %i", 600);
    sceKernelSleepThread();

    return 0;
}
