/**********************************************************************************************
*
*   audio_smoke - P9-S2 output harness
*
*   Generates a deterministic 1 kHz, 1-second, 16-bit stereo sine and plays it through
*   the public raylib API. The PSP backend is inside libraylib.
*
**********************************************************************************************/

#include <math.h>
#include <pspkernel.h>
#include <pspthreadman.h>
#include <stdio.h>

#include <raylib.h>

PSP_MODULE_INFO("audio_smoke", 0, 1, 1);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);

int main(void)
{
    InitWindow(480, 272, "audio smoke (P9-S2)");
    SetTargetFPS(60);

    InitAudioDevice();
    if (!IsAudioDeviceReady()) TraceLog(LOG_ERROR, "AUDIO: device is not ready");

    const int sampleRate = 44100;
    const int sampleFrames = sampleRate;
    short *samples = (short *)MemAlloc(sampleFrames*2*sizeof(short));

    for (int i = 0; i < sampleFrames; i++)
    {
        float value = sinf(2.0f*3.14159265f*1000.0f*((float)i/(float)sampleRate))*0.20f;
        short sample = (short)(value*32767.0f);
        samples[i*2 + 0] = sample;
        samples[i*2 + 1] = sample;
    }

    Wave wave = { 0 };
    wave.frameCount = sampleFrames;
    wave.sampleRate = sampleRate;
    wave.sampleSize = 16;
    wave.channels = 2;
    wave.data = samples;

    Sound tone = LoadSoundFromWave(wave);
    MemFree(samples);

    TraceLog(LOG_INFO, "AUDIO: tone valid=%d", IsSoundValid(tone));
    PlaySound(tone);

    int frames = 0;
    while (!WindowShouldClose() && (frames < 600))
    {
        BeginDrawing();
        ClearBackground(RAYWHITE);
        DrawText(TextFormat("audio smoke %d  %s  sound=%s", frames,
                            IsAudioDeviceReady()? "ready" : "NOT READY",
                            IsSoundPlaying(tone)? "on" : "off"),
                 10, 10, 20, BLACK);
        DrawFPS(10, 40);
        EndDrawing();

        frames++;
        if ((frames%60) == 0) TraceLog(LOG_INFO, "FRAME %d  fps %d  sound=%d", frames, GetFPS(), IsSoundPlaying(tone));
    }

    TraceLog(LOG_INFO, "DONE frames %d  audio=%d  sound_playing=%d", frames, IsAudioDeviceReady(), IsSoundPlaying(tone));

    UnloadSound(tone);
    CloseAudioDevice();

    // Never CloseWindow()/sceKernelExitGame() here: both end the PSPLink session.
    sceKernelSleepThread();
    return 0;
}
