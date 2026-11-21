/*
 * gl_texture.c - texture objects on the GE.
 *
 * Task 6 of phase 5. Three things decide the shape of this file:
 *
 *  - The GE has no LUMINANCE or LUMINANCE_ALPHA texture, and raylib's GL11 path hands over
 *    both: every PNG that decodes to grey+alpha (our sprite sheet and the default font atlas)
 *    arrives as GL_LUMINANCE_ALPHA. Those are expanded to RGBA8888 here, which is exactly
 *    what pspGL's copy_expand_LA does - so captures stay comparable instead of quietly
 *    changing colour.
 *  - 8888 for everything is 4 bytes per texel. Sprint 010 measured texture format as a
 *    *memory* lever only (0.0 % on the frame either way), so fidelity wins here and the
 *    16-bit formats are a later memory decision per texture.
 *  - Textures are uploaded UNSWIZZLED (sceGuTexMode's last argument is 0). GU accepts that;
 *    swizzling is a bandwidth optimisation and does not change a single texel. Add it when a
 *    texture-heavy scene says it matters, not before.
 */

#include <string.h>
#include <stdlib.h>

#include "pspgu_internal.h"

#define PSPGU_MAX_TEXTURES 256

typedef struct {
    int used;
    unsigned int offset;        // EDRAM offset of the texels
    int width, height;
    int tbw;                    // texture buffer width: power of two >= width, as GU wants
    int swizzled;               // 1 when the texels are in the swizzled layout
    int warnedNonSwizzle;       // the dimensions were complained about once
    unsigned int guFormat;      // GU_PSM_*
    int filter;                 // GU_NEAREST / GU_LINEAR
    int wrap;                   // GU_REPEAT / GU_CLAMP for both axes
} PspGuTexture;

static PspGuTexture textures[PSPGU_MAX_TEXTURES];
static unsigned int currentTexture = 0;
static unsigned int defaultTexture = 0;
static unsigned int vramUsed = 0;       // bytes of texture VRAM handed out

// T3: the standard 16-byte-block / 8-row swizzle, the same one PSPRuntime's SwizzleFast
// and the SDK samples use. Swizzling changes no texel, only the order, so a correct
// implementation must leave every capture identical - that is exactly how it is verified.
// PSP_GU_NO_SWIZZLE=1 keeps the linear layout for the A/B.
#ifndef PSP_GU_NO_SWIZZLE
    #define PSPGU_SWIZZLE 1
#else
    #define PSPGU_SWIZZLE 0
#endif

static void swizzleFast(unsigned char *out, const unsigned char *in, unsigned int widthBytes, unsigned int height)
{
    unsigned int blockx, blocky, j;
    unsigned int widthBlocks = widthBytes/16;
    unsigned int heightBlocks = height/8;
    unsigned int srcPitch = (widthBytes - 16)/4;
    unsigned int srcRow = widthBytes*8;
    const unsigned char *ysrc = in;
    unsigned int *dst = (unsigned int *)out;

    for (blocky = 0; blocky < heightBlocks; blocky++)
    {
        const unsigned char *xsrc = ysrc;

        for (blockx = 0; blockx < widthBlocks; blockx++)
        {
            const unsigned int *src = (const unsigned int *)xsrc;

            for (j = 0; j < 8; j++)
            {
                *(dst++) = *(src++); *(dst++) = *(src++); *(dst++) = *(src++); *(dst++) = *(src++);
                src += srcPitch;
            }
            xsrc += 16;
        }
        ysrc += srcRow;
    }
}

// Where texel (x,y) lives in the swizzled layout - needed for sub-image updates
static unsigned int swizzledOffset(int x, int y, int tbw)
{
    unsigned int widthBytes = (unsigned int)tbw*4;
    unsigned int block = (unsigned int)((y/8)*(widthBytes/16) + (x/4))*128;

    return block + (unsigned int)((y%8)*16) + (unsigned int)((x%4)*4);
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

// Pack one source pixel to the GE's 0xAABBGGRR word
static unsigned int pack8888(const unsigned char *p, GLenum format)
{
    switch (format)
    {
        case GL_RGBA:             return ((unsigned int)p[3] << 24) | ((unsigned int)p[2] << 16) |
                                         ((unsigned int)p[1] << 8) | (unsigned int)p[0];
        case GL_RGB:              return 0xFF000000u | ((unsigned int)p[2] << 16) |
                                         ((unsigned int)p[1] << 8) | (unsigned int)p[0];
        case GL_LUMINANCE_ALPHA:  return ((unsigned int)p[1] << 24) | ((unsigned int)p[0] << 16) |
                                         ((unsigned int)p[0] << 8) | (unsigned int)p[0];
        case GL_LUMINANCE:        return 0xFF000000u | ((unsigned int)p[0] << 16) |
                                         ((unsigned int)p[0] << 8) | (unsigned int)p[0];
        default:                  return 0xFFFFFFFFu;
    }
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

// Expand any source the library passes into a GE 8888 texture
static void upload8888(PspGuTexture *t, const unsigned char *pixels, GLenum format, GLenum type)
{
    unsigned char *base = (unsigned char *)sceGeEdramGetAddr() + t->offset;
    unsigned int rowBytes = (unsigned int)t->tbw*4;
    unsigned int total = rowBytes*(unsigned int)t->height;
    unsigned int *scratch = (unsigned int *)calloc(1, total);      // zeroed: padding is real
    int bytes = sourceBytesPerPixel(format, type);

    if (scratch == NULL) return;

    for (int y = 0; y < t->height; y++)
    {
        for (int x = 0; x < t->width; x++)
        {
            unsigned int texel;

            if (isPacked16(type))
            {
                unsigned short v = ((const unsigned short *)pixels)[y*t->width + x];
                unsigned int r = (v & 0x1F) << 3;
                unsigned int g = ((v >> 5) & 0x3F) << 2;
                unsigned int b = ((v >> 11) & 0x1F) << 3;
                unsigned int a = (type == GL_UNSIGNED_SHORT_5_5_5_1)? ((v & 0x8000)? 0xFF : 0) : 0xFF;
                texel = (a << 24) | (b << 16) | (g << 8) | r;
            }
            else texel = pack8888(pixels + ((size_t)y*t->width + x)*bytes, format);

            scratch[(size_t)y*t->tbw + x] = texel;
        }
    }

    // The swizzle is an 8-row block operation: it needs a power-of-two width and a height
    // that is a multiple of 8. A non-power-of-two texture (33x17) has neither, so it stays
    // linear. T2's real fix is to pad BOTH dimensions and correct with TexScale - see the
    // sprint log; until then, linear is safe and swizzled would read out of bounds.
    t->swizzled = (PSPGU_SWIZZLE && (t->tbw == t->width) && ((t->height % 8) == 0))? 1 : 0;

    if (t->swizzled) swizzleFast(base, (const unsigned char *)scratch, rowBytes, (unsigned int)t->height);
    else memcpy(base, scratch, total);

    // A texture that cannot be swizzled still renders - but it samples wrong until phase-5
    // task 6c pads the height and corrects with sceGuTexScale. Say so once, in the log,
    // because the alternative is discovering it in a pixel comparison: that is exactly how
    // the LA-decode bug cost 32 % of a frame before anyone knew where to look.
    if (!t->swizzled && !t->warnedNonSwizzle)
    {
        pspguLog("texture %ux%u: not swizzle-compatible (needs pow2 width, height %% 8 == 0),"
                 " using linear - sampling is wrong until task 6c",
                 t->width, t->height);
        t->warnedNonSwizzle = 1;
    }

    free(scratch);
    pspguWriteback(base, total);
}

// Push the bound texture's state into the GE, which has no texture objects of its own
static void applyTexture(PspGuTexture *t)
{
    pspguBeginFrame();

    sceGuTexMode(t->guFormat, 0, 0, t->swizzled? GU_TRUE : GU_FALSE);
    sceGuTexImage(0, t->width, t->height, t->tbw,
                  (void *)((unsigned char *)sceGeEdramGetAddr() + t->offset));
    // GU_TCC_RGBA: take the ALPHA from the texture, not from the vertex colour. Without
    // it every texel is opaque (the fragment alpha becomes the vertex alpha, which the
    // batcher always sends as 255) - which is exactly how a 50 %-alpha texel rendered as
    // full opacity and a transparent one as opaque black. Every reference sets this pair;
    // PSPRuntime gpumanager.cpp:126, PGE pgeGfx.c:170.
    sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA);
    sceGuTexFilter(t->filter, t->filter);
    sceGuTexWrap(t->wrap, t->wrap);
    // GU's texture coordinate transform is not identity by default, and every reference
    // engine sets it explicitly right here (PSPRuntime gpumanager.cpp:128, PGE pgeGfx.c:174).
    // Leaving it unset makes every sample come from one texel - "wrong texel" with the
    // right texture bound.
    sceGuTexScale(1.0f, 1.0f);
    sceGuTexOffset(0.0f, 0.0f);
    sceGuTexFlush();
}

void pspguInitTextures(void)
{
    memset(textures, 0, sizeof(textures));
    currentTexture = 0;
    vramUsed = 0;

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
            textures[1].width = textures[1].height = 1;
            textures[1].tbw = 1;
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

        textures[id].used = 2;      // reserved, dimensions unknown until glTexImage2D
        ids[i] = id;
    }
}

void glDeleteTextures(GLsizei n, const GLuint *ids)
{
    if (ids == NULL) return;

    for (GLsizei i = 0; i < n; i++)
    {
        PspGuTexture *t = lookup(ids[i]);
        if (t != NULL) t->used = 0;
        // VRAM is not reclaimed: the allocator is a bump arena. Texture churn belongs to
        // phase 6's residency work, and raylib loads its textures once at startup.
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

    pspgu.boundTexture = (id == 0)? defaultTexture : id;
    if (t != NULL) applyTexture(t);
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

    // Every source becomes 8888; see the header comment for why
    t->guFormat = GU_PSM_8888;
    t->width = width;
    t->height = height;
    // GU wants a power-of-two row stride. Rows are written at that stride, and the padding
    // columns are part of the texture (the swizzle moves them around), so they are zeroed
    // rather than left as whatever VRAM held before.
    t->tbw = nextPow2(width);

    if (t->offset == 0)
    {
        unsigned int bytes = (unsigned int)(t->tbw * t->height * 4);

        t->offset = pspguVramAlloc(bytes);
        if (t->offset == 0)
        {
            pspguLog("texture %u: out of VRAM for %dx%d (%u bytes, %u used)",
                     currentTexture, width, height, bytes, vramUsed);
            t->used = 0;
            return;
        }
        vramUsed += bytes;
    }

    if (pixels != NULL) upload8888(t, (const unsigned char *)pixels, format, type);

    t->used = 1;
    applyTexture(t);
}

void glTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width,
                     GLsizei height, GLenum format, GLenum type, const GLvoid *pixels)
{
    PspGuTexture *t;
    unsigned int *dst;
    int bytes;

    (void)target;

    if (!pspgu.current || (pixels == NULL) || (level != 0)) return;

    t = lookup(currentTexture);
    if ((t == NULL) || (t->offset == 0)) return;

    dst = (unsigned int *)((unsigned char *)sceGeEdramGetAddr() + t->offset);
    bytes = sourceBytesPerPixel(format, type);

    // A sub-rectangle goes straight into the existing layout (UpdateTexture's path)
    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            unsigned int texel;

            if (isPacked16(type))
            {
                unsigned short v = ((const unsigned short *)pixels)[y*width + x];
                texel = 0xFF000000u | (((unsigned int)((v >> 11) & 0x1F) << 19) |
                                       ((unsigned int)((v >> 5) & 0x3F) << 10) |
                                       ((unsigned int)(v & 0x1F) << 3));
            }
            else texel = pack8888((const unsigned char *)pixels + ((size_t)y*width + x)*bytes, format);

            if (t->swizzled)
                *(unsigned int *)((unsigned char *)dst + swizzledOffset(xoffset + x, yoffset + y, t->tbw)) = texel;
            else
                dst[(size_t)(yoffset + y)*t->tbw + (xoffset + x)] = texel;
        }
    }

    pspguWriteback(dst, (unsigned int)(t->tbw * t->height * 4));
    pspguBeginFrame();
    sceGuTexFlush();
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

    applyTexture(t);
}
