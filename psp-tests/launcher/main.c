#include <pspkernel.h>
#include "raylib.h"
#include "samples.h"
#include "rlgl.h"
#include <string.h>

PSP_MODULE_INFO("raylib_samples_launcher", 0, 1, 1);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);

#ifndef LAUNCHER_AUTO_INDEX
    #define LAUNCHER_AUTO_INDEX 0
#endif

int main(void)
{
    const char *appDir = GetApplicationDirectory();
    if ((appDir != NULL) && (appDir[0] != 0)) ChangeDirectory(appDir);

    InitWindow(480, 272, "raylib PSP samples");

    TraceLog(LOG_INFO, "LAUNCHER CWD %s", GetWorkingDirectory());

    int category = 0;
    int selected = 0;
    int top = 0;
    int autoDone = 0;
    int frame = 0;

    SetTargetFPS(60);

    TraceLog(LOG_INFO, "LAUNCHER READY samples=%i", SAMPLE_COUNT);

    for (;;)
    {
        frame++;

#ifdef LAUNCHER_AUTOLAUNCH
        if (!autoDone && (frame == 120))
        {
            autoDone = 1;
            TraceLog(LOG_INFO, "LAUNCH %s/%s (auto)", samples[LAUNCHER_AUTO_INDEX].category,
                     samples[LAUNCHER_AUTO_INDEX].name);
            samples[LAUNCHER_AUTO_INDEX].run();
            rlpsp.boundTex = 0xFFFFFFFFu;
            TraceLog(LOG_INFO, "SAMPLE RETURN %s/%s (auto)", samples[LAUNCHER_AUTO_INDEX].category,
                     samples[LAUNCHER_AUTO_INDEX].name);
            SetTargetFPS(60);
        }
#endif

        int list[128];
        int count = 0;
        for (int i = 0; i < SAMPLE_COUNT; i++)
        {
            if (strcmp(samples[i].category, categories[category]) == 0) list[count++] = i;
        }

        if (count == 0) selected = 0;
        else if (selected >= count) selected = count - 1;

        if (IsGamepadButtonPressed(0, GAMEPAD_BUTTON_LEFT_TRIGGER_1))
        {
            category = (category + CATEGORY_COUNT - 1)%CATEGORY_COUNT;
            selected = 0;
            top = 0;
        }
        if (IsGamepadButtonPressed(0, GAMEPAD_BUTTON_RIGHT_TRIGGER_1))
        {
            category = (category + 1)%CATEGORY_COUNT;
            selected = 0;
            top = 0;
        }

        if (count > 0)
        {
            if (IsGamepadButtonPressed(0, GAMEPAD_BUTTON_LEFT_FACE_UP) && (selected > 0)) selected--;
            if (IsGamepadButtonPressed(0, GAMEPAD_BUTTON_LEFT_FACE_DOWN) && (selected < count - 1)) selected++;

            if (selected < top) top = selected;
            if (selected >= top + 8) top = selected - 7;

            if (IsGamepadButtonPressed(0, GAMEPAD_BUTTON_RIGHT_FACE_DOWN))
            {
                TraceLog(LOG_INFO, "LAUNCH %s/%s", samples[list[selected]].category, samples[list[selected]].name);
                samples[list[selected]].run();
                rlpsp.boundTex = 0xFFFFFFFFu;
                TraceLog(LOG_INFO, "SAMPLE RETURN %s/%s", samples[list[selected]].category, samples[list[selected]].name);
                SetTargetFPS(60);
            }
        }

        BeginDrawing();

            ClearBackground((Color){ 18, 18, 24, 255 });

            DrawRectangle(0, 0, 480, 30, (Color){ 30, 30, 44, 255 });
            for (int i = 0; i < CATEGORY_COUNT; i++)
            {
                int x = 10 + i*84;
                DrawText(categories[i], x, 8, 16, (i == category)? WHITE : GRAY);
                if (i == category) DrawRectangle(x, 26, 72, 2, (Color){ 80, 160, 255, 255 });
            }

            for (int r = 0; (r < 8) && ((top + r) < count); r++)
            {
                int index = list[top + r];
                int y = 42 + r*22;
                if ((top + r) == selected)
                {
                    DrawRectangle(6, y - 2, 468, 20, (Color){ 52, 62, 96, 255 });
                    DrawRectangle(6, y - 2, 3, 20, (Color){ 80, 160, 255, 255 });
                }
                DrawText(samples[index].name, 18, y, 18, RAYWHITE);
            }

            if (count == 0) DrawText("no samples in this category", 18, 44, 16, GRAY);

            DrawText("X launch    START returns to the menu    L/R category", 10, 238, 12, LIGHTGRAY);
            DrawText(TextFormat("raylib PSP samples   %i ported", SAMPLE_COUNT), 10, 254, 10, DARKGRAY);

        EndDrawing();

        if (WindowShouldClose()) break;
    }

    CloseWindow();

    return 0;
}
