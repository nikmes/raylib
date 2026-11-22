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

// The GU format decides both the fields present and their byte offsets. This is the
// single place the layout lives: texcoord, colour, normal, position, no padding.
typedef struct {
    unsigned int stride;
    unsigned int offTex, offCol, offNrm, offVtx;
    int vshort;                         // vertex field is signed 16-bit
} PspGuLayout;

// The converting path understands float texcoords/normals/positions and u8 colours.
// Anything else is rejected once and the draw is skipped: reinterpreting bytes as floats
// is how a silent wrong image is produced.
static int arraysSupported(void)
{
    static int logged = 0;
    int ok = (arrVertex.type == GL_FLOAT) || (arrVertex.type == GL_SHORT);

    if (pspgu.clientBits & PSPGU_CS_TEXCOORD) ok = ok && (arrTexCoord.type == GL_FLOAT);
    if (pspgu.clientBits & PSPGU_CS_NORMAL)   ok = ok && (arrNormal.type == GL_FLOAT);
    if (pspgu.clientBits & PSPGU_CS_COLOR)    ok = ok && (arrColor.type == GL_UNSIGNED_BYTE) && (arrColor.size == 4);

    if (!ok && !logged)
    {
        pspguLog("unsupported array types: vertex=0x%x tex=0x%x normal=0x%x colour=0x%x size=%d; draw skipped",
                 arrVertex.type, arrTexCoord.type, arrNormal.type, arrColor.type, arrColor.size);
        logged = 1;
    }

    return ok;
}

// Field offsets and stride implied by the format bits. guVertexFormat() only emits
// GU_TEXTURE_32BITF, GU_COLOR_8888, GU_NORMAL_32BITF and GU_VERTEX_32BITF, so the widths
// are fixed here. The order is GU's, not the application's.
static void vertexLayout(unsigned int fmt, PspGuLayout *l)
{
    unsigned int off = 0;

    l->offTex = off; if (fmt & GU_TEXTURE_BITS) off += 8;
    l->offCol = off; if (fmt & GU_COLOR_BITS)   off += 4;
    l->offNrm = off; if (fmt & GU_NORMAL_BITS)  off += 12;
    l->offVtx = off;
    l->vshort = ((fmt & GU_VERTEX_BITS) == GU_VERTEX_16BIT);
    off += l->vshort? 6 : 12;
    l->stride = (off + 3) & ~3u;        // f32 fields in the next vertex must stay aligned
}

// Does the application already hold one GE-ordered interleaved buffer? Then pspgu can
// hand the pointer to GU instead of packing. Every enabled array must share the stride and
// sit at the layout offset from the same base, with the type the format implies.
static int isDirectLayout(unsigned int fmt, const PspGuLayout *l)
{
    const unsigned char *base;

    if (!(pspgu.clientBits & PSPGU_CS_VERTEX) || (arrVertex.ptr == NULL)) return 0;
    if (!(fmt & GU_VERTEX_BITS)) return 0;
    if (l->vshort != (arrVertex.type == GL_SHORT)) return 0;
    if (arrVertex.stride != (int)l->stride) return 0;

    base = arrVertex.ptr - l->offVtx;

    if (pspgu.clientBits & PSPGU_CS_TEXCOORD)
    {
        if (arrTexCoord.ptr != (const void *)(base + l->offTex)) return 0;
        if (arrTexCoord.stride != (int)l->stride) return 0;
        if (arrTexCoord.type != GL_FLOAT) return 0;
    }

    if (pspgu.clientBits & PSPGU_CS_COLOR)
    {
        if (arrColor.ptr != (const void *)(base + l->offCol)) return 0;
        if (arrColor.stride != (int)l->stride) return 0;
        if ((arrColor.type != GL_UNSIGNED_BYTE) || (arrColor.size != 4)) return 0;
    }

    if (pspgu.clientBits & PSPGU_CS_NORMAL)
    {
        if (arrNormal.ptr != (const void *)(base + l->offNrm)) return 0;
        if (arrNormal.stride != (int)l->stride) return 0;
        if (arrNormal.type != GL_FLOAT) return 0;
    }

    return 1;
}

// One application vertex -> one GU vertex, only the fields the format advertises
// A format without a colour field takes its colour from the GE's global colour register.
// GL11 says that is the last glColor*, which DrawMesh sets from the material. Without this
// the register keeps whatever GU was initialised with (transparent black), so a correctly
// packed mesh draws nothing.
static void applyGlobalColor(unsigned int fmt)
{
    unsigned int col;

    if (fmt & GU_COLOR_BITS) return;            // per-vertex colour wins

    col = ((unsigned int)pspgu.ca << 24) | ((unsigned int)pspgu.cb << 16) |
          ((unsigned int)pspgu.cg << 8) | (unsigned int)pspgu.cr;

    if (pspgu.lighting)
    {
        // GE lighting ignores the vertex colour; GL's current colour becomes the model
        // diffuse, which is how DrawMesh's material colour reaches a lit mesh.
        // pspGL overloads the material ambient with glColor when lighting is on; match it
        // so the A/B compares like state rather than two different conventions.
        sceGuModelColor(pspgu.matEmissive, col, pspgu.matDiffuse, pspgu.matSpecular);
        sceGuColor(col);            // fragment alpha/fallback: GU still reads this register
        return;
    }

    sceGuColor(col);
}

static void packVertex(unsigned char *dst, const PspGuLayout *l, int index)
{
    if (pspgu.clientBits & PSPGU_CS_TEXCOORD)
    {
        const float *uv = (const float *)(arrTexCoord.ptr + (arrTexCoord.stride? index*arrTexCoord.stride : index*8));
        float *o = (float *)(dst + l->offTex);
        o[0] = uv[0]; o[1] = uv[1];
    }

    if (pspgu.clientBits & PSPGU_CS_COLOR)
    {
        const unsigned char *c = arrColor.ptr + (arrColor.stride? index*arrColor.stride : index*4);
        unsigned int *o = (unsigned int *)(dst + l->offCol);
        *o = ((unsigned int)c[3] << 24) | ((unsigned int)c[2] << 16) |
             ((unsigned int)c[1] << 8) | (unsigned int)c[0];
    }

    if (pspgu.clientBits & PSPGU_CS_NORMAL)
    {
        const float *n = (const float *)(arrNormal.ptr + (arrNormal.stride? index*arrNormal.stride : index*12));
        float *o = (float *)(dst + l->offNrm);
        o[0] = n[0]; o[1] = n[1]; o[2] = n[2];
    }

    {
        const unsigned char *base = arrVertex.ptr + (arrVertex.stride? (size_t)index*arrVertex.stride : 0);

        if (l->vshort)
        {
            const short *p = (const short *)base;
            short *o = (short *)(dst + l->offVtx);
            o[0] = p[0]; o[1] = p[1]; o[2] = (arrVertex.size >= 3)? p[2] : 0;
        }
        else
        {
            const float *p = (const float *)base;
            float *o = (float *)(dst + l->offVtx);
            o[0] = p[0]; o[1] = p[1]; o[2] = (arrVertex.size >= 3)? p[2] : 0.0f;
        }
    }
}

//------------------------------------------------------------------------------------
// Sprint 066: CPU clip + re-tessellation for triangles that leave the screen. The GE
// drops a primitive whole when a vertex leaves its guard band, so any triangle with a
// vertex outside NDC is clipped against the six screen planes and re-fanned.
//------------------------------------------------------------------------------------

static void *arenaAllocHard(unsigned int bytes);

typedef struct {
    float x, y, z;
    float u, v;
    float nx, ny, nz;
    float r, g, b, a;
} ClipV;

static void fetchClipVertex(ClipV *out, int index)
{
    memset(out, 0, sizeof(*out));

    if (pspgu.clientBits & PSPGU_CS_TEXCOORD)
    {
        const float *uv = (const float *)(arrTexCoord.ptr + (arrTexCoord.stride? index*arrTexCoord.stride : index*8));
        out->u = uv[0]; out->v = uv[1];
    }

    if (pspgu.clientBits & PSPGU_CS_NORMAL)
    {
        const float *n = (const float *)(arrNormal.ptr + (arrNormal.stride? index*arrNormal.stride : index*12));
        out->nx = n[0]; out->ny = n[1]; out->nz = n[2];
    }

    if (pspgu.clientBits & PSPGU_CS_COLOR)
    {
        const unsigned char *c = arrColor.ptr + (arrColor.stride? index*arrColor.stride : index*4);
        out->r = c[0]/255.0f; out->g = c[1]/255.0f; out->b = c[2]/255.0f; out->a = c[3]/255.0f;
    }
    else
    {
        out->r = pspgu.cr/255.0f; out->g = pspgu.cg/255.0f;
        out->b = pspgu.cb/255.0f; out->a = pspgu.ca/255.0f;
    }

    if (arrVertex.type == GL_SHORT)
    {
        const short *p = (const short *)(arrVertex.ptr + (arrVertex.stride? (size_t)index*arrVertex.stride : 0));
        out->x = p[0]/32768.0f;
        out->y = p[1]/32768.0f;
        out->z = (arrVertex.size >= 3)? p[2]/32768.0f : 0.0f;
    }
    else
    {
        const float *p = (const float *)(arrVertex.ptr + (arrVertex.stride? (size_t)index*arrVertex.stride : 0));
        out->x = p[0]; out->y = p[1]; out->z = (arrVertex.size >= 3)? p[2] : 0.0f;
    }
}

static void clipTransform(const float m[16], const ClipV *v, float out[4])
{
    out[0] = m[0]*v->x + m[4]*v->y + m[8]*v->z + m[12];
    out[1] = m[1]*v->x + m[5]*v->y + m[9]*v->z + m[13];
    out[2] = m[2]*v->x + m[6]*v->y + m[10]*v->z + m[14];
    out[3] = m[3]*v->x + m[7]*v->y + m[11]*v->z + m[15];
}

static float clipDistance(const float c[4], int plane)
{
    switch (plane)
    {
        case 0: return c[0] + c[3];
        case 1: return c[3] - c[0];
        case 2: return c[1] + c[3];
        case 3: return c[3] - c[1];
        case 4: return c[2] + c[3];
        default: return c[3] - c[2];
    }
}

static ClipV clipLerp(const ClipV *a, const ClipV *b, float t)
{
    ClipV o;
    o.x = a->x + (b->x - a->x)*t;
    o.y = a->y + (b->y - a->y)*t;
    o.z = a->z + (b->z - a->z)*t;
    o.u = a->u + (b->u - a->u)*t;
    o.v = a->v + (b->v - a->v)*t;
    o.nx = a->nx + (b->nx - a->nx)*t;
    o.ny = a->ny + (b->ny - a->ny)*t;
    o.nz = a->nz + (b->nz - a->nz)*t;
    o.r = a->r + (b->r - a->r)*t;
    o.g = a->g + (b->g - a->g)*t;
    o.b = a->b + (b->b - a->b)*t;
    o.a = a->a + (b->a - a->a)*t;
    return o;
}

static int clipPolygon(ClipV *poly, int n, const float mvp[16], ClipV *out)
{
    ClipV a[8], b[8];

    if (n > 8) n = 8;
    memcpy(a, poly, (size_t)n*sizeof(ClipV));

    for (int plane = 0; plane < 6; plane++)
    {
        int m = 0;
        for (int i = 0; i < n; i++)
        {
            ClipV *p = &a[i];
            ClipV *q = &a[(i + 1)%n];
            float cp[4], cq[4];
            clipTransform(mvp, p, cp);
            clipTransform(mvp, q, cq);
            float dp = clipDistance(cp, plane);
            float dq = clipDistance(cq, plane);

            if (dp >= 0.0f) b[m++] = *p;
            if ((dp >= 0.0f) != (dq >= 0.0f))
            {
                float t = dp/(dp - dq);
                b[m++] = clipLerp(p, q, t);
                if (m >= 8) break;
            }
        }
        n = m;
        memcpy(a, b, (size_t)n*sizeof(ClipV));
        if (n < 3) return 0;
    }

    memcpy(out, a, (size_t)n*sizeof(ClipV));
    return n;
}

static void packClipVertex(unsigned char *dst, const PspGuLayout *l, const ClipV *v)
{
    if (pspgu.clientBits & PSPGU_CS_TEXCOORD)
    {
        float *o = (float *)(dst + l->offTex); o[0] = v->u; o[1] = v->v;
    }
    if (pspgu.clientBits & PSPGU_CS_COLOR)
    {
        unsigned int *o = (unsigned int *)(dst + l->offCol);
        *o = ((unsigned int)(v->a*255.0f + 0.5f) << 24) |
             ((unsigned int)(v->b*255.0f + 0.5f) << 16) |
             ((unsigned int)(v->g*255.0f + 0.5f) << 8) |
             ((unsigned int)(v->r*255.0f + 0.5f));
    }
    if (pspgu.clientBits & PSPGU_CS_NORMAL)
    {
        float *o = (float *)(dst + l->offNrm); o[0] = v->nx; o[1] = v->ny; o[2] = v->nz;
    }
    {
        float *o = (float *)(dst + l->offVtx); o[0] = v->x; o[1] = v->y; o[2] = v->z;
    }
}

static int clipNeeds(const unsigned short *idx, int count, int first)
{
    float mvp[16];

    if (!(arrVertex.type == GL_FLOAT || arrVertex.type == GL_SHORT)) return 0;
    pspguGetMVP(mvp);

    for (int i = 0; i < count; i++)
    {
        int index = idx? idx[i] : (first + i);
        ClipV v; float c[4];
        fetchClipVertex(&v, index);
        clipTransform(mvp, &v, c);
        float w = c[3];
        if ((c[0] < -w) || (c[0] > w) || (c[1] < -w) || (c[1] > w) ||
            (c[2] < -w) || (c[2] > w)) return 1;
    }

    return 0;
}

static void drawClipped(unsigned int prim, unsigned int fmt, int count, int first,
                        const unsigned short *idx)
{
    float mvp[16];
    unsigned int cfmt = (fmt & ~GU_VERTEX_BITS) | GU_VERTEX_32BITF;
    PspGuLayout cl;
    const int chunkTris = 256;

    vertexLayout(cfmt, &cl);
    pspguGetMVP(mvp);

    int tris = count/3;
    int t = 0;
    int clipped = 0;

    while (t < tris)
    {
        int nTris = tris - t;
        if (nTris > chunkTris) nTris = chunkTris;

        unsigned int bytes = (unsigned int)nTris*6*cl.stride;
        unsigned char *buf = (unsigned char *)pspguArenaAlloc(bytes);
        if (buf == NULL)
        {
            buf = (unsigned char *)arenaAllocHard(bytes);
            if (buf == NULL) return;
        }

        int outVerts = 0;
        for (int k = 0; k < nTris; k++)
        {
            int base = (t + k)*3;
            ClipV in[3], poly[8];
            for (int v = 0; v < 3; v++)
            {
                int index = idx? idx[base + v] : (first + base + v);
                fetchClipVertex(&in[v], index);
            }

            int n = clipPolygon(in, 3, mvp, poly);
            if (n < 3) continue;

            for (int f = 1; f < n - 1; f++)
            {
                packClipVertex(buf + (size_t)outVerts*cl.stride, &cl, &poly[0]);
                packClipVertex(buf + (size_t)(outVerts + 1)*cl.stride, &cl, &poly[f]);
                packClipVertex(buf + (size_t)(outVerts + 2)*cl.stride, &cl, &poly[f + 1]);
                outVerts += 3;
            }
            clipped++;
        }

        if (outVerts > 0)
        {
            pspguWriteback(buf, (unsigned int)outVerts*cl.stride);
            applyGlobalColor(cfmt);
            sceGuDrawArray(prim, cfmt, outVerts, NULL, buf);   // re-fanned, non-indexed
        }
        t += nTris;
    }

    static int logged = 0;
    if (clipped && (logged < 4))
    {
        pspguLog("clip tris=%d fmt=0x%x stride=%u", clipped, cfmt, cl.stride);
        logged++;
    }
}

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

    fmt |= (arrVertex.type == GL_SHORT)? GU_VERTEX_16BIT : GU_VERTEX_32BITF;

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

void glVertexPointer(GLint size, GLenum type, GLsizei stride, const GLvoid *ptr)
{
    arrVertex.ptr = ptr; arrVertex.size = size; arrVertex.type = type;
    arrVertex.stride = stride? stride : size*((type == GL_SHORT)? 2 : (int)sizeof(float));
}

void glTexCoordPointer(GLint size, GLenum type, GLsizei stride, const GLvoid *ptr)
{
    arrTexCoord.ptr = ptr; arrTexCoord.size = size; arrTexCoord.type = type;
    arrTexCoord.stride = stride? stride : size*((type == GL_SHORT)? 2 : (int)sizeof(float));
}

void glColorPointer(GLint size, GLenum type, GLsizei stride, const GLvoid *ptr)
{
    arrColor.ptr = ptr; arrColor.size = size; arrColor.type = type;
    arrColor.stride = stride? stride : size;
}

void glNormalPointer(GLenum type, GLsizei stride, const GLvoid *ptr)
{
    arrNormal.ptr = ptr; arrNormal.size = 3; arrNormal.type = type;
    arrNormal.stride = stride? stride : 3*((type == GL_BYTE)? 1 : (int)sizeof(float));
}

void glDrawArrays(GLenum mode, GLint first, GLsizei count)
{
    unsigned int prim = guPrimitive(mode);
    unsigned int fmt;
    PspGuLayout layout;

    if (!pspgu.current || (count <= 0) || (prim == 0)) return;

    pspguUploadMatrices();

    fmt = guVertexFormat();
    vertexLayout(fmt, &layout);

    if (!arraysSupported()) return;

    {
        static int traced = 0;
        if (traced < 6)
        {
            const float *p = (const float *)arrVertex.ptr;
            pspguLog("draw mode=%d count=%d fmt=0x%x stride=%u fast=%d pos0=(%.1f,%.1f) col0=%08x",
                     (int)mode, (int)count, fmt, layout.stride, isBatchedLayout(), p[0], p[1],
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
        int chunk = (int)((pspgu.arenaBytes > 64)? ((pspgu.arenaBytes - 64) / layout.stride) : 0);

#ifndef PSPGU_NO_CLIP
        if ((prim == GU_TRIANGLES) && clipNeeds(NULL, count, first))
        {
            drawClipped(prim, fmt, count, first, NULL);
            return;
        }
#endif

        if (isDirectLayout(fmt, &layout))
        {
            static int directLogged = 0;
            const unsigned char *base = (const unsigned char *)arrVertex.ptr - layout.offVtx;

            pspguBeginFrame();
            applyGlobalColor(fmt);
            sceGuDrawArray(prim, fmt, count, NULL, base + (size_t)first*layout.stride);
            if (directLogged < 4)
            {
                pspguLog("draw direct=1 count=%d fmt=0x%x stride=%u", (int)count, fmt, layout.stride);
                directLogged++;
            }
            return;
        }

        if (chunk > PSPGU_MAX_CONVERT) chunk = PSPGU_MAX_CONVERT;
        if (chunk <= 0) return;

        while (done < count)
        {
            int n = count - done;
            if (n > chunk) n = chunk;

            pspguBeginFrame();

            unsigned int bytes = (unsigned int)n*layout.stride;
            unsigned char *out = (unsigned char *)pspguArenaAlloc(bytes);
            while (out == NULL)
            {
                if (!pspguArenaReset()) return;
                out = (unsigned char *)pspguArenaAlloc(bytes);
            }

            for (int i = 0; i < n; i++) packVertex(out + (unsigned int)i*layout.stride, &layout, first + done + i);

            pspguWriteback(out, bytes);
            applyGlobalColor(fmt);
            sceGuDrawArray(prim, fmt, n, NULL, out);

            done += n;
        }
    }
}

// Dedup scratch for index ranges that do not fit the arena. Not read by the GE, so it
// lives in main RAM; keys are index+1 because 0xFFFF is a legal vertex index.
#define PSPGU_DEDUP_SLOTS 4096
static unsigned int dedupKey[PSPGU_DEDUP_SLOTS];
static unsigned short dedupSlot[PSPGU_DEDUP_SLOTS];

static void *arenaAllocHard(unsigned int bytes)
{
    void *p = pspguArenaAlloc(bytes);

    if (p == NULL)
    {
        if (!pspguArenaReset()) return NULL;
        p = pspguArenaAlloc(bytes);
    }

    return p;
}

void glDrawElements(GLenum mode, GLsizei count, GLenum type, const GLvoid *indices)
{
    unsigned int prim = guPrimitive(mode);
    unsigned int fmt;
    unsigned int drawFmt;
    PspGuLayout layout;
    const unsigned short *idx = (const unsigned short *)indices;

    if (!pspgu.current || (count <= 0) || (prim == 0)) return;

    if (type != GL_UNSIGNED_SHORT)
    {
        static int logged = 0;
        if (!logged)
        {
            pspguLog("drawElements: unsupported index type 0x%x; draw skipped", (unsigned)type);
            logged = 1;
        }
        return;
    }

    pspguUploadMatrices();
    fmt = guVertexFormat();
    vertexLayout(fmt, &layout);

    // The index width is part of the GE vertex format. Without it the GE reads the index
    // buffer at the wrong stride and draws a fraction of the triangles.
    drawFmt = fmt | GU_INDEX_16BIT;

    if (!arraysSupported()) return;

    {
        static int traced = 0;
        if (traced < 4)
        {
            pspguLog("drawElements count=%d tex2d=%d bound=%u fmt=0x%x draw=0x%x stride=%u", (int)count,
                     pspgu.texEnabled, pspgu.boundTexture, fmt, drawFmt, layout.stride);
            traced++;
        }
    }

    // Convert only the vertices a chunk refers to, rebased so GU indexes from 0. Two
    // strategies per chunk: convert the whole min..max range when it fits the per-draw
    // budget, otherwise hash the referenced vertices and convert each once. The old code
    // dropped to one index per draw when the range exceeded the chunk, which drew nothing
    // for triangles and cost a draw call per index.
    {
        int done = 0;
        // A draw may only split between whole primitives: 2,046 indices is 682 triangles.
        int chunk = 2048;
        if (prim == GU_TRIANGLES) chunk -= chunk % 3;
        else if ((prim == GU_LINES) || (prim == GU_POINTS)) chunk -= chunk % 2;
        else chunk = count;                     // strips/fans: cannot split without care
        unsigned int budget = (pspgu.arenaBytes > 32768)? (pspgu.arenaBytes - 32768) : pspgu.arenaBytes;
        static int dedupLogged = 0;

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
            int dedup = 0;
            unsigned char *verts = NULL;
            unsigned short *rebased = NULL;

            pspguBeginFrame();

#ifndef PSPGU_NO_CLIP
            if ((prim == GU_TRIANGLES) && clipNeeds(idx + done, n, 0))
            {
                drawClipped(prim, fmt, n, 0, idx + done);
                done += n;
                continue;
            }
#endif

            if (isDirectLayout(fmt, &layout))
            {
                static int directLogged = 0;
                unsigned short *rb = (unsigned short *)pspguArenaAlloc((unsigned int)n*sizeof(unsigned short));
                const unsigned char *base = (const unsigned char *)arrVertex.ptr - layout.offVtx;

                if (rb == NULL)
                {
                    rb = (unsigned short *)arenaAllocHard((unsigned int)n*sizeof(unsigned short));
                    if (rb == NULL) return;
                }

                for (int i = 0; i < n; i++) rb[i] = idx[done + i] - lo;
                pspguWriteback(rb, (unsigned int)n*sizeof(unsigned short));
                applyGlobalColor(fmt);
                sceGuDrawArray(prim, drawFmt, n, rb,
                               base + (size_t)lo*layout.stride);

                if (directLogged < 4)
                {
                    pspguLog("drawElements direct=1 n=%d lo=%d fmt=0x%x stride=%u",
                             n, lo, drawFmt, layout.stride);
                    directLogged++;
                }

                done += n;
                continue;
            }

            if (((unsigned int)range*layout.stride + (unsigned int)n*2 + 64) <= budget)
            {
                verts = (unsigned char *)pspguArenaAlloc((unsigned int)range*layout.stride);
                rebased = (unsigned short *)pspguArenaAlloc((unsigned int)n*sizeof(unsigned short));
                if ((verts == NULL) || (rebased == NULL)) dedup = 1;
            }
            else dedup = 1;

            if (!dedup)
            {
                for (int i = 0; i < range; i++) packVertex(verts + (unsigned int)i*layout.stride, &layout, lo + i);
                for (int i = 0; i < n; i++) rebased[i] = idx[done + i] - lo;

                pspguWriteback(verts, (unsigned int)range*layout.stride);
                pspguWriteback(rebased, (unsigned int)n*sizeof(unsigned short));
                applyGlobalColor(fmt);
                sceGuDrawArray(prim, drawFmt, n, rebased, verts);
            }
            else
            {
                int unique = 0;

                if (verts == NULL) verts = (unsigned char *)pspguArenaAlloc((unsigned int)n*layout.stride);
                if (rebased == NULL) rebased = (unsigned short *)pspguArenaAlloc((unsigned int)n*sizeof(unsigned short));
                if ((verts == NULL) || (rebased == NULL))
                {
                    verts = (unsigned char *)arenaAllocHard((unsigned int)n*layout.stride);
                    rebased = (unsigned short *)arenaAllocHard((unsigned int)n*sizeof(unsigned short));
                    if ((verts == NULL) || (rebased == NULL)) return;
                }

                memset(dedupKey, 0, sizeof(dedupKey));

                for (int i = 0; i < n; i++)
                {
                    unsigned int v = idx[done + i];
                    unsigned int h = v & (PSPGU_DEDUP_SLOTS - 1);

                    while ((dedupKey[h] != 0) && (dedupKey[h] != (v + 1)))
                        h = (h + 1) & (PSPGU_DEDUP_SLOTS - 1);

                    if (dedupKey[h] == 0)
                    {
                        dedupKey[h] = v + 1;
                        dedupSlot[h] = (unsigned short)unique;
                        packVertex(verts + (unsigned int)unique*layout.stride, &layout, (int)v);
                        unique++;
                    }

                    rebased[i] = dedupSlot[h];
                }

                pspguWriteback(verts, (unsigned int)unique*layout.stride);
                pspguWriteback(rebased, (unsigned int)n*sizeof(unsigned short));
                applyGlobalColor(fmt);
                sceGuDrawArray(prim, drawFmt, n, rebased, verts);

                if (dedupLogged < 8)
                {
                    pspguLog("drawElements dedup n=%d range=%d unique=%d stride=%u",
                             n, range, unique, layout.stride);
                    dedupLogged++;
                }
            }

            done += n;
        }
    }
}
