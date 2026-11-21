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

// Scratch for the slow path, in GU order: texcoord, colour, normal, position
typedef struct {
    float u, v;
    unsigned int color;                 // 0xAABBGGRR
    float nx, ny, nz;
    float x, y, z;
} PspGuVertex;                          // 32 bytes

static PspGuVertex scratch[PSPGU_MAX_CONVERT];

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
    else out->color = 0xFFFFFFFF;

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
        // Direct: GU's vertex order is the batch's vertex order
        const unsigned char *v = arrTexCoord.ptr + (size_t)first*24;

        pspguWriteback(v, (unsigned int)count*24);
        pspguBeginFrame();
        sceGuDrawArray(prim, fmt, count, NULL, v);
    }
    else
    {
        int done = 0;

        while (done < count)
        {
            int n = count - done;
            if (n > PSPGU_MAX_CONVERT) n = PSPGU_MAX_CONVERT;

            for (int i = 0; i < n; i++) convertVertex(&scratch[i], first + done + i);

            pspguWriteback(scratch, (unsigned int)n*sizeof(PspGuVertex));
            pspguBeginFrame();
            sceGuDrawArray(prim, fmt, n, NULL, scratch);

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

    // Convert the vertices a chunk refers to, rebased so GU indexes from 0. The chunk's
    // index range is not necessarily contiguous or sorted, so take min and max.
    {
        int done = 0;

        while (done < count)
        {
            int n = count - done;
            unsigned short lo = 0xFFFF, hi = 0;
            unsigned short rebased[PSPGU_MAX_CONVERT];

            if (n > PSPGU_MAX_CONVERT) n = PSPGU_MAX_CONVERT;

            for (int i = 0; i < n; i++)
            {
                if (idx[done + i] < lo) lo = idx[done + i];
                if (idx[done + i] > hi) hi = idx[done + i];
            }

            if ((int)(hi - lo + 1) > PSPGU_MAX_CONVERT) n = 1;   // pathological fan; step one at a time

            for (int i = 0; i <= (int)(hi - lo); i++) convertVertex(&scratch[i], lo + i);
            for (int i = 0; i < n; i++) rebased[i] = idx[done + i] - lo;

            pspguWriteback(scratch, (unsigned int)(hi - lo + 1)*sizeof(PspGuVertex));
            pspguWriteback(rebased, (unsigned int)n*sizeof(unsigned short));
            pspguBeginFrame();
            sceGuDrawArray(prim, fmt, n, rebased, scratch);

            done += n;
        }
    }
}
