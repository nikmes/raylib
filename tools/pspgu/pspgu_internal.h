/*
 * pspgu - a GE driver that supplies the GL 1.1 subset raylib's GL11 path needs.
 *
 * Phase 5 of the PSP raylib roadmap. The library needs exactly 52 symbols (44 gl* + 8
 * egl*); this replaces pspGL rather than adding a fifth rlgl backend, so rlgl.h, the
 * sprint 007-009 batcher and every existing test stay untouched. See
 * docs/psp/phases/05-native-gu-backend.md.
 *
 * Written over sceGu rather than the GE registers directly: GU is the SDK's own command
 * layer and already does the state shadowing pspGL hand-rolled.
 *
 * SPRINT 009/010 INVARIANTS THIS FILE MUST HONOUR
 *  - Framebuffer is 5650 (16-bit), which is what pspGL gets today: a capture of RAYWHITE
 *    comes back as 240,240,240 rather than 245,245,245. Changing the format moves every
 *    pixel of every archived capture.
 *  - Backface culling is ON from rlglInit, and the batcher emits (TL,BL,BR),(TL,BR,TR);
 *    a wrong winding makes sprites invisible, not distorted.
 *  - glClear is asked for COLOR|DEPTH every frame; with no depth buffer the DEPTH bit is
 *    a documented no-op (PSP_GU_DEPTH=0).
 */

#ifndef PSPGU_INTERNAL_H
#define PSPGU_INTERNAL_H

#include <psptypes.h>
#include <pspgu.h>
#include <pspge.h>
#include <pspdisplay.h>
#include <pspkernel.h>

// GL/gl.h first: pspGL's egl.h pulls in the GLES headers, which define the GL type
// guards and would leave GLclampd/GLdouble missing if they came first.
#include <GL/gl.h>
#include <GLES/egl.h>

#include <stdio.h>

// The driver sits below raylib, so it cannot call TraceLog: that would invert the layering
// and could recurse through the very GL calls being logged. printf reaches PSPLink's
// console the same way the application's own TRACELOG does.
#define pspguLog(...) do { printf("[PSPGU] "); printf(__VA_ARGS__); printf("\n"); fflush(stdout); } while (0)

// Bump when the driver's observable behaviour changes. Printed in the boot line so a
// stale archive on the device is visible before a measurement is believed; sprint 014
// lost runs to archives that had not rebuilt.
#define PSPGU_BUILD "b26"

// Sprint 015 T2: per-texture PSM. 8888 is the default and preserves pixel parity;
// `auto` (make PSP_GU_TEXFMT=auto) picks 16-bit formats where the source permits it.
#ifndef PSPGU_TEXFMT_AUTO
    #define PSPGU_TEXFMT_AUTO 0
#endif
#define PSPGU_TEXFMT_NAME (PSPGU_TEXFMT_AUTO? "auto" : "8888")

#ifndef PSP_GU_DEPTH
    #define PSP_GU_DEPTH 1              // 16-bit depth buffer, 272 KiB at 512 stride
#endif
#ifndef PSP_GU_WIDTH
    #define PSP_GU_WIDTH 480
#endif
#ifndef PSP_GU_HEIGHT
    #define PSP_GU_HEIGHT 272
#endif
#ifndef PSP_GU_STRIDE
    #define PSP_GU_STRIDE 512               // pixels per line; 512 is the GE's fast width
#endif
#ifndef PSP_GU_LIST_BYTES
    // 0x3FFFF is GU's maximum list size, and 256 KiB is one byte over it. 240 KiB leaves
    // room and still holds a 2,000-sprite frame with plenty to spare.
    #define PSP_GU_LIST_BYTES (240*1024)
#endif
#ifndef PSP_GU_FB_COUNT
    // Three framebuffers, not two. Two buffers can only be correct if the frame being
    // displayed is the one just submitted, which forces a full GE wait before the swap.
    // A third buffer lets the display show frame N-1 (finished) while frame N is still
    // being drawn, which is what makes the queued submission tear-free. 272 KiB more
    // VRAM, recorded in the sprint's budget.
    #define PSP_GU_FB_COUNT 3
#endif
#ifndef PSP_GU_ARENA_BYTES
    // Per-list vertex arena. One batcher flush at RLPSP_BATCH_CAP (4,096) vertices is
    // 96 KiB; 128 KiB also holds the converted path's 32-byte vertices and the text runs
    // that share the same frame. Two lists therefore cost 256 KiB of main RAM. Measured
    // worst case at the 60 fps ceiling: 1,895 sprites = 3,790 vertices = 91.0 KiB.
    #define PSP_GU_ARENA_BYTES (128*1024)
#endif

// libpspgu exposes the queue id of the last direct list it enqueued. sceGuStart()
// enqueues immediately (with the stall address at the list start), so saving this after
// sceGuStart identifies the list we are about to fill. That lets list reuse wait on the
// exact list that used the memory before, instead of a full GE wait on the current frame.
// Declared here rather than included: no pspsdk header exports it, but libpspgu.a does.
extern int ge_list_executed[2];

// One context and one window surface is all raylib ever makes
typedef struct {
    int inited;                             // eglInitialize succeeded
    int current;                            // eglMakeCurrent happened, GU is up
    int vsync;                              // eglSwapInterval value
    int width, height, stride;

    void *listMem[2];                       // rotating command lists
    int listIndex;                          // list being filled, -1 before the first
    int listOpen;                           // between sceGuStart and sceGuFinish
    int listQid[2];                         // GE queue id of each list, -1 when unused
    unsigned int listBytes;                 // capacity of one list
    unsigned int listUsedMax;               // largest list sceGuFinish reported
    unsigned long long listSubmits;
    unsigned long long listWaitUs;          // time spent waiting for a reused list
    unsigned long long listWaitMaxUs;
    unsigned long long listWaits;

    void *arenaMem[2];                      // per-list vertex memory
    unsigned int arenaUsed[2];
    unsigned int arenaBytes;                // capacity of one arena
    unsigned long long arenaOverflows;      // frames that needed the full-sync fallback

    unsigned int fbBytes;                   // one framebuffer, bytes
    void *fbOffset[PSP_GU_FB_COUNT];         // the rotating framebuffers, EDRAM offsets
    int bufQid[PSP_GU_FB_COUNT];             // GE queue id of the list that last drew each
    int frames;                             // completed frames (incremented at swap)
    int frameActive;                        // a frame has drawn since the last swap
    int drawIndex;                          // buffer the open frame draws into
    int swapDisplay;                        // buffer the next swap shows, -1 = keep
    int swapRender;                         // buffer the next frame draws into
    unsigned long long displayWaits;        // waits for the frame about to be shown
    unsigned long long displayWaitUs;
    unsigned long long displayWaitMaxUs;

    void *drawOffset;                       // current draw buffer, as GU wants it
    void *dispOffset;
    void *depthOffset;                      // NULL when PSP_GU_DEPTH is 0
    void *readOffset;                       // buffer holding the last completed frame

    unsigned int vramEnd;                   // 2 MiB
    unsigned int vramReserved;              // block bytes held by framebuffers + depth
    unsigned int vramReservedRequested;     // logical bytes of those reservations
    unsigned int vramRequested;             // logical bytes requested, all callers
    unsigned int vramAllocs;
    unsigned int vramFrees;
    unsigned int vramFailures;

    // Residency/eviction (sprint 016). The texture table is in gl_texture.c; these are
    // the counters that file updates and the report reads.
    unsigned int residencyEvictions;
    unsigned int residencyReloads;
    unsigned int residencyFailed;
    unsigned int residencyCoalesced;
    unsigned int shadowBytes;               // main-RAM texture shadows, not VRAM

    // GL state that GU needs on every draw because GU has no query for it
    unsigned int clearColor;                // 0xAABBGGRR
    float clearDepth;
    unsigned int enableBits;                // GL_* enable bits, mirrored for the draw path
    unsigned int clientBits;                // GL_*_ARRAY bits from glEnableClientState
    unsigned char cr, cg, cb, ca;           // Current colour: what GL11 uses when GL_COLOR_ARRAY is off
    int texEnabled;                         // GL_TEXTURE_2D enable state (GU has no query)
    unsigned int boundTexture;              // texture id currently bound
} PspGuContext;

extern PspGuContext pspgu;

// Framebuffer / list plumbing, used by egl.c and by every gl_*.c that needs to emit
void pspguBeginFrame(void);                 // start a list if one is not open
void pspguEndFrame(void);                   // finish the open list, no GE wait
void pspguFinishAndWait(void);              // finish, then wait for it (readback/boot only)

// VRAM: a 512-byte block allocator over the whole 2 MiB, with coalescing. Framebuffers
// and depth are fixed reservations; textures allocate and free. Offset 0 is a legal
// framebuffer, so failures use PSPGU_VRAM_INVALID and the texture wrapper maps it to 0.
#define PSPGU_VRAM_BLOCK      512u
#define PSPGU_VRAM_BLOCKS     (2u*1024u*1024u/PSPGU_VRAM_BLOCK)     // 4,096
#define PSPGU_VRAM_INVALID    0xFFFFFFFFu

typedef struct {
    unsigned int reserved;       // block bytes held by fixed reservations
    unsigned int requested;      // logical texture bytes requested
    unsigned int used;           // all used blocks * 512 (reserved + textures)
    unsigned int free;           // free blocks * 512
    unsigned int largest;        // longest free span, bytes
    unsigned int internal;       // used - reserved - requested (rounding/alignment)
    unsigned int allocs, frees, failures;
} PspGuVramStats;

unsigned int pspguVramAlloc(unsigned int bytes);                 // texture wrapper, 64-byte aligned
unsigned int pspguVramAllocAligned(unsigned int bytes, unsigned int align);  // INVALID on failure
void pspguVramFree(unsigned int offset);                          // ignores fixed reservations
void pspguVramStats(PspGuVramStats *out);
unsigned int pspguVramFreeBytes(void);      // probes use these two for the churn self-check
unsigned int pspguVramLargestBytes(void);
void pspguVramReport(const char *when);     // VRAM line + residency line

// Residency (sprint 016): the probes need the counters and per-texture residency.
typedef struct {
    unsigned int resident;
    unsigned int shadowBytes;
    unsigned int evictions;
    unsigned int reloads;
    unsigned int failed;
    unsigned int coalesced;
} PspGuResidencyStats;

void pspguResidencyStats(PspGuResidencyStats *out);
void pspguTextureReport(const char *when);  // one residency line
void pspguSetTextureLog(int on);             // probes time uploads without the per-texture log

// The bound texture's geometry, for the draw-time 512-pixel window split. Returns 0 when
// the default texture (or no texture) is bound.
int pspguBoundTextureInfo(int *width, int *height, int *padW, int *padH, int *tbw,
                          int *bpp, int *swizzled, unsigned int *offset);
void pspguWindowStats(unsigned int *draws, unsigned int *switches);

// Per-list vertex memory. Every draw copies its vertices here before handing the pointer
// to GU, so a list queued two frames ago never points at memory this frame writes.
void *pspguArenaAlloc(unsigned int bytes);  // 16-byte aligned, NULL when exhausted
int pspguArenaReset(void);                  // rare fallback: finish+wait, empty the arena

// The GE reads vertices and indices straight out of main RAM, through its own path, so
// anything the CPU wrote has to be written back first. Without this the GE reads stale
// (usually zero) vertices and every primitive is a degenerate point: invisible, not
// distorted. This is phase 5 task 9, and it turned out to be blocking rather than an
// optimisation.
static inline void pspguWriteback(const void *addr, unsigned int bytes)
{
    if ((addr == NULL) || (bytes == 0)) return;
    sceKernelDcacheWritebackRange(addr, bytes);
}

// Textures (gl_texture.c)
void pspguInitTextures(void);

// Matrices (gl_matrix.c): the draw path pushes both before it emits vertices
void pspguResetMatrices(void);
void pspguUploadMatrices(void);
void glGetFloatvMatrices(GLenum pname, GLfloat *params);
float pspguProjectionW(void);       // m[11]: 0 for ortho, -1 for a frustum

// glEnableClientState bits: one source of truth, set in gl_state.c, read in gl_draw.c
#define PSPGU_CS_VERTEX 1u
#define PSPGU_CS_TEXCOORD 2u
#define PSPGU_CS_COLOR 4u
#define PSPGU_CS_NORMAL 8u

// Set when a command was issued outside eglMakeCurrent; the first one logs
extern int pspguWarnedNoContext;

#endif
