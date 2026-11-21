/*
 * gl_matrix.c - the GL matrix stack, on the CPU, pushed to the GE.
 *
 * Task 4 of phase 5. raylib's GL11 path uses glOrtho once per viewport change, glPush/Pop
 * around BeginMode3D and DrawMesh, and glGetFloatv to read both matrices back - so the
 * stack has to live in memory (a GE-only implementation cannot answer the query), and the
 * composed result is pushed with sceGuSetMatrix.
 *
 * Convention: the stack is GL column-major (translation in m[12..14]). GU wants row-major
 * with translation in the fourth row, i.e. the transpose, which is what pspguUpload does.
 */

#include <math.h>
#include <string.h>

#include "pspgu_internal.h"

#define PSPGU_MATRIX_STACK 16       // raylib nests two or three deep; 16 costs nothing

typedef struct {
    float m[16];                    // column-major, GL convention
} PspGuMatrix;

static PspGuMatrix stackProjection[PSPGU_MATRIX_STACK];
static PspGuMatrix stackModelview[PSPGU_MATRIX_STACK];
static int depthProjection, depthModelview;
static int currentMode = GL_MODELVIEW;

// Which matrices the GE has been told about, so an unchanged one is not re-sent
static PspGuMatrix sentProjection, sentModelview;
static int sentProjectionValid = 0, sentModelviewValid = 0;

static PspGuMatrix *currentStack(void)
{
    return (currentMode == GL_PROJECTION)? stackProjection : stackModelview;
}

static int *currentDepth(void)
{
    return (currentMode == GL_PROJECTION)? &depthProjection : &depthModelview;
}

static PspGuMatrix *currentMatrix(void)
{
    return &currentStack()[*currentDepth()];
}

// GL column-major -> the GE's matrix, which is also column-major: ScePspFMatrix4's fields
// are filled in memory order, x.x first. Transposing here is what made a raw full-screen
// sprite collapse to a few pixels around the screen centre - the ortho's translation, sent
// as a row instead of a column, became a w row and every vertex was divided by it.
static void toGuMatrix(const PspGuMatrix *in, ScePspFMatrix4 *out)
{
    memcpy(out, in->m, sizeof(in->m));
}

// Send a matrix to the GE if it changed since the last send
static void upload(PspGuMatrix *m, PspGuMatrix *sent, int *valid, int guType)
{
    ScePspFMatrix4 gu;

    if (!pspgu.current) return;
    if (*valid && (memcmp(m->m, sent->m, sizeof(m->m)) == 0)) return;

    toGuMatrix(m, &gu);

    pspguBeginFrame();
    sceGuSetMatrix(guType, &gu);

    {
        static int traced = 0;
        if (traced < 2)
        {
            pspguLog("matrix type=%d gl=[%.4f %.4f %.4f %.4f | %.4f %.4f %.4f %.4f | %.4f %.4f %.4f %.4f | %.4f %.4f %.4f %.4f]",
                     guType, m->m[0], m->m[1], m->m[2], m->m[3], m->m[4], m->m[5], m->m[6], m->m[7],
                     m->m[8], m->m[9], m->m[10], m->m[11], m->m[12], m->m[13], m->m[14], m->m[15]);
            traced++;
        }
    }

    *sent = *m;
    *valid = 1;
}

static PspGuMatrix *currentProjection(void) { return &stackProjection[depthProjection]; }
static PspGuMatrix *currentModelview(void)  { return &stackModelview[depthModelview]; }

// Called once when the GE comes up: both stacks start as identity, which zeroed statics
// are not. A zeroed matrix would collapse every vertex onto the origin.
void pspguResetMatrices(void)
{
    memset(stackProjection, 0, sizeof(stackProjection));
    memset(stackModelview, 0, sizeof(stackModelview));
    for (int i = 0; i < PSPGU_MATRIX_STACK; i++)
    {
        stackProjection[i].m[0] = stackProjection[i].m[5] = stackProjection[i].m[10] = stackProjection[i].m[15] = 1.0f;
        stackModelview[i].m[0] = stackModelview[i].m[5] = stackModelview[i].m[10] = stackModelview[i].m[15] = 1.0f;
    }
    depthProjection = depthModelview = 0;
    currentMode = GL_MODELVIEW;
    sentProjectionValid = sentModelviewValid = 0;
}

// Called by the draw path: the GE needs both matrices before it can transform anything
void pspguUploadMatrices(void)
{
    upload(currentProjection(), &sentProjection, &sentProjectionValid, GU_PROJECTION);
    upload(currentModelview(), &sentModelview, &sentModelviewValid, GU_MODEL);
}

void glMatrixMode(GLenum mode)
{
    if ((mode == GL_PROJECTION) || (mode == GL_MODELVIEW)) currentMode = mode;
    // GL_TEXTURE has no GE equivalent and nothing in the 2D path uses it
}

void glLoadIdentity(void)
{
    PspGuMatrix *m = currentMatrix();

    memset(m->m, 0, sizeof(m->m));
    m->m[0] = m->m[5] = m->m[10] = m->m[15] = 1.0f;
}

void glPushMatrix(void)
{
    PspGuMatrix *stack = currentStack();
    int *depth = currentDepth();

    if ((*depth + 1) >= PSPGU_MATRIX_STACK) return;     // silently drop, as GL11 does on overflow

    stack[*depth + 1] = stack[*depth];
    (*depth)++;
}

void glPopMatrix(void)
{
    int *depth = currentDepth();

    if (*depth <= 0) return;
    (*depth)--;
}

// result = current * operand, both column-major
static void multiply(PspGuMatrix *result, const float *a, const float *b)
{
    float out[16];

    for (int c = 0; c < 4; c++)
    {
        for (int r = 0; r < 4; r++)
        {
            out[c*4 + r] = a[0*4 + r]*b[c*4 + 0] + a[1*4 + r]*b[c*4 + 1] +
                           a[2*4 + r]*b[c*4 + 2] + a[3*4 + r]*b[c*4 + 3];
        }
    }

    memcpy(result->m, out, sizeof(out));
}

void glMultMatrixf(const GLfloat *matf)
{
    PspGuMatrix out;

    if (matf == NULL) return;
    multiply(&out, currentMatrix()->m, matf);
    *currentMatrix() = out;
}

void glOrtho(GLdouble left, GLdouble right, GLdouble bottom, GLdouble top,
             GLdouble znear, GLdouble zfar)
{
    float m[16];
    float rl = (float)(right - left);
    float tb = (float)(top - bottom);
    float fn = (float)(zfar - znear);
    PspGuMatrix out;

    memset(m, 0, sizeof(m));
    m[0] = 2.0f/rl;
    m[5] = 2.0f/tb;
    m[10] = -2.0f/fn;
    m[12] = -(float)(right + left)/rl;
    m[13] = -(float)(top + bottom)/tb;
    m[14] = -(float)(zfar + znear)/fn;
    m[15] = 1.0f;

    multiply(&out, currentMatrix()->m, m);
    *currentMatrix() = out;
}

void glFrustum(GLdouble left, GLdouble right, GLdouble bottom, GLdouble top,
               GLdouble znear, GLdouble zfar)
{
    float m[16];
    float rl = (float)(right - left);
    float tb = (float)(top - bottom);
    float fn = (float)(zfar - znear);
    PspGuMatrix out;

    memset(m, 0, sizeof(m));
    m[0] = 2.0f*(float)znear/rl;
    m[5] = 2.0f*(float)znear/tb;
    m[8] = (float)(right + left)/rl;
    m[9] = (float)(top + bottom)/tb;
    m[10] = -(float)(zfar + znear)/fn;
    m[11] = -1.0f;
    m[14] = -2.0f*(float)zfar*(float)znear/fn;

    multiply(&out, currentMatrix()->m, m);
    *currentMatrix() = out;
}

void glTranslatef(GLfloat x, GLfloat y, GLfloat z)
{
    PspGuMatrix out;

    memcpy(&out, currentMatrix(), sizeof(out));
    out.m[12] += out.m[0]*x + out.m[4]*y + out.m[8]*z;
    out.m[13] += out.m[1]*x + out.m[5]*y + out.m[9]*z;
    out.m[14] += out.m[2]*x + out.m[6]*y + out.m[10]*z;
    out.m[15] += out.m[3]*x + out.m[7]*y + out.m[11]*z;
    *currentMatrix() = out;
}

void glScalef(GLfloat x, GLfloat y, GLfloat z)
{
    PspGuMatrix *m = currentMatrix();

    for (int r = 0; r < 4; r++) m->m[0*4 + r] *= x;
    for (int r = 0; r < 4; r++) m->m[1*4 + r] *= y;
    for (int r = 0; r < 4; r++) m->m[2*4 + r] *= z;
}

void glRotatef(GLfloat angle, GLfloat x, GLfloat y, GLfloat z)
{
    float c = cosf(angle*(3.14159265358979f/180.0f));
    float s = sinf(angle*(3.14159265358979f/180.0f));
    float len = sqrtf(x*x + y*y + z*z);
    float m[16];
    PspGuMatrix out;

    if (len < 1e-6f) return;
    x /= len; y /= len; z /= len;

    memset(m, 0, sizeof(m));
    m[0]  = x*x*(1.0f - c) + c;      m[1]  = y*x*(1.0f - c) + z*s;   m[2]  = x*z*(1.0f - c) - y*s;
    m[4]  = x*y*(1.0f - c) - z*s;    m[5]  = y*y*(1.0f - c) + c;     m[6]  = y*z*(1.0f - c) + x*s;
    m[8]  = x*z*(1.0f - c) + y*s;    m[9]  = y*z*(1.0f - c) - x*s;   m[10] = z*z*(1.0f - c) + c;
    m[15] = 1.0f;

    multiply(&out, currentMatrix()->m, m);
    *currentMatrix() = out;
}

// raylib reads both matrices back: rlGetMatrixModelview(), rlGetMatrixProjection()
void glGetFloatvMatrices(GLenum pname, GLfloat *params)
{
    const PspGuMatrix *m = NULL;

    switch (pname)
    {
        case GL_MODELVIEW_MATRIX:  m = &stackModelview[depthModelview]; break;
        case GL_PROJECTION_MATRIX: m = &stackProjection[depthProjection]; break;
        default: return;
    }

    memcpy(params, m->m, sizeof(m->m));
}
