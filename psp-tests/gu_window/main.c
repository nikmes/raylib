/*
 * gu_window - sprint 020 T1: can a >512 sheet be sampled as 512-pixel windows with
 * full-stride tbw? Raw GU, no pspgu/raylib: the result decides the driver design.
 *
 * Two orientations (1024x128 and 128x1024), two layouts (linear, swizzled), two windows
 * (0 and 512). Each window is drawn 1:1 and four known source columns/rows are read back
 * from the 5650 framebuffer. The verdict goes to host0:/guwin_done.txt.
 */

#include <pspkernel.h>
#include <pspgu.h>
#include <pspge.h>
#include <pspdisplay.h>
#include <psputils.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

PSP_MODULE_INFO("gu_window", 0, 1, 1);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);

#define SCR_W 480
#define SCR_H 272
#define STRIDE 512
#define FB_BYTES (STRIDE*SCR_H*2)
#define TEX_OFFSET (2*FB_BYTES)

static unsigned int list[16384] __attribute__((aligned(64)));

typedef struct {
    float u, v;
    float x, y, z;
} Vertex;

// Same layout as pspgu's swizzledOffset(): a block is 16 bytes x 8 rows, 4 texels wide
// at 8888.
static unsigned int swizzledOffset(int x, int y, int tbw, int bpp)
{
    unsigned int widthBytes = (unsigned int)tbw*(unsigned int)bpp;
    unsigned int texelsPerBlock = 16u/(unsigned int)bpp;
    unsigned int block = (unsigned int)((y/8)*(widthBytes/16) + (x/(int)texelsPerBlock))*128;

    return block + (unsigned int)((y%8)*16) +
           (unsigned int)((x%(int)texelsPerBlock)*bpp);
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

// Grayscale bands: the 5650 packing is then identical whichever channel order the
// framebuffer uses, so a failed sample means the window, not the palette decoder.
static const unsigned int palette8888[4] = {
    0xFF000000u, 0xFF555555u, 0xFFAAAAAAu, 0xFFFFFFFFu
};

static const unsigned short palette5650[4] = { 0x0000, 0x52AA, 0xAD55, 0xFFFF };

static unsigned int patternColor(int x, int y)
{
    return palette8888[((x/16) + (y/16))%4];
}

// Build the linear sheet; for the swizzled arm write each texel through swizzledOffset.
static unsigned int *buildPattern(int w, int h, int tbw, int swizzled)
{
    size_t bytes = (size_t)tbw*h*4;
    unsigned int *buf = (unsigned int *)malloc(bytes);

    if (buf == NULL) return NULL;
    memset(buf, 0, bytes);

    for (int y = 0; y < h; y++)
    {
        for (int x = 0; x < w; x++)
        {
            unsigned int c = patternColor(x, y);

            if (swizzled) buf[swizzledOffset(x, y, tbw, 4)/4] = c;
            else buf[(size_t)y*tbw + x] = c;
        }
    }

    sceKernelDcacheWritebackRange(buf, (unsigned int)bytes);
    return buf;
}

static unsigned short fbPixel(int x, int y)
{
    unsigned short *fb = (unsigned short *)sceGeEdramGetAddr();
    return fb[(size_t)y*STRIDE + x];
}

// Copy a sheet into EDRAM and write back the destination range. Writing only the source
// buffer leaves the EDRAM lines dirty in D-cache; the GE then reads the previous contents
// on the first draw. pspgu's own upload does the same destination writeback.
static void uploadEdram(const void *src, size_t bytes)
{
    void *dst = (void *)((unsigned int)sceGeEdramGetAddr() + TEX_OFFSET);

    memcpy(dst, src, bytes);
    sceKernelDcacheWritebackRange(dst, (unsigned int)bytes);
    sceKernelDelayThread(2000);     // let the EDRAM writes land before the GE samples
}

// Draw one window at 1:1 into the top-left of the screen. displayW/H cut the window so it
// fits the 480x272 screen; the UVs sample only the shown part.
static void drawWindow(int tbw, int winX, int winY, int winW, int winH,
                       int displayW, int displayH, int swizzled)
{
    unsigned int tbp = TEX_OFFSET + (swizzled? swizzledOffset(winX, winY, tbw, 4)
                                             : (unsigned int)(winY*tbw + winX)*4);
    float u1 = (float)displayW/(float)winW;
    float v1 = (float)displayH/(float)winH;
    Vertex v[2] = {
        { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f },
        { u1,   v1,   (float)displayW, (float)displayH, 0.0f }
    };

    sceGuStart(GU_DIRECT, list);
    sceGuClear(GU_COLOR_BUFFER_BIT);
    sceGuDisable(GU_DEPTH_TEST);
    sceGuDisable(GU_BLEND);
    sceGuDisable(GU_CULL_FACE);
    sceGuEnable(GU_TEXTURE_2D);
    sceGuTexMode(GU_PSM_8888, 0, 0, swizzled? GU_TRUE : GU_FALSE);
    sceGuTexImage(0, winW, winH, tbw, (void *)((unsigned int)sceGeEdramGetAddr() + tbp));
    sceGuTexFlush();
    sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGBA);
    sceGuTexFilter(GU_NEAREST, GU_NEAREST);
    sceGuTexWrap(GU_CLAMP, GU_CLAMP);
    sceGuTexScale(1.0f, 1.0f);
    sceKernelDcacheWritebackRange(v, sizeof(v));
    sceGuDrawArray(GU_SPRITES, GU_TEXTURE_32BITF | GU_VERTEX_32BITF, 2, NULL, v);
    sceGuFinish();
    sceGuSync(0, 0);
}

static int checkWindow(int tbw, int winX, int winY, int winW, int winH,
                       int orientH, int swizzled, char *line, size_t lineSize)
{
    int displayW = orientH? 480 : 128;
    int displayH = orientH? 128 : 256;
    int pass = 0;
    int samples = 4;
    char detail[160] = "";

    drawWindow(tbw, winX, winY, winW, winH, displayW, displayH, swizzled);

    for (int i = 0; i < samples; i++)
    {
        int dx = 8 + i*16;
        int sx = orientH? dx : 64;
        int sy = orientH? 64 : dx;
        int gx = winX + (orientH? dx : 64);
        int gy = winY + (orientH? 64 : dx);
        unsigned short want = palette5650[((gx/16) + (gy/16))%4];
        unsigned short got = fbPixel(sx, sy);
        int ok = (got == want);

        if (ok) pass++;
        snprintf(detail + strlen(detail), sizeof(detail) - strlen(detail),
                 " %d:%04x/%04x%s", i, got, want, ok? "" : "!");
    }

    snprintf(line, lineSize, "WINDOW layout=%s orient=%c win=%d tbw=%d tbp=%u samples=%d pass=%d%s",
             swizzled? "swizzled" : "linear", orientH? 'h' : 'v', winX? 1 : 0, tbw,
             swizzled? swizzledOffset(winX, winY, tbw, 4)
                     : (unsigned int)(winY*tbw + winX)*4,
             samples, pass, detail);
    return pass == samples;
}

int main(void)
{
    char report[4096] = "";
    int control = 1, linearH = 1, swizzledH = 1, linearV = 1, swizzledV = 1;

    guInit();

    // Control: the same sprite path with tbw=512 and no window offset. If this fails,
    // the probe's draw/readback is wrong, not the window layout.
    {
        unsigned int *ctl = buildPattern(512, 128, 512, 0);
        char line[256];

        uploadEdram(ctl, 512*128*4);
        int ok = checkWindow(512, 0, 0, 512, 128, 1, 0, line, sizeof(line));
        control = ok;
        strncat(report, line, sizeof(report) - strlen(report) - 1);
        strncat(report, "\nCONTROL tbw512=", sizeof(report) - strlen(report) - 1);
        strncat(report, ok? "1\n" : "0\n", sizeof(report) - strlen(report) - 1);
        free(ctl);
    }

    // Horizontal: 1024x128, tbw 1024, two 512x128 windows.
    {
        unsigned int *lin = buildPattern(1024, 128, 1024, 0);
        unsigned int *swz = buildPattern(1024, 128, 1024, 1);

        for (int sw = 0; sw < 2; sw++)
        {
            unsigned int *src = sw? swz : lin;
            char line[256];

            uploadEdram(src, 1024*128*4);

            for (int win = 0; win < 2; win++)
            {
                int ok = checkWindow(1024, win*512, 0, 512, 128, 1, sw, line, sizeof(line));
                strncat(report, line, sizeof(report) - strlen(report) - 1);
                strncat(report, "\n", sizeof(report) - strlen(report) - 1);
                if (sw) swizzledH &= ok; else linearH &= ok;
            }
        }

        free(lin); free(swz);
    }

    // Vertical: 128x1024, tbw 128, two 128x512 windows.
    {
        unsigned int *lin = buildPattern(128, 1024, 128, 0);
        unsigned int *swz = buildPattern(128, 1024, 128, 1);

        for (int sw = 0; sw < 2; sw++)
        {
            unsigned int *src = sw? swz : lin;
            char line[256];

            uploadEdram(src, 128*1024*4);

            for (int win = 0; win < 2; win++)
            {
                int ok = checkWindow(128, 0, win*512, 128, 512, 0, sw, line, sizeof(line));
                strncat(report, line, sizeof(report) - strlen(report) - 1);
                strncat(report, "\n", sizeof(report) - strlen(report) - 1);
                if (sw) swizzledV &= ok; else linearV &= ok;
            }
        }

        free(lin); free(swz);
    }

    {
        char verdict[256];
        snprintf(verdict, sizeof(verdict),
                 "VERDICT control=%d linear_h=%d swizzled_h=%d linear_v=%d swizzled_v=%d\n",
                 control, linearH, swizzledH, linearV, swizzledV);
        strncat(report, verdict, sizeof(report) - strlen(report) - 1);
    }

    FILE *f = fopen("host0:/guwin_done.txt", "w");
    if (f != NULL)
    {
        fputs(report, f);
        fputs("DONE\n", f);
        fclose(f);
    }

    sceKernelSleepThread();
    return 0;
}
