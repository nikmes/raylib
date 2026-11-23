/*******************************************************************************************
*
*   raylib [audio] example - music stream — PSP port (P13-S12)
*
*   Original example licensed under an unmodified zlib/libpng license.
*   Copyright (c) 2015-2025 Ramon Santamaria (@raysan5)
*
*   PSP adaptation: 480x272, bounded 600-frame run, pad controls and a
*   deterministic restart/pause so a headless run exercises the stream.
*
********************************************************************************************/

#include <pspkernel.h>
#include "raylib.h"

PSP_MODULE_INFO("audio_music_stream", 0, 1, 1);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);

int main(void)
{
    const int screenWidth = 480;
    const int screenHeight = 272;

    InitWindow(screenWidth, screenHeight, "raylib [audio] example - music stream");

    InitAudioDevice();

    Music music = LoadMusicStream("resources/country.mp3");

    PlayMusicStream(music);

    float timePlayed = 0.0f;
    bool pause = false;

    float pan = 0.0f;
    SetMusicPan(music, pan);

    float volume = 0.8f;
    SetMusicVolume(music, volume);

    SetTargetFPS(60);
    //--------------------------------------------------------------------------------------

    for (int frame = 0; frame < 600; frame++)
    {
        UpdateMusicStream(music);

        // CROSS restarts; frame 30 guarantees a restart in a headless run.
        if ((frame == 30) || IsGamepadButtonPressed(0, GAMEPAD_BUTTON_RIGHT_FACE_DOWN))
        {
            StopMusicStream(music);
            PlayMusicStream(music);
        }

        // CIRCLE pauses/resumes; the two frame triggers toggle it twice.
        if ((frame == 120) || (frame == 300) ||
            IsGamepadButtonPressed(0, GAMEPAD_BUTTON_RIGHT_FACE_RIGHT))
        {
            pause = !pause;

            if (pause) PauseMusicStream(music);
            else ResumeMusicStream(music);
        }

        if (IsGamepadButtonDown(0, GAMEPAD_BUTTON_LEFT_FACE_LEFT))
        {
            pan -= 0.05f;
            if (pan < -1.0f) pan = -1.0f;
            SetMusicPan(music, pan);
        }
        else if (IsGamepadButtonDown(0, GAMEPAD_BUTTON_LEFT_FACE_RIGHT))
        {
            pan += 0.05f;
            if (pan > 1.0f) pan = 1.0f;
            SetMusicPan(music, pan);
        }

        if (IsGamepadButtonDown(0, GAMEPAD_BUTTON_LEFT_FACE_DOWN))
        {
            volume -= 0.05f;
            if (volume < 0.0f) volume = 0.0f;
            SetMusicVolume(music, volume);
        }
        else if (IsGamepadButtonDown(0, GAMEPAD_BUTTON_LEFT_FACE_UP))
        {
            volume += 0.05f;
            if (volume > 1.0f) volume = 1.0f;
            SetMusicVolume(music, volume);
        }

        timePlayed = GetMusicTimePlayed(music)/GetMusicTimeLength(music);
        if (timePlayed > 1.0f) timePlayed = 1.0f;

        BeginDrawing();

            ClearBackground(RAYWHITE);

            DrawText("P13-S12 audio_music_stream", 8, 8, 10, DARKGRAY);
            DrawText("MUSIC SHOULD BE PLAYING!", 120, 50, 20, LIGHTGRAY);

            DrawText("LEFT-RIGHT for PAN CONTROL", 150, 80, 10, DARKBLUE);
            DrawRectangle(140, 95, 200, 12, LIGHTGRAY);
            DrawRectangleLines(140, 95, 200, 12, GRAY);
            DrawRectangle((int)(140 + (pan + 1.0f)/2.0f*200 - 5), 87, 10, 28, DARKGRAY);

            DrawRectangle(80, 135, 320, 12, LIGHTGRAY);
            DrawRectangle(80, 135, (int)(timePlayed*320.0f), 12, MAROON);
            DrawRectangleLines(80, 135, 320, 12, GRAY);

            DrawText("CROSS RESTART   CIRCLE PAUSE/RESUME", 70, 175, 20, LIGHTGRAY);

            DrawText("UP-DOWN for VOLUME CONTROL", 150, 215, 10, DARKGREEN);
            DrawRectangle(140, 232, 200, 12, LIGHTGRAY);
            DrawRectangleLines(140, 232, 200, 12, GRAY);
            DrawRectangle((int)(140 + volume*200 - 5), 224, 10, 28, DARKGRAY);

        EndDrawing();

        if (((frame + 1)%60) == 0) TraceLog(LOG_INFO, "FRAME %i fps %i", frame + 1, GetFPS());
    }

    // De-Initialization
    //--------------------------------------------------------------------------------------
    UnloadMusicStream(music);

    CloseAudioDevice();

    TraceLog(LOG_INFO, "DONE frames %i", 600);
    sceKernelSleepThread();

    return 0;
}
