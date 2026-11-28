/**********************************************************************************************
*
*   rlpsp_mesh.h - entry points of the quarantined PSP 3D mesh path (P10-S25 T1).
*
*   Include this from raylib core files instead of carrying the implementation there. The
*   whole surface is compiled only for PSP with GL 1.1, so a desktop build sees nothing.
*
**********************************************************************************************/

#ifndef RLPSP_MESH_H
#define RLPSP_MESH_H

#if defined(PLATFORM_PSP) && defined(GRAPHICS_API_OPENGL_11)

#include "raylib.h"

// One-time GE-native copy of a static mesh: texcoord f32, normal f32, position s16 about
// the mesh centre, 28-byte stride. The model matrix restores centre and scale.
typedef struct {
    unsigned int *key;                  // mesh->vboId, unique per mesh and copied by value
    unsigned char *verts;
    unsigned int stride;
    float cx, cy, cz;
    float K;
    float model[16];                    // P10-S21 T4b: the constant scale+centre product
    float bmin[3], bmax[3];
} PspGeMesh;

void rlpspEnableCulling(int enable);
void pspSetCleanHint(int on);

unsigned int *pspGeKey(Mesh *mesh);
PspGeMesh *pspGeFind(unsigned int *key);
void pspGeRemove(unsigned int *key);
void pspGeRemoveBounds(unsigned int *key);
void pspGeStoreBounds(Mesh *mesh, bool dynamic);
void pspGePack(Mesh *mesh, bool dynamic);

int pspGeClassifyMesh(Mesh *mesh, Matrix transform);

int pspMergeEligible(Mesh *mesh, Material *material, Matrix transform);
int pspMergeRoom(Mesh *mesh);
void pspMergeAppend(Mesh *mesh, Material *material, Matrix transform);
void rlpspMergeFlush(void);

// Counters and probe helpers, defined in rlpsp_mesh.c and used by DrawMesh's call sites.
extern unsigned int rlpspMergeRuns;
extern unsigned int rlpspMergeInstances;
extern unsigned int rlpspMergeReplays;
extern unsigned int rlpspClassClean;
extern unsigned int rlpspClassDirty;
extern unsigned int rlpspClassCull;

#if defined(PSP_MATRIX_PROBE) || defined(PSP_CALL_PROBE)
    #include <pspkernel.h>
    long long rlpspProbeNow(void);
#endif
#if defined(PSP_MATRIX_PROBE)
extern unsigned long long rlpspProbeComposeUs;
extern unsigned long long rlpspProbeMultUs;
extern unsigned long long rlpspProbeCullUs;
extern unsigned int rlpspProbeDraws;
#endif
#if defined(PSP_CALL_PROBE)
extern unsigned long long rlpspProbeDrawMeshUs;
extern unsigned long long rlpspProbeDriverUs;
extern unsigned long long rlpspProbeStateUs;
extern unsigned long long rlpspMergeBuildUs;
extern unsigned long long rlpspMergeFlushUs;
extern unsigned int rlpspMergeProbeCount;
extern unsigned int rlpspProbeCalls;
void rlpspProbeReport(void);
unsigned long long rlpspProbeTail(int n);
#endif


// Call-site scaffolding: the probes are implementation, so the macros live here and DrawMesh
// keeps one line per seam. Empty in every shipped build.
#if defined(PSP_CALL_PROBE)
    #define PSP_CALL_BEGIN()    SceInt64 _dm0 = rlpspProbeNow()
    #define PSP_CALL_END()      do { rlpspProbeDrawMeshUs += (unsigned long long)(rlpspProbeNow() - _dm0); \
                                     if (++rlpspProbeCalls >= 2000) rlpspProbeReport(); } while (0)
    #define PSP_STATE_BEGIN()   SceInt64 _st0 = rlpspProbeNow()
    #define PSP_STATE_END()     rlpspProbeStateUs += (unsigned long long)(rlpspProbeNow() - _st0)
    #define PSP_DRIVER_BEGIN()  SceInt64 _d0 = rlpspProbeNow()
    #define PSP_DRIVER_END()    rlpspProbeDriverUs += (unsigned long long)(rlpspProbeNow() - _d0)
#else
    #define PSP_CALL_BEGIN()
    #define PSP_CALL_END()
    #define PSP_STATE_BEGIN()
    #define PSP_STATE_END()
    #define PSP_DRIVER_BEGIN()
    #define PSP_DRIVER_END()
#endif
#if defined(PSP_MATRIX_PROBE)
    #define PSP_CULL_BEGIN()    SceInt64 _c0 = rlpspProbeNow()
    #define PSP_CULL_END()      rlpspProbeCullUs += (unsigned long long)(rlpspProbeNow() - _c0)
    #define PSP_MTX_BEGIN()     SceInt64 _m0 = rlpspProbeNow()
    #define PSP_MTX_LAP(us)     do { us += (unsigned long long)(rlpspProbeNow() - _m0); _m0 = rlpspProbeNow(); } while (0)
    #define PSP_MTX_END(us)     us += (unsigned long long)(rlpspProbeNow() - _m0)
    #define PSP_DRAWS_INC()     (rlpspProbeDraws++)
#else
    #define PSP_CULL_BEGIN()
    #define PSP_CULL_END()
    #define PSP_MTX_BEGIN()
    #define PSP_MTX_LAP(us)
    #define PSP_MTX_END(us)
    #define PSP_DRAWS_INC()
#endif

#endif // PLATFORM_PSP && GRAPHICS_API_OPENGL_11
#endif // RLPSP_MESH_H
