/*
 * rlpsp_batch - the PSP batcher, kept out of rlgl.h so upstream merges do not have to
 * read or resolve it. Included by rlgl.h at the point where the batch state must exist;
 * the surrounding #if stays there because that is the condition the rest of rlgl uses.
 *
 * Sprint 095 (P10-S25) T2: moved verbatim, no behaviour change.
 */


#ifndef RLPSP_BATCH_CAP
    #define RLPSP_BATCH_CAP 4096        // Vertices per batch (2,048 sprites), 24 bytes each
#endif
#ifndef RLPSP_ALTERNATION_BUDGET
    #define RLPSP_ALTERNATION_BUDGET 8  // Primitive-class changes per frame before sprites are dropped
#endif

// The GL enums the batcher stores, spelled out: this section of rlgl.h runs before the
// GL headers are included (they come with the implementation, far below). The values are
// fixed by the GL spec and by pspGL's geprim_tab, and platforms/rcore_psp.c asserts at
// compile time that they still match <GL/gl.h>.
#define RLPSP_GL_LINES      0x0001
#define RLPSP_GL_TRIANGLES  0x0004
#define RLPSP_GL_SPRITES    0x000A      // GL_SPRITES_PSP == GL_POLYGON + 1

// Interleaved vertex, 24 B - texcoord 2f, colour 8888, position 3f. That order is what
// the three pointers in rlPspBatchFlush() expect, and it is pspGL's native GE layout.
typedef struct {
    float u, v;                     // Texture coordinates
    unsigned char r, g, b, a;       // Vertex color
    float x, y, z;                  // Position
} RLPspVertex;

typedef struct {
    RLPspVertex verts[RLPSP_BATCH_CAP];
    RLPspVertex quad[4];            // Corners of the quad being assembled
    int count;                      // Vertices buffered
    int mode;                       // GL primitive of the buffered run
    int quads;                      // Current run is RL_QUADS
    int quadN;                      // Corners of the quad being assembled
    int alternations;               // Primitive-class changes in this frame
    unsigned int tex;               // Texture the buffered vertices belong to
    unsigned int pendTex;           // Texture the following vertices will use
    unsigned int boundTex;          // Texture currently bound by the batch (0xFFFFFFFF = unknown)
    float u, v;                     // Current texture coordinate
    unsigned char r, g, b, a;       // Current vertex color
} RLPspBatch;

extern RLPspBatch rlpsp;            // Defined in platforms/rcore_psp.c
void rlPspBatchFlush(void);         // Emit buffered vertices as one draw call
void rlPspBatchFrameEnd(void);      // Emit, then reset the per-frame alternation budget

#define RLPSP_BATCH_ENABLED

// Claim n contiguous slots: settle the pending texture and the capacity first
static inline RLPspVertex *rlPspReserve(int n)
{
    RLPspVertex *v;

    if (rlpsp.pendTex != rlpsp.tex)
    {
        rlPspBatchFlush();
        rlpsp.tex = rlpsp.pendTex;
    }
    if ((rlpsp.count + n) > RLPSP_BATCH_CAP) rlPspBatchFlush();

    v = &rlpsp.verts[rlpsp.count];
    rlpsp.count += n;

    return v;
}

// Select the primitive of the run being buffered, flushing first if it changes
static inline void rlPspSetMode(int mode)
{
    if ((rlpsp.count > 0) && (mode != rlpsp.mode))
    {
        rlPspBatchFlush();
        rlpsp.alternations++;
    }

    rlpsp.mode = mode;
}

// Is this quad a rectangle that is axis-aligned and untransformed, i.e. one GE sprite?
// raylib emits quads as TL, BL, BR, TR, so a sprite is legal when the corners are
// axis-aligned, the texture coordinates follow the same corners in the same direction
// (no mirroring), and all four corners share colour and depth - GE sprites interpolate
// one colour and one depth between the two corners.
static inline int rlPspIsSpriteQuad(const RLPspVertex *q)
{
#ifdef RLPSP_NO_SPRITES
    // Measurement switch: force the two-triangle path at any optimisation level
    (void)q;
    return 0;
#else
    if ((q[0].x != q[1].x) || (q[2].x != q[3].x)) return 0;     // left and right edges vertical
    if ((q[0].y != q[3].y) || (q[1].y != q[2].y)) return 0;     // top and bottom edges horizontal
    if ((q[0].x == q[2].x) || (q[0].y == q[2].y)) return 0;     // degenerate

    // A GE sprite is axis-aligned in SCREEN space. This test runs on the vertex positions
    // as submitted, which in a 3D pass are WORLD coordinates - and a cube face is perfectly
    // axis-aligned there while projecting to a parallelogram on screen. Raylib's 2D always
    // draws at z = 0; anything with depth is 3D and must stay triangles.
    if ((q[0].z != 0.0f) || (q[1].z != 0.0f) || (q[2].z != 0.0f) || (q[3].z != 0.0f)) return 0;

    if (q[1].u != q[0].u) return 0;                             // UVs follow the corners
    if (q[3].u != q[2].u) return 0;
    if (q[3].v != q[0].v) return 0;
    if (q[1].v != q[2].v) return 0;
    if ((q[0].u > q[2].u) || (q[0].v > q[2].v)) return 0;       // no mirrored sprites

    if ((q[0].r != q[1].r) || (q[0].r != q[2].r) || (q[0].r != q[3].r)) return 0;
    if ((q[0].g != q[1].g) || (q[0].g != q[2].g) || (q[0].g != q[3].g)) return 0;
    if ((q[0].b != q[1].b) || (q[0].b != q[2].b) || (q[0].b != q[3].b)) return 0;
    if ((q[0].a != q[1].a) || (q[0].a != q[2].a) || (q[0].a != q[3].a)) return 0;
    if ((q[0].z != q[1].z) || (q[0].z != q[2].z) || (q[0].z != q[3].z)) return 0;

    return 1;
#endif
}

// Batched vertex-level entry points: attributes are collected, primitives are emitted
// by the next flush. Warm path, therefore static inline.
static inline void rlBegin(int mode)
{
    int glMode = 0;

    switch (mode)
    {
        case RL_LINES: glMode = RLPSP_GL_LINES; break;
        case RL_TRIANGLES: glMode = RLPSP_GL_TRIANGLES; break;
        default: break;
    }

    rlpsp.quads = (mode == RL_QUADS)? 1 : 0;
    if (rlpsp.quads == 1) { rlpsp.quadN = 0; return; }      // The primitive is chosen per quad

    if (glMode != 0) rlPspSetMode(glMode);
}

static inline void rlEnd(void)
{
}

static inline void rlTexCoord2f(float x, float y)
{
    rlpsp.u = x;
    rlpsp.v = y;
}

static inline void rlNormal3f(float x, float y, float z)
{
    (void)x; (void)y; (void)z;      // NOTE: lighting is not batched (2D only)
}

static inline void rlColor4ub(unsigned char r, unsigned char g, unsigned char b, unsigned char a)
{
    rlpsp.r = r;
    rlpsp.g = g;
    rlpsp.b = b;
    rlpsp.a = a;
}

static inline void rlColor3f(float x, float y, float z)
{
    rlpsp.r = (unsigned char)(x*255.0f + 0.5f);
    rlpsp.g = (unsigned char)(y*255.0f + 0.5f);
    rlpsp.b = (unsigned char)(z*255.0f + 0.5f);
    rlpsp.a = 255;
}

static inline void rlColor4f(float x, float y, float z, float w)
{
    rlpsp.r = (unsigned char)(x*255.0f + 0.5f);
    rlpsp.g = (unsigned char)(y*255.0f + 0.5f);
    rlpsp.b = (unsigned char)(z*255.0f + 0.5f);
    rlpsp.a = (unsigned char)(w*255.0f + 0.5f);
}

// Deferred on purpose: every raylib draw call is rlSetTexture(T), quad, rlSetTexture(0),
// so binding on the change would emit twice per draw call and batching would measure zero
static inline void rlSetTexture(unsigned int id)
{
    rlpsp.pendTex = id;
}

// Append one vertex, assembling quads and choosing their primitive on the fourth corner
static inline void rlPspPushVertex(float x, float y, float z)
{
    RLPspVertex *q;
    RLPspVertex *v;

    if (rlpsp.quads == 0)
    {
        v = rlPspReserve(1);
        v->u = rlpsp.u;   v->v = rlpsp.v;
        v->r = rlpsp.r;   v->g = rlpsp.g;   v->b = rlpsp.b;   v->a = rlpsp.a;
        v->x = x;         v->y = y;         v->z = z;
        return;
    }

    q = &rlpsp.quad[rlpsp.quadN++];
    q->u = rlpsp.u;   q->v = rlpsp.v;
    q->r = rlpsp.r;   q->g = rlpsp.g;   q->b = rlpsp.b;   q->a = rlpsp.a;
    q->x = x;         q->y = y;         q->z = z;

    if (rlpsp.quadN == 4)
    {
        rlpsp.quadN = 0;

        // One GE sprite if the quad qualifies and this frame has not spent its
        // alternation budget; two triangles otherwise. The budget is what stops a
        // scene that mixes rotated and unrotated sprites from issuing one draw per
        // sprite: 2D order cannot be reordered, so the only lever is to stop paying.
        if ((rlPspIsSpriteQuad(rlpsp.quad) == 1) && (rlpsp.alternations < RLPSP_ALTERNATION_BUDGET))
        {
            rlPspSetMode(RLPSP_GL_SPRITES);
            v = rlPspReserve(2);
            v[0] = rlpsp.quad[0];       // top-left
            v[1] = rlpsp.quad[2];       // bottom-right
        }
        else
        {
            rlPspSetMode(RLPSP_GL_TRIANGLES);
            v = rlPspReserve(6);
            v[0] = rlpsp.quad[0];
            v[1] = rlpsp.quad[1];
            v[2] = rlpsp.quad[2];
            v[3] = rlpsp.quad[0];
            v[4] = rlpsp.quad[2];
            v[5] = rlpsp.quad[3];
        }
    }
}

// P10-S30: the guards on the fast quad call below. 0 restores the twelve-call path, which
// is the A/B control for every number this feature claims.
#ifndef RLPSP_FAST_QUAD
    #define RLPSP_FAST_QUAD 1
#endif

// P10-S30: one axis-aligned textured quad, emitted without the four-corner staging.
// raylib's DrawTexturePro builds the same two vertices through rlBegin/rlVertex2f; this
// takes the four corners it already has and writes the pair once. The triangle fallback
// keeps the alternation budget honest for scenes that mix rotated quads.
static inline void rlpspDrawQuad(unsigned int tex, float u0, float v0, float u1, float v1,
                                 float x0, float y0, float x1, float y1,
                                 unsigned char r, unsigned char g, unsigned char b, unsigned char a)
{
    RLPspVertex *v;

    rlpsp.pendTex = tex;                    // rlPspReserve settles it (and flushes if it changed)

    if (rlpsp.alternations < RLPSP_ALTERNATION_BUDGET)
    {
        rlPspSetMode(RLPSP_GL_SPRITES);
        v = rlPspReserve(2);
        v[0] = (RLPspVertex){ u0, v0, r, g, b, a, x0, y0, 0.0f };
        v[1] = (RLPspVertex){ u1, v1, r, g, b, a, x1, y1, 0.0f };
    }
    else
    {
        rlPspSetMode(RLPSP_GL_TRIANGLES);
        v = rlPspReserve(6);
        v[0] = (RLPspVertex){ u0, v0, r, g, b, a, x0, y0, 0.0f };   // top-left
        v[1] = (RLPspVertex){ u0, v1, r, g, b, a, x0, y1, 0.0f };   // bottom-left
        v[2] = (RLPspVertex){ u1, v1, r, g, b, a, x1, y1, 0.0f };   // bottom-right
        v[3] = v[0];
        v[4] = v[2];
        v[5] = (RLPspVertex){ u1, v0, r, g, b, a, x1, y0, 0.0f };   // top-right
    }
}

static inline void rlVertex3f(float x, float y, float z) { rlPspPushVertex(x, y, z); }
static inline void rlVertex2f(float x, float y) { rlPspPushVertex(x, y, 0.0f); }
static inline void rlVertex2i(int x, int y) { rlPspPushVertex((float)x, (float)y, 0.0f); }

