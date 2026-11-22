/*
 * math_probe - P11-S12 helper-layer probe
 *
 * Times and checks src/rlpsp_math.h in both arms:
 *   make PSP_VFPU_HELPERS=1   -> SDK wrappers + VFPU
 *   make PSP_VFPU_HELPERS=0   -> scalar fallback
 *
 * Headless: no window, no renderer. Logs to the PSPLink console.
 */

#include <pspkernel.h>
#include <stdio.h>
#include <math.h>
#include <string.h>

#define RAYMATH_STATIC_INLINE
#include "../../src/platforms/rlpsp_math.c"

PSP_MODULE_INFO("math_probe", 0, 1, 1);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);

#define ITERS 10000

static volatile float sink;
static unsigned char copySrc[1 << 20];
static unsigned char copyDst[1 << 20];

static SceInt64 Now(void) { return sceKernelGetSystemTimeWide(); }

static float MaxDiffMatrix(Matrix a, Matrix b)
{
    const float *pa = (const float *)&a;
    const float *pb = (const float *)&b;
    float d = 0.0f;
    for (int i = 0; i < 16; i++)
    {
        float v = fabsf(pa[i] - pb[i]);
        if (v > d) d = v;
    }
    return d;
}

static float MaxDiffQuat(Quaternion a, Quaternion b)
{
    float d = fabsf(a.x - b.x);
    if (fabsf(a.y - b.y) > d) d = fabsf(a.y - b.y);
    if (fabsf(a.z - b.z) > d) d = fabsf(a.z - b.z);
    if (fabsf(a.w - b.w) > d) d = fabsf(a.w - b.w);
    return d;
}

static float MaxDiffVec3(Vector3 a, Vector3 b)
{
    float d = fabsf(a.x - b.x);
    if (fabsf(a.y - b.y) > d) d = fabsf(a.y - b.y);
    if (fabsf(a.z - b.z) > d) d = fabsf(a.z - b.z);
    return d;
}

int main(void)
{
    rlpsp_vfpu_init();
    printf("MATH helpers=%d\n", PSP_VFPU_HELPERS);

    float t0, dt;
    int i;

    // Scalar time
    t0 = (float)Now();
    for (i = 0; i < ITERS; i++) sink += rlpsp_sinf((float)(i%360)*0.0174532925f);
    dt = (float)(Now() - t0)/ITERS;
    printf("MATH sin us=%.3f\n", dt);

    t0 = (float)Now();
    for (i = 0; i < ITERS; i++) sink += rlpsp_cosf((float)(i%360)*0.0174532925f);
    dt = (float)(Now() - t0)/ITERS;
    printf("MATH cos us=%.3f\n", dt);

    t0 = (float)Now();
    for (i = 0; i < ITERS; i++) sink += rlpsp_atan2f((float)(i%360), (float)(i%90 + 1));
    dt = (float)(Now() - t0)/ITERS;
    printf("MATH atan2 us=%.3f\n", dt);

    t0 = (float)Now();
    for (i = 0; i < ITERS; i++) sink += rlpsp_powf(1.0f + (float)(i%100)/100.0f, 1.5f);
    dt = (float)(Now() - t0)/ITERS;
    printf("MATH pow us=%.3f\n", dt);

    // Accuracy (same loop, compared with libm)
    float maxErr = 0.0f;
    for (i = 0; i < 1000; i++)
    {
        float x = (float)(i - 500)*0.01f;
        float e = fabsf(rlpsp_sinf(x) - sinf(x));
        if (e > maxErr) maxErr = e;
    }
    printf("MATH sin err=%.8f\n", maxErr);

    maxErr = 0.0f;
    for (i = 0; i < 1000; i++)
    {
        float y = (float)(i - 500)*0.01f;
        float e = fabsf(rlpsp_atan2f(y, 1.0f) - atan2f(y, 1.0f));
        if (e > maxErr) maxErr = e;
    }
    printf("MATH atan2 err=%.8f\n", maxErr);

    // Vector 3
    Vector3 va = { 1.0f, 2.0f, 3.0f };
    Vector3 vb = { -4.0f, 5.0f, 6.0f };
    t0 = (float)Now();
    for (i = 0; i < ITERS; i++)
    {
        Vector3 v = { (float)(i%7), 2.0f, 3.0f };
        sink += rlpsp_vec3_dot(v, vb);
    }
    dt = (float)(Now() - t0)/ITERS;
    printf("MATH vec3dot us=%.3f diff=%.8f\n", dt, fabsf(rlpsp_vec3_dot(va, vb) - Vector3DotProduct(va, vb)));

    t0 = (float)Now();
    for (i = 0; i < ITERS; i++)
    {
        Vector3 v = { (float)(i%7), 2.0f, 3.0f };
        sink += rlpsp_vec3_cross(v, vb).x;
    }
    dt = (float)(Now() - t0)/ITERS;
    printf("MATH vec3cross us=%.3f diff=%.8f\n", dt, MaxDiffVec3(rlpsp_vec3_cross(va, vb), Vector3CrossProduct(va, vb)));

    t0 = (float)Now();
    for (i = 0; i < ITERS; i++)
    {
        Vector3 v = { 1.0f + (float)(i%7), 2.0f, 3.0f };
        sink += rlpsp_vec3_normalize(v).x;
    }
    dt = (float)(Now() - t0)/ITERS;
    printf("MATH vec3norm us=%.3f diff=%.8f\n", dt, MaxDiffVec3(rlpsp_vec3_normalize(va), Vector3Normalize(va)));

    // Matrix multiply
    Matrix ma = MatrixRotateXYZ((Vector3){ 0.2f, 0.4f, 0.1f });
    Matrix mb = MatrixTranslate(1.0f, 2.0f, 3.0f);
    t0 = (float)Now();
    for (i = 0; i < ITERS; i++) { Matrix mm = rlpsp_mat4_multiply(ma, mb); sink += ((const float *)&mm)[0]; }
    dt = (float)(Now() - t0)/ITERS;
    printf("MATH mat4mul us=%.3f diff=%.8f\n", dt, MaxDiffMatrix(rlpsp_mat4_multiply(ma, mb), MatrixMultiply(ma, mb)));

    // Quaternion
    Quaternion qa = QuaternionFromAxisAngle((Vector3){ 0.0f, 1.0f, 0.0f }, 0.3f);
    Quaternion qb = QuaternionFromAxisAngle((Vector3){ 1.0f, 0.0f, 0.0f }, -0.7f);
    t0 = (float)Now();
    for (i = 0; i < ITERS; i++) sink += rlpsp_quat_multiply(qa, qb).w;
    dt = (float)(Now() - t0)/ITERS;
    printf("MATH quatmul us=%.3f diff=%.8f\n", dt, MaxDiffQuat(rlpsp_quat_multiply(qa, qb), QuaternionMultiply(qa, qb)));

    Quaternion qAxis = rlpsp_quat_from_axis_angle((Vector3){ 0.0f, 1.0f, 0.0f }, 0.5f);
    printf("MATH quataxis diff=%.8f\n", MaxDiffQuat(qAxis, QuaternionFromAxisAngle((Vector3){ 0.0f, 1.0f, 0.0f }, 0.5f)));
    printf("MATH quatrot diff=%.8f\n", MaxDiffVec3(rlpsp_quat_rotate(va, qa), Vector3RotateByQuaternion(va, qa)));
    printf("MATH quatnorm diff=%.8f\n", MaxDiffQuat(rlpsp_quat_normalize(qa), QuaternionNormalize(qa)));

    // Copy
    for (i = 0; i < (1 << 20); i++) copySrc[i] = (unsigned char)i;
    size_t sizes[] = { 64, 256, 4096, 1 << 20 };
    for (int s = 0; s < 4; s++)
    {
        size_t bytes = sizes[s];
        t0 = (float)Now();
        for (i = 0; i < 8; i++) rlpsp_copy64(copyDst, copySrc, bytes);
        dt = (float)(Now() - t0)/8.0f;
        printf("MATH copy%u us=%.3f ok=%d\n", (unsigned int)bytes, dt, memcmp(copyDst, copySrc, bytes) == 0);

        t0 = (float)Now();
        for (i = 0; i < 8; i++) memcpy(copyDst, copySrc, bytes);
        dt = (float)(Now() - t0)/8.0f;
        printf("MATH memcpy%u us=%.3f\n", (unsigned int)bytes, dt);
    }

    // Packed colour round trips
    Color c = { 10, 20, 30, 200 };
    uint16_t v565 = rlpsp_color_to_565(c);
    Color b565 = rlpsp_color_from_565(v565);
    printf("MATH 565 %04x %d,%d,%d\n", v565, b565.r, b565.g, b565.b);

    uint16_t v5551 = rlpsp_color_to_5551(c);
    Color b5551 = rlpsp_color_from_5551(v5551);
    printf("MATH 5551 %04x %d,%d,%d,%d\n", v5551, b5551.r, b5551.g, b5551.b, b5551.a);

    uint16_t v4444 = rlpsp_color_to_4444(c);
    Color b4444 = rlpsp_color_from_4444(v4444);
    printf("MATH 4444 %04x %d,%d,%d,%d\n", v4444, b4444.r, b4444.g, b4444.b, b4444.a);

    printf("MATH DONE sink=%f\n", sink);
    rlpsp_vfpu_shutdown();
    sceKernelSleepThread();
    return 0;
}
