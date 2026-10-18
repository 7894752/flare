/*
===========================================================================
flare (Call of Duty, 2003).

Distributed under the terms in COPYING (GPL version 3 or later,
with id Software's additional terms).

Collision, packed vertex and blend records for models.
===========================================================================
*/
#ifndef FLARE_XMODEL_RECORDS_H
#define FLARE_XMODEL_RECORDS_H

typedef struct XRigidVertex_s {
    float normal[3];
    float xyz[3];
} XRigidVertex_s;

typedef struct XBlendVertex_s {
    float normal[3];
    int numBlends; /* Additional influences after this primary influence. */
    float xyz[3];
    int boneOffset; /* Byte offset in the skeleton matrix array. */
} XBlendVertex_s;

typedef struct XWeightedVertex_s {
    XBlendVertex_s vertex;
    float weight;
} XWeightedVertex_s;

typedef struct XSimpleBlendInfo_s {
    float xyz[3];
    int boneOffset; /* Byte offset in the skeleton matrix array. */
    float weight;
} XSimpleBlendInfo_s;

typedef struct XStripInfo_s {
    int numStrips;
    unsigned char *numVerts;
    unsigned short *stripIndexes;
} XStripInfo_s;

typedef struct XModelCollTri_s {
    float plane[4];
    float svec[4];
    float tvec[4];
} XModelCollTri_s;

typedef struct XModelCollSurf_s {
    XModelCollTri_s *collTris;
    int numCollTris;
    float mins[3];
    float maxs[3];
    int boneIdx;
    int contents;
    int surfFlags;
} XModelCollSurf_s;

typedef char flare_layout_XModelCollTri_size[(sizeof(XModelCollTri_s) == 0x30) ? 1 : -1];
typedef char flare_layout_XModelCollSurf_size[(sizeof(XModelCollSurf_s) == 0x2C) ? 1 : -1];
typedef char flare_layout_XRigidVertex_size[(sizeof(XRigidVertex_s) == 0x18) ? 1 : -1];
typedef char flare_layout_XBlendVertex_size[(sizeof(XBlendVertex_s) == 0x20) ? 1 : -1];
typedef char flare_layout_XWeightedVertex_size[(sizeof(XWeightedVertex_s) == 0x24) ? 1 : -1];
typedef char flare_layout_XSimpleBlendInfo_size[(sizeof(XSimpleBlendInfo_s) == 0x14) ? 1 : -1];
typedef char flare_layout_XStripInfo_size[(sizeof(XStripInfo_s) == 0x0C) ? 1 : -1];

#endif
