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

// T4: the same 64x64 image uploaded three ways - 5650, 8888 and CLUT8 (T8 + 256-entry
// palette). Every colour is exactly representable in all three, so any pixel difference
// is a decode bug, not quantisation.
#define TEX8888_OFF (TEX_OFF + 0x4000)
#define TEX8_OFF    (TEX8888_OFF + 0x4000)
#define CLUT_OFF    (TEX8_OFF + 0x1000)
#define TEX_COLORS 64

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

// T3a: the same quad grid drawn two ways. A list needs 6 vertices per quad (36,000 for
// 6,000 quads); per-row strips need 2*(COLS+1) per row (12,120 in 60 draws).
#define QUAD_COLS 100
#define QUAD_ROWS 60
#define QUAD_TRIS (QUAD_COLS*QUAD_ROWS*2)
#define LIST_VERTS (QUAD_TRIS*3)
#define STRIP_ROW_VERTS (2*(QUAD_COLS+1))
#define STRIP_VERTS (QUAD_ROWS*STRIP_ROW_VERTS)

static V2 sprites[SPRITES*2] __attribute__((aligned(64)));
static V3 cubes[CUBE_VERTS] __attribute__((aligned(64)));
static unsigned short cubeIndex[CUBE_VERTS] __attribute__((aligned(64)));
static V16 mesh16[MESH_VERTS] __attribute__((aligned(64)));
static V8  mesh8[MESH_VERTS] __attribute__((aligned(64)));
static V16 listMesh[LIST_VERTS] __attribute__((aligned(64)));
static V16 stripMesh[STRIP_VERTS] __attribute__((aligned(64)));

// T5b: large sprites drawn whole vs sliced into 64-px vertical strips. Same UV mapping,
// same pixels; only the quad count changes (16 vs 32 on this grid).
#define BIG_N 16
#define BIG_SLICES 2

static V2 bigWhole[BIG_N*6] __attribute__((aligned(64)));
static V2 bigSlice[BIG_N*BIG_SLICES*6] __attribute__((aligned(64)));

// T6: what a static CALL list could save is the CPU cost of emitting N draw commands.
// 1,000 small quads drawn as 1,000 calls, then as one call over the same 6,000 vertices.
#define K1_N 1000

typedef struct { unsigned int color; float x, y, z; } V1;
static V1 k1[K1_N*6] __attribute__((aligned(64)));

// T4 texture source: one image, three encodings, one palette.
static unsigned short tex5650[64*64] __attribute__((aligned(64)));
static unsigned int   tex8888[64*64] __attribute__((aligned(64)));
static unsigned char  tex8[64*64] __attribute__((aligned(64)));
static unsigned int   clut[256] __attribute__((aligned(64)));

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

static V16 mk16(unsigned int color, float x, float y)
{
    V16 v = { color, (short)lrintf(x*32767.0f), (short)lrintf(y*32767.0f), 0, 0 };
    return v;
}

static void makeQuadMesh(void)
{
    int li = 0, si = 0;

    for (int r = 0; r < QUAD_ROWS; r++)
    {
        float y0 = -1.0f + 2.0f*(float)r/QUAD_ROWS;
        float y1 = -1.0f + 2.0f*(float)(r+1)/QUAD_ROWS;
        unsigned int color = (r & 1)? 0xFF7A96C8 : 0xFF5A78B4;

        for (int c = 0; c < QUAD_COLS; c++)
        {
            float x0 = -1.0f + 2.0f*(float)c/QUAD_COLS;
            float x1 = -1.0f + 2.0f*(float)(c+1)/QUAD_COLS;
            V16 a = mk16(color, x0, y0);
            V16 b = mk16(color, x0, y1);
            V16 d = mk16(color, x1, y0);
            V16 e = mk16(color, x1, y1);

            listMesh[li++] = a; listMesh[li++] = b; listMesh[li++] = d;
            listMesh[li++] = d; listMesh[li++] = b; listMesh[li++] = e;
        }

        for (int c = 0; c <= QUAD_COLS; c++)
        {
            float x = -1.0f + 2.0f*(float)c/QUAD_COLS;
            stripMesh[si++] = mk16(color, x, y0);
            stripMesh[si++] = mk16(color, x, y1);
        }
    }
}

static void emitMeshStates(void)
{
    sceGuDisable(GU_TEXTURE_2D);
    sceGuDisable(GU_BLEND);
    sceGuDisable(GU_DEPTH_TEST);
    sceGuDisable(GU_CULL_FACE);
    sceGuDisable(GU_LIGHTING);
    sceGuDisable(GU_FOG);
    loadMeshModel();
}

static unsigned int buildStrip(void)
{
    sceGuStart(GU_SEND, list);
    emitMeshStates();
    for (int r = 0; r < QUAD_ROWS; r++)
        sceGumDrawArray(GU_TRIANGLE_STRIP,
                        GU_COLOR_8888 | GU_VERTEX_16BIT | GU_TRANSFORM_3D,
                        STRIP_ROW_VERTS, NULL, stripMesh + r*STRIP_ROW_VERTS);
    return (unsigned int)sceGuFinish();
}

static unsigned int buildList(void)
{
    sceGuStart(GU_SEND, list);
    emitMeshStates();
    sceGumDrawArray(GU_TRIANGLES, GU_COLOR_8888 | GU_VERTEX_16BIT | GU_TRANSFORM_3D,
                    LIST_VERTS, NULL, listMesh);
    return (unsigned int)sceGuFinish();
}

// T3b: what the per-draw index copy costs. The driver's run copy is the loop form
// (`rb[i] = idx[...]`); memcpy is the ceiling if the run were contiguous.
#define IDX_N 36000
static unsigned short idxSrc[IDX_N] __attribute__((aligned(64)));
static unsigned short idxDst[IDX_N] __attribute__((aligned(64)));

static void benchIndexCopy(void)
{
    unsigned long long loop[5], mcpy[5];

    for (int i = 0; i < IDX_N; i++) idxSrc[i] = (unsigned short)(i & 0x3FFF);

    for (int r = 0; r < 2; r++)
    {
        for (int i = 0; i < IDX_N; i++) idxDst[i] = idxSrc[i];
        memcpy(idxDst, idxSrc, sizeof(idxSrc));
    }

    for (int r = 0; r < 5; r++)
    {
        unsigned long long t0 = nowUs();
        for (int i = 0; i < IDX_N; i++) idxDst[i] = idxSrc[i];
        loop[r] = nowUs() - t0;
    }

    for (int r = 0; r < 5; r++)
    {
        unsigned long long t0 = nowUs();
        memcpy(idxDst, idxSrc, sizeof(idxSrc));
        mcpy[r] = nowUs() - t0;
    }

    qsort(loop, 5, sizeof(loop[0]), cmpU64);
    qsort(mcpy, 5, sizeof(mcpy[0]), cmpU64);
    printf("IDXCOPY n=%d loop_med=%llu us memcpy_med=%llu us (check %u)\n",
           IDX_N, loop[2], mcpy[2], idxDst[7]);
    fflush(stdout);
}

static unsigned int expand5650(unsigned short w)
{
    unsigned int r5 = w & 0x1F, g6 = (w >> 5) & 0x3F, b5 = (w >> 11) & 0x1F;
    unsigned int r8 = (r5 << 3)|(r5 >> 2);
    unsigned int g8 = (g6 << 2)|(g6 >> 4);
    unsigned int b8 = (b5 << 3)|(b5 >> 2);

    return 0xFF000000u | (b8 << 16) | (g8 << 8) | r8;   // ABGR words, as GU stores 8888
}

static unsigned short makeColor5650(int idx)
{
    return (unsigned short)((idx & 0x1F) | (((idx*3) & 0x3F) << 5) | (((idx*7) & 0x1F) << 11));
}

static void makeTextures(void)
{
    unsigned char *edram = (unsigned char *)sceGeEdramGetAddr();

    for (int k = 0; k < 256; k++) clut[k] = expand5650(makeColor5650(k % TEX_COLORS));

    for (int y = 0; y < 64; y++)
    {
        for (int x = 0; x < 64; x++)
        {
            int idx = (((x >> 3) & 7)*8 + ((y >> 3) & 7)) % TEX_COLORS;

            tex8[y*64 + x] = (unsigned char)idx;
            tex8888[y*64 + x] = expand5650(makeColor5650(idx));
            tex5650[y*64 + x] = makeColor5650(idx);
        }
    }

    memcpy(edram + TEX_OFF, tex5650, sizeof(tex5650));
    memcpy(edram + TEX8888_OFF, tex8888, sizeof(tex8888));
    memcpy(edram + TEX8_OFF, tex8, sizeof(tex8));
}

static void makeBigSprites(void)
{
    int w = 0, s = 0;

    for (int r = 0; r < 4; r++)
    {
        for (int c = 0; c < 4; c++)
        {
            float x = (float)c*120.0f, y = (float)r*68.0f;
            unsigned int color = (((r + c) & 1)? 0x80C0E0FF : 0x8040A0FF);

            V2 p0 = { 0.0f, 0.0f, color, x, y, 0.0f };
            V2 p1 = { 0.0f, 68.0f, color, x, y + 68.0f, 0.0f };
            V2 p2 = { 120.0f, 0.0f, color, x + 120.0f, y, 0.0f };
            V2 p3 = { 120.0f, 68.0f, color, x + 120.0f, y + 68.0f, 0.0f };

            bigWhole[w++] = p0; bigWhole[w++] = p1; bigWhole[w++] = p2;
            bigWhole[w++] = p2; bigWhole[w++] = p1; bigWhole[w++] = p3;

            // Two 64-px strips: [0,64) and [64,120); the UV split matches the pixel split.
            float cuts[3] = { 0.0f, 64.0f, 120.0f };

            for (int k = 0; k < BIG_SLICES; k++)
            {
                float u0 = cuts[k], u1 = cuts[k+1];
                float dx0 = x + u0, dx1 = x + u1;
                V2 q0 = { u0, 0.0f, color, dx0, y, 0.0f };
                V2 q1 = { u0, 68.0f, color, dx0, y + 68.0f, 0.0f };
                V2 q2 = { u1, 0.0f, color, dx1, y, 0.0f };
                V2 q3 = { u1, 68.0f, color, dx1, y + 68.0f, 0.0f };

                bigSlice[s++] = q0; bigSlice[s++] = q1; bigSlice[s++] = q2;
                bigSlice[s++] = q2; bigSlice[s++] = q1; bigSlice[s++] = q3;
            }
        }
    }
}

static void makeK1(void)
{
    int k = 0;

    for (int r = 0; r < 25; r++)
    {
        for (int c = 0; c < 40; c++)
        {
            float x = (float)c*12.0f, y = (float)r*10.86f;
            unsigned int col = (((r + c) & 1)? 0xFF7A96C8 : 0xFF5A78B4);
            V1 a  = { col, x, y, 0.0f };
            V1 b  = { col, x, y + 8.0f, 0.0f };
            V1 d  = { col, x + 8.0f, y, 0.0f };
            V1 e  = { col, x + 8.0f, y + 8.0f, 0.0f };

            k1[k++] = a; k1[k++] = b; k1[k++] = d;
            k1[k++] = d; k1[k++] = b; k1[k++] = e;
        }
    }
}

static void emitK1States(void)
{
    sceGuDisable(GU_TEXTURE_2D);
    sceGuDisable(GU_BLEND);
    sceGuDisable(GU_DEPTH_TEST);
    sceGuDisable(GU_CULL_FACE);
    sceGuDisable(GU_LIGHTING);
    sceGuDisable(GU_FOG);
}

static unsigned int buildDraw1k(void)
{
    sceGuStart(GU_SEND, list);
    emitK1States();
    for (int i = 0; i < K1_N; i++)
        sceGuDrawArray(GU_TRIANGLES,
                       GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D,
                       6, NULL, &k1[i*6]);
    return (unsigned int)sceGuFinish();
}

static unsigned int buildDrawOne(void)
{
    sceGuStart(GU_SEND, list);
    emitK1States();
    sceGuDrawArray(GU_TRIANGLES,
                   GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D,
                   K1_N*6, NULL, k1);
    return (unsigned int)sceGuFinish();
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

static void emitSpriteTexState(unsigned int psm, unsigned int texOff, int clut8)
{
    unsigned char *edram = (unsigned char *)sceGeEdramGetAddr();

    sceGuDisable(GU_DEPTH_TEST);
    sceGuEnable(GU_BLEND);
    sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
    sceGuEnable(GU_TEXTURE_2D);

    if (clut8)
    {
        sceGuClutMode(GU_PSM_8888, 0, 0xFF, 0);
        sceGuClutLoad(8, clut);          // 8 blocks of 32 entries = 256
    }

    sceGuTexMode(psm, 0, 0, GU_FALSE);
    sceGuTexImage(0, 64, 64, 64, (void *)(edram + texOff));
    sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA);
    sceGuTexFilter(GU_NEAREST, GU_NEAREST);
    sceGuTexScale(1.0f, 1.0f);
    sceGuTexOffset(0.0f, 0.0f);
    sceGuTexFlush();
}

static void emitSpritesTex(unsigned int psm, unsigned int texOff, int clut8)
{
    emitSpriteTexState(psm, texOff, clut8);
    sceGuDrawArray(GU_SPRITES,
                   GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D,
                   SPRITES*2, NULL, sprites);
}

static unsigned int buildBigWhole(void)
{
    sceGuStart(GU_SEND, list);
    emitSpriteTexState(GU_PSM_5650, TEX_OFF, 0);
    sceGuDrawArray(GU_TRIANGLES,
                   GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D,
                   BIG_N*6, NULL, bigWhole);
    return (unsigned int)sceGuFinish();
}

static unsigned int buildBigSlice(void)
{
    sceGuStart(GU_SEND, list);
    emitSpriteTexState(GU_PSM_5650, TEX_OFF, 0);
    sceGuDrawArray(GU_TRIANGLES,
                   GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D,
                   BIG_N*BIG_SLICES*6, NULL, bigSlice);
    return (unsigned int)sceGuFinish();
}

// Instrument check: the same whole-sprite draw 8 times. If GE time does not scale with
// this, the 40 us reading is a timer artefact and the T5b comparison is meaningless.
static unsigned int buildBigWhole8x(void)
{
    sceGuStart(GU_SEND, list);
    emitSpriteTexState(GU_PSM_5650, TEX_OFF, 0);
    for (int k = 0; k < 8; k++)
        sceGuDrawArray(GU_TRIANGLES,
                       GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D,
                       BIG_N*6, NULL, bigWhole);
    return (unsigned int)sceGuFinish();
}

static void emitSprites(void)
{
    emitSpritesTex(GU_PSM_5650, TEX_OFF, 0);
}

static unsigned int buildSprites8888(void)
{
    sceGuStart(GU_SEND, list);
    emitSpritesTex(GU_PSM_8888, TEX8888_OFF, 0);
    return (unsigned int)sceGuFinish();
}

static unsigned int buildSprites8(void)
{
    sceGuStart(GU_SEND, list);
    emitSpritesTex(GU_PSM_T8, TEX8_OFF, 1);
    return (unsigned int)sceGuFinish();
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
    makeQuadMesh();
    makeBigSprites();
    makeK1();
    sceKernelDcacheWritebackRange(sprites, sizeof(sprites));
    sceKernelDcacheWritebackRange(cubes, sizeof(cubes));
    sceKernelDcacheWritebackRange(cubeIndex, sizeof(cubeIndex));
    sceKernelDcacheWritebackRange(mesh16, sizeof(mesh16));
    sceKernelDcacheWritebackRange(mesh8, sizeof(mesh8));
    sceKernelDcacheWritebackRange(listMesh, sizeof(listMesh));
    sceKernelDcacheWritebackRange(stripMesh, sizeof(stripMesh));
    sceKernelDcacheWritebackRange(bigWhole, sizeof(bigWhole));
    sceKernelDcacheWritebackRange(bigSlice, sizeof(bigSlice));
    sceKernelDcacheWritebackRange(k1, sizeof(k1));

    initGu();
    makeTextures();          // after initGu's checker: all texture arms share one image
    printf("GE_PROBE sprites=%d cubes=%d list=%d bytes\n",
           SPRITES, CUBES, LIST_BYTES);
    fflush(stdout);

    bench("clear", buildClear);
    bench("sprites", buildSprites);
    bench("sprites8888", buildSprites8888);
    bench("sprites8", buildSprites8);
    bench("big", buildBigWhole);
    bench("bigslice", buildBigSlice);
    bench("big8x", buildBigWhole8x);
    bench("k1_draws", buildDraw1k);
    bench("k1_one", buildDrawOne);
    bench("cubes", buildCubes);
    bench("frame", buildFrame);
    bench("mesh16", buildMesh16);
    bench("mesh8", buildMesh8);
    bench("strip", buildStrip);
    bench("list", buildList);
    benchIndexCopy();

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

        // T3a: strips vs the triangle list, same quad grid and same vertices.
        unsigned int sdiff = 0, nbStrip = 0, nbList = 0;

        used = buildClear(); sendAndWait(used);
        used = buildStrip(); sendAndWait(used);
        capture(cap16);

        used = buildClear(); sendAndWait(used);
        used = buildList();  sendAndWait(used);
        capture(cap8);

        for (int i = 0; i < SCR_W*SCR_H; i++)
        {
            if (cap16[i] != cap8[i]) sdiff++;
            if (cap16[i] != 0) nbStrip++;
            if (cap8[i] != 0) nbList++;
        }

        printf("STRIP_PARITY diff=%u of %d nonblack_strip=%u nonblack_list=%u "
               "verts_strip=%d verts_list=%d\n",
               sdiff, SCR_W*SCR_H, nbStrip, nbList, STRIP_VERTS, LIST_VERTS);
        fflush(stdout);

        // T4: the same 2,000 sprites through 5650, 8888 and CLUT8.
        unsigned int d8888 = 0, d8 = 0, nbTex = 0;

        used = buildClear();         sendAndWait(used);
        used = buildSprites();       sendAndWait(used);
        capture(cap16);

        used = buildClear();         sendAndWait(used);
        used = buildSprites8888();   sendAndWait(used);
        capture(cap8);
        for (int i = 0; i < SCR_W*SCR_H; i++) if (cap16[i] != cap8[i]) d8888++;

        used = buildClear();         sendAndWait(used);
        used = buildSprites8();      sendAndWait(used);
        capture(cap8);
        for (int i = 0; i < SCR_W*SCR_H; i++)
        {
            if (cap16[i] != cap8[i]) d8++;
            if (cap16[i] != 0) nbTex++;
        }

        printf("TEX_PARITY diff_5650_8888=%u diff_5650_t8=%u nonblack=%u "
               "(vram 5650=8K 8888=16K t8=4K+1K)\n", d8888, d8, nbTex);
        fflush(stdout);

        // T5b: large sprites whole vs 64-px sliced.
        unsigned int dslice = 0, nbBig = 0;

        used = buildClear();     sendAndWait(used);
        used = buildBigWhole();  sendAndWait(used);
        capture(cap16);

        used = buildClear();     sendAndWait(used);
        used = buildBigSlice();  sendAndWait(used);
        capture(cap8);

        for (int i = 0; i < SCR_W*SCR_H; i++)
        {
            if (cap16[i] != cap8[i]) dslice++;
            if (cap16[i] != 0) nbBig++;
        }

        printf("SLICE_PARITY diff=%u of %d nonblack=%u quads_whole=%d quads_sliced=%d\n",
               dslice, SCR_W*SCR_H, nbBig, BIG_N, BIG_N*BIG_SLICES);
        fflush(stdout);
    }

    printf("GE_DONE\n");
    fflush(stdout);
    printf("DONE\n");
    fflush(stdout);
    sceKernelSleepThread();
    return 0;
}
