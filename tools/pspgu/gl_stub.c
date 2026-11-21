/*
 * gl_stub.c - the symbols phase 5 has not implemented yet.
 *
 * Every one of these is on the measured 52-symbol list, so the link needs them now. They
 * are replaced file by file: gl_matrix.c (task 4), gl_draw.c (task 5), gl_texture.c
 * (task 6), gl_query.c (task 7). Anything that returns a value returns something the
 * library can survive, so the vertical slice can boot and show a cleared screen.
 *
 * The `pspguStub` counter exists so a run can say how much of the frame is still being
 * thrown away: if drawing a scene touches no stub, that scene is fully implemented.
 */

#include <string.h>

#include "pspgu_internal.h"

unsigned int pspguStubCalls = 0;
unsigned int pspguStubMatrix = 0;
unsigned int pspguStubDraw = 0;

//------------------------------------------------------------------------------------
// Queries and readback - task 7
//------------------------------------------------------------------------------------

static const char *pspguVendor = "pspgu";
static const char *pspguRenderer = "PlayStation Portable GE (libgu)";
static const char *pspguVersion = "1.1 pspgu-0";

const GLubyte *glGetString(GLenum name)
{
    // The platform layer prints all three of these with %s at boot, so none may be NULL
    switch (name)
    {
        case GL_VENDOR:   return (const GLubyte *)pspguVendor;
        case GL_RENDERER: return (const GLubyte *)pspguRenderer;
        case GL_VERSION:  return (const GLubyte *)pspguVersion;
        default:          return (const GLubyte *)"";
    }
}

void glGetFloatv(GLenum pname, GLfloat *params)
{
    if (params == NULL) return;

    switch (pname)
    {
        case GL_LINE_WIDTH: params[0] = 1.0f; break;
        case GL_MODELVIEW_MATRIX:
        case GL_PROJECTION_MATRIX:
            glGetFloatvMatrices(pname, params);
            break;
        default: params[0] = 0.0f; break;
    }
}

void glReadPixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format,
                  GLenum type, GLvoid *pixels)
{
    (void)x; (void)y; (void)width; (void)height; (void)format; (void)type; (void)pixels;
    // Task 7: expand the 5650 framebuffer to RGBA8888 and flip. Nothing in the slice path
    // reads pixels, and scrshot (PSPLink) is what our captures use.
}
