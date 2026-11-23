/*
 * gl_fbo.c - single-colour-attachment framebuffer objects over the GE draw buffer.
 *
 * Sprint 065. The GE has no framebuffer object; its draw buffer is a VRAM address, so an
 * FBO is the saved screen state plus a redirect to the colour texture's block. A bind saves
 * draw buffer, depth, dimensions and viewport/scissor; an unbind restores them. The colour
 * texture is promoted to an 8192-byte-aligned block and pinned by the texture layer.
 */

#include <string.h>
#include <stdint.h>

#include "pspgu_internal.h"

#ifndef GL_FRAMEBUFFER
    #define GL_FRAMEBUFFER 0x8D40
#endif
#ifndef GL_COLOR_ATTACHMENT0
    #define GL_COLOR_ATTACHMENT0 0x8CE0
#endif
#ifndef GL_DEPTH_ATTACHMENT
    #define GL_DEPTH_ATTACHMENT 0x8D00
#endif
#ifndef GL_RENDERBUFFER
    #define GL_RENDERBUFFER 0x8D41
#endif
#ifndef GL_TEXTURE_2D
    #define GL_TEXTURE_2D 0x0DE1
#endif
#ifndef GL_FRAMEBUFFER_COMPLETE
    #define GL_FRAMEBUFFER_COMPLETE 0x8CD5
#endif
#ifndef GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT
    #define GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT 0x8CD7
#endif

#define PSPGU_MAX_FBO 8
#define PSPGU_MAX_RB  8

typedef struct {
    int used;
    unsigned int colorTex;
    unsigned int depthRb;
} PspGuFbo;

typedef struct {
    int used;
    unsigned int offset;
    int width, height, stride;
} PspGuRb;

void glBindFramebuffer(GLenum target, GLuint id);

static PspGuFbo fbos[PSPGU_MAX_FBO];
static PspGuRb rbs[PSPGU_MAX_RB];
static unsigned int boundFbo = 0;
static unsigned int boundRb = 0;

typedef struct {
    int valid;
    unsigned int id;
    void *draw, *depth;
    int width, height, stride, psm, dither;
    int vpX, vpY, vpW, vpH;
    int scX, scY, scW, scH;
} FboSaved;

static FboSaved saved;

static PspGuFbo *fboAt(unsigned int id)
{
    if ((id == 0) || (id > PSPGU_MAX_FBO)) return NULL;
    PspGuFbo *f = &fbos[id - 1];
    return f->used? f : NULL;
}

static PspGuRb *rbAt(unsigned int id)
{
    if ((id == 0) || (id > PSPGU_MAX_RB)) return NULL;
    PspGuRb *r = &rbs[id - 1];
    return r->used? r : NULL;
}

static int nextPow2(int v)
{
    int p = 64;
    while (p < v) p <<= 1;
    return p;
}

void glGenFramebuffers(GLsizei n, GLuint *ids)
{
    if (ids == NULL) return;
    for (int i = 0; i < n; i++)
    {
        ids[i] = 0;
        for (int f = 0; f < PSPGU_MAX_FBO; f++)
        {
            if (!fbos[f].used)
            {
                fbos[f].used = 1;
                fbos[f].colorTex = 0;
                fbos[f].depthRb = 0;
                ids[i] = (GLuint)(f + 1);
                break;
            }
        }
    }
}

void glDeleteFramebuffers(GLsizei n, const GLuint *ids)
{
    if (ids == NULL) return;
    for (int i = 0; i < n; i++)
    {
        PspGuFbo *f = fboAt(ids[i]);
        if (f == NULL) continue;
        if (boundFbo == ids[i]) glBindFramebuffer(GL_FRAMEBUFFER, 0);
        if (f->colorTex != 0) pspguTextureMarkDirty(f->colorTex);
        f->used = 0;
    }
}

void glBindFramebuffer(GLenum target, GLuint id)
{
    (void)target;

    if (id == boundFbo) return;

    if (id == 0)
    {
        if (saved.valid)
        {
            pspguBeginFrame();
            pspgu.drawOffset = saved.draw;
            pspgu.depthOffset = saved.depth;
            pspgu.width = saved.width;
            pspgu.height = saved.height;
            pspgu.stride = saved.stride;
            pspgu.drawPsm = saved.psm;
            pspgu.vpX = saved.vpX; pspgu.vpY = saved.vpY;
            pspgu.vpW = saved.vpW; pspgu.vpH = saved.vpH;
            pspgu.scX = saved.scX; pspgu.scY = saved.scY;
            pspgu.scW = saved.scW; pspgu.scH = saved.scH;

            // Offset 0 is a legal framebuffer (screen buffer 0); do not treat it as NULL.
            sceGuDrawBufferList(saved.psm? saved.psm : GU_PSM_5650, saved.draw, saved.stride);
            if (saved.depth != NULL) sceGuDepthBuffer(saved.depth, saved.stride);
            pspgu.ditherEnabled = saved.dither;
            pspguApplyDither();
            sceGuOffset(2048 - (saved.width/2), 2048 - (saved.height/2));
            sceGuViewport(saved.vpX, saved.vpY, saved.vpW, saved.vpH);
            sceGuScissor(saved.scX, saved.scY, saved.scW, saved.scH);

            if (saved.id != 0)
            {
                PspGuFbo *f = fboAt(saved.id);
                if ((f != NULL) && (f->colorTex != 0)) pspguTextureMarkDirty(f->colorTex);
            }
            saved.valid = 0;
        }
        boundFbo = 0;
        return;
    }

    PspGuFbo *f = fboAt(id);
    if (f == NULL) return;
    if (f->colorTex == 0)
    {
        // Attachment happens while the FBO is bound; allow an incomplete object.
        boundFbo = id;
        return;
    }

    unsigned int off = 0;
    int w = 0, h = 0, stride = 0, psm = 0;
    if (!pspguTextureRenderTarget(f->colorTex, &off, &w, &h, &stride, &psm)) return;

    PspGuRb *rb = rbAt(f->depthRb);

    if (!saved.valid)
    {
        saved.valid = 1;
        saved.id = id;
#if PSPGU_FB_RESTORE == 1
        // P10-S9 candidate (a): advance the frame before saving the screen target.
        // Measured: fixes the flicker but leaves a 4096-black target band (BAND_DIAG).
        pspguBeginFrame();
        saved.draw = pspgu.drawOffset;
#elif PSPGU_FB_RESTORE == 2
        // P10-S9 candidate (b): save this frame's rotating buffer directly, without
        // starting a list before the save. The frame index is the current one.
        saved.draw = pspgu.fbOffset[pspgu.frames % PSP_GU_FB_COUNT];
#else
        saved.draw = pspgu.drawOffset;
#endif
        saved.depth = pspgu.depthOffset;
        saved.width = pspgu.width;
        saved.height = pspgu.height;
        saved.stride = pspgu.stride;
        saved.psm = pspgu.drawPsm;
        saved.dither = pspgu.ditherEnabled;
        saved.vpX = pspgu.vpX; saved.vpY = pspgu.vpY;
        saved.vpW = pspgu.vpW; saved.vpH = pspgu.vpH;
        saved.scX = pspgu.scX; saved.scY = pspgu.scY;
        saved.scW = pspgu.scW; saved.scH = pspgu.scH;
    }

    pspguBeginFrame();

    pspgu.drawOffset = (void *)(uintptr_t)off;
    pspgu.width = w;
    pspgu.height = h;
    pspgu.stride = stride;
    pspgu.drawPsm = psm;
    if (rb != NULL) pspgu.depthOffset = (void *)(uintptr_t)rb->offset;

    sceGuDrawBufferList(psm, pspgu.drawOffset, stride);
    sceGuDisable(GU_DITHER);                // dither once, on the screen; not in the target
    if (pspgu.depthOffset != NULL) sceGuDepthBuffer(pspgu.depthOffset, stride);
    sceGuOffset(2048 - (w/2), 2048 - (h/2));
    pspgu.vpX = 2048; pspgu.vpY = 2048; pspgu.vpW = w; pspgu.vpH = h;
    pspgu.scX = 0; pspgu.scY = 0; pspgu.scW = w; pspgu.scH = h;
    sceGuViewport(pspgu.vpX, pspgu.vpY, w, h);
    sceGuScissor(0, 0, w, h);

    boundFbo = id;
}

void glFramebufferTexture2D(GLenum target, GLenum attachment, GLenum textarget,
                            GLuint texture, GLint level)
{
    (void)target; (void)level;

    if ((attachment != GL_COLOR_ATTACHMENT0) || (textarget != GL_TEXTURE_2D) || (boundFbo == 0))
    {
        static int logged = 0;
        if (!logged) { pspguLog("glFramebufferTexture2D: only COLOR_ATTACHMENT0 + TEXTURE_2D"); logged = 1; }
        return;
    }

    PspGuFbo *f = fboAt(boundFbo);
    if (f == NULL) return;

    unsigned int off; int w, h, stride, psm;
    if (!pspguTextureRenderTarget(texture, &off, &w, &h, &stride, &psm)) return;

    f->colorTex = texture;
}

void glFramebufferRenderbuffer(GLenum target, GLenum attachment, GLenum rbtarget, GLuint rb)
{
    (void)target; (void)rbtarget;

    if ((attachment != GL_DEPTH_ATTACHMENT) || (boundFbo == 0)) return;

    PspGuFbo *f = fboAt(boundFbo);
    if (f == NULL) return;
    if (rbAt(rb) == NULL) return;
    f->depthRb = rb;
}

GLenum glCheckFramebufferStatus(GLenum target)
{
    (void)target;

    PspGuFbo *f = fboAt(boundFbo);
    if ((f == NULL) || (f->colorTex == 0)) return GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT;
    return GL_FRAMEBUFFER_COMPLETE;
}

void glGenRenderbuffers(GLsizei n, GLuint *ids)
{
    if (ids == NULL) return;
    for (int i = 0; i < n; i++)
    {
        ids[i] = 0;
        for (int r = 0; r < PSPGU_MAX_RB; r++)
        {
            if (!rbs[r].used)
            {
                rbs[r].used = 1;
                rbs[r].offset = 0;
                ids[i] = (GLuint)(r + 1);
                break;
            }
        }
    }
}

void glDeleteRenderbuffers(GLsizei n, const GLuint *ids)
{
    if (ids == NULL) return;
    for (int i = 0; i < n; i++)
    {
        PspGuRb *r = rbAt(ids[i]);
        if (r == NULL) continue;
        for (int f = 0; f < PSPGU_MAX_FBO; f++)
            if (fbos[f].used && fbos[f].depthRb == ids[i]) fbos[f].depthRb = 0;
        if (r->offset != 0) pspguVramFree(r->offset);
        r->used = 0;
    }
}

void glBindRenderbuffer(GLenum target, GLuint id)
{
    (void)target;
    boundRb = id;
}

void glRenderbufferStorage(GLenum target, GLenum format, GLsizei width, GLsizei height)
{
    (void)target; (void)format;

    PspGuRb *r = rbAt(boundRb);
    if (r == NULL) return;

    if (width < 64)
    {
        static int logged = 0;
        if (!logged) { pspguLog("renderbuffer: width %d < 64 is not a legal GE stride", width); logged = 1; }
        return;
    }

    int padW = nextPow2(width);
    int padH = nextPow2(height);
    // The GE's frame buffer height stays the display's 272 rows; a shorter target depth
    // block would let the clear/draw overflow into the colour block (8 rows at 256x256).
    int allocH = (padH < PSP_GU_HEIGHT)? PSP_GU_HEIGHT : padH;
    unsigned int bytes = (unsigned int)padW*(unsigned int)allocH*2;  // GE depth is 16-bit

    if (r->offset != 0) { pspguVramFree(r->offset); r->offset = 0; }
    unsigned int off = pspguVramAllocAligned(bytes, 8192);

    if (off == PSPGU_VRAM_INVALID)
    {
        pspguLog("renderbuffer: no 8192-aligned %u-byte depth block", bytes);
        return;
    }

    r->offset = off;
    r->width = width;
    r->height = height;
    r->stride = padW;
}
