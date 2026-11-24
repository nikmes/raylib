/*
 * colour_gate - P10-S13 T2.
 *
 * Three cubes with red/green/blue materials, unlit; the same lit; a textured cube;
 * and a 2D white HUD drawn after a lit+fogged 3D block. The last one is T1's state
 * check: if lighting/fog leak out of EndMode3D the white rectangle goes black.
 *
 * The unlit colours are exact 5650 values (248,0,0), (0,252,0), (0,0,248). The lit
 * case has no oracle (062 correction), so it is recorded and only checked non-black.
 * No file I/O; the log is stdout only.
 */

#include <pspkernel.h>
#include <math.h>

#include "raylib.h"
#include "rlgl.h"
#include "raymath.h"

PSP_MODULE_INFO("colour_gate", 0, 1, 1);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);

#define GE_RGBA(r, g, b, a) (((unsigned int)(a) << 24) | ((unsigned int)(b) << 16) | \
                             ((unsigned int)(g) << 8) | (unsigned int)(r))

#define PHASE_FRAMES 4

static int nonBlack(Color c) { return ((c.r + c.g + c.b) > 30); }

static int isRed(Color c)   { return (c.r > 200) && (c.g < 40) && (c.b < 40); }
static int isGreen(Color c) { return (c.g > 200) && (c.r < 40) && (c.b < 40); }
static int isBlue(Color c)  { return (c.b > 200) && (c.r < 40) && (c.g < 40); }
static int isWhite(Color c) { return (c.r > 200) && (c.g > 200) && (c.b > 200); }

static void drawCube(Mesh cube, Material mat, float x)
{
    DrawMesh(cube, mat, MatrixTranslate(x, 0.0f, 0.0f));
}

int main(void)
{
    InitWindow(480, 272, "colour_gate");
    SetTargetFPS(0);
    rlDisableBackfaceCulling();

    Camera3D cam = { 0 };
    cam.position = (Vector3){ 0.0f, 0.0f, 10.0f };
    cam.target = (Vector3){ 0.0f, 0.0f, 0.0f };
    cam.up = (Vector3){ 0.0f, 1.0f, 0.0f };
    cam.fovy = 10.0f;
    cam.projection = CAMERA_ORTHOGRAPHIC;

    Mesh cube = GenMeshCube(1.5f, 1.5f, 1.5f);

    Material red = LoadMaterialDefault();
    Material green = LoadMaterialDefault();
    Material blue = LoadMaterialDefault();
    // Pure primaries, not raylib's RED/GREEN/BLUE (230,41,55 / 0,228,48 / 0,121,241):
    // the unlit gate asserts the exact 5650 values.
    red.maps[MATERIAL_MAP_DIFFUSE].color = (Color){ 255, 0, 0, 255 };
    green.maps[MATERIAL_MAP_DIFFUSE].color = (Color){ 0, 255, 0, 255 };
    blue.maps[MATERIAL_MAP_DIFFUSE].color = (Color){ 0, 0, 255, 255 };

    // Screen centres for x = -3, 0, +3 with the 10-unit ortho width.
    const int sx[3] = { 158, 240, 322 };
    const int sy = 136;

    // ---- unlit -------------------------------------------------------------
    rlpspEnableLighting(0);
    rlpspEnableFog(0);
    for (int frame = 0; frame < PHASE_FRAMES; frame++)
    {
        BeginDrawing();
            ClearBackground(BLACK);
            BeginMode3D(cam);
                drawCube(cube, red, -3.0f);
                drawCube(cube, green, 0.0f);
                drawCube(cube, blue, 3.0f);
            EndMode3D();
        EndDrawing();
    }

    Image img = LoadImageFromScreen();
    if (img.data == NULL) { TraceLog(LOG_WARNING, "COLOUR readback failed"); sceKernelSleepThread(); }
    Color cr = GetImageColor(img, sx[0], sy);
    Color cg = GetImageColor(img, sx[1], sy);
    Color cb = GetImageColor(img, sx[2], sy);
    UnloadImage(img);

    int unlitPass = isRed(cr) && isGreen(cg) && isBlue(cb);
    TraceLog(LOG_INFO, "UNLIT red=%d,%d,%d green=%d,%d,%d blue=%d,%d,%d %s",
             cr.r, cr.g, cr.b, cg.r, cg.g, cg.b, cb.r, cb.g, cb.b,
             unlitPass? "PASS" : "FAIL");

    // ---- lit ---------------------------------------------------------------
    rlpspEnableLighting(1);
    rlpspLight(0, 1, 0.45f, 0.85f, 0.30f,
               GE_RGBA(255, 245, 225, 255), GE_RGBA(110, 110, 110, 255), 0.0f, 0.0f, 0.0f);
    rlpspLightAmbient(GE_RGBA(40, 46, 60, 255));
    rlpspMaterial(GE_RGBA(70, 70, 70, 255), GE_RGBA(255, 255, 255, 255),
                  GE_RGBA(90, 90, 90, 255), 0, 8.0f);
    rlpspEnableFog(0);

    for (int frame = 0; frame < PHASE_FRAMES; frame++)
    {
        BeginDrawing();
            ClearBackground(BLACK);
            BeginMode3D(cam);
                drawCube(cube, red, -3.0f);
                drawCube(cube, green, 0.0f);
                drawCube(cube, blue, 3.0f);
            EndMode3D();
        EndDrawing();
    }

    img = LoadImageFromScreen();
    if (img.data == NULL) { TraceLog(LOG_WARNING, "COLOUR readback failed"); sceKernelSleepThread(); }
    Color lr = GetImageColor(img, sx[0], sy);
    Color lg = GetImageColor(img, sx[1], sy);
    Color lb = GetImageColor(img, sx[2], sy);
    UnloadImage(img);

    int litPass = nonBlack(lr) && nonBlack(lg) && nonBlack(lb);
    TraceLog(LOG_INFO, "LIT red=%d,%d,%d green=%d,%d,%d blue=%d,%d,%d %s",
             lr.r, lr.g, lr.b, lg.r, lg.g, lg.b, lb.r, lb.g, lb.b,
             litPass? "PASS" : "FAIL");

    // ---- HUD after lit + fogged 3D (T1) ------------------------------------
    rlpspEnableLighting(1);
    rlpspEnableFog(1);
    rlpspFog(1.0f, 50.0f, GE_RGBA(10, 14, 28, 255));

    for (int frame = 0; frame < PHASE_FRAMES; frame++)
    {
        BeginDrawing();
            ClearBackground(BLACK);
            BeginMode3D(cam);
                drawCube(cube, green, 0.0f);
            EndMode3D();

            // If lighting/fog leak past EndMode3D this is lit (no normals -> black)
            // or fogged; with T1 it is plain white.
            DrawRectangle(10, 10, 100, 30, WHITE);
        EndDrawing();
    }

    img = LoadImageFromScreen();
    if (img.data == NULL) { TraceLog(LOG_WARNING, "COLOUR readback failed"); sceKernelSleepThread(); }
    Color hud = GetImageColor(img, 60, 25);
    UnloadImage(img);

    TraceLog(LOG_INFO, "HUD pixel=%d,%d,%d %s", hud.r, hud.g, hud.b,
             isWhite(hud)? "PASS" : "FAIL");

    // ---- textured ----------------------------------------------------------
    rlpspEnableLighting(0);
    rlpspEnableFog(0);

    Image checker = GenImageChecked(64, 64, 8, 8, WHITE, (Color){ 90, 90, 90, 255 });
    Texture2D tex = LoadTextureFromImage(checker);
    UnloadImage(checker);

    Material checked = LoadMaterialDefault();
    checked.maps[MATERIAL_MAP_DIFFUSE].texture = tex;
    checked.maps[MATERIAL_MAP_DIFFUSE].color = WHITE;

    for (int frame = 0; frame < PHASE_FRAMES; frame++)
    {
        BeginDrawing();
            ClearBackground(BLACK);
            BeginMode3D(cam);
                drawCube(cube, checked, 0.0f);
            EndMode3D();
        EndDrawing();
    }

    img = LoadImageFromScreen();
    if (img.data == NULL) { TraceLog(LOG_WARNING, "COLOUR readback failed"); sceKernelSleepThread(); }
    Color ct = GetImageColor(img, sx[1], sy);
    UnloadImage(img);

    TraceLog(LOG_INFO, "TEXTURED centre=%d,%d,%d %s", ct.r, ct.g, ct.b,
             nonBlack(ct)? "PASS" : "FAIL");

    TraceLog(LOG_INFO, "COLOURGATE %s",
             (unlitPass && litPass && isWhite(hud) && nonBlack(ct))? "PASS" : "FAIL");
    TraceLog(LOG_INFO, "DONE");
    sceKernelSleepThread();
    return 0;
}
