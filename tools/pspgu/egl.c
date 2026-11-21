/*
 * egl.c - display, context, surface, swap, and the one place the GE is initialised.
 *
 * This is task 1 of phase 5: the vertical slice. Everything the platform layer asks for
 * is here, and the framebuffers live in EDRAM exactly as pspGL's do, so a capture is
 * comparable pixel for pixel with the pspGL build.
 */

#include <stdio.h>
#include <string.h>
#include <malloc.h>

#include "pspgu_internal.h"

PspGuContext pspgu;
int pspguWarnedNoContext = 0;

// GU needs EDRAM offsets, not addresses: 0 is the start of video memory
#define EDRAM(offset) ((void *)(offset))

static unsigned int alignedListBytes(unsigned int n)
{
    return (n + 63) & ~63u;             // 64-byte alignment, as GU's own examples use
}

// Boot the GE. Called from eglMakeCurrent, which is the first moment a GL call is legal.
static void pspguInitGu(void)
{
    // Fill the struct first: fb is derived from it, and reading it before the assignment
    // gave 0-byte framebuffers that still cleared correctly, which is exactly the kind of
    // bug this log line exists to catch.
    pspgu.width = PSP_GU_WIDTH;
    pspgu.height = PSP_GU_HEIGHT;
    pspgu.stride = PSP_GU_STRIDE;

    unsigned int fb = pspgu.stride * pspgu.height * 2;      // 5650 = 2 bytes per pixel

    pspgu.fbBytes = fb;

    pspgu.listBase = memalign(64, alignedListBytes(PSP_GU_LIST_BYTES));
    pspgu.list = pspgu.listBase;

    // VRAM map: draw framebuffer, display framebuffer, optional depth, then textures
    pspgu.drawOffset = EDRAM(0);
    pspgu.dispOffset = EDRAM(fb);
    pspgu.vramNext = 2*fb;

#if PSP_GU_DEPTH
    pspgu.depthOffset = EDRAM(2*fb);
    pspgu.vramNext = 3*fb;
#else
    pspgu.depthOffset = NULL;
#endif
    pspgu.vramEnd = 2*1024*1024;        // the whole of EDRAM

    sceGuInit();

    sceGuStart(GU_DIRECT, pspgu.listBase);
    pspgu.listOpen = 1;

    sceGuDrawBuffer(GU_PSM_5650, pspgu.drawOffset, pspgu.stride);
    sceGuDispBuffer(PSP_GU_WIDTH, PSP_GU_HEIGHT, pspgu.dispOffset, pspgu.stride);

    // 480 centred in a 512-wide buffer: the recipe in the phase page, with numbers
    // GU's matrices are undefined after sceGuInit: samples always set all three. The view
    // matrix in particular has to be identity or the MVP collapses every vertex to a point
    // - which looks exactly like "the driver draws nothing", while the clear still works.
    {
        ScePspFMatrix4 identity = {
            { 1.0f, 0.0f, 0.0f, 0.0f },
            { 0.0f, 1.0f, 0.0f, 0.0f },
            { 0.0f, 0.0f, 1.0f, 0.0f },
            { 0.0f, 0.0f, 0.0f, 1.0f }
        };
        sceGuSetMatrix(GU_VIEW, &identity);
        sceGuSetMatrix(GU_MODEL, &identity);
    }

    sceGuOffset(2048 - PSP_GU_WIDTH/2, 2048 - PSP_GU_HEIGHT/2);
    sceGuViewport(2048, 2048, PSP_GU_WIDTH, PSP_GU_HEIGHT);
    sceGuScissor(0, 0, PSP_GU_WIDTH, PSP_GU_HEIGHT);
    sceGuEnable(GU_SCISSOR_TEST);

#if PSP_GU_DEPTH
    sceGuDepthBuffer(pspgu.depthOffset, pspgu.stride);
    sceGuDepthRange(65535, 0);
#endif

    sceGuFinish();
    sceGuSync(0, 0);
    pspgu.listOpen = 0;

    sceDisplayWaitVblankStart();
    sceGuDisplay(GU_TRUE);

    pspguInitTextures();
    pspguResetMatrices();
    pspgu.clearColor = 0xFF000000;      // opaque black, what rlglInit clears to
    pspgu.clearDepth = 1.0f;
    pspgu.vsync = 1;
    pspgu.current = 1;

    pspguLog("GU up: %dx%d stride %d, fb %u KiB x2, depth %s (%u KiB), textures from %u KiB, list %u KiB",
                 pspgu.width, pspgu.height, pspgu.stride, pspgu.fbBytes/1024,
                 (pspgu.depthOffset != NULL)? "yes":"no",
                 (pspgu.depthOffset != NULL)? (pspgu.fbBytes/1024) : 0,
                 pspgu.vramNext/1024, PSP_GU_LIST_BYTES/1024);
}

// Start a list if none is open. Every emitter calls this; it is free when already open.
void pspguBeginFrame(void)
{
    if (!pspgu.current) return;

    if (!pspgu.listOpen)
    {
        pspgu.list = pspgu.listBase;
        sceGuStart(GU_DIRECT, pspgu.list);
        pspgu.listOpen = 1;
    }
}

// Finish the list and let the GE work through it. GU_SYNC_WHAT_QUEUED would be cheaper;
// the slice uses a full sync so a hang is a hang and not a mystery.
void pspguEndFrame(void)
{
    if (!pspgu.current || !pspgu.listOpen) return;

    sceGuFinish();
    sceGuSync(0, 0);
    pspgu.listOpen = 0;
}

unsigned int pspguVramAlloc(unsigned int bytes)
{
    unsigned int off = (pspgu.vramNext + 63) & ~63u;

    if ((off + bytes) > pspgu.vramEnd) return 0;        // out of VRAM; caller decides

    pspgu.vramNext = off + bytes;

    return off;
}

//------------------------------------------------------------------------------------
// EGL
//------------------------------------------------------------------------------------

EGLDisplay eglGetDisplay(NativeDisplayType display_id)
{
    (void)display_id;

    return (EGLDisplay)&pspgu;          // one display, and it is the context struct
}

EGLBoolean eglInitialize(EGLDisplay dpy, EGLint *major, EGLint *minor)
{
    (void)dpy;

    if (major) *major = 1;
    if (minor) *minor = 4;
    pspgu.inited = 1;

    return EGL_TRUE;
}

EGLBoolean eglChooseConfig(EGLDisplay dpy, const EGLint *attrib_list, EGLConfig *configs,
                           EGLint config_size, EGLint *num_config)
{
    (void)dpy; (void)attrib_list; (void)config_size;

    // One config, 5650 + optional 16-bit depth. The platform layer asks for
    // EGL_DEPTH_SIZE 1 and checks that exactly one config comes back.
    if (configs != NULL) configs[0] = (EGLConfig)&pspgu;
    if (num_config != NULL) *num_config = 1;

    return EGL_TRUE;
}

EGLContext eglCreateContext(EGLDisplay dpy, EGLConfig config, EGLContext share,
                            const EGLint *attrib_list)
{
    (void)dpy; (void)config; (void)share; (void)attrib_list;

    return (EGLContext)&pspgu;
}

EGLSurface eglCreateWindowSurface(EGLDisplay dpy, EGLConfig config, NativeWindowType win,
                                  const EGLint *attrib_list)
{
    (void)dpy; (void)config; (void)win; (void)attrib_list;

    return (EGLSurface)&pspgu;
}

EGLBoolean eglMakeCurrent(EGLDisplay dpy, EGLSurface draw, EGLSurface read, EGLContext ctx)
{
    (void)dpy; (void)draw; (void)read; (void)ctx;

    if (!pspgu.current) pspguInitGu();

    return EGL_TRUE;
}

EGLBoolean eglSwapBuffers(EGLDisplay dpy, EGLSurface surface)
{
    (void)dpy; (void)surface;

    if (!pspgu.current) return EGL_FALSE;

#ifdef PSP_GU_SELFTEST
    // Diagnostic: a green sprite covering the screen, drawn with raw GU. If this shows and
    // raylib's geometry does not, the GU setup is fine and the data path is the problem.
    {
        typedef struct { unsigned int color; float x, y, z; } SelfTestVertex;
        static SelfTestVertex quad[2] = {
            { 0xFF00FF00, 0.0f, 0.0f, 0.0f },
            { 0xFF00FF00, 480.0f, 272.0f, 0.0f }
        };

        sceGuDisable(GU_TEXTURE_2D);
        sceGuDisable(GU_BLEND);
        sceGuDisable(GU_CULL_FACE);     // is culling what makes everything invisible?
        sceGuDisable(GU_DEPTH_TEST);
        pspguWriteback(quad, sizeof(quad));
        sceGuDrawArray(GU_SPRITES, GU_COLOR_8888 | GU_VERTEX_32BITF, 2, NULL, quad);
        pspguLog("selftest: sprite at 0,0..480,272 issued");
    }
#endif

    pspguEndFrame();

    if (pspgu.vsync) sceDisplayWaitVblankStart();
    sceGuSwapBuffers();

    return EGL_TRUE;
}

EGLBoolean eglSwapInterval(EGLDisplay dpy, EGLint interval)
{
    (void)dpy;

    // Not in the 52 symbols the library references, because PSP_DISABLE_VSYNC compiles
    // the only call out - but the platform layer still names it in that build.
    pspgu.vsync = (interval == 0)? 0 : 1;

    return EGL_TRUE;
}

EGLint eglGetError(void)
{
    return EGL_SUCCESS;
}

EGLBoolean eglTerminate(EGLDisplay dpy)
{
    (void)dpy;

    if (!pspgu.current) return EGL_TRUE;

    pspguEndFrame();
    sceGuTerm();
    pspgu.current = 0;

    return EGL_TRUE;
}
