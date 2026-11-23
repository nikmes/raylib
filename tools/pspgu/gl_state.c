/*
 * gl_state.c - the GL state the library actually sets, mapped onto GU.
 *
 * Task 1 of phase 5: enough to clear the screen and to keep the state that rlglInit
 * establishes from being silently lost. Draw/texture/matrix calls live in their own files
 * and are stubs until their tasks come up.
 */

#include <stdlib.h>

#include "pspgu_internal.h"

// Convert a GL colour (0..1 floats) to the GE's 0xAABBGGRR
static unsigned int guColor(float r, float g, float b, float a)
{
    unsigned int cr = (unsigned int)((r < 0.0f)? 0.0f : (r > 1.0f)? 1.0f : r*255.0f + 0.5f);
    unsigned int cg = (unsigned int)((g < 0.0f)? 0.0f : (g > 1.0f)? 1.0f : g*255.0f + 0.5f);
    unsigned int cb = (unsigned int)((b < 0.0f)? 0.0f : (b > 1.0f)? 1.0f : b*255.0f + 0.5f);
    unsigned int ca = (unsigned int)((a < 0.0f)? 0.0f : (a > 1.0f)? 1.0f : a*255.0f + 0.5f);

    return (ca << 24) | (cb << 16) | (cg << 8) | cr;
}

void glClearColor(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha)
{
    pspgu.clearColor = guColor(red, green, blue, alpha);
}

void glClearDepth(GLclampd depth)
{
    pspgu.clearDepth = (float)depth;
}

void glClear(GLbitfield mask)
{
    unsigned int bits = 0;

    if (!pspgu.current) return;

    if (mask & GL_COLOR_BUFFER_BIT)
    {
        sceGuClearColor(pspgu.clearColor);
        bits |= GU_COLOR_BUFFER_BIT;
    }

    // The library asks for COLOR|DEPTH on every frame (rlClearScreenBuffers). With no
    // depth buffer the DEPTH bit must be dropped here rather than handed to the GE,
    // which would be told to clear through a null Z pointer.
    if ((mask & GL_DEPTH_BUFFER_BIT) && (pspgu.depthOffset != NULL))
    {
        // GU's depth range is REVERSED, sceGuDepthRange(65535, 0) - near is 65535, far is 0 -
        // so GL's glClearDepth(1.0) ("far") has to become 0, not 65535. pspGL does exactly
        // this conversion (glClearDepth.c: 65535 - 65535*depth). Clearing to 65535 while the
        // range is reversed stores the NEAR value everywhere, so with GL_LEQUAL no fragment is
        // ever rejected and a cube's faces paint over each other in submission order - which
        // is what 722 colours and a mottled cube look like.
        sceGuClearDepth(65535u - (unsigned int)(pspgu.clearDepth * 65535.0f));
        bits |= GU_DEPTH_BUFFER_BIT;
    }

    if (bits != 0)
    {
        pspguBeginFrame();
        sceGuClear(bits);
    }
}

// Push one light's stored state. GU keeps no query, so glLight* mirrors it here and
// re-emits the whole light; the command stream is small and this only runs on change.
static void emitLight(int n)
{
    sceGuLight(n, pspgu.lightType[n], GU_DIFFUSE_AND_SPECULAR, &pspgu.lightPos[n]);
    sceGuLightColor(n, GU_AMBIENT, pspgu.lightAmbient[n]);
    sceGuLightColor(n, GU_DIFFUSE, pspgu.lightDiffuse[n]);
    sceGuLightColor(n, GU_SPECULAR, pspgu.lightSpecular[n]);
    sceGuLightAtt(n, pspgu.lightAtt[n][0], pspgu.lightAtt[n][1], pspgu.lightAtt[n][2]);
    if (pspgu.lightHasSpot[n])
        sceGuLightSpot(n, &pspgu.lightSpotDir[n], pspgu.lightSpotExp[n], pspgu.lightSpotCut[n]);
}

static void emitMaterial(void)
{
    sceGuModelColor(pspgu.matEmissive, pspgu.matAmbient, pspgu.matDiffuse, pspgu.matSpecular);
    sceGuSpecular(pspgu.matShininess);
}

// GL_DITHER is a request; additive blending can override the state without changing the
// request. On a 5650 target a low-alpha additive gradient dithers into dark speckles,
// which is imperceptible as banding if the dither is dropped for that pass.
void pspguApplyDither(void)
{
    if (!pspgu.current || !pspgu.listOpen) return;

    if (pspgu.ditherEnabled && !pspgu.ditherScoped) sceGuEnable(GU_DITHER);
    else sceGuDisable(GU_DITHER);
}

void glEnable(GLenum cap)
{
    if (!pspgu.current) return;
    pspguBeginFrame();

    switch (cap)
    {
        case GL_BLEND:          sceGuEnable(GU_BLEND); break;
        case GL_DEPTH_TEST:     if (pspgu.depthOffset) sceGuEnable(GU_DEPTH_TEST); break;
        case GL_CULL_FACE:      sceGuEnable(GU_CULL_FACE); break;
        case GL_SCISSOR_TEST:   sceGuEnable(GU_SCISSOR_TEST); break;
        case GL_TEXTURE_2D:     pspgu.texEnabled = 1; sceGuEnable(GU_TEXTURE_2D); break;
        case GL_LIGHTING:       pspgu.lighting = 1; sceGuEnable(GU_LIGHTING); break;
        case GL_LIGHT0:
        case GL_LIGHT1:
        case GL_LIGHT2:
        case GL_LIGHT3:
        {
            int n = (int)cap - (int)GL_LIGHT0;
            pspgu.lightEnable |= (1u << n);
            emitLight(n);
            sceGuEnable(GU_LIGHT0 + n);
        } break;
        case GL_DITHER:         pspgu.ditherEnabled = 1; pspguApplyDither(); break;
        case GL_FOG:
            pspgu.fogEnabled = 1;
            sceGuFog(pspgu.fogStart, pspgu.fogEnd, pspgu.fogColor);
            sceGuEnable(GU_FOG);
            break;
        case GL_LINE_SMOOTH:    break;      // the GE has no line antialiasing
        default: break;
    }
}

void glDisable(GLenum cap)
{
    if (!pspgu.current) return;
    pspguBeginFrame();

    switch (cap)
    {
        case GL_BLEND:          sceGuDisable(GU_BLEND); break;
        case GL_DEPTH_TEST:     sceGuDisable(GU_DEPTH_TEST); break;
        case GL_CULL_FACE:      sceGuDisable(GU_CULL_FACE); break;
        case GL_SCISSOR_TEST:   sceGuDisable(GU_SCISSOR_TEST); break;
        case GL_TEXTURE_2D:     pspgu.texEnabled = 0; sceGuDisable(GU_TEXTURE_2D); break;
        case GL_LIGHTING:       pspgu.lighting = 0; sceGuDisable(GU_LIGHTING); break;
        case GL_LIGHT0:
        case GL_LIGHT1:
        case GL_LIGHT2:
        case GL_LIGHT3:
        {
            int n = (int)cap - (int)GL_LIGHT0;
            pspgu.lightEnable &= ~(1u << n);
            sceGuDisable(GU_LIGHT0 + n);
        } break;
        case GL_FOG:            pspgu.fogEnabled = 0; sceGuDisable(GU_FOG); break;
        case GL_DITHER:         pspgu.ditherEnabled = 0; pspguApplyDither(); break;
        default: break;
    }
}

void glBlendFunc(GLenum sfactor, GLenum dfactor)
{
    int src = GU_SRC_ALPHA;
    int dst = GU_ONE_MINUS_SRC_ALPHA;
    unsigned int srcfix = 0;
    unsigned int dstfix = 0;
    int additive = 0;

    if (!pspgu.current) return;

    // The library only ever asks for the alpha pair (rlSetBlendMode is a GL33 no-op, so
    // GL11 gets the single glBlendFunc in rlglInit) plus whatever an app sets itself.
    if ((sfactor == GL_SRC_ALPHA) && (dfactor == GL_ONE_MINUS_SRC_ALPHA)) { src = GU_SRC_ALPHA; dst = GU_ONE_MINUS_SRC_ALPHA; }
    else if ((sfactor == GL_SRC_ALPHA) && (dfactor == GL_ONE))
    {
        // GL_ONE has no GU factor of its own. GU_FIX with fixed 1.0 is the accepted
        // encoding (PGE pgeGfx.c:423); the old (GU_FIX, 0) pair multiplied the
        // destination by zero, so additive draws added nothing at all.
        src = GU_SRC_ALPHA; dst = GU_FIX; dstfix = 0xFFFFFFFF; additive = 1;
    }
    else if ((sfactor == GL_ONE) && (dfactor == GL_ONE))
    {
        src = GU_FIX; dst = GU_FIX; srcfix = 0xFFFFFFFF; dstfix = 0xFFFFFFFF; additive = 1;
    }

    pspguBeginFrame();
#if PSPGU_DITHER_ADDITIVE
    pspgu.ditherScoped = additive;
    pspguApplyDither();
#else
    (void)additive;
#endif
    sceGuBlendFunc(GU_ADD, src, dst, srcfix, dstfix);
}

void glDepthFunc(GLenum func)
{
    int ge = GU_GEQUAL;

    if (!pspgu.current) return;

    // The PSP's Z coordinate system is backwards from OpenGL's, so EVERY comparison is
    // inverted - pspGL says so in as many words (glDepthFunc.c: "we need to reverse order of
    // depth comparisons") and its mapping table does exactly that. Our first version mapped
    // GL_LEQUAL to GU_LEQUAL: with a reversed range that rejects every fragment, so the 3D
    // cubes vanished entirely (613 colours = the 2D-only frame).
    switch (func)
    {
        case GL_NEVER:   ge = GU_NEVER; break;
        case GL_LESS:    ge = GU_GEQUAL; break;
        case GL_EQUAL:   ge = GU_EQUAL; break;
        case GL_LEQUAL:  ge = GU_GREATER; break;
        case GL_GREATER: ge = GU_LEQUAL; break;
        case GL_GEQUAL:  ge = GU_LESS; break;
        case GL_NOTEQUAL: ge = GU_NOTEQUAL; break;
        case GL_ALWAYS:  ge = GU_ALWAYS; break;
        default: break;
    }

    pspguBeginFrame();
    sceGuDepthFunc(ge);
}

void glDepthMask(GLboolean flag)
{
    if (!pspgu.current) return;
    pspguBeginFrame();
    // GU's mask is inverted relative to GL's: sceGuDepthMask(GU_TRUE) DISABLES Z writes.
    // Mapping GL_TRUE straight to GU_TRUE turned every rlDisableDepthMask() pass into a
    // depth-writing one and every rlEnableDepthMask() into a read-only pass.
    sceGuDepthMask((flag == GL_TRUE)? GU_FALSE : GU_TRUE);
}

void glColorMask(GLboolean r, GLboolean g, GLboolean b, GLboolean a)
{
    if (!pspgu.current) return;
    pspguBeginFrame();
    sceGuPixelMask(((a != 0)? 0xFF000000u : 0u) | ((b != 0)? 0x00FF0000u : 0u) |
                   ((g != 0)? 0x0000FF00u : 0u) | ((r != 0)? 0x000000FFu : 0u));
}

void glCullFace(GLenum mode)
{
    if (!pspgu.current) return;
    pspguBeginFrame();
    // The GE culls one side and has no separate front/back select the way GL does; the
    // convention that matters is the one glFrontFace sets.
    if (mode == GL_FRONT) sceGuFrontFace(GU_CW);
    else sceGuFrontFace(GU_CCW);
}

void glFrontFace(GLenum mode)
{
    if (!pspgu.current) return;
    pspguBeginFrame();
    sceGuFrontFace((mode == GL_CW)? GU_CW : GU_CCW);
}

void glShadeModel(GLenum mode)
{
    if (!pspgu.current) return;
    pspguBeginFrame();
    sceGuShadeModel((mode == GL_FLAT)? GU_FLAT : GU_SMOOTH);
}

void glViewport(GLint x, GLint y, GLsizei width, GLsizei height)
{
    if (!pspgu.current) return;
    pspguBeginFrame();
    // Offset is (2048 - W/2, 2048 - H/2) from init, so a GL viewport at (x,y) has its
    // centre at offset + (x + w/2, y + h/2) in GU's 4096-wide space.
    pspgu.vpX = 2048 - pspgu.width/2 + x + width/2;
    pspgu.vpY = 2048 - pspgu.height/2 + y + height/2;
    pspgu.vpW = width;
    pspgu.vpH = height;
    sceGuViewport(pspgu.vpX, pspgu.vpY, pspgu.vpW, pspgu.vpH);
}

void glScissor(GLint x, GLint y, GLsizei width, GLsizei height)
{
    if (!pspgu.current) return;
    pspguBeginFrame();
    pspgu.scX = x; pspgu.scY = y; pspgu.scW = width; pspgu.scH = height;
    sceGuScissor(x, y, width, height);
}

void glLineWidth(GLfloat width)
{
    // This SDK exposes no GU line-width call (the GE command exists, pspgu.h does not wrap
    // it). Recorded for the draw path to use if it ever needs more than one pixel.
    (void)width;
}

void glPolygonMode(GLenum face, GLenum mode)
{
    (void)face;
    (void)mode;
    // GU has no polygon mode; wireframe and point modes are a later task if a sample needs
    // them. Nothing in the GL11 2D path sets it.
}

void glPixelStorei(GLenum pname, GLint param)
{
    (void)pname;
    (void)param;
    // Only GL_UNPACK/PACK_ALIGNMENT reach here, and our upload path handles packed RGBA.
}

void glEnableClientState(GLenum array)
{
    // GU has no client-state enables: the vertex format is an argument to sceGuDrawArray,
    // built from these bits when the draw happens (gl_draw.c). Record them here.
    switch (array)
    {
        case GL_VERTEX_ARRAY:        pspgu.clientBits |= PSPGU_CS_VERTEX; break;
        case GL_TEXTURE_COORD_ARRAY: pspgu.clientBits |= PSPGU_CS_TEXCOORD; break;
        case GL_COLOR_ARRAY:         pspgu.clientBits |= PSPGU_CS_COLOR; break;
        case GL_NORMAL_ARRAY:        pspgu.clientBits |= PSPGU_CS_NORMAL; break;
        default: break;
    }
}

void glDisableClientState(GLenum array)
{
    switch (array)
    {
        case GL_VERTEX_ARRAY:        pspgu.clientBits &= ~PSPGU_CS_VERTEX; break;
        case GL_TEXTURE_COORD_ARRAY: pspgu.clientBits &= ~PSPGU_CS_TEXCOORD; break;
        case GL_COLOR_ARRAY:         pspgu.clientBits &= ~PSPGU_CS_COLOR; break;
        case GL_NORMAL_ARRAY:        pspgu.clientBits &= ~PSPGU_CS_NORMAL; break;
        default: break;
    }
}

//------------------------------------------------------------------------------------
// Lighting / material / fog (sprint 062)
//------------------------------------------------------------------------------------

void glLightfv(GLenum light, GLenum pname, const GLfloat *params)
{
    int n = (int)light - (int)GL_LIGHT0;

    if (!pspgu.current || (n < 0) || (n > 3) || (params == NULL)) return;
    pspguBeginFrame();

    switch (pname)
    {
        case GL_POSITION:
            pspgu.lightPos[n].x = params[0];
            pspgu.lightPos[n].y = params[1];
            pspgu.lightPos[n].z = params[2];
            pspgu.lightType[n] = (params[3] == 0.0f)? GU_DIRECTIONAL :
                                 (pspgu.lightHasSpot[n]? GU_SPOTLIGHT : GU_POINTLIGHT);
            break;
        case GL_AMBIENT:        pspgu.lightAmbient[n]  = guColor(params[0], params[1], params[2], params[3]); break;
        case GL_DIFFUSE:        pspgu.lightDiffuse[n]  = guColor(params[0], params[1], params[2], params[3]); break;
        case GL_SPECULAR:       pspgu.lightSpecular[n] = guColor(params[0], params[1], params[2], params[3]); break;
        case GL_CONSTANT_ATTENUATION:  pspgu.lightAtt[n][0] = params[0]; break;
        case GL_LINEAR_ATTENUATION:    pspgu.lightAtt[n][1] = params[0]; break;
        case GL_QUADRATIC_ATTENUATION: pspgu.lightAtt[n][2] = params[0]; break;
        case GL_SPOT_DIRECTION:
            pspgu.lightSpotDir[n].x = params[0];
            pspgu.lightSpotDir[n].y = params[1];
            pspgu.lightSpotDir[n].z = params[2];
            pspgu.lightHasSpot[n] = 1;
            if (pspgu.lightType[n] != GU_DIRECTIONAL) pspgu.lightType[n] = GU_SPOTLIGHT;
            break;
        case GL_SPOT_EXPONENT:  pspgu.lightSpotExp[n] = params[0]; pspgu.lightHasSpot[n] = 1; break;
        case GL_SPOT_CUTOFF:
            pspgu.lightSpotCut[n] = params[0];
            if (params[0] >= 180.0f) pspgu.lightHasSpot[n] = 0;
            else { pspgu.lightHasSpot[n] = 1; if (pspgu.lightType[n] != GU_DIRECTIONAL) pspgu.lightType[n] = GU_SPOTLIGHT; }
            break;
        default:
        {
            static int logged = 0;
            if (!logged) { pspguLog("glLightfv: unsupported pname 0x%x; ignored", (unsigned)pname); logged = 1; }
            return;
        }
    }

    emitLight(n);
}

void glLightf(GLenum light, GLenum pname, GLfloat param)
{
    GLfloat p[4] = { param, 0.0f, 0.0f, 0.0f };
    glLightfv(light, pname, p);
}

void glLightModelfv(GLenum pname, const GLfloat *params)
{
    if (!pspgu.current || (params == NULL)) return;
    pspguBeginFrame();

    switch (pname)
    {
        case GL_LIGHT_MODEL_AMBIENT:
            pspgu.sceneAmbient = guColor(params[0], params[1], params[2], params[3]);
            sceGuAmbient(pspgu.sceneAmbient);
            break;
        case GL_LIGHT_MODEL_COLOR_CONTROL:
            sceGuLightMode((params[0] == GL_SEPARATE_SPECULAR_COLOR)?
                           GU_SEPARATE_SPECULAR_COLOR : GU_SINGLE_COLOR);
            break;
        case GL_LIGHT_MODEL_TWO_SIDE:
        {
            static int logged = 0;
            if ((params[0] != 0.0f) && !logged)
            {
                pspguLog("glLightModel(TWO_SIDE): the GE is single-sided; ignored");
                logged = 1;
            }
        } break;
        default:
        {
            static int logged = 0;
            if (!logged) { pspguLog("glLightModelfv: unsupported pname 0x%x; ignored", (unsigned)pname); logged = 1; }
        } break;
    }
}

void glLightModeli(GLenum pname, GLint param)
{
    GLfloat p[4] = { (GLfloat)param, 0.0f, 0.0f, 0.0f };
    glLightModelfv(pname, p);
}

void glMaterialfv(GLenum face, GLenum pname, const GLfloat *params)
{
    (void)face;                                  // the GE has one material for both faces

    if (!pspgu.current || (params == NULL)) return;
    pspguBeginFrame();

    switch (pname)
    {
        case GL_AMBIENT:  pspgu.matAmbient  = guColor(params[0], params[1], params[2], params[3]); break;
        case GL_DIFFUSE:  pspgu.matDiffuse  = guColor(params[0], params[1], params[2], params[3]); break;
        case GL_SPECULAR: pspgu.matSpecular = guColor(params[0], params[1], params[2], params[3]); break;
        case GL_EMISSION: pspgu.matEmissive = guColor(params[0], params[1], params[2], params[3]); break;
        case GL_AMBIENT_AND_DIFFUSE:
            pspgu.matAmbient = pspgu.matDiffuse = guColor(params[0], params[1], params[2], params[3]);
            break;
        case GL_SHININESS:
            pspgu.matShininess = params[0];
            sceGuSpecular(pspgu.matShininess);
            return;
        default:
        {
            static int logged = 0;
            if (!logged) { pspguLog("glMaterialfv: unsupported pname 0x%x; ignored", (unsigned)pname); logged = 1; }
            return;
        }
    }

    emitMaterial();
}

void glMaterialf(GLenum face, GLenum pname, GLfloat param)
{
    GLfloat p[4] = { param, 0.0f, 0.0f, 0.0f };
    glMaterialfv(face, pname, p);
}

void glColorMaterial(GLenum face, GLenum mode)
{
    static int logged = 0;
    (void)face; (void)mode;
    if (!logged) { pspguLog("glColorMaterial: unsupported; glColor* updates the model diffuse"); logged = 1; }
}

void glFogf(GLenum pname, GLfloat param)
{
    if (!pspgu.current) return;

    switch (pname)
    {
        case GL_FOG_MODE:
            if (param != GL_LINEAR)
            {
                static int logged = 0;
                if (!logged) { pspguLog("glFog: only GL_LINEAR is supported (0x%x); fog disabled", (unsigned)param); logged = 1; }
                pspgu.fogEnabled = 0;
                pspguBeginFrame();
                sceGuDisable(GU_FOG);
            }
            break;
        case GL_FOG_START: pspgu.fogStart = param; break;
        case GL_FOG_END:   pspgu.fogEnd = param; break;
        default:
        {
            static int logged = 0;
            if (!logged) { pspguLog("glFogf: unsupported pname 0x%x; ignored", (unsigned)pname); logged = 1; }
        } return;
    }

    if (pspgu.fogEnabled)
    {
        pspguBeginFrame();
        sceGuFog(pspgu.fogStart, pspgu.fogEnd, pspgu.fogColor);
    }
}

void glFogi(GLenum pname, GLint param) { glFogf(pname, (GLfloat)param); }

void glFogfv(GLenum pname, const GLfloat *params)
{
    if (params == NULL) return;

    if (pname == GL_FOG_COLOR)
    {
        pspgu.fogColor = guColor(params[0], params[1], params[2], params[3]);
        if (pspgu.fogEnabled)
        {
            pspguBeginFrame();
            sceGuFog(pspgu.fogStart, pspgu.fogEnd, pspgu.fogColor);
        }
    }
    else glFogf(pname, params[0]);
}

// FBOs live in this object so a weak reference from rlgl cannot leave gl_fbo.o
// unextracted from the archive: weak undefined symbols do not pull archive members.
#include "gl_fbo.c"
