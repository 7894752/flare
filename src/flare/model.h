/*
===========================================================================
flare (Call of Duty, 2003).

Distributed under the terms in COPYING (GPL version 3 or later,
with id Software's additional terms).

Map model instances, transformed surfaces and the model cache.
===========================================================================
*/
#ifndef FLARE_MODEL_H
#define FLARE_MODEL_H

#include <stddef.h>
#include "flare/mathlib.h"

struct MapInfo;
struct Occluder;

/* Posed model-space positions, UVs and a flat triangle index buffer. */
struct ModelSurface
{
    short material;
    unsigned short flags;
    float (*vertices)[3];
    float (*texcoords)[2];
    unsigned short numVertices, numIndices; /* three indices per triangle */
    unsigned short *indices;
};

struct ModelAsset
{
    ModelAsset *next;               /* modelList chain (FindModel) */
    char name[64];
    int numSurfaces;
    ModelSurface *surfaces;
};

class MiscModel
{
public:
    MiscModel(MapInfo *map, const char *name, unsigned short flags, const Vector3d &origin, const Vector3d axis[3]);
    bool ShouldIgnore(int entityNum) const;
    Bounds GetBounds() const { return bounds; }
    void CollectOccluders(int entityNum, Occluder **first, Occluder **last);

    ModelAsset *model;              /* NULL when the model failed to load */
    int frameStamp;                 /* last draw or traversal frame */
    int entityNum;
    Bounds bounds;                  /* world bounds of the transformed vertices */
    Vector3d axis[3];
    Vector3d origin;
    unsigned short flags;           /* polygon flags combined with each surface's flags */
};

#endif
