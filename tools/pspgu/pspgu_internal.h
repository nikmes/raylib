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

// One context and one window surface is all raylib ever makes
typedef struct {
    int inited;                             // eglInitialize succeeded
    int current;                            // eglMakeCurrent happened, GU is up
    int vsync;                              // eglSwapInterval value
    int width, height, stride;

    void *list;                             // the command list being filled
    void *listBase;
    int listOpen;                           // between sceGuStart and sceGuFinish

    unsigned int fbBytes;                   // one framebuffer, bytes
    void *drawOffset;                       // offsets inside EDRAM, as GU wants them
    void *dispOffset;
    void *depthOffset;                      // NULL when PSP_GU_DEPTH is 0

    unsigned int vramNext;                  // bump allocator for textures, after the buffers
    unsigned int vramEnd;

    // GL state that GU needs on every draw because GU has no query for it
    unsigned int clearColor;                // 0xAABBGGRR
    float clearDepth;
    unsigned int enableBits;                // GL_* enable bits, mirrored for the draw path
    unsigned int clientBits;                // GL_*_ARRAY bits from glEnableClientState
} PspGuContext;

extern PspGuContext pspgu;

// Framebuffer / list plumbing, used by egl.c and by every gl_*.c that needs to emit
void pspguBeginFrame(void);                 // start a list if one is not open
void pspguEndFrame(void);                   // finish + sync what is queued
unsigned int pspguVramAlloc(unsigned int bytes);    // bump allocator, returns EDRAM offset

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

// glEnableClientState bits: one source of truth, set in gl_state.c, read in gl_draw.c
#define PSPGU_CS_VERTEX 1u
#define PSPGU_CS_TEXCOORD 2u
#define PSPGU_CS_COLOR 4u
#define PSPGU_CS_NORMAL 8u

// Set when a command was issued outside eglMakeCurrent; the first one logs
extern int pspguWarnedNoContext;

#endif
