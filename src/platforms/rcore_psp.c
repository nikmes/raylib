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
    TRACELOG(LOG_WARNING, "SetGamepadMappings() not implemented on target platform");
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

    // Save previous gamepad button states
    for (int i = 0; i < MAX_GAMEPAD_BUTTONS; i++)
    {
        CORE.Input.Gamepad.previousButtonState[0][i] = CORE.Input.Gamepad.currentButtonState[0][i];
    }

    // Reset last gamepad button/axis registered state
    CORE.Input.Gamepad.lastButtonPressed = 0; // GAMEPAD_BUTTON_UNKNOWN

    // Register previous touch states
    for (int i = 0; i < MAX_TOUCH_POINTS; i++) CORE.Input.Touch.previousTouchState[i] = CORE.Input.Touch.currentTouchState[i];

    // Reset touch positions
    // TODO: It resets on target platform the mouse position and not filled again until a move-event,
    // so, if mouse is not moved it returns a (0, 0) position... this behaviour should be reviewed!
    //for (int i = 0; i < MAX_TOUCH_POINTS; i++) CORE.Input.Touch.position[i] = (Vector2){ 0, 0 };

    // Register previous keys states
    // NOTE: Android supports up to 260 keys
    //for (int i = 0; i < 260; i++)
    //{
    //    CORE.Input.Keyboard.previousKeyState[i] = CORE.Input.Keyboard.currentKeyState[i];
    //    CORE.Input.Keyboard.keyRepeatInFrame[i] = 0;
    //}

    // Poll input events for PSP
    SceCtrlData pad;
    sceCtrlReadBufferPositive(&pad, 1); // Read PSP controller input

    // Map PSP buttons to raylib gamepad buttons
    CORE.Input.Gamepad.currentButtonState[0][GAMEPAD_BUTTON_RIGHT_FACE_DOWN] = (pad.Buttons & PSP_CTRL_CROSS) ? 1 : 0;    // Cross button
    CORE.Input.Gamepad.currentButtonState[0][GAMEPAD_BUTTON_RIGHT_FACE_RIGHT] = (pad.Buttons & PSP_CTRL_CIRCLE) ? 1 : 0;  // Circle button
    CORE.Input.Gamepad.currentButtonState[0][GAMEPAD_BUTTON_RIGHT_FACE_LEFT] = (pad.Buttons & PSP_CTRL_SQUARE) ? 1 : 0;   // Square button
    CORE.Input.Gamepad.currentButtonState[0][GAMEPAD_BUTTON_RIGHT_FACE_UP] = (pad.Buttons & PSP_CTRL_TRIANGLE) ? 1 : 0;   // Triangle button

    // DPAD
    CORE.Input.Gamepad.currentButtonState[0][GAMEPAD_BUTTON_LEFT_FACE_UP] = (pad.Buttons & PSP_CTRL_UP) ? 1 : 0;
    CORE.Input.Gamepad.currentButtonState[0][GAMEPAD_BUTTON_LEFT_FACE_RIGHT] = (pad.Buttons & PSP_CTRL_RIGHT) ? 1 : 0;
    CORE.Input.Gamepad.currentButtonState[0][GAMEPAD_BUTTON_LEFT_FACE_DOWN] = (pad.Buttons & PSP_CTRL_DOWN) ? 1 : 0;
    CORE.Input.Gamepad.currentButtonState[0][GAMEPAD_BUTTON_LEFT_FACE_LEFT] = (pad.Buttons & PSP_CTRL_LEFT) ? 1 : 0;

    // Other buttons
    CORE.Input.Gamepad.currentButtonState[0][GAMEPAD_BUTTON_LEFT_TRIGGER_1] = (pad.Buttons & PSP_CTRL_LTRIGGER) ? 1 : 0;
    CORE.Input.Gamepad.currentButtonState[0][GAMEPAD_BUTTON_RIGHT_TRIGGER_1] = (pad.Buttons & PSP_CTRL_RTRIGGER) ? 1 : 0;
    CORE.Input.Gamepad.currentButtonState[0][GAMEPAD_BUTTON_MIDDLE_LEFT] = (pad.Buttons & PSP_CTRL_SELECT) ? 1 : 0;      // Select button
    CORE.Input.Gamepad.currentButtonState[0][GAMEPAD_BUTTON_MIDDLE_RIGHT] = (pad.Buttons & PSP_CTRL_START) ? 1 : 0;      // Start button

    // Set analog stick values
    CORE.Input.Gamepad.axisCount[0] = 2;
    CORE.Input.Gamepad.axisCount[1] = 2; // Available on PSP Go with DualShock 3
    
    // Normalize analog values from [0..255] to [-1..1]
    CORE.Input.Gamepad.axisState[0][GAMEPAD_AXIS_LEFT_X] = ((float)pad.Lx - 128.0f)/128.0f;
    CORE.Input.Gamepad.axisState[0][GAMEPAD_AXIS_LEFT_Y] = ((float)pad.Ly - 128.0f)/128.0f;
    
    // Apply deadzone to analog stick input (values in range [-0.2..0.2] are discarded)
    float deadZone = 0.2f;
    if ((CORE.Input.Gamepad.axisState[0][GAMEPAD_AXIS_LEFT_X] < deadZone) && 
        (CORE.Input.Gamepad.axisState[0][GAMEPAD_AXIS_LEFT_X] > -deadZone)) CORE.Input.Gamepad.axisState[0][GAMEPAD_AXIS_LEFT_X] = 0.0f;
    if ((CORE.Input.Gamepad.axisState[0][GAMEPAD_AXIS_LEFT_Y] < deadZone) && 
        (CORE.Input.Gamepad.axisState[0][GAMEPAD_AXIS_LEFT_Y] > -deadZone)) CORE.Input.Gamepad.axisState[0][GAMEPAD_AXIS_LEFT_Y] = 0.0f;

    // Normalize right analog values from [0..255] to [-1..1]
    // NOTE: Right analog stick is not available on PSP, but it is apparently available on PSP Go with DualShock 3
    CORE.Input.Gamepad.axisState[0][GAMEPAD_AXIS_RIGHT_X] = ((float)pad.Rsrv[0] - 128.0f)/128.0f;
    CORE.Input.Gamepad.axisState[0][GAMEPAD_AXIS_RIGHT_Y] = ((float)pad.Rsrv[1] - 128.0f)/128.0f;

    // Apply deadzone to right analog stick input (values in range [-0.2..0.2] are discarded)
    if ((CORE.Input.Gamepad.axisState[0][GAMEPAD_AXIS_RIGHT_X] < deadZone) && 
        (CORE.Input.Gamepad.axisState[0][GAMEPAD_AXIS_RIGHT_X] > -deadZone)) CORE.Input.Gamepad.axisState[0][GAMEPAD_AXIS_RIGHT_X] = 0.0f;
    if ((CORE.Input.Gamepad.axisState[0][GAMEPAD_AXIS_RIGHT_Y] < deadZone) &&
        (CORE.Input.Gamepad.axisState[0][GAMEPAD_AXIS_RIGHT_Y] > -deadZone)) CORE.Input.Gamepad.axisState[0][GAMEPAD_AXIS_RIGHT_Y] = 0.0f;
    
    // Check if any button was pressed for the first time this frame
    for (int i = 0; i < MAX_GAMEPAD_BUTTONS; i++)
    {
        if (CORE.Input.Gamepad.currentButtonState[0][i] && !CORE.Input.Gamepad.previousButtonState[0][i])
        {
            CORE.Input.Gamepad.lastButtonPressed = i;
            break;
        }
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
// that calls it keeps linking and running; the warning is emitted once, not per
// frame, so a game cannot flood the log by vibrating every tick.
void SetGamepadVibration(int gamepad, float leftMotor, float rightMotor, float duration)
{
    (void)gamepad; (void)leftMotor; (void)rightMotor; (void)duration;

    static bool vibrationWarned = false;
    if (!vibrationWarned)
    {
        vibrationWarned = true;
        TRACELOG(LOG_WARNING, "SetGamepadVibration() ignored: PSP has no rumble motors");
    }
}

//----------------------------------------------------------------------------------
// Module Internal Functions Definition
//----------------------------------------------------------------------------------
/* Exit callback */
int exit_callback(int arg1, int arg2, void *common)
{
    CORE.Window.shouldClose = true;
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
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);

    // Mark the first gamepad as ready
    CORE.Input.Gamepad.ready[0] = true;
    
    // Set PSP controller name
    strcpy(CORE.Input.Gamepad.name[0], "PSP Controller");

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
        // Measurement switch (sprint 004): 0 asks pspGL not to pace the swap to the
        // display. Off by default so shipped behaviour is unchanged.
        eglSwapInterval(platform.device, 0);
#endif

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
    TRACELOG(LOG_INFO, "PLATFORM: Initialized");

    return 0;
}

// Close platform
void ClosePlatform(void)
{
    // TODO: De-initialize graphics, inputs and more
    sceKernelExitGame();
}

//----------------------------------------------------------------------------------
// GL11 vertex batch (sprint 007, enabled with PSP_BATCH=1)
//----------------------------------------------------------------------------------
// The immediate-mode path calls glBegin()/glVertex3f()/glEnd() once per primitive,
// so a frame with 1025 sprites submits 1025 draw calls. Here the same vertices
// accumulate in one static buffer and leave as a single glDrawArrays() per texture.
//
// NOTE: The state lives in this file, not in rlgl.h, on purpose. rlgl.h is included
// by rcore.c, rshapes.c, rtextures.c, rtext.c and rmodels.c; a static buffer in the
// header would be one copy per translation unit, so the vertices pushed by
// rtextures.c would be invisible to the flush rcore.c runs at EndDrawing().
#if defined(GRAPHICS_API_OPENGL_11) && defined(PSP_BATCH) && (PSP_BATCH == 1)

#ifndef RLPSP_BATCH_CAP
    #define RLPSP_BATCH_CAP 4096    // Vertices held per batch (340 quads), 24 bytes each
#endif

// Interleaved vertex, 24 bytes: the offsets of the three arrays below are what
// glVertexPointer()/glTexCoordPointer()/glColorPointer() are pointed at
typedef struct {
    float u, v;                     // Texture coordinates
    unsigned char r, g, b, a;       // Vertex color
    float x, y, z;                  // Position
} RLPspVertex;

static RLPspVertex rlPspVerts[RLPSP_BATCH_CAP];
static int rlPspCount = 0;                  // Vertices currently buffered
static int rlPspMode = 0;                   // GL primitive mode of the buffered run
static int rlPspQuads = 0;                  // Current run is RL_QUADS, expanded to triangles
static unsigned int rlPspTex = 0;           // Texture the buffered vertices belong to
static unsigned int rlPspPendTex = 0;       // Texture the following vertices will use
static float rlPspU = 0.0f;                 // Current texture coordinate
static float rlPspV = 0.0f;
static unsigned char rlPspR = 255;          // Current vertex color
static unsigned char rlPspG = 255;
static unsigned char rlPspB = 255;
static unsigned char rlPspA = 255;
static RLPspVertex rlPspQuad[4];            // Corners of the quad being assembled
static int rlPspQuadN = 0;

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
    if (rlPspCount == 0)
    {
        rlPspQuadN = 0;
        return;
    }

    if (rlPspTex > 0)
    {
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, rlPspTex);
    }
    else
    {
        glDisable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);
    glVertexPointer(3, GL_FLOAT, sizeof(RLPspVertex), &rlPspVerts[0].x);
    glTexCoordPointer(2, GL_FLOAT, sizeof(RLPspVertex), &rlPspVerts[0].u);
    glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(RLPspVertex), &rlPspVerts[0].r);

#if defined(RLPSP_LOCK_ARRAYS)
    glLockArraysEXT(0, rlPspCount);
    glDrawArrays(rlPspMode, 0, rlPspCount);
    glUnlockArraysEXT();
#else
    glDrawArrays(rlPspMode, 0, rlPspCount);
#endif

#ifdef RLPSP_BATCH_TRACE
    TraceLog(LOG_INFO, "BATCH flush tex=%u mode=%i count=%i", rlPspTex, rlPspMode, rlPspCount);
#endif

    glDisableClientState(GL_COLOR_ARRAY);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    glDisableClientState(GL_VERTEX_ARRAY);

    rlPspCount = 0;
    rlPspQuadN = 0;             // A flush can only happen between primitives: drop a partial quad
}

// Claim n contiguous slots, settling texture switches and the batch limit first
static RLPspVertex *rlPspReserve(int n)
{
    RLPspVertex *v = NULL;

    if (rlPspPendTex != rlPspTex)
    {
        rlPspBatchFlush();
        rlPspTex = rlPspPendTex;
    }
    if ((rlPspCount + n) > RLPSP_BATCH_CAP) rlPspBatchFlush();

    v = &rlPspVerts[rlPspCount];
    rlPspCount += n;

    return v;
}

// Select the texture for the vertices that follow
// NOTE: Deferred on purpose. Every raylib draw call is rlSetTexture(T), quad,
// rlSetTexture(0): flushing on the change would emit twice per draw call and the
// batch would never hold more than one primitive. The switch is applied when the
// next vertex arrives, so the trailing rlSetTexture(0) costs nothing.
void rlPspBatchSetTexture(unsigned int id)
{
    rlPspPendTex = id;
#ifdef RLPSP_BATCH_TRACE
    TraceLog(LOG_INFO, "BATCH set tex=%u (cur=%u count=%i)", id, rlPspTex, rlPspCount);
#endif
}

// Start a run of primitives
void rlPspBatchBegin(int mode)
{
    int glMode = 0;

    switch (mode)
    {
        // NOTE: RL_QUADS is NOT sent to GL as GL_QUADS. pspGL maps GL_QUADS to a GE
        // triangle fan (pspgl_varray.c, geprim_tab), so a run of N quads would fan
        // from the first vertex of the run and every quad after the first would
        // share it. Quads are expanded to two triangles on push instead.
        case RL_LINES: glMode = GL_LINES; break;
        case RL_TRIANGLES: glMode = GL_TRIANGLES; break;
        case RL_QUADS: glMode = GL_TRIANGLES; break;
        default: break;
    }

    // A run of a different mode cannot share the draw call
    if ((rlPspCount > 0) && (glMode != rlPspMode)) rlPspBatchFlush();

    rlPspMode = glMode;
    rlPspQuads = (mode == RL_QUADS)? 1 : 0;
    if (rlPspQuads == 0) rlPspQuadN = 0;
}

// End a run of primitives: the run is emitted by the next flush, not here
void rlPspBatchEnd(void)
{
}

// Append one vertex
void rlPspBatchVertex(float x, float y, float z)
{
    RLPspVertex *v = NULL;

    if (rlPspQuads == 1)
    {
        // Assemble the quad, then write it out as triangles (0,1,2) and (0,2,3)
        RLPspVertex *q = &rlPspQuad[rlPspQuadN++];

        q->u = rlPspU;
        q->v = rlPspV;
        q->r = rlPspR;
        q->g = rlPspG;
        q->b = rlPspB;
        q->a = rlPspA;
        q->x = x;
        q->y = y;
        q->z = z;

        if (rlPspQuadN == 4)
        {
            rlPspQuadN = 0;
            v = rlPspReserve(6);
            v[0] = rlPspQuad[0];
            v[1] = rlPspQuad[1];
            v[2] = rlPspQuad[2];
            v[3] = rlPspQuad[0];
            v[4] = rlPspQuad[2];
            v[5] = rlPspQuad[3];
        }

        return;
    }

    v = rlPspReserve(1);
    v->u = rlPspU;
    v->v = rlPspV;
    v->r = rlPspR;
    v->g = rlPspG;
    v->b = rlPspB;
    v->a = rlPspA;
    v->x = x;
    v->y = y;
    v->z = z;
}

// Set current texture coordinate
void rlPspBatchTexCoord(float u, float v)
{
    rlPspU = u;
    rlPspV = v;
}

// Set current vertex color
void rlPspBatchColor(unsigned char r, unsigned char g, unsigned char b, unsigned char a)
{
    rlPspR = r;
    rlPspG = g;
    rlPspB = b;
    rlPspA = a;
}

#endif

// EOF
