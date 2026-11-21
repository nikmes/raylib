/*
 * texture_churn - sprint 015 T3: does the VRAM allocator return memory?
 *
 * The bump allocator this replaces could not reclaim a deleted texture, so "unload"
 * was a lie and a load/unload loop leaked until VRAM ran out. This probe loads 16
 * textures of alternating sizes, unloads them in a deliberately awkward order, and
 * repeats 100 times. The driver prints its block table every 10 cycles:
 *
 *   [PSPGU] VRAM [churn N]: reserved=... requested=... used=... free=... largest=...
 *
 * Pass: free and largest at churn 100 match churn start within one 512-byte block,
 * and no line reports failures or VRAM FULL.
 *
 * The driver symbol is exported from libGL.a; this probe is allowed to call it because
 * there is no GL query for allocator state.
 */

#include <pspkernel.h>
#include <stdio.h>

#include "raylib.h"

void pspguVramReport(const char *when);
unsigned int pspguVramFreeBytes(void);
unsigned int pspguVramLargestBytes(void);

PSP_MODULE_INFO("texture_churn", 0, 1, 1);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);

// Stage marker for a hang diagnosis; host0: survives losing the console.
static void mark(const char *stage)
{
    SaveFileText("churn_stage.txt", (char *)stage);
}

#define TEX_PER_CYCLE 16
#ifndef CHURN_CYCLES
    #define CHURN_CYCLES 100
#endif
#define CYCLES CHURN_CYCLES

int main(void)
{
    InitWindow(480, 272, "texture churn");
    SetTargetFPS(0);

    // raylib logs every texture load/unload at INFO: 3,200 lines over PSPLink's USB
    // stdout backpressures the host and turns a CPU-bound churn into an IO wait. The
    // driver's VRAM lines use printf directly and are unaffected.
    SetTraceLogLevel(LOG_WARNING);

    Image imgs[3] = {
        GenImageColor(64, 64, RED),
        GenImageColor(128, 32, GREEN),
        GenImageColor(32, 128, BLUE),
    };

    Texture2D tex[TEX_PER_CYCLE];

    // Baseline after InitWindow's default font/default texture, before any churn.
    pspguVramReport("churn start");
    unsigned int free0 = pspguVramFreeBytes();
    unsigned int largest0 = pspguVramLargestBytes();

    for (int cycle = 0; cycle < CYCLES; cycle++)
    {
#ifdef CHURN_TRACE
        TraceLog(LOG_WARNING, "CHURN cycle %i start", cycle);
#endif
        mark(TextFormat("%i load", cycle));
        for (int i = 0; i < TEX_PER_CYCLE; i++)
        {
            // Alternate sizes so allocations split and free spans of different lengths.
            tex[i] = LoadTextureFromImage(imgs[(i + cycle) % 3]);
        }

        // Awkward free order: even slots first, then odd, so holes cannot simply be
        // handed back in allocation order.
        mark(TextFormat("%i free", cycle));
        for (int i = 0; i < TEX_PER_CYCLE; i += 2) UnloadTexture(tex[i]);
        for (int i = 1; i < TEX_PER_CYCLE; i += 2) UnloadTexture(tex[i]);

        // End a frame per cycle. Without this the texture-state commands accumulate in
        // one 240 KiB display list and overrun it: the churn stopped at cycle ~50 with
        // heap/allocator state still perfect. GU has no list-capacity check of its own.
        mark(TextFormat("%i draw", cycle));
        BeginDrawing();
            ClearBackground(BLACK);
        EndDrawing();

        if (((cycle + 1) % 10) == 0)
        {
            pspguVramReport(TextFormat("churn %d", cycle + 1));
        }
        mark(TextFormat("%i end", cycle));
#ifdef CHURN_TRACE
        TraceLog(LOG_WARNING, "CHURN cycle %i end", cycle);
#endif
    }

    for (int i = 0; i < 3; i++) UnloadImage(imgs[i]);

    unsigned int free1 = pspguVramFreeBytes();
    unsigned int largest1 = pspguVramLargestBytes();
    int ok = (free1 + 512 >= free0) && (free1 <= free0 + 512) &&
             (largest1 + 512 >= largest0) && (largest1 <= largest0 + 512);

    // A host-window timeout must not be able to lose the verdict: write it to host0:.
    SaveFileText("churn_done.txt",
                 TextFormat("DONE cycles=%d free0=%u free1=%u largest0=%u largest1=%u verdict=%s\n",
                            CYCLES, free0, free1, largest0, largest1, ok? "PASS" : "FAIL"));

    TraceLog(LOG_WARNING, "DONE churn %i x %i textures free0=%u free1=%u largest0=%u largest1=%u %s",
             CYCLES, TEX_PER_CYCLE, free0, free1, largest0, largest1, ok? "PASS" : "FAIL");
    sceKernelSleepThread();

    return 0;
}
