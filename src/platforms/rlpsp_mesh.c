/**********************************************************************************************
*
*   rlpsp_mesh - the PSP 3D mesh path, quarantined out of raylib's rmodels.c (P10-S25 T1).
*
*   Why this file exists: rmodels.c is upstream raylib, so every PSP line in it is a merge
*   conflict against the next upstream release, and the mesh path - the cull classifier, the
*   merged runs, the baked meshes - was by far the largest block. With it here, rmodels.c keeps
*   the call sites and nothing else.
*
*   The counters and their clock helper stay in rmodels.c, beside the code that reports them;
*   rlpsp_mesh.h declares them for this file. Nothing here is reachable from a desktop build.
*
**********************************************************************************************/

#include "rlpsp_mesh.h"

#if defined(PLATFORM_PSP) && defined(GRAPHICS_API_OPENGL_11)

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <pspkernel.h>
#include <GL/gl.h>

#include "rlgl.h"
#include "raymath.h"

#if defined(PLATFORM_PSP) && defined(GRAPHICS_API_OPENGL_11)

typedef struct {
    unsigned int *key;
    float bmin[3], bmax[3];
} PspGeBounds;

int rlpspCulling = 1;

// P10-S20: the driver exports this as a weak symbol, so a raylib built once links against
// pspgu (which honours it) and pspGL (which does not have it).
void pspSetCleanHint(int on)
{
    if (pspguSetCleanHint != NULL) pspguSetCleanHint(on);
}

void rlpspEnableCulling(int enable) { rlpspCulling = enable; }

#define PSP_GE_MAX 256
static PspGeMesh pspGe[PSP_GE_MAX];
static int pspGeCount = 0;

// P10-S22: the bake was keyed by vboId, so a procedural mesh - never uploaded, which is
// what the showcase and most DrawMesh callers use - never got one. The key is the uploaded
// array when there is one, else the vertex array itself.
// In-place vertex mutation is unsupported on this path: the bake, like the merge bounds
// cache, is computed once and never re-validated. UpdateMeshBuffer() is the one sanctioned
// route and now drops both entries, so a caller that goes through it is safe; a caller that
// writes mesh.vertices directly is not, and must call UnloadMesh() instead.
unsigned int *pspGeKey(Mesh *mesh)
{
    if (mesh == NULL) return NULL;
    if (mesh->vboId != NULL) return mesh->vboId;
    return (unsigned int *)mesh->vertices;
}

PspGeMesh *pspGeFind(unsigned int *key)
{
    if (key == NULL) return NULL;
    for (int i = 0; i < pspGeCount; i++) if (pspGe[i].key == key) return &pspGe[i];
    return NULL;
}

void pspGeRemove(unsigned int *key)
{
    for (int i = 0; i < pspGeCount; i++)
    {
        if (pspGe[i].key != key) continue;
        free(pspGe[i].verts);
        pspGe[i] = pspGe[pspGeCount - 1];
        pspGeCount--;
        return;
    }
}

// Packing is for the shipping driver only. pspGL does not render the same interleaved
// GL_SHORT layout (it produced no pixels in the 061 A/B), and the reference build must keep
// working from the same libraylib.a: ask the driver once and fall back to float arrays.
static int pspGeAvailable(void)
{
    static int cached = -1;
    if (cached < 0)
    {
        const char *vendor = (const char *)glGetString(GL_VENDOR);
        if (vendor == NULL) return 0;   // before GL init: ask again later
        cached = (strstr(vendor, "pspgu") != NULL);
    }
    return cached;
}

#define PSP_GE_BOUNDS_MAX 256
static PspGeBounds pspBounds[PSP_GE_BOUNDS_MAX];
static int pspBoundsCount = 0;

static PspGeBounds *pspGeFindBounds(unsigned int *key)
{
    if (key == NULL) return NULL;
    for (int i = 0; i < pspBoundsCount; i++) if (pspBounds[i].key == key) return &pspBounds[i];
    return NULL;
}

void pspGeStoreBounds(Mesh *mesh, bool dynamic)
{
    if (dynamic || (mesh->vertices == NULL) || (mesh->vertexCount <= 0) || (mesh->vboId == NULL)) return;
    if (pspGeFindBounds(mesh->vboId) != NULL) return;
    if (pspBoundsCount >= PSP_GE_BOUNDS_MAX) return;

    PspGeBounds *b = &pspBounds[pspBoundsCount];
    b->key = mesh->vboId;
    for (int c = 0; c < 3; c++) { b->bmin[c] = mesh->vertices[c]; b->bmax[c] = mesh->vertices[c]; }
    for (int i = 1; i < mesh->vertexCount; i++)
        for (int c = 0; c < 3; c++)
        {
            float v = mesh->vertices[i*3 + c];
            if (v < b->bmin[c]) b->bmin[c] = v;
            if (v > b->bmax[c]) b->bmax[c] = v;
        }
    pspBoundsCount++;
}

void pspGeRemoveBounds(unsigned int *key)
{
    for (int i = 0; i < pspBoundsCount; i++)
    {
        if (pspBounds[i].key != key) continue;
        pspBounds[i] = pspBounds[pspBoundsCount - 1];
        pspBoundsCount--;
        return;
    }
}

static void mat4Mul(float out[16], const float a[16], const float b[16])
{
    for (int c = 0; c < 4; c++)
        for (int r = 0; r < 4; r++)
            out[c*4 + r] = a[0*4+r]*b[c*4+0] + a[1*4+r]*b[c*4+1] +
                           a[2*4+r]*b[c*4+2] + a[3*4+r]*b[c*4+3];
}

#if defined(PLATFORM_PSP)
// pspgu-only accelerators. Weak on purpose: a pspGL build resolves both to NULL and the
// cull stays scalar, so the A/B links against either driver.
extern int pspguMat4MulVfpu(float *out, const float *a, const float *b) __attribute__((weak));
extern int pspguTransformPointsVfpu(const float *m, const float *in, float *out, int count) __attribute__((weak));
static float pspCullP[16], pspCullV[16], pspCullPV[16];
static int pspCullPVValid = 0;
#endif

// Margins for the guard-band classification (see pspGeClassifyMesh). The clip-space Z
// range is compressed by the perspective divide, so the near/far margins are absolute
// fractions of w, not NDC percentages: at distance 24 with near 0.01, w-z is already
// 8e-4 w, and a 5 % margin excluded the whole scene (measured: 278 of 278 submitted
// instances classified dirty before this correction). The constants are needed by every
// build because the classification runs whether or not merging is compiled in.
#define PSP_MERGE_CLEAN_XY 7.9f     // inside pspgu's 8.0 guard band, with float slack
#define PSP_MERGE_CLEAN_NEAR 0.001f // w + z >= 0.1 % of w: only geometry at the eye is out
#define PSP_MERGE_CLEAN_FAR  0.00001f // w - z >= 1e-5 w: the float-order slack at the far plane

#if defined(PLATFORM_PSP) && defined(GRAPHICS_API_OPENGL_11) && defined(PSP_MERGE_DRAWS) && (PSP_MERGE_DRAWS == 1)
// The cube scene (and any DrawMesh on a mesh that was never uploaded) has no AABB in the
// cull table. Merging needs one, and the per-instance test must stay exact, so compute it
// lazily here, keyed by the vertex array. The cull does not use these entries: pulling
// un-uploaded meshes into the cull would change the pre-merge baseline this sprint is
// measured against.
#define PSP_MERGE_BOUNDS_MAX 128
static PspGeBounds pspMergeBounds[PSP_MERGE_BOUNDS_MAX];
static int pspMergeBoundsCount = 0;

static PspGeBounds *pspMergeBoundsFor(Mesh *mesh)
{
    unsigned int *key;

    if ((mesh->vertices == NULL) || (mesh->vertexCount <= 0)) return NULL;

    key = (unsigned int *)mesh->vertices;
    for (int i = 0; i < pspMergeBoundsCount; i++)
        if (pspMergeBounds[i].key == key) return &pspMergeBounds[i];

    if (pspMergeBoundsCount >= PSP_MERGE_BOUNDS_MAX) return NULL;

    PspGeBounds *b = &pspMergeBounds[pspMergeBoundsCount];

    b->key = key;
    for (int c = 0; c < 3; c++) { b->bmin[c] = mesh->vertices[c]; b->bmax[c] = mesh->vertices[c]; }
    for (int i = 1; i < mesh->vertexCount; i++)
        for (int c = 0; c < 3; c++)
        {
            float v = mesh->vertices[i*3 + c];
            if (v < b->bmin[c]) b->bmin[c] = v;
            if (v > b->bmax[c]) b->bmax[c] = v;
        }

    pspMergeBoundsCount++;
    return b;
}
#endif

// Object classification, not just a cull: 0 = all 8 corners outside one NDC plane (skip),
// 1 = the whole AABB is inside the guard band (its vertices cannot need the clipper),
// 2 = drawn but crossing a guard-band plane (or no AABB: stay on the per-object path).
// The six clip planes are linear in clip coordinates, so testing the 8 corners proves the
// property for every vertex of the mesh -- that is what makes a merged run safe.
int pspGeClassifyMesh(Mesh *mesh, Matrix transform)
{
    if (!rlpspCulling) return 2;

    PspGeBounds *b = pspGeFindBounds(mesh->vboId);
#if defined(PLATFORM_PSP) && defined(GRAPHICS_API_OPENGL_11) && defined(PSP_MERGE_DRAWS) && (PSP_MERGE_DRAWS == 1)
    int fromUpload = (b != NULL);
    if (b == NULL) b = pspMergeBoundsFor(mesh);
#else
    int fromUpload = 1;
#endif
    if (b == NULL) return 2;

#if defined(PSP_MERGE_DIAG)
    {
        static int diag = 0;
        if (!diag)
        {
            diag = 1;
            TRACELOG(RL_LOG_INFO, "MERGE diag vbo=%p uploadBounds=%d mergeBounds=%d culling=%d",
                     (void *)mesh->vboId, fromUpload, pspMergeBoundsCount, rlpspCulling);
        }
    }
#endif

    GLfloat P[16], V[16];
    glGetFloatv(GL_PROJECTION_MATRIX, P);
    glGetFloatv(GL_MODELVIEW_MATRIX, V);
    if (P[11] == 0.0f) return 2;                 // ortho/2D: not a 3D frustum

    float M[16];
    memcpy(M, MatrixToFloat(transform), sizeof(M));

    // P*V changes at most once per frame and M per mesh, so cache the composed pair by
    // value (two 64-byte compares) and pay one multiply per draw instead of two.
    if (!pspCullPVValid || (memcmp(P, pspCullP, sizeof(P)) != 0) || (memcmp(V, pspCullV, sizeof(V)) != 0))
    {
        mat4Mul(pspCullPV, P, V);
        memcpy(pspCullP, P, sizeof(P));
        memcpy(pspCullV, V, sizeof(V));
        pspCullPVValid = 1;
    }

    float MVP[16];

    if ((pspguMat4MulVfpu == NULL) || !pspguMat4MulVfpu(MVP, pspCullPV, M)) mat4Mul(MVP, pspCullPV, M);

    float c[8][4] = {
        { b->bmin[0], b->bmin[1], b->bmin[2], 1.0f }, { b->bmax[0], b->bmin[1], b->bmin[2], 1.0f },
        { b->bmin[0], b->bmax[1], b->bmin[2], 1.0f }, { b->bmax[0], b->bmax[1], b->bmin[2], 1.0f },
        { b->bmin[0], b->bmin[1], b->bmax[2], 1.0f }, { b->bmax[0], b->bmin[1], b->bmax[2], 1.0f },
        { b->bmin[0], b->bmax[1], b->bmax[2], 1.0f }, { b->bmax[0], b->bmax[1], b->bmax[2], 1.0f }
    };

    float transformed[32] __attribute__((aligned(16)));
    int haveClip = (pspguTransformPointsVfpu != NULL) &&
                   pspguTransformPointsVfpu(MVP, (const float *)c, transformed, 8);

    int allLeft = 1, allRight = 1, allDown = 1, allUp = 1, allNear = 1, allFar = 1;
    int clean = 1;
    for (int i = 0; i < 8; i++)
    {
        float x, y, z, w;

        if (haveClip)
        {
            x = transformed[i*4 + 0]; y = transformed[i*4 + 1];
            z = transformed[i*4 + 2]; w = transformed[i*4 + 3];
        }
        else
        {
            x = MVP[0]*c[i][0] + MVP[4]*c[i][1] + MVP[8]*c[i][2] + MVP[12];
            y = MVP[1]*c[i][0] + MVP[5]*c[i][1] + MVP[9]*c[i][2] + MVP[13];
            z = MVP[2]*c[i][0] + MVP[6]*c[i][1] + MVP[10]*c[i][2] + MVP[14];
            w = MVP[3]*c[i][0] + MVP[7]*c[i][1] + MVP[11]*c[i][2] + MVP[15];
        }

        if (!(x < -w)) allLeft = 0;
        if (!(x >  w)) allRight = 0;
        if (!(y < -w)) allDown = 0;
        if (!(y >  w)) allUp = 0;
        if (!(z < -w)) allNear = 0;
        if (!(z >  w)) allFar = 0;

        // Guard-band test, with a margin: the cull composes (P*V)*M and the clipper goes
        // through modelview and projection separately, so the two float orders disagree by
        // a hair near the plane. Inside 7.9w / +-0.95w the clipper cannot disagree with us,
        // and the GE's own band (8.53 NDC) covers the rest.
        if ((w <= 0.0f) ||
            !(PSP_MERGE_CLEAN_XY*w + x >= 0.0f) || !(PSP_MERGE_CLEAN_XY*w - x >= 0.0f) ||
            !(PSP_MERGE_CLEAN_XY*w + y >= 0.0f) || !(PSP_MERGE_CLEAN_XY*w - y >= 0.0f) ||
            !(z + PSP_MERGE_CLEAN_NEAR*w >= 0.0f) || !((1.0f - PSP_MERGE_CLEAN_FAR)*w - z >= 0.0f))
        {
            clean = 0;
#if defined(PSP_MERGE_DIAG)
            {
                static int diagFails = 0;
                if (diagFails < 3)
                {
                    diagFails++;
                    TRACELOG(RL_LOG_INFO, "MERGE fail corner=%d nx=%.6f ny=%.6f nz=%.6f w=%.4f",
                             i, (w != 0.0f)? x/w : 0.0f, (w != 0.0f)? y/w : 0.0f,
                             (w != 0.0f)? z/w : 0.0f, w);
                }
            }
#endif
        }
    }

    if (fromUpload && (allLeft || allRight || allDown || allUp || allNear || allFar))
    {
        static unsigned int culled = 0;
        culled++;
        if ((culled % 1000) == 1) TRACELOG(RL_LOG_INFO, "CULL total=%u", culled);
        rlpspClassCull++;
        return 0;
    }

    if (clean) rlpspClassClean++; else rlpspClassDirty++;
    return clean? 1 : 2;
}

//------------------------------------------------------------------------------------
// Sprint 078 (P10-S11): merged clean runs
//------------------------------------------------------------------------------------
// DrawMesh appends a clean instance -- its AABB is inside the guard band, so none of its
// vertices can need the clipper -- to a run with the same mesh, texture and material
// colour. At flush the run is expanded into world space and drawn with one call. The
// camera's modelview is not touched (the per-object path multiplies the model on top of
// it; here the model is already in the vertices), so the GE applies the same view and
// projection it applies per object. Expansion is float32 where the per-object path lets
// the GE transform in float24, so pixel parity is measured, not assumed (T4).
#if defined(PLATFORM_PSP) && defined(GRAPHICS_API_OPENGL_11) && defined(PSP_MERGE_DRAWS) && (PSP_MERGE_DRAWS == 1)

#ifndef GL_VERTEX_ARRAY
    #define GL_VERTEX_ARRAY         0x8074
    #define GL_NORMAL_ARRAY         0x8075
    #define GL_TEXTURE_COORD_ARRAY  0x8078
#endif

#define PSP_MERGE_MAX_VERTS 8192
#define PSP_MERGE_MAX_IDX   24576
#define PSP_MERGE_MAX_INST  1024
#define PSP_MERGE_STRIDE    32          // uv 0, normal 8, position 20 (GU's order, no colour)
#define PSP_MERGE_CLEAN     1u

extern int pspguDrawMergedArrays(GLenum mode, GLsizei indexCount, const GLvoid *indices,
                                 GLsizei vertexCount, const GLvoid *verts,
                                 unsigned int flags) __attribute__((weak));
extern int pspguArenaRoom(void) __attribute__((weak));

typedef struct {
    Mesh mesh;
    unsigned int tex;
    unsigned int color;
    int valid;
    int instances;
    Matrix xf[PSP_MERGE_MAX_INST];
    float view[16];                     // modelview at the first instance
} PspMergeRun;

static PspMergeRun pspMergeRun;
static unsigned char pspMergeVerts[PSP_MERGE_MAX_VERTS*PSP_MERGE_STRIDE];
static unsigned short pspMergeIdx[PSP_MERGE_MAX_IDX];

static int pspMergeActive(void)
{
    static int cached = -1;

    if (cached < 0) cached = (pspguDrawMergedArrays != NULL)? 1 : 0;

    return cached;
}

static unsigned int pspMergeTex(Material *material)
{
    return (unsigned int)material->maps[MATERIAL_MAP_DIFFUSE].texture.id;
}

static unsigned int pspMergeColor(Material *material)
{
    Color c = material->maps[MATERIAL_MAP_DIFFUSE].color;

    return (unsigned int)c.r | ((unsigned int)c.g << 8) | ((unsigned int)c.b << 16) | ((unsigned int)c.a << 24);
}

// Eligibility for the run, minus the key: the classification already proved the instance
// clean, which also proves an AABB existed. Everything here is a precondition for
// reproducing the per-object transform exactly.
int pspMergeEligible(Mesh *mesh, Material *material, Matrix transform)
{
    const float *f;
    float sx, sy, sz;

    if (!pspMergeActive() || !rlpspCulling) return 0;
    if ((mesh->vertices == NULL) || (mesh->indices == NULL)) return 0;
    if ((mesh->texcoords == NULL) || (mesh->normals == NULL)) return 0;
    if ((mesh->colors != NULL) || (mesh->animVertices != NULL) || (mesh->animNormals != NULL)) return 0;
    if ((mesh->vertexCount <= 0) || (mesh->triangleCount <= 0)) return 0;
    if ((mesh->vertexCount > PSP_MERGE_MAX_VERTS/2) || (mesh->triangleCount*3 > PSP_MERGE_MAX_IDX/2)) return 0;

    // Translation + rotation + uniform scale only: the normal is rotated by the basis
    // vectors, which is exact only when the three lengths agree.
    f = MatrixToFloat(transform);
    sx = f[0]*f[0] + f[1]*f[1] + f[2]*f[2];
    sy = f[4]*f[4] + f[5]*f[5] + f[6]*f[6];
    sz = f[8]*f[8] + f[9]*f[9] + f[10]*f[10];
    if ((sx <= 1e-8f) || (sy <= 1e-8f) || (sz <= 1e-8f)) return 0;
    if ((fabsf(sx - sy) > 0.01f*sx) || (fabsf(sx - sz) > 0.01f*sx)) return 0;
    if (fabsf(f[0]*f[4] + f[1]*f[5] + f[2]*f[6]) > 0.01f*sx) return 0;
    if (fabsf(f[0]*f[8] + f[1]*f[9] + f[2]*f[10]) > 0.01f*sx) return 0;
    if (fabsf(f[4]*f[8] + f[5]*f[9] + f[6]*f[10]) > 0.01f*sx) return 0;

    (void)material;
    return 1;
}

static int pspMergeKeyMatches(Mesh *mesh, unsigned int tex, unsigned int color)
{
    PspMergeRun *r = &pspMergeRun;

    return r->valid && (r->mesh.vertices == mesh->vertices) && (r->mesh.indices == mesh->indices) &&
           (r->tex == tex) && (r->color == color);
}

// One more instance must fit both our buffers and the driver's arena: the extension copies
// the run into the per-list arena, and a run that does not fit is replayed per object.
int pspMergeRoom(Mesh *mesh)
{
    PspMergeRun *r = &pspMergeRun;
    int vc = mesh->vertexCount, ic = mesh->triangleCount*3;
    unsigned int need = (unsigned int)vc*PSP_MERGE_STRIDE + (unsigned int)ic*2 + 128;

    if (r->instances >= PSP_MERGE_MAX_INST) return 0;
    if (((r->instances + 1)*vc) > PSP_MERGE_MAX_VERTS) return 0;
    if (((r->instances + 1)*ic) > PSP_MERGE_MAX_IDX) return 0;
    if ((pspguArenaRoom != NULL) && (pspguArenaRoom() < (int)need)) return 0;

    return 1;
}

void rlpspMergeFlush(void);     // defined below; declared in rlgl.h for the flush hooks

void pspMergeAppend(Mesh *mesh, Material *material, Matrix transform)
{
    PspMergeRun *r = &pspMergeRun;
    unsigned int tex = pspMergeTex(material), color = pspMergeColor(material);

    if (!pspMergeKeyMatches(mesh, tex, color))
    {
        rlpspMergeFlush();
        r->valid = 1;
        r->instances = 0;
        r->mesh = *mesh;
        r->tex = tex;
        r->color = color;
        glGetFloatv(GL_MODELVIEW_MATRIX, r->view);
    }

    r->xf[r->instances++] = transform;
}

static void pspMergeBuild(Mesh *mesh, int vc, int ic)
{
    PspMergeRun *r = &pspMergeRun;
    unsigned char *v = pspMergeVerts;

    // P10-S26 T2: the merged index run depends only on (ic, vc, mesh->indices) - never on
    // the instance transforms - so a frame whose run key and instance count are unchanged
    // can keep the buffer built last time. The prefix for k < instances is identical; a
    // larger run is rebuilt because its tail was never written.
#ifndef PSP_MERGE_IDX_REUSE
    #define PSP_MERGE_IDX_REUSE 1
#endif
#if PSP_MERGE_IDX_REUSE
    static int lastIc = -1, lastVc = -1, lastInstances = -1;
    static const unsigned short *lastSrc = NULL;
    static unsigned int idxSkips = 0;

    int rebuildIdx = !((ic == lastIc) && (vc == lastVc) && (mesh->indices == lastSrc) &&
                       (r->instances <= lastInstances));

    if (rebuildIdx)
    {
        lastIc = ic; lastVc = vc; lastSrc = mesh->indices; lastInstances = r->instances;
    }
    else if (++idxSkips == 400)
    {
        TRACELOG(LOG_INFO, "PSP: merged index run reused %u frames in a row (ic=%d)", idxSkips, ic);
    }
#else
    int rebuildIdx = 1;
#endif

#ifndef PSP_MERGE_FAST_NORMAL
    #define PSP_MERGE_FAST_NORMAL 0
#endif

    for (int k = 0; k < r->instances; k++)
    {
        const float *f = MatrixToFloat(r->xf[k]);
        const float *p = mesh->vertices, *n = mesh->normals, *uv = mesh->texcoords;
#if PSP_MERGE_FAST_NORMAL
        // pspMergeEligible proved uniform scale and an orthogonal basis, so for a unit normal
        // |M*n| = sqrt(sx). One reciprocal per instance replaces a sqrtf per vertex - but only
        // when the basis is uniform to 0.1 %; the key tolerates 1 %, and inside that band the
        // per-vertex normalize stays the exact branch.
        float sx = f[0]*f[0] + f[1]*f[1] + f[2]*f[2];
        float sy = f[4]*f[4] + f[5]*f[5] + f[6]*f[6];
        float sz = f[8]*f[8] + f[9]*f[9] + f[10]*f[10];
        int fastNormal = (sx > 1e-8f) &&
                         (fabsf(sx - sy) <= 0.001f*sx) && (fabsf(sx - sz) <= 0.001f*sx);
        float ninv = fastNormal? (1.0f/sqrtf(sx)) : 1.0f;
#endif

#if PSP_MERGE_FAST_NORMAL
        // Two loop variants rather than a per-vertex branch: the branch cost 2.3 ms/frame at
        // 400 cubes when it sat inside the loop.
        if (fastNormal)
        {
            for (int j = 0; j < vc; j++, v += PSP_MERGE_STRIDE)
            {
                float nx = n[j*3 + 0], ny = n[j*3 + 1], nz = n[j*3 + 2];
                float *o = (float *)v;

                o[0] = uv[j*2 + 0];
                o[1] = uv[j*2 + 1];
                o[2] = (nx*f[0] + ny*f[4] + nz*f[8])*ninv;
                o[3] = (nx*f[1] + ny*f[5] + nz*f[9])*ninv;
                o[4] = (nx*f[2] + ny*f[6] + nz*f[10])*ninv;
                o[5] = p[j*3 + 0]*f[0] + p[j*3 + 1]*f[4] + p[j*3 + 2]*f[8] + f[12];
                o[6] = p[j*3 + 0]*f[1] + p[j*3 + 1]*f[5] + p[j*3 + 2]*f[9] + f[13];
                o[7] = p[j*3 + 0]*f[2] + p[j*3 + 1]*f[6] + p[j*3 + 2]*f[10] + f[14];
            }
        }
        else
#endif
        {
            for (int j = 0; j < vc; j++, v += PSP_MERGE_STRIDE)
            {
                float nx = n[j*3 + 0], ny = n[j*3 + 1], nz = n[j*3 + 2];
                float wx = nx*f[0] + ny*f[4] + nz*f[8];
                float wy = nx*f[1] + ny*f[5] + nz*f[9];
                float wz = nx*f[2] + ny*f[6] + nz*f[10];
                float len = sqrtf(wx*wx + wy*wy + wz*wz);
                float *o = (float *)v;

                if (len > 1e-8f) { len = 1.0f/len; wx *= len; wy *= len; wz *= len; }

                o[0] = uv[j*2 + 0];
                o[1] = uv[j*2 + 1];
                o[2] = wx; o[3] = wy; o[4] = wz;
                o[5] = p[j*3 + 0]*f[0] + p[j*3 + 1]*f[4] + p[j*3 + 2]*f[8] + f[12];
                o[6] = p[j*3 + 0]*f[1] + p[j*3 + 1]*f[5] + p[j*3 + 2]*f[9] + f[13];
                o[7] = p[j*3 + 0]*f[2] + p[j*3 + 1]*f[6] + p[j*3 + 2]*f[10] + f[14];
            }
        }

        if (rebuildIdx)
            for (int j = 0; j < ic; j++) pspMergeIdx[k*ic + j] = (unsigned short)(mesh->indices[j] + k*vc);
    }
}

// Replay for the case the extension declines (arena full, older driver): the same generic
// path DrawMesh would have taken, one instance at a time.
static void pspMergeReplay(Mesh *mesh, int vc, int ic)
{
    PspMergeRun *r = &pspMergeRun;

    if (r->tex > 0) rlEnableTexture(r->tex); else rlDisableTexture();
    glColor4ub((unsigned char)(r->color & 0xFF), (unsigned char)((r->color >> 8) & 0xFF),
               (unsigned char)((r->color >> 16) & 0xFF), (unsigned char)((r->color >> 24) & 0xFF));

    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(3, GL_FLOAT, 0, mesh->vertices);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glTexCoordPointer(2, GL_FLOAT, 0, mesh->texcoords);
    glEnableClientState(GL_NORMAL_ARRAY);
    glNormalPointer(GL_FLOAT, 0, mesh->normals);

    for (int k = 0; k < r->instances; k++)
    {
        rlPushMatrix();
        rlMultMatrixf(MatrixToFloat(r->xf[k]));
        if (mesh->indices != NULL) rlDrawVertexArrayElements(0, ic, mesh->indices);
        else rlDrawVertexArray(0, vc);
        rlPopMatrix();
    }

    rlDisableStatePointer(GL_VERTEX_ARRAY);
    rlDisableStatePointer(GL_TEXTURE_COORD_ARRAY);
    rlDisableStatePointer(GL_NORMAL_ARRAY);
    rlDisableTexture();
}

void rlpspMergeFlush(void)
{
    PspMergeRun *r = &pspMergeRun;
    int vc, ic, merged = 0;
    float mv[16];

    if (!r->valid || (r->instances <= 0)) { r->valid = 0; r->instances = 0; return; }

    vc = (int)r->mesh.vertexCount;
    ic = r->mesh.triangleCount*3;

#if defined(PSP_MERGE_DIAG)
    {
        static int lg = 0;
        if (lg < 6) { lg++; TRACELOG(RL_LOG_INFO, "MERGE run colour=%08x instances=%d", r->color, r->instances); }
    }
#endif

    // The run's vertices are world space; drawing them under a different modelview than the
    // one the instances were submitted under would move every instance in the run.
    glGetFloatv(GL_MODELVIEW_MATRIX, mv);

    if ((memcmp(mv, r->view, sizeof(mv)) == 0) && (pspguDrawMergedArrays != NULL))
    {
#if defined(PSP_CALL_PROBE)
        SceInt64 _b0 = rlpspProbeNow();
#endif
        pspMergeBuild(&r->mesh, vc, ic);
#if defined(PSP_CALL_PROBE)
        rlpspMergeBuildUs += (unsigned long long)(rlpspProbeNow() - _b0);
        SceInt64 _f0 = rlpspProbeNow();
#endif

        if (r->tex > 0) rlEnableTexture(r->tex); else rlDisableTexture();
        // glColor4ub, not rlColor4ub: under PSP_BATCH the latter only stores the 2D batch's
        // colour and never reaches the GE, so the run would draw in whatever global colour
        // the last 2D flush left behind (measured: every material rendered white).
        glColor4ub((unsigned char)(r->color & 0xFF), (unsigned char)((r->color >> 8) & 0xFF),
                   (unsigned char)((r->color >> 16) & 0xFF), (unsigned char)((r->color >> 24) & 0xFF));

        glEnableClientState(GL_VERTEX_ARRAY);
        glVertexPointer(3, GL_FLOAT, PSP_MERGE_STRIDE, pspMergeVerts + 20);
        glEnableClientState(GL_TEXTURE_COORD_ARRAY);
        glTexCoordPointer(2, GL_FLOAT, PSP_MERGE_STRIDE, pspMergeVerts + 0);
        glEnableClientState(GL_NORMAL_ARRAY);
        glNormalPointer(GL_FLOAT, PSP_MERGE_STRIDE, pspMergeVerts + 8);

#if defined(PSP_MERGE_NO_HINT)
        merged = pspguDrawMergedArrays(GL_TRIANGLES, r->instances*ic, pspMergeIdx,
                                       r->instances*vc, pspMergeVerts, 0);
#else
        merged = pspguDrawMergedArrays(GL_TRIANGLES, r->instances*ic, pspMergeIdx,
                                       r->instances*vc, pspMergeVerts, PSP_MERGE_CLEAN);
#endif

#if defined(PSP_CALL_PROBE)
        rlpspMergeFlushUs += (unsigned long long)(rlpspProbeNow() - _f0);
        if (++rlpspMergeProbeCount == 200)
        {
            TRACELOG(LOG_INFO, "MERGEX build=%.1f us flush=%.1f us per run (n=%u)",
                     (double)rlpspMergeBuildUs/200.0, (double)rlpspMergeFlushUs/200.0, rlpspMergeProbeCount);
            rlpspMergeBuildUs = 0; rlpspMergeFlushUs = 0; rlpspMergeProbeCount = 0;
        }
#endif

        rlDisableStatePointer(GL_VERTEX_ARRAY);
        rlDisableStatePointer(GL_TEXTURE_COORD_ARRAY);
        rlDisableStatePointer(GL_NORMAL_ARRAY);
        rlDisableTexture();
    }

    if (!merged)
    {
        pspMergeReplay(&r->mesh, vc, ic);
        rlpspMergeReplays++;
    }
    else { rlpspMergeRuns++; rlpspMergeInstances += (unsigned int)r->instances; }

    r->valid = 0;
    r->instances = 0;
}
#endif

void pspGePack(Mesh *mesh, bool dynamic)
{
    if (!pspGeAvailable()) return;
    if (dynamic || (mesh->animVertices != NULL) || (mesh->animNormals != NULL)) return;
    if (mesh->colors != NULL) return;      // baked colour lives in the float path's colour array
    if ((mesh->vertices == NULL) || (mesh->texcoords == NULL) || (mesh->normals == NULL)) return;
    if ((mesh->vertexCount <= 0) || (pspGeKey(mesh) == NULL)) return;
    if ((pspGeCount >= PSP_GE_MAX) || (pspGeFind(pspGeKey(mesh)) != NULL)) return;

    float minx = mesh->vertices[0], maxx = minx;
    float miny = mesh->vertices[1], maxy = miny;
    float minz = mesh->vertices[2], maxz = minz;
    for (int i = 1; i < mesh->vertexCount; i++)
    {
        float x = mesh->vertices[i*3 + 0], y = mesh->vertices[i*3 + 1], z = mesh->vertices[i*3 + 2];
        if (x < minx) minx = x; if (x > maxx) maxx = x;
        if (y < miny) miny = y; if (y > maxy) maxy = y;
        if (z < minz) minz = z; if (z > maxz) maxz = z;
    }

    float cx = (minx + maxx)*0.5f, cy = (miny + maxy)*0.5f, cz = (minz + maxz)*0.5f;
    float hx = (maxx - minx)*0.5f, hy = (maxy - miny)*0.5f, hz = (maxz - minz)*0.5f;
    float half = hx; if (hy > half) half = hy; if (hz > half) half = hz;
    if (half <= 0.0f) return;

    float K = 32767.0f/half;
    unsigned int stride = 28;
    unsigned char *verts = (unsigned char *)calloc((size_t)mesh->vertexCount, stride);
    if (verts == NULL) return;

    for (int i = 0; i < mesh->vertexCount; i++)
    {
        unsigned char *o = verts + (size_t)i*stride;
        float *uv = (float *)(o + 0);
        float *n  = (float *)(o + 8);
        short *p  = (short *)(o + 20);

        uv[0] = mesh->texcoords[i*2 + 0];
        uv[1] = mesh->texcoords[i*2 + 1];
        // The model matrix carries the uniform quantisation scale S. The GE's normal
        // transform is the inverse-transpose, so a unit normal would come out divided by
        // S; pre-multiplying by S keeps it unit and the lit result matches the float path.
        float S = 32768.0f/K;
        n[0] = mesh->normals[i*3 + 0]*S;
        n[1] = mesh->normals[i*3 + 1]*S;
        n[2] = mesh->normals[i*3 + 2]*S;
        p[0] = (short)lrintf((mesh->vertices[i*3 + 0] - cx)*K);
        p[1] = (short)lrintf((mesh->vertices[i*3 + 1] - cy)*K);
        p[2] = (short)lrintf((mesh->vertices[i*3 + 2] - cz)*K);
    }

    sceKernelDcacheWritebackRange(verts, (unsigned int)mesh->vertexCount*stride);

    pspGe[pspGeCount].key = pspGeKey(mesh);
    pspGe[pspGeCount].verts = verts;
    pspGe[pspGeCount].stride = stride;
    pspGe[pspGeCount].cx = cx; pspGe[pspGeCount].cy = cy; pspGe[pspGeCount].cz = cz;
    pspGe[pspGeCount].K = K;
    {
        // Scale(32768/K) * Translate(centre), stored once so a draw composes one matrix.
        float sc = 32768.0f/K;
        float *mm = pspGe[pspGeCount].model;
        for (int i = 0; i < 16; i++) mm[i] = 0.0f;
        mm[0] = sc; mm[5] = sc; mm[10] = sc; mm[15] = 1.0f;
        mm[12] = sc*cx; mm[13] = sc*cy; mm[14] = sc*cz;
    }
    pspGe[pspGeCount].bmin[0] = minx; pspGe[pspGeCount].bmin[1] = miny; pspGe[pspGeCount].bmin[2] = minz;
    pspGe[pspGeCount].bmax[0] = maxx; pspGe[pspGeCount].bmax[1] = maxy; pspGe[pspGeCount].bmax[2] = maxz;
    pspGeCount++;
    TRACELOG(LOG_INFO, "PSP: baked mesh verts=%d key=%p stride=%u K=%.1f",
             mesh->vertexCount, (void *)pspGe[pspGeCount-1].key, stride, K);
}
#endif


// Sprint 070 (P10-S8): measurement-only attribution for matrix_probe MODE=scene.
// The switch is off in every shipped build; when on it adds two syscalls per segment.
// Sprint 078 (P10-S11) adds the call-level pair under PSP_CALL_PROBE: whole DrawMesh and
// the driver entry, so the rlgl state block is the difference and needs no timer of its own.
#if defined(PLATFORM_PSP) && (defined(PSP_MATRIX_PROBE) || defined(PSP_CALL_PROBE))
    #include <pspkernel.h>
    SceInt64 rlpspProbeNow(void) { return sceKernelGetSystemTimeWide(); }
#endif
#if defined(PLATFORM_PSP) && defined(PSP_MATRIX_PROBE)
    unsigned long long rlpspProbeComposeUs;
    unsigned long long rlpspProbeMultUs;
    unsigned long long rlpspProbeCullUs;
    unsigned int rlpspProbeDraws;
#endif
#if defined(PLATFORM_PSP) && defined(PSP_CALL_PROBE)
    unsigned long long rlpspProbeDrawMeshUs;
    unsigned long long rlpspProbeDriverUs;
    // P10-S26 T4: the merge path bypasses DrawMesh's per-object part, so it needs its own
    // pair: the expansion (pspMergeBuild) and the flush (state + driver copy + draw).
    unsigned long long rlpspMergeBuildUs, rlpspMergeFlushUs;
    unsigned int rlpspMergeProbeCount;
    unsigned long long rlpspProbeStateUs;     // texture enable + pointer setup (T4a's target)

    // Sprint 091 T1: the counters existed from 078 but nothing printed them, so the split
    // could not be read. One line per 2,000 calls, then reset: per-call averages of the
    // whole DrawMesh, the cull, the driver call, and the residual (rlgl/raylib surface).
    unsigned int rlpspProbeCalls;

    void rlpspProbeReport(void)
    {
        unsigned int n = (rlpspProbeCalls > 0)? rlpspProbeCalls : 1;
        double total = (double)rlpspProbeDrawMeshUs/n;
        double cull = (double)rlpspProbeCullUs/n;
        double driver = (double)rlpspProbeDriverUs/n;

        TraceLog(LOG_INFO, "CALLPROBE calls=%u drawmesh=%.1f us cull=%.1f driver=%.1f self=%.1f compose=%.1f mult=%.1f state=%.1f",
                 rlpspProbeCalls, total, cull, driver, total - cull - driver,
                 (double)rlpspProbeComposeUs/n, (double)rlpspProbeMultUs/n,
                 (double)rlpspProbeStateUs/n);

        rlpspProbeDrawMeshUs = 0; rlpspProbeDriverUs = 0; rlpspProbeCullUs = 0;
        rlpspProbeComposeUs = 0; rlpspProbeMultUs = 0; rlpspProbeStateUs = 0;
        rlpspProbeDraws = 0; rlpspProbeCalls = 0;
    }

    // Cost of one clock pair, so the attribution can subtract it: every measured line is
    // inflated by the same syscall, and at 500 calls/frame that is milliseconds, not noise.
    unsigned long long rlpspProbeTail(int n)
    {
        SceInt64 t0 = rlpspProbeNow();
        unsigned long long acc = 0;

        for (int i = 0; i < n; i++)
        {
            SceInt64 a = rlpspProbeNow();
            acc += (unsigned long long)(a & 0);
        }

        return (unsigned long long)(rlpspProbeNow() - t0) + acc;
    }
#endif
#if defined(PLATFORM_PSP)
    // Sprint 078 counters: always present so a test build can print them whether or not the
    // merge engine was compiled in (they stay zero in a merge-off or pspGL build).
    unsigned int rlpspMergeRuns;        // merged draws emitted
    unsigned int rlpspMergeInstances;   // instances that went into them
    unsigned int rlpspMergeReplays;     // runs replayed per object (driver declined)
    unsigned int rlpspClassClean;
    unsigned int rlpspClassDirty;
    unsigned int rlpspClassCull;
#endif
#endif
