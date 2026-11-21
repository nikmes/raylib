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
    unsigned int requestedBytes; // logical allocation size, for reuse and freeing
    int width, height;          // logical size the application asked for
    int tbw;                    // texture buffer width: power of two >= width, as GU wants
    int padW, padH;             // allocated size: powers of two >= width/height
    float scaleU, scaleV;       // logical/allocated, for sceGuTexScale
    int swizzled;               // 1 when the texels are in the swizzled layout
    int warnedNonSwizzle;       // the dimensions were complained about once
    unsigned int guFormat;      // GU_PSM_*
    int filter;                 // GU_NEAREST / GU_LINEAR
    int wrap;                   // GU_REPEAT / GU_CLAMP for both axes
} PspGuTexture;

static PspGuTexture textures[PSPGU_MAX_TEXTURES];
static unsigned int currentTexture = 0;
static unsigned int defaultTexture = 0;

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
    // The padded rows are part of the texture the GE samples; they are zeroed (calloc)
    // and included in the swizzle. Allocating only height rows and telling GU a padded
    // height reads past the texture.
    unsigned int total = rowBytes*(unsigned int)t->padH;
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

    // Both allocated dimensions are now powers of two (padW = tbw, padH >= height), so
    // the 8-row swizzle always applies. A non-power-of-two texture is sampled through
    // sceGuTexScale(realW/padW, realH/padH), set in applyTexture.
    t->swizzled = (PSPGU_SWIZZLE && (t->padH % 8 == 0))? 1 : 0;

    if (t->swizzled) swizzleFast(base, (const unsigned char *)scratch, rowBytes, (unsigned int)t->padH);
    else memcpy(base, scratch, total);

    if (!t->swizzled && !t->warnedNonSwizzle)
    {
        pspguLog("texture %ux%u: swizzle disabled by build (PSP_GU_NO_SWIZZLE)", t->width, t->height);
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
    // GU samples the allocated (padded) image. The logical size is restored by TexScale.
    sceGuTexImage(0, t->padW, t->padH, t->tbw,
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
    // Sprint 012's open item: TexScale is identity only for a power-of-two texture. A
    // padded one needs the logical/allocated ratio, or every sample is compressed into
    // the top-left of the padded image.
    sceGuTexScale(t->scaleU, t->scaleV);
    sceGuTexOffset(0.0f, 0.0f);
    sceGuTexFlush();
}

void pspguInitTextures(void)
{
    memset(textures, 0, sizeof(textures));
    currentTexture = 0;

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

        if (t == NULL) continue;
        if (ids[i] == defaultTexture) continue;     // bind(0) must always have a texel

        if (t->offset != 0)
        {
            pspguVramFree(t->offset);
            t->offset = 0;
            t->requestedBytes = 0;
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

    pspgu.boundTexture = (id == 0)? defaultTexture : id;
    // glGenTextures reserves an id and raylib binds it before glTexImage2D; there is no
    // image to describe yet, and guarding here avoids a 0x0 sceGuTexImage.
    if ((t != NULL) && (t->offset != 0) && (t->padW > 0) && (t->padH > 0)) applyTexture(t);
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
    // GU wants power-of-two dimensions and row stride. The padding columns and rows are
    // part of the image the GE samples (the swizzle moves them around), so they are
    // allocated and zeroed; the logical size is restored with sceGuTexScale.
    t->tbw = nextPow2(width);
    t->padW = t->tbw;
    t->padH = nextPow2(height);
    if (t->padH < 8) t->padH = 8;       // the swizzle is an 8-row block operation
    t->scaleU = (float)width / (float)t->padW;
    t->scaleV = (float)height / (float)t->padH;

    {
        unsigned int bytes = (unsigned int)(t->padW * t->padH * 4);

        // Reuse the block when the padded size is unchanged; otherwise return the old
        // span and take a new one. The allocator rounds to 512-byte blocks.
        if ((t->offset == 0) || (t->requestedBytes != bytes))
        {
            if (t->offset != 0)
            {
                pspguVramFree(t->offset);
                t->offset = 0;
            }

            t->offset = pspguVramAlloc(bytes);
            if (t->offset == 0)
            {
                PspGuVramStats st;
                pspguVramStats(&st);
                pspguLog("texture %u: out of VRAM for %dx%d (%u bytes; used=%u free=%u largest=%u)",
                         currentTexture, width, height, bytes, st.used, st.free, st.largest);
                t->used = 0;
                return;
            }
            t->requestedBytes = bytes;
        }
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

    pspguWriteback(dst, (unsigned int)(t->tbw * t->padH * 4));
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

    if ((t->offset != 0) && (t->padW > 0) && (t->padH > 0)) applyTexture(t);
}
