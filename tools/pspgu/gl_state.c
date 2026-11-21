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
        sceGuClearDepth((unsigned int)(pspgu.clearDepth * 65535.0f));
        bits |= GU_DEPTH_BUFFER_BIT;
    }

    if (bits != 0)
    {
        pspguBeginFrame();
        sceGuClear(bits);
    }
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
        case GL_TEXTURE_2D:     sceGuEnable(GU_TEXTURE_2D); break;
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
        case GL_TEXTURE_2D:     sceGuDisable(GU_TEXTURE_2D); break;
        default: break;
    }
}

void glBlendFunc(GLenum sfactor, GLenum dfactor)
{
    int src = GU_SRC_ALPHA;
    int dst = GU_ONE_MINUS_SRC_ALPHA;

    if (!pspgu.current) return;

    // The library only ever asks for the alpha pair (rlSetBlendMode is a GL33 no-op, so
    // GL11 gets the single glBlendFunc in rlglInit) plus whatever an app sets itself.
    if ((sfactor == GL_SRC_ALPHA) && (dfactor == GL_ONE_MINUS_SRC_ALPHA)) { src = GU_SRC_ALPHA; dst = GU_ONE_MINUS_SRC_ALPHA; }
    else if ((sfactor == GL_SRC_ALPHA) && (dfactor == GL_ONE))          { src = GU_SRC_ALPHA; dst = GU_FIX; }
    else if ((sfactor == GL_ONE) && (dfactor == GL_ONE))                { src = GU_FIX;      dst = GU_FIX; }

    pspguBeginFrame();
    sceGuBlendFunc(GU_ADD, src, dst, 0, 0);
}

void glDepthFunc(GLenum func)
{
    int ge = GU_LESS;

    if (!pspgu.current) return;

    switch (func)
    {
        case GL_NEVER: ge = GU_NEVER; break;
        case GL_LESS: ge = GU_LESS; break;
        case GL_EQUAL: ge = GU_EQUAL; break;
        case GL_LEQUAL: ge = GU_LEQUAL; break;
        case GL_GREATER: ge = GU_GREATER; break;
        case GL_GEQUAL: ge = GU_GEQUAL; break;
        case GL_ALWAYS: ge = GU_ALWAYS; break;
        default: break;
    }

    pspguBeginFrame();
    sceGuDepthFunc(ge);
}

void glDepthMask(GLboolean flag)
{
    if (!pspgu.current) return;
    pspguBeginFrame();
    sceGuDepthMask((flag == GL_TRUE)? GU_TRUE : GU_FALSE);
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
    sceGuViewport(2048 - pspgu.width/2 + x + width/2,
                  2048 - pspgu.height/2 + y + height/2, width, height);
}

void glScissor(GLint x, GLint y, GLsizei width, GLsizei height)
{
    if (!pspgu.current) return;
    pspguBeginFrame();
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
