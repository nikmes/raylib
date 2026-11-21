/*
 * gl_query.c - GL queries and screen readback.
 *
 * These three symbols used to live in gl_stub.c, which existed to make the link succeed
 * before they were implemented. They are real now:
 *
 *  - glGetString/glGetFloatv answer from the driver's own state. The matrix stack lives
 *    on the CPU (gl_matrix.c) precisely so these queries are possible.
 *  - glReadPixels expands the 5650 framebuffer to 8-bit RGBA and returns rows in GL
 *    order (row 0 is the bottom of the screen). raylib's rlReadScreenPixels() flips
 *    that back for its top-down Image.
 *
 * The read buffer is the last completed frame. Rendering is deferred to the list
 * submission in eglSwapBuffers, so "the current draw buffer" mid-frame is only half a
 * frame; a read after EndDrawing is the frame the display just received. If a list is
 * open when this is called, it is finished and waited first, and the draw buffer (the
 * frame just submitted) is read instead. Readback is a diagnostic path, not a per-frame
 * one; the wait is deliberate.
 */

#include <string.h>

#include "pspgu_internal.h"

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

// The 5650 framebuffer is standard 5-6-5: RED in the low five bits, green in the middle,
// blue in the high five. The test is the raylib tint (30,40,200) in batch_check: the PNG
// must carry (24,40,206); the opposite order gives (206,40,24). Bit replication is the
// correct 5->8 expansion. PSPLink's scrshot writes the same bytes in BGR order, so a
// comparison swaps its channels first; then the two agree within one 5-bit step (max 7).
static void expand5650(unsigned short v, unsigned char *rgb)
{
    unsigned int r = (v >> 0) & 0x1F;
    unsigned int g = (v >> 5) & 0x3F;
    unsigned int b = (v >> 11) & 0x1F;

    rgb[0] = (unsigned char)((r << 3) | (r >> 2));
    rgb[1] = (unsigned char)((g << 2) | (g >> 4));
    rgb[2] = (unsigned char)((b << 3) | (b >> 2));
}

void glReadPixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format,
                  GLenum type, GLvoid *pixels)
{
    static int warned = 0;

    if (!pspgu.current || (pixels == NULL) || (width <= 0) || (height <= 0)) return;

    if ((format != GL_RGBA) && (format != GL_RGB))
    {
        if (!warned) { pspguLog("glReadPixels: unsupported format 0x%x", (unsigned)format); warned = 1; }
        return;
    }
    if (type != GL_UNSIGNED_BYTE)
    {
        if (!warned) { pspguLog("glReadPixels: unsupported type 0x%x", (unsigned)type); warned = 1; }
        return;
    }

    // If a list is open, make the frame it describes complete and read the draw buffer.
    // Otherwise the last submitted frame is the one the display is showing.
    void *frame;
    if (pspgu.listOpen)
    {
        pspguFinishAndWait();
        frame = pspgu.drawOffset;
    }
    else frame = (pspgu.readOffset != NULL)? pspgu.readOffset : pspgu.drawOffset;

    const unsigned char *base = (const unsigned char *)sceGeEdramGetAddr();

    int x0 = (x < 0)? 0 : (int)x;
    int y0 = (y < 0)? 0 : (int)y;
    int x1 = (int)x + width;
    int y1 = (int)y + height;

    if (x1 > pspgu.width) x1 = pspgu.width;
    if (y1 > pspgu.height) y1 = pspgu.height;
    if ((x1 <= x0) || (y1 <= y0)) return;

    int bpp = (format == GL_RGBA)? 4 : 3;
    unsigned char *out = (unsigned char *)pixels;
    unsigned int p = (unsigned int)frame;

    for (int row = 0; row < (y1 - y0); row++)
    {
        int gl_y = y0 + row;
        int src_y = pspgu.height - 1 - gl_y;    // VRAM row 0 is the top; GL row 0 is the bottom
        const unsigned short *src = (const unsigned short *)(base + p +
                                   (size_t)src_y*pspgu.stride*2);

        for (int col = 0; col < (x1 - x0); col++)
        {
            unsigned char rgb[3];
            expand5650(src[x0 + col], rgb);

            // The destination row is the requested width; clipped columns keep their place
            unsigned char *d = out + ((size_t)row*(unsigned int)width + (x0 - (int)x) + col)*bpp;
            d[0] = rgb[0];
            d[1] = rgb[1];
            d[2] = rgb[2];
            if (bpp == 4) d[3] = 255;
        }
    }
}
