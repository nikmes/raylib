/*
 * gated_features - sprint 017 T2/T3: the two measurement-gated texture paths.
 *
 *   make MODE=clut              # GU_PSM_T4 + 16-entry palette, 128x128, vs 4444 bytes
 *   make MODE=mipmap MIPS=0     # 256x256 checker drawn at 64x64, level 0 only
 *   make MODE=mipmap MIPS=1     # same draw with a 7-level mip chain
 *
 * Raw GU, no pspgu/raylib: the point is to prove the GE path and price it, not to change
 * the driver. The verdict/bytes/timings go to host0:/gated_done.txt; the capture is taken
 * after the app sleeps.
 */

#include <pspkernel.h>
#include <pspgu.h>
#include <pspge.h>
#include <pspdisplay.h>
#include <psputils.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

PSP_MODULE_INFO("gated_features", 0, 1, 1);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);

#define SCR_W 480
#define SCR_H 272
#define STRIDE 512
#define FB_BYTES (STRIDE*SCR_H*2)
#define TEX_OFFSET (2*FB_BYTES)

static unsigned int list[16384] __attribute__((aligned(64)));

typedef struct { float u, v, x, y, z; } Vertex;

static unsigned long long nowUs(void)
{
    return (unsigned long long)sceKernelGetSystemTimeWide();
}

static void guInit(void)
{
    sceGuInit();

    sceGuStart(GU_DIRECT, list);
    sceGuDrawBuffer(GU_PSM_5650, (void *)0, STRIDE);
    sceGuDispBuffer(SCR_W, SCR_H, (void *)(unsigned int)FB_BYTES, STRIDE);
    sceGuOffset(2048 - SCR_W/2, 2048 - SCR_H/2);
    sceGuViewport(2048, 2048, SCR_W, SCR_H);
    sceGuScissor(0, 0, SCR_W, SCR_H);
    sceGuEnable(GU_SCISSOR_TEST);
    sceGuClearColor(0xFF202020);

    // GU's matrices are undefined after sceGuInit; the clear works without them, the
    // sprite does not. Use the driver's ortho (0..480, 0..272, y down) and identity
    // view/model.
    {
        ScePspFMatrix4 identity = {
            { 1.0f, 0.0f, 0.0f, 0.0f },
            { 0.0f, 1.0f, 0.0f, 0.0f },
            { 0.0f, 0.0f, 1.0f, 0.0f },
            { 0.0f, 0.0f, 0.0f, 1.0f }
        };
        ScePspFMatrix4 proj = {
            { 2.0f/SCR_W, 0.0f, 0.0f, 0.0f },
            { 0.0f, -2.0f/SCR_H, 0.0f, 0.0f },
            { 0.0f, 0.0f, -1.0f, 0.0f },
            { -1.0f, 1.0f, -1.0f, 1.0f }
        };
        sceGuSetMatrix(GU_VIEW, &identity);
        sceGuSetMatrix(GU_MODEL, &identity);
        sceGuSetMatrix(GU_PROJECTION, &proj);
    }
    sceGuFinish();
    sceGuSync(0, 0);

    sceDisplayWaitVblankStart();
    sceGuDisplay(GU_TRUE);
}

static void drawSprite(Vertex *v, unsigned int fmt)
{
    sceKernelDcacheWritebackRange(v, sizeof(Vertex)*2);

    sceGuStart(GU_DIRECT, list);
    sceGuClear(GU_COLOR_BUFFER_BIT);
    sceGuDisable(GU_DEPTH_TEST);
    sceGuDisable(GU_BLEND);
    sceGuDisable(GU_CULL_FACE);

    /* the caller has set the texture state in this list before calling */
    sceGuDrawArray(GU_SPRITES, fmt, 2, NULL, v);

    sceGuFinish();
    sceGuSync(0, 0);
    sceDisplayWaitVblankStart();
    sceGuSwapBuffers();
}

static void sleepDone(const char *text)
{
    FILE *f = fopen("host0:/gated_done.txt", "w");
    if (f != NULL) { fputs(text, f); fclose(f); }
    sceKernelSleepThread();
}

/*------------------------------------------------------------------------------------
 * CLUT arm: 128x128 T4 with a 16-entry palette
 *------------------------------------------------------------------------------------*/
#ifdef MODE_CLUT

static unsigned char t4[128*64];                       /* 8,192 B */
static unsigned short t4444[128*128];                  /* 32,768 B */
static unsigned int palette[16];

static unsigned int pack4444(unsigned int c)
{
    unsigned int r = (c >> 0) & 0xFF, g = (c >> 8) & 0xFF, b = (c >> 16) & 0xFF, a = (c >> 24) & 0xFF;
    return ((a >> 4) << 12) | ((b >> 4) << 8) | ((g >> 4) << 4) | (r >> 4);
}

static void buildClutPattern(void)
{
    memset(t4, 0, sizeof(t4));

    palette[0] = 0xFF202020;    /* background */
    palette[1] = 0xFFFFFFFF;    /* white border */
    palette[2] = 0xFF0000FF;    /* red */
    palette[3] = 0xFF00FF00;    /* green */
    palette[4] = 0xFFFF0000;    /* blue */
    for (int i = 5; i < 16; i++) palette[i] = 0xFF808080;

    for (int cy = 0; cy < 16; cy++)
    {
        for (int cx = 0; cx < 16; cx++)
        {
            for (int y = 0; y < 8; y++)
            {
                for (int x = 0; x < 8; x++)
                {
                    int px = cx*8 + x, py = cy*8 + y;
                    int idx;

                    if ((x == 0) || (y == 0) || (x == 7) || (y == 7)) idx = 1;
                    else if ((cx + cy) % 4 == 0) idx = 2;
                    else if ((cx + cy) % 4 == 1) idx = 3;
                    else if ((cx + cy) % 4 == 2) idx = 4;
                    else idx = ((x + y) & 1)? 1 : 0;

                    int byte = py*64 + px/2;
                    if (px & 1) t4[byte] |= (unsigned char)(idx << 4);
                    else t4[byte] |= (unsigned char)idx;
                }
            }
        }
    }
}

static void runClut(void)
{
    unsigned int edram = (unsigned int)sceGeEdramGetAddr();
    void *texmem = (void *)(edram + TEX_OFFSET);

    buildClutPattern();

    /* 4444 comparison image from the same pattern/palette */
    for (int y = 0; y < 128; y++)
        for (int x = 0; x < 128; x++)
        {
            int byte = y*64 + x/2;
            int idx = (x & 1)? (t4[byte] >> 4) : (t4[byte] & 0xF);
            t4444[y*128 + x] = (unsigned short)pack4444(palette[idx]);
        }

    /* time a raw T4 copy (8 KiB) vs the 4444 image (32 KiB), 30 runs each */
    double t4ms, p4444ms;

    {
        double a = nowUs();
        for (int i = 0; i < 30; i++)
        {
            memcpy(texmem, t4, sizeof(t4));
            sceKernelDcacheWritebackRange(t4, sizeof(t4));
        }
        t4ms = (nowUs() - a)/30000.0;
    }
    {
        double a = nowUs();
        for (int i = 0; i < 30; i++)
        {
            memcpy(texmem, t4444, sizeof(t4444));
            sceKernelDcacheWritebackRange(t4444, sizeof(t4444));
        }
        p4444ms = (nowUs() - a)/30000.0;
    }

    /* upload the T4 pattern (last copy is the 4444; re-copy T4) and draw it */
    memcpy(texmem, t4, sizeof(t4));
    sceKernelDcacheWritebackRange(texmem, sizeof(t4));

    Vertex v[2] = {
        { 0.0f, 0.0f, 100.0f, 50.0f, 0.0f },
        { 1.0f, 1.0f, 228.0f, 178.0f, 0.0f },
    };

    sceKernelDcacheWritebackRange(v, sizeof(v));

    sceGuStart(GU_DIRECT, list);
    sceGuClear(GU_COLOR_BUFFER_BIT);
    sceGuDisable(GU_DEPTH_TEST);
    sceGuDisable(GU_BLEND);
    sceGuDisable(GU_CULL_FACE);

    sceGuEnable(GU_TEXTURE_2D);
    sceGuClutMode(GU_PSM_8888, 0, 0xFF, 0);
    sceGuClutLoad(2, palette);
    sceGuTexMode(GU_PSM_T4, 0, 0, GU_FALSE);
    sceGuTexImage(0, 128, 128, 128, texmem);
    sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGBA);
    sceGuTexFilter(GU_NEAREST, GU_NEAREST);
    sceGuTexWrap(GU_CLAMP, GU_CLAMP);
    sceGuTexScale(1.0f, 1.0f);
    sceGuTexOffset(0.0f, 0.0f);
    sceGuTexFlush();

    sceGuDrawArray(GU_SPRITES, GU_TEXTURE_32BITF | GU_VERTEX_32BITF, 2, NULL, v);

    // Diagnostic: a colour-only quad at (10,10) 20x20. If this appears and the texture
    // does not, the matrix/viewport/display path is fine and the texture state is the
    // suspect; if it is absent, the geometry path is the suspect.
    {
        typedef struct { unsigned int color; float x, y, z; } CVertex;
        CVertex mark[2] = {
            { 0xFF0000FFu, 10.0f, 10.0f, 0.0f },
            { 0xFF0000FFu, 30.0f, 30.0f, 0.0f },
        };
        sceKernelDcacheWritebackRange(mark, sizeof(mark));
        sceGuDisable(GU_TEXTURE_2D);
        sceGuDrawArray(GU_SPRITES, GU_COLOR_8888 | GU_VERTEX_32BITF, 2, NULL, mark);
    }

    sceGuFinish();
    sceGuSync(0, 0);
    sceDisplayWaitVblankStart();
    sceGuSwapBuffers();

    char msg[256];
    snprintf(msg, sizeof(msg),
             "DONE mode=clut bytes_t4=8192 bytes_palette=64 bytes_4444=32768 t4ms=%.3f p4444ms=%.3f\n",
             (float)t4ms, (float)p4444ms);
    sleepDone(msg);
}

#endif

/*------------------------------------------------------------------------------------
 * Mipmap arm: 256x256 RGBA checker at 64x64
 *------------------------------------------------------------------------------------*/
#ifdef MODE_MIPMAP

static unsigned int level0[256*256];
static unsigned int levels[7][256*256];                /* only the used prefixes matter */
static unsigned int levelOff[7];

static unsigned int ave2(unsigned int a, unsigned int b)
{
    unsigned int r = (((a >> 0) & 0xFF) + ((b >> 0) & 0xFF)) / 2;
    unsigned int g = (((a >> 8) & 0xFF) + ((b >> 8) & 0xFF)) / 2;
    unsigned int bl = (((a >> 16) & 0xFF) + ((b >> 16) & 0xFF)) / 2;
    unsigned int al = (((a >> 24) & 0xFF) + ((b >> 24) & 0xFF)) / 2;
    return (al << 24) | (bl << 16) | (g << 8) | r;
}

static void buildMips(void)
{
    for (int y = 0; y < 256; y++)
        for (int x = 0; x < 256; x++)
        {
            int cell = ((x/4) + (y/4)) & 1;
            level0[y*256 + x] = cell? 0xFF0080FFu : 0xFF00FFFFu;   /* blue / yellow, ABGR */
        }

    memcpy(levels[0], level0, sizeof(level0));
    levelOff[0] = 0;
    levelOff[1] = 262144;

    for (int l = 1; l < 7; l++)
    {
        int w = 256 >> l, h = 256 >> l;
        unsigned int *src = levels[l-1], *dst = levels[l];
        for (int y = 0; y < h; y++)
            for (int x = 0; x < w; x++)
            {
                int sw = 256 >> (l-1);
                unsigned int a = src[(2*y)*sw + 2*x];
                unsigned int b = src[(2*y)*sw + 2*x + 1];
                unsigned int c = src[(2*y+1)*sw + 2*x];
                unsigned int d = src[(2*y+1)*sw + 2*x + 1];
                dst[y*w + x] = ave2(ave2(a, b), ave2(c, d));
            }
        if (l > 1) levelOff[l] = levelOff[l-1] + (256u >> (l-1))*(256u >> (l-1))*4;
    }
}

static void runMipmap(void)
{
    unsigned int edram = (unsigned int)sceGeEdramGetAddr();
    unsigned char *texmem = (unsigned char *)(edram + TEX_OFFSET);
    int mipsOn = 0;
#ifdef MIPS_ON
    mipsOn = 1;
#endif

    buildMips();

    unsigned int bytes = mipsOn? 349504u : 262144u;

    /* copy each level (or only level 0) to EDRAM; time 30 runs */
    double ms;
    {
        double a = nowUs();
        for (int i = 0; i < 30; i++)
        {
            if (mipsOn)
            {
                for (int l = 0; l < 7; l++)
                {
                    int w = 256 >> l, h = 256 >> l;
                    memcpy(texmem + levelOff[l], levels[l], (unsigned int)w*h*4);
                }
                sceKernelDcacheWritebackRange(texmem, bytes);
            }
            else
            {
                memcpy(texmem, levels[0], 262144);
                sceKernelDcacheWritebackRange(texmem, 262144);
            }
        }
        ms = (nowUs() - a)/30000.0;
    }

    // GU_SPRITES does not compute texture derivatives, so it cannot select a mip level.
    // Two 3D triangles do; the ortho matrices are already in place.
    Vertex mv[6] = {
        { 0.0f, 0.0f, 208.0f, 104.0f, 0.0f },
        { 0.0f, 1.0f, 208.0f, 168.0f, 0.0f },
        { 1.0f, 1.0f, 272.0f, 168.0f, 0.0f },
        { 0.0f, 0.0f, 208.0f, 104.0f, 0.0f },
        { 1.0f, 1.0f, 272.0f, 168.0f, 0.0f },
        { 1.0f, 0.0f, 272.0f, 104.0f, 0.0f },
    };
    sceKernelDcacheWritebackRange(mv, sizeof(mv));

    sceGuStart(GU_DIRECT, list);
    sceGuClear(GU_COLOR_BUFFER_BIT);
    sceGuDisable(GU_DEPTH_TEST);
    sceGuDisable(GU_BLEND);
    sceGuDisable(GU_CULL_FACE);

    sceGuEnable(GU_TEXTURE_2D);

    if (mipsOn)
    {
        sceGuTexMode(GU_PSM_8888, 7, 0, GU_FALSE);
        for (int l = 0; l < 7; l++)
        {
            int w = 256 >> l, h = 256 >> l;
            sceGuTexImage(l, w, h, w, texmem + levelOff[l]);
        }
    }
    else
    {
        sceGuTexMode(GU_PSM_8888, 0, 0, GU_FALSE);
        sceGuTexImage(0, 256, 256, 256, texmem);
    }

    sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGBA);
    sceGuTexFilter(mipsOn? GU_LINEAR_MIPMAP_LINEAR : GU_LINEAR, GU_LINEAR);
    sceGuTexWrap(GU_CLAMP, GU_CLAMP);
    sceGuTexScale(1.0f, 1.0f);
    sceGuTexOffset(0.0f, 0.0f);
    sceGuTexFlush();

    sceGuDrawArray(GU_TRIANGLES, GU_TEXTURE_32BITF | GU_VERTEX_32BITF | GU_TRANSFORM_3D, 6, NULL, mv);

    {
        typedef struct { unsigned int color; float x, y, z; } CVertex;
        CVertex mark[2] = {
            { 0xFF0000FFu, 10.0f, 10.0f, 0.0f },
            { 0xFF0000FFu, 30.0f, 30.0f, 0.0f },
        };
        sceKernelDcacheWritebackRange(mark, sizeof(mark));
        sceGuDisable(GU_TEXTURE_2D);
        sceGuDrawArray(GU_SPRITES, GU_COLOR_8888 | GU_VERTEX_32BITF, 2, NULL, mark);
    }

    sceGuFinish();
    sceGuSync(0, 0);
    sceDisplayWaitVblankStart();
    sceGuSwapBuffers();

    char msg[256];
    snprintf(msg, sizeof(msg), "DONE mode=mipmap on=%d bytes=%u ms=%.3f\n", mipsOn, bytes, (float)ms);
    sleepDone(msg);
}

#endif

int main(void)
{
    guInit();

#if defined(MODE_CLUT)
    runClut();
#elif defined(MODE_MIPMAP)
    runMipmap();
#else
#error "Set MODE=clut|mipmap"
#endif

    return 0;
}
