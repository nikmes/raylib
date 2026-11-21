/*
 * gl_immediate.c - the immediate-mode entry points, for PSP_BATCH=0 libraries and for the
 * --wrap counter harness.
 *
 * Phase 5 task 12. Two consumers, one reason: `-Wl,--wrap=glBegin` needs the *real* symbol to
 * exist at link time, so without these nothing in psp-tests links with WRAP=1 — and a
 * PSP_BATCH=0 raylib calls glBegin, glVertex and glEnd for every primitive.
 *
 * Design (settled at the sprint 013 review):
 *  - vertices accumulate into the SAME 24-byte interleaved layout the batcher produces, and
 *    are emitted through the same sceGuDrawArray call glDrawArrays uses. A second drawing
 *    implementation would be untested by the 2D gate that protects everything else.
 *  - GL_QUADS is expanded into two triangles: GU has no quad primitive, and raylib's
 *    immediate path draws quads.
 *  - the buffer is finite and its overflow behaviour is defined: emit, then continue.
 */

#include <string.h>

#include "pspgu_internal.h"

#define PSPGU_IMM_MAX 1024              // vertices per glBegin/glEnd run (24 KiB)

typedef struct {
    float u, v;
    unsigned char r, g, b, a;
    float x, y, z;
} PspGuImmVertex;

static PspGuImmVertex immVertices[PSPGU_IMM_MAX];
static PspGuImmVertex immTriangles[PSPGU_IMM_MAX*2];      // quads expand 4 -> 6
static int immCount;
static unsigned int immMode;            // GL primitive
static float immU, immV;
// The current colour lives in pspgu, not here: GL11 says it is what the vertex colour is
// when GL_COLOR_ARRAY is off, which is exactly how DrawMesh draws (it calls rlColor4ub and
// then sends arrays with no colour). A private static here made the array path paint white.
#define immR (pspgu.cr)
#define immG (pspgu.cg)
#define immB (pspgu.cb)
#define immA (pspgu.ca)

static unsigned int immGuPrimitive(void)
{
    switch (immMode)
    {
        case GL_TRIANGLES:      return GU_TRIANGLES;
        case GL_TRIANGLE_STRIP: return GU_TRIANGLE_STRIP;
        case GL_TRIANGLE_FAN:   return GU_TRIANGLE_FAN;
        case GL_LINES:          return GU_LINES;
        case GL_LINE_STRIP:     return GU_LINE_STRIP;
        case GL_POINTS:         return GU_POINTS;
        default:                return 0;
    }
}

// Emit whatever is buffered, converting quads first because GU cannot draw them
static void immFlush(void)
{
    unsigned int prim;
    const void *data = immVertices;
    int count = immCount;

    immCount = 0;
    if (count <= 0) return;

    if (immMode == GL_QUADS)
    {
        int quads = count/4;
        int out = 0;

        for (int q = 0; q < quads; q++)
        {
            const PspGuImmVertex *v = &immVertices[q*4];

            immTriangles[out++] = v[0]; immTriangles[out++] = v[1]; immTriangles[out++] = v[2];
            immTriangles[out++] = v[0]; immTriangles[out++] = v[2]; immTriangles[out++] = v[3];
        }

        if (out == 0) return;           // fewer than four vertices: nothing drawable

        data = immTriangles;
        count = out;
        prim = GU_TRIANGLES;
    }
    else
    {
        prim = immGuPrimitive();
        if (prim == 0) return;
    }

    pspguUploadMatrices();
    pspguWriteback(data, (unsigned int)count*sizeof(PspGuImmVertex));
    pspguBeginFrame();
    sceGuDrawArray(prim, GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF, count, NULL, data);
}

void glBegin(GLenum mode)
{
    if (!pspgu.current) return;

    immFlush();                         // never mix two runs
    immMode = (unsigned int)mode;
    immCount = 0;
}

void glEnd(void)
{
    if (!pspgu.current) return;
    immFlush();
}

void glTexCoord2f(GLfloat s, GLfloat t)
{
    immU = s;
    immV = t;
}

void glColor4ub(GLubyte r, GLubyte g, GLubyte b, GLubyte a)
{
    immR = r; immG = g; immB = b; immA = a;
}

void glColor3f(GLfloat r, GLfloat g, GLfloat b)
{
    immR = (unsigned char)(r*255.0f + 0.5f);
    immG = (unsigned char)(g*255.0f + 0.5f);
    immB = (unsigned char)(b*255.0f + 0.5f);
    immA = 255;
}

void glColor4f(GLfloat r, GLfloat g, GLfloat b, GLfloat a)
{
    immR = (unsigned char)(r*255.0f + 0.5f);
    immG = (unsigned char)(g*255.0f + 0.5f);
    immB = (unsigned char)(b*255.0f + 0.5f);
    immA = (unsigned char)(a*255.0f + 0.5f);
}

void glNormal3f(GLfloat x, GLfloat y, GLfloat z)
{
    // The immediate path carries no normal: the GE format used here has none, and lighting
    // is not part of phase 5. Recorded so nobody assumes it was applied.
    (void)x; (void)y; (void)z;
}

static void immVertex(float x, float y, float z)
{
    PspGuImmVertex *v;

    if (immCount >= PSPGU_IMM_MAX) immFlush();      // defined overflow: emit, then continue

    v = &immVertices[immCount++];
    v->u = immU; v->v = immV;
    v->r = immR; v->g = immG; v->b = immB; v->a = immA;
    v->x = x; v->y = y; v->z = z;
}

void glVertex2f(GLfloat x, GLfloat y) { immVertex(x, y, 0.0f); }
void glVertex2i(GLint x, GLint y)     { immVertex((float)x, (float)y, 0.0f); }
void glVertex3f(GLfloat x, GLfloat y, GLfloat z) { immVertex(x, y, z); }
