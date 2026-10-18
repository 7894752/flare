/*
===========================================================================
flare (Call of Duty, 2003).

Distributed under the terms in COPYING (GPL version 3 or later,
with id Software's additional terms).

Loaded BSP records and map entities.
===========================================================================
*/
#ifndef FLARE_MAP_H
#define FLARE_MAP_H

#include "flare/mathlib.h"

#ifndef FLARE_LAYOUT
#define FLARE_LAYOUT(name, cond) typedef char flare_layout_##name[(cond) ? 1 : -1]
#define FLARE_OFFSET(type, member) ((int)(&((type *)0)->member))
#endif

struct BspEntity;
struct LeafLight;                   /* Lights contributing to a BSP leaf. */

/* Entity key/value pairs and the transform of its model instance. */
struct MapEntity
{
    MapEntity() {}
    BspEntity *bspEntity;
    char **keys;
    char **values;
    int numKeys;
    int sourceIndex;
    int modelIndex;
    Vector3d origin;
    Vector3d axis[3];
    bool modelLoaded;
};
FLARE_LAYOUT(MapEntity_size, sizeof(MapEntity) == 0x80);

/* BSP input records (bspio.cpp). */
struct BspEntity
{
    float origin[3];
    struct BspBrushList *brushes;
    struct PatchMesh *patches;
    struct TerrainSurface *terrainSurfaces;
    int firstDrawSurface;
    struct KeyValuePair *epairs;
};

struct BspShader
{
    char name[64];
    int surfaceFlags;
    int contentFlags;
};

struct BspPlane
{
    float normal[3];
    float distance;
};

struct BspModel
{
    float mins[3], maxs[3];
    int firstSurface, numSurfaces;
    int firstLeafSurface, numLeafSurfaces;
    int firstBrush, numBrushes;
};

struct BspBrush
{
    short numSides, shaderIndex;
};

struct BspBrushSide
{
    union { float distance; int planeIndex; } plane;
    int shaderIndex;
};

struct BspVertex
{
    float xyz[3], texCoord[2], lightmap[2], normal[3];
    unsigned char color[4];
};

struct BspSurface
{
    short shaderIndex, lightmapIndex;
    int firstVertex;
    short numVertices, numIndices;
    int firstIndex;
};

/* The loaded map. */
struct MapInfo
{
    bool errors;                    /* a leaf-light warning has been printed */
    MapEntity *entities;
    int numEntities;
    BspShader *shaders;
    int numShaders;
    BspPlane *planes;
    int numPlanes;
    BspModel *models;
    int numModels;
    BspBrush *brushes;
    int numBrushes;
    BspBrushSide *brushSides;
    int numBrushSides;
    BspVertex *vertices;
    int numVertices;
    short *indices;
    int numIndices;
    BspSurface *surfaces;
    int numSurfaces;
    unsigned char (*lightmaps)[512][512][3];   /* RGB lightmap pages */
    int numLightmaps;
    int numLightLeaves;             /* clusters with light lists */
    LeafLight **leafLights;         /* per-leaf light list heads */
    Bounds *leafBounds;             /* per-leaf bounds for light warnings */
    bool *leafGlobalLights;         /* leaf sees sun or ambient lights */
};
FLARE_LAYOUT(MapInfo_size, sizeof(MapInfo) == 0x64);
FLARE_LAYOUT(MapInfo_members, FLARE_OFFSET(MapInfo, entities) == 0x04 && FLARE_OFFSET(MapInfo, surfaces) == 0x44
                                  && FLARE_OFFSET(MapInfo, lightmaps) == 0x4C && FLARE_OFFSET(MapInfo, numLightLeaves) == 0x54
                                  && FLARE_OFFSET(MapInfo, leafGlobalLights) == 0x60);

/* BSP input image (bspio.cpp) and loaded scene (entities.cpp). */
extern unsigned char *bspFile;
extern int bspFileSize;
extern MapInfo mapInfo;

struct BspNode;
struct Vector3d;
extern BspNode *bspRoot;
void FloodBspFromPoint(BspNode *node, const Vector3d &point);

struct Plane;
bool CreateBrush(const Plane *planes, int numPlanes, bool solid);

#endif
