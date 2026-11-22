/*
 * scene_3d - the phase 10 3D corpus skeleton.
 *
 * Sprint 060 T4: a deterministic, fixed-camera scene whose only variable is the arm.
 *   SCENE=flat   one cube through glDrawElements (no texture)
 *   SCENE=tex    one cube with a generated checker texture
 *   SCENE=both   both cubes (default)
 *   SCENE=grid   4,096-vertex mesh whose index windows span the whole vertex range, so
 *                glDrawElements must use the dedup chunker
 *
 * Later P10 sprints add their own arms; this skeleton deliberately has no animation,
 * no lighting and no render-to-texture. It logs STAT/SPLIT in the bunnymark format and
 * stays resident after DONE so a scrshot can be compared twice.
 */

#include <pspkernel.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "raylib.h"
#include "raymath.h"
#include "psp_counters.h"

PSP_MODULE_INFO("scene_3d", 0, 1, 1);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);

#ifndef SCENE_FRAMES
#define SCENE_FRAMES 120
#endif
#ifndef SCENE_NAME
#define SCENE_NAME "both"
#endif

static int cmpFloat(const void *a, const void *b)
{
    float x = *(const float *)a;
    float y = *(const float *)b;
    return (x > y) - (x < y);
}

#if defined(SCENE_GRID)
// 64x64 vertices in a plane. The index buffer steps by 3, so every 2,048-index window
// references vertices across the whole 0..4,095 range: range*stride does not fit the
// per-draw budget and the dedup path is exercised.
static Mesh makeGrid(void)
{
    const int side = 64;
    const int count = side*side;
    const int indices = 2046;
    Mesh mesh = { 0 };

    mesh.vertexCount = count;
    mesh.triangleCount = indices/3;
    mesh.vertices = (float *)malloc((size_t)count*3*sizeof(float));
    mesh.texcoords = (float *)malloc((size_t)count*2*sizeof(float));
    mesh.normals = (float *)malloc((size_t)count*3*sizeof(float));
    mesh.indices = (unsigned short *)malloc((size_t)indices*sizeof(unsigned short));

    for (int y = 0; y < side; y++)
    {
        for (int x = 0; x < side; x++)
        {
            int i = y*side + x;
            mesh.vertices[i*3 + 0] = ((float)x/(side - 1)*2.0f - 1.0f)*2.0f;
            mesh.vertices[i*3 + 1] = ((float)y/(side - 1)*2.0f - 1.0f)*2.0f;
            mesh.vertices[i*3 + 2] = 0.0f;
            mesh.texcoords[i*2 + 0] = (float)x/(side - 1);
            mesh.texcoords[i*2 + 1] = (float)y/(side - 1);
            mesh.normals[i*3 + 0] = 0.0f;
            mesh.normals[i*3 + 1] = 0.0f;
            mesh.normals[i*3 + 2] = 1.0f;
        }
    }

    for (int i = 0; i < indices; i++) mesh.indices[i] = (unsigned short)((i*3) & (count - 1));

    return mesh;
}
#endif

int main(void)
{
    InitWindow(480, 272, "scene_3d");
    SetTargetFPS(0);

    Camera cam = { 0 };
    cam.position = (Vector3){ 0.0f, 1.0f, 4.0f };
    cam.target = (Vector3){ 0.0f, 0.0f, 0.0f };
    cam.up = (Vector3){ 0.0f, 1.0f, 0.0f };
    cam.fovy = 45.0f;
    cam.projection = CAMERA_PERSPECTIVE;

    Mesh cube = GenMeshCube(1.0f, 1.0f, 1.0f);
    Material flatMat = LoadMaterialDefault();

    Texture2D checker = { 0 };
    Material texMat = LoadMaterialDefault();
#if defined(SCENE_TEX) || defined(SCENE_BOTH)
    {
        Image img = GenImageChecked(64, 64, 8, 8, RAYWHITE, DARKGRAY);
        checker = LoadTextureFromImage(img);
        UnloadImage(img);
        texMat.maps[MATERIAL_MAP_DIFFUSE].texture = checker;
    }
#endif

#if defined(SCENE_GRID)
    Mesh grid = makeGrid();
    Material gridMat = LoadMaterialDefault();
#endif

    Matrix flatXform = MatrixTranslate(-1.2f, 0.0f, 0.0f);
    Matrix texXform  = MatrixTranslate( 1.2f, 0.0f, 0.0f);
    Matrix idXform   = MatrixIdentity();

    float *frameTimes = (float *)malloc((size_t)SCENE_FRAMES*sizeof(float));
    double sum = 0.0;

    for (int frame = 0; frame < SCENE_FRAMES; frame++)
    {
        BeginDrawing();
            ClearBackground(BLACK);
            BeginMode3D(cam);
#if defined(SCENE_GRID)
                DrawMesh(grid, gridMat, idXform);
#else
  #if !defined(SCENE_TEX)
                DrawMesh(cube, flatMat, flatXform);
  #endif
  #if defined(SCENE_TEX) || defined(SCENE_BOTH)
                DrawMesh(cube, texMat, texXform);
  #endif
#endif
            EndMode3D();
        EndDrawing();

        frameTimes[frame] = (float)GetFrameTime();
        sum += frameTimes[frame];

        if (((frame + 1) % 60) == 0)
        {
            TraceLog(LOG_INFO, "FRAME %i fps %i", frame + 1, GetFPS());
            PspCountersDump("frame", frame + 1);
        }
    }

    qsort(frameTimes, SCENE_FRAMES, sizeof(float), cmpFloat);

    float median = frameTimes[SCENE_FRAMES/2];
    float p95 = frameTimes[(int)(SCENE_FRAMES*0.95f) < SCENE_FRAMES? (int)(SCENE_FRAMES*0.95f) : SCENE_FRAMES - 1];
    float worst = frameTimes[SCENE_FRAMES - 1];
    float best = frameTimes[0];

    PspCountersDump("done", SCENE_FRAMES);
    TraceLog(LOG_INFO, "STAT n=%i min=%.3f median=%.3f p95=%.3f worst=%.3f avg=%.3f",
             SCENE_FRAMES, best*1000.0f, median*1000.0f, p95*1000.0f, worst*1000.0f,
             (float)(sum/SCENE_FRAMES)*1000.0f);

    double swapAvg = (PspCountersSwapCalls() > 0)?
        (PspCountersSwapSeconds()/PspCountersSwapCalls())*1000.0 : 0.0;
    double workAvg = (float)(sum/SCENE_FRAMES)*1000.0 - swapAvg;
    TraceLog(LOG_INFO, "SPLIT n=%i work=%.3f swap=%.3f swapcalls=%u", SCENE_FRAMES, workAvg, swapAvg,
             PspCountersSwapCalls());
    TraceLog(LOG_INFO, "DONE frames %i scene=%s", SCENE_FRAMES, SCENE_NAME);

    sceKernelSleepThread();

    return 0;
}
