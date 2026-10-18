/*
===========================================================================
flare (Call of Duty, 2003).

Distributed under the terms in COPYING (GPL version 3 or later,
with id Software's additional terms).

Skeletal models, animation records and the DObj interface.
===========================================================================
*/
#ifndef FLARE_XMODEL_H
#define FLARE_XMODEL_H

#ifndef FLARE_QBOOLEAN_DEFINED
#define FLARE_QBOOLEAN_DEFINED
typedef enum { qfalse, qtrue } qboolean;
#endif

typedef struct XAnimParent_s {
    unsigned short flags;
    unsigned short children;
} XAnimParent_s;

typedef struct XAnimEntry_s {
    unsigned short numAnims;
    unsigned short parent;
    union {
        void *parts;
        XAnimParent_s animParent;
    } u;
} XAnimEntry_s;

typedef struct XAnim_s {
    const char *debugName;
    unsigned int size;
    XAnimEntry_s entries[1];
} XAnim_s;

typedef struct XAnimTree_s {
    XAnim_s *anims;
    unsigned int animToModelIndex;
    unsigned short infoArray[1];
} XAnimTree_s;

/* Bone quaternion, translation weight and translation. */
typedef struct DObjAnimMat_s {
    float quat[4];
    float transWeight;
    float trans[3];
} DObjAnimMat_s;

typedef struct DObjSkelMat {
    float axis[3][4];
    float origin[4];
} DObjSkelMat;

typedef struct XBoneInfo_s {
    float bounds[2][3];
    float offset[3];
    float radiusSquared;
} XBoneInfo_s;

typedef struct XBoneNames_s {
    short size;
    unsigned short names[1];
} XBoneNames_s;

typedef struct XBoneHierarchy_s {
    XBoneNames_s *names;
    unsigned char parentList[1];
} XBoneHierarchy_s;

typedef struct XModelPartsInfo {
    XBoneHierarchy_s *hierarchy;
    short numRootBones;
    short flags;
    XBoneInfo_s *boneInfo;
    short (*quats)[4];
    float (*trans)[3];
    unsigned char *partClassification;
} XModelPartsInfo;

typedef struct XModelPartsFile_s {
    const char *name;
    XModelPartsInfo *data;
    void (*free)(struct XModelPartsFile_s *);
} XModelPartsFile_s;

/* Surface vertices, triangles, skinning data and renderer state. */
typedef struct XSurface_s {
    unsigned char tileMode;
    unsigned char deformed;
    short numVerts;
    short numTris;
    short boneIndex;
    int partBits[4];
    void *vertexInfo; /* Additional bone influences for blended vertices. */
    unsigned short *triIndexes;
    void *blendInfo; /* Rigid vertices or primary blend records. */
    float (*texCoords)[2];
    unsigned int reserved;
    void *renderData[2];
} XSurface_s;

typedef struct XModelSurfs_s {
    struct XModelSurfs_s *next;
    short numSurfs;
    XSurface_s **surfs;
} XModelSurfs_s;

typedef struct XModelSurfsFile_s {
    const char *name;
    XModelSurfs_s *data;
    void (*free)(struct XModelSurfsFile_s *);
} XModelSurfsFile_s;

typedef struct XModelLodInfo_s {
    float dist;
    const char *filename;
    short numsurfs;
    short surfIndex;
    unsigned short *surfNames;
    XModelSurfsFile_s *surfs;
} XModelLodInfo_s;

typedef struct XModelInfo {
    XModelPartsFile_s *parts;
    XModelLodInfo_s lodInfo[3];
    void *collSurfs;
    int numCollSurfs;
    int contents;
    float mins[3];
    float maxs[3];
    short numLods;
    short collLod;
} XModelInfo;

typedef struct XModel_s {
    const char *name;
    XModelInfo *data;
    void (*free)(struct XModel_s *);
} XModel_s;

typedef struct DSkel_s {
    int animPartBits[4];
    int controlPartBits[4];
    int skelPartBits[4];
    DObjSkelMat mat[1];
} DSkel_s;

#define DOBJ_MAX_SUBMODELS 8

typedef struct DObj_s {
    XAnimTree_s *tree;
    DSkel_s *skel;
    int timeStamp;
    unsigned short *animToModel;
    unsigned short skelFlags;
    unsigned short entnum;
    unsigned short duplicateParts;
    unsigned char numModels;
    unsigned char numBones;
    XModel_s *models[DOBJ_MAX_SUBMODELS];
    unsigned short boneNames[DOBJ_MAX_SUBMODELS];
    unsigned char modelParents[DOBJ_MAX_SUBMODELS];
    unsigned char matOffset[DOBJ_MAX_SUBMODELS];
} DObj_s;

typedef char flare_layout_DObj_size[(sizeof(DObj_s) == 0x58) ? 1 : -1];
typedef char flare_layout_XSurface_size[(sizeof(XSurface_s) == 0x34) ? 1 : -1];
typedef char flare_layout_XModelInfo_size[(sizeof(XModelInfo) == 0x68) ? 1 : -1];

/* Caller-supplied model list entry for DObjCreate. */
typedef struct DObjModel_s {
    XModel_s *model;
    const char *boneName;
    unsigned short boneNames;
} DObjModel_s;

typedef struct DObjSurface_s {
    unsigned short modelIndex;
    unsigned short surfaceIndex;
} DObjSurface_s;

/* Model trace fraction, surface, normal, part and solid-state results. */
typedef struct DObjTrace_s {
    float fraction;
    int surfaceFlags;
    float normal[3];
    unsigned short partName;
    unsigned short partGroup;
    unsigned char startsolid;
    unsigned char allsolid;
} DObjTrace_s;

/* DObj creation, traces and pose access. */
void DObjCreate(const DObjModel_s *dobjModels, unsigned short numModels, XAnimTree_s *tree, DObj_s *obj, unsigned short entnum);
/* partBits receives the surface bone mask as four 32-bit words. */
void DObjGetSurfaces(const DObj_s *obj, DObjSurface_s *surfaceInfo, int *partBits, const int *lods);
void DObjTraceline(DObj_s *obj, const float *start, const float *end, const unsigned char *priorityMap, DObjTrace_s *results);
void DObjGeomTraceline(DObj_s *obj, float *start, float *end, int contentmask, DObjTrace_s *results);
void DObjCalcAnim(const DObj_s *obj, const int *partBits);
void DObjCalcSkel(const DObj_s *obj, const int *partBits);
void DObjGetHierarchyBits(DObj_s *obj, int boneIndex, int *partBits);
void DObjCompleteHierarchyBits(DObj_s *obj, int *partBits);
qboolean DObjSkelExistsConst(const DObj_s *obj, int timeStamp);
void ConvertQuatToSkelMat(const float *quat, DObjSkelMat *mat);
void DObjSkelMatrixTransformVector43Equals(float *v, const DObjSkelMat *matrix);
void LocalQuatMultiplyEquals(float *a, const float *b);
void LocalQuatMultiplyReverseEquals(const float *a, float *b);
void DObjInit(void);
void DObjShutdown(void);
void DObjFree(DObj_s *obj);
int DObjGetAllocSkelSize(const DObj_s *obj);
qboolean DObjSkelExists(DObj_s *obj, int timeStamp);
void DObjCreateSkel(DObj_s *obj, void *buf);
qboolean DObjSkelIsBoneUpToDate(const DObj_s *obj, int boneIndex);
qboolean DObjSkelAreBonesUpToDate(const DObj_s *obj, const int *partBits);
DObjSkelMat *DObjGetMatrixArray(const DObj_s *obj, int modelIndex);
int DObjGetNumModels(const DObj_s *obj);
XModel_s *DObjGetModel(const DObj_s *obj, int modelIndex);
void DObjGetBounds(const DObj_s *obj, float *mins, float *maxs);
DObjAnimMat_s *DObjGetRotTransArray(const DObj_s *obj);
void DObjGetBoneInfo(const DObj_s *obj, XBoneInfo_s **boneInfo);
qboolean DObjSetRotTransIndex(const DObj_s *obj, const char *partBits, int boneIndex);
qboolean DObjSetControlRotTransIndex(const DObj_s *obj, const char *partBits, int boneIndex);
int DObjGetNumSurfaces(const DObj_s *obj, const int *lods);
XSurface_s *DObjGetSurface(const DObj_s *obj, int modelIndex, int surfaceIndex, const int *lods);
const char *DObjGetSurfaceName(const DObj_s *obj, int modelIndex, int subMatIndex, const int *lods);
int DObjGetBoneIndex(DObj_s *obj, const char *name);
const char *DObjGetBoneName(const DObj_s *obj, int index);
XAnimTree_s *DObjGetTree(const DObj_s *obj);
qboolean DObjBad(const DObj_s *obj);
int DObjNumBones(const DObj_s *obj);
int DObjGetLodForDist(const DObj_s *obj, int modelIndex, float dist);
qboolean DObjHasContents(const DObj_s *obj, int contents);
void DObjDumpInfo(const DObj_s *obj);

/* Skeletal model and surface loading and access (xmodel.cpp). */
void XModelGetBasePose(const XModel_s *model, DObjSkelMat *pose);
void XSurfaceGetVerts(const XSurface_s *surface, const DObjSkelMat *mats, float *verts, float *texCoords, float *normals);
void XModelSetOptimize(int optimize);
qboolean XModelBad(const XModel_s *model);
void XModelEnforceExist(int enforce);
void XModelSurfsFree(XModelSurfsFile_s *modelSurfs);
void XModelPartsFree(XModelPartsFile_s *modelParts);
XModel_s *XModelPrecache(const char *name, int loadSurfaces, void *(*alloc)(unsigned int), void *(*allocTemp)(unsigned int));
void XModelFree(XModel_s *model);
qboolean XModelExists(const char *name);
void XModelClearData(unsigned char *pLow, unsigned char *pHigh);
int XModelNumBones(const XModel_s *model);
unsigned short *XModelBoneNames(const XModel_s *model);
int XModelGetBoneIndex(const XModel_s *model, unsigned short name);
void XModelGetBounds(const XModel_s *model, float *mins, float *maxs);
int XModelGetSurfaces(const XModel_s *model, XSurface_s ***surfaces, int lod);
const char *XModelGetSurfaceName(const XModel_s *model, int subMatIndex, int lod);
XSurface_s *XSurfaceCloneSurface(const XSurface_s *surface, void *(*alloc)(unsigned int));
int XSurfaceGetNumVerts(const XSurface_s *surface);
int XSurfaceGetNumTris(const XSurface_s *surface);
int XSurfaceTileMode(const XSurface_s *surface);
void XSurfaceGetTris(const XSurface_s *surface, unsigned short *pIndexes, unsigned short base);
void *XSurfaceGetBlendInfoArray(const XSurface_s *surface);
void *XSurfaceGetVertexInfoArray(const XSurface_s *surface);
int XSurfaceGetBoneIndex(const XSurface_s *surface);
const char *XModelGetName(const XModel_s *model);
int XModelGetContents(const XModel_s *model);
int XModelGetNumLods(const XModel_s *model);
int XModelGetCollLod(const XModel_s *model);
void XModelSetTestLods(int lodLevel, float dist);
void XModelSetTestLodDist(float dist);
int XModelGetLodForDist(const XModel_s *model, float dist);

struct trace_s;
int XModelTraceLine(XModel_s *model, struct trace_s *results, struct DObjSkelMat *pose, float *localStart, float *localEnd, int contentmask);

#endif
