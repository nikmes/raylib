/*
 * ge_probe - P10-S15 T1: separate GE time from CPU list-build time.
 *
 * The driver uses GU_DIRECT, so sceGeDrawSync() cannot report GE time: it sees the
 * whole CPU render phase. This probe builds a representative frame in a GU_SEND list,
 * writes it back, enqueues it and waits with the CPU idle. CPU build time is measured
 * around sceGuStart/sceGuFinish; GE time is writeback + sceGeListEnQueue +
 * sceGeListSync(PSP_GE_LIST_DONE).
 *
 * Categories: clear, 2D sprites (2,000), 3D lit cubes (400, one indexed draw), frame.
 * No raylib/pspgu: the point is the GE, not the driver's direct-list overlap.
 */

#include <pspkernel.h>
#include <pspgu.h>
#include <pspge.h>
#include <pspdisplay.h>
#include <psputils.h>
#include <pspgum.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

PSP_MODULE_INFO("ge_probe", 0, 1, 1);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);

#define SCR_W 480
#define SCR_H 272
#define STRIDE 512
#define FB_BYTES (STRIDE*SCR_H*2)
#define DEPTH_OFF (2*FB_BYTES)
#define TEX_OFF (3*FB_BYTES)
#define LIST_BYTES (240*1024)

#define SPRITES 2000
#define CUBES 400
#define CUBE_VERTS (CUBES*36)

// T2: a vertex-bound mesh - 12,000 triangles that tile the screen once, so fill is
// ~130k pixels but the GE transforms 36,000 vertices. Same geometry in s16 and s8.
#define MESH_COLS 120
#define MESH_ROWS 100
#define MESH_TRIS (MESH_COLS*MESH_ROWS)
#define MESH_VERTS (MESH_TRIS*3)

static unsigned int list[LIST_BYTES/4] __attribute__((aligned(64)));

typedef struct { float u, v; unsigned int color; float x, y, z; } V2;
typedef struct { unsigned int color; float nx, ny, nz; float x, y, z; } V3;
typedef struct { unsigned int color; short x, y, z, pad; } V16;
typedef struct { unsigned int color; signed char x, y, z, pad; } V8;

static V2 sprites[SPRITES*2] __attribute__((aligned(64)));
static V3 cubes[CUBE_VERTS] __attribute__((aligned(64)));
static unsigned short cubeIndex[CUBE_VERTS] __attribute__((aligned(64)));
static V16 mesh16[MESH_VERTS] __attribute__((aligned(64)));
static V8  mesh8[MESH_VERTS] __attribute__((aligned(64)));

// Captures for the T2 parity check: the same frame drawn with each vertex format.
static unsigned short cap16[SCR_W*SCR_H] __attribute__((aligned(64)));
static unsigned short cap8[SCR_W*SCR_H] __attribute__((aligned(64)));

static unsigned long long nowUs(void)
{
    return (unsigned long long)sceKernelGetSystemTimeWide();
}

static int cmpU64(const void *a, const void *b)
{
    unsigned long long x = *(const unsigned long long *)a;
    unsigned long long y = *(const unsigned long long *)b;
    return (x < y)? -1 : (x > y);
}

/* Cube faces, CCW when viewed from outside; normals per face. */
static const float corner[8][3] = {
    {-0.5f,-0.5f,-0.5f}, { 0.5f,-0.5f,-0.5f}, { 0.5f, 0.5f,-0.5f}, {-0.5f, 0.5f,-0.5f},
    {-0.5f,-0.5f, 0.5f}, { 0.5f,-0.5f, 0.5f}, { 0.5f, 0.5f, 0.5f}, {-0.5f, 0.5f, 0.5f}
};
static const int face[6][4] = {
    {4,5,6,7}, {1,0,3,2}, {3,2,6,7}, {0,1,5,4}, {5,1,2,6}, {0,4,7,3}
};
static const float faceNormal[6][3] = {
    {0,0,1}, {0,0,-1}, {0,1,0}, {0,-1,0}, {1,0,0}, {-1,0,0}
};

static void makeScene(void)
{
    // 2,000 overlapping 32x32 sprites, two colours alternating.
    int i = 0;
    for (int row = 0; row < 40; row++)
    {
        for (int col = 0; col < 50; col++, i++)
        {
            float x = 8.0f + col*9.0f;
            float y = 8.0f + row*6.0f;
            unsigned int color = ((i & 1)? 0x80C0E0FF : 0x8040A0FF); // ABGR, alpha 0x80

            sprites[i*2+0] = (V2){ 0.0f, 0.0f, color, x, y, 0.0f };
            sprites[i*2+1] = (V2){ 64.0f, 64.0f, color, x+32.0f, y+32.0f, 0.0f };
        }
    }

    // 400 cubes on a 20x20 grid, world z = -12..-29, one indexed triangle list.
    i = 0;
    for (int row = 0; row < 20; row++)
    {
        for (int col = 0; col < 20; col++, i++)
        {
            float cx = ((float)col - 9.5f)*0.9f;
            float cz = -12.0f - (float)row*0.9f;
            unsigned int color = ((i & 1)? 0xFF7A96C8 : 0xFF5A78B4); // ABGR

            for (int f = 0; f < 6; f++)
            {
                const int *q = face[f];
                int order[6] = { q[0], q[2], q[1], q[0], q[3], q[2] };
                for (int k = 0; k < 6; k++)
                {
                    V3 *v = &cubes[i*36 + f*6 + k];
                    const float *p = corner[order[k]];
                    v->color = color;
                    v->nx = faceNormal[f][0];
                    v->ny = faceNormal[f][1];
                    v->nz = faceNormal[f][2];
                    v->x = cx + p[0]*0.8f;
                    v->y = p[1]*0.8f;
                    v->z = cz + p[2]*0.8f;
                }
            }
        }
    }

    for (i = 0; i < CUBE_VERTS; i++) cubeIndex[i] = (unsigned short)i;
}

// T2 mesh: unit-square coordinates in [-1,1] stored as s16 (x*32767) and s8 (x*127); the
// GE normalises both back to [-1,1]. A model matrix maps that to a 14x8 quad at z=-10, so
// the two arms draw identical geometry and only the vertex size differs.
static void makeMesh(void)
{
    int k = 0;

    for (int r = 0; r < MESH_ROWS; r++)
    {
        for (int c = 0; c < MESH_COLS; c++)
        {
            float x0 = -1.0f + 2.0f*(float)c/MESH_COLS;
            float x1 = -1.0f + 2.0f*(float)(c+1)/MESH_COLS;
            float y0 = -1.0f + 2.0f*(float)r/MESH_ROWS;
            float y1 = -1.0f + 2.0f*(float)(r+1)/MESH_ROWS;
            float pt[3][2] = { { x0, y0 }, { x1, y0 }, { x0, y1 } };
            unsigned int color = ((r + c) & 1)? 0xFF7A96C8 : 0xFF5A78B4;

            for (int t = 0; t < 3; t++, k++)
            {
                mesh16[k] = (V16){ color,
                                   (short)lrintf(pt[t][0]*32767.0f),
                                   (short)lrintf(pt[t][1]*32767.0f), 0, 0 };
                mesh8[k]  = (V8){ color,
                                  (signed char)lrintf(pt[t][0]*127.0f),
                                  (signed char)lrintf(pt[t][1]*127.0f), 0, 0 };
            }
        }
    }
}

static void loadMeshModel(void)
{
    ScePspFMatrix4 m;

    // Column-major, as GU expects: x/y/z are the basis columns, w carries translation.
    memset(&m, 0, sizeof(m));
    m.x.x = 7.0f;
    m.y.y = 4.0f;
    m.z.z = 1.0f;
    m.w.z = -10.0f;
    m.w.w = 1.0f;
    sceGumMatrixMode(GU_MODEL);
    sceGumLoadMatrix(&m);
}

static void emitMesh(unsigned int fmt, const void *verts)
{
    sceGuDisable(GU_TEXTURE_2D);
    sceGuDisable(GU_BLEND);
    sceGuDisable(GU_DEPTH_TEST);
    sceGuDisable(GU_CULL_FACE);
    sceGuDisable(GU_LIGHTING);
    sceGuDisable(GU_FOG);
    loadMeshModel();
    // sceGumDrawArray uploads the matrix stack the GU draw needs; sceGuDrawArray would use
    // whatever the previous draw left in the GE.
    sceGumDrawArray(GU_TRIANGLES, fmt | GU_TRANSFORM_3D, MESH_VERTS, NULL, verts);
}

static unsigned int buildMesh16(void)
{
    sceGuStart(GU_SEND, list);
    emitMesh(GU_COLOR_8888 | GU_VERTEX_16BIT, mesh16);
    return (unsigned int)sceGuFinish();
}

static unsigned int buildMesh8(void)
{
    sceGuStart(GU_SEND, list);
    emitMesh(GU_COLOR_8888 | GU_VERTEX_8BIT, mesh8);
    return (unsigned int)sceGuFinish();
}

// Read the draw buffer straight out of EDRAM (uncached), after the GE has finished.
static void capture(unsigned short *dst)
{
    const unsigned short *src = (const unsigned short *)sceGeEdramGetAddr();

    for (int y = 0; y < SCR_H; y++)
        memcpy(dst + (size_t)y*SCR_W, src + (size_t)y*STRIDE, SCR_W*2);
}

static void initGu(void)
{
    unsigned char *edram = (unsigned char *)sceGeEdramGetAddr();
    unsigned short *tex = (unsigned short *)(edram + TEX_OFF);

    // 64x64 5650 checker texture for the sprites.
    for (int y = 0; y < 64; y++)
        for (int x = 0; x < 64; x++)
            tex[y*64+x] = ((x ^ y) & 8)? 0xFFFF : 0x7C00;

    sceGuInit();
    sceGuStart(GU_DIRECT, list);
    sceGuDrawBuffer(GU_PSM_5650, (void *)0, STRIDE);
    sceGuDispBuffer(SCR_W, SCR_H, (void *)FB_BYTES, STRIDE);
    sceGuDepthBuffer((void *)DEPTH_OFF, STRIDE);
    sceGuOffset(2048 - SCR_W/2, 2048 - SCR_H/2);
    sceGuViewport(2048, 2048, SCR_W, SCR_H);
    sceGuScissor(0, 0, SCR_W, SCR_H);
    sceGuEnable(GU_SCISSOR_TEST);
    sceGuDepthRange(65535, 0);
    sceGuClearDepth(0);
    sceGuClearColor(0xFF000000);
    sceGuDepthFunc(GU_GEQUAL);
    sceGuEnable(GU_DEPTH_TEST);
    sceGuFrontFace(GU_CW);
    sceGuShadeModel(GU_SMOOTH);
    sceGuEnable(GU_CULL_FACE);
    sceGuEnable(GU_CLIP_PLANES);
    sceGuEnable(GU_DITHER);

    sceGumMatrixMode(GU_PROJECTION);
    sceGumLoadIdentity();
    sceGumPerspective(45.0f, (float)SCR_W/(float)SCR_H, 1.0f, 100.0f);
    sceGumMatrixMode(GU_VIEW);
    sceGumLoadIdentity();
    sceGumMatrixMode(GU_MODEL);
    sceGumLoadIdentity();

    sceGuFinish();
    sceGuSync(GU_SYNC_FINISH, GU_SYNC_WHAT_DONE);
    sceDisplayWaitVblankStart();
    sceGuDisplay(GU_TRUE);
}

static void emitSprites(void)
{
    sceGuDisable(GU_DEPTH_TEST);
    sceGuEnable(GU_BLEND);
    sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
    sceGuEnable(GU_TEXTURE_2D);
    sceGuTexMode(GU_PSM_5650, 0, 0, GU_FALSE);
    sceGuTexImage(0, 64, 64, 64, (void *)((unsigned char *)sceGeEdramGetAddr() + TEX_OFF));
    sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA);
    sceGuTexFilter(GU_NEAREST, GU_NEAREST);
    sceGuTexScale(1.0f, 1.0f);
    sceGuTexOffset(0.0f, 0.0f);
    sceGuTexFlush();
    sceGuDrawArray(GU_SPRITES,
                   GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D,
                   SPRITES*2, NULL, sprites);
}

static void emitCubes(void)
{
    ScePspFVector3 dir = { 0.45f, 0.85f, 0.30f };

    sceGuDisable(GU_TEXTURE_2D);
    sceGuDisable(GU_BLEND);
    sceGuEnable(GU_DEPTH_TEST);
    sceGuEnable(GU_CULL_FACE);
    sceGuEnable(GU_LIGHTING);
    sceGuEnable(GU_LIGHT0);
    sceGuLight(0, GU_DIRECTIONAL, GU_DIFFUSE, &dir);
    sceGuLightColor(0, GU_DIFFUSE, 0xFFFFFFFF);
    sceGuLightColor(0, GU_AMBIENT, 0xFF404040);
    sceGuLightAtt(0, 1.0f, 0.0f, 0.0f);
    sceGuColorMaterial(GU_AMBIENT | GU_DIFFUSE | GU_SPECULAR);
    sceGuModelColor(0, 0xFF404040, 0xFFFFFFFF, 0xFFFFFFFF);
    sceGuSpecular(8.0f);
    sceGuEnable(GU_FOG);
    sceGuFog(12.0f, 48.0f, 0xFF1C0E0A);

    sceGumDrawArray(GU_TRIANGLES,
                    GU_COLOR_8888 | GU_NORMAL_32BITF | GU_VERTEX_32BITF |
                    GU_TRANSFORM_3D | GU_INDEX_16BIT,
                    CUBE_VERTS, cubeIndex, cubes);

    sceGuDisable(GU_FOG);
    sceGuDisable(GU_LIGHTING);
}

static unsigned int buildClear(void)
{
    sceGuStart(GU_SEND, list);
    sceGuClearColor(0xFF000000);
    sceGuClearDepth(0);
    sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);
    return (unsigned int)sceGuFinish();
}

static unsigned int buildSprites(void)
{
    sceGuStart(GU_SEND, list);
    emitSprites();
    return (unsigned int)sceGuFinish();
}

static unsigned int buildCubes(void)
{
    sceGuStart(GU_SEND, list);
    emitCubes();
    return (unsigned int)sceGuFinish();
}

static unsigned int buildFrame(void)
{
    sceGuStart(GU_SEND, list);
    sceGuClearColor(0xFF000000);
    sceGuClearDepth(0);
    sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);
    emitSprites();
    emitCubes();
    return (unsigned int)sceGuFinish();
}

static void sendAndWait(unsigned int bytes)
{
    static int logged = 0;
    PspGeListArgs args;

    memset(&args, 0, sizeof(args));
    args.size = sizeof(args);

    // The list is built with CPU stores in cached memory. The GE reads it from RAM, so
    // the written bytes must be flushed before the list is published (doublelist sample).
    sceKernelDcacheWritebackRange(list, bytes);

    // sceGuSendList() does not expose the queue id; sceGuSync() then waits on the last
    // direct list, not the GU_SEND list we just sent. Enqueue directly and wait on the
    // returned id so the CPU is idle for the whole GE execution. stall = NULL: the list
    // is transferred immediately (passing `list` would stall the GE at its first byte).
    int id = sceGeListEnQueue(list, NULL, -1, &args);
    if (id >= 0)
    {
        int st = sceGeListSync(id, PSP_GE_LIST_DONE);
        int dq = sceGeListDeQueue(id);
        if (!logged)
        {
            printf("GE enqueue id=%d sync=%d deq=%d bytes=%u\n", id, st, dq, bytes);
            fflush(stdout);
            logged = 1;
        }
    }
    else if (!logged)
    {
        printf("GE enqueue FAILED id=%d\n", id);
        fflush(stdout);
        logged = 1;
    }
}

static void bench(const char *name, unsigned int (*build)(void))
{
    unsigned long long cpu[5], ge[5];
    unsigned int used = 0;

    for (int i = 0; i < 2; i++) { used = build(); sendAndWait(used); }

    for (int i = 0; i < 5; i++)
    {
        unsigned long long t0 = nowUs();
        used = build();
        unsigned long long t1 = nowUs();
        sendAndWait(used);
        unsigned long long t2 = nowUs();
        cpu[i] = t1 - t0;
        ge[i] = t2 - t1;
    }

    qsort(cpu, 5, sizeof(cpu[0]), cmpU64);
    qsort(ge, 5, sizeof(ge[0]), cmpU64);
    printf("GE %-8s cpu_med=%llu us ge_med=%llu us list_bytes=%u\n",
           name, cpu[2], ge[2], used);
    fflush(stdout);
}

int main(void)
{
    makeScene();
    makeMesh();
    sceKernelDcacheWritebackRange(sprites, sizeof(sprites));
    sceKernelDcacheWritebackRange(cubes, sizeof(cubes));
    sceKernelDcacheWritebackRange(cubeIndex, sizeof(cubeIndex));
    sceKernelDcacheWritebackRange(mesh16, sizeof(mesh16));
    sceKernelDcacheWritebackRange(mesh8, sizeof(mesh8));

    initGu();
    printf("GE_PROBE sprites=%d cubes=%d list=%d bytes\n",
           SPRITES, CUBES, LIST_BYTES);
    fflush(stdout);

    bench("clear", buildClear);
    bench("sprites", buildSprites);
    bench("cubes", buildCubes);
    bench("frame", buildFrame);
    bench("mesh16", buildMesh16);
    bench("mesh8", buildMesh8);

    // T2 parity: same geometry and colours, one format per frame; count differing pixels.
    // Both nonblack counts are printed so an all-black frame cannot pass as "0 diff".
    {
        unsigned int used;

        used = buildClear();  sendAndWait(used);
        used = buildMesh16(); sendAndWait(used);
        capture(cap16);

        used = buildClear();  sendAndWait(used);
        used = buildMesh8();  sendAndWait(used);
        capture(cap8);

        unsigned int diff = 0, nb16 = 0, nb8 = 0, nbCube = 0;

        // Instrument control: the cubes are known to draw, so a zero here means the
        // capture path is broken and neither mesh number can be trusted.
        used = buildClear(); sendAndWait(used);
        used = buildCubes(); sendAndWait(used);
        capture(cap8);
        for (int i = 0; i < SCR_W*SCR_H; i++) if (cap8[i] != 0) nbCube++;

        used = buildClear();  sendAndWait(used);
        used = buildMesh16(); sendAndWait(used);
        capture(cap16);

        used = buildClear();  sendAndWait(used);
        used = buildMesh8();  sendAndWait(used);
        capture(cap8);

        for (int i = 0; i < SCR_W*SCR_H; i++)
        {
            if (cap16[i] != cap8[i]) diff++;
            if (cap16[i] != 0) nb16++;
            if (cap8[i] != 0) nb8++;
        }

        printf("MESH_PARITY diff=%u of %d nonblack16=%u nonblack8=%u cubes_nonblack=%u\n",
               diff, SCR_W*SCR_H, nb16, nb8, nbCube);
        fflush(stdout);
    }

    printf("GE_DONE\n");
    fflush(stdout);
    printf("DONE\n");
    fflush(stdout);
    sceKernelSleepThread();
    return 0;
}
