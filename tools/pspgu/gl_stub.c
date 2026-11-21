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
unsigned int pspguStubTexture = 0;

//------------------------------------------------------------------------------------
// Matrices - task 4
//------------------------------------------------------------------------------------

void glMatrixMode(GLenum mode)                              { (void)mode; pspguStubMatrix++; }
void glLoadIdentity(void)                                   { pspguStubMatrix++; }
void glPushMatrix(void)                                     { pspguStubMatrix++; }
void glPopMatrix(void)                                      { pspguStubMatrix++; }
void glMultMatrixf(const GLfloat *m)                        { (void)m; pspguStubMatrix++; }
void glOrtho(GLdouble l, GLdouble r, GLdouble b, GLdouble t, GLdouble n, GLdouble f)
                                                            { (void)l; (void)r; (void)b; (void)t; (void)n; (void)f; pspguStubMatrix++; }
void glFrustum(GLdouble l, GLdouble r, GLdouble b, GLdouble t, GLdouble n, GLdouble f)
                                                            { (void)l; (void)r; (void)b; (void)t; (void)n; (void)f; pspguStubMatrix++; }
void glRotatef(GLfloat a, GLfloat x, GLfloat y, GLfloat z)  { (void)a; (void)x; (void)y; (void)z; pspguStubMatrix++; }
void glScalef(GLfloat x, GLfloat y, GLfloat z)              { (void)x; (void)y; (void)z; pspguStubMatrix++; }
void glTranslatef(GLfloat x, GLfloat y, GLfloat z)          { (void)x; (void)y; (void)z; pspguStubMatrix++; }

//------------------------------------------------------------------------------------
// Vertex arrays and draws - task 5
//------------------------------------------------------------------------------------

void glVertexPointer(GLint size, GLenum type, GLsizei stride, const GLvoid *ptr)
                                                            { (void)size; (void)type; (void)stride; (void)ptr; pspguStubDraw++; }
void glTexCoordPointer(GLint size, GLenum type, GLsizei stride, const GLvoid *ptr)
                                                            { (void)size; (void)type; (void)stride; (void)ptr; pspguStubDraw++; }
void glColorPointer(GLint size, GLenum type, GLsizei stride, const GLvoid *ptr)
                                                            { (void)size; (void)type; (void)stride; (void)ptr; pspguStubDraw++; }
void glNormalPointer(GLenum type, GLsizei stride, const GLvoid *ptr)
                                                            { (void)type; (void)stride; (void)ptr; pspguStubDraw++; }
void glDrawArrays(GLenum mode, GLint first, GLsizei count)
                                                            { (void)mode; (void)first; (void)count; pspguStubDraw++; }
void glDrawElements(GLenum mode, GLsizei count, GLenum type, const GLvoid *indices)
                                                            { (void)mode; (void)count; (void)type; (void)indices; pspguStubDraw++; }

//------------------------------------------------------------------------------------
// Textures - task 6
//------------------------------------------------------------------------------------

// Ids must look real to the library: 0 is "no texture" and raylib treats it that way
static GLuint nextTextureId = 1;

void glGenTextures(GLsizei n, GLuint *textures)
{
    if (textures == NULL) return;
    for (GLsizei i = 0; i < n; i++) textures[i] = nextTextureId++;
    pspguStubTexture++;
}

void glDeleteTextures(GLsizei n, const GLuint *textures)    { (void)n; (void)textures; pspguStubTexture++; }
void glBindTexture(GLenum target, GLuint texture)           { (void)target; (void)texture; pspguStubTexture++; }
void glTexImage2D(GLenum target, GLint level, GLint internalFormat, GLsizei width, GLsizei height,
                  GLint border, GLenum format, GLenum type, const GLvoid *pixels)
{
    (void)target; (void)level; (void)internalFormat; (void)width; (void)height;
    (void)border; (void)format; (void)type; (void)pixels;
    pspguStubTexture++;
}
void glTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width,
                     GLsizei height, GLenum format, GLenum type, const GLvoid *pixels)
{
    (void)target; (void)level; (void)xoffset; (void)yoffset; (void)width; (void)height;
    (void)format; (void)type; (void)pixels;
    pspguStubTexture++;
}
void glTexParameteri(GLenum target, GLenum pname, GLint param)
                                                            { (void)target; (void)pname; (void)param; pspguStubTexture++; }

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
        {
            // Identity for now; gl_matrix.c answers these from the real stack
            memset(params, 0, 16*sizeof(GLfloat));
            params[0] = params[5] = params[10] = params[15] = 1.0f;
        } break;
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
