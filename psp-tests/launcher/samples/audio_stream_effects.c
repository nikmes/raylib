/*******************************************************************************************
*
*   raylib [audio] example - stream effects — PSP port (P13-S13)
*
*   Original example licensed under an unmodified zlib/libpng license.
*   Copyright (c) 2022-2025 Ramon Santamaria (@raysan5)
*
*   PSP adaptation: 480x272, bounded 600-frame run, pad controls and
*   deterministic LPF/delay toggles for a headless run.
*
********************************************************************************************/

#include "raylib.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>


#include <stdlib.h>


static float *delayBuffer = NULL;
static unsigned int delayBufferSize = 0;
static unsigned int delayReadIndex = 2;
static unsigned int delayWriteIndex = 0;

static void AudioProcessEffectLPF(void *buffer, unsigned int frames);
static void AudioProcessEffectDelay(void *buffer, unsigned int frames);

void sample_audio_stream_effects(void)
{
    const int screenWidth = 480;
    const int screenHeight = 272;
    InitAudioDevice();

    Music music = LoadMusicStream("resources/country.mp3");

    delayBufferSize = 48000*2;
    delayBuffer = (float *)RL_CALLOC(delayBufferSize, sizeof(float));

    PlayMusicStream(music);

    float timePlayed = 0.0f;
    bool pause = false;

    bool enableEffectLPF = false;
    bool enableEffectDelay = false;

    SetTargetFPS(60);
    //--------------------------------------------------------------------------------------

    for (int frame = 0; !IsGamepadButtonPressed(0, GAMEPAD_BUTTON_MIDDLE_RIGHT) && !WindowShouldClose(); frame++)
    {
        UpdateMusicStream(music);

        if ((frame == 30) || IsGamepadButtonPressed(0, GAMEPAD_BUTTON_RIGHT_FACE_DOWN))
        {
            StopMusicStream(music);
            PlayMusicStream(music);
        }

        if ((frame == 480) || (frame == 540) ||
            IsGamepadButtonPressed(0, GAMEPAD_BUTTON_RIGHT_FACE_RIGHT))
        {
            pause = !pause;

            if (pause) PauseMusicStream(music);
            else ResumeMusicStream(music);
        }

        if ((frame == 120) || (frame == 240) ||
            IsGamepadButtonPressed(0, GAMEPAD_BUTTON_RIGHT_FACE_UP))
        {
            enableEffectLPF = !enableEffectLPF;
            if (enableEffectLPF) AttachAudioStreamProcessor(music.stream, AudioProcessEffectLPF);
            else DetachAudioStreamProcessor(music.stream, AudioProcessEffectLPF);
        }

        if ((frame == 360) || (frame == 480) ||
            IsGamepadButtonPressed(0, GAMEPAD_BUTTON_RIGHT_FACE_LEFT))
        {
            enableEffectDelay = !enableEffectDelay;
            if (enableEffectDelay) AttachAudioStreamProcessor(music.stream, AudioProcessEffectDelay);
            else DetachAudioStreamProcessor(music.stream, AudioProcessEffectDelay);
        }

        timePlayed = GetMusicTimePlayed(music)/GetMusicTimeLength(music);
        if (timePlayed > 1.0f) timePlayed = 1.0f;

        BeginDrawing();

            ClearBackground(RAYWHITE);

            DrawText("P13-S13 audio_stream_effects", 8, 8, 10, DARKGRAY);
            DrawText("MUSIC SHOULD BE PLAYING!", 120, 50, 20, LIGHTGRAY);

            DrawRectangle(80, 90, 320, 12, LIGHTGRAY);
            DrawRectangle(80, 90, (int)(timePlayed*320.0f), 12, MAROON);
            DrawRectangleLines(80, 90, 320, 12, GRAY);

            DrawText("CROSS RESTART   CIRCLE PAUSE/RESUME", 60, 130, 16, LIGHTGRAY);
            DrawText(TextFormat("TRIANGLE LPF: %s", enableEffectLPF? "ON" : "OFF"), 60, 175, 20, GRAY);
            DrawText(TextFormat("SQUARE DELAY: %s", enableEffectDelay? "ON" : "OFF"), 60, 205, 20, GRAY);

        EndDrawing();

        if (((frame + 1)%60) == 0) TraceLog(LOG_INFO, "FRAME %i fps %i", frame + 1, GetFPS());
    }

    // De-Initialization
    //--------------------------------------------------------------------------------------
    if (enableEffectLPF) DetachAudioStreamProcessor(music.stream, AudioProcessEffectLPF);
    if (enableEffectDelay) DetachAudioStreamProcessor(music.stream, AudioProcessEffectDelay);

    UnloadMusicStream(music);

    CloseAudioDevice();

    RL_FREE(delayBuffer);

    return;
}

//------------------------------------------------------------------------------------
// Module Functions Definition
//------------------------------------------------------------------------------------
static void AudioProcessEffectLPF(void *buffer, unsigned int frames)
{
    static float low[2] = { 0.0f, 0.0f };
    static const float cutoff = 70.0f/44100.0f;
    const float k = cutoff/(cutoff + 0.1591549431f);

    float *bufferData = (float *)buffer;
    for (unsigned int i = 0; i < frames*2; i += 2)
    {
        const float l = bufferData[i];
        const float r = bufferData[i + 1];

        low[0] += k*(l - low[0]);
        low[1] += k*(r - low[1]);
        bufferData[i] = low[0];
        bufferData[i + 1] = low[1];
    }
}

static void AudioProcessEffectDelay(void *buffer, unsigned int frames)
{
    for (unsigned int i = 0; i < frames*2; i += 2)
    {
        float leftDelay = delayBuffer[delayReadIndex++];
        float rightDelay = delayBuffer[delayReadIndex++];

        if (delayReadIndex == delayBufferSize) delayReadIndex = 0;

        ((float *)buffer)[i] = 0.5f*((float *)buffer)[i] + 0.5f*leftDelay;
        ((float *)buffer)[i + 1] = 0.5f*((float *)buffer)[i + 1] + 0.5f*rightDelay;

        delayBuffer[delayWriteIndex++] = ((float *)buffer)[i];
        delayBuffer[delayWriteIndex++] = ((float *)buffer)[i + 1];
        if (delayWriteIndex == delayBufferSize) delayWriteIndex = 0;
    }
}
