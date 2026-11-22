/*******************************************************************************************
*
*   pad_probe - raw nub measurement tool (sprint 043, P8-S3)
*
*   PSP-only. Reads the controller with sceCtrlPeekBufferPositive, which does not consume
*   a sample, so it cannot steal input from raylib's own poll. It never calls
*   sceCtrlReadBuffer* or sceCtrlReadLatch.
*
*   Two modes, selected at build time:
*     IDLE_SECS=0  (default) - the numbered movement sequence, then one RAWSUM and RAWPOS
*                              line per offset. Host output happens only between steps.
*     IDLE_SECS=360          - idle-timer test. Prints nothing until the window ends, then
*                              elapsed and the largest frame gap. Used with a library built
*                              with PSP_PAD_IDLE=1 and PSP_PAD_STATS_INTERVAL=0.
*
********************************************************************************************/

#include "raylib.h"

#include <pspctrl.h>
#include <math.h>

#ifndef RUN
    #define RUN 1
#endif
#ifndef IDLE_SECS
    #define IDLE_SECS 0
#endif
#ifndef MANUAL_SECS
    #define MANUAL_SECS 0
#endif

#define PRE_SECS 10.0f

#if (IDLE_SECS <= 0) && (MANUAL_SECS <= 0)
#define MAX_SAMPLES 8192

enum { STEP_IDLE, STEP_HOLD, STEP_ROLL, STEP_RELEASE, STEP_OFFSET };

typedef struct {
    const char *label;
    float seconds;
    int kind;
} Step;

static const Step fixedSteps[] = {
    { "IDLE: HANDS OFF the nub", 10.0f, STEP_IDLE },
    { "PUSH the nub fully RIGHT and hold", 5.0f, STEP_HOLD },
    { "PUSH the nub fully LEFT and hold",  5.0f, STEP_HOLD },
    { "PUSH the nub fully UP and hold",    5.0f, STEP_HOLD },
    { "PUSH the nub fully DOWN and hold",  5.0f, STEP_HOLD },
    { "ROLL the nub clockwise on the rim",    6.0f, STEP_ROLL },
    { "ROLL the nub anticlockwise on the rim", 6.0f, STEP_ROLL },
    { "RELEASE: centre the nub",           4.0f, STEP_RELEASE },
};

#define FIXED_COUNT  ((int)(sizeof(fixedSteps)/sizeof(fixedSteps[0])))
#define RAMP_COUNT   4
#define RAMP_SECS    5.0f
#define STEP_COUNT   (FIXED_COUNT + RAMP_COUNT)

typedef struct {
    unsigned char lx;
    unsigned char ly;
    unsigned char step;
} Sample;

static Sample samples[MAX_SAMPLES];
static int sampleCount = 0;
static float stepStart[STEP_COUNT + 1];

static unsigned int lxMin = 255, lxMax = 0;
static unsigned int lyMin = 255, lyMax = 0;
static int centreX = 128, centreY = 128;
static int centreReady = 0;

static const unsigned int rampTargets[4] = { 10, 20, 40, 80 };
static const char *rampNames[4] = {
    "SLOWLY push the nub RIGHT from centre to the rim",
    "SLOWLY push the nub LEFT from centre to the rim",
    "SLOWLY push the nub UP from centre to the rim",
    "SLOWLY push the nub DOWN from centre to the rim",
};

static unsigned char MedianFromHistogram(const unsigned int *hist, int count)
{
    int half = count/2;
    int acc = 0;

    for (int v = 0; v < 256; v++)
    {
        acc += (int)hist[v];
        if (acc > half) return (unsigned char)v;
    }

    return 128;
}

static void ComputeCentre(void)
{
    unsigned int histX[256] = { 0 };
    unsigned int histY[256] = { 0 };
    int count = 0;

    for (int i = 0; i < sampleCount; i++)
    {
        if (samples[i].step != STEP_IDLE) continue;
        histX[samples[i].lx]++;
        histY[samples[i].ly]++;
        count++;
    }

    if (count == 0) return;
    centreX = MedianFromHistogram(histX, count);
    centreY = MedianFromHistogram(histY, count);
    centreReady = 1;
}

static float RangeX(int raw)
{
    return (raw >= centreX)? (float)(lxMax - centreX) : (float)(centreX - lxMin);
}

static float RangeY(int raw)
{
    return (raw >= centreY)? (float)(lyMax - centreY) : (float)(centreY - lyMin);
}

static void Normalize(int rawX, int rawY, float *nx, float *ny)
{
    float rx = RangeX(rawX);
    float ry = RangeY(rawY);

    if (rx < 1.0f) rx = 1.0f;
    if (ry < 1.0f) ry = 1.0f;

    *nx = (float)(rawX - centreX)/rx;
    *ny = (float)(rawY - centreY)/ry;
}

static float Magnitude(float nx, float ny)
{
    return sqrtf(nx*nx + ny*ny);
}

static void BuildStepStarts(void)
{
    float t = 0.0f;

    for (int i = 0; i < FIXED_COUNT; i++)
    {
        stepStart[i] = t;
        t += fixedSteps[i].seconds;
    }
    for (int i = 0; i < RAMP_COUNT; i++)
    {
        stepStart[FIXED_COUNT + i] = t;
        t += RAMP_SECS;
    }
    stepStart[STEP_COUNT] = t;
}

static int StepAt(float elapsed)
{
    int step = 0;
    while (((step + 1) <= STEP_COUNT) && (elapsed >= stepStart[step + 1])) step++;
    return step;
}

static const char *StepLabel(int step)
{
    if (step < FIXED_COUNT) return fixedSteps[step].label;

    int i = step - FIXED_COUNT;
    if ((i >= 0) && (i < RAMP_COUNT)) return rampNames[i];
    return "?";
}

static void PrintSummary(void)
{
    int samplesUsed = 0;
    float idleMax = 0.0f;
    float rimMax = 0.0f;
    int idleLxMin = 255, idleLxMax = 0, idleLyMin = 255, idleLyMax = 0;
    int rxp, rxn, ryp, ryn;

    ComputeCentre();

    rxp = lxMax - centreX;
    rxn = centreX - lxMin;
    ryp = lyMax - centreY;
    ryn = centreY - lyMin;

    for (int i = 0; i < sampleCount; i++)
    {
        float nx, ny, mag;

        Normalize(samples[i].lx, samples[i].ly, &nx, &ny);
        mag = Magnitude(nx, ny);

        if (samples[i].step == STEP_IDLE)
        {
            if (mag > idleMax) idleMax = mag;
            if (samples[i].lx < idleLxMin) idleLxMin = samples[i].lx;
            if (samples[i].lx > idleLxMax) idleLxMax = samples[i].lx;
            if (samples[i].ly < idleLyMin) idleLyMin = samples[i].ly;
            if (samples[i].ly > idleLyMax) idleLyMax = samples[i].ly;
        }
        else if ((samples[i].step == FIXED_COUNT - 3) || (samples[i].step == FIXED_COUNT - 2))
        {
            if (mag > rimMax) rimMax = mag;
        }

        samplesUsed++;
    }

    TraceLog(LOG_INFO, "RAWSUM run=%d cx=%d cy=%d lx_min=%u lx_max=%u ly_min=%u ly_max=%u "
        "rxp=%d rxn=%d ryp=%d ryn=%d idle_dx=%d idle_dy=%d idle_max=%.3f rim_max=%.3f samples=%d",
        RUN, centreX, centreY, lxMin, lxMax, lyMin, lyMax, rxp, rxn, ryp, ryn,
        idleLxMax - idleLxMin, idleLyMax - idleLyMin, idleMax, rimMax, samplesUsed);

    static const int rampSign[RAMP_COUNT] = { +1, -1, -1, +1 }; // right, left, up, down
    static const char *rampDir[RAMP_COUNT] = { "RIGHT", "LEFT", "UP", "DOWN" };

    for (int dir = 0; dir < RAMP_COUNT; dir++)
    {
        int step = FIXED_COUNT + dir;
        int axisX = (dir < 2);

        for (int t = 0; t < 4; t++)
        {
            unsigned int target = rampTargets[t];
            int bestRaw = 0;
            int bestErr = 999;

            for (int s = 0; s < sampleCount; s++)
            {
                int raw, wanted, err;

                if (samples[s].step != step) continue;
                raw = axisX? samples[s].lx : samples[s].ly;
                wanted = (axisX? centreX : centreY) + rampSign[dir]*(int)target;
                err = raw - wanted;
                if (err < 0) err = -err;

                if (err < bestErr)
                {
                    bestErr = err;
                    bestRaw = raw;
                }
            }

            if (bestErr == 999) continue;

            TraceLog(LOG_INFO, "RAWPOS dir=%s target=%u raw=%d err=%d", rampDir[dir],
                target, bestRaw, bestErr);
        }
    }
}

#endif

#if IDLE_SECS > 0

int main(void)
{
    double start;
    double last;
    double maxGap = 0.0;
    InitWindow(480, 272, "pad idle probe");
    if (!IsWindowReady()) return 1;

    start = GetTime();
    last = start;

    for (;;)
    {
        SceCtrlData pad = { 0 };
        double now = GetTime();
        double gap = now - last;
        double elapsed = now - start;

        if (gap > maxGap) maxGap = gap;
        last = now;

        sceCtrlPeekBufferPositive(&pad, 1);

        BeginDrawing();
        ClearBackground(BLACK);

        DrawText("IDLE TIMER TEST", 10, 10, 20, RAYWHITE);
        DrawText("Do not touch the nub. Do not press a button.", 10, 40, 10, YELLOW);
        DrawText(TextFormat("elapsed %d s / %d s", (int)elapsed, IDLE_SECS), 10, 60, 20, GREEN);

        // Heartbeat: toggles once per second, so a frozen app is visible on screen.
        DrawRectangle(10, 90, 40, 40, (((int)elapsed % 2) == 0)? GREEN : DARKGRAY);

        DrawText(TextFormat("raw Lx %3u  Ly %3u", pad.Lx, pad.Ly), 10, 150, 10, SKYBLUE);
        DrawText("host output only after the window", 10, 250, 10, DARKGRAY);

        EndDrawing();

        if (elapsed >= (double)IDLE_SECS) break;

        double frameSeconds = 1.0/60.0;
        double frameElapsed = GetTime() - now;
        if (frameElapsed < frameSeconds) WaitTime(frameSeconds - frameElapsed);
    }

    TraceLog(LOG_INFO, "PAD idle elapsed=%us sleepgap_ms=%.1f", (unsigned int)IDLE_SECS, maxGap*1000.0);
    TraceLog(LOG_INFO, "DONE idle run=%d", RUN);

    for (;;) WaitTime(0.1);
}

#elif MANUAL_SECS > 0

int main(void)
{
    double start;
    double next;
    double seqStart;

    InitWindow(480, 272, "pad probe manual");
    if (!IsWindowReady()) return 1;

    seqStart = GetTime() + PRE_SECS;

    while (GetTime() < seqStart)
    {
        double now = GetTime();

        BeginDrawing();
        ClearBackground(BLACK);
        DrawText("NUB LIVE CHECK", 10, 10, 20, RAYWHITE);
        DrawText("GET READY", 10, 60, 30, ORANGE);
        DrawText(TextFormat("starts in %.0f s", seqStart - now), 10, 100, 20, YELLOW);
        DrawText("Keep the nub untouched until the countdown ends.", 10, 140, 10, LIGHTGRAY);
        EndDrawing();

        double frameElapsed = GetTime() - now;
        if (frameElapsed < 1.0/60.0) WaitTime(1.0/60.0 - frameElapsed);
    }

    start = GetTime();
    next = start;

    while ((GetTime() - start) < (double)MANUAL_SECS)
    {
        SceCtrlData pad = { 0 };
        double now = GetTime();

        sceCtrlPeekBufferPositive(&pad, 1);

        if (now >= next)
        {
            TraceLog(LOG_INFO, "RAW live t=%.0f lx=%u ly=%u LX=%.3f LY=%.3f", now - start, pad.Lx, pad.Ly,
                GetGamepadAxisMovement(0, GAMEPAD_AXIS_LEFT_X),
                GetGamepadAxisMovement(0, GAMEPAD_AXIS_LEFT_Y));
            next += 1.0;
        }

        BeginDrawing();
        ClearBackground(BLACK);
        DrawText("NUB LIVE CHECK", 10, 10, 20, RAYWHITE);
        DrawText("Follow the schedule.", 10, 44, 10, YELLOW);
        DrawText(TextFormat("Lx %3u   Ly %3u", pad.Lx, pad.Ly), 10, 80, 40, GREEN);
        DrawText(TextFormat("LX %+.2f  LY %+.2f",
            GetGamepadAxisMovement(0, GAMEPAD_AXIS_LEFT_X),
            GetGamepadAxisMovement(0, GAMEPAD_AXIS_LEFT_Y)), 10, 130, 20, SKYBLUE);
        DrawText(TextFormat("%.0f s / %d s", now - start, MANUAL_SECS), 10, 170, 10, LIGHTGRAY);
        EndDrawing();

        double frameElapsed = GetTime() - now;
        if (frameElapsed < 1.0/60.0) WaitTime(1.0/60.0 - frameElapsed);
    }

    TraceLog(LOG_INFO, "DONE manual");
    for (;;) WaitTime(0.1);
}

#else

int main(void)
{
    int lastStep = -1;

    InitWindow(480, 272, "pad probe");
    if (!IsWindowReady()) return 1;

    BuildStepStarts();

    double start = GetTime();
    double seqStart = start + PRE_SECS;

    for (;;)
    {
        SceCtrlData pad = { 0 };
        double now = GetTime();
        double elapsed = now - seqStart;

        if (elapsed < 0.0)
        {
            // GET READY pre-roll: nothing is sampled until the sequence clock starts.
            sceCtrlPeekBufferPositive(&pad, 1);

            BeginDrawing();
            ClearBackground(BLACK);
            DrawText("pad probe", 10, 10, 20, RAYWHITE);
            DrawText(TextFormat("run %d", RUN), 150, 14, 10, GRAY);
            DrawText("GET READY", 10, 60, 30, ORANGE);
            DrawText(TextFormat("starts in %.0f s", -elapsed), 10, 100, 20, YELLOW);
            DrawText("keep the nub untouched for step 1", 10, 130, 10, LIGHTGRAY);
            EndDrawing();

            double frameElapsed = GetTime() - now;
            if (frameElapsed < 1.0/60.0) WaitTime(1.0/60.0 - frameElapsed);
            continue;
        }

        if (elapsed >= (double)stepStart[STEP_COUNT]) break;

        int step = StepAt((float)elapsed);

        sceCtrlPeekBufferPositive(&pad, 1);

        if (sampleCount < MAX_SAMPLES)
        {
            samples[sampleCount].lx = (unsigned char)pad.Lx;
            samples[sampleCount].ly = (unsigned char)pad.Ly;
            samples[sampleCount].step = (unsigned char)step;
            sampleCount++;
        }

        if (pad.Lx < lxMin) lxMin = pad.Lx;
        if (pad.Lx > lxMax) lxMax = pad.Lx;
        if (pad.Ly < lyMin) lyMin = pad.Ly;
        if (pad.Ly > lyMax) lyMax = pad.Ly;

        if ((step > STEP_IDLE) && !centreReady) ComputeCentre();

        float nx, ny;
        Normalize(pad.Lx, pad.Ly, &nx, &ny);

        BeginDrawing();
        ClearBackground(BLACK);

        DrawText("pad probe", 10, 10, 20, RAYWHITE);
        DrawText(TextFormat("run %d  step %d/%d", RUN, step + 1, STEP_COUNT), 150, 14, 10, GRAY);
        DrawText(StepLabel(step), 10, 38, 12, YELLOW);

        float remaining = stepStart[step + 1] - (float)elapsed;
        if (remaining < 0.0f) remaining = 0.0f;
        DrawText(TextFormat("remaining %.1f s", remaining), 10, 58, 10, LIGHTGRAY);

        DrawText(TextFormat("raw Lx %3u  Ly %3u    cx %3d  cy %3d", pad.Lx, pad.Ly, centreX, centreY),
            10, 78, 10, SKYBLUE);
        DrawText(TextFormat("norm %+.2f %+.2f  mag %.2f", nx, ny, Magnitude(nx, ny)), 10, 92, 10, GREEN);

        // Raw crosshair: 0..255 mapped into a 160x160 box.
        int boxX = 300;
        int boxY = 60;
        int boxSize = 160;
        DrawRectangleLines(boxX, boxY, boxSize, boxSize, DARKGRAY);
        DrawRectangle(boxX + boxSize/2, boxY + 1, 1, boxSize - 2, DARKGRAY);
        DrawRectangle(boxX + 1, boxY + boxSize/2, boxSize - 2, 1, DARKGRAY);

        int dotX = boxX + (int)((float)pad.Lx*boxSize/255.0f);
        int dotY = boxY + (int)((float)pad.Ly*boxSize/255.0f);
        DrawCircle(dotX, dotY, 3, ORANGE);

        DrawText(TextFormat("samples %d", sampleCount), 10, 250, 10, DARKGRAY);

        EndDrawing();

        if (step != lastStep)
        {
            TraceLog(LOG_INFO, "PAD step=%d label=%s", step, StepLabel(step));
            lastStep = step;
        }

        double frameSeconds = 1.0/60.0;
        double frameElapsed = GetTime() - now;
        if (frameElapsed < frameSeconds) WaitTime(frameSeconds - frameElapsed);
    }

    PrintSummary();
    TraceLog(LOG_INFO, "DONE probe run=%d", RUN);

    for (;;) WaitTime(0.1);
}

#endif
