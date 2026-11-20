#include <string.h>

#include "raylib.h"
#include "psp_counters.h"

PspCounters pspCounters = { 0 };

static double swapSeconds;
static unsigned int swapCalls;

void PspCountersReset(void)
{
    memset(&pspCounters, 0, sizeof(pspCounters));
    swapSeconds = 0.0;
    swapCalls = 0;
}

#ifdef WRAP_COUNTERS
// Provided by libGL.a; the linker redirects these through --wrap.
extern void __real_glBegin(GLenum mode);
extern void __real_glEnd(void);
extern void __real_glVertex2f(GLfloat x, GLfloat y);
extern void __real_glVertex2i(GLint x, GLint y);
extern void __real_glBindTexture(GLenum target, GLuint texture);
extern void __real_glDrawArrays(GLenum mode, GLint first, GLsizei count);
extern void __real_glDrawElements(GLenum mode, GLsizei count, GLenum type, const GLvoid *indices);

void __wrap_glBegin(GLenum mode)
{
    pspCounters.begins++;
    __real_glBegin(mode);
}

void __wrap_glEnd(void)
{
    pspCounters.ends++;
    __real_glEnd();
}

void __wrap_glVertex2f(GLfloat x, GLfloat y)
{
    pspCounters.vertices++;
    __real_glVertex2f(x, y);
}

void __wrap_glVertex2i(GLint x, GLint y)
{
    pspCounters.vertices++;
    __real_glVertex2i(x, y);
}

void __wrap_glBindTexture(GLenum target, GLuint texture)
{
    if (texture != 0) pspCounters.binds++;
    else pspCounters.unbinds++;

#ifdef TRACE_DRAWS
    TraceLog(LOG_INFO, "BIND texture=%u", (unsigned)texture);
#endif
    __real_glBindTexture(target, texture);
}

void __wrap_glDrawArrays(GLenum mode, GLint first, GLsizei count)
{
    pspCounters.drawArrays++;
    // The batched path submits no glVertex* calls at all, so the vertex total has to
    // come from here or the batched arm would report zero vertices drawn.
    if (count > 0) pspCounters.arrayVertices += (unsigned int)count;
#ifdef TRACE_DRAWS
    TraceLog(LOG_INFO, "DRAW arrays mode=%i count=%i", (int)mode, (int)count);
#endif
    __real_glDrawArrays(mode, first, count);
}

void __wrap_glDrawElements(GLenum mode, GLsizei count, GLenum type, const GLvoid *indices)
{
    pspCounters.drawElements++;
    __real_glDrawElements(mode, count, type, indices);
}

#endif // WRAP_COUNTERS

// eglSwapBuffers is wrapped with opaque pointer types on purpose: including
// GLES/egl.h alongside GL/gl.h risks redefining the same types.
extern unsigned int __real_eglSwapBuffers(void *dpy, void *surf);

unsigned int __wrap_eglSwapBuffers(void *dpy, void *surf)
{
    double t0 = GetTime();
    unsigned int r = __real_eglSwapBuffers(dpy, surf);
    swapSeconds += GetTime() - t0;
    swapCalls++;
    return r;
}

double PspCountersSwapSeconds(void) { return swapSeconds; }
unsigned int PspCountersSwapCalls(void) { return swapCalls; }

void PspCountersDump(const char *tag, int frames)
{
    TraceLog(LOG_INFO, "COUNT %s frames=%d begins=%u ends=%u verts=%u binds=%u unbinds=%u arrays=%u arrayVerts=%u",
             tag, frames, pspCounters.begins, pspCounters.ends, pspCounters.vertices,
             pspCounters.binds, pspCounters.unbinds,
             pspCounters.drawArrays + pspCounters.drawElements, pspCounters.arrayVertices);
}
