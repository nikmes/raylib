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

static unsigned long long pspguNowUs(void)
{
    return (unsigned long long)sceKernelGetSystemTimeWide();
}

// sceGuSwapBuffers normally just exchanges two pointers. With three buffers we choose
// which completed frame to show and which free buffer the next frame draws into. GU then
// sends the new draw pointer on the next sceGuStart, so the list carries it.
static void pspguSwapCallback(void **display, void **render)
{
    if (pspgu.swapDisplay >= 0) *display = pspgu.fbOffset[pspgu.swapDisplay];
    *render = pspgu.fbOffset[pspgu.swapRender];
}

//------------------------------------------------------------------------------------
// VRAM: 512-byte block allocator (sprint 015 T3)
//------------------------------------------------------------------------------------
// The whole 2 MiB EDRAM is 4,096 blocks. Each span starts with a header; the table is
// scanned by jumping span to span. 4 bytes per block = 16 KiB of main RAM, cache-resident.
// Framebuffers and depth are fixed reservations and are never freed; textures allocate
// and free, and adjacent free spans coalesce. The bump allocator this replaces could not
// reclaim a deleted texture and could not report a largest block.
typedef struct {
    unsigned short blocks;      // span length in 512-byte blocks
    unsigned char free;         // 1 = free
    unsigned char fixed;        // 1 = framebuffer/depth reservation, never freed
    unsigned int requested;     // logical bytes requested, live only (0 when free)
} VramSpan;                     // 8 bytes x 4,096 = 32 KiB in main RAM

static VramSpan vramMap[PSPGU_VRAM_BLOCKS];
static unsigned int vramUsedBlocks;

static void vramInit(void)
{
    memset(vramMap, 0, sizeof(vramMap));
    vramMap[0].blocks = PSPGU_VRAM_BLOCKS;
    vramMap[0].free = 1;
    vramUsedBlocks = 0;

    pspgu.vramEnd = 2*1024*1024;
    pspgu.vramReserved = 0;
    pspgu.vramReservedRequested = 0;
    pspgu.vramRequested = 0;
    pspgu.vramAllocs = 0;
    pspgu.vramFrees = 0;
    pspgu.vramFailures = 0;
}

static unsigned int vramLargestFree(void)
{
    unsigned int i = 0, best = 0;

    while (i < PSPGU_VRAM_BLOCKS)
    {
        unsigned int n = vramMap[i].blocks;
        if (n == 0) break;
        if (vramMap[i].free && (n > best)) best = n;
        i += n;
    }

    return best*PSPGU_VRAM_BLOCK;
}

// bytes, alignment, fixed reservation. Returns an EDRAM offset or PSPGU_VRAM_INVALID.
static unsigned int vramAllocRaw(unsigned int bytes, unsigned int align, int fixed)
{
    unsigned int need = (bytes + PSPGU_VRAM_BLOCK - 1)/PSPGU_VRAM_BLOCK;

    if ((bytes == 0) || (need > PSPGU_VRAM_BLOCKS)) { pspgu.vramFailures++; return PSPGU_VRAM_INVALID; }
    if (align < PSPGU_VRAM_BLOCK) align = PSPGU_VRAM_BLOCK;

    int best = -1;
    unsigned int bestBlocks = PSPGU_VRAM_BLOCKS + 1, bestSkip = 0;
    unsigned int i = 0;

    // best fit, with the alignment prefix counted in the span
    while (i < PSPGU_VRAM_BLOCKS)
    {
        unsigned int n = vramMap[i].blocks;
        if (n == 0) break;

        if (vramMap[i].free)
        {
            unsigned int startByte = i*PSPGU_VRAM_BLOCK;
            unsigned int aligned = (startByte + align - 1) & ~(align - 1);
            unsigned int skip = (aligned - startByte)/PSPGU_VRAM_BLOCK;

            if ((skip + need <= n) && (n < bestBlocks))
            {
                best = (int)i;
                bestBlocks = n;
                bestSkip = skip;
            }
        }
        i += n;
    }

    if (best < 0) { pspgu.vramFailures++; return PSPGU_VRAM_INVALID; }

    unsigned int at = (unsigned int)best;
    unsigned int old = vramMap[at].blocks;

    if (bestSkip > 0)
    {
        vramMap[at].blocks = (unsigned short)bestSkip;
        vramMap[at].free = 1;
        vramMap[at].fixed = 0;
        vramMap[at].requested = 0;
        at += bestSkip;
    }

    vramMap[at].blocks = (unsigned short)need;
    vramMap[at].free = 0;
    vramMap[at].fixed = (unsigned char)fixed;
    vramMap[at].requested = bytes;

    unsigned int tail = old - bestSkip - need;
    if (tail > 0)
    {
        vramMap[at + need].blocks = (unsigned short)tail;
        vramMap[at + need].free = 1;
        vramMap[at + need].fixed = 0;
        vramMap[at + need].requested = 0;
    }

    vramUsedBlocks += need;
    pspgu.vramAllocs++;
    pspgu.vramRequested += bytes;

    return at*PSPGU_VRAM_BLOCK;
}

static void vramFreeRaw(unsigned int offset)
{
    if ((offset == PSPGU_VRAM_INVALID) || ((offset % PSPGU_VRAM_BLOCK) != 0)) return;
    if (offset >= pspgu.vramEnd) return;

    unsigned int at = offset/PSPGU_VRAM_BLOCK;
    VramSpan *s = &vramMap[at];

    if (s->blocks == 0) { pspguLog("VRAM free: %08x is not a span", offset); return; }
    if (s->free) return;                        // double free is a no-op
    if (s->fixed) { pspguLog("VRAM free: %08x is a fixed reservation", offset); return; }

    unsigned int freed = s->blocks;

    vramUsedBlocks -= freed;
    if (pspgu.vramRequested >= s->requested) pspgu.vramRequested -= s->requested;
    else pspgu.vramRequested = 0;
    s->requested = 0;
    s->free = 1;

    // merge the next free span
    unsigned int next = at + s->blocks;
    if ((next < PSPGU_VRAM_BLOCKS) && vramMap[next].free)
    {
        s->blocks = (unsigned short)(s->blocks + vramMap[next].blocks);
        vramMap[next].blocks = 0;
    }

    // merge the previous free span: find the header whose span ends at `at`
    unsigned int prev = 0, guard = 0;
    while ((prev < at) && ((prev + vramMap[prev].blocks) < at))
    {
        if ((vramMap[prev].blocks == 0) || (++guard > PSPGU_VRAM_BLOCKS))
        {
            pspguLog("VRAM: span table corrupt while freeing %08x", offset);
            return;
        }
        prev += vramMap[prev].blocks;
    }
    if (((prev + vramMap[prev].blocks) == at) && vramMap[prev].free)
    {
        vramMap[prev].blocks = (unsigned short)(vramMap[prev].blocks + s->blocks);
        s->blocks = 0;
    }

    pspgu.vramFrees++;
}

unsigned int pspguVramAllocAligned(unsigned int bytes, unsigned int align)
{
    return vramAllocRaw(bytes, align, 0);
}

// Texture wrapper: textures never occupy offset 0, so 0 keeps meaning "failed" at the
// existing call sites. Framebuffer reservations use vramAllocRaw directly and check for
// PSPGU_VRAM_INVALID, because offset 0 is a legal framebuffer.
unsigned int pspguVramAlloc(unsigned int bytes)
{
    unsigned int off = vramAllocRaw(bytes, 64, 0);
    return (off == PSPGU_VRAM_INVALID)? 0 : off;
}

void pspguVramFree(unsigned int offset)
{
    vramFreeRaw(offset);
}

void pspguVramStats(PspGuVramStats *out)
{
    if (out == NULL) return;

    out->reserved = pspgu.vramReserved;
    out->requested = (pspgu.vramRequested >= pspgu.vramReservedRequested)?
                     (pspgu.vramRequested - pspgu.vramReservedRequested) : 0;
    out->used = vramUsedBlocks*PSPGU_VRAM_BLOCK;
    out->free = (PSPGU_VRAM_BLOCKS - vramUsedBlocks)*PSPGU_VRAM_BLOCK;
    out->largest = vramLargestFree();
    out->internal = (out->used > out->reserved + out->requested)?
                    (out->used - out->reserved - out->requested) : 0;
    out->allocs = pspgu.vramAllocs;
    out->frees = pspgu.vramFrees;
    out->failures = pspgu.vramFailures;
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
    pspgu.listBytes = alignedListBytes(PSP_GU_LIST_BYTES);
    pspgu.arenaBytes = PSP_GU_ARENA_BYTES;

    // Two of each. Frame N fills one list and one arena; frame N+1 the other pair. A list
    // is only rewritten after the GE has finished the frame that last used it (see
    // pspguBeginFrame), so its vertex memory is never overwritten under the hardware.
    for (int i = 0; i < 2; i++)
    {
        pspgu.listMem[i] = memalign(64, pspgu.listBytes);
        pspgu.listQid[i] = -1;
        pspgu.arenaMem[i] = memalign(64, pspgu.arenaBytes);
        pspgu.arenaUsed[i] = 0;
    }
    pspgu.listIndex = -1;

    // VRAM map: three framebuffers, optional depth, then textures. Three buffers are what
    // makes a queued submission tear-free: the display can show frame N-1 while frame N
    // is still being drawn, so no steady frame has to wait for the GE before swapping.
    // The reservations go through the same allocator as textures; they are 8 KiB aligned
    // (GU requires it) and marked fixed, so a texture can never free them. Offset 0 is a
    // legal framebuffer, which is why failures use PSPGU_VRAM_INVALID.
    vramInit();

    for (int i = 0; i < PSP_GU_FB_COUNT; i++)
    {
        unsigned int off = vramAllocRaw(fb, 8192, 1);

        if (off == PSPGU_VRAM_INVALID) pspguLog("VRAM: framebuffer %d reservation FAILED", i);
        pspgu.fbOffset[i] = EDRAM(off);
        pspgu.bufQid[i] = -1;
    }

#if PSP_GU_DEPTH
    {
        unsigned int off = vramAllocRaw(fb, 8192, 1);
        if (off == PSPGU_VRAM_INVALID) pspguLog("VRAM: depth reservation FAILED");
        pspgu.depthOffset = EDRAM(off);
    }
#else
    pspgu.depthOffset = NULL;
#endif

    // Snapshot the fixed reservations before any texture allocates. `reserved` is block
    // bytes; `reservedRequested` is the same total in logical bytes (they are equal here
    // because every buffer is a whole number of blocks).
    pspgu.vramReserved = vramUsedBlocks*PSPGU_VRAM_BLOCK;
    pspgu.vramReservedRequested = pspgu.vramRequested;
    pspgu.vramEnd = 2*1024*1024;        // the whole of EDRAM
    pspgu.readOffset = pspgu.fbOffset[PSP_GU_FB_COUNT - 1];
    pspgu.drawOffset = pspgu.fbOffset[0];
    pspgu.dispOffset = pspgu.fbOffset[PSP_GU_FB_COUNT - 1];

    sceGuInit();

    // Switch the display at the vblank the swap already waited for, not one frame later.
    // NEXTFRAME sounds safer but schedules the switch a full vblank after the call, so a
    // three-buffer rotation shows buffer (f-1) two frames later - which is exactly the
    // buffer frame f+2 draws into. Measured with sceDisplayGetFrameBuf: the display and
    // the draw buffer became the same address. NEXTHSYNC makes the completed frame visible
    // for the very next frame; the wait at swap guarantees it is complete.
    guSwapBuffersBehaviour(PSP_DISPLAY_SETBUF_NEXTHSYNC);
    guSwapBuffersCallback(pspguSwapCallback);

    pspgu.listIndex = 0;
    int bootList = sceGuStart(GU_DIRECT, pspgu.listMem[0]);
    pspgu.listQid[0] = (bootList >= 0)? ge_list_executed[0] : -1;
    pspgu.listOpen = 1;

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

    // Dithering: pspGL's captures show the background alternating (240,240,240) and
    // (248,248,248) pixel by pixel, i.e. a 5650 buffer being dithered to hide banding.
    // Ours was flat, which is 50 % of the frame's pixels differing for no visible reason.
    // daedalus' memory map (AI_MEMORY_MAP §12.4, "Fast clear + dither") is the reference.
    sceGuEnable(GU_DITHER);

#if PSP_GU_DEPTH
    sceGuDepthBuffer(pspgu.depthOffset, pspgu.stride);
    sceGuDepthRange(65535, 0);
#endif

    // Clear every buffer once, before sceGuDispBuffer sets the height. After that,
    // sceGuDrawBuffer's convenience auto-assignment would invent a depth buffer in the
    // PSP_GU_DEPTH=0 build, so this is the only safe point to move the draw pointer.
    {
        unsigned int clearBits = GU_COLOR_BUFFER_BIT;
#if PSP_GU_DEPTH
        clearBits |= GU_DEPTH_BUFFER_BIT;
#endif
        sceGuClearColor(0xFF000000);

        for (int i = 0; i < PSP_GU_FB_COUNT; i++)
        {
            sceGuDrawBuffer(GU_PSM_5650, pspgu.fbOffset[i], pspgu.stride);
            sceGuClear(clearBits);
        }

        sceGuDrawBuffer(GU_PSM_5650, pspgu.fbOffset[0], pspgu.stride);
        sceGuDispBuffer(PSP_GU_WIDTH, PSP_GU_HEIGHT,
                        pspgu.fbOffset[PSP_GU_FB_COUNT - 1], pspgu.stride);
    }

    {
        unsigned int used = sceGuFinish();      // bytes, as sceGuFinish reports them
        pspgu.listUsedMax = used;
        if (pspgu.listQid[0] >= 0) sceGeListSync(pspgu.listQid[0], PSP_GE_LIST_DONE);
        pspgu.listOpen = 0;
    }

    sceDisplayWaitVblankStart();
    sceGuDisplay(GU_TRUE);

    // Show a cleared buffer, never buffer 0 while frame 0 is drawing into it.
    sceDisplaySetFrameBuf((void *)((unsigned int)sceGeEdramGetAddr() +
                          (unsigned int)pspgu.fbOffset[PSP_GU_FB_COUNT - 1]),
                          pspgu.stride, GU_PSM_5650, PSP_DISPLAY_SETBUF_NEXTHSYNC);

    pspguInitTextures();
    pspguResetMatrices();
    pspgu.cr = pspgu.cg = pspgu.cb = pspgu.ca = 255;   // opaque white, GL's initial colour
    pspgu.clearColor = 0xFF000000;      // opaque black, what rlglInit clears to
    pspgu.clearDepth = 1.0f;
    pspgu.vsync = 1;
    pspgu.current = 1;

    pspguLog("GU up: pspgu %s, %dx%d stride %d, fb %u KiB x%d, depth %s (%u KiB), lists 2 x %u KiB, arenas 2 x %u KiB",
                 PSPGU_BUILD, pspgu.width, pspgu.height, pspgu.stride,
                 pspgu.fbBytes/1024, PSP_GU_FB_COUNT,
                 (pspgu.depthOffset != NULL)? "yes":"no",
                 (pspgu.depthOffset != NULL)? (pspgu.fbBytes/1024) : 0,
                 pspgu.listBytes/1024, pspgu.arenaBytes/1024);
    pspguVramReport("boot");
}

// Start a list if none is open. Every emitter calls this; it is free when already open.
//
// Rotation: before a list is reused, wait for the GE to finish the frame that last used
// it. That is two frames old, so the wait is normally already satisfied, but it is a real
// guarantee that this frame cannot overwrite a list or arena the hardware is still
// reading. It is a targeted wait, not the old full sync on the frame just submitted.
void pspguBeginFrame(void)
{
    if (!pspgu.current || pspgu.listOpen) return;

    // One framebuffer per display frame. EndFrame/readback may close and reopen lists
    // inside the same frame, so this is keyed on the frame, not on list submissions.
    if (!pspgu.frameActive)
    {
        pspgu.drawIndex = pspgu.frames % PSP_GU_FB_COUNT;
        pspgu.drawOffset = pspgu.fbOffset[pspgu.drawIndex];
        pspgu.frameActive = 1;
    }

    int next = (pspgu.listIndex == 0)? 1 : 0;

    if (pspgu.listQid[next] >= 0)
    {
        unsigned long long start = pspguNowUs();
        sceGeListSync(pspgu.listQid[next], PSP_GE_LIST_DONE);
        unsigned long long spent = pspguNowUs() - start;

        pspgu.listWaits++;
        pspgu.listWaitUs += spent;
        if (spent > pspgu.listWaitMaxUs) pspgu.listWaitMaxUs = spent;
    }

    pspgu.listIndex = next;
    pspgu.arenaUsed[next] = 0;

    int res = sceGuStart(GU_DIRECT, pspgu.listMem[next]);
    pspgu.listQid[next] = (res >= 0)? ge_list_executed[0] : -1;
    pspgu.listOpen = 1;
}

// Finish the open list and let the GE work through it. No wait: the display switch is
// deferred to the next frame (guSwapBuffersBehaviour), which gives the GE a period.
void pspguEndFrame(void)
{
    if (!pspgu.current || !pspgu.listOpen) return;

    unsigned int used = sceGuFinish();

    pspgu.listOpen = 0;
    pspgu.listSubmits++;
    pspgu.bufQid[pspgu.drawIndex] = pspgu.listQid[pspgu.listIndex];
    if (used > pspgu.listUsedMax) pspgu.listUsedMax = used;

    if (used >= pspgu.listBytes)
    {
        // GU has no capacity check of its own and a list that runs past its buffer
        // corrupts memory; this is the assertion the sprint asks for.
        pspguLog("LIST OVERFLOW: %u bytes used of %u", used, pspgu.listBytes);
    }

    // One steady-state line, early enough not to disturb the percentiles: the point of
    // T5 is a targeted wait on the list two frames old, and these counters are how that
    // claim is checked rather than felt.
    if (pspgu.listSubmits == 50)
    {
        unsigned long long avg = (pspgu.listWaits > 0)? pspgu.listWaitUs/pspgu.listWaits : 0;
        unsigned long long davg = (pspgu.displayWaits > 0)?
                                  pspgu.displayWaitUs/pspgu.displayWaits : 0;
        pspguLog("lists: submits=%llu waits=%llu avg_wait=%llu us max_wait=%llu us | "
                 "display waits=%llu avg=%llu us max=%llu us | list_used_max=%u "
                 "arena_bytes=%u arena_overflows=%llu",
                 pspgu.listSubmits, pspgu.listWaits, avg, pspgu.listWaitMaxUs,
                 pspgu.displayWaits, davg, pspgu.displayWaitMaxUs,
                 pspgu.listUsedMax, pspgu.arenaBytes, pspgu.arenaOverflows);
    }
}

// Finish and wait for the list just submitted. Used at boot and by readback; a steady
// frame must not call this.
void pspguFinishAndWait(void)
{
    if (!pspgu.current || !pspgu.listOpen) return;

    int qid = pspgu.listQid[pspgu.listIndex];

    pspguEndFrame();
    if (qid >= 0) sceGeListSync(qid, PSP_GE_LIST_DONE);
}

unsigned int pspguVramFreeBytes(void)
{
    return (PSPGU_VRAM_BLOCKS - vramUsedBlocks)*PSPGU_VRAM_BLOCK;
}

unsigned int pspguVramLargestBytes(void)
{
    return vramLargestFree();
}

// One line from the allocator's own table. `internal` is real: used - reserved - requested
// is block rounding and alignment. The display lists and vertex arenas are main RAM and
// are not part of the 2 MiB sum.
void pspguVramReport(const char *when)
{
    PspGuVramStats s;

    pspguVramStats(&s);
    pspguLog("VRAM [%s]: reserved=%u requested=%u used=%u free=%u largest=%u internal=%u allocs=%u frees=%u failures=%u (bytes)",
             when, s.reserved, s.requested, s.used, s.free, s.largest, s.internal,
             s.allocs, s.frees, s.failures);
}

// Per-list vertex memory. The pointer handed to sceGuDrawArray must stay valid until the
// GE has finished that list; with two lists in flight, one arena per list is what makes
// that true without a full wait.
void *pspguArenaAlloc(unsigned int bytes)
{
    int i = pspgu.listIndex;

    if ((i < 0) || !pspgu.listOpen) return NULL;

    // 64 bytes, not 16: the PSP D-cache line is 64 bytes and the GE reads this memory
    // after sceKernelDcacheWritebackRange. An allocation that starts mid-line shares a
    // dirty line with the previous draw; one polyline in basic_shapes showed a one-pixel
    // edge shift that way. Line-aligned starts make every writeback cover its own lines.
    unsigned int off = (pspgu.arenaUsed[i] + 63) & ~63u;

    if ((off + bytes) > pspgu.arenaBytes) return NULL;

    pspgu.arenaUsed[i] = off + bytes;

    return (unsigned char *)pspgu.arenaMem[i] + off;
}

// A frame larger than the arena cannot be rotated without overwriting vertices the GE
// may still read. The safe fallback is a full wait on the list just finished, then a
// fresh list on the same memory. It is counted, and it should stay zero for the measured
// configuration: the 60 fps ceiling uses 91 KiB of the 128 KiB arena.
int pspguArenaReset(void)
{
    int i = pspgu.listIndex;

    if (!pspgu.current || (i < 0) || !pspgu.listOpen) return 0;

    int qid = pspgu.listQid[i];

    pspguEndFrame();
    if (qid >= 0) sceGeListSync(qid, PSP_GE_LIST_DONE);

    pspgu.arenaUsed[i] = 0;

    int res = sceGuStart(GU_DIRECT, pspgu.listMem[i]);
    pspgu.listQid[i] = (res >= 0)? ge_list_executed[0] : -1;
    pspgu.listOpen = 1;
    pspgu.arenaOverflows++;

    pspguLog("vertex arena exhausted: full sync fallback (%llu total)", pspgu.arenaOverflows);

    return 1;
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

    // One line, once, after the application's textures are loaded: the boot line cannot
    // know them yet. It is printed before the first swap so the frame times that follow
    // are not measured with the log in them.
    static int vramAssetsReported = 0;
    if (!vramAssetsReported) { pspguVramReport("assets"); vramAssetsReported = 1; }

    int finished = pspgu.frames;

    if (finished >= 1)
    {
        // Show frame finished-1, the one that had this whole frame to complete. This is
        // the only wait on the display path, and it targets a finished frame, not the one
        // just submitted.
        int show = (finished - 1) % PSP_GU_FB_COUNT;

        if (pspgu.bufQid[show] >= 0)
        {
            unsigned long long start = pspguNowUs();
            sceGeListSync(pspgu.bufQid[show], PSP_GE_LIST_DONE);
            unsigned long long spent = pspguNowUs() - start;

            pspgu.displayWaits++;
            pspgu.displayWaitUs += spent;
            if (spent > pspgu.displayWaitMaxUs) pspgu.displayWaitMaxUs = spent;
        }

        pspgu.swapDisplay = show;
        pspgu.readOffset = pspgu.fbOffset[show];
    }
    else
    {
        // Nothing has completed yet: show a cleared buffer, never the one being drawn.
        pspgu.swapDisplay = PSP_GU_FB_COUNT - 1;
        pspgu.readOffset = pspgu.fbOffset[pspgu.swapDisplay];
    }

    pspgu.swapRender = (finished + 1) % PSP_GU_FB_COUNT;
    pspgu.frames++;
    pspgu.frameActive = 0;

    if (pspgu.vsync) sceDisplayWaitVblankStart();

    sceGuSwapBuffers();     // callback picks swapDisplay/swapRender

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

    pspguFinishAndWait();
    sceGuTerm();
    pspgu.current = 0;

    return EGL_TRUE;
}
