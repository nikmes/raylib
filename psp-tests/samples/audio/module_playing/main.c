/*******************************************************************************************
*
*   raylib [audio] example - module playing — PSP port (P13-S12)
*
*   Original example licensed under an unmodified zlib/libpng license.
*   Copyright (c) 2016-2025 Ramon Santamaria (@raysan5)
*
*   PSP adaptation: 480x272, bounded 600-frame run, pad controls and
*   deterministic pitch/pause events for a headless run.
*
********************************************************************************************/

#include <pspkernel.h>
#include "raylib.h"

PSP_MODULE_INFO("audio_module_playing", 0, 1, 1);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);

#define MAX_CIRCLES  64

typedef struct {
    Vector2 position;
    float radius;
    float alpha;
    float speed;
    Color color;
} CircleWave;

int main(void)
{
    const int screenWidth = 480;
    const int screenHeight = 272;

    InitWindow(screenWidth, screenHeight, "raylib [audio] example - module playing");

    InitAudioDevice();

    Color colors[14] = { ORANGE, RED, GOLD, LIME, BLUE, VIOLET, BROWN, LIGHTGRAY, PINK,
                         YELLOW, GREEN, SKYBLUE, PURPLE, BEIGE };

    CircleWave circles[MAX_CIRCLES] = { 0 };

    for (int i = MAX_CIRCLES - 1; i >= 0; i--)
    {
        circles[i].alpha = 0.0f;
        circles[i].radius = (float)GetRandomValue(10, 40);
        circles[i].position.x = (float)GetRandomValue((int)circles[i].radius, (int)(screenWidth - circles[i].radius));
        circles[i].position.y = (float)GetRandomValue((int)circles[i].radius, (int)(screenHeight - circles[i].radius));
        circles[i].speed = (float)GetRandomValue(1, 100)/2000.0f;
        circles[i].color = colors[GetRandomValue(0, 13)];
    }

    Music music = LoadMusicStream("resources/mini1111.xm");
    music.looping = false;
    float pitch = 1.0f;

    PlayMusicStream(music);

    float timePlayed = 0.0f;
    bool pause = false;

    SetTargetFPS(60);
    //--------------------------------------------------------------------------------------

    for (int frame = 0; frame < 600; frame++)
    {
        UpdateMusicStream(music);

        if ((frame == 30) || IsGamepadButtonPressed(0, GAMEPAD_BUTTON_RIGHT_FACE_DOWN))
        {
            StopMusicStream(music);
            PlayMusicStream(music);
            pause = false;
        }

        if ((frame == 300) || (frame == 420) ||
            IsGamepadButtonPressed(0, GAMEPAD_BUTTON_RIGHT_FACE_RIGHT))
        {
            pause = !pause;

            if (pause) PauseMusicStream(music);
            else ResumeMusicStream(music);
        }

        if (frame == 180) pitch = 1.15f;
        if (frame == 480) pitch = 1.0f;

        if (IsGamepadButtonDown(0, GAMEPAD_BUTTON_LEFT_FACE_DOWN)) pitch -= 0.01f;
        else if (IsGamepadButtonDown(0, GAMEPAD_BUTTON_LEFT_FACE_UP)) pitch += 0.01f;

        SetMusicPitch(music, pitch);

        timePlayed = GetMusicTimePlayed(music)/GetMusicTimeLength(music)*(screenWidth - 40);

        for (int i = MAX_CIRCLES - 1; (i >= 0) && !pause; i--)
        {
            circles[i].alpha += circles[i].speed;
            circles[i].radius += circles[i].speed*10.0f;

            if (circles[i].alpha > 1.0f) circles[i].speed *= -1;

            if (circles[i].alpha <= 0.0f)
            {
                circles[i].alpha = 0.0f;
                circles[i].radius = (float)GetRandomValue(10, 40);
                circles[i].position.x = (float)GetRandomValue((int)circles[i].radius, (int)(screenWidth - circles[i].radius));
                circles[i].position.y = (float)GetRandomValue((int)circles[i].radius, (int)(screenHeight - circles[i].radius));
                circles[i].color = colors[GetRandomValue(0, 13)];
                circles[i].speed = (float)GetRandomValue(1, 100)/2000.0f;
            }
        }

        BeginDrawing();

            ClearBackground(RAYWHITE);

            for (int i = MAX_CIRCLES - 1; i >= 0; i--)
            {
                DrawCircleV(circles[i].position, circles[i].radius, Fade(circles[i].color, circles[i].alpha));
            }

            DrawRectangle(20, screenHeight - 32, screenWidth - 40, 12, LIGHTGRAY);
            DrawRectangle(20, screenHeight - 32, (int)timePlayed, 12, MAROON);
            DrawRectangleLines(20, screenHeight - 32, screenWidth - 40, 12, GRAY);

            DrawRectangle(20, 20, 300, 112, WHITE);
            DrawRectangleLines(20, 20, 300, 112, GRAY);
            DrawText("P13-S12 audio_module_playing", 24, 8, 10, DARKGRAY);
            DrawText("CROSS RESTART", 34, 34, 16, BLACK);
            DrawText("CIRCLE PAUSE/RESUME", 34, 56, 16, BLACK);
            DrawText("D-pad UP/DOWN SPEED", 34, 78, 16, BLACK);
            DrawText(TextFormat("SPEED: %f", pitch), 34, 100, 16, MAROON);

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
