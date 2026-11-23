/*
 * clear_probe - P10-S12 T2: measure the GE clear path with and without
 * GU_FAST_CLEAR_BIT. One clear per frame; work is frame time minus swap time.
 */

#include <pspkernel.h>
#include <stdio.h>
#include <stdlib.h>

#include "raylib.h"
#include "psp_counters.h"

PSP_MODULE_INFO("clear_probe", 0, 1, 1);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);

#define FRAMES 300

static int cmpFloat(const void *a, const void *b)
{
    float x = *(const float *)a, y = *(const float *)b;
    return (x < y)? -1 : (x > y);
}

int main(int argc, char **argv)
{
    (void)argc; (void)argv;

    float *times = (float *)malloc(sizeof(float)*FRAMES);
    if (times == NULL) return 1;

    InitWindow(480, 272, "clear_probe");
    SetTargetFPS(0);

    PspCountersReset();

    double sum = 0.0;
    for (int i = 0; i < FRAMES; i++)
    {
        BeginDrawing();
            ClearBackground(BLACK);
        EndDrawing();
        times[i] = GetFrameTime();
        sum += times[i];
    }

    qsort(times, FRAMES, sizeof(float), cmpFloat);

    double avg = (sum/FRAMES)*1000.0;
    double swapAvg = (PspCountersSwapCalls() > 0)?
                     (PspCountersSwapSeconds()/PspCountersSwapCalls())*1000.0 : 0.0;
    double work = avg - swapAvg;

    TraceLog(LOG_INFO, "STAT n=%i min=%.3f median=%.3f p95=%.3f worst=%.3f avg=%.3f",
             FRAMES, times[0]*1000.0f, times[FRAMES/2]*1000.0f,
             times[(int)(FRAMES*0.95f)]*1000.0f, times[FRAMES-1]*1000.0f, (float)avg);
    TraceLog(LOG_INFO, "SPLIT n=%i work=%.3f swap=%.3f swapcalls=%u",
             FRAMES, work, swapAvg, PspCountersSwapCalls());
    TraceLog(LOG_INFO, "DONE frames %d", FRAMES);

    sceKernelSleepThread();
    free(times);
    return 0;
}
