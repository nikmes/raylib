/*
 * triangle_probe - sprint 013 T1: one triangle, three predicted screen positions.
 *
 * The mesh scene says our 3D is "roughly twice as wide from x=0" while raylib's own
 * BeginMode3D numbers predict x 81..191 for the cube - and pspGL lands on the prediction.
 * That leaves only two candidates: the matrix is altered between our array and the GE's
 * registers, or the modelview is applied twice. This measures one vertex at a time.
 *
 * Camera (0,0,2) -> (0,0,0), fovy 45, perspective, near 0.05, far 4000.
 *   top   = 0.05*tan(22.5deg) ... no: raylib's BeginMode3D uses near*tan(fovy/2) = 0.05*0.414 = 0.0207
 *   Wait - that is the *frustum* half-height at the near plane, and it makes the projection
 *   independent of distance. With near 0.05 the visible half-height at z=0 (2 units out) is
 *   0.0207 * (2/0.05) = 0.828. half-width = 0.828 * 480/272 = 1.461.
 *
 * A vertex at world (x,y,0) therefore maps to
 *   ndc = (x/1.461, y/0.828),  screen = (240 + 240*ndc_x, 136 - 136*ndc_y)
 * so the three vertices below must land on:
 *   (-0.5,-0.5) -> (158,218)   (0.5,-0.5) -> (322,218)   (0,0.5) -> (240,54)
 */

#include <pspkernel.h>
#include <stdio.h>

#include "raylib.h"

PSP_MODULE_INFO("triangle_probe", 0, 1, 1);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);

int main(int argc, char **argv)
{
    InitWindow(480, 272, "triangle_probe");
    SetTargetFPS(0);

    Camera cam = { 0 };
    cam.position = (Vector3){ 0.0f, 0.0f, 2.0f };
    cam.target = (Vector3){ 0.0f, 0.0f, 0.0f };
    cam.up = (Vector3){ 0.0f, 1.0f, 0.0f };
    cam.fovy = 45.0f;
    cam.projection = CAMERA_PERSPECTIVE;

    for (int frame = 0; frame < 2; frame++)
    {
        BeginDrawing();
            ClearBackground(BLACK);

            BeginMode3D(cam);
                DrawTriangle3D((Vector3){ -0.5f, -0.5f, 0.0f },
                               (Vector3){  0.5f, -0.5f, 0.0f },
                               (Vector3){  0.0f,  0.5f, 0.0f }, RED);
            EndMode3D();

        EndDrawing();
    }

    TraceLog(LOG_INFO, "DONE frames 2  predicted=(158,218) (322,218) (240,54)");
    sceKernelSleepThread();

    return 0;
}
