/*
 * matrix_probe - sprint 070 (P10-S8): is the camera/matrix CPU work visible?
 *
 *   MODE=api    per-call cost of every camera/matrix helper (T1)
 *   MODE=calls  GetWorldToScreenEx per call vs a precomputed view-projection (T2)
 *   MODE=scene  N cubes with STAT/SPLIT and the PSP_MATRIX_PROBE attribution (T3)
 *
 * Timing protocol: 100-call warm-up, 20 blocks, median/p95 of the block means, a
 * volatile sink so nothing is elided, sceKernelGetSystemTimeWide() in microseconds.
 * MODE=api/calls call InitWindow once (GetWorldToScreen reads the screen size) and
 * never enter a frame loop, so vsync cannot quantise the numbers.
 */

#include <pspkernel.h>
#include <pspgum.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "raylib.h"
#include "raymath.h"
#include "psp_counters.h"

PSP_MODULE_INFO("matrix_probe", 0, 1, 1);
#if defined(PSP_GUM_VFPU)
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);
#else
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);
#endif

#define ITERS 10000
#define BLOCKS 20

static volatile float sink;

#if defined(MODE_API) || defined(MODE_CALLS) || defined(MODE_GUM)
static SceInt64 probeNow(void) { return sceKernelGetSystemTimeWide(); }
#endif

#if defined(MODE_API) || defined(MODE_GUM)
// Every measured expression must depend on this, or GCC hoists the inline raymath
// helpers out of the loop and the "cost" becomes the clock call (measured 0.068 us).
static float jit[16] = { 0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f,
                         0.9f, 1.0f, 1.1f, 1.2f, 1.3f, 1.4f, 1.5f, 1.6f };

static int cmpDouble(const void *a, const void *b)
{
    double x = *(const double *)a, y = *(const double *)b;

    return (x < y)? -1 : (x > y);
}

#if defined(MODE_API)
static float MeasureUpdateCamera(void)
{
    static Camera c;
    c.position = (Vector3){ 0.0f, 1.0f, 4.0f };
    c.target = (Vector3){ 0.0f, 0.0f, 0.0f };
    c.up = (Vector3){ 0.0f, 1.0f, 0.0f };
    c.fovy = 45.0f;
    c.projection = CAMERA_CUSTOM;

    UpdateCamera(&c, CAMERA_CUSTOM);

    return c.position.x;
}
#endif

#define MEASURE(label, expr) do { \
    for (int i = 0; i < 100; i++) sink += (float)(expr); \
    double t[BLOCKS]; \
    for (int b = 0; b < BLOCKS; b++) \
    { \
        SceInt64 t0 = probeNow(); \
        for (int i = 0; i < ITERS; i++) sink += (float)(expr); \
        t[b] = (double)(probeNow() - t0)/(double)ITERS; \
    } \
    qsort(t, BLOCKS, sizeof(double), cmpDouble); \
    TraceLog(LOG_INFO, "API %-24s n=%d med_us=%.3f p95_us=%.3f sum=%.1f", \
             label, ITERS, t[BLOCKS/2], t[BLOCKS-1], (double)sink); \
} while (0)

#endif

#if defined(MODE_API)
static void runApi(void)
{
    Camera2D cam2 = { { 240.0f, 136.0f }, { 0.0f, 0.0f }, 0.0f, 1.0f };
    Camera cam = { { 0.0f, 1.0f, 4.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f }, 45.0f, CAMERA_PERSPECTIVE };
    Vector2 v2 = { 12.0f, 34.0f };
    Vector3 v3 = { 1.0f, 2.0f, 3.0f };
    Matrix m = MatrixIdentity();
    float aspect = 480.0f/272.0f;

    MEASURE("GetCameraMatrix2D", (cam2.offset.x = jit[i & 15], GetCameraMatrix2D(cam2).m0));
    MEASURE("GetWorldToScreen2D", (v2.x = jit[i & 15], GetWorldToScreen2D(v2, cam2).x));
    MEASURE("GetScreenToWorld2D", (v2.y = jit[i & 15], GetScreenToWorld2D(v2, cam2).x));
    MEASURE("GetWorldToScreen", (v3.x = jit[i & 15], GetWorldToScreen(v3, cam).x));
    MEASURE("GetWorldToScreenEx", (v3.y = jit[i & 15], GetWorldToScreenEx(v3, cam, 480, 272).x));
    MEASURE("MatrixLookAt", MatrixLookAt((Vector3){ jit[i & 15], 1.0f, 4.0f }, cam.target, cam.up).m0);
    MEASURE("MatrixPerspective", MatrixPerspective(45.0f*DEG2RAD, aspect*(1.0f + jit[i & 15]*1e-6f), 0.01f, 1000.0f).m0);
    MEASURE("MatrixOrtho", MatrixOrtho(-1.0f*jit[i & 15], 1.0f, -1.0f, 1.0f, 0.01f, 1000.0f).m0);
    MEASURE("MatrixMultiply", MatrixMultiply(m, m).m0);
    MEASURE("Vector3Transform", Vector3Transform((Vector3){ jit[i & 15], 2.0f, 3.0f }, m).x);
    MEASURE("QuaternionTransform", QuaternionTransform((Quaternion){ jit[i & 15], 2.0f, 3.0f, 1.0f }, m).x);
    MEASURE("UpdateCamera(CUSTOM)", MeasureUpdateCamera());
    MEASURE("GetSystemTimeWide", (float)probeNow());   // calibration: one timer call

    TraceLog(LOG_INFO, "CHECK sink=%f", (double)sink);
}
#endif

#if defined(MODE_CALLS)
static Vector3 *points;

static double timePass(int cached)
{
    Camera cam = { { 0.0f, 1.0f, 4.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f }, 45.0f, CAMERA_PERSPECTIVE };
    Matrix view = MatrixLookAt(cam.position, cam.target, cam.up);
    Matrix proj = MatrixPerspective(cam.fovy*DEG2RAD, 480.0f/272.0f, 0.01f, 1000.0f);
    Matrix vp = MatrixMultiply(view, proj);
    SceInt64 t0 = probeNow();

    for (int i = 0; i < COUNT; i++)
    {
        Vector2 s;

        if (cached)
        {
            Quaternion q = { points[i].x, points[i].y, points[i].z, 1.0f };
            q = QuaternionTransform(q, vp);
            s.x = (q.x/q.w + 1.0f)*0.5f*480.0f;
            s.y = (1.0f - q.y/q.w)*0.5f*272.0f;
        }
        else s = GetWorldToScreenEx(points[i], cam, 480, 272);

        sink += s.x;
    }

    return (double)(probeNow() - t0)/(double)COUNT;     // us per position
}

static void runCalls(void)
{
    points = (Vector3 *)malloc((size_t)COUNT*sizeof(Vector3));

    for (int i = 0; i < COUNT; i++)
    {
        int rows = (COUNT + 9)/10;
        float ox = ((float)(i % 10) - 4.5f)*0.8f;
        float oz = ((float)(i/10) - (float)(rows - 1)*0.5f)*0.8f;
        points[i] = (Vector3){ ox, 0.5f, oz };
    }

    // The cached arm must agree with the API before its time is believed.
    {
        Camera cam = { { 0.0f, 1.0f, 4.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f }, 45.0f, CAMERA_PERSPECTIVE };
        Matrix vp = MatrixMultiply(MatrixLookAt(cam.position, cam.target, cam.up),
                                   MatrixPerspective(cam.fovy*DEG2RAD, 480.0f/272.0f, 0.01f, 1000.0f));
        float worst = 0.0f;

        for (int i = 0; i < COUNT; i++)
        {
            Vector2 a = GetWorldToScreenEx(points[i], cam, 480, 272);
            Quaternion q = { points[i].x, points[i].y, points[i].z, 1.0f };
            q = QuaternionTransform(q, vp);
            Vector2 b = { (q.x/q.w + 1.0f)*0.5f*480.0f, (1.0f - q.y/q.w)*0.5f*272.0f };
            float d = fabsf(a.x - b.x);
            if (d > worst) worst = d;
            d = fabsf(a.y - b.y);
            if (d > worst) worst = d;
        }
        TraceLog(LOG_INFO, "CHECK cached_vs_api_maxdiff_px=%.4f", (double)worst);
    }

    // Warm-up both arms.
    for (int i = 0; i < 5; i++) { sink += (float)timePass(0); sink += (float)timePass(1); }

    double per = 0.0, cac = 0.0;

    for (int b = 0; b < BLOCKS; b++)
    {
        per += timePass(0);
        cac += timePass(1);
    }

    per /= BLOCKS; cac /= BLOCKS;

    TraceLog(LOG_INFO, "CALLS n=%d per_call_us_per_pos=%.4f cached_us_per_pos=%.4f total_us=%.1f/%.1f ratio=%.2f",
             COUNT, per, cac, per*COUNT, cac*COUNT, (cac > 0.0)? (per/cac) : 0.0);
    TraceLog(LOG_INFO, "CHECK sink=%f", (double)sink);
    free(points);
}
#endif

#if defined(MODE_GUM)
static void runGum(void)
{
#if defined(GUM_SKIP_MULT)
    ScePspFMatrix4 m1, m2, mr;
#else
    ScePspFMatrix4 m1, m2, mm, mr;
#endif
    ScePspFVector3 eye = { 0.0f, 1.0f, 4.0f }, ctr = { 0.0f, 0.0f, 0.0f }, up = { 0.0f, 1.0f, 0.0f };
    ScePspFVector3 one = { 1.0f, 1.0f, 1.0f };
    Matrix scale = MatrixScale(0.5f, 0.5f, 0.5f);
    Matrix trans = MatrixTranslate(1.0f, 2.0f, 3.0f);
    Matrix xf = MatrixIdentity();

    ScePspFQuaternion qq = { 0.0f, 0.0f, 0.0f, 1.0f };

    gumInit();
    gumNormalizeQuaternion(&qq);
    gumLoadIdentity(&m1);
    gumLoadIdentity(&m2);

#if !defined(GUM_SKIP_MULT)
    MEASURE("gumMultMatrix", (gumMultMatrix(&mr, &m1, &m2), mr.x.x));
#endif
    MEASURE("gumLookAt", (eye.x = jit[i & 15], gumLookAt(&mr, &eye, &ctr, &up), mr.x.x));
    MEASURE("gumPerspective", (gumPerspective(&mr, 45.0f*DEG2RAD, 480.0f/272.0f, 0.01f, 1000.0f), mr.x.x));
    MEASURE("gumOrtho", (gumOrtho(&mr, -1.0f, 1.0f, -1.0f, 1.0f, 0.01f, 1000.0f), mr.x.x));
    MEASURE("gumTranslate", (gumTranslate(&mr, &eye), mr.x.x));
    MEASURE("gumScale", (gumScale(&mr, &one), mr.x.x));
    MEASURE("compose_raymath", MatrixMultiply(MatrixMultiply(scale, trans), xf).m0);
#if !defined(GUM_SKIP_MULT)
    MEASURE("compose_gum2x", (gumMultMatrix(&mm, &m1, &m2), gumMultMatrix(&mr, &mm, &m2), mr.x.x));
#endif
    MEASURE("gumRotateVector", (gumRotateVector(&up, &qq, &eye), up.x));

    TraceLog(LOG_INFO, "CHECK sink=%f", (double)sink);
}
#endif

#if defined(MODE_SCENE)
#if defined(PSP_MATRIX_PROBE)
extern unsigned long long rlpspProbeComposeUs, rlpspProbeMultUs, rlpspProbeCullUs;
extern unsigned int rlpspProbeDraws;
#endif

static int cmpFloat(const void *a, const void *b)
{
    float x = *(const float *)a, y = *(const float *)b;

    return (x < y)? -1 : (x > y);
}

static void runScene(void)
{
    float *times = (float *)malloc((size_t)FRAMES*sizeof(float));
    double sum = 0.0;

    Mesh cube = GenMeshCube(1.0f, 1.0f, 1.0f);
    Material mat = LoadMaterialDefault();
    Camera cam = { { 0.0f, 1.0f, 4.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f }, 45.0f, CAMERA_PERSPECTIVE };

    PspCountersReset();

    for (int frame = 0; frame < FRAMES; frame++)
    {
        BeginDrawing();
            ClearBackground(BLACK);
            BeginMode3D(cam);
                int rows = (COUNT + 9)/10;
                for (int i = 0; i < COUNT; i++)
                {
                    float ox = ((float)(i % 10) - 4.5f)*0.8f;
                    float oz = ((float)(i/10) - (float)(rows - 1)*0.5f)*0.8f;
                    DrawMesh(cube, mat, MatrixTranslate(ox, 0.5f, oz));
                }
            EndMode3D();
        EndDrawing();

        times[frame] = (float)GetFrameTime();
        sum += times[frame];
    }

    qsort(times, FRAMES, sizeof(float), cmpFloat);

    float median = times[FRAMES/2];
    float p95 = times[(int)(FRAMES*0.95f) < FRAMES ? (int)(FRAMES*0.95f) : FRAMES - 1];
    double swapAvg = (PspCountersSwapCalls() > 0)? (PspCountersSwapSeconds()/PspCountersSwapCalls())*1000.0 : 0.0;
    double workAvg = (sum/FRAMES)*1000.0 - swapAvg;

    TraceLog(LOG_INFO, "STAT n=%i min=%.3f median=%.3f p95=%.3f worst=%.3f avg=%.3f",
             FRAMES, times[0]*1000.0f, median*1000.0f, p95*1000.0f, times[FRAMES - 1]*1000.0f,
             (float)(sum/FRAMES)*1000.0f);
    TraceLog(LOG_INFO, "SPLIT n=%i work=%.3f swap=%.3f swapcalls=%u", FRAMES, workAvg, swapAvg,
             PspCountersSwapCalls());

#if defined(PSP_MATRIX_PROBE)
    double compose = (double)rlpspProbeComposeUs/FRAMES/1000.0;
    double mult = (double)rlpspProbeMultUs/FRAMES/1000.0;
    double cull = (double)rlpspProbeCullUs/FRAMES/1000.0;
    double draws = (double)rlpspProbeDraws/FRAMES;

    TraceLog(LOG_INFO, "MATRIXPROBE n=%d draws_per_frame=%.1f compose_ms=%.3f mult_ms=%.3f cull_ms=%.3f share_pct=%.1f",
             COUNT, draws, compose, mult, cull,
             (workAvg > 0.0)? ((compose + mult + cull)/workAvg*100.0) : 0.0);

#if defined(PSP_CALL_PROBE)
    {
        extern unsigned long long rlpspProbeDrawMeshUs, rlpspProbeDriverUs;
        extern unsigned long long rlpspProbeTail(int n);
        extern unsigned int rlpspProbeDraws;

        unsigned long long tail = rlpspProbeTail(1000);

        TraceLog(LOG_INFO, "CALLTAIL pairs=1000 us_per_pair=%.3f", (double)tail/1000.0/1000.0);

        // Denominators differ on purpose: the whole call and the cull run on every
        // DrawMesh (culled included), while compose/multiply/driver only run on the ones
        // that reach the per-object path.
        double calls = (double)COUNT;
        double sub = (draws > 0.0)? draws : 1.0;
        double total = (double)rlpspProbeDrawMeshUs/FRAMES/1000.0/calls;
        double cullCall = cull*1000.0/calls;
        double driverSub = (double)rlpspProbeDriverUs/FRAMES/1000.0/sub;
        double composeSub = compose*1000.0/sub;
        double multSub = mult*1000.0/sub;
        double state = total - cullCall - composeSub*(sub/calls) - multSub*(sub/calls) - driverSub*(sub/calls);

        TraceLog(LOG_INFO, "CALLPROBE n=%d calls_per_frame=%.1f submitted=%.1f total_ms=%.4f cull_ms=%.4f compose_ms=%.4f mult_ms=%.4f state_ms=%.4f driver_ms=%.4f",
                 COUNT, calls, sub, total, cullCall, composeSub, multSub, state, driverSub);
    }
#endif
#else
    TraceLog(LOG_INFO, "MATRIXPROBE counters off (build libraylib.a with PSP_MATRIX_PROBE=1)");
#endif

    TraceLog(LOG_INFO, "DONE frames %d count %d", FRAMES, COUNT);

    {
        extern unsigned int rlpspMergeRuns, rlpspMergeInstances, rlpspMergeReplays;
        extern unsigned int rlpspClassClean, rlpspClassDirty, rlpspClassCull;

        TraceLog(LOG_INFO, "MERGEPROBE runs=%u instances=%u replays=%u class_clean=%u class_dirty=%u class_cull=%u",
                 rlpspMergeRuns, rlpspMergeInstances, rlpspMergeReplays,
                 rlpspClassClean, rlpspClassDirty, rlpspClassCull);
    }

    sceKernelSleepThread();
}
#endif

int main(void)
{
    InitWindow(480, 272, "matrix_probe");
    SetTargetFPS(0);

#if defined(MODE_API)
    TraceLog(LOG_INFO, "MODE api");
    runApi();
    TraceLog(LOG_INFO, "DONE");
    sceKernelSleepThread();
#elif defined(MODE_CALLS)
    TraceLog(LOG_INFO, "MODE calls");
    runCalls();
    TraceLog(LOG_INFO, "DONE");
    sceKernelSleepThread();
#elif defined(MODE_GUM)
#if defined(PSP_GUM_VFPU)
    TraceLog(LOG_INFO, "MODE gum vfpu");
#else
    TraceLog(LOG_INFO, "MODE gum default");
#endif
    runGum();
    TraceLog(LOG_INFO, "DONE");
    sceKernelSleepThread();
#elif defined(MODE_SCENE)
    TraceLog(LOG_INFO, "MODE scene");
    runScene();
#endif

    return 0;
}
