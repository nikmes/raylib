/* P9-S8 listening demo: one clean, loud MP3, beginning to end, looped twice.
 *
 * Isolates the MP3 feeder path: no processors, no effects, full volume, minimal
 * drawing. The log goes to host0 because a multi-minute run outlives PSPLink's
 * SIO buffer.
 */

#include <pspkernel.h>
#include <stdio.h>

#include <raylib.h>

PSP_MODULE_INFO("audio_demo", 0, 1, 1);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);

#ifndef DEMO_MUSIC_PATH
    #define DEMO_MUSIC_PATH "/country.mp3"
#endif
#ifndef DEMO_LOOPS
    #define DEMO_LOOPS 2
#endif
#define DEMO_MP3   "host0:" DEMO_MUSIC_PATH
#define DEMO_FRAMES (60*300)   // hard cap: 5 minutes

int main(void)
{
    if (freopen("host0:/audio_demo.log", "w", stdout) == NULL) { }
    setvbuf(stdout, NULL, _IOLBF, 0);

    InitWindow(480, 272, "audio demo (P9-S8)");
    SetTargetFPS(60);

    InitAudioDevice();
    if (!IsAudioDeviceReady()) TraceLog(LOG_ERROR, "DEMO audio device not ready");

    SetMasterVolume(1.0f);

    Music music = LoadMusicStream(DEMO_MP3);
    float length = GetMusicTimeLength(music);
    TraceLog(LOG_INFO, "DEMO valid=%d length=%.2f volume=1.00", IsMusicValid(music), length);

    if (!IsMusicValid(music))
    {
        TraceLog(LOG_ERROR, "DEMO cannot load %s", DEMO_MP3);
        CloseAudioDevice();
        sceKernelSleepThread();
        return 0;
    }

    SetMusicVolume(music, 1.0f);
    PlayMusicStream(music);

    int loops = 0;
    int frame = 0;
    float previous = 0.0f;

    for (frame = 0; (frame < DEMO_FRAMES) && (loops < DEMO_LOOPS); frame++)
    {
        UpdateMusicStream(music);

        float played = GetMusicTimePlayed(music);
        if ((frame > 30) && (played + 0.5f < previous))
        {
            loops++;
            TraceLog(LOG_INFO, "DEMO LOOP %d at t=%.2f", loops, played);
        }
        previous = played;

        BeginDrawing();
        ClearBackground(RAYWHITE);
        DrawText(TextFormat("audio demo  loop %d/%d", loops, DEMO_LOOPS), 10, 10, 20, BLACK);
        DrawText(TextFormat("%.2f / %.2f s", played, length), 10, 40, 20, DARKGRAY);
        DrawFPS(10, 70);
        EndDrawing();

        if ((frame > 0) && ((frame%60) == 0))
        {
            TraceLog(LOG_INFO, "DEMO FRAME %d fps %d t=%.2f loop=%d", frame, GetFPS(), played, loops);
        }
    }

    TraceLog(LOG_INFO, "DEMO DONE loops=%d frames=%d length=%.2f", loops, frame, length);

    StopMusicStream(music);
    UnloadMusicStream(music);
    CloseAudioDevice();

    // Never CloseWindow()/sceKernelExitGame(): both end the PSPLink session.
    sceKernelSleepThread();
    return 0;
}
