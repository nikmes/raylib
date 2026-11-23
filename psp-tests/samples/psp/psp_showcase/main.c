/*******************************************************************************************
*
*   psp_showcase - a 3D demo written for this port, not a ported upstream sample.
*
*   400 lit, textured cubes in two merged runs, a fogged floor grid, a slow orbit camera
*   and a 2D HUD. It exists to show the renderer doing the thing sprint 078 built: one
*   clean instance classification per cube, one draw per colour, no per-object overhead.
*
*   Built with FRAMES=0 it runs until the PSP is powered off. For a capture build pass
*   FRAMES=<n>: the app prints its frame statistics, logs DONE and sleeps, which is what
*   the screenshot workflow requires.
*
*   Controls: none. The camera orbits on its own.
*
********************************************************************************************/

#include <pspkernel.h>
#include <stdio.h>
#include <math.h>

#include "raylib.h"
#include "rlgl.h"
#include "raymath.h"

PSP_MODULE_INFO("psp_showcase", 0, 1, 1);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);

#ifndef SHOWCASE_FRAMES
    #define SHOWCASE_FRAMES 0
#endif

#define GRID 20                 // GRID x GRID cubes
#define SPACING 1.15f

// GE colours are 0xAABBGGRR, not raylib's RGBA.
#define GE_RGBA(r, g, b, a) (((unsigned int)(a) << 24) | ((unsigned int)(b) << 16) | \
                             ((unsigned int)(g) << 8) | (unsigned int)(r))

int main(void)
{
    InitWindow(480, 272, "psp showcase");
    SetTargetFPS(0);

    Image img = GenImageChecked(64, 64, 8, 8, (Color){ 236, 242, 255, 255 }, (Color){ 176, 192, 226, 255 });
    Texture2D tex = LoadTextureFromImage(img);
    UnloadImage(img);

    Mesh cube = GenMeshCube(1.0f, 1.0f, 1.0f);

    Material cool = LoadMaterialDefault();
    cool.maps[MATERIAL_MAP_DIFFUSE].texture = tex;
    cool.maps[MATERIAL_MAP_DIFFUSE].color = (Color){ 120, 200, 255, 255 };

    // Material.maps is a pointer: a struct copy would share the array and both materials
    // would end up the same colour (this demo did exactly that on its first run).
    Material warm = LoadMaterialDefault();
    warm.maps[MATERIAL_MAP_DIFFUSE].texture = tex;
    warm.maps[MATERIAL_MAP_DIFFUSE].color = (Color){ 255, 150, 90, 255 };

    Mesh ground = GenMeshPlane(44.0f, 44.0f, 1, 1);
    Material gmat = LoadMaterialDefault();
    gmat.maps[MATERIAL_MAP_DIFFUSE].texture = tex;
    gmat.maps[MATERIAL_MAP_DIFFUSE].color = (Color){ 60, 74, 110, 255 };

    Camera cam = { 0 };
    cam.target = (Vector3){ 0.0f, 1.2f, 0.0f };
    cam.up = (Vector3){ 0.0f, 1.0f, 0.0f };
    cam.fovy = 50.0f;
    cam.projection = CAMERA_PERSPECTIVE;

    // Fixed-function lighting: one directional light from above-left, dark ambient so the
    // unlit faces fall into the fog colour instead of black.
    float lx = -0.45f, ly = 0.85f, lz = 0.30f;
    float ll = sqrtf(lx*lx + ly*ly + lz*lz);
    rlpspEnableLighting(1);
    rlpspLight(0, 1, lx/ll, ly/ll, lz/ll, GE_RGBA(255, 245, 225, 255), GE_RGBA(110, 110, 110, 255), 0.0f, 0.0f, 0.0f);
    rlpspLightAmbient(GE_RGBA(22, 26, 44, 255));
    rlpspMaterial(GE_RGBA(70, 70, 70, 255), GE_RGBA(255, 255, 255, 255),
                  GE_RGBA(90, 90, 90, 255), 0, 8.0f);

    TraceLog(LOG_INFO, "COLOURS cool=%06x warm=%06x",
             (unsigned int)(cool.maps[MATERIAL_MAP_DIFFUSE].color.r << 16 | cool.maps[MATERIAL_MAP_DIFFUSE].color.g << 8 | cool.maps[MATERIAL_MAP_DIFFUSE].color.b),
             (unsigned int)(warm.maps[MATERIAL_MAP_DIFFUSE].color.r << 16 | warm.maps[MATERIAL_MAP_DIFFUSE].color.g << 8 | warm.maps[MATERIAL_MAP_DIFFUSE].color.b));

    // Frame statistics for the capture log.
    int frames = 0;
    double sum = 0.0;

    for (;;)
    {
        float t = (float)GetTime();
        float a = t*0.18f;

        cam.position = (Vector3){ sinf(a)*17.0f, 7.5f + 2.5f*sinf(t*0.25f), cosf(a)*17.0f };

        BeginDrawing();
            ClearBackground((Color){ 10, 14, 28, 255 });

            // HUD, then an explicit 2D flush: this fork's BeginMode3D does not flush the
            // batch, so 2D vertices added here would otherwise be transformed by the camera.
            DrawRectangle(8, 8, 212, 62, (Color){ 6, 10, 22, 210 });
            DrawRectangleLines(8, 8, 212, 62, (Color){ 90, 140, 210, 255 });
            DrawText("PSP-1000  GE  pspgu", 14, 14, 10, (Color){ 150, 190, 240, 255 });
            DrawText(TextFormat("%d cubes merged", GRID*GRID), 14, 28, 10, RAYWHITE);
            DrawText(TextFormat("%d fps", GetFPS()), 14, 42, 10, (Color){ 120, 220, 160, 255 });
            rlDrawRenderBatchActive();

            rlpspEnableLighting(1);
            BeginMode3D(cam);

                rlpspEnableFog(1);
                rlpspFog(12.0f, 48.0f, GE_RGBA(10, 14, 28, 255));

                DrawMesh(ground, gmat, MatrixTranslate(0.0f, -0.55f, 0.0f));

                // Two passes so the colour never changes inside a run: 400 cubes become
                // two merged draws instead of 400.
                for (int pass = 0; pass < 2; pass++)
                {
                    Material mat = pass? warm : cool;

                    for (int z = 0; z < GRID; z++)
                    {
                        for (int x = 0; x < GRID; x++)
                        {
                            if (((x + z) & 1) != pass) continue;

                            float px = ((float)x - (GRID - 1)*0.5f)*SPACING;
                            float pz = ((float)z - (GRID - 1)*0.5f)*SPACING;
                            float wave = sinf(t*1.5f + x*0.42f + z*0.31f);
                            float py = 1.1f + 0.75f*wave;
                            float s = 0.62f + 0.16f*sinf(t*1.1f + x*0.2f - z*0.25f);

                            Matrix m = MatrixMultiply(
                                MatrixMultiply(MatrixScale(s, s, s),
                                               MatrixRotateY(t*0.9f + x*0.2f + z*0.15f)),
                                MatrixTranslate(px, py, pz));

                            DrawMesh(cube, mat, m);
                        }
                    }
                }

                rlpspEnableFog(0);
            EndMode3D();
            rlpspEnableLighting(0);     // 2D has no normals; lit sprites go black




        EndDrawing();

        frames++;
        sum += GetFrameTime();

#if SHOWCASE_FRAMES > 0
        if (frames >= SHOWCASE_FRAMES) break;
#endif
    }

    double avg = (frames > 0)? (sum/frames)*1000.0 : 0.0;

    {
        extern unsigned int rlpspMergeRuns, rlpspMergeInstances, rlpspMergeReplays;
        extern unsigned int rlpspClassClean, rlpspClassDirty, rlpspClassCull;

        TraceLog(LOG_INFO, "MERGEPROBE runs=%u instances=%u replays=%u clean=%u dirty=%u cull=%u",
                 rlpspMergeRuns, rlpspMergeInstances, rlpspMergeReplays,
                 rlpspClassClean, rlpspClassDirty, rlpspClassCull);
    }
    TraceLog(LOG_INFO, "STAT n=%d avg=%.3f fps=%.1f", frames, avg, (avg > 0.0)? 1000.0/avg : 0.0);
    TraceLog(LOG_INFO, "DONE frames %d cubes %d", frames, GRID*GRID);
    sceKernelSleepThread();

    return 0;
}
