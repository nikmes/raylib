/*
 * texture_residency - sprint 016: does the eviction policy behave?
 *
 * Modes (make MODE=...):
 *   fit      12 x 128x128 RGBA8888 = 786,432 B, below the ~913 KiB free. Draw 120
 *            frames; the policy must not evict or coalesce.
 *   over     28 x 128x128 = 1,835,008 B. Loading and drawing forces evictions and
 *            reloads; no allocation may fail.
 *   priority load 12, set two of them 1.0/0.1, age the frames, load 4 more; the
 *            0.1 texture must be the victim. PRIORITY_SWAP=1 swaps the pair and the
 *            expected victim.
 *   frag     mixed sizes, unload every third, then a 256x256 allocation; the large
 *            allocation must succeed and be resident.
 *
 * The verdict is written to host0:/residency_done.txt, so a lost console window cannot
 * lose it. Run default 8888: the residency policy is format-agnostic.
 */

#include <pspkernel.h>
#include <stdio.h>
#include <stdlib.h>

#include "raylib.h"
#include "rlgl.h"
#include <GL/gl.h>

// The driver's test-facing symbols; the struct must match pspgu_internal.h.
typedef struct {
    unsigned int resident;
    unsigned int shadowBytes;
    unsigned int evictions;
    unsigned int reloads;
    unsigned int failed;
    unsigned int coalesced;
} PspGuResidencyStats;

void pspguVramReport(const char *when);
void pspguResidencyStats(PspGuResidencyStats *out);

PSP_MODULE_INFO("texture_residency", 0, 1, 1);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);

#define MAX_TEX 40
static Texture2D tex[MAX_TEX];
static int texCount = 0;

static void makeTex(int w, int h, int seed)
{
    if (texCount >= MAX_TEX) return;

    Image img = GenImageColor(w, h, (Color){
        (unsigned char)(30 + (seed*53) % 200),
        (unsigned char)(30 + (seed*97) % 200),
        (unsigned char)(30 + (seed*29) % 200),
        255
    });

    tex[texCount++] = LoadTextureFromImage(img);
    UnloadImage(img);
}

static void drawAll(int n)
{
    BeginDrawing();
        ClearBackground(BLACK);
        for (int i = 0; i < n; i++)
        {
            DrawTextureEx(tex[i], (Vector2){ 4 + (i%10)*46, 4 + (i/10)*46 }, 0.0f, 0.25f, WHITE);
        }
    EndDrawing();
}

static void emptyFrames(int n)
{
    for (int i = 0; i < n; i++)
    {
        BeginDrawing();
            ClearBackground(BLACK);
        EndDrawing();
    }
}

static void finish(const char *mode, int a, int b, int pass)
{
    PspGuResidencyStats st;

    pspguResidencyStats(&st);
    pspguVramReport("end");

    SaveFileText("residency_done.txt",
                 TextFormat("DONE mode=%s resident=%u shadow=%u evictions=%u reloads=%u failed=%u coalesced=%u a=%d b=%d verdict=%s\n",
                            mode, st.resident, st.shadowBytes, st.evictions, st.reloads,
                            st.failed, st.coalesced, a, b, pass? "PASS" : "FAIL"));

    TraceLog(LOG_WARNING,
             "DONE residency mode=%s evictions=%u reloads=%u failed=%u coalesced=%u a=%d b=%d %s",
             mode, st.evictions, st.reloads, st.failed, st.coalesced, a, b,
             pass? "PASS" : "FAIL");

    sceKernelSleepThread();
}

int main(void)
{
    InitWindow(480, 272, "texture residency");
    SetTargetFPS(0);
    SetTraceLogLevel(LOG_WARNING);      // no per-texture INFO over PSPLink

    pspguVramReport("start");

#if defined(MODE_FIT)
    for (int i = 0; i < 12; i++) makeTex(128, 128, i);
    for (int f = 0; f < 120; f++) drawAll(12);

    {
        PspGuResidencyStats st;
        pspguResidencyStats(&st);
        finish("fit", -1, -1, (st.evictions == 0) && (st.failed == 0));
    }
#elif defined(MODE_OVER)
    for (int i = 0; i < 28; i++) makeTex(128, 128, i);
    for (int f = 0; f < 120; f++) drawAll(28);

    {
        PspGuResidencyStats st;
        pspguResidencyStats(&st);
        finish("over", -1, -1, (st.evictions > 0) && (st.reloads > 0) && (st.failed == 0));
    }
#elif defined(MODE_PRIORITY)
    for (int i = 0; i < 12; i++) makeTex(128, 128, i);

    drawAll(12);            // mark lastUsed
    emptyFrames(4);         // age past the 2-frame pin

    GLuint ids[12];
    GLclampf pri[12];
    for (int i = 0; i < 12; i++) { ids[i] = tex[i].id; pri[i] = 0.5f; }

    int high = 2, low = 3;  // texture array indices, after the default texel
#ifdef PRIORITY_SWAP
    high = 3; low = 2;
#endif
    pri[high] = 1.0f;
    pri[low] = 0.1f;
    glPrioritizeTextures(12, ids, pri);

    for (int i = 0; i < 4; i++) makeTex(128, 128, 12 + i);

    {
        GLuint q[2] = { tex[high].id, tex[low].id };
        GLboolean res[2] = { GL_FALSE, GL_FALSE };
        glAreTexturesResident(2, q, res);

        // high must stay resident, low must be the victim.
        finish("priority", res[0], res[1],
               (res[0] == GL_TRUE) && (res[1] == GL_FALSE));
    }
#elif defined(MODE_FRAG)
    {
        // Fill with 12 x 128x128 (786,432 B), unload every third (four 64 KiB holes),
        // then request a 256x256 (262,144 B). The free tail alone is too small and the
        // holes are too small; eviction must coalesce them.
        for (int i = 0; i < 12; i++) makeTex(128, 128, i);
        drawAll(12);

        for (int i = 0; i < 12; i += 3) UnloadTexture(tex[i]);

        makeTex(256, 256, 20);

        GLuint big = tex[texCount - 1].id;
        GLboolean resident = GL_FALSE;
        glAreTexturesResident(1, &big, &resident);

        PspGuResidencyStats st;
        pspguResidencyStats(&st);

        finish("frag", resident, -1,
               (resident == GL_TRUE) && (st.evictions > 0) && (st.coalesced > 0));
    }
#else
#error "Set MODE=fit|over|priority|frag"
#endif

    return 0;
}
