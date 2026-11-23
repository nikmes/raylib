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
#include <string.h>

#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"
#include <GL/gl.h>
#include "psp_counters.h"

#ifdef SAMPLE_DIAG
void pspguDebugFrameStats(const char *tag);
#endif

PSP_MODULE_INFO("scene_3d", 0, 1, 1);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);

#ifndef SCENE_FRAMES
#define SCENE_FRAMES 120
#endif
#ifndef SCENE_NAME
#define SCENE_NAME "both"
#endif
#ifndef FAR_VALUE
#define FAR_VALUE 0.0f
#endif
#ifndef INSTANCES
#define INSTANCES 1
#endif
#if defined(LIGHT_BAKE)
// CPU Lambert + ambient into mesh.colors, once. Alpha stays opaque; the packed path is
// bypassed for colour meshes, so this is the float-path comparison for T5.
static void bakeCube(Mesh *mesh, Vector3 dir, unsigned ambient, unsigned diffuse)
{
    mesh->colors = (unsigned char *)malloc((size_t)mesh->vertexCount*4);
    if (mesh->colors == NULL) return;

    float lx = dir.x, ly = dir.y, lz = dir.z;
    float len = sqrtf(lx*lx + ly*ly + lz*lz);
    if (len > 0.0f) { lx /= len; ly /= len; lz /= len; }

    for (int i = 0; i < mesh->vertexCount; i++)
    {
        float d = mesh->normals[i*3 + 0]*lx + mesh->normals[i*3 + 1]*ly + mesh->normals[i*3 + 2]*lz;
        if (d < 0.0f) d = 0.0f;
        for (int c = 0; c < 3; c++)
        {
            float a = (float)((ambient >> (c*8)) & 0xFF);
            float m = (float)((diffuse >> (c*8)) & 0xFF);
            float v = a + m*d;
            if (v > 255.0f) v = 255.0f;
            mesh->colors[i*4 + c] = (unsigned char)(v + 0.5f);
        }
        mesh->colors[i*4 + 3] = 255;
    }
}
#endif

#ifdef JITTER_ARM
#define JITTER_FRAMES 16
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

#ifdef JITTER_ARM
// Diagnostic: read one RGBA row of the current frame and return the left edge of the
// white cube. glReadPixels finishes and waits by design; this arm is excluded from timing.
static int cubeLeftEdge(int y)
{
    unsigned char row[480*4];
    glReadPixels(0, y, 480, 1, GL_RGBA, GL_UNSIGNED_BYTE, row);
    for (int x = 0; x < 480; x++)
        if (row[x*4] > 128) return x;
    return -1;
}
#endif

#ifdef SAMPLE_ARM
// 16x16-quad ground in the XZ plane, normals up, checker UVs. Static, so UploadMesh packs
// it once and the direct path draws it.
static Mesh makeGround(void)
{
    const int q = 16;
    const int side = q + 1;
    const int count = side*side;
    const int indices = q*q*6;
    Mesh mesh = { 0 };

    mesh.vertexCount = count;
    mesh.triangleCount = indices/3;
    mesh.vertices = (float *)malloc((size_t)count*3*sizeof(float));
    mesh.texcoords = (float *)malloc((size_t)count*2*sizeof(float));
    mesh.normals = (float *)malloc((size_t)count*3*sizeof(float));
    mesh.indices = (unsigned short *)malloc((size_t)indices*sizeof(unsigned short));

    for (int z = 0; z < side; z++)
        for (int x = 0; x < side; x++)
        {
            int i = z*side + x;
            mesh.vertices[i*3 + 0] = (float)x/q*8.0f - 4.0f;
            mesh.vertices[i*3 + 1] = 0.0f;
            mesh.vertices[i*3 + 2] = (float)z/q*8.0f - 4.0f;
            mesh.texcoords[i*2 + 0] = (float)x/q*4.0f;
            mesh.texcoords[i*2 + 1] = (float)z/q*4.0f;
            mesh.normals[i*3 + 0] = 0.0f;
            mesh.normals[i*3 + 1] = 1.0f;
            mesh.normals[i*3 + 2] = 0.0f;
        }

    int k = 0;
    for (int z = 0; z < q; z++)
        for (int x = 0; x < q; x++)
        {
            unsigned short a = (unsigned short)(z*side + x);
            unsigned short b = (unsigned short)(z*side + x + 1);
            unsigned short c = (unsigned short)((z + 1)*side + x);
            unsigned short d = (unsigned short)((z + 1)*side + x + 1);
            mesh.indices[k++] = a; mesh.indices[k++] = c; mesh.indices[k++] = b;
            mesh.indices[k++] = b; mesh.indices[k++] = c; mesh.indices[k++] = d;
        }

    return mesh;
}
#endif

int main(void)
{
    InitWindow(480, 272, "scene_3d");
    SetTargetFPS(0);
    rlpspEnableCulling(CULL_ON);

    Camera cam = { 0 };
    cam.position = (Vector3){ FAR_VALUE, 1.0f, 4.0f };
    cam.target = (Vector3){ FAR_VALUE, 0.0f, 0.0f };
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
    UploadMesh(&grid, false);          // one-time GE-native pack; exercises the direct path
    Material gridMat = LoadMaterialDefault();
#endif

    Matrix flatXform = MatrixTranslate(FAR_VALUE - 1.2f, 0.0f, 0.0f);
    Matrix texXform  = MatrixTranslate(FAR_VALUE + 1.2f, 0.0f, 0.0f);
    Matrix idXform   = MatrixTranslate(FAR_VALUE, 0.0f, 0.0f);

    float *frameTimes = (float *)malloc((size_t)SCENE_FRAMES*sizeof(float));
    double sum = 0.0;

#ifdef SAMPLE_ARM
    {
        RenderTexture2D panel = { 0 };
#ifndef NO_PANEL
        panel = LoadRenderTexture(256, 256);
#endif
        Mesh ground = makeGround();
        UploadMesh(&ground, false);
        Mesh torus = GenMeshTorus(0.25f, 0.6f, 12, 24);
        Material groundMat = texMat;
        Material torusMat = texMat;

        Camera cam2 = cam;
        cam2.position = (Vector3){ FAR_VALUE, 0.6f, 2.5f };
        cam2.target   = (Vector3){ FAR_VALUE, 0.4f, 0.0f };

        for (int frame = 0; frame < SCENE_FRAMES; frame++)
        {
            if (panel.id != 0) BeginTextureMode(panel);
                if (frame == 0)
                {
                    rlpspEnableLighting(1);
                    rlpspLightAmbient(0xFF404040);
                    rlpspMaterial(0xFF333333, 0xFFCCCCCC, 0x00000000, 0x00000000, 0.0f);
                    rlpspLight(0, 1, 0.39f, 0.79f, 0.47f, 0xFFFFFFFF, 0xFF000000, 1.0f, 0.0f, 0.0f);
                    rlpspFog(6.0f, 14.0f, 0xFF201008);
                    rlpspEnableFog(1);
                }
                ClearBackground(BLACK);
                BeginMode3D(cam2);
                    DrawMesh(torus, torusMat, MatrixTranslate(FAR_VALUE, 0.6f, 0.0f));
                EndMode3D();
            if (panel.id != 0) EndTextureMode();

            BeginDrawing();
                ClearBackground(BLACK);
                BeginMode3D(cam);
                    DrawMesh(ground, groundMat, MatrixTranslate(FAR_VALUE, 0.0f, 0.0f));
                    int rows = (INSTANCES + 9)/10;
                    for (int i = 0; i < INSTANCES; i++)
                    {
                        float ox = ((float)(i % 10) - 4.5f)*0.8f;
                        float oz = ((float)(i / 10) - (float)(rows - 1)*0.5f)*0.8f;
                        DrawMesh(cube, texMat, MatrixTranslate(FAR_VALUE + ox, 0.5f, oz));
                    }
                    DrawMesh(torus, torusMat, MatrixTranslate(FAR_VALUE, 0.6f, 0.0f));
                EndMode3D();
                if (panel.id != 0) DrawTexture(panel.texture, 216, 16, WHITE);
            EndDrawing();

#ifdef SAMPLE_DIAG
            {
                static unsigned char prev[480*272*4];
                static unsigned char cur[480*272*4];
                static int have = 0;
                glReadPixels(0, 0, 480, 272, GL_RGBA, GL_UNSIGNED_BYTE, cur);
                if (have)
                {
                    int diff = 0;
                    int band[17] = { 0 };
                    int minx = 480, maxx = -1, miny = 272, maxy = -1, samples = 0;
                    for (int y = 0; y < 272; y++)
                    {
                        for (int x = 0; x < 480; x++)
                        {
                            int i = (y*480 + x)*4;
                            if ((cur[i] != prev[i]) || (cur[i+1] != prev[i+1]) || (cur[i+2] != prev[i+2]))
                            {
                                diff++;
                                band[y/16]++;
                                if (x < minx) minx = x;
                                if (x > maxx) maxx = x;
                                if (y < miny) miny = y;
                                if (y > maxy) maxy = y;
                                if (samples < 4)
                                {
                                    TraceLog(LOG_INFO, "FLICKPX x=%d y=%d prev=%d,%d,%d cur=%d,%d,%d",
                                             x, y, prev[i], prev[i+1], prev[i+2], cur[i], cur[i+1], cur[i+2]);
                                    samples++;
                                }
                            }
                        }
                    }
                    TraceLog(LOG_INFO, "FLICKBB frame=%d x=%d..%d y=%d..%d", frame, minx, maxx, miny, maxy);
                    TraceLog(LOG_INFO, "FLICK frame=%d diff_px=%d y0=%d y1=%d y2=%d y3=%d y4=%d y5=%d y6=%d y7=%d y8=%d y9=%d y10=%d y11=%d y12=%d y13=%d y14=%d y15=%d y16=%d",
                             frame, diff, band[0],band[1],band[2],band[3],band[4],band[5],band[6],band[7],
                             band[8],band[9],band[10],band[11],band[12],band[13],band[14],band[15],band[16]);
                }
                memcpy(prev, cur, sizeof(cur));
                have = 1;
                if ((frame >= 1) && (frame <= 5))
                {
                    char tag[16];
                    snprintf(tag, sizeof(tag), "f%d", frame);
                    pspguDebugFrameStats(tag);
                }
            }
#endif

            frameTimes[frame] = (float)GetFrameTime();
            sum += frameTimes[frame];

            if (((frame + 1) % 60) == 0)
            {
                TraceLog(LOG_INFO, "FRAME %i fps %i", frame + 1, GetFPS());
                PspCountersDump("frame", frame + 1);
            }
        }
    }
#elif defined(JITTER_ARM)
    int edges[JITTER_FRAMES];
    for (int frame = 0; frame < JITTER_FRAMES; frame++)
    {
        // Camera and cube travel together; the orbit step is constant, so any irregularity
        // in the measured edge is quantisation, not the intended motion.
        cam.position.x = FAR_VALUE + ((float)frame - 7.5f)*0.1f;
        cam.target.x   = FAR_VALUE + ((float)frame - 7.5f)*0.1f;

        BeginDrawing();
            ClearBackground(BLACK);
            BeginMode3D(cam);
                DrawMesh(cube, flatMat, flatXform);
            EndMode3D();
        EndDrawing();
        // After the swap, the last submitted frame is readable without a mid-frame finish.
        edges[frame] = cubeLeftEdge(136);

        TraceLog(LOG_INFO, "JITTER frame=%d edge=%d", frame, edges[frame]);
    }
    int lo = 480, hi = -1;
    for (int i = 0; i < JITTER_FRAMES; i++)
    {
        if (edges[i] < 0) continue;
        if (edges[i] < lo) lo = edges[i];
        if (edges[i] > hi) hi = edges[i];
    }
    // mean step and worst deviation from it
    double stepSum = 0.0; int stepN = 0;
    for (int i = 1; i < JITTER_FRAMES; i++)
    {
        if ((edges[i] >= 0) && (edges[i-1] >= 0)) { stepSum += edges[i] - edges[i-1]; stepN++; }
    }
    double stepMean = (stepN > 0)? stepSum/stepN : 0.0;
    double worstDev = 0.0;
    for (int i = 1; i < JITTER_FRAMES; i++)
    {
        if ((edges[i] >= 0) && (edges[i-1] >= 0))
        {
            double d = (edges[i] - edges[i-1]) - stepMean;
            if (d < 0) d = -d;
            if (d > worstDev) worstDev = d;
        }
    }
    TraceLog(LOG_INFO, "JITTER_RESULT far=%d min=%d max=%d span=%d step=%.3f worst=%.3f",
             (int)FAR_VALUE, lo, hi, hi - lo, stepMean, worstDev);
#else
    int drawn = 0;
    for (int frame = 0; frame < SCENE_FRAMES; frame++)
    {
        BeginDrawing();
            ClearBackground(BLACK);

            if (frame == 0)
            {
#if defined(LIGHT_BAKE)
                bakeCube(&cube, (Vector3){ -0.39f, -0.79f, -0.47f }, 0xFF404040, 0xFFCCCCCC);
#endif
#if defined(LIGHT_GE)
                rlpspEnableLighting(1);
                rlpspLightAmbient(0xFF404040);
                rlpspMaterial(0xFF333333, 0xFFCCCCCC, 0xFF000000, 0xFF000000, 0.0f);
                rlpspLight(0, 1, 0.39f, 0.79f, 0.47f, 0xFFFFFFFF, 0xFF000000, 1.0f, 0.0f, 0.0f);
#endif
#if defined(FOG_LINEAR)
                rlpspFog(3.0f, 6.0f, 0xFF0000FF);
                rlpspEnableFog(1);
#endif
#if defined(TEXENV_REPLACE)
                rlpspTexEnv(GL_REPLACE, 0xFFFFFFFF);
#elif defined(TEXENV_DECAL)
                rlpspTexEnv(GL_DECAL, 0xFFFFFFFF);
#elif defined(TEXENV_ADD)
                rlpspTexEnv(GL_ADD, 0x00000000);
#endif
            }

            BeginMode3D(cam);
#if defined(SCENE_GRID)
                DrawMesh(grid, gridMat, idXform);
#else
  #if !defined(SCENE_TEX)
                DrawMesh(cube, flatMat, flatXform);
  #endif
  #if defined(SCENE_TEX) || defined(SCENE_BOTH)
                if (INSTANCES == 1) DrawMesh(cube, texMat, texXform);
                else
                {
                    for (int i = 0; i < INSTANCES; i++)
                    {
                        int rows = (INSTANCES + 9)/10;
                        float ox = ((float)(i % 10) - 4.5f)*0.8f;
                        float oy = ((float)(i / 10) - (float)(rows - 1)*0.5f)*0.8f;
                        DrawMesh(cube, texMat, MatrixTranslate(FAR_VALUE + ox, oy, 0.0f));
                    }
                }
                drawn += INSTANCES;
  #endif
#endif
            EndMode3D();
        EndDrawing();

        frameTimes[frame] = (float)GetFrameTime();
        sum += frameTimes[frame];
        (void)drawn;

        if (((frame + 1) % 60) == 0)
        {
            TraceLog(LOG_INFO, "FRAME %i fps %i", frame + 1, GetFPS());
            PspCountersDump("frame", frame + 1);
        }
    }
#endif

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
