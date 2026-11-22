/*******************************************************************************************
*
*   raylib [audio] example - stream callback — PSP port (P13-S13)
*
*   Original example licensed under an unmodified zlib/libpng license.
*   Copyright (c) 2026 Dan Hoang (@dan-hoang)
*
*   PSP adaptation: 480x272, bounded 600-frame run, pad controls and a
*   deterministic wave/frequency cycle for a headless run.
*
********************************************************************************************/

#include <pspkernel.h>
#include "raylib.h"

#include <stdlib.h>
#include <math.h>

PSP_MODULE_INFO("audio_stream_callback", 0, 1, 1);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);

#define BUFFER_SIZE 4096
#define SAMPLE_RATE 44100

typedef enum {
    SINE,
    SQUARE,
    TRIANGLE,
    SAWTOOTH
} WaveType;

static void SineCallback(void *framesOut, unsigned int frameCount);
static void SquareCallback(void *framesOut, unsigned int frameCount);
static void TriangleCallback(void *framesOut, unsigned int frameCount);
static void SawtoothCallback(void *framesOut, unsigned int frameCount);

static int waveFrequency = 440;
static int newWaveFrequency = 440;
static int waveIndex = 0;

static float buffer[SAMPLE_RATE] = { 0 };
static AudioCallback waveCallbacks[] = { SineCallback, SquareCallback, TriangleCallback, SawtoothCallback };
static char *waveTypesAsString[] = { "sine", "square", "triangle", "sawtooth" };

int main(void)
{
    const int screenWidth = 480;
    const int screenHeight = 272;

    InitWindow(screenWidth, screenHeight, "raylib [audio] example - stream callback");

    InitAudioDevice();

    SetAudioStreamBufferSizeDefault(BUFFER_SIZE);

    AudioStream stream = LoadAudioStream(SAMPLE_RATE, 32, 1);
    PlayAudioStream(stream);

    WaveType waveType = SINE;
    SetAudioStreamCallback(stream, waveCallbacks[waveType]);

    SetTargetFPS(30);
    //--------------------------------------------------------------------------------------

    for (int frame = 0; frame < 600; frame++)
    {
        if (IsGamepadButtonDown(0, GAMEPAD_BUTTON_LEFT_FACE_UP))
        {
            newWaveFrequency += 10;
            if (newWaveFrequency > 12500) newWaveFrequency = 12500;
        }

        if (IsGamepadButtonDown(0, GAMEPAD_BUTTON_LEFT_FACE_DOWN))
        {
            newWaveFrequency -= 10;
            if (newWaveFrequency < 20) newWaveFrequency = 20;
        }

        if (frame == 240) newWaveFrequency = 660;
        if (frame == 480) newWaveFrequency = 440;

        bool changeWave = ((frame > 0) && ((frame%60) == 0));

        if (IsGamepadButtonPressed(0, GAMEPAD_BUTTON_LEFT_FACE_LEFT))
        {
            if (waveType == SINE) waveType = SAWTOOTH;
            else if (waveType == SQUARE) waveType = SINE;
            else if (waveType == TRIANGLE) waveType = SQUARE;
            else waveType = TRIANGLE;

            SetAudioStreamCallback(stream, waveCallbacks[waveType]);
        }

        if (IsGamepadButtonPressed(0, GAMEPAD_BUTTON_LEFT_FACE_RIGHT) || changeWave)
        {
            if (waveType == SINE) waveType = SQUARE;
            else if (waveType == SQUARE) waveType = TRIANGLE;
            else if (waveType == TRIANGLE) waveType = SAWTOOTH;
            else waveType = SINE;

            SetAudioStreamCallback(stream, waveCallbacks[waveType]);
        }

        BeginDrawing();

            ClearBackground(RAYWHITE);

            DrawText("P13-S13 audio_stream_callback", 8, 8, 10, DARKGRAY);
            DrawText(TextFormat("frequency: %i", newWaveFrequency), 280, 10, 16, RED);
            DrawText(TextFormat("wave type: %s", waveTypesAsString[waveType]), 280, 30, 16, RED);
            DrawText("Up/down frequency", 10, 30, 16, DARKGRAY);
            DrawText("Left/right wave type", 10, 52, 16, DARKGRAY);

            for (int i = 0; i < screenWidth - 1; i++)
            {
                Vector2 startPos = {(float)i, 180 - 40*buffer[SAMPLE_RATE - SAMPLE_RATE/100 + i*SAMPLE_RATE/100/screenWidth] };
                Vector2 endPos = { (float)(i + 1), 180 - 40*buffer[SAMPLE_RATE - SAMPLE_RATE/100 + (i + 1)*SAMPLE_RATE/100/screenWidth] };
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

//------------------------------------------------------------------------------------
// Module Functions Definition
//------------------------------------------------------------------------------------
static void SineCallback(void *framesOut, unsigned int frameCount)
{
    int wavelength = SAMPLE_RATE/waveFrequency;

    for (unsigned int i = 0; i < frameCount; i++)
    {
        ((float *)framesOut)[i] = sinf(2*PI*waveIndex/wavelength);

        waveIndex++;

        if (waveIndex >= wavelength)
        {
            waveFrequency = newWaveFrequency;
            waveIndex = 0;
        }
    }

    for (unsigned int i = 0; i < SAMPLE_RATE - frameCount; i++) buffer[i] = buffer[i + frameCount];
    for (unsigned int i = 0; i < frameCount; i++) buffer[SAMPLE_RATE - frameCount + i] = ((float *)framesOut)[i];
}

static void SquareCallback(void *framesOut, unsigned int frameCount)
{
    int wavelength = SAMPLE_RATE/waveFrequency;

    for (unsigned int i = 0; i < frameCount; i++)
    {
        ((float *)framesOut)[i] = (waveIndex < wavelength/2)? 1.0f : -1.0f;
        waveIndex++;

        if (waveIndex >= wavelength)
        {
            waveFrequency = newWaveFrequency;
            waveIndex = 0;
        }
    }

    for (unsigned int i = 0; i < SAMPLE_RATE - frameCount; i++) buffer[i] = buffer[i + frameCount];
    for (unsigned int i = 0; i < frameCount; i++) buffer[SAMPLE_RATE - frameCount + i] = ((float *)framesOut)[i];
}

static void TriangleCallback(void *framesOut, unsigned int frameCount)
{
    int wavelength = SAMPLE_RATE/waveFrequency;

    for (unsigned int i = 0; i < frameCount; i++)
    {
        ((float *)framesOut)[i] = (waveIndex < wavelength/2)? (-1 + 2.0f*waveIndex/(wavelength/2)) : (1 - 2.0f*(waveIndex - wavelength/2)/(wavelength/2));
        waveIndex++;

        if (waveIndex >= wavelength)
        {
            waveFrequency = newWaveFrequency;
            waveIndex = 0;
        }
    }

    for (unsigned int i = 0; i < SAMPLE_RATE - frameCount; i++) buffer[i] = buffer[i + frameCount];
    for (unsigned int i = 0; i < frameCount; i++) buffer[SAMPLE_RATE - frameCount + i] = ((float *)framesOut)[i];
}

static void SawtoothCallback(void *framesOut, unsigned int frameCount)
{
    int wavelength = SAMPLE_RATE/waveFrequency;

    for (unsigned int i = 0; i < frameCount; i++)
    {
        ((float *)framesOut)[i] = -1 + 2.0f*waveIndex/wavelength;
        waveIndex++;

        if (waveIndex >= wavelength)
        {
            waveFrequency = newWaveFrequency;
            waveIndex = 0;
        }
    }

    for (unsigned int i = 0; i < SAMPLE_RATE - frameCount; i++) buffer[i] = buffer[i + frameCount];
    for (unsigned int i = 0; i < frameCount; i++) buffer[SAMPLE_RATE - frameCount + i] = ((float *)framesOut)[i];
}
