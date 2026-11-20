/*
 * Link-time counter harness for the PSP backend.
 *
 * raylib draws on the GL11 path through glBegin/glEnd and immediate vertices. We
 * interpose on those symbols with -Wl,--wrap=<sym> and forward to __real_<sym>,
 * so counting happens at the boundary between raylib and pspGL without a single
 * line changed in src/.
 *
 * Why not inside rlgl: rlgl.h is upstream code we intend to replace with a native
 * rGL backend. Anything added there has to be carried through that change and
 * through every merge until then. The wrap symbols are the only thing that
 * changes when the backend does.
 */

#ifndef PSP_COUNTERS_H
#define PSP_COUNTERS_H

#include <GL/gl.h>

typedef struct {
    unsigned int begins;        // glBegin calls, all modes
    unsigned int ends;          // glEnd calls: one per submission into pspGL
    unsigned int vertices;      // glVertex2f + glVertex2i
    unsigned int binds;         // glBindTexture with a non-zero texture
    unsigned int unbinds;       // glBindTexture(0)
    unsigned int drawArrays;    // glDrawArrays calls (PSP_BATCH=1: one per texture switch)
    unsigned int drawElements;
    unsigned int arrayVertices; // vertices submitted through glDrawArrays
} PspCounters;

extern PspCounters pspCounters;

void PspCountersReset(void);
void PspCountersDump(const char *tag, int frames);

// Time spent inside eglSwapBuffers - the vsync wait, not our work. Reported
// separately because the display paces the swap and that must not be confused
// with the cost of drawing.
double PspCountersSwapSeconds(void);
unsigned int PspCountersSwapCalls(void);

#endif
