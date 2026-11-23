/*
 * gu_state_probe - the three rendering-guide gates added in b37.
 *
 *   make MODE=depthmask    control pair (writes on) vs rlDisableDepthMask pair
 *   make MODE=clipplanes   lines crossing the far plane, GU_CLIP_PLANES on vs off
 *   make MODE=dither       constant low-alpha additive fill; counts the dither pattern
 *
 * Each mode renders a few frames, reads the frame back and prints one verdict line.
 * The last frame stays on screen for a capture. Run the clipplanes and dither arms
 * with the matching PSPGU_* library build; depthmask is a single fixed build.
 */

#include <pspkernel.h>
#include <pspgu.h>
#include <stdio.h>
#include <GL/gl.h>

#include "raylib.h"
#include "rlgl.h"

PSP_MODULE_INFO("gu_state_probe", 0, 1, 1);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);

#define FRAMES 6

static void finishProbe(void)
{
    TraceLog(LOG_INFO, "DONE");
    sceKernelSleepThread();
}

/*------------------------------------------------------------------------------------
 * MODE=depthmask
 *------------------------------------------------------------------------------------*/
#ifdef MODE_DEPTHMASK

static void quadZ(float x0, float y0, float x1, float y1, float z, Color c)
{
    Vector3 a = { x0, y0, z }, b = { x0, y1, z }, d = { x1, y0, z }, e = { x1, y1, z };
    DrawTriangle3D(a, b, e, c);
    DrawTriangle3D(a, e, d, c);
}

int main(void)
{
    InitWindow(480, 272, "gu_state_probe depthmask");
    SetTargetFPS(0);
    rlDisableBackfaceCulling();

    Camera3D cam = { 0 };
    cam.position = (Vector3){ 0.0f, 0.0f, 10.0f };
    cam.target = (Vector3){ 0.0f, 0.0f, 0.0f };
    cam.up = (Vector3){ 0.0f, 1.0f, 0.0f };
    cam.fovy = 10.0f;
    cam.projection = CAMERA_ORTHOGRAPHIC;

    for (int frame = 0; frame < FRAMES; frame++)
    {
        BeginDrawing();
            ClearBackground(BLACK);
            BeginMode3D(cam);
                // Control: writes on, so the near blue quad wins over the far green one.
                rlEnableDepthMask();
                quadZ(-7.0f, -2.0f, -1.0f, 2.0f,  0.4f, BLUE);
                quadZ(-7.0f, -2.0f, -1.0f, 2.0f, -0.4f, GREEN);

                // Test: the near blue quad must not write depth, so the far green wins.
                rlDisableDepthMask();
                quadZ(1.0f, -2.0f, 7.0f, 2.0f,  0.4f, BLUE);
                rlEnableDepthMask();
                quadZ(1.0f, -2.0f, 7.0f, 2.0f, -0.4f, GREEN);
            EndMode3D();
        EndDrawing();
    }

    Image img = LoadImageFromScreen();
    if (img.data == NULL) { TraceLog(LOG_WARNING, "DEPTHMASK readback failed"); finishProbe(); }
    Color left = GetImageColor(img, 120, 136);
    Color right = GetImageColor(img, 360, 136);
    UnloadImage(img);

    int pass = (left.b > left.g) && (right.g > right.b);
    TraceLog(LOG_INFO, "DEPTHMASK left=%d,%d,%d right=%d,%d,%d %s",
             left.r, left.g, left.b, right.r, right.g, right.b, pass? "PASS" : "FAIL");

    finishProbe();
    return 0;
}

/*------------------------------------------------------------------------------------
 * MODE=clipplanes
 *------------------------------------------------------------------------------------*/
#elif defined(MODE_CLIPPLANES)

int main(void)
{
    InitWindow(480, 272, "gu_state_probe clipplanes");
    SetTargetFPS(0);
    rlDisableBackfaceCulling();

    Camera3D cam = { 0 };
    cam.position = (Vector3){ 0.0f, 0.0f, 10.0f };
    cam.target = (Vector3){ 0.0f, 0.0f, 0.0f };
    cam.up = (Vector3){ 0.0f, 1.0f, 0.0f };
    cam.fovy = 45.0f;
    cam.projection = CAMERA_PERSPECTIVE;

    for (int frame = 0; frame < FRAMES; frame++)
    {
        BeginDrawing();
            ClearBackground(BLACK);
            BeginMode3D(cam);
                // Every line starts 5 units in front of the camera and ends far beyond
                // the 4000-unit far plane. GL_LINES bypasses the driver's CPU clip pass.
                for (int i = 0; i < 32; i++)
                {
                    float x = -3.0f + (float)i*0.2f;
                    DrawLine3D((Vector3){ x, 0.0f, 5.0f },
                               (Vector3){ x, 0.0f, -100000.0f }, WHITE);
                }

                // Control: a near-only triangle. If this is missing, the draw path is
                // broken, not the clip state.
                DrawTriangle3D((Vector3){ -6.0f, -2.0f, 5.0f },
                               (Vector3){ -3.0f, -2.0f, 5.0f },
                               (Vector3){ -6.0f,  2.0f, 5.0f }, RED);

                // Test: one vertex beyond the far plane. With the normal build the CPU
                // clip pass rewrites this; the meaningful arms use PSPGU_NO_CLIP, where
                // the primitive reaches the GE and GU_CLIP_PLANES decides its fate.
                DrawTriangle3D((Vector3){ 1.0f, -2.0f, 5.0f },
                               (Vector3){ 4.0f, -2.0f, 5.0f },
                               (Vector3){ 1.0f,  2.0f, -100000.0f }, GREEN);
            EndMode3D();
        EndDrawing();
    }

    Image img = LoadImageFromScreen();
    if (img.data == NULL) { TraceLog(LOG_WARNING, "CLIPPLANES readback failed"); finishProbe(); }
    int nonblack = 0, red = 0, green = 0;

    for (int y = 0; y < img.height; y++)
    {
        for (int x = 0; x < img.width; x++)
        {
            Color c = GetImageColor(img, x, y);
            if ((c.r | c.g | c.b) != 0) nonblack++;
            if ((c.r > 200) && (c.g < 80) && (c.b < 80)) red++;
            else if ((c.g > 200) && (c.r < 80) && (c.b < 80)) green++;
        }
    }
    UnloadImage(img);

    TraceLog(LOG_INFO, "CLIPPLANES nonblack=%d red=%d green=%d "
             "(red must be >0; green tells the far-plane result)",
             nonblack, red, green);
    finishProbe();
    return 0;
}

/*------------------------------------------------------------------------------------
 * MODE=dither
 *------------------------------------------------------------------------------------*/
#elif defined(MODE_DITHER)

int main(void)
{
    InitWindow(480, 272, "gu_state_probe dither");
    SetTargetFPS(0);

    for (int frame = 0; frame < FRAMES; frame++)
    {
        BeginDrawing();
            ClearBackground(BLACK);

            // A vertical alpha gradient added to black. Every row is flat along x, so
            // any horizontal alternation inside a row is dither. Different corner colours
            // make the batcher reject the sprite path and emit triangles, which preserve
            // the interpolation the dither is supposed to break up.
            glEnable(GL_DITHER);        // request it explicitly; the scoped build may veto
            glBlendFunc(GL_SRC_ALPHA, GL_ONE);
            if (frame == 0)
                TraceLog(LOG_INFO, "DITHER drawstate=%d", sceGuGetStatus(GU_DITHER));
            DrawRectangleGradientV(0, 0, 480, 272,
                                   (Color){ 96, 96, 96, 0 },
                                   (Color){ 96, 96, 96, 64 });
            // The 2D batcher holds the vertices until EndDrawing; flush now or the draw
            // would inherit the restored alpha blend instead of the additive one.
            rlDrawRenderBatchActive();
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        EndDrawing();
    }

    Image img = LoadImageFromScreen();
    if (img.data == NULL) { TraceLog(LOG_WARNING, "DITHER readback failed"); finishProbe(); }
    int transitions = 0, maxv = 0, distinct = 0;
    unsigned char seen[64] = { 0 };

    for (int y = 80; y <= 200; y += 10)
    {
        int prev = -1;

        for (int x = 20; x < 460; x++)
        {
            Color c = GetImageColor(img, x, y);
            int v = (c.r << 16) | (c.g << 8) | c.b;
            if ((prev >= 0) && (v != prev)) transitions++;
            prev = v;
            if (c.g > maxv) maxv = c.g;
        }

        Color m = GetImageColor(img, 240, y);
        if ((m.g < 64) && !seen[m.g]) { seen[m.g] = 1; distinct++; }
    }

    Color mid = GetImageColor(img, 240, 136);
    UnloadImage(img);

    TraceLog(LOG_INFO, "DITHER transitions=%d max=%d distinct=%d mid=%d,%d,%d state=%d "
             "(scoped: transitions=0; unscoped: >0)",
             transitions, maxv, distinct, mid.r, mid.g, mid.b,
             sceGuGetStatus(GU_DITHER));
    finishProbe();
    return 0;
}

#else
#error "MODE must define MODE_DEPTHMASK, MODE_CLIPPLANES or MODE_DITHER"
#endif
