/*
===========================================================================
flare (Call of Duty, 2003).

Distributed under the terms in COPYING (GPL version 3 or later,
with id Software's additional terms).

Lightmap coverage, sample areas and lighting colors.
===========================================================================
*/
#include <string.h>
#include <stdlib.h>
#include "common/q_round.h"

#include "flare/mathlib.h"
#include "flare/geometry.h"

#include "flare/lightmap.h"
#include "flare/map.h"
#include "flare/options.h"
#include "flare/color.h"
#include "polygon.h"
#include "flare/print.h"
#include "flare/preview_step.h"

#define SIDE_FRONT 0
#define SIDE_BACK 1
#define SIDE_ON 2
#define SIDE_CROSS 3

#define LIGHTMAP_SIZE 512
#define MAX_SUBDIVIDE_POINTS 516
#define NUM_SUBDIVIDE_BUFFERS 4
#define SOLID_LIGHTMAP_OFFSET 0.375
#define LIGHTMAP_AREA_TOLERANCE 0.01

/*
=============
ColorRGB::operator*=
=============
*/
inline void ColorRGB::operator*=(float scale)
{
    r *= scale;
    g *= scale;
    b *= scale;
}

typedef float vec2_t[2];

Polygon *lightmapAreaPolygon = 0;
float lightmapAreaScale = 0;
Lightmap *lightmapSamples = 0;
int numLightmaps = 0;
unsigned char (*lightmapMask)[LIGHTMAP_SIZE][LIGHTMAP_SIZE] = 0;
int maskLightmapIndex = 0;
ColorRGB lightmapMaskColor = { 0, 0, 0 };



void BuildLightmapMask_r(BspNode *node);
void BuildPolygonLightmapMask(Polygon *poly);
void ProcessLightmapMask(void);
void RemoveLmapAreaInSolid(void);
void FinalizeLightmapAreas(void);
void DebugLightmapAreas(void);
void InitLightmapColors(void);
void GenerateLightmapAreas(Polygon *poly, float scale);
void AccumulateLightmapArea(float area, const float *st, const vec2_t *points, int count);



#define LM_MAX(a,b) (((a) > (b)) ? (a) : (b))
#define LM_MIN(a,b) (((a) < (b)) ? (a) : (b))

void LightmapMaskCallback(float area, const float *st, const vec2_t *points, int count);
bool RemoveLmapAreaInSolid_r(BspNode *node, Polygon *poly);

/*
=============
BuildLightmapMask

Collect directional coverage bits at half-texel resolution, then use them
to extend lighting colors into uncovered texels.
=============
*/
void BuildLightmapMask(void)
{
    lightmapMask = new unsigned char[numLightmaps][LIGHTMAP_SIZE][LIGHTMAP_SIZE];
    memset(lightmapMask, 0, numLightmaps * sizeof(*lightmapMask));
    BuildLightmapMask_r(bspRoot);
    ProcessLightmapMask();
    delete [] lightmapMask;
    lightmapMask = 0;
}

/*
=============
BuildLightmapMask_r
=============
*/
void BuildLightmapMask_r(BspNode *node)
{
    Polygon *poly;
    if (node->isLeaf)
        return;
    for (poly = node->polygons; poly; poly = poly->next)
        if (poly->info->lightmapIndex >= 0)
            BuildPolygonLightmapMask(poly);
    BuildLightmapMask_r(node->children[0]);
    BuildLightmapMask_r(node->children[1]);
}

/*
=============
BuildPolygonLightmapMask

Project the polygon with consistent winding and subdivide at half-texel
steps. Each covered quadrant marks which neighboring texels it faces.
=============
*/
void BuildPolygonLightmapMask(Polygon *poly)
{
    int i;
    vec2_t buffers[NUM_SUBDIVIDE_BUFFERS][MAX_SUBDIVIDE_POINTS];
    if (poly->IsLightmapFlipped())
    {
        for (i = 0; i < poly->numPoints; i++)
            poly->WorldToLightmap(&poly->points[poly->numPoints - 1 - i], buffers[0][i]);
    }
    else
    {
        for (i = 0; i < poly->numPoints; i++)
            poly->WorldToLightmap(&poly->points[i], buffers[0][i]);
    }
    maskLightmapIndex = poly->info->lightmapIndex;
    if (options.debugLmapBleeding)
    {
        srand((unsigned int)poly->GetInfo());
        lightmapMaskColor.r = rand() * (1.0f / RAND_MAX);
        lightmapMaskColor.g = rand() * (1.0f / RAND_MAX);
        lightmapMaskColor.b = rand() * (1.0f / RAND_MAX);
        lightmapMaskColor *= 1.0f / lightmapMaskColor.Luminance();
    }
    SubdividePolygon2D(buffers, poly->numPoints, poly->lightmapWidth * 2, poly->lightmapHeight * 2,
                       (float)poly->lightmapX, (float)poly->lightmapY, 0.5f, 0.5f,
                       LightmapMaskCallback);
    lightmapAreaScale = 0;
    lightmapAreaPolygon = 0;
}

/*
=============
LightmapMaskCallback

Mark the directions covered by this half-texel fragment. The optional preview
colors each polygon's footprint independently.
=============
*/
void LightmapMaskCallback(float area, const float *st, const vec2_t *points, int count)
{
    /* Each half-texel quadrant marks the adjacent cells it touches. */
    static const unsigned char lightmapQuarterMasks[2][2] = {
        {0x70, 0x1C},
        {0xC1, 0x07}
    };
    int s = FloorToInt(st[0]);
    int t = FloorToInt(st[1]);
    int sSub = st[0] - s >= 0.5f ? 1 : 0;
    int tSub = st[1] - t >= 0.5f ? 1 : 0;
    lightmapMask[maskLightmapIndex][t][s] |= lightmapQuarterMasks[tSub][sSub];
    if (options.debugLmapBleeding)
        lightmapSamples[maskLightmapIndex].samples[t][s].color = lightmapMaskColor;
}

/*
=============
ProcessLightmapMask

Fill uncovered texels from covered neighbors whose mask points toward
them. The 3 by 3 weights favor edge neighbors over diagonal neighbors.
Only colors change; uncovered texels keep zero area.
=============
*/
void ProcessLightmapMask(void)
{
    static const int lightmapNeighborWeights[3][3] = {
        {1, 6, 1},
        {6, 36, 6},
        {1, 6, 1}
    };
    static const unsigned char lightmapNeighborMasks[3][3] = {
        {0x02, 0x01, 0x80},
        {0x04, 0x00, 0x40},
        {0x08, 0x10, 0x20}
    };
    int i, s, s2;
    for (i = 0; i < numLightmaps; i++)
    {
        ColorRGB color;
        int weight, t2;
        for (int t = 0; t < LIGHTMAP_SIZE; t++)
            for (s = 0; s < LIGHTMAP_SIZE; s++)
            {
                if (lightmapSamples[i].samples[t][s].area == 0.0f)
                {
                    int sMin = LM_MAX(0, s - 1);
                    int sMax = LM_MIN(LIGHTMAP_SIZE - 1, s + 1);
                    int tMin = LM_MAX(0, t - 1);
                    int tMax = LM_MIN(LIGHTMAP_SIZE - 1, t + 1);
                    color.Clear();
                    weight = 0;
                    for (t2 = tMin; t2 <= tMax; t2++)
                        for (s2 = sMin; s2 <= sMax; s2++)
                        {
                            if (lightmapSamples[i].samples[t2][s2].area > 0.0f)
                            {
                                int deltaS = s2 - s + 1;
                                int deltaT = t2 - t + 1;
                                int j;
                                if (lightmapMask[i][t2][s2] & lightmapNeighborMasks[deltaT][deltaS])
                                {
                                    color += lightmapSamples[i].samples[t2][s2].color *
                                             (float)lightmapNeighborWeights[deltaT][deltaS];
                                    weight += lightmapNeighborWeights[deltaT][deltaS];
                                }
                            }
                        }
                    if (weight > 1)
                        color *= 1.0f / weight;
                    lightmapSamples[i].samples[t][s].color = color;
                }
            }
    }
}

/*
=============
InitAmbientLightmaps
=============
*/
void InitAmbientLightmaps(void)
{
    int i;
    for (i = 0; i < numLightmaps; i++)
    {
        int t, s;
        for (t = 0; t < LIGHTMAP_SIZE; t++)
            for (s = 0; s < LIGHTMAP_SIZE; s++)
                lightmapSamples[i].samples[t][s].color = options.ambientColor;
    }
}

/*
=============
AllocLightmapSamples
=============
*/
void AllocLightmapSamples(void)
{
    int i;
    for (i = 0; i < numPermanentPolygons; i++)
        if (numLightmaps <= polygons[i].info->lightmapIndex)
            numLightmaps = polygons[i].info->lightmapIndex + 1;
    lightmapSamples = new Lightmap[numLightmaps];
    if (!lightmapSamples)
        Q3Map_Error("Allocation failed on lightmap data (%i bytes)\n", numLightmaps * sizeof(*lightmapSamples));
    memset(lightmapSamples, 0, numLightmaps * sizeof(*lightmapSamples));
    for (i = 0; i < numPermanentPolygons; i++)
        GenerateLightmapAreas(&polygons[i], 1.0f);
    InitLightmapColors();
}

/*
=============
GenerateLightmapAreas

Subdivide in lightmap space and accumulate each fragment's signed area.
A negative scale removes coverage, as used for fragments inside solid.
=============
*/
void GenerateLightmapAreas(Polygon *poly, float scale)
{
    int i;
    vec2_t buffers[NUM_SUBDIVIDE_BUFFERS][MAX_SUBDIVIDE_POINTS];
    if (poly->info->lightmapIndex < 0)
        return;
    if (poly->IsLightmapFlipped())
    {
        for (i = 0; i < poly->numPoints; i++)
            poly->WorldToLightmap(&poly->points[poly->numPoints - 1 - i], buffers[0][i]);
    }
    else
    {
        for (i = 0; i < poly->numPoints; i++)
            poly->WorldToLightmap(&poly->points[i], buffers[0][i]);
    }
    lightmapAreaScale = scale;
    lightmapAreaPolygon = poly;
    SubdividePolygon2D(buffers, poly->numPoints, poly->lightmapWidth, poly->lightmapHeight,
                       (float)poly->lightmapX, (float)poly->lightmapY, 1.0f, 1.0f,
                       AccumulateLightmapArea);
    lightmapAreaScale = 0;
    lightmapAreaPolygon = 0;
}

/*
=============
AccumulateLightmapArea

Add this clipped fragment's area to its texel on the current lightmap page.
=============
*/
void AccumulateLightmapArea(float area, const float *st, const vec2_t *points, int count)
{
    LightmapSample (*samples)[LIGHTMAP_SIZE] = lightmapSamples[lightmapAreaPolygon->info->lightmapIndex].samples;
    int s = (int)st[0];
    int t = (int)st[1];
    samples[t][s].area += area * lightmapAreaScale;
}

/*
=============
InitLightmapColors

Reuse the BSP lightmap colors when the page counts agree. Otherwise
initialize the allocated pages to white.
=============
*/
void InitLightmapColors(void)
{
    int i, s, t;
    if (mapInfo.numLightmaps != numLightmaps)
    {
        for (i = 0; i < numLightmaps; i++)
            for (t = 0; t < LIGHTMAP_SIZE; t++)
                for (s = 0; s < LIGHTMAP_SIZE; s++)
                {
                    ColorRGB white = { 1.0f, 1.0f, 1.0f };
                    lightmapSamples[i].samples[t][s].color = white;
                }
    }
    else
    {
        for (i = 0; i < numLightmaps; i++)
            for (t = 0; t < LIGHTMAP_SIZE; t++)
                for (s = 0; s < LIGHTMAP_SIZE; s++)
                    lightmapSamples[i].samples[t][s].color.FromBytes(mapInfo.lightmaps[i][t][s]);
    }
}

/*
=============
FinalizeLightmaps
=============
*/
void FinalizeLightmaps(void)
{
    if (!options.noCorrectSolid)
        RemoveLmapAreaInSolid();
    FinalizeLightmapAreas();
    if (options.debugLmapAreas)
        DebugLightmapAreas();
}

/*
=============
RemoveLmapAreaInSolid

Subtract coverage in solid from a slightly offset copy of each polygon.
Restore all coverage when no fragment reaches flooded space.
=============
*/
void RemoveLmapAreaInSolid(void)
{
    int i;
    BeginProgress("Removing areas in solid...");
    GL_Step(0);
    for (i = 0; i < numPermanentPolygons; i++)
    {
        Polygon *poly = &polygons[i];
        if (poly->info->lightmapIndex < 0)
            continue;
        GL_StepPolygon(1, poly);
        Polygon *expanded = OffsetPolygon(poly, SOLID_LIGHTMAP_OFFSET);
        if (!expanded)
            Q3Map_Error("RemoveLmapAreaInSolid: Couldn't allocate temporary polygon.\n");
        if (!RemoveLmapAreaInSolid_r(bspRoot, expanded))
            GenerateLightmapAreas(expanded, 1.0f);
        expanded->Free();
        SetProgress(i, numPermanentPolygons);
    }
    EndProgress();
    GL_ClearStep();
}

/*
=============
RemoveLmapAreaInSolid_r

Split the polygon through the BSP, subtracting fragments in unflooded
leaves. Return whether any fragment reaches flooded space.
=============
*/
bool RemoveLmapAreaInSolid_r(BspNode *node, Polygon *poly)
{
    Polygon *pFront, *pBack;
    int sideFlags;
    bool outsideSolid;
    if (node->isLeaf)
    {
        if (node->flooded)
            return true;
        GenerateLightmapAreas(poly, -1.0f);
        return false;
    }
    sideFlags = SplitPolygonEpsilon(poly, node->plane, &pFront, &pBack);
    switch (sideFlags)
    {
    case SIDE_FRONT:
        return RemoveLmapAreaInSolid_r(node->children[0], poly);
    case SIDE_BACK:
        return RemoveLmapAreaInSolid_r(node->children[1], poly);
    case SIDE_CROSS:
        outsideSolid = RemoveLmapAreaInSolid_r(node->children[0], pFront);
        pFront->Free();
        outsideSolid |= RemoveLmapAreaInSolid_r(node->children[1], pBack);
        pBack->Free();
        return outsideSolid;
    case SIDE_ON:
        if (node->plane.normal * poly->info->plane.normal > 0)
            return RemoveLmapAreaInSolid_r(node->children[0], poly);
        else
            return RemoveLmapAreaInSolid_r(node->children[1], poly);
    }
    return true;
}

/*
=============
FinalizeLightmapAreas

Discard negligible coverage and snap areas close to integers to suppress
roundoff left by polygon clipping.
=============
*/
void FinalizeLightmapAreas(void)
{
    int i;
    for (i = 0; i < numLightmaps; i++)
    {
        int t, s;
        for (t = 0; t < LIGHTMAP_SIZE; t++)
            for (s = 0; s < LIGHTMAP_SIZE; s++)
            {
                if (lightmapSamples[i].samples[t][s].area < EQUAL_EPSILON * 2)
                    lightmapSamples[i].samples[t][s].area = 0.0f;
                else
                {
                    int area = RoundToInt(lightmapSamples[i].samples[t][s].area);
                    if (fabs(lightmapSamples[i].samples[t][s].area - area) < EQUAL_EPSILON * 2)
                        lightmapSamples[i].samples[t][s].area = (float)area;
                }
            }
    }
}

/*
=============
DebugLightmapAreas

Display half the accumulated area as grayscale. Overlapping coverage
turns red; negative coverage beyond the tolerance turns blue.
=============
*/
void DebugLightmapAreas(void)
{
    int i;
    for (i = 0; i < numLightmaps; i++)
    {
        int t, s;
        for (t = 0; t < LIGHTMAP_SIZE; t++)
            for (s = 0; s < LIGHTMAP_SIZE; s++)
            {
                float area = lightmapSamples[i].samples[t][s].area * 0.5f;
                if (area > 1.0 + LIGHTMAP_AREA_TOLERANCE)
                {
                    lightmapSamples[i].samples[t][s].color.r = 1.0f;
                    lightmapSamples[i].samples[t][s].color.g =
                        (2.0f - area < 0.0f) ? 0.0f : 2.0f - area;
                    lightmapSamples[i].samples[t][s].color.b = 0.0f;
                }
                else if (area > 0.0f)
                {
                    lightmapSamples[i].samples[t][s].color.r = area * 0.75f + 0.25f;
                    lightmapSamples[i].samples[t][s].color.g = area * 0.75f + 0.25f;
                    lightmapSamples[i].samples[t][s].color.b = area * 0.75f + 0.25f;
                }
                else if (area < -LIGHTMAP_AREA_TOLERANCE)
                {
                    lightmapSamples[i].samples[t][s].color.r = 0.0f;
                    lightmapSamples[i].samples[t][s].color.g = 0.0f;
                    lightmapSamples[i].samples[t][s].color.b = 1.0f;
                }
                else
                {
                    lightmapSamples[i].samples[t][s].color.r = 0.0f;
                    lightmapSamples[i].samples[t][s].color.g = 0.0f;
                    lightmapSamples[i].samples[t][s].color.b = 0.0f;
                }
            }
    }
}

/*
=============
ColorRGB::FromBytes
=============
*/
void ColorRGB::FromBytes(const unsigned char *rgb)
{
    r = (options.overbrightScale / 255.0f) * rgb[0];
    g = (options.overbrightScale / 255.0f) * rgb[1];
    b = (options.overbrightScale / 255.0f) * rgb[2];
}

/*
=============
LightmapSample::LightmapSample
=============
*/
LightmapSample::LightmapSample(void)
{
}
