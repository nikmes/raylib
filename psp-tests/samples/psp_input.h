#ifndef PSP_SAMPLE_INPUT_H
#define PSP_SAMPLE_INPUT_H

// Shared input adaptation for the Phase 13 sample ports.
//
// The samples keep their upstream 800x450 coordinate space. The port opens a
// 480x272 window and draws through a 0.6 camera, so GetScreenWidth/Height are
// overridden to the virtual size. Keyboard calls map to the PSP pad; the nub
// maps to a virtual mouse. A small deterministic schedule presses the common
// keys so a headless run exercises the demo without a human at the controls.

#include "raylib.h"

#ifndef PSP_INPUT_SCREEN_WIDTH
    #define PSP_INPUT_SCREEN_WIDTH 800
#endif
#ifndef PSP_INPUT_SCREEN_HEIGHT
    #define PSP_INPUT_SCREEN_HEIGHT 450
#endif

static int pspFrame = 0;

static inline Camera2D PspSceneCamera(void)
{
    Camera2D cam = { 0 };
    cam.zoom = 0.6f;
    return cam;
}

static inline bool PspKeyPressed(int key)
{
    switch (key)
    {
        case KEY_SPACE:
            return IsGamepadButtonPressed(0, GAMEPAD_BUTTON_RIGHT_FACE_DOWN) ||
                   (pspFrame == 30) || ((pspFrame%240) == 30);
        case KEY_ENTER:
            return IsGamepadButtonPressed(0, GAMEPAD_BUTTON_RIGHT_FACE_LEFT) || (pspFrame == 90);
        case KEY_P:
            return IsGamepadButtonPressed(0, GAMEPAD_BUTTON_RIGHT_FACE_RIGHT) || (pspFrame == 300);
        case KEY_R:
            return IsGamepadButtonPressed(0, GAMEPAD_BUTTON_RIGHT_FACE_UP) || (pspFrame == 150);
        case KEY_G:
            return IsGamepadButtonPressed(0, GAMEPAD_BUTTON_RIGHT_FACE_UP) || (pspFrame == 210);
        case KEY_F:
        case KEY_Q:
            return IsGamepadButtonPressed(0, GAMEPAD_BUTTON_RIGHT_FACE_UP);
        case KEY_D:
        case KEY_Z:
            return IsGamepadButtonPressed(0, GAMEPAD_BUTTON_RIGHT_FACE_LEFT);
        case KEY_E:
            return IsGamepadButtonPressed(0, GAMEPAD_BUTTON_RIGHT_FACE_RIGHT);
        case KEY_X:
            return IsGamepadButtonPressed(0, GAMEPAD_BUTTON_RIGHT_FACE_DOWN);
        case KEY_LEFT:
        case KEY_A:
            return IsGamepadButtonPressed(0, GAMEPAD_BUTTON_LEFT_FACE_LEFT) ||
                   (pspFrame == 60) || ((pspFrame%240) == 60);
        case KEY_RIGHT:
            return IsGamepadButtonPressed(0, GAMEPAD_BUTTON_LEFT_FACE_RIGHT) ||
                   (pspFrame == 120) || ((pspFrame%240) == 120);
        case KEY_UP:
        case KEY_W:
            return IsGamepadButtonPressed(0, GAMEPAD_BUTTON_LEFT_FACE_UP) ||
                   (pspFrame == 180) || ((pspFrame%240) == 180);
        case KEY_DOWN:
        case KEY_S:
            return IsGamepadButtonPressed(0, GAMEPAD_BUTTON_LEFT_FACE_DOWN) ||
                   (pspFrame == 210) || ((pspFrame%240) == 210);
        default: return false;
    }
}

static inline bool PspKeyDown(int key)
{
    switch (key)
    {
        case KEY_SPACE: return IsGamepadButtonDown(0, GAMEPAD_BUTTON_RIGHT_FACE_DOWN);
        case KEY_LEFT_CONTROL: return IsGamepadButtonDown(0, GAMEPAD_BUTTON_LEFT_TRIGGER_1);
        case KEY_ENTER: return IsGamepadButtonDown(0, GAMEPAD_BUTTON_RIGHT_FACE_LEFT);
        case KEY_P: return IsGamepadButtonDown(0, GAMEPAD_BUTTON_RIGHT_FACE_RIGHT);
        case KEY_LEFT:
        case KEY_A: return IsGamepadButtonDown(0, GAMEPAD_BUTTON_LEFT_FACE_LEFT);
        case KEY_RIGHT:
        case KEY_D: return IsGamepadButtonDown(0, GAMEPAD_BUTTON_LEFT_FACE_RIGHT);
        case KEY_UP:
        case KEY_W: return IsGamepadButtonDown(0, GAMEPAD_BUTTON_LEFT_FACE_UP);
        case KEY_DOWN:
        case KEY_S: return IsGamepadButtonDown(0, GAMEPAD_BUTTON_LEFT_FACE_DOWN);
        default: return false;
    }
}

static inline bool PspKeyReleased(int key)
{
    switch (key)
    {
        case KEY_SPACE: return IsGamepadButtonReleased(0, GAMEPAD_BUTTON_RIGHT_FACE_DOWN);
        case KEY_ENTER: return IsGamepadButtonReleased(0, GAMEPAD_BUTTON_RIGHT_FACE_LEFT);
        case KEY_LEFT:
        case KEY_A: return IsGamepadButtonReleased(0, GAMEPAD_BUTTON_LEFT_FACE_LEFT);
        case KEY_RIGHT:
        case KEY_D: return IsGamepadButtonReleased(0, GAMEPAD_BUTTON_LEFT_FACE_RIGHT);
        case KEY_UP:
        case KEY_W: return IsGamepadButtonReleased(0, GAMEPAD_BUTTON_LEFT_FACE_UP);
        case KEY_DOWN:
        case KEY_S: return IsGamepadButtonReleased(0, GAMEPAD_BUTTON_LEFT_FACE_DOWN);
        default: return false;
    }
}

static inline Vector2 PspMousePosition(void)
{
    float x = GetGamepadAxisMovement(0, GAMEPAD_AXIS_LEFT_X);
    float y = GetGamepadAxisMovement(0, GAMEPAD_AXIS_LEFT_Y);
    return (Vector2){ (x + 1.0f)*0.5f*PSP_INPUT_SCREEN_WIDTH, (y + 1.0f)*0.5f*PSP_INPUT_SCREEN_HEIGHT };
}

static Vector2 pspPrevMouse = { 0 };
static int pspMouseInit = 0;

static inline Vector2 PspMouseDelta(void)
{
    Vector2 p = PspMousePosition();
    if (!pspMouseInit) { pspPrevMouse = p; pspMouseInit = 1; return (Vector2){ 0, 0 }; }
    Vector2 d = { p.x - pspPrevMouse.x, p.y - pspPrevMouse.y };
    pspPrevMouse = p;
    return d;
}

static inline float PspMouseX(void) { return PspMousePosition().x; }
static inline float PspMouseY(void) { return PspMousePosition().y; }

static inline bool PspMouseDown(int button)
{
    if (button == MOUSE_BUTTON_LEFT)
        return IsGamepadButtonDown(0, GAMEPAD_BUTTON_RIGHT_FACE_DOWN) ||
               ((pspFrame >= 60) && (pspFrame < 90));
    if (button == MOUSE_BUTTON_RIGHT) return IsGamepadButtonDown(0, GAMEPAD_BUTTON_RIGHT_FACE_RIGHT);
    if (button == MOUSE_BUTTON_MIDDLE) return IsGamepadButtonDown(0, GAMEPAD_BUTTON_RIGHT_FACE_LEFT);
    return false;
}

static inline bool PspMousePressed(int button)
{
    if (button == MOUSE_BUTTON_LEFT)
        return IsGamepadButtonPressed(0, GAMEPAD_BUTTON_RIGHT_FACE_DOWN) || (pspFrame == 60);
    if (button == MOUSE_BUTTON_RIGHT) return IsGamepadButtonPressed(0, GAMEPAD_BUTTON_RIGHT_FACE_RIGHT);
    if (button == MOUSE_BUTTON_MIDDLE) return IsGamepadButtonPressed(0, GAMEPAD_BUTTON_RIGHT_FACE_LEFT);
    return false;
}

static inline bool PspMouseReleased(int button)
{
    if (button == MOUSE_BUTTON_LEFT)
        return IsGamepadButtonReleased(0, GAMEPAD_BUTTON_RIGHT_FACE_DOWN) || (pspFrame == 90);
    if (button == MOUSE_BUTTON_RIGHT) return IsGamepadButtonReleased(0, GAMEPAD_BUTTON_RIGHT_FACE_RIGHT);
    if (button == MOUSE_BUTTON_MIDDLE) return IsGamepadButtonReleased(0, GAMEPAD_BUTTON_RIGHT_FACE_LEFT);
    return false;
}

static inline float PspMouseWheel(void)
{
    if (IsGamepadButtonPressed(0, GAMEPAD_BUTTON_LEFT_FACE_UP)) return 1.0f;
    if (IsGamepadButtonPressed(0, GAMEPAD_BUTTON_LEFT_FACE_DOWN)) return -1.0f;
    if (pspFrame == 90) return 1.0f;
    return 0.0f;
}

static inline void PspBeginScissorMode(int x, int y, int w, int h)
{
    BeginScissorMode((int)(x*0.6f), (int)(y*0.6f), (int)(w*0.6f), (int)(h*0.6f));
}

#define IsKeyPressed(k) PspKeyPressed(k)
#define IsKeyDown(k) PspKeyDown(k)
#define IsKeyReleased(k) PspKeyReleased(k)
#define IsKeyUp(k) (!PspKeyDown(k))
#define GetMousePosition() PspMousePosition()
#define GetMouseDelta() PspMouseDelta()
#define GetMouseX() PspMouseX()
#define GetMouseY() PspMouseY()
#define GetMouseWheelMove() PspMouseWheel()
#define IsMouseButtonDown(b) PspMouseDown(b)
#define IsMouseButtonPressed(b) PspMousePressed(b)
#define IsMouseButtonReleased(b) PspMouseReleased(b)
#define BeginScissorMode(x, y, w, h) PspBeginScissorMode(x, y, w, h)
#define GetScreenWidth() PSP_INPUT_SCREEN_WIDTH
#define GetScreenHeight() PSP_INPUT_SCREEN_HEIGHT

#endif
