/*
 * gl_texture.c - texture objects on the GE.
 *
 * Task 6 of phase 5. Three things decide the shape of this file:
 *
 *  - The GE has no LUMINANCE or LUMINANCE_ALPHA texture, and raylib's GL11 path hands over
 *    both: every PNG that decodes to grey+alpha (our sprite sheet and the default font atlas)
 *    arrives as GL_LUMINANCE_ALPHA. Those are decoded to RGBA and packed into the texture's
 *    chosen PSM; the default 8888 keeps captures comparable to pspGL.
 *  - Sprint 015: per-texture PSM (default 8888; `PSP_GU_TEXFMT=auto` is the measured 5650/
 *    4444 build) and a direct per-block upload. One swizzle block is 16 bytes x 8 rows; a
 *    128-byte stack tile is filled and written through the uncached EDRAM alias, so the
 *    upload allocates no full-image heap scratch.
 */

#include <string.h>
#include <stdlib.h>

#include "pspgu_internal.h"

#define PSPGU_MAX_TEXTURES 256

typedef struct {
    int used;
    unsigned int offset;        // EDRAM offset of the texels (0 when non-resident)
    unsigned int requestedBytes; // logical allocation size, for reuse and freeing
    int resident;               // 1 = offset holds valid texels
    unsigned int lastUsed;      // pspgu.frames at the last bind/use
    float priority;             // 0..1, higher is kept (glPrioritizeTextures)
    unsigned char *shadow;      // packed linear texels in main RAM, for reload
    unsigned int shadowBytes;
    int width, height;          // logical size the application asked for
    int tbw;                    // texture buffer width: power of two >= width, as GU wants
    int padW, padH;             // allocated size: powers of two >= width/height
    float scaleU, scaleV;       // logical/allocated, for sceGuTexScale
    int swizzled;               // 1 when the texels are in the swizzled layout
    int renderTarget;           // attached to an FBO: no shadow, never evicted
    int pinned;                 // held by an FBO
    int dirty;                  // the GE wrote it; TexFlush+TexSync before sampling
    int warnedNonSwizzle;       // the dimensions were complained about once
    unsigned int guFormat;      // GU_PSM_*
    int filter;                 // GU_NEAREST / GU_LINEAR
    int wrap;                   // GU_REPEAT / GU_CLAMP for both axes
} PspGuTexture;

static PspGuTexture textures[PSPGU_MAX_TEXTURES];
static unsigned int currentTexture = 0;
static unsigned int defaultTexture = 0;
static int textureLogEnabled = 1;       // probes turn the per-texture line off while timing

// T3: the standard 16-byte-block / 8-row swizzle, the same one PSPRuntime's SwizzleFast
// and the SDK samples use. Swizzling changes no texel, only the order, so a correct
// implementation must leave every capture identical - that is exactly how it is verified.
// PSP_GU_NO_SWIZZLE=1 keeps the linear layout for the A/B.
#ifndef PSP_GU_NO_SWIZZLE
    #define PSPGU_SWIZZLE 1
#else
    #define PSPGU_SWIZZLE 0
#endif

// Where texel (x,y) lives in the swizzled layout - needed for sub-image updates. A
// swizzle block is 16 bytes x 8 rows: 4 texels wide at 8888, 8 at 16-bit.
static unsigned int swizzledOffset(int x, int y, int tbw, int bpp)
{
    unsigned int widthBytes = (unsigned int)tbw*(unsigned int)bpp;
    unsigned int texelsPerBlock = 16u/(unsigned int)bpp;
    unsigned int block = (unsigned int)((y/8)*(widthBytes/16) + (x/(int)texelsPerBlock))*128;

    return block + (unsigned int)((y%8)*16) +
           (unsigned int)((x%(int)texelsPerBlock)*bpp);
}

static int nextPow2(int v)
{
    int p = 1;
    while (p < v) p <<= 1;
    return p;
}

static PspGuTexture *lookup(unsigned int id)
{
    if ((id == 0) || (id >= PSPGU_MAX_TEXTURES)) return NULL;
    return (textures[id].used)? &textures[id] : NULL;
}

// A 2-byte source is EITHER a packed 16-bit colour (RGB565/RGBA5551/RGBA4444) OR two
// 8-bit channels (LUMINANCE_ALPHA). Confusing the two decodes grey+alpha as RGB565, which
// is what turned our sprite sheet and font atlas into nonsense colours - the last 32.6 %
// of the frame. Branch on the type, never on the byte count.
static int isPacked16(GLenum type)
{
    return (type == GL_UNSIGNED_SHORT_5_6_5) ||
           (type == GL_UNSIGNED_SHORT_5_5_5_1) ||
           (type == GL_UNSIGNED_SHORT_4_4_4_4);
}

static int sourceBytesPerPixel(GLenum format, GLenum type)
{
    if (type == GL_UNSIGNED_SHORT_5_6_5)    return 2;
    if (type == GL_UNSIGNED_SHORT_5_5_5_1)  return 2;
    if (type == GL_UNSIGNED_SHORT_4_4_4_4)  return 2;

    switch (format)
    {
        case GL_RGBA:            return 4;
        case GL_RGB:             return 3;
        case GL_LUMINANCE_ALPHA: return 2;
        case GL_LUMINANCE:       return 1;
        default:                 return 4;
    }
}

static int bytesPerTexel(unsigned int guFormat)
{
    return (guFormat == GU_PSM_8888)? 4 : 2;
}

// Decode one source pixel to 8-bit RGBA. Every upload path goes through this, so a
// packed 16-bit source is decoded by its TYPE: 565, 5551 and 4444 are different layouts,
// and the old code decoded all three as 565.
static void readSourceRGBA(const unsigned char *p, GLenum format, GLenum type,
                           int *r, int *g, int *b, int *a)
{
    if (isPacked16(type))
    {
        unsigned short v = *(const unsigned short *)p;

        switch (type)
        {
            case GL_UNSIGNED_SHORT_5_6_5:
                *r = (v & 0x1F) << 3; *g = ((v >> 5) & 0x3F) << 2;
                *b = ((v >> 11) & 0x1F) << 3; *a = 255; break;
            case GL_UNSIGNED_SHORT_5_5_5_1:
                *r = (v & 0x1F) << 3; *g = ((v >> 5) & 0x1F) << 3;
                *b = ((v >> 10) & 0x1F) << 3; *a = (v & 0x8000)? 255 : 0; break;
            case GL_UNSIGNED_SHORT_4_4_4_4:
                *r = (v & 0x0F) << 4; *g = ((v >> 4) & 0x0F) << 4;
                *b = ((v >> 8) & 0x0F) << 4; *a = ((v >> 12) & 0x0F) << 4; break;
            default:
                *r = *g = *b = *a = 255; break;
        }
        return;
    }

    switch (format)
    {
        case GL_RGBA:            *r = p[0]; *g = p[1]; *b = p[2]; *a = p[3]; break;
        case GL_RGB:             *r = p[0]; *g = p[1]; *b = p[2]; *a = 255; break;
        case GL_LUMINANCE_ALPHA: *r = *g = *b = p[0]; *a = p[1]; break;
        case GL_LUMINANCE:       *r = *g = *b = p[0]; *a = 255; break;
        default:                 *r = *g = *b = *a = 255; break;
    }
}

// Pack RGBA into the destination PSM. 5650 and 4444 put red in the low bits, matching the
// framebuffer expansion the readback proved; a red/blue swap in an `auto` capture means
// this order is wrong, not the source.
static unsigned int packRGBA(int r, int g, int b, int a, unsigned int fmt)
{
    if (fmt == GU_PSM_5650)
        return ((unsigned)(r >> 3) & 0x1F) |
               (((unsigned)(g >> 2) & 0x3F) << 5) |
               (((unsigned)(b >> 3) & 0x1F) << 11);

    if (fmt == GU_PSM_4444)
        return ((unsigned)(r >> 4) & 0x0F) |
               (((unsigned)(g >> 4) & 0x0F) << 4) |
               (((unsigned)(b >> 4) & 0x0F) << 8) |
               (((unsigned)(a >> 4) & 0x0F) << 12);

    return ((unsigned)a << 24) | ((unsigned)b << 16) | ((unsigned)g << 8) | (unsigned)r;
}

// Sprint 015 T2: choose a format per texture. 8888 is the default (pixel parity); `auto`
// is the measured build. Only an opaque GL_RGB source becomes 5650; an RGBA source
// becomes 4444 only when every alpha is 0 or 255. A GRAY_ALPHA source (the font atlas)
// always stays 8888: its two bytes are luminance+alpha, not a packed 16-bit colour, and
// the 4444 path would turn glyphs into solid blocks.
static unsigned int chooseFormat(GLenum format, GLenum type, const unsigned char *pixels,
                                 int width, int height)
{
#if PSPGU_TEXFMT_AUTO
    if (pixels != NULL)
    {
        if ((type == GL_UNSIGNED_BYTE) && (format == GL_RGB)) return GU_PSM_5650;

        if ((type == GL_UNSIGNED_BYTE) && (format == GL_RGBA))
        {
            int bytes = sourceBytesPerPixel(format, type);
            int binary = 1;

            for (int i = 0; i < width*height; i++)
            {
                unsigned char a = pixels[(size_t)i*bytes + 3];
                if ((a != 0) && (a != 255)) { binary = 0; break; }
            }
            if (binary) return GU_PSM_4444;
        }
    }
#else
    (void)format; (void)type; (void)pixels; (void)width; (void)height;
#endif

    return GU_PSM_8888;
}

static unsigned int texBytes(const PspGuTexture *t)
{
    return (unsigned int)t->padW*(unsigned int)t->padH*(unsigned int)bytesPerTexel(t->guFormat);
}

// Write the packed linear shadow to VRAM. One 8-row band at a time keeps the source reads
// sequential and cache-resident; the destination is the swizzled block layout, followed
// by one exact range writeback. A band is copied in 512-pixel chunks so a tbw up to 1024
// needs no 32 KiB stack. No full-image scratch and no whole-cache invalidate.
static void uploadShadowToVram(PspGuTexture *t)
{
    int bpp = bytesPerTexel(t->guFormat);
    unsigned int widthBytes = (unsigned int)t->tbw*(unsigned int)bpp;
    unsigned int texelsPerBlock = 16u/(unsigned int)bpp;
    unsigned int bandRow = widthBytes;
    unsigned int total = bandRow*(unsigned int)t->padH;
    unsigned char *base = (unsigned char *)sceGeEdramGetAddr() + t->offset;

    if (!t->shadow || (t->shadowBytes < total)) return;

    if (!t->swizzled)
    {
        memcpy(base, t->shadow, total);
        pspguWriteback(base, total);
        return;
    }

    if ((t->tbw > 1024) || (t->padH > 1024))
    {
        pspguLog("texture %ux%u: %d x %d buffer exceeds the GE 1024-pixel limit",
                 t->width, t->height, t->tbw, t->padH);
        return;
    }

    unsigned char band[8*512*4];       // 8 rows x 512 px x 8888 = 16 KiB

    for (unsigned int by = 0; by < (unsigned int)t->padH; by += 8)
    {
        for (unsigned int cx = 0; cx < (unsigned int)t->tbw; cx += 512)
        {
            unsigned int chunk = (unsigned int)t->tbw - cx;
            if (chunk > 512) chunk = 512;

            unsigned int chunkRow = chunk*(unsigned int)bpp;

            for (unsigned int row = 0; row < 8; row++)
                memcpy(band + row*chunkRow, t->shadow + (size_t)(by + row)*bandRow + cx*(unsigned int)bpp, chunkRow);

            for (unsigned int bx = 0; bx < chunk; bx += texelsPerBlock)
            {
                unsigned int block = (by/8)*(widthBytes/16) + (cx + bx)/texelsPerBlock;

                for (unsigned int row = 0; row < 8; row++)
                {
                    memcpy(base + (size_t)block*128 + row*16,
                           band + row*chunkRow + bx*(unsigned int)bpp, 16);
                }
            }
        }
    }

    pspguWriteback(base, total);
}

// Decode the source into the packed linear shadow, then write it to VRAM. The shadow is
// the reload source for residency; padding is zeroed here, once.
static void uploadTexels(PspGuTexture *t, const unsigned char *pixels, GLenum format, GLenum type)
{
    int bpp = bytesPerTexel(t->guFormat);
    unsigned int bandRow = (unsigned int)t->tbw*(unsigned int)bpp;
    unsigned int total = bandRow*(unsigned int)t->padH;
    int bytes = sourceBytesPerPixel(format, type);

    if (!t->shadow || (t->shadowBytes < total)) return;

    t->swizzled = PSPGU_SWIZZLE? 1 : 0;
    memset(t->shadow, 0, total);

    for (unsigned int y = 0; y < (unsigned int)t->height; y++)
    {
        for (unsigned int x = 0; x < (unsigned int)t->width; x++)
        {
            int r, g, b, a;
            unsigned int texel;

            readSourceRGBA(pixels + ((size_t)y*t->width + x)*bytes, format, type, &r, &g, &b, &a);
            texel = packRGBA(r, g, b, a, t->guFormat);

            if (bpp == 4) *(unsigned int *)(t->shadow + ((size_t)y*t->tbw + x)*4) = texel;
            else *(unsigned short *)(t->shadow + ((size_t)y*t->tbw + x)*2) = (unsigned short)texel;
        }
    }

    if (!t->swizzled && !t->warnedNonSwizzle)
    {
        pspguLog("texture %ux%u: swizzle disabled by build (PSP_GU_NO_SWIZZLE)", t->width, t->height);
        t->warnedNonSwizzle = 1;
    }

    uploadShadowToVram(t);
}

// Push the bound texture's state into the GE, which has no texture objects of its own
static void applyTexture(PspGuTexture *t)
{
    pspguBeginFrame();

    // sceGuTexImage width/height are capped at 512. For a larger texture this binds the
    // first window; glDrawArrays replaces it with the correct window per draw.
    int winW = (t->padW > 512)? 512 : t->padW;
    int winH = (t->padH > 512)? 512 : t->padH;
    float scaleU = ((t->padW > 512) || (t->padH > 512))? 1.0f : t->scaleU;
    float scaleV = ((t->padW > 512) || (t->padH > 512))? 1.0f : t->scaleV;

    sceGuTexMode(t->guFormat, 0, 0, t->swizzled? GU_TRUE : GU_FALSE);
    // GU samples the allocated (padded) image. The logical size is restored by TexScale.
    sceGuTexImage(0, winW, winH, t->tbw,
                  (void *)((unsigned char *)sceGeEdramGetAddr() + t->offset));
    // GU_TCC_RGBA: take the ALPHA from the texture, not from the vertex colour. Without
    // it every texel is opaque (the fragment alpha becomes the vertex alpha, which the
    // batcher always sends as 255) - which is exactly how a 50 %-alpha texel rendered as
    // full opacity and a transparent one as opaque black. Every reference sets this pair;
    // PSPRuntime gpumanager.cpp:126, PGE pgeGfx.c:170.
    // GE 8888 draw buffers do not retain alpha, so a render target is sampled as RGB
    // (opaque) rather than letting TCC_RGBA read a zero alpha and blend it away.
    sceGuTexFunc(pspgu.texEnvMode, t->renderTarget? GU_TCC_RGB : GU_TCC_RGBA);
    sceGuTexEnvColor(pspgu.texEnvColor);
    // Render targets are sampled 1:1; point sampling avoids half-texel blending at edges.
    sceGuTexFilter(t->renderTarget? GU_NEAREST : t->filter,
                   t->renderTarget? GU_NEAREST : t->filter);
    sceGuTexWrap(t->wrap, t->wrap);
    // GU's texture coordinate transform is not identity by default, and every reference
    // engine sets it explicitly right here (PSPRuntime gpumanager.cpp:128, PGE pgeGfx.c:174).
    // Leaving it unset makes every sample come from one texel - "wrong texel" with the
    // right texture bound.
    // Sprint 012's open item: TexScale is identity only for a power-of-two texture. A
    // padded one needs the logical/allocated ratio, or every sample is compressed into
    // the top-left of the padded image.
    sceGuTexScale(scaleU, scaleV);
    sceGuTexOffset(0.0f, 0.0f);
    sceGuTexFlush();
    if (t->dirty)
    {
        // The GE wrote this target; make the next texture load wait for that write.
        sceGuTexSync();
        t->dirty = 0;
    }
}

// Evict until one span can satisfy `bytes`. Priority is the strong weight; age breaks
// ties. A texture used in the last 2 frames is pinned: a list in flight may still sample
// it. If every candidate is pinned, finish and wait for the lists, then retry. If the
// total free is enough but no single span is (fragmentation), the evictions coalesce it;
// that is counted as `coalesced`, not as a data-moving compaction.
static int residencyEvictFor(unsigned int bytes, PspGuTexture *exclude)
{
    int waited = 0, evicted = 0;
    int totalEnough = (pspguVramFreeBytes() >= bytes);

    while (pspguVramLargestBytes() < bytes)
    {
        int victim = -1;
        float bestPriority = 2.0f;
        unsigned int bestAge = 0;

        for (unsigned int i = 2; i < PSPGU_MAX_TEXTURES; i++)
        {
            PspGuTexture *t = &textures[i];

            if (!t->used || !t->resident || !t->shadow) continue;
            if (t == exclude) continue;
            if (i == pspgu.boundTexture) continue;
            if (!waited && ((pspgu.frames - t->lastUsed) <= 2)) continue;

            unsigned int age = pspgu.frames - t->lastUsed;

            if ((t->priority < bestPriority) ||
                ((t->priority == bestPriority) && (age > bestAge)))
            {
                bestPriority = t->priority;
                bestAge = age;
                victim = (int)i;
            }
        }

        if (victim < 0)
        {
            if (!waited)
            {
                pspguFinishAndWait();
                waited = 1;
                continue;
            }
            return 0;
        }

        PspGuTexture *v = &textures[victim];
        pspguVramFree(v->offset);
        v->offset = 0;
        v->resident = 0;
        pspgu.residencyEvictions++;
        evicted = 1;
    }

    if (evicted && totalEnough) pspgu.residencyCoalesced++;
    return 1;
}

static int residencyAlloc(PspGuTexture *t, unsigned int bytes)
{
    unsigned int off = pspguVramAlloc(bytes);

    if (off == 0)
    {
        if (!residencyEvictFor(bytes, t))
        {
            pspgu.residencyFailed++;
            return 0;
        }

        off = pspguVramAlloc(bytes);
        if (off == 0)
        {
            pspgu.residencyFailed++;
            return 0;
        }
    }

    t->offset = off;
    t->requestedBytes = bytes;
    t->resident = 1;
    return 1;
}

static int residencyMakeResident(PspGuTexture *t)
{
    if (t->resident) return 1;
    if (!t->shadow || (t->padW <= 0) || (t->padH <= 0)) return 0;

    unsigned int bytes = texBytes(t);

    if (!residencyAlloc(t, bytes)) return 0;

    uploadShadowToVram(t);
    pspgu.residencyReloads++;
    applyTexture(t);

    return 1;
}

void pspguInitTextures(void)
{
    memset(textures, 0, sizeof(textures));
    currentTexture = 0;

    pspgu.residencyEvictions = 0;
    pspgu.residencyReloads = 0;
    pspgu.residencyFailed = 0;
    pspgu.residencyCoalesced = 0;
    pspgu.shadowBytes = 0;

    // A 1x1 white texel, so bind(0) can never leave the GE sampling an undefined address -
    // which is what blacked the whole screen when GL_TEXTURE_2D was first enabled.
    {
        unsigned int offset = pspguVramAlloc(4);
        unsigned int *px = (unsigned int *)((unsigned char *)sceGeEdramGetAddr() + offset);

        if (offset != 0)
        {
            *px = 0xFFFFFFFFu;
            pspguWriteback(px, 4);

            defaultTexture = 1;
            textures[1].used = 1;
            textures[1].offset = offset;
            textures[1].requestedBytes = 4;
            textures[1].resident = 1;
            textures[1].priority = 1.0f;
            textures[1].lastUsed = pspgu.frames;
            textures[1].width = textures[1].height = 1;
            textures[1].tbw = 1;
            textures[1].padW = textures[1].padH = 1;
            textures[1].scaleU = textures[1].scaleV = 1.0f;
            textures[1].guFormat = GU_PSM_8888;
            textures[1].filter = GU_NEAREST;
            textures[1].wrap = GU_CLAMP;
        }
    }
}

void glGenTextures(GLsizei n, GLuint *ids)
{
    if (ids == NULL) return;

    for (GLsizei i = 0; i < n; i++)
    {
        unsigned int id = defaultTexture + 1;

        while ((id < PSPGU_MAX_TEXTURES) && textures[id].used) id++;
        if (id >= PSPGU_MAX_TEXTURES) { ids[i] = 0; continue; }

        PspGuTexture *t = &textures[id];

        if (t->shadow != NULL)
        {
            if (pspgu.shadowBytes >= t->shadowBytes) pspgu.shadowBytes -= t->shadowBytes;
            else pspgu.shadowBytes = 0;
            free(t->shadow);
        }

        memset(t, 0, sizeof(*t));
        t->used = 2;                // reserved; dimensions unknown until glTexImage2D
        t->priority = 1.0f;
        t->lastUsed = pspgu.frames;
        ids[i] = id;
    }
}

void glDeleteTextures(GLsizei n, const GLuint *ids)
{
    if (ids == NULL) return;

    for (GLsizei i = 0; i < n; i++)
    {
        PspGuTexture *t = lookup(ids[i]);

        if (t == NULL) continue;
        if (ids[i] == defaultTexture) continue;     // bind(0) must always have a texel

        if (t->resident && (t->offset != 0))
        {
            pspguVramFree(t->offset);
            t->offset = 0;
            t->resident = 0;
            t->requestedBytes = 0;
        }

        if (t->shadow != NULL)
        {
            if (pspgu.shadowBytes >= t->shadowBytes) pspgu.shadowBytes -= t->shadowBytes;
            else pspgu.shadowBytes = 0;
            free(t->shadow);
            t->shadow = NULL;
            t->shadowBytes = 0;
        }

        t->used = 0;
    }
}

void glBindTexture(GLenum target, GLuint id)
{
    PspGuTexture *t;

    (void)target;
    if (!pspgu.current) return;

    if (id == 0) { currentTexture = defaultTexture; t = lookup(defaultTexture); }
    else
    {
        currentTexture = id;
        t = lookup(id);
    }

    if (t != NULL)
    {
        t->lastUsed = pspgu.frames;

        if ((t->padW > 0) && !t->resident)
        {
            if (!residencyMakeResident(t))
            {
                // A texture that cannot be made resident must not sample freed VRAM.
                pspguLog("residency: texture %u could not be made resident", id);
                t = NULL;
            }
        }
    }

    if ((t != NULL) && (t->padW > 0) && t->resident)
    {
        pspgu.boundTexture = (id == 0)? defaultTexture : id;
        applyTexture(t);
    }
    else
    {
        // Reserved (generated, no image yet) or a failed reload: sample the white texel.
        pspgu.boundTexture = defaultTexture;
        PspGuTexture *d = lookup(defaultTexture);
        if ((d != NULL) && d->resident) applyTexture(d);
    }
}

void glTexImage2D(GLenum target, GLint level, GLint internalFormat, GLsizei width, GLsizei height,
                  GLint border, GLenum format, GLenum type, const GLvoid *pixels)
{
    PspGuTexture *t;

    (void)target; (void)internalFormat; (void)border;

    if (!pspgu.current || (width <= 0) || (height <= 0)) return;
    if (level != 0) return;             // mipmaps: raylib uploads level 0 only

    t = lookup(currentTexture);
    if (t == NULL) { currentTexture = defaultTexture; return; }

    // One format decision per texture; the default build is 8888.
    t->guFormat = chooseFormat(format, type, (const unsigned char *)pixels, width, height);
    t->width = width;
    t->height = height;
    t->tbw = nextPow2(width);
    t->padW = t->tbw;
    t->padH = nextPow2(height);
    if (t->padH < 8) t->padH = 8;
    t->scaleU = (float)width / (float)t->padW;
    t->scaleV = (float)height / (float)t->padH;

    // The GE buffer width/height is at most 1024 (width/height arguments to
    // sceGuTexImage are still <=512 windows). A larger allocation cannot be sampled.
    if ((t->padW > 1024) || (t->padH > 1024))
    {
        pspguLog("texture %u: %dx%d exceeds the GE 1024-pixel buffer limit (%d x %d)",
                 currentTexture, width, height, t->padW, t->padH);
        if (t->shadow != NULL)
        {
            if (pspgu.shadowBytes >= t->shadowBytes) pspgu.shadowBytes -= t->shadowBytes;
            else pspgu.shadowBytes = 0;
            free(t->shadow);
            t->shadow = NULL;
            t->shadowBytes = 0;
        }
        t->used = 0;
        return;
    }

    unsigned int bytes = texBytes(t);

    if (textureLogEnabled)
        pspguLog("texture %u: %dx%d src=0x%x -> PSM 0x%x (%u B)",
                 currentTexture, width, height, format, t->guFormat, bytes);

    // Main-RAM shadow: the reload source, not VRAM.
    if ((t->shadow == NULL) || (t->shadowBytes != bytes))
    {
        if (t->shadow != NULL)
        {
            if (pspgu.shadowBytes >= t->shadowBytes) pspgu.shadowBytes -= t->shadowBytes;
            else pspgu.shadowBytes = 0;
            free(t->shadow);
            t->shadow = NULL;
            t->shadowBytes = 0;
        }

        t->shadow = (unsigned char *)calloc(1, bytes);
        if (t->shadow == NULL)
        {
            pspguLog("texture %u: shadow allocation failed (%u B)", currentTexture, bytes);
            t->used = 0;
            return;
        }
        t->shadowBytes = bytes;
        pspgu.shadowBytes += bytes;
    }

    int reused = (t->resident && (t->requestedBytes == bytes));

    if (!reused)
    {
        if (t->resident && (t->offset != 0))
        {
            pspguVramFree(t->offset);
            t->offset = 0;
            t->resident = 0;
            t->requestedBytes = 0;
        }

        if (!residencyAlloc(t, bytes))
        {
            PspGuVramStats st;
            pspguVramStats(&st);
            pspguLog("texture %u: out of VRAM for %dx%d (%u bytes; used=%u free=%u largest=%u)",
                     currentTexture, width, height, bytes, st.used, st.free, st.largest);
            if (t->shadow != NULL)
            {
                if (pspgu.shadowBytes >= t->shadowBytes) pspgu.shadowBytes -= t->shadowBytes;
                else pspgu.shadowBytes = 0;
                free(t->shadow);
                t->shadow = NULL;
                t->shadowBytes = 0;
            }
            t->used = 0;
            return;
        }
    }

    if (pixels != NULL) uploadTexels(t, (const unsigned char *)pixels, format, type);
    else
    {
        memset(t->shadow, 0, bytes);
        uploadShadowToVram(t);
    }

    t->used = 1;
    t->resident = 1;
    t->lastUsed = pspgu.frames;
    applyTexture(t);
}

void glTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width,
                     GLsizei height, GLenum format, GLenum type, const GLvoid *pixels)
{
    PspGuTexture *t;
    unsigned char *dst;
    int bpp, bytes, resident;

    (void)target;

    if (!pspgu.current || (pixels == NULL) || (level != 0)) return;

    t = lookup(currentTexture);
    if ((t == NULL) || (t->shadow == NULL)) return;

    bpp = bytesPerTexel(t->guFormat);
    bytes = sourceBytesPerPixel(format, type);
    resident = (t->resident && (t->offset != 0));
    dst = resident? ((unsigned char *)sceGeEdramGetAddr() + t->offset) : NULL;

    // A sub-rectangle updates the shadow always and VRAM only when resident. A
    // non-resident texture picks the change up on its next reload.
    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            int r, g, b, a;
            unsigned int texel;
            size_t index = (size_t)(yoffset + y)*t->tbw + (xoffset + x);

            readSourceRGBA((const unsigned char *)pixels + ((size_t)y*width + x)*bytes,
                           format, type, &r, &g, &b, &a);
            texel = packRGBA(r, g, b, a, t->guFormat);

            if (bpp == 4) *(unsigned int *)(t->shadow + index*4) = texel;
            else *(unsigned short *)(t->shadow + index*2) = (unsigned short)texel;

            if (resident)
            {
                unsigned int off = t->swizzled
                    ? swizzledOffset(xoffset + x, yoffset + y, t->tbw, bpp)
                    : (unsigned int)(index*bpp);

                if (bpp == 4) *(unsigned int *)(dst + off) = texel;
                else *(unsigned short *)(dst + off) = (unsigned short)texel;
            }
        }
    }

    if (resident)
    {
        pspguWriteback(dst, (unsigned int)(t->tbw * t->padH * bpp));
        pspguBeginFrame();
        sceGuTexFlush();
    }
}

void glTexParameteri(GLenum target, GLenum pname, GLint param)
{
    PspGuTexture *t;

    (void)target;

    if (!pspgu.current) return;
    t = lookup(currentTexture);
    if (t == NULL) return;

    switch (pname)
    {
        case GL_TEXTURE_MIN_FILTER:
        case GL_TEXTURE_MAG_FILTER:
            t->filter = (param == GL_NEAREST)? GU_NEAREST : GU_LINEAR;
            break;
        case GL_TEXTURE_WRAP_S:
        case GL_TEXTURE_WRAP_T:
            t->wrap = (param == GL_CLAMP_TO_EDGE)? GU_CLAMP : GU_REPEAT;
            break;
        default: return;
    }

    if (t->resident && (t->padW > 0) && (t->padH > 0)) applyTexture(t);
}

// GL 1.1 application priority: higher is kept. The default is 1.0; the residency victim
// rule uses this first and age second.
void glPrioritizeTextures(GLsizei n, const GLuint *ids, const GLclampf *priorities)
{
    if ((ids == NULL) || (priorities == NULL)) return;

    for (GLsizei i = 0; i < n; i++)
    {
        PspGuTexture *t = lookup(ids[i]);
        if (t == NULL) continue;

        float p = priorities[i];
        if (p < 0.0f) p = 0.0f;
        if (p > 1.0f) p = 1.0f;
        t->priority = p;
    }
}

// The probe asks whether a texture is resident; GL_TEXTURE_RESIDENT is the standard query.
void glGetTexParameteriv(GLenum target, GLenum pname, GLint *params)
{
    PspGuTexture *t;

    (void)target;
    if (params == NULL) return;

    t = lookup(currentTexture);

    switch (pname)
    {
        case GL_TEXTURE_RESIDENT:
            *params = (t != NULL && t->resident)? 1 : 0;
            break;
        case GL_TEXTURE_PRIORITY:
            *params = (t != NULL)? (GLint)(t->priority*1000.0f) : 0;
            break;
        default:
            *params = 0;
            break;
    }
}

// Standard GL 1.1 query: report residency without binding, so the prioritisation probe
// can inspect the victim without reloading it.
GLboolean glAreTexturesResident(GLsizei n, const GLuint *ids, GLboolean *residences)
{
    GLboolean all = GL_TRUE;

    if ((ids == NULL) || (residences == NULL)) return GL_FALSE;

    for (GLsizei i = 0; i < n; i++)
    {
        PspGuTexture *t = lookup(ids[i]);
        residences[i] = (t != NULL && t->resident)? GL_TRUE : GL_FALSE;
        if (!residences[i]) all = GL_FALSE;
    }

    return all;
}

void pspguResidencyStats(PspGuResidencyStats *out)
{
    if (out == NULL) return;

    memset(out, 0, sizeof(*out));
    out->shadowBytes = pspgu.shadowBytes;
    out->evictions = pspgu.residencyEvictions;
    out->reloads = pspgu.residencyReloads;
    out->failed = pspgu.residencyFailed;
    out->coalesced = pspgu.residencyCoalesced;

    for (unsigned int i = 1; i < PSPGU_MAX_TEXTURES; i++)
    {
        if (textures[i].used && textures[i].resident) out->resident++;
    }
}

void pspguSetTextureLog(int on)
{
    textureLogEnabled = on? 1 : 0;
}

int pspguBoundTextureInfo(int *width, int *height, int *padW, int *padH, int *tbw,
                          int *bpp, int *swizzled, unsigned int *offset)
{
    PspGuTexture *t = lookup(pspgu.boundTexture);

    if ((t == NULL) || (t->padW <= 0) || (t->padH <= 0)) return 0;
    if (t->padW > 512 || t->padH > 512)
    {
        if (width != NULL) *width = t->width;
        if (height != NULL) *height = t->height;
        if (padW != NULL) *padW = t->padW;
        if (padH != NULL) *padH = t->padH;
        if (tbw != NULL) *tbw = t->tbw;
        if (bpp != NULL) *bpp = bytesPerTexel(t->guFormat);
        if (swizzled != NULL) *swizzled = t->swizzled;
        if (offset != NULL) *offset = t->offset;
        return 1;
    }

    return 0;
}

void pspguTextureReport(const char *when)
{
    PspGuResidencyStats st;

    pspguResidencyStats(&st);
    pspguLog("RESIDENCY [%s]: resident=%u shadow=%u evictions=%u reloads=%u failed=%u coalesced=%u (bytes/events)",
             when, st.resident, st.shadowBytes, st.evictions, st.reloads, st.failed, st.coalesced);
}

//------------------------------------------------------------------------------------
// Texture environment (sprint 062). MODULATE is the default and the 2D path's mode.
//------------------------------------------------------------------------------------

void glTexEnvi(GLenum target, GLenum pname, GLint param)
{
    if (!pspgu.current) return;

    if (target != GL_TEXTURE_ENV) return;

    if (pname == GL_TEXTURE_ENV_MODE)
    {
        int mode;
        switch (param)
        {
            case GL_MODULATE: mode = GU_TFX_MODULATE; break;
            case GL_REPLACE:  mode = GU_TFX_REPLACE;  break;
            case GL_DECAL:    mode = GU_TFX_DECAL;    break;
            case GL_ADD:      mode = GU_TFX_ADD;      break;
            case GL_BLEND:    mode = GU_TFX_BLEND;    break;
            default:
            {
                static int logged = 0;
                if (!logged) { pspguLog("glTexEnv: unsupported mode 0x%x (COMBINE?); keeping 0x%x", (unsigned)param, pspgu.texEnvMode); logged = 1; }
            } return;
        }
        pspgu.texEnvMode = mode;
        return;
    }

    {
        static int logged = 0;
        if (!logged) { pspguLog("glTexEnvi: unsupported pname 0x%x; ignored", (unsigned)pname); logged = 1; }
    }
}

void glTexEnvfv(GLenum target, GLenum pname, const GLfloat *params)
{
    if (!pspgu.current || (params == NULL) || (target != GL_TEXTURE_ENV)) return;

    if (pname == GL_TEXTURE_ENV_MODE) { glTexEnvi(target, pname, (GLint)params[0]); return; }

    if (pname == GL_TEXTURE_ENV_COLOR)
    {
        unsigned int r = (unsigned int)(params[0]*255.0f + 0.5f);
        unsigned int g = (unsigned int)(params[1]*255.0f + 0.5f);
        unsigned int b = (unsigned int)(params[2]*255.0f + 0.5f);
        unsigned int a = (unsigned int)(params[3]*255.0f + 0.5f);
        pspgu.texEnvColor = (a << 24) | (b << 16) | (g << 8) | r;
        return;
    }

    {
        static int logged = 0;
        if (!logged) { pspguLog("glTexEnvfv: unsupported pname 0x%x; ignored", (unsigned)pname); logged = 1; }
    }
}

void glTexEnvf(GLenum target, GLenum pname, GLfloat param)
{
    if (pname == GL_TEXTURE_ENV_MODE) glTexEnvi(target, pname, (GLint)param);
}

//------------------------------------------------------------------------------------
// Render-target support (sprint 065)
//------------------------------------------------------------------------------------

int pspguTextureRenderTarget(unsigned int id, unsigned int *offset, int *w, int *h,
                             int *stride, int *psm)
{
    PspGuTexture *t = lookup(id);

    if ((t == NULL) || !t->used || !t->resident || (t->padW < 64) || (t->padH < 1)) return 0;

    int bpp = (t->guFormat == GU_PSM_8888)? 4 : 2;
    unsigned int bytes = (unsigned int)t->padW*(unsigned int)t->padH*(unsigned int)bpp;

    // GU draw buffers need an 8192-byte-aligned base. Textures are 64-aligned, so a fresh
    // target is promoted here; attach happens before the first draw, so a copy is rare.
    if ((t->offset & 8191u) != 0)
    {
        unsigned int off = pspguVramAllocAligned(bytes, 8192);

        if (off == PSPGU_VRAM_INVALID)
        {
            pspguLog("render target %u: no 8192-aligned %u-byte block", id, bytes);
            return 0;
        }

        unsigned char *base = (unsigned char *)sceGeEdramGetAddr();
        memcpy(base + off, base + t->offset, bytes);
        pspguVramFree(t->offset);
        t->offset = off;
        t->requestedBytes = bytes;
    }

    if (t->shadow != NULL)
    {
        if (pspgu.shadowBytes >= t->shadowBytes) pspgu.shadowBytes -= t->shadowBytes;
        else pspgu.shadowBytes = 0;
        free(t->shadow);
        t->shadow = NULL;
        t->shadowBytes = 0;
    }

    t->renderTarget = 1;
    t->pinned = 1;
    t->dirty = 1;
    t->swizzled = 0;        // the GE writes the draw buffer linearly

    if (offset != NULL) *offset = t->offset;
    if (w != NULL) *w = t->width;
    if (h != NULL) *h = t->height;
    if (stride != NULL) *stride = t->padW;
    if (psm != NULL) *psm = (int)t->guFormat;
    return 1;
}

void pspguTextureMarkDirty(unsigned int id)
{
    PspGuTexture *t = lookup(id);
    if (t != NULL) t->dirty = 1;
}

int pspguTextureIsRenderTarget(unsigned int id)
{
    PspGuTexture *t = lookup(id);
    return (t != NULL) && t->renderTarget;
}
