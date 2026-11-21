/*
 * texture_matrix - sprint 017 T1: upload/swizzle by pixel format and size.
 *
 *   make MATRIX=size   # RGBA8888 at 32,64,128,256,384,512 (512 expected not to fit)
 *   make MATRIX=fmt    # RGB/5650, opaque RGBA/4444, alpha-128 RGBA/8888 at 32,128,256,512
 *
 * The Image is built once per row; the timed region is LoadTextureFromImage +
 * UnloadTexture, no PNG/file decode. The driver's per-texture log is turned off while
 * timing; the texture bytes come from the residency shadow counter. One frame is ended
 * per row so the 240 KiB display list cannot fill (the churn finding).
 *
 * Run the same binary on the default 8888 driver and on --texfmt=auto. The resulting
 * matrix is written to matrix_done.txt.
 */

#include <pspkernel.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "raylib.h"

typedef struct {
    unsigned int resident;
    unsigned int shadowBytes;
    unsigned int evictions;
    unsigned int reloads;
    unsigned int failed;
    unsigned int coalesced;
} PspGuResidencyStats;

void pspguResidencyStats(PspGuResidencyStats *out);
void pspguVramReport(const char *when);
void pspguSetTextureLog(int on);

PSP_MODULE_INFO("texture_matrix", 0, 1, 1);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);

#define SRC_RGBA   0
#define SRC_RGB    1
#define SRC_ALPHA  2

static char out[8192];
static int outLen = 0;

static void appendf(const char *fmt, ...)
{
    if (outLen >= (int)sizeof(out) - 1) return;

    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(out + outLen, sizeof(out) - outLen, fmt, ap);
    va_end(ap);

    if (n > 0) outLen += n;
    if (outLen >= (int)sizeof(out)) outLen = sizeof(out) - 1;
}

static const char *srcName(int src)
{
    return (src == SRC_RGB)? "RGB" : (src == SRC_ALPHA)? "RGBA128" : "RGBA";
}

static Image makeImage(int w, int h, int src)
{
    Image img = GenImageColor(w, h, (Color){ 200, 40, 90, 255 });

    if (src == SRC_RGB) ImageFormat(&img, PIXELFORMAT_UNCOMPRESSED_R8G8B8);
    else if (src == SRC_ALPHA)
    {
        Color *p = (Color *)img.data;
        for (int i = 0; i < w*h; i++) p[i].a = 128;
    }

    return img;
}

static int cmpDouble(const void *a, const void *b)
{
    double x = *(const double *)a, y = *(const double *)b;
    return (x > y) - (x < y);
}

static void runRow(int size, int src)
{
    Image img = makeImage(size, size, src);

    PspGuResidencyStats s0, s1;
    pspguResidencyStats(&s0);
    Texture2D first = LoadTextureFromImage(img);
    pspguResidencyStats(&s1);
    UnloadTexture(first);

    unsigned int bytes = (s1.shadowBytes > s0.shadowBytes)? (s1.shadowBytes - s0.shadowBytes) : 0;

    double t[30];
    for (int i = 0; i < 30; i++)
    {
        double a = GetTime();
        Texture2D tx = LoadTextureFromImage(img);
        UnloadTexture(tx);
        t[i] = GetTime() - a;
    }

    qsort(t, 30, sizeof(double), cmpDouble);

    appendf("size=%d src=%s bytes=%u med=%.3f min=%.3f max=%.3f\n",
            size, srcName(src), bytes,
            (float)(t[15]*1000.0), (float)(t[0]*1000.0), (float)(t[29]*1000.0));

    // End a frame so the texture commands do not accumulate in one list.
    BeginDrawing();
        ClearBackground(BLACK);
    EndDrawing();

    UnloadImage(img);
}

int main(void)
{
    InitWindow(480, 272, "texture matrix");
    SetTargetFPS(0);
    SetTraceLogLevel(LOG_WARNING);
    pspguSetTextureLog(0);

    pspguVramReport("start");

#ifdef MATRIX_SIZE
    const int sizes[] = { 32, 64, 128, 256, 384, 512 };
    for (unsigned i = 0; i < sizeof(sizes)/sizeof(sizes[0]); i++) runRow(sizes[i], SRC_RGBA);
#elif defined(MATRIX_FMT)
    const int sizes[] = { 32, 128, 256, 512 };
    const int srcs[] = { SRC_RGB, SRC_RGBA, SRC_ALPHA };
    for (unsigned i = 0; i < sizeof(sizes)/sizeof(sizes[0]); i++)
        for (unsigned j = 0; j < sizeof(srcs)/sizeof(srcs[0]); j++)
            runRow(sizes[i], srcs[j]);
#else
#error "Set MATRIX=size|fmt"
#endif

    pspguVramReport("end");
    SaveFileText("matrix_done.txt", out);

    TraceLog(LOG_WARNING, "DONE matrix rows written");
    sceKernelSleepThread();

    return 0;
}
