/*
 * clip_probe - sprint 066 T1: one vertex walks out of the GE's 4096 guard band while the
 * other two keep a visible sliver on screen. Below the limit the sliver draws; above it the
 * whole primitive is dropped.
 */
#include <pspkernel.h>
#include <stdio.h>
#include <stdlib.h>
#include "raylib.h"
#include "rlgl.h"
#include "raymath.h"

PSP_MODULE_INFO("clip_probe", 0, 1, 1);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);

#ifndef EDGE_X
#define EDGE_X 150
#endif

int main(void)
{
    InitWindow(480, 272, "clip_probe");
    SetTargetFPS(0);
    rlDisableBackfaceCulling();

#ifdef MESH3D
    Mesh mesh = { 0 };
    mesh.vertexCount = 3;
    mesh.triangleCount = 1;
    mesh.vertices = (float *)malloc(3*3*sizeof(float));
    mesh.vertices[0] = -0.5f; mesh.vertices[1] = -0.5f; mesh.vertices[2] = 0.0f;
    mesh.vertices[3] =  0.5f; mesh.vertices[4] = -0.5f; mesh.vertices[5] = 0.0f;
#ifdef NEAR_VERT
    mesh.vertices[6] = 0.0f; mesh.vertices[7] = 0.0f; mesh.vertices[8] = 4.01f;  // behind the camera
#else
    mesh.vertices[6] = (float)EDGE_X*0.01f; mesh.vertices[7] = 0.0f; mesh.vertices[8] = 0.0f;
#endif
    Material mat = LoadMaterialDefault();

    Camera cam = { 0 };
    cam.position = (Vector3){ 0.0f, 0.0f, 4.0f };
    cam.target = (Vector3){ 0.0f, 0.0f, 0.0f };
    cam.up = (Vector3){ 0.0f, 1.0f, 0.0f };
    cam.fovy = 45.0f;
    cam.projection = CAMERA_PERSPECTIVE;

    for (int frame = 0; frame < 2; frame++)
    {
        BeginDrawing();
            ClearBackground(BLACK);
            BeginMode3D(cam);
                DrawMesh(mesh, mat, MatrixIdentity());
            EndMode3D();
        EndDrawing();
    }
#else
    for (int frame = 0; frame < 2; frame++)
    {
        BeginDrawing();
            ClearBackground(BLACK);
            DrawTriangle((Vector2){ 100.0f, 100.0f },
                         (Vector2){ 400.0f, 200.0f },
                         (Vector2){ (float)EDGE_X, 150.0f }, WHITE);
        EndDrawing();
    }
#endif

    TraceLog(LOG_INFO, "EDGE %d", EDGE_X);
    TraceLog(LOG_INFO, "DONE");
    sceKernelSleepThread();
    return 0;
}
