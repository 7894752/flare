/*
===========================================================================
flare (Call of Duty, 2003).

Distributed under the terms in COPYING (GPL version 3 or later,
with id Software's additional terms).

BSP leaf lights, light indices and entity lighting.
===========================================================================
*/
#include "flare/math_types.h"
#include "light.h"
#include "flare/map.h"
#include "flare/options.h"
#include "flare/lightmap.h"
#include "common/q_round.h"
#include <stdlib.h>
#include "flare/traceresult.h"
#include "flare/polygon.h"
typedef unsigned char byte;
typedef float vec2_t[2], vec3_t[3];
#include "code/qcommon/qfiles.h"
#include <stdio.h>
#include <string.h>

#define POLYFLAG_VISUAL_ONLY 0x08
#define POLYFLAG_MODEL_SURFACE 0x20
#define POLYFLAG_PORTAL POLYFLAG_0x40

#define MAX_LEAF_LIGHTS 15
#define LIGHTMAP_SIZE 512

#define SIDE_FRONT 0
#define SIDE_BACK 1
#define SIDE_CROSS 3

void Q3Map_Error(const char *format, ...);
void Q3Map_Verbose(const char *format, ...);
void Q3Map_Warning(int level, const char *format, ...);

struct LeafLight
{
    LeafLight *next;
    float intensity;
    Light *light;
};

extern "C" { extern int numLights; }
Light *lights[MAX_MAP_LIGHTS] = { 0 };
extern "C" { extern bspLight_t dlights[MAX_MAP_LIGHTS]; }
extern "C" { extern int numLightIndexes; }
extern "C" { extern short bspLightIndexes[MAX_MAP_LIGHT_INDEXES]; }

float lastDiscardedIntensity = 0.0f;
extern "C" { extern dnode_t dnodes[]; }
extern "C" { extern dplane_t dplanes[]; }
extern "C" { extern dleaf_t dleafs[]; }
extern "C" { extern int numleafs; }

void WarnTooManyLeafLights(Light *light, int leaf, float intensity);
void FilterLightPolygon_r(int node, Light *light, Polygon *poly);

extern "C" {
void LoadBSPFile(const char *name);
void WriteBSPFile(const char *name);
void ParseEntities();
void UnparseEntities();
void SetKeyValue(BspEntity *entity, const char *key, const char *value);
extern char dEntstring[];
extern BspShader dshaders[];
extern int numShaders;

extern int numplanes;
extern BspModel dmodels[];
extern int nummodels;
extern BspBrush dbrushes[];
extern int numbrushes;
extern BspBrushSide dbrushsides[];
extern int numbrushsides;
extern BspVertex triangleSoupData[];
extern int numDrawVerts;
extern short drawIndexes[];
extern int numDrawIndexes;
extern BspSurface triangleSurfaces[];
extern int numTriangleSurfaces;
extern unsigned char lightBytes[];
extern int numLightBytes;
}
bool Q3Map_ReadEntityText(const char *text);
bool LoadMapShaders();
void Q3Map_ReadPolyFile(MapInfo *map, const char *name);
void Q3Map_ReadPortalFile(const char *name);
void ConvertLightmaps();
void TraceSegment(const Vector3d &start, const Vector3d &end, TraceResult *trace, unsigned int mask);

bool FindLeafLight(Light *light, int leaf, float intensity);
short FindLightIndex(Light *light);
void AddBspLight(Light *light);
int CompareLightIndices(const void *a, const void *b);
void BuildLeafLightIndices();
int SplitPolygonEpsilon(Polygon *poly, const Plane &splitPlane, Polygon **front, Polygon **backPolygon);

/*
=============
AddLeafLight

Keep at most fifteen local lights per cluster. When full, reuse the
weakest entry for a light of at least equal intensity. Global lights
need only a flag and do not consume an entry.
=============
*/
void AddLeafLight(Light *light, int leaf, float intensity)
{
    if (light->isGlobal)
    {
        mapInfo.leafGlobalLights[leaf] = true;
        return;
    }
    if (!light->HasOrigin())
        mapInfo.leafGlobalLights[leaf] = true;

    float minIntensity = FLT_MAX;
    int numEntries = 0;
    LeafLight *prev = 0;
    LeafLight *link, *newLight;
    for (link = mapInfo.leafLights[leaf]; link; numEntries++, prev = link, link = link->next)
    {
        if (minIntensity > link->intensity)
            minIntensity = link->intensity;
    }
    if (numEntries == MAX_LEAF_LIGHTS)
    {
        if (minIntensity > intensity)
        {
            if (intensity != lastDiscardedIntensity)
            {
                lastDiscardedIntensity = intensity;
                WarnTooManyLeafLights(light, leaf, intensity);
            }
            return;
        }
        WarnTooManyLeafLights(prev->light, leaf, minIntensity);
        if (mapInfo.leafLights[leaf]->intensity == minIntensity)
        {
            newLight = mapInfo.leafLights[leaf];
            mapInfo.leafLights[leaf] = newLight->next;
        }
        else
        {
            for (link = mapInfo.leafLights[leaf]; link->next->intensity != minIntensity; link = link->next)
                ;
            newLight = link->next;
            link->next = newLight->next;
        }
    }
    else
        newLight = new LeafLight;

    newLight->light = light;
    newLight->intensity = intensity;
    newLight->next = mapInfo.leafLights[leaf];
    mapInfo.leafLights[leaf] = newLight;
}

/*
=============
WarnTooManyLeafLights
=============
*/
void WarnTooManyLeafLights(Light *light, int leaf, float intensity)
{
    char originText[256], intensityText[256], message[1024];
    if (light->HasOrigin())
    {
        Vector3d origin = light->GetOrigin();
        sprintf(originText, "light at (%g %g %g)", origin.x, origin.y, origin.z);
    }
    else
        strcpy(originText, "sun light");
    if (intensity == FLT_MAX)
        strcpy(intensityText, "; too many lights in leaf");
    else
        sprintf(intensityText, " with max intensity %.3g", intensity);
    sprintf(message, "leaf at (%.0f %.0f %.0f) - (%.0f %.0f %.0f) ignoring %s%s\n",
            mapInfo.leafBounds[leaf].mins[0], mapInfo.leafBounds[leaf].mins[1],
            mapInfo.leafBounds[leaf].mins[2], mapInfo.leafBounds[leaf].maxs[0],
            mapInfo.leafBounds[leaf].maxs[1], mapInfo.leafBounds[leaf].maxs[2],
            originText, intensityText);
    if (intensity < 0.04f)
        Q3Map_Verbose(message);
    else if (intensity < 0.08f)
        Q3Map_Warning(1, message);
    else
    {
        Q3Map_Warning(1, message);
        mapInfo.errors = true;
    }
}

/*
=============
AddLightAtPoint

Find the BSP leaf containing the origin and register the light in its
cluster. FLT_MAX keeps source lights ahead of attenuated portal entries.
=============
*/
void AddLightAtPoint(Light *light, const Vector3d &point)
{
    int node = 0;
    dplane_t *plane;
    double distance;
    dleaf_t *leaf;

    while (node >= 0)
    {
        plane = &dplanes[dnodes[node].planeNum];
        distance = plane->normal[0] * point.x + plane->normal[1] * point.y
                 + plane->normal[2] * point.z - plane->dist;
        if (distance >= 0.0)
            node = dnodes[node].children[0];
        else
            node = dnodes[node].children[1];
    }
    leaf = &dleafs[-(node + 1)];
    if (leaf->cluster >= 0 && !FindLeafLight(light, leaf->cluster, FLT_MAX))
        AddLeafLight(light, leaf->cluster, FLT_MAX);
}

/*
=============
FilterLightPolygon
=============
*/
void FilterLightPolygon(Light *light, Polygon *poly)
{
    FilterLightPolygon_r(0, light, poly);
}

/*
=============
FilterLightPolygon_r

Split the light polygon through the disk BSP and register every reached
cluster. Coplanar polygons visit both children.
=============
*/
void FilterLightPolygon_r(int node, Light *light, Polygon *poly)
{
    dleaf_t *pBspLeaf;
    dplane_t *pDiskPlane;
    Plane splitPlane;
    Polygon *front, *backPolygon;
    int iSideFlags;
    if (node < 0)
    {
        pBspLeaf = &dleafs[-(node + 1)];
        if (pBspLeaf->cluster >= 0 && !FindLeafLight(light, pBspLeaf->cluster, FLT_MAX))
            AddLeafLight(light, pBspLeaf->cluster, FLT_MAX);
        return;
    }
    pDiskPlane = &dplanes[dnodes[node].planeNum];
    splitPlane.normal.x = pDiskPlane->normal[0];
    splitPlane.normal.y = pDiskPlane->normal[1];
    splitPlane.normal.z = pDiskPlane->normal[2];
    splitPlane.dist = pDiskPlane->dist;
    splitPlane.SetSignBits();
    iSideFlags = SplitPolygonEpsilon(poly, splitPlane, &front, &backPolygon);
    if (iSideFlags == SIDE_CROSS)
    {
        FilterLightPolygon_r(dnodes[node].children[0], light, front);
        front->Free();
        FilterLightPolygon_r(dnodes[node].children[1], light, backPolygon);
        backPolygon->Free();
    }
    else if (iSideFlags == SIDE_FRONT)
        FilterLightPolygon_r(dnodes[node].children[0], light, front);
    else if (iSideFlags == SIDE_BACK)
        FilterLightPolygon_r(dnodes[node].children[1], light, backPolygon);
    else
    {
        FilterLightPolygon_r(dnodes[node].children[0], light, poly);
        FilterLightPolygon_r(dnodes[node].children[1], light, poly);
    }
}

/*
=============
FindLeafLight

Find an existing cluster entry, raising its saved intensity if needed.
Return true when leaf-light generation is disabled.
=============
*/
bool FindLeafLight(Light *light, int leaf, float intensity)
{
    if (options.noLeafLights)
        return true;
    if (leaf >= mapInfo.numLightLeaves)
        Q3Map_Error("iLeafNum(%i) > g_Map.iClusterCount(%i) -- .BSP probably doesn't match .PRT file\n", leaf, mapInfo.numLightLeaves);
    if (light->isGlobal)
        return mapInfo.leafGlobalLights[leaf];

    LeafLight *link;
    for (link = mapInfo.leafLights[leaf]; link; link = link->next)
    {
        if (link->light == light)
        {
            if (link->intensity < intensity)
                link->intensity = intensity;
            return true;
        }
    }
    return false;
}

/*
=============
AddLightIndices

Reuse an existing sequence, including an overlap at the end of the index list.
=============
*/
short AddLightIndices(const short *indices, int count)
{
    short i, j;
    for (i = 0, j = 0; i < numLightIndexes; i++, j = 0)
    {
        while (j < count && i+j < numLightIndexes)
        {
            if (bspLightIndexes[i+j] != indices[j])
                break;
            j++;
        }
        if (j == count)
            return i;
        if (i+j == numLightIndexes)
            break;
    }
    numLightIndexes = i + count;
    if (numLightIndexes > MAX_MAP_LIGHT_INDEXES)
        Q3Map_Error("MAX_MAP_LIGHTINDEXES(%i) exceeded\n", numLightIndexes);
    while (j < count)
    {
        bspLightIndexes[i+j] = indices[j];
        j++;
    }
    return i;
}

/*
=============
LoadBspForLighting
=============
*/
bool LoadBspForLighting(const char *name)
{
    LoadBSPFile(name);
    ParseEntities();
    Q3Map_ReadEntityText(dEntstring);
    mapInfo.shaders = dshaders;
    mapInfo.numShaders = numShaders;
    mapInfo.planes = (BspPlane *)dplanes;
    mapInfo.numPlanes = numplanes;
    mapInfo.models = dmodels;
    mapInfo.numModels = nummodels;
    mapInfo.brushes = dbrushes;
    mapInfo.numBrushes = numbrushes;
    mapInfo.brushSides = dbrushsides;
    mapInfo.numBrushSides = numbrushsides;
    mapInfo.vertices = triangleSoupData;
    mapInfo.numVertices = numDrawVerts;
    mapInfo.indices = drawIndexes;
    mapInfo.numIndices = numDrawIndexes;
    mapInfo.surfaces = triangleSurfaces;
    mapInfo.numSurfaces = numTriangleSurfaces;
    mapInfo.lightmaps = (unsigned char (*)[LIGHTMAP_SIZE][LIGHTMAP_SIZE][3])lightBytes;
    mapInfo.numLightmaps = numLightBytes / sizeof(*mapInfo.lightmaps);
    if (!LoadMapShaders())
        return false;
    Q3Map_ReadPolyFile(&mapInfo, name);
    if (!options.noLeafLights)
    {
        Q3Map_ReadPortalFile(name);
        mapInfo.leafLights = new LeafLight *[mapInfo.numLightLeaves];
        memset(mapInfo.leafLights, 0, mapInfo.numLightLeaves * sizeof(LeafLight *));
        mapInfo.leafGlobalLights = new bool[mapInfo.numLightLeaves];
        memset(mapInfo.leafGlobalLights, 0, mapInfo.numLightLeaves);
    }
    return true;
}

/*
=============
GetEntityLighting

Trace down from a model origin and bilinearly sample the hit lightmap.
=============
*/
bool GetEntityLighting(const MapEntity *entity, char *text)
{
    Vector3d end;
    TraceResult trace;
    Lightmap *lightmap;
    float color[3], coords[2];
    int s0, t0, i;
    float ds, dt;

    end = entity->origin;
    end.z -= 1024.0;
    TraceSegment(entity->origin, end, &trace,
                 POLYFLAG_VISUAL_ONLY | POLYFLAG_MODEL | POLYFLAG_MODEL_SURFACE | POLYFLAG_PORTAL);
    if (!trace.hit || trace.hit->info->lightmapIndex < 0)
        return false;
    lightmap = &lightmapSamples[trace.hit->info->lightmapIndex];
    trace.hit->WorldToLightmap(&trace.hitPoint, coords);
    s0 = FloorToInt(coords[0]);
    ds = coords[0] - s0;
    t0 = FloorToInt(coords[1]);
    dt = coords[1] - t0;
    for (i = 0; i < 3; i++)
    {
        color[i] = (1.0f-dt) * lightmap->samples[t0][s0].color[i] * (1.0f-ds)
                   + dt * lightmap->samples[t0+1][s0].color[i] * (1.0f-ds)
                   + dt * lightmap->samples[t0+1][s0+1].color[i] * ds
                   + (1.0f-dt) * lightmap->samples[t0][s0+1].color[i] * ds;
        if (color[i] > options.overbrightScale)
            color[i] = options.overbrightScale;
    }
    sprintf(text, "%.3g %.3g %.3g", color[0], color[1], color[2]);
    return true;
}

/*
=============
WriteBspLighting
=============
*/
void WriteBspLighting(const char *name)
{
    int i;
    MapEntity *entity;
    mapInfo.numLightmaps = numLightmaps;
    numLightBytes = mapInfo.numLightmaps * sizeof(*mapInfo.lightmaps);
    ConvertLightmaps();
    for (i = 0; i < mapInfo.numEntities; i++)
    {
        entity = &mapInfo.entities[i];
        if (!entity->modelLoaded)
            continue;
        char text[64];
        if (GetEntityLighting(entity, text))
            SetKeyValue(entity->bspEntity, "lightingPrecalc", text);
    }
    UnparseEntities();
    if (!options.noLeafLights)
        BuildLeafLightIndices();
    WriteBSPFile(name);
}

/*
=============
BuildLeafLightIndices

Export each leaf's lights once, sort their indices, and share index runs
between BSP leaves in the same cluster. A -1 entry denotes global light.
=============
*/
void BuildLeafLightIndices()
{
    numLights = 0;
    numLightIndexes = 0;
    int clusterIndex;
    LeafLight *lightLink;
    short indices[MAX_MAP_LIGHTS + 1];
    int numIndexes;
    for (clusterIndex = 0; clusterIndex < mapInfo.numLightLeaves; clusterIndex++)
    {
        numIndexes = 0;
        for (lightLink = mapInfo.leafLights[clusterIndex]; lightLink; numIndexes++, lightLink = lightLink->next)
        {
            indices[numIndexes] = FindLightIndex(lightLink->light);
            if (indices[numIndexes] < 0)
            {
                indices[numIndexes] = numLights;
                AddBspLight(lightLink->light);
            }
        }
        if (mapInfo.leafGlobalLights[clusterIndex])
        {
            indices[numIndexes] = -1;
            numIndexes++;
        }
        qsort(indices, numIndexes, sizeof(short), CompareLightIndices);
        for (int j = 0; j < numleafs; j++)
        {
            if (dleafs[j].cluster == clusterIndex)
            {
                dleafs[j].firstLightIndex = AddLightIndices(indices, numIndexes);
                dleafs[j].numLightIndexes = numIndexes;
            }
        }
    }
}

/*
=============
FindLightIndex
=============
*/
short FindLightIndex(Light *light)
{
    short i;
    for (i = 0; i < numLights; i++)
    {
        if (lights[i] == light)
            return i;
    }
    return -1;
}

/*
=============
AddBspLight
=============
*/
void AddBspLight(Light *light)
{
    if (numLights >= MAX_MAP_LIGHTS)
        Q3Map_Error("MAX_MAP_LIGHTS (%i) exceeded\n", MAX_MAP_LIGHTS);
    lights[numLights] = light;
    light->Export(&dlights[numLights]);
    numLights++;
}

/*
=============
CompareLightIndices
=============
*/
int CompareLightIndices(const void *a, const void *b)
{
    const short *l1, *l2;
    l1 = (const short *)a;
    l2 = (const short *)b;
    return *l1 - *l2;
}
