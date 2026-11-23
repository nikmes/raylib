/**********************************************************************************************
*
*   rcore_vita - Functions to manage window, graphics device and inputs
*
*   PLATFORM: PSP
*       - PlayStation PSP
*
*   LIMITATIONS:
*       - Limitation 01
*       - Limitation 02
*
*   POSSIBLE IMPROVEMENTS:
*       - Improvement 01
*       - Improvement 02
*
*   ADDITIONAL NOTES:
*       - Based in original port by Antonio Jose Ramos Marquez aka bigboss @psxdev	
*
*   CONFIGURATION:
*       - make PLATFORM=PLATFORM_PSP
*
*   DEPENDENCIES:
*       PSPDEV, PSPSDK - Provides C API to access PSP homebrew functionality
*
*
*   LICENSE: zlib/libpng
*
*   Copyright (c) 2013-2023 Ramon Santamaria (@raysan5) and contributors
*
*   This software is provided "as-is", without any express or implied warranty. In no event
*   will the authors be held liable for any damages arising from the use of this software.
*
*   Permission is granted to anyone to use this software for any purpose, including commercial
*   applications, and to alter it and redistribute it freely, subject to the following restrictions:
*
*     1. The origin of this software must not be misrepresented; you must not claim that you
*     wrote the original software. If you use this software in a product, an acknowledgment
*     in the product documentation would be appreciated but is not required.
*
*     2. Altered source versions must be plainly marked as such, and must not be misrepresented
*     as being the original software.
*
*     3. This notice may not be removed or altered from any source distribution.
*
**********************************************************************************************/
#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspdebug.h>
#include <pspctrl.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <GLES/egl.h>
#include <GLES/gl.h>

//----------------------------------------------------------------------------------
// Types and Structures Definition
//----------------------------------------------------------------------------------
typedef struct {
    // TODO: Define the platform specific variables required

    // Display data
    EGLDisplay device;                  // Native display device (physical screen connection)
    EGLSurface surface;                 // Surface to draw on, framebuffers (connected to context)
    EGLContext context;                 // Graphic context, mode in which drawing can be done
    EGLConfig config;                   // Graphic config
} PlatformData;

//----------------------------------------------------------------------------------
// Global Variables Definition
//----------------------------------------------------------------------------------
extern CoreData CORE;                   // Global CORE state context

static PlatformData platform = { 0 };   // Platform specific data
static int pspVsync = 1;                // Display vblank sync, on by default (PSP_SetVsync)

//----------------------------------------------------------------------------------
// Module Internal Functions Declaration
//----------------------------------------------------------------------------------
int InitPlatform(void);          // Initialize platform (graphics, inputs and more)
bool InitGraphicsDevice(void);   // Initialize graphics device

//----------------------------------------------------------------------------------
// Module Functions Declaration
//----------------------------------------------------------------------------------
// NOTE: Functions declaration is provided by raylib.h

//----------------------------------------------------------------------------------
// Module Functions Definition: Window and Graphics Device
//----------------------------------------------------------------------------------

// Check if application should close
bool WindowShouldClose(void)
{
    if (CORE.Window.ready) return CORE.Window.shouldClose;
    else return true;
}

// Toggle fullscreen mode
void ToggleFullscreen(void)
{
    TRACELOG(LOG_WARNING, "ToggleFullscreen() not available on target platform");
}

// Toggle borderless windowed mode
void ToggleBorderlessWindowed(void)
{
    TRACELOG(LOG_WARNING, "ToggleBorderlessWindowed() not available on target platform");
}

// Set window state: maximized, if resizable
void MaximizeWindow(void)
{
    TRACELOG(LOG_WARNING, "MaximizeWindow() not available on target platform");
}

// Set window state: minimized
void MinimizeWindow(void)
{
    TRACELOG(LOG_WARNING, "MinimizeWindow() not available on target platform");
}

// Set window state: not minimized/maximized
void RestoreWindow(void)
{
    TRACELOG(LOG_WARNING, "RestoreWindow() not available on target platform");
}

// Set window configuration state using flags
void SetWindowState(unsigned int flags)
{
    TRACELOG(LOG_WARNING, "SetWindowState() not available on target platform");
}

// Clear window configuration state flags
void ClearWindowState(unsigned int flags)
{
    TRACELOG(LOG_WARNING, "ClearWindowState() not available on target platform");
}

// Set icon for window
void SetWindowIcon(Image image)
{
    TRACELOG(LOG_WARNING, "SetWindowIcon() not available on target platform");
}

// Set icon for window
void SetWindowIcons(Image *images, int count)
{
    TRACELOG(LOG_WARNING, "SetWindowIcons() not available on target platform");
}

// Set title for window
void SetWindowTitle(const char *title)
{
    CORE.Window.title = title;
}

// Set window position on screen (windowed mode)
void SetWindowPosition(int x, int y)
{
    TRACELOG(LOG_WARNING, "SetWindowPosition() not available on target platform");
}

// Set monitor for the current window
void SetWindowMonitor(int monitor)
{
    TRACELOG(LOG_WARNING, "SetWindowMonitor() not available on target platform");
}

// Set window minimum dimensions (FLAG_WINDOW_RESIZABLE)
void SetWindowMinSize(int width, int height)
{
    CORE.Window.screenMin.width = width;
    CORE.Window.screenMin.height = height;
}

// Set window maximum dimensions (FLAG_WINDOW_RESIZABLE)
void SetWindowMaxSize(int width, int height)
{
    CORE.Window.screenMax.width = width;
    CORE.Window.screenMax.height = height;
}

// Set window dimensions
void SetWindowSize(int width, int height)
{
    TRACELOG(LOG_WARNING, "SetWindowSize() not available on target platform");
}

// Set window opacity, value opacity is between 0.0 and 1.0
void SetWindowOpacity(float opacity)
{
    TRACELOG(LOG_WARNING, "SetWindowOpacity() not available on target platform");
}

// Set window focused
void SetWindowFocused(void)
{
    TRACELOG(LOG_WARNING, "SetWindowFocused() not available on target platform");
}

// Get native window handle
void *GetWindowHandle(void)
{
    TRACELOG(LOG_WARNING, "GetWindowHandle() not implemented on target platform");
    return NULL;
}

// Get number of monitors
int GetMonitorCount(void)
{
    TRACELOG(LOG_WARNING, "GetMonitorCount() not implemented on target platform");
    return 1;
}

// Get number of monitors
int GetCurrentMonitor(void)
{
    TRACELOG(LOG_WARNING, "GetCurrentMonitor() not implemented on target platform");
    return 0;
}

// Get selected monitor position
Vector2 GetMonitorPosition(int monitor)
{
    TRACELOG(LOG_WARNING, "GetMonitorPosition() not implemented on target platform");
    return (Vector2){ 0, 0 };
}

// Get selected monitor width (currently used by monitor)
int GetMonitorWidth(int monitor)
{
    TRACELOG(LOG_WARNING, "GetMonitorWidth() not implemented on target platform");
    return 0;
}

// Get selected monitor height (currently used by monitor)
int GetMonitorHeight(int monitor)
{
    TRACELOG(LOG_WARNING, "GetMonitorHeight() not implemented on target platform");
    return 0;
}

// Get selected monitor physical width in millimetres
int GetMonitorPhysicalWidth(int monitor)
{
    TRACELOG(LOG_WARNING, "GetMonitorPhysicalWidth() not implemented on target platform");
    return 0;
}

// Get selected monitor physical height in millimetres
int GetMonitorPhysicalHeight(int monitor)
{
    TRACELOG(LOG_WARNING, "GetMonitorPhysicalHeight() not implemented on target platform");
    return 0;
}

// Get selected monitor refresh rate
int GetMonitorRefreshRate(int monitor)
{
    TRACELOG(LOG_WARNING, "GetMonitorRefreshRate() not implemented on target platform");
    return 0;
}

// Get the human-readable, UTF-8 encoded name of the selected monitor
const char *GetMonitorName(int monitor)
{
    TRACELOG(LOG_WARNING, "GetMonitorName() not implemented on target platform");
    return "";
}

// Get window position XY on monitor
Vector2 GetWindowPosition(void)
{
    TRACELOG(LOG_WARNING, "GetWindowPosition() not implemented on target platform");
    return (Vector2){ 0, 0 };
}

// Get window scale DPI factor for current monitor
Vector2 GetWindowScaleDPI(void)
{
    TRACELOG(LOG_WARNING, "GetWindowScaleDPI() not implemented on target platform");
    return (Vector2){ 1.0f, 1.0f };
}

// Set clipboard text content
void SetClipboardText(const char *text)
{
    TRACELOG(LOG_WARNING, "SetClipboardText() not implemented on target platform");
}

// Get clipboard text content
// NOTE: returned string is allocated and freed by GLFW
const char *GetClipboardText(void)
{
    TRACELOG(LOG_WARNING, "GetClipboardText() not implemented on target platform");
    return NULL;
}

// Show mouse cursor
void ShowCursor(void)
{
    CORE.Input.Mouse.cursorHidden = false;
}

// Hides mouse cursor
void HideCursor(void)
{
    CORE.Input.Mouse.cursorHidden = true;
}

// Enables cursor (unlock cursor)
void EnableCursor(void)
{
    // Set cursor position in the middle
    SetMousePosition(CORE.Window.screen.width/2, CORE.Window.screen.height/2);

    CORE.Input.Mouse.cursorHidden = false;
}

// Disables cursor (lock cursor)
void DisableCursor(void)
{
    // Set cursor position in the middle
    SetMousePosition(CORE.Window.screen.width/2, CORE.Window.screen.height/2);

    CORE.Input.Mouse.cursorHidden = true;
}

// Swap back buffer with front buffer (screen drawing)
void SwapScreenBuffer(void)
{
    eglSwapBuffers(platform.device, platform.surface);
}

// Enable/disable display vblank sync at runtime (PSP only).
// pspgu implements this through eglSwapInterval; on means wait for vblank.
void PSP_SetVsync(int enable)
{
    pspVsync = (enable != 0);

    if (platform.device != EGL_NO_DISPLAY)
    {
        eglSwapInterval(platform.device, pspVsync);
        TRACELOG(LOG_INFO, "PLATFORM: VSYNC %s", pspVsync? "on" : "off");
    }
}

//----------------------------------------------------------------------------------
// Module Functions Definition: Misc
//----------------------------------------------------------------------------------

// Get elapsed time measure in seconds since InitTimer()
double GetTime(void)
{
    double time = 0.0;
    struct timespec ts = { 0 };
    clock_gettime(CLOCK_MONOTONIC, &ts);
    unsigned long long int nanoSeconds = (unsigned long long int)ts.tv_sec*1000000000LLU + (unsigned long long int)ts.tv_nsec;

    time = (double)(nanoSeconds - CORE.Time.base)*1e-9;  // Elapsed time since InitTimer()

    return time;
}

// Open URL with default system browser (if available)
// NOTE: This function is only safe to use if you control the URL given.
// A user could craft a malicious string performing another action.
// Only call this function yourself not with user input or make sure to check the string yourself.
// Ref: https://github.com/raysan5/raylib/issues/686
void OpenURL(const char *url)
{
   // Security check to (partially) avoid malicious code on target platform
    if (strchr(url, '\'') != NULL) TRACELOG(LOG_WARNING, "SYSTEM: Provided URL could be potentially malicious, avoid [\'] character");
    else
    {
        // TODO:
    }
}

// Get clipboard image
// NOTE: A PSP has no clipboard. raylib declares this in its public API, so the
// function is provided and returns an empty image (data == NULL) instead of
// leaving an undefined symbol for applications to trip over.
Image GetClipboardImage(void)
{
    Image image = { 0 };
    return image;
}

//----------------------------------------------------------------------------------
// Module Functions Definition: Inputs
//----------------------------------------------------------------------------------

// Set internal gamepad mappings
int SetGamepadMappings(const char *mappings)
{
    // No mapping database on this platform; a portable game may call this once at
    // start-up, so keep it quiet unless debug logging is enabled.
    (void)mappings;
    TRACELOG(LOG_DEBUG, "SetGamepadMappings() is a no-op on this platform");
    return 0;
}

// Set mouse position XY
void SetMousePosition(int x, int y)
{
    CORE.Input.Mouse.currentPosition = (Vector2){ (float)x, (float)y };
    CORE.Input.Mouse.previousPosition = CORE.Input.Mouse.currentPosition;
}

// Set mouse cursor
void SetMouseCursor(int cursor)
{
    TRACELOG(LOG_WARNING, "SetMouseCursor() not implemented on target platform");
}

// Sprint 041 (P8-S1): one platform-neutral gamepad contract.
#ifndef PSP_PAD_STRICT
    #define PSP_PAD_STRICT 1
#endif
#ifndef PSP_PAD_FAIL
    #define PSP_PAD_FAIL 0
#endif
#ifndef PSP_PAD_POLL
    #define PSP_PAD_POLL 1
#endif
#ifndef PSP_PAD_LATCH
    #define PSP_PAD_LATCH 1
#endif
#ifndef PSP_PAD_FREEZE
    #define PSP_PAD_FREEZE 1
#endif
#ifndef PSP_PAD_CYCLE
    #define PSP_PAD_CYCLE 10000
#endif
#ifndef PSP_PAD_TRACE
    #define PSP_PAD_TRACE 0
#endif
#ifndef PSP_PAD_FORCE_FREEZE
    #define PSP_PAD_FORCE_FREEZE 0
#endif
#ifndef PSP_PAD_STATS_INTERVAL
    #define PSP_PAD_STATS_INTERVAL 300
#endif

// Sprint 043 (P8-S3): nub calibration and idle-cancel arms.
#ifndef PSP_PAD_CALIB
    #define PSP_PAD_CALIB 1
#endif
#ifndef PSP_PAD_IDLE
    #define PSP_PAD_IDLE 1
#endif
// Measured on PSP-1000, sprint 043 §7: median of four clean runs. Run 4 was
// rejected (idle_dx=82). The neutral has two resting states ~19 counts apart,
// so dz covers the worst idle magnitude (0.154) plus the 0.02 margin.
#ifndef PSP_NUB_CX
    #define PSP_NUB_CX 133
#endif
#ifndef PSP_NUB_CY
    #define PSP_NUB_CY 112
#endif
#ifndef PSP_NUB_RXP
    #define PSP_NUB_RXP 122
#endif
#ifndef PSP_NUB_RXN
    #define PSP_NUB_RXN 133
#endif
#ifndef PSP_NUB_RYP
    #define PSP_NUB_RYP 143
#endif
#ifndef PSP_NUB_RYN
    #define PSP_NUB_RYN 112
#endif
#ifndef PSP_NUB_DZ
    #define PSP_NUB_DZ 0.18f
#endif
// Idle-cancel pair, raw movement counts. T3 adopts 0x38: below Sony's 0x40 and
// half the smallest measured half-range (112/2).
#ifndef PSP_PAD_IDLE_RESET
    #define PSP_PAD_IDLE_RESET 0x38
#endif
#ifndef PSP_PAD_IDLE_BACK
    #define PSP_PAD_IDLE_BACK 0x38
#endif
// Sprint 044 (P8-S4): optional pad->keyboard/mouse compatibility, off by default.
#ifndef PSP_PAD_AS_KEYBOARD
    #define PSP_PAD_AS_KEYBOARD 0
#endif
#ifndef PSP_PAD_AS_MOUSE
    #define PSP_PAD_AS_MOUSE 0
#endif
#ifndef PSP_PAD_KEY_THRESHOLD
    #define PSP_PAD_KEY_THRESHOLD 0.5f
#endif
#ifndef PSP_PAD_MOUSE_SPEED
    #define PSP_PAD_MOUSE_SPEED 600.0f
#endif

static const struct {
    unsigned int psp;
    int raylib;
} pspButtonMap[] = {
    { PSP_CTRL_CROSS, GAMEPAD_BUTTON_RIGHT_FACE_DOWN },
    { PSP_CTRL_CIRCLE, GAMEPAD_BUTTON_RIGHT_FACE_RIGHT },
    { PSP_CTRL_SQUARE, GAMEPAD_BUTTON_RIGHT_FACE_LEFT },
    { PSP_CTRL_TRIANGLE, GAMEPAD_BUTTON_RIGHT_FACE_UP },
    { PSP_CTRL_UP, GAMEPAD_BUTTON_LEFT_FACE_UP },
    { PSP_CTRL_RIGHT, GAMEPAD_BUTTON_LEFT_FACE_RIGHT },
    { PSP_CTRL_DOWN, GAMEPAD_BUTTON_LEFT_FACE_DOWN },
    { PSP_CTRL_LEFT, GAMEPAD_BUTTON_LEFT_FACE_LEFT },
    { PSP_CTRL_LTRIGGER, GAMEPAD_BUTTON_LEFT_TRIGGER_1 },
    { PSP_CTRL_RTRIGGER, GAMEPAD_BUTTON_RIGHT_TRIGGER_1 },
    { PSP_CTRL_SELECT, GAMEPAD_BUTTON_MIDDLE_LEFT },
    { PSP_CTRL_START, GAMEPAD_BUTTON_MIDDLE_RIGHT },
};

#define PSP_BUTTON_MAP_COUNT ((int)(sizeof(pspButtonMap)/sizeof(pspButtonMap[0])))

typedef struct {
    unsigned int polls;
    unsigned int ret0;
    unsigned int retneg;
    unsigned int retgt1;
    int blockus_max;
    unsigned int lagus_max;
    unsigned int latch_only;
    unsigned int spurious;
    unsigned int frozen_total;
} PspStats;

static PspStats pspStats = { 0 };

static void PspLogStats(void)
{
    if ((pspStats.polls == 1) ||
        ((PSP_PAD_STATS_INTERVAL > 0) && ((pspStats.polls % PSP_PAD_STATS_INTERVAL) == 0)))
    {
        TRACELOG(LOG_INFO, "PAD stats polls=%u ret0=%u retneg=%u retgt1=%u blockus_max=%d lagms_max=%.2f latchOnly=%u spurious=%u frozen=%u",
            pspStats.polls, pspStats.ret0, pspStats.retneg, pspStats.retgt1, pspStats.blockus_max,
            (double)pspStats.lagus_max/1000.0, pspStats.latch_only, pspStats.spurious, pspStats.frozen_total);
    }
}

// Register all input events
void PollInputEvents(void)
{
#if defined(SUPPORT_GESTURES_SYSTEM)
    // NOTE: Gestures update must be called every frame to reset gestures correctly
    // because ProcessGestureEvent() is just called on an event, not every frame
    UpdateGestures();
#endif

    // Reset keys/chars pressed registered
    CORE.Input.Keyboard.keyPressedQueueCount = 0;
    CORE.Input.Keyboard.charPressedQueueCount = 0;

    // Reset key repeats
    for (int i = 0; i < MAX_KEYBOARD_KEYS; i++) CORE.Input.Keyboard.keyRepeatInFrame[i] = 0;

    static int pspSuppressedPrev = 0;
#if PSP_PAD_FREEZE
    static int pspResync = 0;
    static unsigned int pspFrozenFrames = 0;
#endif

    pspStats.polls++;

#if !PSP_PAD_FREEZE
    // Control arm keeps the old save-before-read order.
    for (int i = 0; i < MAX_GAMEPAD_BUTTONS; i++)
    {
        CORE.Input.Gamepad.previousButtonState[0][i] = CORE.Input.Gamepad.currentButtonState[0][i];
    }

    // Reset last gamepad button/axis registered state
    CORE.Input.Gamepad.lastButtonPressed = 0; // GAMEPAD_BUTTON_UNKNOWN
#endif

    // Register previous touch states
    for (int i = 0; i < MAX_TOUCH_POINTS; i++) CORE.Input.Touch.previousTouchState[i] = CORE.Input.Touch.currentTouchState[i];

    // Reset touch positions
    // TODO: It resets on target platform the mouse position and not filled again until a move-event,
    // so, if mouse is not moved it returns a (0, 0) position... this behaviour should be reviewed!
    //for (int i = 0; i < MAX_TOUCH_POINTS; i++) CORE.Input.Touch.position[i] = (Vector2){ 0, 0 };

    // Register previous key states. The imported file left this commented out; the
    // optional pad->keyboard path needs it so IsKeyPressed fires once per press.
    for (int i = 0; i < MAX_KEYBOARD_KEYS; i++)
    {
        CORE.Input.Keyboard.previousKeyState[i] = CORE.Input.Keyboard.currentKeyState[i];
    }

    // Poll input events for PSP (sprint 042, P8-S2)
    SceCtrlData pad = { 0 };
    int ret = 1;
    SceInt64 pollStart = sceKernelGetSystemTimeWide();

#if PSP_PAD_POLL == 0
    #if PSP_PAD_FAIL
        // 041 test hook: read-only, Cross or Square held for 60 consecutive polls.
        static int pspFailCount = 0;
        ret = sceCtrlReadBufferPositive(&pad, 1);
        if (ret > 0)
        {
            if ((pad.Buttons & (PSP_CTRL_CROSS | PSP_CTRL_SQUARE)) != 0)
            {
                pspFailCount++;
                if (pspFailCount >= 60) ret = -1;
            }
            else pspFailCount = 0;
        }
    #else
        ret = sceCtrlReadBufferPositive(&pad, 1);
    #endif
#elif PSP_PAD_POLL == 1
    ret = sceCtrlPeekBufferPositive(&pad, 1);
#else
    {
        SceCtrlData padN[6] = { 0 };
        int n = sceCtrlReadBufferPositive(padN, 6);
        ret = n;
        if (n > 0)
        {
            pad = padN[n - 1];
            for (int i = 0; i < (n - 1); i++) pad.Buttons |= padN[i].Buttons;
        }
    }
#endif

    SceInt64 pollEnd = sceKernelGetSystemTimeWide();
    int blockus = (int)(pollEnd - pollStart);
    if (blockus > pspStats.blockus_max) pspStats.blockus_max = blockus;
    if ((pad.TimeStamp > 0) && (pollEnd >= (SceInt64)pad.TimeStamp))
    {
        unsigned int lagus = (unsigned int)(pollEnd - pad.TimeStamp);
        if (lagus > pspStats.lagus_max) pspStats.lagus_max = lagus;
    }
    if (ret < 0) pspStats.retneg++;
    else if (ret == 0) pspStats.ret0++;
    else if (ret > 1) pspStats.retgt1++;
    PspLogStats();

    // Peek is not documented to return a sample count; the first probe confirms it.
#if PSP_PAD_POLL == 1
    int success = (ret >= 0);
#else
    int success = (ret > 0);
#endif

#if PSP_PAD_TRACE
    static int pspTraceCount = 0;
    if (pspTraceCount < 240)
    {
        TRACELOG(LOG_INFO, "PAD raw ret=%d success=%d blockus=%d now=%lld ts=%u buttons=0x%08X lx=%u ly=%u",
            ret, success, blockus, (long long)pollEnd, pad.TimeStamp, pad.Buttons, pad.Lx, pad.Ly);
        pspTraceCount++;
    }
#endif

#if PSP_PAD_STRICT
    static int pspLastSuccess = 1;
    if (!success)
    {
        if (pspLastSuccess) TRACELOG(LOG_WARNING, "PAD input failed ret=%d", ret);
        pspLastSuccess = 0;
#if PSP_PAD_LATCH
        {
            SceCtrlLatch discard = { 0 };
            sceCtrlReadLatch(&discard);    // clear a make from the failed window
        }
#endif
        return;
    }
    if (!pspLastSuccess) TRACELOG(LOG_INFO, "PAD input recovered");
    pspLastSuccess = 1;
#else
    (void)success;
#endif

#if PSP_PAD_FORCE_FREEZE
    static int pspForceCount = 0;
    if ((pad.Buttons & PSP_CTRL_CROSS) != 0) pspForceCount++;
    if ((pspForceCount >= 60) && (pspForceCount < 63)) pad.Buttons = PSP_CTRL_HOLD;
#endif

#if PSP_PAD_FREEZE
    if ((pad.Buttons & (PSP_CTRL_HOME | PSP_CTRL_HOLD)) != 0)
    {
#if PSP_PAD_LATCH
        {
            SceCtrlLatch discard = { 0 };
            sceCtrlReadLatch(&discard);    // discard a make from the frozen window
        }
#endif
        CORE.Input.Gamepad.lastButtonPressed = 0;
        pspSuppressedPrev = 1;
        pspFrozenFrames++;
        pspStats.frozen_total++;
        return;                            // snapshots untouched
    }
    if (pspSuppressedPrev)
    {
        pspResync = 1;
        pspSuppressedPrev = 0;
    }
    for (int i = 0; i < MAX_GAMEPAD_BUTTONS; i++)
    {
        CORE.Input.Gamepad.previousButtonState[0][i] = CORE.Input.Gamepad.currentButtonState[0][i];
    }
    CORE.Input.Gamepad.lastButtonPressed = 0;
#endif

    // Map the controller through one table into raylib's positional enum.
    memset(CORE.Input.Gamepad.currentButtonState[0], 0, sizeof(CORE.Input.Gamepad.currentButtonState[0]));
    for (int i = 0; i < PSP_BUTTON_MAP_COUNT; i++)
    {
        if ((pad.Buttons & pspButtonMap[i].psp) != 0) CORE.Input.Gamepad.currentButtonState[0][pspButtonMap[i].raylib] = 1;
    }

    // Normalize the nub; sprint 043 (P8-S3) selects the calibrated path.
#if PSP_PAD_CALIB
    {
        float dx = (float)pad.Lx - (float)PSP_NUB_CX;
        float dy = (float)pad.Ly - (float)PSP_NUB_CY;
        float rx = (dx >= 0.0f)? (float)PSP_NUB_RXP : (float)PSP_NUB_RXN;
        float ry = (dy >= 0.0f)? (float)PSP_NUB_RYP : (float)PSP_NUB_RYN;
        float nx = dx/rx;
        float ny = dy/ry;
        float mag;

        if (nx > 1.0f) nx = 1.0f; else if (nx < -1.0f) nx = -1.0f;
        if (ny > 1.0f) ny = 1.0f; else if (ny < -1.0f) ny = -1.0f;

        mag = sqrtf(nx*nx + ny*ny);
        if (mag < PSP_NUB_DZ)
        {
            nx = 0.0f;
            ny = 0.0f;
        }
        else
        {
            float k = (mag - PSP_NUB_DZ)/(1.0f - PSP_NUB_DZ);
            nx *= k/mag;
            ny *= k/mag;
            mag = sqrtf(nx*nx + ny*ny);
            if (mag > 1.0f)
            {
                nx /= mag;
                ny /= mag;
            }
        }

        CORE.Input.Gamepad.axisState[0][GAMEPAD_AXIS_LEFT_X] = nx;
        CORE.Input.Gamepad.axisState[0][GAMEPAD_AXIS_LEFT_Y] = ny;
    }
#else
    // 041/042 control: fixed centre and a per-axis 0.2 deadzone.
    CORE.Input.Gamepad.axisState[0][GAMEPAD_AXIS_LEFT_X] = ((float)pad.Lx - 128.0f)/128.0f;
    CORE.Input.Gamepad.axisState[0][GAMEPAD_AXIS_LEFT_Y] = ((float)pad.Ly - 128.0f)/128.0f;

    if ((CORE.Input.Gamepad.axisState[0][GAMEPAD_AXIS_LEFT_X] < 0.2f) &&
        (CORE.Input.Gamepad.axisState[0][GAMEPAD_AXIS_LEFT_X] > -0.2f)) CORE.Input.Gamepad.axisState[0][GAMEPAD_AXIS_LEFT_X] = 0.0f;
    if ((CORE.Input.Gamepad.axisState[0][GAMEPAD_AXIS_LEFT_Y] < 0.2f) &&
        (CORE.Input.Gamepad.axisState[0][GAMEPAD_AXIS_LEFT_Y] > -0.2f)) CORE.Input.Gamepad.axisState[0][GAMEPAD_AXIS_LEFT_Y] = 0.0f;
#endif

#if !PSP_PAD_STRICT
    // Control arm: keep the old right-stick bytes and their deadzone.
    CORE.Input.Gamepad.axisState[0][GAMEPAD_AXIS_RIGHT_X] = ((float)pad.Rsrv[0] - 128.0f)/128.0f;
    CORE.Input.Gamepad.axisState[0][GAMEPAD_AXIS_RIGHT_Y] = ((float)pad.Rsrv[1] - 128.0f)/128.0f;
    if ((CORE.Input.Gamepad.axisState[0][GAMEPAD_AXIS_RIGHT_X] < 0.2f) &&
        (CORE.Input.Gamepad.axisState[0][GAMEPAD_AXIS_RIGHT_X] > -0.2f)) CORE.Input.Gamepad.axisState[0][GAMEPAD_AXIS_RIGHT_X] = 0.0f;
    if ((CORE.Input.Gamepad.axisState[0][GAMEPAD_AXIS_RIGHT_Y] < 0.2f) &&
        (CORE.Input.Gamepad.axisState[0][GAMEPAD_AXIS_RIGHT_Y] > -0.2f)) CORE.Input.Gamepad.axisState[0][GAMEPAD_AXIS_RIGHT_Y] = 0.0f;
#endif

#if PSP_PAD_AS_KEYBOARD
    // D-pad or calibrated nub -> arrows. Frozen frames return above, so this block
    // only runs on a clean sample; a release during HOLD yields one release edge.
    {
        float lx = CORE.Input.Gamepad.axisState[0][GAMEPAD_AXIS_LEFT_X];
        float ly = CORE.Input.Gamepad.axisState[0][GAMEPAD_AXIS_LEFT_Y];
        int keyDown[4];
        const int keyCode[4] = { KEY_LEFT, KEY_RIGHT, KEY_UP, KEY_DOWN };

        keyDown[0] = ((pad.Buttons & PSP_CTRL_LEFT) != 0) || (lx < -PSP_PAD_KEY_THRESHOLD);
        keyDown[1] = ((pad.Buttons & PSP_CTRL_RIGHT) != 0) || (lx > PSP_PAD_KEY_THRESHOLD);
        keyDown[2] = ((pad.Buttons & PSP_CTRL_UP) != 0) || (ly < -PSP_PAD_KEY_THRESHOLD);
        keyDown[3] = ((pad.Buttons & PSP_CTRL_DOWN) != 0) || (ly > PSP_PAD_KEY_THRESHOLD);

        for (int i = 0; i < 4; i++)
        {
            int key = keyCode[i];

            if (keyDown[i] && !CORE.Input.Keyboard.previousKeyState[key])
            {
                if (CORE.Input.Keyboard.keyPressedQueueCount < MAX_KEY_PRESSED_QUEUE)
                {
                    CORE.Input.Keyboard.keyPressedQueue[CORE.Input.Keyboard.keyPressedQueueCount] = key;
                    CORE.Input.Keyboard.keyPressedQueueCount++;
                }
            }

            CORE.Input.Keyboard.currentKeyState[key] = keyDown[i]? 1 : 0;
        }
    }
#endif

#if PSP_PAD_AS_MOUSE
    // Calibrated axes are already 0 inside the deadzone, so the cursor does not drift.
    {
        float lx = CORE.Input.Gamepad.axisState[0][GAMEPAD_AXIS_LEFT_X];
        float ly = CORE.Input.Gamepad.axisState[0][GAMEPAD_AXIS_LEFT_Y];

        CORE.Input.Mouse.previousPosition = CORE.Input.Mouse.currentPosition;

        if ((lx != 0.0f) || (ly != 0.0f))
        {
            float dt = GetFrameTime();

            CORE.Input.Mouse.currentPosition.x += lx*PSP_PAD_MOUSE_SPEED*dt;
            CORE.Input.Mouse.currentPosition.y += ly*PSP_PAD_MOUSE_SPEED*dt;

            if (CORE.Input.Mouse.currentPosition.x < 0.0f) CORE.Input.Mouse.currentPosition.x = 0.0f;
            if (CORE.Input.Mouse.currentPosition.y < 0.0f) CORE.Input.Mouse.currentPosition.y = 0.0f;
            if (CORE.Input.Mouse.currentPosition.x > (float)(CORE.Window.screen.width - 1)) CORE.Input.Mouse.currentPosition.x = (float)(CORE.Window.screen.width - 1);
            if (CORE.Input.Mouse.currentPosition.y > (float)(CORE.Window.screen.height - 1)) CORE.Input.Mouse.currentPosition.y = (float)(CORE.Window.screen.height - 1);
        }
    }
#endif

#if PSP_PAD_FREEZE
    if (pspResync)
    {
        memcpy(CORE.Input.Gamepad.previousButtonState[0], CORE.Input.Gamepad.currentButtonState[0],
            sizeof(CORE.Input.Gamepad.previousButtonState[0]));
        CORE.Input.Gamepad.lastButtonPressed = 0;
#if PSP_PAD_LATCH
        {
            SceCtrlLatch discard = { 0 };
            sceCtrlReadLatch(&discard);
        }
#endif
        TRACELOG(LOG_INFO, "PAD unfreeze frames=%u", pspFrozenFrames);
        pspFrozenFrames = 0;
        pspResync = 0;
    }
    else
#endif
    {
        unsigned int snapshotNewMask = 0;
        for (int i = 0; i < MAX_GAMEPAD_BUTTONS; i++)
        {
            if (CORE.Input.Gamepad.currentButtonState[0][i] && !CORE.Input.Gamepad.previousButtonState[0][i])
            {
                snapshotNewMask |= (1u << i);
                if (CORE.Input.Gamepad.lastButtonPressed == GAMEPAD_BUTTON_UNKNOWN)
                    CORE.Input.Gamepad.lastButtonPressed = i;
            }
        }

#if PSP_PAD_LATCH
        {
            SceCtrlLatch latch = { 0 };
            unsigned int pspMappedButtons = 0;
            int latchPressed = 0;

            sceCtrlReadLatch(&latch);
            for (int i = 0; i < PSP_BUTTON_MAP_COUNT; i++) pspMappedButtons |= pspButtonMap[i].psp;

            for (int i = 0; i < PSP_BUTTON_MAP_COUNT; i++)
            {
                if ((latch.uiMake & pspButtonMap[i].psp) != 0)
                {
                    if ((snapshotNewMask & (1u << pspButtonMap[i].raylib)) == 0) pspStats.latch_only++;
                }
            }

            // First mapped make in GAMEPAD_BUTTON_* enum order.
            for (int b = 1; b < MAX_GAMEPAD_BUTTONS; b++)
            {
                for (int i = 0; i < PSP_BUTTON_MAP_COUNT; i++)
                {
                    if ((pspButtonMap[i].raylib == b) && ((latch.uiMake & pspButtonMap[i].psp) != 0))
                    {
                        latchPressed = b;
                        break;
                    }
                }
                if (latchPressed != 0) break;
            }

            if ((CORE.Input.Gamepad.lastButtonPressed == GAMEPAD_BUTTON_UNKNOWN) && (latchPressed != 0))
                CORE.Input.Gamepad.lastButtonPressed = latchPressed;

            if (latch.uiMake != 0)
            {
                TRACELOG(LOG_INFO, "PAD latchMake=0x%08X mapped=0x%08X latchOnly=%u",
                    latch.uiMake, pspMappedButtons, pspStats.latch_only);
            }
        }
#else
        (void)snapshotNewMask;
#endif

#if !PSP_PAD_FREEZE
        if ((CORE.Input.Gamepad.lastButtonPressed != GAMEPAD_BUTTON_UNKNOWN) && pspSuppressedPrev)
        {
            pspStats.spurious++;
            TRACELOG(LOG_INFO, "PAD spurious=%u", pspStats.spurious);
        }
        pspSuppressedPrev = (pad.Buttons & (PSP_CTRL_HOME | PSP_CTRL_HOLD)) != 0;
#endif
    }
}

// Get name of a QWERTY key on the current keyboard layout
// NOTE: A PSP has no keyboard and therefore no layout. The function is part of
// raylib's public API, so it returns an empty string rather than being absent;
// nothing is logged because tools may call it once per key per frame.
const char *GetKeyName(int key)
{
    (void)key;
    return "";
}

// Set gamepad vibration
// NOTE: No PSP model has rumble motors. This stays a no-op so portable game code
// that calls it keeps linking and running; the message is debug-level and emitted
// once, so a game cannot flood the log by vibrating every tick.
void SetGamepadVibration(int gamepad, float leftMotor, float rightMotor, float duration)
{
    (void)gamepad; (void)leftMotor; (void)rightMotor; (void)duration;

    static bool vibrationWarned = false;
    if (!vibrationWarned)
    {
        vibrationWarned = true;
        TRACELOG(LOG_DEBUG, "SetGamepadVibration() ignored: PSP has no rumble motors");
    }
}

//----------------------------------------------------------------------------------
// Module Internal Functions Definition
//----------------------------------------------------------------------------------
/* Exit callback */
int exit_callback(int arg1, int arg2, void *common)
{
    CORE.Window.shouldClose = true;
    sceKernelExitGame();
    return 0;
}

/* Callback thread */
int CallbackThread(SceSize args, void *argp)
{
    int cbid;

    cbid = sceKernelCreateCallback("Exit Callback", exit_callback, NULL);
    sceKernelRegisterExitCallback(cbid);
    sceKernelSleepThreadCB();

    return 0;
}

/* Sets up the callback thread and returns its thread id */
int SetupCallbacks(void)
{
    int thid = 0;

    thid = sceKernelCreateThread("update_thread", CallbackThread, 0x11, 0xFA0, 0, 0);
    if(thid >= 0)
    {
        sceKernelStartThread(thid, 0, 0);
    }

    return thid;
}

void CustomLog(int msgType, const char *text, va_list args)
{
    char buffer[2048] = { 0 };


    vsnprintf(buffer,2048, text, args);
    buffer[2048-1] = 0;


    switch (msgType)
    {
        case LOG_TRACE: printf("[PSP][TRACE]%s\n",buffer); break;
        case LOG_DEBUG: printf("[PSP][DEBUG]%s\n",buffer); break;
        case LOG_INFO: printf("[PSP][INFO]%s\n",buffer); break;
        case LOG_WARNING: printf("[PSP][WARNING]%s\n",buffer); break;
        case LOG_ERROR: printf("[PSP][ERROR]%s\n",buffer); break;
        case LOG_FATAL: printf("[PSP][FATAL]%s\n",buffer); break;
        default: break;
    }
}
// Initialize platform: graphics, inputs and more
int InitPlatform(void)
{
    //call backs
    SetupCallbacks();
    //init control
    sceCtrlSetSamplingCycle(PSP_PAD_CYCLE);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);

    // Sprint 043 (P8-S3): record the firmware idle pair before changing it, then apply
    // the measured pair only when the idle arm is selected.
    {
        int idleReset = -1;
        int idleBack = -1;
        int idleGet = sceCtrlGetIdleCancelThreshold(&idleReset, &idleBack);
        TRACELOG(LOG_INFO, "PAD idle get ret=%d reset=%d back=%d", idleGet, idleReset, idleBack);
#if PSP_PAD_IDLE
        int idleSet = sceCtrlSetIdleCancelThreshold(PSP_PAD_IDLE_RESET, PSP_PAD_IDLE_BACK);
        TRACELOG(LOG_INFO, "PAD idle set reset=%d back=%d ret=%d",
            PSP_PAD_IDLE_RESET, PSP_PAD_IDLE_BACK, idleSet);
#endif
    }

    // Mark the first gamepad as ready and advertise exactly one pad.
    CORE.Input.Gamepad.ready[0] = true;
    CORE.Input.Gamepad.axisCount[0] = 2;
    CORE.Input.Gamepad.axisCount[1] = 0;
    CORE.Input.Gamepad.ready[1] = false;
    CORE.Input.Gamepad.name[1][0] = '\0';

    // Set PSP controller name
    strcpy(CORE.Input.Gamepad.name[0], "PSP Controller");

    // Fill every axis with a platform-neutral default once (sprint 041).
    memset(CORE.Input.Gamepad.axisState[0], 0, sizeof(CORE.Input.Gamepad.axisState[0]));
#if PSP_PAD_STRICT
    CORE.Input.Gamepad.axisState[0][GAMEPAD_AXIS_LEFT_TRIGGER] = -1.0f;
    CORE.Input.Gamepad.axisState[0][GAMEPAD_AXIS_RIGHT_TRIGGER] = -1.0f;
#endif
    TRACELOG(LOG_INFO, "PAD contract=%d fail=%d", PSP_PAD_STRICT, PSP_PAD_FAIL);
    TRACELOG(LOG_INFO, "PAD poll=%d latch=%d freeze=%d cycle=%d trace=%d force=%d stats=%d",
        PSP_PAD_POLL, PSP_PAD_LATCH, PSP_PAD_FREEZE, PSP_PAD_CYCLE, PSP_PAD_TRACE, PSP_PAD_FORCE_FREEZE,
        PSP_PAD_STATS_INTERVAL);
    TRACELOG(LOG_INFO, "PAD calib=%d cx=%d cy=%d rxp=%d rxn=%d ryp=%d ryn=%d dz=%.2f",
        PSP_PAD_CALIB, PSP_NUB_CX, PSP_NUB_CY, PSP_NUB_RXP, PSP_NUB_RXN, PSP_NUB_RYP, PSP_NUB_RYN,
        (double)PSP_NUB_DZ);

    // NOTE: 6.2 has no CORE.Window.fullscreen member; fullscreen is a flag only.
    CORE.Window.flags |= FLAG_FULLSCREEN_MODE;

    CORE.Window.display.width = CORE.Window.screen.width;            // User desired width
    CORE.Window.display.height = CORE.Window.screen.height;          // User desired height
    CORE.Window.render.width = CORE.Window.screen.width;
    CORE.Window.render.height = CORE.Window.screen.height;
    CORE.Window.eventWaiting = false;
    CORE.Window.screenScale = MatrixIdentity();     // No draw scaling required by default
    CORE.Window.currentFbo.width = CORE.Window.screen.width;
    CORE.Window.currentFbo.height = CORE.Window.screen.height;
    CORE.Input.Mouse.currentPosition.x = (float)CORE.Window.screen.width/2.0f;
    CORE.Input.Mouse.currentPosition.y = (float)CORE.Window.screen.height/2.0f;
    CORE.Input.Mouse.scale = (Vector2){ 1.0f, 1.0f };


    // NOTE: MSAA is not wired to EGL in this port: the attribute list below is
    // fixed, and pspGL's config selection does not consider EGL_SAMPLES. The
    // previous code computed samples/sampleBuffer and never passed them on, which
    // logged a request that could not be honoured.
    const EGLint attrib_list [] = {
        EGL_RED_SIZE, 1,
        EGL_GREEN_SIZE, 1,
        EGL_BLUE_SIZE, 1,
        EGL_ALPHA_SIZE, 0,
        EGL_DEPTH_SIZE, 1,
        EGL_NONE
    };
    

    EGLint numConfigs = 0;

    // Get an EGL device connection
    platform.device = eglGetDisplay(0);
    if (platform.device == EGL_NO_DISPLAY)
    {
        TRACELOG(LOG_WARNING, "PLATFORM: Failed to initialize EGL device");
        return -1;
    }

    // Initialize the EGL device connection
    if (eglInitialize(platform.device, NULL, NULL) == EGL_FALSE)
    {
        // If all of the calls to eglInitialize returned EGL_FALSE then an error has occurred.
        TRACELOG(LOG_WARNING, "PLATFORM: Failed to initialize EGL device");
        return -1;
    }

    
    // Set rendering API
    int ret;
   /* int ret=eglBindAPI(EGL_OPENGL_ES_API);
    if(!ret)
    {
        ret=eglGetError();
        TRACELOG(LOG_ERROR,"PLATFORM:  eglBindAPI failed: 0x%08X",ret);
        return -1;
    }
    TRACELOG(LOG_INFO,"PLATFORM: eglBindAPI success.");*/

	// Get an appropriate EGL framebuffer configuration
    ret=eglChooseConfig(platform.device, attrib_list, &platform.config, 1, &numConfigs);
    if(!ret)
    {
        ret=eglGetError();
        TRACELOG(LOG_ERROR,"PLATFORM: eglChooseConfig failed: 0x%08X",ret);
        return -1;
    }
    if (numConfigs!=1)
    {
        TRACELOG(LOG_ERROR,"PLATFORM: No available configuration found.");
        return -1;
    }
    TRACELOG(LOG_INFO,"PLATFORM: eglChooseConfig success.");
    // Create an EGL rendering context
    platform.context = eglCreateContext(platform.device, platform.config, NULL, NULL);
    if (platform.context == EGL_NO_CONTEXT)
    {
        TRACELOG(LOG_WARNING, "PLATFORM: Failed to create EGL context");
        return -1;
    }

    // Create an EGL window surface
    //---------------------------------------------------------------------------------
    //EGLint displayFormat = 0;

    // EGL_NATIVE_VISUAL_ID is an attribute of the EGLConfig that is guaranteed to be accepted by ANativeWindow_setBuffersGeometry()
    // As soon as we picked a EGLConfig, we can safely reconfigure the ANativeWindow buffers to match, using EGL_NATIVE_VISUAL_ID
    //eglGetConfigAttrib(platform.device, platform.config, EGL_NATIVE_VISUAL_ID, &displayFormat);

    // At this point we need to manage render size vs screen size
    // NOTE: This function use and modify global module variables:
    //  -> CORE.Window.screen.width/CORE.Window.screen.height
    //  -> CORE.Window.render.width/CORE.Window.render.height
    //  -> CORE.Window.screenScale
    // NOTE: 6.2's SetupViewport() is not 5.5's SetupFramebuffer(): it issues real
    // GL calls (rlViewport, rlOrtho, rlLoadIdentity). Calling it here - before the
    // surface exists and before eglMakeCurrent - faults inside pspGL with a data
    // bus error, because there is no current context yet. It is called after
    // eglMakeCurrent below instead.

    //ANativeWindow_setBuffersGeometry(platform.app->window, CORE.Window.render.width, CORE.Window.render.height, displayFormat);
    //ANativeWindow_setBuffersGeometry(platform.app->window, 0, 0, displayFormat);       // Force use of native display size

    platform.surface = eglCreateWindowSurface(platform.device, platform.config, 0, NULL);
    if(platform.surface==EGL_NO_SURFACE)
    {
        ret=eglGetError();
        TRACELOG(LOG_ERROR,"PLATFORM: eglCreateWindowSurface failed: 0x%08X",ret);
        return -1;
    }
    TRACELOG(LOG_INFO,"PLATFORM: eglCreateWindowSurface success.");

    // There must be at least one frame displayed before the buffers are swapped
    //eglSwapInterval(platform.device, 1);
    // NOTE: eglSwapInterval() is called after eglMakeCurrent() below. Calling it
    // here faults inside pspGL with a data bus error, exactly like the
    // SetupViewport case in sprint 001: no context is current yet.

    if (eglMakeCurrent(platform.device, platform.surface, platform.surface, platform.context) == EGL_FALSE)
    {
        ret=eglGetError();
        TRACELOG(LOG_ERROR,"PLATFORM: Failed to attach EGL rendering context to EGL surface 0x%08X",ret);
        return -1;
    }
    else
    {
        CORE.Window.render.width = CORE.Window.screen.width;
        CORE.Window.render.height = CORE.Window.screen.height;
        CORE.Window.currentFbo.width = CORE.Window.render.width;
        CORE.Window.currentFbo.height = CORE.Window.render.height;

        // Now that a context is current, the GL-side viewport/projection setup is safe
        SetupViewport(CORE.Window.display.width, CORE.Window.display.height);

#if defined(PSP_DISABLE_VSYNC)
        // Measurement switch (sprint 004): start with the swap unpaced.
        pspVsync = 0;
#endif
        // Apply the runtime setting (on by default) now that the context is current.
        eglSwapInterval(platform.device, pspVsync);

        TRACELOG(LOG_INFO, "PLATFORM: Device initialized successfully");
        TRACELOG(LOG_INFO, "    > Display size: %i x %i", CORE.Window.display.width, CORE.Window.display.height);
        TRACELOG(LOG_INFO, "    > Screen size:  %i x %i", CORE.Window.screen.width, CORE.Window.screen.height);
        TRACELOG(LOG_INFO, "    > Render size:  %i x %i", CORE.Window.render.width, CORE.Window.render.height);
        TRACELOG(LOG_INFO, "    > Viewport offsets: %i, %i", CORE.Window.renderOffset.x, CORE.Window.renderOffset.y);
    }

    // Load OpenGL extensions
    // NOTE: GL procedures address loader is required to load extensions
    //rlLoadExtensions(eglGetProcAddress);
    //const char *gl_exts = (char *) glGetString(GL_EXTENSIONS);
    TRACELOG(LOG_INFO,"PLATFORM: GL_VENDOR:   \"%s\"", glGetString(GL_VENDOR));
    TRACELOG(LOG_INFO,"PLATFORM: GL_VERSION:  \"%s\"", glGetString(GL_VERSION));
    TRACELOG(LOG_INFO,"PLATFORM: GL_RENDERER: \"%s\"", glGetString(GL_RENDERER));
    //TRACELOG(LOG_INFO,"PLATFORM: SL_VERSION:  \"%s\"", glGetString(GL_SHADING_LANGUAGE_VERSION));
    CORE.Window.ready = true;

    // Initialize hi-res timer
    InitTimer();

    // Initialize base path for storage
    CORE.Storage.basePath = GetWorkingDirectory();
#if defined(PSP_CULL) && defined(PSP_TRI_FAST)
    TRACELOG(LOG_INFO, "RLGL: PSP_CULL=%d PSP_TRI_FAST=%d", PSP_CULL, PSP_TRI_FAST);
#endif
    TRACELOG(LOG_INFO, "PLATFORM: Initialized");

    return 0;
}

// Close platform
void ClosePlatform(void)
{
    // TODO: De-initialize graphics, inputs and more
    sceKernelExitGame();
}
//
// GL11 vertex batch - state and emission (sprint 007, made inline in sprint 009)
//----------------------------------------------------------------------------------
// The vertex-level entry points (rlBegin, rlVertex*, rlTexCoord2f, rlColor*, rlSetTexture)
// live in rlgl.h as static inline functions: a 2D sprite used to pay a dozen calls that
// crossed into rcore.o and then into this file, with six float arguments on the stack.
// What cannot be inlined is here - the single buffer every caller shares, the emission,
// and the per-frame reset.
#if defined(GRAPHICS_API_OPENGL_11) && defined(PSP_BATCH) && (PSP_BATCH == 1)

// The batcher spells its GL enums out in rlgl.h (that section runs before the GL
// headers). Assert here, where <GL/gl.h> is in scope, that they have not drifted.
typedef char rlpsp_gl_enum_check[
    ((GL_LINES == RLPSP_GL_LINES) && (GL_TRIANGLES == RLPSP_GL_TRIANGLES) &&
     (GL_SPRITES_PSP == RLPSP_GL_SPRITES))? 1 : -1];

// One instance for the whole library. rlgl.h declares it as extern, and six translation
// units include that header, so anything static there would be six buffers and the flush
// at EndDrawing() would see an empty one.
RLPspBatch rlpsp = { .boundTex = 0xFFFFFFFFu };  // Nothing bound yet, so the first flush binds

#if defined(RLPSP_LOCK_ARRAYS)
// pspGL's GL_EXT_vertex_array entry points: locking makes the driver copy the arrays
// into GE form once and draw from that cache. Declared here because rlgl.h includes
// <GL/gl.h> only, not <GL/glext.h>.
extern void glLockArraysEXT(GLint first, GLsizei count);
extern void glUnlockArraysEXT(void);
#endif

// Emit every buffered vertex as one vertex-array draw
void rlPspBatchFlush(void)
{
    if (rlpsp.count == 0)
    {
        rlpsp.quadN = 0;            // A flush can only land between primitives
        return;
    }

    // Bind only when the texture actually changes (sprint 009 T2). A capacity split or
    // a primitive-class change inside one texture run used to re-bind it every time.
    if (rlpsp.tex != rlpsp.boundTex)
    {
        if (rlpsp.tex > 0)
        {
            glEnable(GL_TEXTURE_2D);
            glBindTexture(GL_TEXTURE_2D, rlpsp.tex);
        }
        else
        {
            glDisable(GL_TEXTURE_2D);
            glBindTexture(GL_TEXTURE_2D, 0);
        }
        rlpsp.boundTex = rlpsp.tex;
    }

    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);
    glVertexPointer(3, GL_FLOAT, sizeof(RLPspVertex), &rlpsp.verts[0].x);
    glTexCoordPointer(2, GL_FLOAT, sizeof(RLPspVertex), &rlpsp.verts[0].u);
    glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(RLPspVertex), &rlpsp.verts[0].r);

#if defined(RLPSP_LOCK_ARRAYS)
    glLockArraysEXT(0, rlpsp.count);
    glDrawArrays(rlpsp.mode, 0, rlpsp.count);
    glUnlockArraysEXT();
#else
    glDrawArrays(rlpsp.mode, 0, rlpsp.count);
#endif

#ifdef RLPSP_BATCH_TRACE
    TraceLog(LOG_INFO, "BATCH flush tex=%u mode=%i count=%i alt=%i", rlpsp.tex, rlpsp.mode,
             rlpsp.count, rlpsp.alternations);
#endif

    glDisableClientState(GL_COLOR_ARRAY);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    glDisableClientState(GL_VERTEX_ARRAY);

    rlpsp.count = 0;
    rlpsp.quadN = 0;
}

// End of frame: emit what is left and start the alternation budget over
void rlPspBatchFrameEnd(void)
{
    rlPspBatchFlush();
    rlpsp.alternations = 0;
}

#endif

// EOF
