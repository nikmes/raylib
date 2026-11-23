/**********************************************************************************************
*
*   rlpsp_math - internal VFPU/scalar helpers for the PSP build (P11-S12)
*
*   License-clean layer: wraps the permissive PSPSDK (pspmath, pspgum, pspvfpu) when
*   PSP_VFPU_HELPERS=1, with scalar fallbacks otherwise. Nothing from /home/psp/ref is
*   copied here; the GPL references are behaviour only.
*
*   Thread contract: call rlpsp_vfpu_init() once per thread before the first helper.
*   pspvfpu_initcontext() sets the calling thread's VFPU attribute (see pspvfpu.h).
*   The VFPU helpers use matrix 6; GUM owns 0-3. The context reserves 6-7.
*
*   On-demand: include this header only in the TU that needs it. The camera
*   projections (lookAt/perspective/ortho) stay on raymath until the probe verifies
*   GUM's units/handedness; multiply/vector/quaternion use GUM now.
*
**********************************************************************************************/

#ifndef RLPSP_MATH_H
#define RLPSP_MATH_H

#include <math.h>
#include <stdint.h>
#include <string.h>

#include "raylib.h"
#include "raymath.h"

#ifndef PSP_VFPU_HELPERS
    #define PSP_VFPU_HELPERS 0
#endif
// P10-S12 T3: scoped VFPU sincos only; does not enable the rest of the helper layer.
#ifndef PSP_VFPU_SINCOS
    #define PSP_VFPU_SINCOS 0
#endif
// GUM vec/mat/quat and the VFPU copy measured slower than the inlined scalar
// paths at the sizes raylib uses (P11-S12 probe); they stay opt-in for A/B only.
#ifndef RLPSP_VFPU_GUM
    #define RLPSP_VFPU_GUM 0
#endif
#ifndef RLPSP_VFPU_COPY
    #define RLPSP_VFPU_COPY 0
#endif

#if defined(PLATFORM_PSP) && (PSP_VFPU_HELPERS == 1)
    #include <pspmath.h>
    #include <pspgum.h>
    #include <pspvfpu.h>
    #define RLPSP_VFPU 1
#else
    #define RLPSP_VFPU 0
#endif

#if defined(PLATFORM_PSP) && (PSP_VFPU_SINCOS == 1)
    #include <pspvfpu.h>
    #define RLPSP_VFPU_SINCOS 1
#else
    #define RLPSP_VFPU_SINCOS 0
#endif

#if RLPSP_VFPU || RLPSP_VFPU_SINCOS
void rlpsp_vfpu_init(void);
void rlpsp_vfpu_shutdown(void);
#else
static inline void rlpsp_vfpu_init(void) { }
static inline void rlpsp_vfpu_shutdown(void) { }
#endif

//----------------------------------------------------------------------------------
// Scalar
//----------------------------------------------------------------------------------
static inline float rlpsp_sinf(float x)
{
#if RLPSP_VFPU
    return vfpu_sinf(x);
#else
    return sinf(x);
#endif
}

static inline float rlpsp_cosf(float x)
{
#if RLPSP_VFPU
    return vfpu_cosf(x);
#else
    return cosf(x);
#endif
}

static inline void rlpsp_sincosf(float x, float *s, float *c)
{
#if RLPSP_VFPU
    vfpu_sincos(x, s, c);
#elif RLPSP_VFPU_SINCOS
    // Scoped VFPU path: no libpspmath dependency. Matrix 6 is reserved by
    // rlpsp_vfpu_init(), so C600/S600 cannot clobber GUM's 0-3.
    float xv = x;
    __asm__ volatile(
        "lv.s S000, 0(%2)\n"
        "vcst.s S001, VFPU_2_PI\n"
        "vmul.s S000, S000, S001\n"
        "vrot.p C600, S000, [s, c]\n"
        "mfv %0, S600\n"
        "mfv %1, S601\n"
        : "=r"(*s), "=r"(*c) : "r"(&xv) : "memory");
#else
    if (s != NULL) *s = sinf(x);
    if (c != NULL) *c = cosf(x);
#endif
}

static inline float rlpsp_atan2f(float y, float x)
{
#if RLPSP_VFPU
    return vfpu_atan2f(y, x);
#else
    return atan2f(y, x);
#endif
}

static inline float rlpsp_powf(float x, float y)
{
#if RLPSP_VFPU
    return vfpu_powf(x, y);
#else
    return powf(x, y);
#endif
}

static inline float rlpsp_expf(float x)
{
#if RLPSP_VFPU
    return vfpu_expf(x);
#else
    return expf(x);
#endif
}

static inline float rlpsp_logf(float x)
{
#if RLPSP_VFPU
    return vfpu_logf(x);
#else
    return logf(x);
#endif
}

static inline float rlpsp_sqrtf(float x) { return sqrtf(x); }
static inline float rlpsp_rsqrtf(float x) { return 1.0f/sqrtf(x); }

//----------------------------------------------------------------------------------
// Vector 3
//----------------------------------------------------------------------------------
static inline Vector3 rlpsp_vec3_add(Vector3 a, Vector3 b) { return (Vector3){ a.x + b.x, a.y + b.y, a.z + b.z }; }
static inline Vector3 rlpsp_vec3_sub(Vector3 a, Vector3 b) { return (Vector3){ a.x - b.x, a.y - b.y, a.z - b.z }; }
static inline Vector3 rlpsp_vec3_scale(Vector3 v, float s) { return (Vector3){ v.x*s, v.y*s, v.z*s }; }

static inline float rlpsp_vec3_dot(Vector3 a, Vector3 b)
{
#if RLPSP_VFPU && RLPSP_VFPU_GUM
    ScePspFVector3 va = { a.x, a.y, a.z };
    ScePspFVector3 vb = { b.x, b.y, b.z };
    return gumDotProduct(&va, &vb);
#else
    return a.x*b.x + a.y*b.y + a.z*b.z;
#endif
}

static inline Vector3 rlpsp_vec3_cross(Vector3 a, Vector3 b)
{
#if RLPSP_VFPU && RLPSP_VFPU_GUM
    ScePspFVector3 va = { a.x, a.y, a.z };
    ScePspFVector3 vb = { b.x, b.y, b.z };
    ScePspFVector3 vr;
    gumCrossProduct(&vr, &va, &vb);
    return (Vector3){ vr.x, vr.y, vr.z };
#else
    return Vector3CrossProduct(a, b);
#endif
}

static inline float rlpsp_vec3_length(Vector3 v) { return sqrtf(rlpsp_vec3_dot(v, v)); }

static inline Vector3 rlpsp_vec3_normalize(Vector3 v)
{
#if RLPSP_VFPU && RLPSP_VFPU_GUM
    ScePspFVector3 vv = { v.x, v.y, v.z };
    gumNormalize(&vv);
    return (Vector3){ vv.x, vv.y, vv.z };
#else
    return Vector3Normalize(v);
#endif
}

static inline Vector3 rlpsp_vec3_lerp(Vector3 a, Vector3 b, float t) { return Vector3Lerp(a, b, t); }

//----------------------------------------------------------------------------------
// Vector 4
//----------------------------------------------------------------------------------
static inline Vector4 rlpsp_vec4_add(Vector4 a, Vector4 b) { return (Vector4){ a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w }; }
static inline Vector4 rlpsp_vec4_sub(Vector4 a, Vector4 b) { return (Vector4){ a.x - b.x, a.y - b.y, a.z - b.z, a.w - b.w }; }
static inline Vector4 rlpsp_vec4_scale(Vector4 v, float s) { return (Vector4){ v.x*s, v.y*s, v.z*s, v.w*s }; }
static inline float rlpsp_vec4_dot(Vector4 a, Vector4 b) { return a.x*b.x + a.y*b.y + a.z*b.z + a.w*b.w; }
static inline float rlpsp_vec4_length(Vector4 v) { return sqrtf(rlpsp_vec4_dot(v, v)); }
static inline Vector4 rlpsp_vec4_normalize(Vector4 v) { float l = rlpsp_vec4_length(v); return (l > 0.0f)? rlpsp_vec4_scale(v, 1.0f/l) : v; }

//----------------------------------------------------------------------------------
// Matrix 4x4
//----------------------------------------------------------------------------------
static inline Matrix rlpsp_mat4_identity(void) { return MatrixIdentity(); }
static inline Matrix rlpsp_mat4_transpose(Matrix m) { return MatrixTranspose(m); }
static inline Vector3 rlpsp_mat4_transform(Vector3 v, Matrix m) { return Vector3Transform(v, m); }

static inline Matrix rlpsp_mat4_multiply(Matrix a, Matrix b)
{
#if RLPSP_VFPU && RLPSP_VFPU_GUM
    Matrix r;
    gumMultMatrix((ScePspFMatrix4 *)&r, (const ScePspFMatrix4 *)&a, (const ScePspFMatrix4 *)&b);
    return r;
#else
    return MatrixMultiply(a, b);
#endif
}

// Camera projections stay on raymath until T4 verifies GUM's units/handedness.
static inline Matrix rlpsp_mat4_lookAt(Vector3 eye, Vector3 target, Vector3 up) { return MatrixLookAt(eye, target, up); }
static inline Matrix rlpsp_mat4_perspective(float fovYRadians, float aspect, float nearPlane, float farPlane) { return MatrixPerspective(fovYRadians, aspect, nearPlane, farPlane); }
static inline Matrix rlpsp_mat4_ortho(float left, float right, float bottom, float top, float nearPlane, float farPlane) { return MatrixOrtho(left, right, bottom, top, nearPlane, farPlane); }

//----------------------------------------------------------------------------------
// Quaternion
//----------------------------------------------------------------------------------
static inline Quaternion rlpsp_quat_identity(void) { return QuaternionIdentity(); }

static inline Quaternion rlpsp_quat_multiply(Quaternion a, Quaternion b)
{
#if RLPSP_VFPU && RLPSP_VFPU_GUM
    Quaternion r;
    gumMultQuaternion((ScePspFQuaternion *)&r, (const ScePspFQuaternion *)&a, (const ScePspFQuaternion *)&b);
    return r;
#else
    return QuaternionMultiply(a, b);
#endif
}

static inline Quaternion rlpsp_quat_normalize(Quaternion q)
{
#if RLPSP_VFPU && RLPSP_VFPU_GUM
    gumNormalizeQuaternion((ScePspFQuaternion *)&q);
    return q;
#else
    return QuaternionNormalize(q);
#endif
}

static inline Quaternion rlpsp_quat_from_axis_angle(Vector3 axis, float angleRadians)
{
#if RLPSP_VFPU && RLPSP_VFPU_GUM
    Quaternion q;
    ScePspFVector3 a = { axis.x, axis.y, axis.z };
    gumLoadAxisAngle((ScePspFQuaternion *)&q, &a, angleRadians);
    return q;
#else
    return QuaternionFromAxisAngle(axis, angleRadians);
#endif
}

static inline Vector3 rlpsp_quat_rotate(Vector3 v, Quaternion q)
{
#if RLPSP_VFPU && RLPSP_VFPU_GUM
    ScePspFVector3 vv = { v.x, v.y, v.z };
    ScePspFVector3 vr;
    gumRotateVector(&vr, (const ScePspFQuaternion *)&q, &vv);
    return (Vector3){ vr.x, vr.y, vr.z };
#else
    return Vector3RotateByQuaternion(v, q);
#endif
}

//----------------------------------------------------------------------------------
// Packed colour (RGBA8 <-> 565/5551/4444)
//----------------------------------------------------------------------------------
static inline uint16_t rlpsp_color_to_565(Color c) { return (uint16_t)(((c.r >> 3) << 11) | ((c.g >> 2) << 5) | (c.b >> 3)); }
static inline uint16_t rlpsp_color_to_5551(Color c) { return (uint16_t)(((c.r >> 3) << 11) | ((c.g >> 3) << 6) | ((c.b >> 3) << 1) | (c.a >> 7)); }
static inline uint16_t rlpsp_color_to_4444(Color c) { return (uint16_t)(((c.r >> 4) << 12) | ((c.g >> 4) << 8) | ((c.b >> 4) << 4) | (c.a >> 4)); }

static inline Color rlpsp_color_from_565(uint16_t v)
{
    Color c = { (unsigned char)(((v >> 11) & 0x1F) << 3), (unsigned char)(((v >> 5) & 0x3F) << 2), (unsigned char)((v & 0x1F) << 3), 255 };
    return c;
}

static inline Color rlpsp_color_from_5551(uint16_t v)
{
    Color c = { (unsigned char)(((v >> 11) & 0x1F) << 3), (unsigned char)(((v >> 6) & 0x1F) << 3), (unsigned char)(((v >> 1) & 0x1F) << 3), (unsigned char)((v & 1)? 255 : 0) };
    return c;
}

static inline Color rlpsp_color_from_4444(uint16_t v)
{
    Color c = { (unsigned char)(((v >> 12) & 0xF) << 4), (unsigned char)(((v >> 8) & 0xF) << 4), (unsigned char)(((v >> 4) & 0xF) << 4), (unsigned char)((v & 0xF) << 4) };
    return c;
}

//----------------------------------------------------------------------------------
// Bulk copy: 64-byte lv.q/sv.q blocks, memcpy for unaligned bytes.
// Matrix 6 is reserved by rlpsp_vfpu_init(), so GUM (0-3) is untouched.
//----------------------------------------------------------------------------------
static inline void rlpsp_copy64(void *dst, const void *src, size_t bytes)
{
#if RLPSP_VFPU && RLPSP_VFPU_COPY
    if ((((uintptr_t)src | (uintptr_t)dst) & 15) == 0)
    {
        while (bytes >= 64)
        {
            __asm__ volatile(
                "lv.q R600, 0(%0)\n"
                "lv.q R601, 16(%0)\n"
                "lv.q R602, 32(%0)\n"
                "lv.q R603, 48(%0)\n"
                "sv.q R600, 0(%1)\n"
                "sv.q R601, 16(%1)\n"
                "sv.q R602, 32(%1)\n"
                "sv.q R603, 48(%1)\n"
                :
                : "r"(src), "r"(dst)
                : "memory");

            src = (const unsigned char *)src + 64;
            dst = (unsigned char *)dst + 64;
            bytes -= 64;
        }
    }
#endif
    if (bytes > 0) memcpy(dst, src, bytes);
}

#endif // RLPSP_MATH_H
