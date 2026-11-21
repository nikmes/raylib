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
// is the measured build. Only an opaque GL_RGB source becomes 5650; an alpha source
// becomes 4444 only when every alpha is 0 or 255. Anti-aliased alpha (the font atlas,
// the half-alpha probe) stays 8888, because 4-bit alpha would be a visible change.
static unsigned int chooseFormat(GLenum format, GLenum type, const unsigned char *pixels,
                                 int width, int height)
{
#if PSPGU_TEXFMT_AUTO
    if (pixels != NULL)
    {
        if ((type == GL_UNSIGNED_BYTE) && (format == GL_RGB)) return GU_PSM_5650;

        if ((type == GL_UNSIGNED_BYTE) &&
            ((format == GL_RGBA) || (format == GL_LUMINANCE_ALPHA)))
        {
            int bytes = sourceBytesPerPixel(format, type);
            int alphaIndex = (format == GL_RGBA)? 3 : 1;
            int binary = 1;

            for (int i = 0; i < width*height; i++)
            {
                unsigned char a = pixels[(size_t)i*bytes + alphaIndex];
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

// Upload any source into the texture's chosen PSM and swizzle the padded image.
static void uploadTexels(PspGuTexture *t, const unsigned char *pixels, GLenum format, GLenum type)
{
    int bpp = bytesPerTexel(t->guFormat);
    unsigned char *base = (unsigned char *)sceGeEdramGetAddr() + t->offset;
    unsigned int rowBytes = (unsigned int)t->tbw*(unsigned int)bpp;
    unsigned int total = rowBytes*(unsigned int)t->padH;
    unsigned char *scratch = (unsigned char *)calloc(1, total);     // zeroed: padding is real
    int bytes = sourceBytesPerPixel(format, type);

    if (scratch == NULL) return;

    for (int y = 0; y < t->height; y++)
    {
        for (int x = 0; x < t->width; x++)
        {
            int r, g, b, a;
            unsigned int texel;

            readSourceRGBA(pixels + ((size_t)y*t->width + x)*bytes, format, type, &r, &g, &b, &a);
            texel = packRGBA(r, g, b, a, t->guFormat);

            if (bpp == 4) ((unsigned int *)scratch)[(size_t)y*t->tbw + x] = texel;
            else ((unsigned short *)scratch)[(size_t)y*t->tbw + x] = (unsigned short)texel;
        }
    }

    // Both allocated dimensions are powers of two (padW = tbw, padH >= height), so the
    // 8-row swizzle always applies. A non-power-of-two texture is sampled through
    // sceGuTexScale(realW/padW, realH/padH), set in applyTexture.
    t->swizzled = (PSPGU_SWIZZLE && (t->padH % 8 == 0))? 1 : 0;

    if (t->swizzled) swizzleFast(base, scratch, rowBytes, (unsigned int)t->padH);
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

    // Sprint 015 T2: one format decision per texture. The default build is 8888, so the
    // parity gate cannot move; `PSP_GU_TEXFMT=auto` is the measured 16-bit build.
    t->guFormat = chooseFormat(format, type, (const unsigned char *)pixels, width, height);
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
        int bpp = bytesPerTexel(t->guFormat);
        unsigned int bytes = (unsigned int)(t->padW * t->padH * bpp);

        pspguLog("texture %u: %dx%d src=0x%x -> PSM 0x%x (%u B)",
                 currentTexture, width, height, format, t->guFormat, bytes);

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

    if (pixels != NULL) uploadTexels(t, (const unsigned char *)pixels, format, type);

    t->used = 1;
    applyTexture(t);
}

void glTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width,
                     GLsizei height, GLenum format, GLenum type, const GLvoid *pixels)
{
    PspGuTexture *t;
    unsigned char *dst;
    int bpp, bytes;

    (void)target;

    if (!pspgu.current || (pixels == NULL) || (level != 0)) return;

    t = lookup(currentTexture);
    if ((t == NULL) || (t->offset == 0)) return;

    bpp = bytesPerTexel(t->guFormat);
    dst = (unsigned char *)sceGeEdramGetAddr() + t->offset;
    bytes = sourceBytesPerPixel(format, type);

    // A sub-rectangle goes straight into the existing layout (UpdateTexture's path),
    // 8888 or 16-bit, with the same source decode as the full upload.
    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            int r, g, b, a;
            unsigned int off, texel;

            readSourceRGBA((const unsigned char *)pixels + ((size_t)y*width + x)*bytes,
                           format, type, &r, &g, &b, &a);
            texel = packRGBA(r, g, b, a, t->guFormat);

            off = t->swizzled
                ? swizzledOffset(xoffset + x, yoffset + y, t->tbw, bpp)
                : (unsigned int)((size_t)(yoffset + y)*t->tbw + (xoffset + x))*bpp;

            if (bpp == 4) *(unsigned int *)(dst + off) = texel;
            else *(unsigned short *)(dst + off) = (unsigned short)texel;
        }
    }

    pspguWriteback(dst, (unsigned int)(t->tbw * t->padH * bpp));
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
