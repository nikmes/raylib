/*******************************************************************************************
*
*   pad_api - the raylib gamepad API, with no platform-specific code
*
*   This is the portable half of sprint 041 (P8-S1): it must build and run unchanged on a
*   desktop and on the console. Do not add platform headers, platform symbols or conditionals.
*
********************************************************************************************/

#include "raylib.h"

#ifndef FRAMES
    #define FRAMES 1800
#endif

typedef struct {
    int button;
    const char *name;
} ButtonLabel;

static const ButtonLabel buttons[] = {
    { GAMEPAD_BUTTON_RIGHT_FACE_DOWN, "Cross / down" },
    { GAMEPAD_BUTTON_RIGHT_FACE_RIGHT, "Circle / right" },
    { GAMEPAD_BUTTON_LEFT_FACE_LEFT, "Square / left" },
    { GAMEPAD_BUTTON_RIGHT_FACE_UP, "Triangle / up" },
    { GAMEPAD_BUTTON_LEFT_FACE_UP, "D-pad up" },
    { GAMEPAD_BUTTON_LEFT_FACE_RIGHT, "D-pad right" },
    { GAMEPAD_BUTTON_LEFT_FACE_DOWN, "D-pad down" },
    { GAMEPAD_BUTTON_LEFT_FACE_LEFT, "D-pad left" },
    { GAMEPAD_BUTTON_LEFT_TRIGGER_1, "L shoulder" },
    { GAMEPAD_BUTTON_RIGHT_TRIGGER_1, "R shoulder" },
    { GAMEPAD_BUTTON_MIDDLE_LEFT, "Select" },
    { GAMEPAD_BUTTON_MIDDLE_RIGHT, "Start" },
};

#define BUTTON_COUNT ((int)(sizeof(buttons)/sizeof(buttons[0])))

int main(void)
{
    InitWindow(480, 272, "raylib gamepad API");
    if (!IsWindowReady()) return 1;

    int lastShown = 0;

    for (int frame = 0; frame < FRAMES; frame++)
    {
        int pressed = GetGamepadButtonPressed();
        if ((pressed != 0) && (pressed != lastShown)) TraceLog(LOG_INFO, "PAD pressed=%d frame=%d", pressed, frame);
        lastShown = pressed;

        if ((frame % 60) == 0)
        {
            TraceLog(LOG_INFO, "PAD state frame=%d ready0=%d ready1=%d name0=%s axes0=%d axes1=%d last=%d cross=%d square=%d LX=%.2f LY=%.2f RX=%.2f RY=%.2f LT=%.2f RT=%.2f",
                frame, IsGamepadAvailable(0), IsGamepadAvailable(1), GetGamepadName(0),
                GetGamepadAxisCount(0), GetGamepadAxisCount(1), pressed,
                IsGamepadButtonDown(0, GAMEPAD_BUTTON_RIGHT_FACE_DOWN),
                IsGamepadButtonDown(0, GAMEPAD_BUTTON_RIGHT_FACE_LEFT),
                GetGamepadAxisMovement(0, GAMEPAD_AXIS_LEFT_X),
                GetGamepadAxisMovement(0, GAMEPAD_AXIS_LEFT_Y),
                GetGamepadAxisMovement(0, GAMEPAD_AXIS_RIGHT_X),
                GetGamepadAxisMovement(0, GAMEPAD_AXIS_RIGHT_Y),
                GetGamepadAxisMovement(0, GAMEPAD_AXIS_LEFT_TRIGGER),
                GetGamepadAxisMovement(0, GAMEPAD_AXIS_RIGHT_TRIGGER));
        }

        BeginDrawing();
        ClearBackground(BLACK);

        DrawText("raylib gamepad API", 10, 10, 20, RAYWHITE);
        DrawText(TextFormat("PAD0 %-16s ready=%s axes=%d",
            GetGamepadName(0), (IsGamepadAvailable(0)? "yes" : "no"), GetGamepadAxisCount(0)),
            10, 38, 10, GREEN);
        DrawText(TextFormat("PAD1 %-16s ready=%s axes=%d",
            GetGamepadName(1), (IsGamepadAvailable(1)? "yes" : "no"), GetGamepadAxisCount(1)),
            10, 52, 10, GRAY);
        DrawText(TextFormat("last pressed=%d  frame=%d/%d", pressed, frame, FRAMES), 10, 66, 10, YELLOW);
        DrawText(TextFormat("LX %+.2f  LY %+.2f  RX %+.2f  RY %+.2f  LT %+.2f  RT %+.2f",
            GetGamepadAxisMovement(0, GAMEPAD_AXIS_LEFT_X),
            GetGamepadAxisMovement(0, GAMEPAD_AXIS_LEFT_Y),
            GetGamepadAxisMovement(0, GAMEPAD_AXIS_RIGHT_X),
            GetGamepadAxisMovement(0, GAMEPAD_AXIS_RIGHT_Y),
            GetGamepadAxisMovement(0, GAMEPAD_AXIS_LEFT_TRIGGER),
            GetGamepadAxisMovement(0, GAMEPAD_AXIS_RIGHT_TRIGGER)),
            10, 82, 10, SKYBLUE);

        for (int i = 0; i < BUTTON_COUNT; i++)
        {
            int column = i/6;
            int row = i%6;
            int x = 10 + column*235;
            int y = 105 + row*22;
            bool down = IsGamepadButtonDown(0, buttons[i].button);

            DrawRectangle(x, y, 10, 10, (down? GREEN : DARKGRAY));
            DrawText(TextFormat("%-18s %s", buttons[i].name, (down? "DOWN" : "up")), x + 16, y, 10,
                (down? RAYWHITE : LIGHTGRAY));
        }

        EndDrawing();
    }

    TraceLog(LOG_INFO, "DONE frames %d", FRAMES);

    // Stay resident without drawing: ClosePlatform() calls sceKernelExitGame(), and the
    // measurement protocol captures only after DONE. The host resets the console.
    for (;;) WaitTime(0.1);

    return 0;
}
