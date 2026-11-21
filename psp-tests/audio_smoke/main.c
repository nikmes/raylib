/**********************************************************************************************
*
*   audio_smoke - P9-S1 silent-backend harness
*
*   Calls only the public raylib API. The PSP backend is inside libraylib.
*
**********************************************************************************************/

#include <pspkernel.h>
#include <pspthreadman.h>
#include <stdio.h>

#include <raylib.h>

PSP_MODULE_INFO("audio_smoke", 0, 1, 1);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);

int main(void)
{
    InitWindow(480, 272, "audio smoke (P9-S1)");
    SetTargetFPS(60);

    InitAudioDevice();
    if (!IsAudioDeviceReady()) TraceLog(LOG_ERROR, "AUDIO: device is not ready");

    int frames = 0;
    while (!WindowShouldClose() && (frames < 600))
    {
        BeginDrawing();
        ClearBackground(RAYWHITE);
        DrawText(TextFormat("audio smoke %d  %s", frames,
                            IsAudioDeviceReady()? "ready" : "NOT READY"),
                 10, 10, 20, BLACK);
        DrawFPS(10, 40);
        EndDrawing();

        frames++;
        if ((frames%60) == 0) TraceLog(LOG_INFO, "FRAME %d  fps %d", frames, GetFPS());
    }

    TraceLog(LOG_INFO, "DONE frames %d  audio=%d", frames, IsAudioDeviceReady());

    CloseAudioDevice();

    // Never CloseWindow()/sceKernelExitGame() here: both end the PSPLink session.
    sceKernelSleepThread();
    return 0;
}
