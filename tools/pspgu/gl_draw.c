/*
 * gl_draw.c - vertex arrays and draw calls.
 *
 * Task 5 of phase 5. Two paths, because the library uses two:
 *
 *  FAST - the sprint 007-009 batcher hands us one interleaved 24-byte vertex:
 *         texcoord 2x f32, colour 4x u8, position 3x f32. That is exactly the order GU's
 *         vertex format expects, so the pointer goes to sceGuDrawArray unchanged and the
 *         GE reads the batch buffer directly. No copy.
 *  SLOW - everything else (DrawMesh hands over separate, tightly packed arrays): convert
 *         into a scratch buffer in GU order, then draw. pspGL does the same, for the same
 *         reason.
 *
 * Modes: GL_SPRITES_PSP (10) -> GU_SPRITES, GL_TRIANGLES (4) -> GU_TRIANGLES,
 * GL_LINES (1) -> GU_LINES. Sprite mode is what the batcher uses for axis-aligned quads.
 */

#include <string.h>

#include "pspgu_internal.h"

#define PSPGU_MAX_CONVERT (8*1024)      // vertices converted per draw call in the slow path

// GL client arrays as the application set them
typedef struct {
    const unsigned char *ptr;
    GLint size;
    GLenum type;
    GLsizei stride;
} PspGuArray;

static PspGuArray arrVertex, arrTexCoord, arrColor, arrNormal;

// One converted vertex, in GU order: texcoord, colour, normal, position
typedef struct {
    float u, v;
    unsigned int color;                 // 0xAABBGGRR
    float nx, ny, nz;
    float x, y, z;
} PspGuVertex;                          // 32 bytes

// Vertices live in the current list's arena, never in a shared scratch buffer: with two
// lists in flight a single scratch buffer would be rewritten by frame N while frame N-1
// still pointed at it. This is the per-list vertex memory T5 requires.
static void *arenaCopy(const void *src, unsigned int bytes)
{
    unsigned char *dst;

    pspguBeginFrame();

    dst = (unsigned char *)pspguArenaAlloc(bytes);
    while (dst == NULL)
    {
        if (!pspguArenaReset()) return NULL;
        dst = (unsigned char *)pspguArenaAlloc(bytes);
    }

    memcpy(dst, src, bytes);
    pspguWriteback(dst, bytes);

    return dst;
}

//------------------------------------------------------------------------------------
// Sprint 020: textures wider or taller than 512, sampled as full-stride windows.
// A texture is a single VRAM buffer; the draw is split into primitives whose texture
// coordinates lie in one 512x512 window. Contiguous pieces in the same window go out
// in one draw, in primitive order. Textures <=512 never enter this path.
//------------------------------------------------------------------------------------
#define SPLIT_MAX_VERTS 1024

typedef struct {
    float u, v;
    unsigned int color;
    float x, y, z;
} SplitVertex;

typedef struct {
    float tx, ty;               // texel coordinates in the logical image
    float x, y, z;
    unsigned int color;
} SplitPoly;

static SplitVertex splitBuf[SPLIT_MAX_VERTS];
static int splitCount;
static int splitWx = -1, splitWy = -1;
static unsigned int splitPrim, splitFmt;
static int splitUnsupportedLogged;
static unsigned int splitDrawCalls, splitWindowSwitches;

static struct {
    int width, height, tbw, bpp, swizzled;
    unsigned int offset;
} splitTex;

static int splitNextPow2(int v)
{
    int p = 1;
    while (p < v) p <<= 1;
    return p;
}

// pspgu's swizzled block offset (16 bytes x 8 rows) for a logical texel.
static unsigned int splitSwizzledOffset(int x, int y, int tbw, int bpp)
{
    unsigned int widthBytes = (unsigned int)tbw*(unsigned int)bpp;
    unsigned int texelsPerBlock = 16u/(unsigned int)bpp;
    unsigned int block = (unsigned int)((y/8)*(widthBytes/16) + (x/(int)texelsPerBlock))*128;

    return block + (unsigned int)((y%8)*16) +
           (unsigned int)((x%(int)texelsPerBlock)*bpp);
}

static void splitFlush(void)
{
    if (splitCount == 0) return;

    void *dst = arenaCopy(splitBuf, (unsigned int)splitCount*sizeof(SplitVertex));
    if (dst != NULL)
    {
        sceGuDrawArray(splitPrim, splitFmt, splitCount, NULL, dst);
        splitDrawCalls++;
    }
    splitCount = 0;
}

static void splitSetWindow(int wx, int wy)
{
    if ((wx == splitWx) && (wy == splitWy)) return;

    splitFlush();

    int winW = splitTex.width - wx;
    int winH = splitTex.height - wy;
    if (winW > 512) winW = 512;
    if (winH > 512) winH = 512;

    int powW = splitNextPow2(winW);
    int powH = splitNextPow2(winH);
    if (powW > 512) powW = 512;
    if (powH > 512) powH = 512;

    unsigned int tbp = splitTex.swizzled? splitSwizzledOffset(wx, wy, splitTex.tbw, splitTex.bpp)
                                       : (unsigned int)(wy*splitTex.tbw + wx)*splitTex.bpp;
    unsigned int base = (unsigned int)sceGeEdramGetAddr() + splitTex.offset;

    sceGuTexImage(0, powW, powH, splitTex.tbw, (void *)(base + tbp));
    sceGuTexScale(1.0f, 1.0f);

    splitWx = wx;
    splitWy = wy;
    splitWindowSwitches++;
}

static void splitEmit(const SplitVertex *v, int n)
{
    if ((splitCount + n) > SPLIT_MAX_VERTS) splitFlush();

    memcpy(&splitBuf[splitCount], v, (size_t)n*sizeof(SplitVertex));
    splitCount += n;
}

static unsigned int splitLerpColor(unsigned int a, unsigned int b, float t)
{
    unsigned int out = 0;

    for (int shift = 0; shift < 32; shift += 8)
    {
        int ca = (int)((a >> shift) & 0xFF);
        int cb = (int)((b >> shift) & 0xFF);
        int c = ca + (int)((cb - ca)*t + 0.5f);

        if (c < 0) c = 0;
        if (c > 255) c = 255;
        out |= (unsigned int)c << shift;
    }

    return out;
}

static SplitPoly splitLerp(const SplitPoly *a, const SplitPoly *b, float t)
{
    SplitPoly p;

    p.tx = a->tx + (b->tx - a->tx)*t;
    p.ty = a->ty + (b->ty - a->ty)*t;
    p.x = a->x + (b->x - a->x)*t;
    p.y = a->y + (b->y - a->y)*t;
    p.z = a->z + (b->z - a->z)*t;
    p.color = splitLerpColor(a->color, b->color, t);

    return p;
}

// Sutherland-Hodgman against one axis-aligned half-plane in texel space.
static int splitClipEdge(const SplitPoly *in, int n, SplitPoly *out, int axis, float limit, int keepGreater)
{
    int m = 0;

    for (int i = 0; i < n; i++)
    {
        const SplitPoly *a = &in[i];
        const SplitPoly *b = &in[(i + 1)%n];
        float ta = axis? a->ty : a->tx;
        float tb = axis? b->ty : b->tx;
        int ina = keepGreater? (ta >= limit) : (ta <= limit);
        int inb = keepGreater? (tb >= limit) : (tb <= limit);

        if (ina) out[m++] = *a;

        if (ina != inb)
        {
            float den = ta - tb;
            float t = (den != 0.0f)? (ta - limit)/den : 0.0f;
            out[m++] = splitLerp(a, b, t);
        }
    }

    return m;
}

static void splitSprite(const SplitVertex *a, const SplitVertex *b)
{
    float txa = a->u*(float)splitTex.width;
    float txb = b->u*(float)splitTex.width;
    float tya = a->v*(float)splitTex.height;
    float tyb = b->v*(float)splitTex.height;

    if ((txb <= txa) || (tyb <= tya)) return;
    if (txa < 0.0f) txa = 0.0f;
    if (tya < 0.0f) tya = 0.0f;
    if (txb > (float)splitTex.width) txb = (float)splitTex.width;
    if (tyb > (float)splitTex.height) tyb = (float)splitTex.height;

    float spanX = txb - txa;
    float spanY = tyb - tya;
    if ((spanX <= 0.0f) || (spanY <= 0.0f)) return;

    for (int wy = (int)(tya/512.0f)*512; (float)wy < tyb; wy += 512)
    {
        for (int wx = (int)(txa/512.0f)*512; (float)wx < txb; wx += 512)
        {
            int winW = splitTex.width - wx;
            int winH = splitTex.height - wy;
            if (winW > 512) winW = 512;
            if (winH > 512) winH = 512;

            float sx0 = (txa > (float)wx)? txa : (float)wx;
            float sx1 = (txb < (float)(wx + winW))? txb : (float)(wx + winW);
            float sy0 = (tya > (float)wy)? tya : (float)wy;
            float sy1 = (tyb < (float)(wy + winH))? tyb : (float)(wy + winH);

            if ((sx1 <= sx0) || (sy1 <= sy0)) continue;

            splitSetWindow(wx, wy);

            int powW = splitNextPow2(winW);
            int powH = splitNextPow2(winH);
            if (powW > 512) powW = 512;
            if (powH > 512) powH = 512;

            float fx0 = (sx0 - txa)/spanX;
            float fx1 = (sx1 - txa)/spanX;
            float fy0 = (sy0 - tya)/spanY;
            float fy1 = (sy1 - tya)/spanY;

            SplitVertex v[2];
            v[0].u = (sx0 - (float)wx)/(float)powW;
            v[0].v = (sy0 - (float)wy)/(float)powH;
            v[0].color = a->color;
            v[0].x = a->x + (b->x - a->x)*fx0;
            v[0].y = a->y + (b->y - a->y)*fy0;
            v[0].z = a->z + (b->z - a->z)*fy0;

            v[1].u = (sx1 - (float)wx)/(float)powW;
            v[1].v = (sy1 - (float)wy)/(float)powH;
            v[1].color = a->color;
            v[1].x = a->x + (b->x - a->x)*fx1;
            v[1].y = a->y + (b->y - a->y)*fy1;
            v[1].z = a->z + (b->z - a->z)*fy1;

            splitEmit(v, 2);
        }
    }
}

static void splitTriangle(const SplitVertex *v)
{
    SplitPoly poly[3] = {
        { v[0].u*(float)splitTex.width, v[0].v*(float)splitTex.height, v[0].x, v[0].y, v[0].z, v[0].color },
        { v[1].u*(float)splitTex.width, v[1].v*(float)splitTex.height, v[1].x, v[1].y, v[1].z, v[1].color },
        { v[2].u*(float)splitTex.width, v[2].v*(float)splitTex.height, v[2].x, v[2].y, v[2].z, v[2].color }
    };

    float minX = poly[0].tx, maxX = poly[0].tx;
    float minY = poly[0].ty, maxY = poly[0].ty;
    for (int i = 1; i < 3; i++)
    {
        if (poly[i].tx < minX) minX = poly[i].tx;
        if (poly[i].tx > maxX) maxX = poly[i].tx;
        if (poly[i].ty < minY) minY = poly[i].ty;
        if (poly[i].ty > maxY) maxY = poly[i].ty;
    }

    if ((maxX <= minX) || (maxY <= minY)) return;

    for (int wy = (int)(minY/512.0f)*512; (float)wy < maxY; wy += 512)
    {
        for (int wx = (int)(minX/512.0f)*512; (float)wx < maxX; wx += 512)
        {
            int winW = splitTex.width - wx;
            int winH = splitTex.height - wy;
            if (winW > 512) winW = 512;
            if (winH > 512) winH = 512;

            int powW = splitNextPow2(winW);
            int powH = splitNextPow2(winH);
            if (powW > 512) powW = 512;
            if (powH > 512) powH = 512;

            SplitPoly bufA[10], bufB[10];
            for (int i = 0; i < 3; i++) bufA[i] = poly[i];

            int n = splitClipEdge(bufA, 3, bufB, 0, (float)wx, 1);
            if (n == 0) continue;
            n = splitClipEdge(bufB, n, bufA, 0, (float)(wx + winW), 0);
            if (n == 0) continue;
            n = splitClipEdge(bufA, n, bufB, 1, (float)wy, 1);
            if (n == 0) continue;
            n = splitClipEdge(bufB, n, bufA, 1, (float)(wy + winH), 0);
            if (n < 3) continue;

            splitSetWindow(wx, wy);

            for (int k = 1; k + 1 < n; k++)
            {
                SplitVertex out[3];

                const SplitPoly *src[3] = { &bufA[0], &bufA[k], &bufA[k + 1] };
                for (int j = 0; j < 3; j++)
                {
                    out[j].u = (src[j]->tx - (float)wx)/(float)powW;
                    out[j].v = (src[j]->ty - (float)wy)/(float)powH;
                    out[j].color = src[j]->color;
                    out[j].x = src[j]->x;
                    out[j].y = src[j]->y;
                    out[j].z = src[j]->z;
                }

                splitEmit(out, 3);
            }
        }
    }
}

static int drawOversized(unsigned int prim, unsigned int fmt, const SplitVertex *src, int count){
    if ((src == NULL) || (count <= 0)) return 1;

    if ((prim != GU_SPRITES) && (prim != GU_TRIANGLES))
    {
        if (!splitUnsupportedLogged)
        {
            pspguLog("window: lines or another mode with an oversized texture are not supported");
            splitUnsupportedLogged = 1;
        }
        return 1;
    }

    int per = (prim == GU_SPRITES)? 2 : 3;
    if ((count%per) != 0) return 0;         // not a complete primitive run; fall back

    pspguBeginFrame();

    splitPrim = prim;
    splitFmt = fmt;
    splitCount = 0;
    splitWx = splitWy = -1;

    unsigned int d0 = splitDrawCalls, s0 = splitWindowSwitches;

    for (int i = 0; i < count; i += per)
    {
        if (prim == GU_SPRITES) splitSprite(&src[i], &src[i + 1]);
        else splitTriangle(&src[i]);
    }

    splitFlush();

    static int traced;
    if (traced < 3)
    {
        pspguLog("window: prim=%u count=%d draws=%u switches=%u",
                 prim, count, splitDrawCalls - d0, splitWindowSwitches - s0);
        traced++;
    }

    return 1;
}

void pspguWindowStats(unsigned int *draws, unsigned int *switches)
{
    if (draws != NULL) *draws = splitDrawCalls;
    if (switches != NULL) *switches = splitWindowSwitches;
}

static unsigned int guPrimitive(GLenum mode)
{
    switch (mode)
    {
        case GL_TRIANGLES:     return GU_TRIANGLES;
        case GL_LINES:         return GU_LINES;
        case GL_LINE_STRIP:    return GU_LINE_STRIP;
        case GL_TRIANGLE_FAN:  return GU_TRIANGLE_FAN;
        case GL_TRIANGLE_STRIP:return GU_TRIANGLE_STRIP;
        case GL_POINTS:        return GU_POINTS;
        case GL_SPRITES_PSP:   return GU_SPRITES;
        default:               return 0;
    }
}

// The format bits mirror what the application enabled, in GU's order
static unsigned int guVertexFormat(void)
{
    unsigned int fmt = 0;

    if (pspgu.clientBits & PSPGU_CS_TEXCOORD) fmt |= GU_TEXTURE_32BITF;
    if (pspgu.clientBits & PSPGU_CS_COLOR)    fmt |= GU_COLOR_8888;
    if (pspgu.clientBits & PSPGU_CS_NORMAL)   fmt |= GU_NORMAL_32BITF;

    fmt |= GU_VERTEX_32BITF;

    return fmt;
}

// Is this the batcher's own layout? One stride, and the three pointers sit at the field
// offsets of { u,v | rgba | x,y,z }: uv at +0, colour at +8, position at +12.
static int isBatchedLayout(void)
{
    if (!(pspgu.clientBits & PSPGU_CS_VERTEX)) return 0;
    if (arrVertex.stride != 24) return 0;
    if (!(pspgu.clientBits & PSPGU_CS_TEXCOORD) || !(pspgu.clientBits & PSPGU_CS_COLOR)) return 0;
    if (arrTexCoord.stride != 24 || arrColor.stride != 24) return 0;
    if ((arrColor.ptr - arrTexCoord.ptr) != 8) return 0;
    if ((arrVertex.ptr - arrTexCoord.ptr) != 12) return 0;
    if ((arrVertex.type != GL_FLOAT) || (arrTexCoord.type != GL_FLOAT)) return 0;
    if ((arrColor.type != GL_UNSIGNED_BYTE) || (arrColor.size != 4)) return 0;

    return 1;
}

// One application vertex -> one GU vertex, in the slow path
static void convertVertex(PspGuVertex *out, int index)
{
    const unsigned char *base = arrVertex.ptr;   // every array shares the vertex index

    memset(out, 0, sizeof(*out));

    if (pspgu.clientBits & PSPGU_CS_TEXCOORD)
    {
        const float *uv = (const float *)(arrTexCoord.ptr + (arrTexCoord.stride? index*arrTexCoord.stride : index*8));
        out->u = uv[0];
        out->v = uv[1];
    }

    if (pspgu.clientBits & PSPGU_CS_COLOR)
    {
        const unsigned char *c = arrColor.ptr + (arrColor.stride? index*arrColor.stride : index*4);
        out->color = ((unsigned int)c[3] << 24) | ((unsigned int)c[2] << 16) |
                     ((unsigned int)c[1] << 8) | (unsigned int)c[0];
    }
    else
    {
        // No colour array: GL11 takes the colour from the last glColor*, which is how
        // DrawMesh draws (material colour, arrays, no colour array). Hardcoding white here
        // rendered meshes white where pspGL renders the material.
        out->color = ((unsigned int)pspgu.ca << 24) | ((unsigned int)pspgu.cb << 16) |
                     ((unsigned int)pspgu.cg << 8) | (unsigned int)pspgu.cr;
    }

    if (pspgu.clientBits & PSPGU_CS_NORMAL)
    {
        const float *n = (const float *)(arrNormal.ptr + (arrNormal.stride? index*arrNormal.stride : index*12));
        out->nx = n[0]; out->ny = n[1]; out->nz = n[2];
    }

    if (pspgu.clientBits & PSPGU_CS_VERTEX)
    {
        const float *p = (const float *)(base + (arrVertex.stride? index*arrVertex.stride : 0));
        // GL positions arrive as the array the app set, which may be 2 or 3 components
        out->x = p[0];
        out->y = p[1];
        out->z = (arrVertex.size >= 3)? p[2] : 0.0f;
    }
}

void glVertexPointer(GLint size, GLenum type, GLsizei stride, const GLvoid *ptr)
{
    arrVertex.ptr = ptr; arrVertex.size = size; arrVertex.type = type;
    arrVertex.stride = stride? stride : size*(int)sizeof(float);
}

void glTexCoordPointer(GLint size, GLenum type, GLsizei stride, const GLvoid *ptr)
{
    arrTexCoord.ptr = ptr; arrTexCoord.size = size; arrTexCoord.type = type;
    arrTexCoord.stride = stride? stride : size*(int)sizeof(float);
}

void glColorPointer(GLint size, GLenum type, GLsizei stride, const GLvoid *ptr)
{
    arrColor.ptr = ptr; arrColor.size = size; arrColor.type = type;
    arrColor.stride = stride? stride : size;
}

void glNormalPointer(GLenum type, GLsizei stride, const GLvoid *ptr)
{
    arrNormal.ptr = ptr; arrNormal.size = 3; arrNormal.type = type;
    arrNormal.stride = stride? stride : 3*(int)sizeof(float);
}

void glDrawArrays(GLenum mode, GLint first, GLsizei count)
{
    unsigned int prim = guPrimitive(mode);
    unsigned int fmt;

    if (!pspgu.current || (count <= 0) || (prim == 0)) return;

    pspguUploadMatrices();

    fmt = guVertexFormat();

    {
        static int traced = 0;
        if (traced < 6)
        {
            const float *p = (const float *)arrVertex.ptr;
            pspguLog("draw mode=%d count=%d fmt=0x%x fast=%d pos0=(%.1f,%.1f) col0=%08x",
                     (int)mode, (int)count, fmt, isBatchedLayout(), p[0], p[1],
                     (arrColor.ptr)? *(const unsigned int *)(arrColor.ptr) : 0);
            traced++;
        }
    }

    if (isBatchedLayout())
    {
        int tw, th, tpw, tph, tbw, bpp, swz;
        unsigned int toff;

        // Sprint 020: a texture above 512 in either dimension is sampled through
        // 512-pixel windows. The <=512 path below is unchanged.
        if (pspgu.texEnabled &&
            pspguBoundTextureInfo(&tw, &th, &tpw, &tph, &tbw, &bpp, &swz, &toff))
        {
            splitTex.width = tw;
            splitTex.height = th;
            splitTex.tbw = tbw;
            splitTex.bpp = bpp;
            splitTex.swizzled = swz;
            splitTex.offset = toff;

            if (drawOversized(prim, fmt,
                              (const SplitVertex *)(arrTexCoord.ptr + (size_t)first*24),
                              count)) return;
        }

        // Same layout as GU: copy the run into this list's arena, then hand GU the copy.
        // The batcher's own buffer is reused next frame, so a direct pointer would let
        // frame N rewrite vertices that frame N-1's list still reads.
        unsigned int bytes = (unsigned int)count*24;
        void *v = arenaCopy(arrTexCoord.ptr + (size_t)first*24, bytes);

        if (v != NULL) sceGuDrawArray(prim, fmt, count, NULL, v);
    }
    else
    {
        int done = 0;
        int chunk = (int)(pspgu.arenaBytes / sizeof(PspGuVertex));

        if (chunk > PSPGU_MAX_CONVERT) chunk = PSPGU_MAX_CONVERT;

        while (done < count)
        {
            int n = count - done;
            if (n > chunk) n = chunk;

            pspguBeginFrame();

            PspGuVertex *out = (PspGuVertex *)pspguArenaAlloc((unsigned int)n*sizeof(PspGuVertex));
            while (out == NULL)
            {
                if (!pspguArenaReset()) return;
                out = (PspGuVertex *)pspguArenaAlloc((unsigned int)n*sizeof(PspGuVertex));
            }

            for (int i = 0; i < n; i++) convertVertex(&out[i], first + done + i);

            pspguWriteback(out, (unsigned int)n*sizeof(PspGuVertex));
            sceGuDrawArray(prim, fmt, n, NULL, out);

            done += n;
        }
    }
}

void glDrawElements(GLenum mode, GLsizei count, GLenum type, const GLvoid *indices)
{
    unsigned int prim = guPrimitive(mode);
    unsigned int fmt;
    const unsigned short *idx = (const unsigned short *)indices;

    if (!pspgu.current || (count <= 0) || (prim == 0)) return;
    if (type != GL_UNSIGNED_SHORT) return;      // meshes always come indexed by uint16

    pspguUploadMatrices();
    fmt = guVertexFormat();

    {
        static int traced = 0;
        if (traced < 4)
        {
            pspguLog("drawElements count=%d tex2d=%d bound=%u fmt=0x%x", (int)count,
                     pspgu.texEnabled, pspgu.boundTexture, fmt);
            traced++;
        }
    }

    // Convert the vertices a chunk refers to, rebased so GU indexes from 0. The chunk's
    // index range is not necessarily contiguous or sorted, so take min and max.
    {
        int done = 0;

        // 2,048 indices with a 2,048-vertex range is 64 KiB of vertices plus 4 KiB of
        // indices, which always fits one arena even after the 16-byte alignment.
        int chunk = 2048;

        while (done < count)
        {
            int n = count - done;
            unsigned short lo = 0xFFFF, hi = 0;

            if (n > chunk) n = chunk;

            for (int i = 0; i < n; i++)
            {
                if (idx[done + i] < lo) lo = idx[done + i];
                if (idx[done + i] > hi) hi = idx[done + i];
            }

            int range = (int)hi - (int)lo + 1;
            if (range > chunk) { n = 1; range = 1; }   // pathological fan; step one at a time
            if (n <= 0) return;

            pspguBeginFrame();

            PspGuVertex *verts = (PspGuVertex *)pspguArenaAlloc((unsigned int)range*sizeof(PspGuVertex));
            unsigned short *rebased = (unsigned short *)pspguArenaAlloc((unsigned int)n*sizeof(unsigned short));

            while ((verts == NULL) || (rebased == NULL))
            {
                if (!pspguArenaReset()) return;
                verts = (PspGuVertex *)pspguArenaAlloc((unsigned int)range*sizeof(PspGuVertex));
                rebased = (unsigned short *)pspguArenaAlloc((unsigned int)n*sizeof(unsigned short));
            }

            for (int i = 0; i < range; i++) convertVertex(&verts[i], lo + i);
            for (int i = 0; i < n; i++) rebased[i] = idx[done + i] - lo;

            pspguWriteback(verts, (unsigned int)range*sizeof(PspGuVertex));
            pspguWriteback(rebased, (unsigned int)n*sizeof(unsigned short));
            sceGuDrawArray(prim, fmt, n, rebased, verts);

            done += n;
        }
    }
}
