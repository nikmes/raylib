/*******************************************************************************************
*
*   raylib [audio] example - amp envelope — PSP port (P13-S13)
*
*   Original example licensed under an unmodified zlib/libpng license.
*   Copyright (c) 2026 Arbinda Rizki Muhammad (@arbipink)
*
*   PSP adaptation: 480x272, bounded 600-frame run, CROSS trigger plus a
*   deterministic attack/release so a headless run plays a full envelope.
*
********************************************************************************************/

#include "raylib.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "raygui.h"

#include <math.h>


#define BUFFER_SIZE      4096
#define SAMPLE_RATE     44100

typedef enum {
    IDLE,
    ATTACK,
    DECAY,
    SUSTAIN,
    RELEASE
} ADSRState;

typedef struct {
    float attackTime;
    float decayTime;
    float sustainLevel;
    float releaseTime;
    float currentValue;
    ADSRState state;
} Envelope;

static float buffer[BUFFER_SIZE] = { 0 };

static void FillAudioBuffer(int i, float *buffer, float envelopeValue, float *audioTime);
static void UpdateEnvelope(Envelope *env);
static void DrawADSRGraph(Envelope *env, Rectangle bounds);

void sample_audio_amp_envelope(void)
{
    const int screenWidth = 480;
    const int screenHeight = 272;
    InitAudioDevice();

    SetAudioStreamBufferSizeDefault(BUFFER_SIZE);

    AudioStream stream = LoadAudioStream(SAMPLE_RATE, 32, 1);

    float audioTime = 0.0f;

    Envelope env = {
        .attackTime = 1.0f,
        .decayTime = 1.0f,
        .sustainLevel = 0.5f,
        .releaseTime = 1.0f,
        .currentValue = 0.0f,
        .state = IDLE
    };

    SetTargetFPS(60);
    //--------------------------------------------------------------------------------------

    for (int frame = 0; !IsGamepadButtonPressed(0, GAMEPAD_BUTTON_MIDDLE_RIGHT) && !WindowShouldClose(); frame++)
    {
        if ((frame == 60) || IsGamepadButtonPressed(0, GAMEPAD_BUTTON_RIGHT_FACE_DOWN)) env.state = ATTACK;

        if (((frame == 240) && (env.state != IDLE)) ||
            IsGamepadButtonReleased(0, GAMEPAD_BUTTON_RIGHT_FACE_DOWN)) env.state = RELEASE;

        if (IsAudioStreamProcessed(stream))
        {
            if ((env.state != IDLE) || (env.currentValue > 0.0f))
            {
                for (int i = 0; i < BUFFER_SIZE; i++)
                {
                    UpdateEnvelope(&env);
                    FillAudioBuffer(i, buffer, env.currentValue, &audioTime);
                }
            }
            else
            {
                for (int i = 0; i < BUFFER_SIZE; i++) buffer[i] = 0;
                audioTime = 0.0f;
            }

            UpdateAudioStream(stream, buffer, BUFFER_SIZE);
        }

        if (!IsAudioStreamPlaying(stream)) PlayAudioStream(stream);

        BeginDrawing();

            ClearBackground(RAYWHITE);

            DrawText("P13-S13 audio_amp_envelope", 8, 8, 10, DARKGRAY);

            GuiSliderBar((Rectangle){ 60, 40, 300, 20 }, "Attack (s)", TextFormat("%2.2fs", env.attackTime), &env.attackTime, 0.1f, 3.0f);
            GuiSliderBar((Rectangle){ 60, 70, 300, 20 }, "Decay (s)", TextFormat("%2.2fs", env.decayTime), &env.decayTime, 0.1f, 3.0f);
            GuiSliderBar((Rectangle){ 60, 100, 300, 20 }, "Sustain", TextFormat("%2.2f", env.sustainLevel), &env.sustainLevel, 0.0f, 1.0f);
            GuiSliderBar((Rectangle){ 60, 130, 300, 20 }, "Release (s)", TextFormat("%2.2fs", env.releaseTime), &env.releaseTime, 0.1f, 3.0f);

            DrawADSRGraph(&env, (Rectangle){ 60, 185, 300, 50 });

            DrawCircleV((Vector2){ 390, 215 - (env.currentValue*40) }, 4, MAROON);
            DrawText(TextFormat("Gain: %2.2f", env.currentValue), 400, 205 - (env.currentValue*40), 10, MAROON);

            DrawText("Press CROSS to PLAY the sound!", 80, 245, 16, LIGHTGRAY);

        EndDrawing();

        if (((frame + 1)%60) == 0) TraceLog(LOG_INFO, "FRAME %i fps %i", frame + 1, GetFPS());
    }

    // De-Initialization
    //--------------------------------------------------------------------------------------
    UnloadAudioStream(stream);
    CloseAudioDevice();

    return;
}

//------------------------------------------------------------------------------------
// Module Functions Definition
//------------------------------------------------------------------------------------
static void FillAudioBuffer(int i, float *buffer, float envelopeValue, float *audioTime)
{
    int frequency = 440;
    buffer[i] = envelopeValue*sinf(2.0f*PI*frequency*(*audioTime));
    *audioTime += (1.0f/SAMPLE_RATE);
}

static void UpdateEnvelope(Envelope *env)
{
    float sampleTime = 1.0f/SAMPLE_RATE;

    switch(env->state)
    {
        case ATTACK:
        {
            env->currentValue += (1.0f/env->attackTime)*sampleTime;
            if (env->currentValue >= 1.0f)
            {
                env->currentValue = 1.0f;
                env->state = DECAY;
            }
        } break;
        case DECAY:
        {
            env->currentValue -= ((1.0f - env->sustainLevel)/env->decayTime)*sampleTime;
            if (env->currentValue <= env->sustainLevel)
            {
                env->currentValue = env->sustainLevel;
                env->state = SUSTAIN;
            }
        } break;
        case SUSTAIN:
        {
            env->currentValue = env->sustainLevel;
        } break;
        case RELEASE:
        {
            env->currentValue -= (env->sustainLevel/env->releaseTime)*sampleTime;
            if (env->currentValue <= 0.001f)
            {
                env->currentValue = 0.0f;
                env->state = IDLE;
            }
        } break;
        default: break;
    }
}

static void DrawADSRGraph(Envelope *env, Rectangle bounds)
{
    DrawRectangleRec(bounds, Fade(LIGHTGRAY, 0.3f));
    DrawRectangleLinesEx(bounds, 1, GRAY);

    float sustainWidth = 1.0f;
    float totalTime = env->attackTime + env->decayTime + sustainWidth + env->releaseTime;

    float scaleX = bounds.width/totalTime;
    float scaleY = bounds.height;

    Vector2 start = { bounds.x, bounds.y + bounds.height };
    Vector2 peak = { start.x + (env->attackTime*scaleX), bounds.y };
    Vector2 sustain = { peak.x + (env->decayTime*scaleX), bounds.y + (1.0f - env->sustainLevel)*scaleY };
    Vector2 rel = { sustain.x + (sustainWidth*scaleX), sustain.y };
    Vector2 end = { rel.x + (env->releaseTime*scaleX), bounds.y + bounds.height };

    DrawLineV(start, peak, SKYBLUE);
    DrawLineV(peak, sustain, BLUE);
    DrawLineV(sustain, rel, DARKBLUE);
    DrawLineV(rel, end, ORANGE);

    DrawText("ADSR Visualizer", bounds.x, bounds.y - 15, 10, DARKGRAY);
}
