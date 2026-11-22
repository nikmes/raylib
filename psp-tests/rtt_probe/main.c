/*
 * rtt_probe - render-to-texture gate (sprint 065).
 *
 *   MODE=direct    draw the pattern to the screen (reference)
 *   MODE=rtt       render the pattern into a 256x256 target, sample 1:1
 *   MODE=readback  render into the target and read it back while bound
 *   MODE=depth     near cube drawn first, far cube second; depth must keep the near one
 *   MODE=churn     create/delete textures beside the target, then sample it again
 *   MODE=size      300x200 target, padded and sampled with TexScale
 *
 * LoadRenderTexture returns 0 on a driver without FBOs (pspGL); the app logs that and
 * stays resident so the run recipe can capture it.
 */

#include <pspkernel.h>
#include <stdio.h>
#include <stdlib.h>

#include "raylib.h"
#include "rlgl.h"
#include <GL/gl.h>

PSP_MODULE_INFO("rtt_probe", 0, 1, 1);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);

#ifndef TARGET_W
#define TARGET_W 256
#endif
#ifndef TARGET_H
#define TARGET_H 256
#endif

static void drawPattern(int w, int h)
{
    int q = w/8;

    ClearBackground(RAYWHITE);
    DrawRectangle(0, 0, q, q, RED);
    DrawRectangle(w - q, 0, q, q, GREEN);
    DrawRectangle(0, h - q, q, q, BLUE);
    DrawRectangle(w - q, h - q, q, q, YELLOW);
    DrawRectangle(w/2 - 4, h/2 - 4, 8, 8, BLACK);
}

static int colourAt(const unsigned char *px, int x, int y)
{
    const unsigned char *p = px + ((size_t)y*TARGET_W + x)*4;
    return (p[0] << 16) | (p[1] << 8) | p[2];
}

int main(void)
{
    InitWindow(480, 272, "rtt_probe");
    SetTargetFPS(0);

    RenderTexture2D target = LoadRenderTexture(TARGET_W, TARGET_H);
    TraceLog(LOG_INFO, "RTT target id=%u texture=%u depth=%u size=%dx%d",
             target.id, target.texture.id, target.depth.id, target.texture.width, target.texture.height);

    if (target.id == 0)
    {
        TraceLog(LOG_INFO, "RTT_RESULT unavailable (driver has no FBOs)");
        TraceLog(LOG_INFO, "DONE");
        sceKernelSleepThread();
        return 0;
    }

#if defined(MODE_DEPTH)
    Camera cam = { 0 };
    cam.position = (Vector3){ 0.0f, 0.0f, 3.0f };
    cam.target = (Vector3){ 0.0f, 0.0f, 0.0f };
    cam.up = (Vector3){ 0.0f, 1.0f, 0.0f };
    cam.fovy = 45.0f;
    cam.projection = CAMERA_PERSPECTIVE;
#endif

    for (int frame = 0; frame < 2; frame++)
    {
#if defined(MODE_DIRECT)
        BeginDrawing();
            drawPattern(TARGET_W, TARGET_H);
        EndDrawing();
#elif defined(MODE_RTT) || defined(MODE_CHURN)
        BeginTextureMode(target);
            drawPattern(TARGET_W, TARGET_H);
        EndTextureMode();

        if (frame == 0)
        {
#if defined(MODE_CHURN)
            for (int i = 0; i < 4; i++)
            {
                Image img = GenImageColor(64, 64, (Color){ (unsigned char)(i*40), 80, 120, 255 });
                Texture2D t = LoadTextureFromImage(img);
                UnloadTexture(t);
                UnloadImage(img);
            }
            TraceLog(LOG_INFO, "CHURN created and deleted 4 textures");
#endif
        }

        BeginDrawing();
            ClearBackground(BLACK);
            DrawTexture(target.texture, 0, 0, WHITE);
        EndDrawing();
#elif defined(MODE_READBACK)
        BeginTextureMode(target);
            drawPattern(TARGET_W, TARGET_H);
            rlDrawRenderBatchActive();

            static unsigned char px[TARGET_W*TARGET_H*4];
            glReadPixels(0, 0, TARGET_W, TARGET_H, GL_RGBA, GL_UNSIGNED_BYTE, px);

            int diff = 0;
            for (int y = 0; y < TARGET_H; y++)
                for (int x = 0; x < TARGET_W; x++)
                {
                    const unsigned char *p = px + ((size_t)y*TARGET_W + x)*4;
                    // glReadPixels row 0 is the bottom; the pattern's "top" is TARGET_H-1-y.
                    int ty = TARGET_H - 1 - y;
                    int want = 245 | (245 << 8) | (245 << 16);           // RAYWHITE
                    if ((x < TARGET_W/8) && (ty < TARGET_H/8)) want = 230 | (41 << 8) | (55 << 16);
                    if ((x >= TARGET_W - TARGET_W/8) && (ty < TARGET_H/8)) want = 0 | (228 << 8) | (48 << 16);
                    if ((x < TARGET_W/8) && (ty >= TARGET_H - TARGET_H/8)) want = 0 | (117 << 8) | (44 << 16);
                    if (p[0] != (want & 0xFF)) diff++;
                }

            int blackLo = 0, blackHi = 0;
            for (int y = 0; y < TARGET_H; y++)
                for (int x = 0; x < TARGET_W; x++)
                {
                    const unsigned char *p = px + ((size_t)y*TARGET_W + x)*4;
                    int black = (p[0] + p[1] + p[2]) < 24;
                    if (y < 8) blackLo += black;
                    if (y >= TARGET_H - 8) blackHi += black;
                }
            TraceLog(LOG_INFO, "READBACK_ROWS black_bottom8=%d black_top8=%d", blackLo, blackHi);
            TraceLog(LOG_INFO, "READBACK alpha=%02x %02x %02x %02x tl=%06x tr=%06x bl=%06x centre=%06x diff=%d",
                     px[((size_t)(TARGET_H/16)*TARGET_W + TARGET_W/16)*4 + 3],
                     px[((size_t)(TARGET_H/16)*TARGET_W + TARGET_W - TARGET_W/16)*4 + 3],
                     px[((size_t)(TARGET_H - TARGET_H/16)*TARGET_W + TARGET_W/16)*4 + 3],
                     px[((size_t)(TARGET_H/2)*TARGET_W + TARGET_W/2)*4 + 3],
                     colourAt(px, TARGET_W/16, TARGET_H/16),
                     colourAt(px, TARGET_W - TARGET_W/16, TARGET_H/16),
                     colourAt(px, TARGET_W/16, TARGET_H/16),
                     colourAt(px, TARGET_W/2, TARGET_H/2), diff);
        EndTextureMode();

        BeginDrawing();
            ClearBackground(BLACK);
        EndDrawing();
#elif defined(MODE_DEPTH)
        BeginTextureMode(target);
            ClearBackground(BLACK);
            BeginMode3D(cam);
                DrawCube((Vector3){ 0.0f, 0.0f, 0.6f }, 1.4f, 1.4f, 1.4f, RED);   // near, drawn first
                DrawCube((Vector3){ 0.0f, 0.0f, -0.6f }, 1.4f, 1.4f, 1.4f, GREEN); // far, drawn second
            EndMode3D();
        EndTextureMode();

        BeginDrawing();
            ClearBackground(BLACK);
            DrawTexture(target.texture, 0, 0, WHITE);
        EndDrawing();
#else
        #error "MODE must be direct, rtt, readback, depth, churn or size"
#endif
    }

    TraceLog(LOG_INFO, "DONE");
    sceKernelSleepThread();

    return 0;
}
